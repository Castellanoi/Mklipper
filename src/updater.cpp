/*
    SPDX-FileCopyrightText: 2026 Mklipper contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "updater.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QSysInfo>
#include <QTemporaryDir>
#include <QVersionNumber>

using namespace Qt::StringLiterals;

namespace
{
constexpr auto kDefaultReleasesApiUrl = "https://api.github.com/repos/Castellanoi/Mklipper/releases/latest";
constexpr int kApiTimeoutMs = 25000;
constexpr int kDownloadTimeoutMs = 180000;
constexpr qint64 kMaxChecksumBytes = 64 * 1024;

QString userAgentString()
{
    return QStringLiteral("Mklipper/%1 (Linux Mint; Qt6)").arg(Updater::currentVersion());
}
} // namespace

Updater::Updater(QObject *parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
{
    m_nam->setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);
}

Updater::~Updater()
{
    if (m_activeReply) {
        m_activeReply->disconnect(this);
        m_activeReply->abort();
        m_activeReply = nullptr;
    }
    cleanupDownload();
}

QString Updater::currentVersion()
{
    return QStringLiteral(MKLIPPER_VERSION);
}

QString Updater::systemArchitecture()
{
    static QString cachedArch;
    if (!cachedArch.isEmpty()) {
        return cachedArch;
    }

    QProcess proc;
    proc.start(u"dpkg"_s, {u"--print-architecture"_s});
    if (proc.waitForFinished(3000) && proc.exitCode() == 0) {
        const QString out = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
        if (!out.isEmpty()) {
            cachedArch = out;
            return cachedArch;
        }
    }

    const QString cpu = QSysInfo::currentCpuArchitecture();
    if (cpu == u"x86_64"_s) {
        cachedArch = u"amd64"_s;
    } else if (cpu == u"arm64"_s || cpu == u"aarch64"_s) {
        cachedArch = u"arm64"_s;
    } else if (cpu == u"i386"_s) {
        cachedArch = u"i386"_s;
    } else if (cpu == u"arm"_s) {
        cachedArch = u"armhf"_s;
    } else {
        cachedArch = cpu;
    }
    return cachedArch;
}

QString Updater::normalizeVersionString(const QString &rawTag)
{
    QString v = rawTag.trimmed();
    if (v.startsWith(QLatin1Char('v'), Qt::CaseInsensitive)) {
        v.remove(0, 1);
    }
    // Strip build metadata (+...) and pre-release suffix (-...) for QVersionNumber comparison
    const int plusIdx = v.indexOf(QLatin1Char('+'));
    if (plusIdx >= 0) {
        v.truncate(plusIdx);
    }
    const int dashIdx = v.indexOf(QLatin1Char('-'));
    if (dashIdx >= 0) {
        v.truncate(dashIdx);
    }
    return v.trimmed();
}

bool Updater::isNewerVersion(const QString &candidate, const QString &current)
{
    const QString cleanCand = normalizeVersionString(candidate);
    const QString cleanCur = normalizeVersionString(current);
    const QVersionNumber vCand = QVersionNumber::fromString(cleanCand);
    const QVersionNumber vCur = QVersionNumber::fromString(cleanCur);
    if (vCand.isNull() || vCur.isNull()) {
        return false;
    }
    return QVersionNumber::compare(vCand, vCur) > 0;
}

QString Updater::parseSha256ForFile(const QByteArray &checksumContent, const QString &fileName)
{
    static const QRegularExpression reHashAndFile(
        QStringLiteral(R"(^\s*([0-9a-fA-F]{64})\s+[*]?(.+?)\s*$)"));
    static const QRegularExpression reBareHash(
        QStringLiteral(R"(^\s*([0-9a-fA-F]{64})\s*$)"));

    const QString baseTarget = QFileInfo(fileName).fileName();
    const QString text = QString::fromUtf8(checksumContent);
    const QStringList lines = text.split(QLatin1Char('\n'), Qt::SkipEmptyParts);

    for (const QString &line : lines) {
        const auto m = reHashAndFile.match(line);
        if (m.hasMatch()) {
            const QString listedName = QFileInfo(m.captured(2).trimmed()).fileName();
            if (listedName == baseTarget) {
                return m.captured(1).toLower();
            }
        }
    }

    // Support single-line bare hash files (e.g. mklipper_1.1.0_amd64.deb.sha256)
    if (lines.size() == 1) {
        const auto m = reBareHash.match(lines.first());
        if (m.hasMatch()) {
            return m.captured(1).toLower();
        }
    }

    return {};
}

void Updater::setState(State newState)
{
    if (m_state != newState) {
        m_state = newState;
        Q_EMIT stateChanged(m_state);
    }
}

QUrl Updater::latestReleaseApiUrl() const
{
    const QByteArray overrideEnv = qgetenv("MKLIPPER_UPDATE_URL").trimmed();
    if (!overrideEnv.isEmpty()) {
        return QUrl::fromUserInput(QString::fromUtf8(overrideEnv));
    }
    return QUrl(QString::fromLatin1(kDefaultReleasesApiUrl));
}

bool Updater::isAllowedUrl(const QUrl &url) const
{
    if (!url.isValid()) {
        return false;
    }
    if (url.scheme() == u"https"_s) {
        return true;
    }
    // Allow plain HTTP only to loopback when MKLIPPER_UPDATE_URL is explicitly set for local testing
    if (url.scheme() == u"http"_s && qEnvironmentVariableIsSet("MKLIPPER_UPDATE_URL")) {
        const QString host = url.host().toLower();
        return host == u"127.0.0.1"_s || host == u"localhost"_s || host == u"::1"_s;
    }
    return false;
}

void Updater::checkForUpdates(bool userInitiated)
{
    if (isBusy()) {
        return;
    }

    const QUrl apiUrl = latestReleaseApiUrl();
    if (!isAllowedUrl(apiUrl)) {
        Q_EMIT checkFailed(tr("La URL de actualización configurada no es válida o no usa HTTPS."), userInitiated);
        return;
    }

    m_userInitiatedCheck = userInitiated;
    setState(State::Checking);
    Q_EMIT statusMessage(tr("Buscando actualizaciones…"));

    QNetworkRequest req(apiUrl);
    req.setRawHeader("User-Agent", userAgentString().toUtf8());
    req.setRawHeader("Accept", "application/vnd.github+json");
    req.setTransferTimeout(kApiTimeoutMs);

    QNetworkReply *reply = m_nam->get(req);
    m_activeReply = reply;
    connect(reply, &QNetworkReply::finished, this, &Updater::onCheckReplyFinished);
}

void Updater::onCheckReplyFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    if (!reply) {
        return;
    }
    reply->deleteLater();
    if (m_activeReply == reply) {
        m_activeReply = nullptr;
    }

    const bool userInitiated = m_userInitiatedCheck;

    if (m_state != State::Checking) {
        return;
    }

    const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    if (reply->error() == QNetworkReply::OperationCanceledError) {
        setState(State::Idle);
        Q_EMIT cancelled();
        return;
    }

    if (httpStatus == 404) {
        // Repo has no GitHub Releases published yet
        setState(State::Idle);
        m_hasPendingUpdate = false;
        Q_EMIT alreadyUpToDate(currentVersion(), userInitiated);
        return;
    }

    if (reply->error() != QNetworkReply::NoError) {
        setState(State::Idle);
        QString msg;
        if (httpStatus == 403 || httpStatus == 429) {
            msg = tr("Se alcanzó el límite temporal de consultas de GitHub. Intentá de nuevo en unos minutos.");
        } else {
            msg = tr("No se pudo conectar con GitHub para buscar actualizaciones (%1).").arg(reply->errorString());
        }
        Q_EMIT checkFailed(msg, userInitiated);
        return;
    }

    const QByteArray payload = reply->readAll();
    QJsonParseError parseErr;
    const QJsonDocument doc = QJsonDocument::fromJson(payload, &parseErr);
    if (parseErr.error != QJsonParseError::NoError || !doc.isObject()) {
        setState(State::Idle);
        Q_EMIT checkFailed(tr("Respuesta inválida del servidor de actualizaciones."), userInitiated);
        return;
    }

    const QJsonObject obj = doc.object();
    const QString tagName = obj.value(u"tag_name"_s).toString().trimmed();
    const QString version = normalizeVersionString(tagName);
    if (version.isEmpty()) {
        setState(State::Idle);
        Q_EMIT checkFailed(tr("No se encontró un número de versión válido en el último release."), userInitiated);
        return;
    }

    if (!isNewerVersion(version, currentVersion())) {
        setState(State::Idle);
        m_hasPendingUpdate = false;
        Q_EMIT alreadyUpToDate(currentVersion(), userInitiated);
        return;
    }

    ReleaseInfo info;
    info.tagName = tagName;
    info.version = version;
    info.title = obj.value(u"name"_s).toString().trimmed();
    if (info.title.isEmpty()) {
        info.title = QStringLiteral("Mklipper %1").arg(tagName);
    }
    info.releaseNotes = obj.value(u"body"_s).toString().trimmed();
    info.htmlUrl = QUrl(obj.value(u"html_url"_s).toString().trimmed());
    info.architecture = systemArchitecture();

    const QString preferredSuffix = QStringLiteral("_%1.deb").arg(info.architecture);
    const QString allSuffix = u"_all.deb"_s;

    const QJsonArray assets = obj.value(u"assets"_s).toArray();
    for (const QJsonValue &val : assets) {
        const QJsonObject asset = val.toObject();
        const QString name = QFileInfo(asset.value(u"name"_s).toString().trimmed()).fileName();
        const QUrl downloadUrl(asset.value(u"browser_download_url"_s).toString().trimmed());
        if (!isAllowedUrl(downloadUrl)) {
            continue;
        }

        if (name.endsWith(preferredSuffix, Qt::CaseInsensitive)) {
            info.assetName = name;
            info.assetUrl = downloadUrl;
            info.assetSize = asset.value(u"size"_s).toInteger();
        } else if (!info.hasDebAsset() && name.endsWith(allSuffix, Qt::CaseInsensitive)) {
            info.assetName = name;
            info.assetUrl = downloadUrl;
            info.assetSize = asset.value(u"size"_s).toInteger();
        }
    }

    // Second pass: look for matching SHA-256 checksum asset
    for (const QJsonValue &val : assets) {
        const QJsonObject asset = val.toObject();
        const QString name = QFileInfo(asset.value(u"name"_s).toString().trimmed()).fileName();
        const QUrl downloadUrl(asset.value(u"browser_download_url"_s).toString().trimmed());
        if (!isAllowedUrl(downloadUrl)) {
            continue;
        }

        const bool isExactSha = !info.assetName.isEmpty()
            && name.compare(info.assetName + u".sha256"_s, Qt::CaseInsensitive) == 0;
        const bool isSumsFile = name.compare(u"SHA256SUMS"_s, Qt::CaseInsensitive) == 0
            || name.compare(u"sha256sums.txt"_s, Qt::CaseInsensitive) == 0;

        if (isExactSha) {
            info.checksumUrl = downloadUrl;
            info.checksumFileName = name;
            break;
        }
        if (isSumsFile && !info.checksumUrl.isValid()) {
            info.checksumUrl = downloadUrl;
            info.checksumFileName = name;
        }
    }

    m_latestRelease = info;
    m_hasPendingUpdate = true;
    setState(State::Idle);
    Q_EMIT updateAvailable(info, userInitiated);
}

void Updater::downloadAndInstall(const ReleaseInfo &release)
{
    if (isBusy()) {
        return;
    }

    if (!release.hasDebAsset() || !isAllowedUrl(release.assetUrl)) {
        Q_EMIT installFailed(
            tr("No se encontró un paquete .deb compatible con tu arquitectura (%1) en este release.")
                .arg(systemArchitecture()),
            {});
        return;
    }

    cleanupDownload();
    m_pendingInstall = release;
    m_expectedSha256.clear();
    m_installOutput.clear();

    setState(State::Downloading);

    if (release.checksumUrl.isValid() && isAllowedUrl(release.checksumUrl)) {
        Q_EMIT statusMessage(tr("Descargando suma de verificación SHA-256…"));
        QNetworkRequest req(release.checksumUrl);
        req.setRawHeader("User-Agent", userAgentString().toUtf8());
        req.setTransferTimeout(kApiTimeoutMs);

        QNetworkReply *reply = m_nam->get(req);
        m_activeReply = reply;
        connect(reply, &QNetworkReply::finished, this, &Updater::onChecksumReplyFinished);
    } else {
        startDebDownload();
    }
}

void Updater::onChecksumReplyFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    if (!reply) {
        return;
    }
    reply->deleteLater();
    if (m_activeReply == reply) {
        m_activeReply = nullptr;
    }

    if (m_state != State::Downloading) {
        return;
    }

    if (reply->error() == QNetworkReply::OperationCanceledError) {
        cleanupDownload();
        setState(State::Idle);
        Q_EMIT cancelled();
        return;
    }

    if (reply->error() != QNetworkReply::NoError) {
        const QString err = reply->errorString();
        cleanupDownload();
        setState(State::Idle);
        Q_EMIT installFailed(tr("No se pudo descargar el archivo de verificación SHA-256."), err);
        return;
    }

    const QByteArray data = reply->read(kMaxChecksumBytes);
    m_expectedSha256 = parseSha256ForFile(data, m_pendingInstall.assetName);
    if (m_expectedSha256.isEmpty()) {
        cleanupDownload();
        setState(State::Idle);
        Q_EMIT installFailed(
            tr("El archivo %1 no contiene una suma SHA-256 válida para %2.")
                .arg(m_pendingInstall.checksumFileName, m_pendingInstall.assetName),
            QString::fromUtf8(data));
        return;
    }

    startDebDownload();
}

void Updater::startDebDownload()
{
    m_tempDir = std::make_unique<QTemporaryDir>(QDir::tempPath() + u"/mklipper-update-XXXXXX"_s);
    if (!m_tempDir->isValid()) {
        cleanupDownload();
        setState(State::Idle);
        Q_EMIT installFailed(tr("No se pudo crear un directorio temporal para descargar la actualización."),
                             m_tempDir ? m_tempDir->errorString() : QString());
        return;
    }

    // Ensure _apt user can traverse the temporary directory when apt-get installs the local .deb
    QFile::setPermissions(m_tempDir->path(),
                          QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner
                              | QFileDevice::ReadGroup | QFileDevice::ExeGroup
                              | QFileDevice::ReadOther | QFileDevice::ExeOther);

    const QString safeName = QFileInfo(m_pendingInstall.assetName).fileName();
    m_downloadedDebPath = m_tempDir->filePath(safeName);

    m_downloadFile = std::make_unique<QFile>(m_downloadedDebPath);
    if (!m_downloadFile->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        const QString err = m_downloadFile->errorString();
        cleanupDownload();
        setState(State::Idle);
        Q_EMIT installFailed(tr("No se pudo abrir el archivo temporal para escribir la actualización."), err);
        return;
    }

    Q_EMIT statusMessage(tr("Descargando %1…").arg(safeName));

    QNetworkRequest req(m_pendingInstall.assetUrl);
    req.setRawHeader("User-Agent", userAgentString().toUtf8());
    req.setTransferTimeout(kDownloadTimeoutMs);

    QNetworkReply *reply = m_nam->get(req);
    m_activeReply = reply;

    connect(reply, &QNetworkReply::readyRead, this, &Updater::onDebReadyRead);
    connect(reply, &QNetworkReply::downloadProgress, this, [this](qint64 received, qint64 total) {
        const qint64 effectiveTotal = total > 0 ? total : m_pendingInstall.assetSize;
        Q_EMIT downloadProgress(received, effectiveTotal);
    });
    connect(reply, &QNetworkReply::finished, this, &Updater::onDebReplyFinished);
}

void Updater::onDebReadyRead()
{
    auto *reply = qobject_cast<QNetworkReply *>(sender());
    if (!reply || !m_downloadFile || !m_downloadFile->isOpen()) {
        return;
    }
    const QByteArray chunk = reply->readAll();
    if (!chunk.isEmpty() && m_downloadFile->write(chunk) != chunk.size()) {
        reply->abort();
    }
}

void Updater::onDebReplyFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    if (!reply) {
        return;
    }
    reply->deleteLater();
    if (m_activeReply == reply) {
        m_activeReply = nullptr;
    }

    if (m_state != State::Downloading) {
        cleanupDownload();
        return;
    }

    if (m_downloadFile && m_downloadFile->isOpen()) {
        const QByteArray tail = reply->readAll();
        if (!tail.isEmpty()) {
            m_downloadFile->write(tail);
        }
        m_downloadFile->flush();
        m_downloadFile->close();
    }

    if (reply->error() == QNetworkReply::OperationCanceledError) {
        cleanupDownload();
        setState(State::Idle);
        Q_EMIT cancelled();
        return;
    }

    if (reply->error() != QNetworkReply::NoError) {
        const QString err = reply->errorString();
        cleanupDownload();
        setState(State::Idle);
        Q_EMIT installFailed(tr("Error al descargar el paquete de actualización."), err);
        return;
    }

    // Make .deb world-readable for _apt
    QFile::setPermissions(m_downloadedDebPath,
                          QFileDevice::ReadOwner | QFileDevice::WriteOwner
                              | QFileDevice::ReadGroup | QFileDevice::ReadOther);

    // Verify Debian ar archive magic header and optional SHA-256
    QFile verifyFile(m_downloadedDebPath);
    if (!verifyFile.open(QIODevice::ReadOnly)) {
        const QString err = verifyFile.errorString();
        cleanupDownload();
        setState(State::Idle);
        Q_EMIT installFailed(tr("No se pudo leer el paquete descargado para verificarlo."), err);
        return;
    }

    const QByteArray header = verifyFile.read(8);
    if (header != "!<arch>\n") {
        cleanupDownload();
        setState(State::Idle);
        Q_EMIT installFailed(tr("El archivo descargado no es un paquete Debian (.deb) válido."), {});
        return;
    }

    if (!m_expectedSha256.isEmpty()) {
        Q_EMIT statusMessage(tr("Verificando integridad (SHA-256)…"));
        verifyFile.seek(0);
        QCryptographicHash hasher(QCryptographicHash::Sha256);
        if (!hasher.addData(&verifyFile)) {
            cleanupDownload();
            setState(State::Idle);
            Q_EMIT installFailed(tr("No se pudo calcular la suma SHA-256 del paquete descargado."), {});
            return;
        }
        const QString actualSha = QString::fromLatin1(hasher.result().toHex()).toLower();
        if (actualSha != m_expectedSha256) {
            cleanupDownload();
            setState(State::Idle);
            Q_EMIT installFailed(
                tr("Error de verificación de integridad: la suma SHA-256 no coincide."),
                tr("Esperada: %1\nObtenida: %2").arg(m_expectedSha256, actualSha));
            return;
        }
    }
    verifyFile.close();

    launchPackageInstall(m_downloadedDebPath);
}

void Updater::launchPackageInstall(const QString &debPath)
{
    const QString pkexec = QStandardPaths::findExecutable(u"pkexec"_s);
    if (pkexec.isEmpty()) {
        cleanupDownload();
        setState(State::Idle);
        Q_EMIT installFailed(
            tr("No se encontró 'pkexec' en el sistema para solicitar permisos de administrador."),
            tr("Podés instalar el paquete manualmente con: sudo apt install ./%1")
                .arg(m_pendingInstall.assetName));
        return;
    }

    setState(State::Installing);
    Q_EMIT statusMessage(tr("Instalando actualización… Ingresá tu contraseña si el sistema la solicita."));

    auto *proc = new QProcess(this);
    m_installProcess = proc;
    proc->setProcessChannelMode(QProcess::MergedChannels);

    connect(proc, &QProcess::readyReadStandardOutput, this, [this, proc]() {
        m_installOutput.append(QString::fromUtf8(proc->readAllStandardOutput()));
    });

    connect(proc, &QProcess::errorOccurred, this, [this, proc](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            const QString errStr = proc->errorString();
            if (m_installProcess == proc) {
                m_installProcess = nullptr;
            }
            proc->deleteLater();
            cleanupDownload();
            setState(State::Idle);
            Q_EMIT installFailed(tr("No se pudo iniciar el instalador del sistema (pkexec)."), errStr);
        }
    });

    connect(proc, &QProcess::finished, this, [this, proc](int exitCode, QProcess::ExitStatus exitStatus) {
        m_installOutput.append(QString::fromUtf8(proc->readAllStandardOutput()));
        if (m_installProcess == proc) {
            m_installProcess = nullptr;
        }
        proc->deleteLater();

        const ReleaseInfo completed = m_pendingInstall;
        const QString output = m_installOutput.trimmed();
        cleanupDownload();
        setState(State::Idle);

        if (exitStatus == QProcess::NormalExit && exitCode == 0) {
            m_hasPendingUpdate = false;
            Q_EMIT installSucceeded(completed);
        } else if (exitCode == 126 || exitCode == 127) {
            Q_EMIT cancelled();
        } else {
            Q_EMIT installFailed(
                tr("La instalación del paquete falló (código %1).").arg(exitCode),
                output);
        }
    });

    // Run non-interactive apt-get so dpkg never stalls waiting on conffile prompts
    proc->start(pkexec,
                {u"env"_s,
                 u"DEBIAN_FRONTEND=noninteractive"_s,
                 u"apt-get"_s,
                 u"install"_s,
                 u"-y"_s,
                 u"-o"_s,
                 u"Dpkg::Options::=--force-confdef"_s,
                 u"-o"_s,
                 u"Dpkg::Options::=--force-confold"_s,
                 debPath});
}

void Updater::cancel()
{
    if (m_activeReply) {
        m_activeReply->abort();
        return;
    }
    if (m_installProcess && m_installProcess->state() != QProcess::NotRunning) {
        // Do not abruptly kill apt/dpkg mid-transaction; let it finish safely
        return;
    }
    cleanupDownload();
    setState(State::Idle);
}

void Updater::cleanupDownload()
{
    if (m_downloadFile) {
        if (m_downloadFile->isOpen()) {
            m_downloadFile->close();
        }
        m_downloadFile.reset();
    }
    m_downloadedDebPath.clear();
    m_tempDir.reset();
}
