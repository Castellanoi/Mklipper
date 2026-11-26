/*
    SPDX-FileCopyrightText: 2026 Mklipper contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "updatedialog.h"

#include "flaticons.h"
#include "settings.h"
#include "themestyle.h"
#include "translations.h"

#include <QCloseEvent>
#include <QDesktopServices>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QTextBrowser>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;

UpdateDialog::UpdateDialog(Updater *updater, QWidget *parent)
    : QDialog(parent)
    , m_updater(updater)
{
    setWindowTitle(tr("Actualizar Mklipper"));
    setMinimumWidth(580);

    const ThemeTokens t = ThemeTokens::fromPalette(palette());
    setWindowIcon(FlatIcons::appLogoIcon(t.accent));
    setStyleSheet(ThemeStyle::dialogStyleSheet(t));

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(18, 18, 18, 16);
    mainLayout->setSpacing(14);

    // Header
    auto *headerLayout = new QHBoxLayout;
    headerLayout->setSpacing(12);

    auto *iconLabel = new QLabel(this);
    iconLabel->setPixmap(FlatIcons::appLogoPixmap(t.accent, 48, devicePixelRatioF()));
    iconLabel->setFixedSize(48, 48);
    headerLayout->addWidget(iconLabel, 0, Qt::AlignTop);

    auto *titleLayout = new QVBoxLayout;
    titleLayout->setSpacing(3);
    m_titleLabel = new QLabel(this);
    m_titleLabel->setTextFormat(Qt::PlainText);
    QFont titleFont = m_titleLabel->font();
    titleFont.setBold(true);
    titleFont.setPointSize(titleFont.pointSize() + 2);
    m_titleLabel->setFont(titleFont);
    m_titleLabel->setWordWrap(true);

    m_subtitleLabel = new QLabel(this);
    m_subtitleLabel->setWordWrap(true);

    titleLayout->addWidget(m_titleLabel);
    titleLayout->addWidget(m_subtitleLabel);
    headerLayout->addLayout(titleLayout, 1);
    mainLayout->addLayout(headerLayout);

    // Release notes (only open safe http/https links when clicked)
    m_notesBrowser = new QTextBrowser(this);
    m_notesBrowser->setOpenExternalLinks(false);
    m_notesBrowser->setOpenLinks(false);
    connect(m_notesBrowser, &QTextBrowser::anchorClicked, this, [](const QUrl &url) {
        const QString scheme = url.scheme().toLower();
        if (url.isValid() && !url.host().isEmpty() && (scheme == u"https"_s || scheme == u"http"_s)) {
            QDesktopServices::openUrl(url);
        }
    });
    m_notesBrowser->setMinimumHeight(160);
    m_notesBrowser->setMaximumHeight(280);
    m_notesBrowser->setVisible(false);
    mainLayout->addWidget(m_notesBrowser, 1);

    // Status + Progress
    m_statusLabel = new QLabel(this);
    m_statusLabel->setTextFormat(Qt::PlainText);
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setVisible(false);
    mainLayout->addWidget(m_statusLabel);

    m_progressBar = new QProgressBar(this);
    m_progressBar->setVisible(false);
    mainLayout->addWidget(m_progressBar);

    // Error details
    m_detailsEdit = new QPlainTextEdit(this);
    m_detailsEdit->setReadOnly(true);
    m_detailsEdit->setMaximumHeight(130);
    m_detailsEdit->setVisible(false);
    mainLayout->addWidget(m_detailsEdit);

    // Buttons
    auto *btnDivider = new QFrame(this);
    btnDivider->setObjectName(u"mklipperDivider"_s);
    mainLayout->addWidget(btnDivider);

    auto *btnLayout = new QHBoxLayout;
    btnLayout->setSpacing(8);
    m_openGithubBtn = new QPushButton(
        FlatIcons::icon(FlatIcon::ExternalLink, t.textPrimary, t.textPrimary, t.textMuted, 14),
        tr("Ver en GitHub"),
        this);
    m_openGithubBtn->setIconSize(QSize(14, 14));
    m_openGithubBtn->setCursor(Qt::PointingHandCursor);

    m_skipBtn = new QPushButton(tr("Omitir esta versión"), this);
    m_skipBtn->setCursor(Qt::PointingHandCursor);

    m_retryBtn = new QPushButton(
        FlatIcons::icon(FlatIcon::Refresh, t.textPrimary, t.textPrimary, t.textMuted, 15),
        tr("Reintentar"),
        this);
    m_retryBtn->setIconSize(QSize(15, 15));
    m_retryBtn->setCursor(Qt::PointingHandCursor);

    m_updateBtn = new QPushButton(
        FlatIcons::icon(FlatIcon::Update, t.accentFg, t.accentFg, t.textMuted, 15),
        tr("Actualizar ahora"),
        this);
    m_updateBtn->setProperty("primary", true);
    m_updateBtn->setIconSize(QSize(15, 15));
    m_updateBtn->setCursor(Qt::PointingHandCursor);
    m_updateBtn->setDefault(true);

    m_closeBtn = new QPushButton(tr("Cerrar"), this);
    m_closeBtn->setCursor(Qt::PointingHandCursor);

    btnLayout->addWidget(m_openGithubBtn);
    btnLayout->addWidget(m_skipBtn);
    btnLayout->addStretch();
    btnLayout->addWidget(m_retryBtn);
    btnLayout->addWidget(m_updateBtn);
    btnLayout->addWidget(m_closeBtn);
    mainLayout->addLayout(btnLayout);

    connect(m_openGithubBtn, &QPushButton::clicked, this, &UpdateDialog::onOpenReleasePageClicked);
    connect(m_skipBtn, &QPushButton::clicked, this, &UpdateDialog::onSkipVersionClicked);
    connect(m_retryBtn, &QPushButton::clicked, this, &UpdateDialog::startCheck);
    connect(m_updateBtn, &QPushButton::clicked, this, &UpdateDialog::onUpdateClicked);
    connect(m_closeBtn, &QPushButton::clicked, this, &UpdateDialog::reject);

    if (m_updater) {
        connect(m_updater, &Updater::stateChanged, this, &UpdateDialog::onStateChanged);
        connect(m_updater, &Updater::updateAvailable, this, &UpdateDialog::onUpdateAvailable);
        connect(m_updater, &Updater::alreadyUpToDate, this, &UpdateDialog::onAlreadyUpToDate);
        connect(m_updater, &Updater::checkFailed, this, &UpdateDialog::onCheckFailed);
        connect(m_updater, &Updater::downloadProgress, this, &UpdateDialog::onDownloadProgress);
        connect(m_updater, &Updater::statusMessage, this, &UpdateDialog::onStatusMessage);
        connect(m_updater, &Updater::installSucceeded, this, &UpdateDialog::onInstallSucceeded);
        connect(m_updater, &Updater::installFailed, this, &UpdateDialog::onInstallFailed);
        connect(m_updater, &Updater::cancelled, this, &UpdateDialog::onCancelled);
    }
}

void UpdateDialog::startCheck()
{
    if (!m_updater) {
        return;
    }
    setMode(Mode::Checking);
    m_updater->checkForUpdates(true);
}

void UpdateDialog::showRelease(const ReleaseInfo &release)
{
    m_release = release;
    setMode(Mode::UpdateAvailable);
}

void UpdateDialog::closeEvent(QCloseEvent *event)
{
    if (m_updater && m_updater->state() == Updater::State::Installing) {
        event->ignore();
        return;
    }
    if (m_updater && m_updater->isBusy()) {
        m_updater->cancel();
    }
    QDialog::closeEvent(event);
}

void UpdateDialog::reject()
{
    if (m_updater && m_updater->state() == Updater::State::Installing) {
        return;
    }
    if (m_updater && m_updater->isBusy()) {
        m_updater->cancel();
        if (m_mode == Mode::Downloading) {
            return;
        }
    }
    QDialog::reject();
}

void UpdateDialog::onStateChanged(Updater::State state)
{
    if (state == Updater::State::Checking) {
        setMode(Mode::Checking);
    } else if (state == Updater::State::Downloading) {
        setMode(Mode::Downloading);
    } else if (state == Updater::State::Installing) {
        setMode(Mode::Installing);
    }
}

void UpdateDialog::onUpdateAvailable(const ReleaseInfo &release, bool /*userInitiated*/)
{
    showRelease(release);
}

void UpdateDialog::onAlreadyUpToDate(const QString &currentVersion, bool /*userInitiated*/)
{
    m_titleLabel->setText(tr("Mklipper está actualizado"));
    m_subtitleLabel->setTextFormat(Qt::PlainText);
    m_subtitleLabel->setText(tr("Ya tenés instalada la última versión disponible (%1).").arg(currentVersion));
    setMode(Mode::UpToDate);
}

void UpdateDialog::onCheckFailed(const QString &errorMessage, bool /*userInitiated*/)
{
    m_titleLabel->setText(tr("No se pudo buscar actualizaciones"));
    m_subtitleLabel->setTextFormat(Qt::PlainText);
    m_subtitleLabel->setText(errorMessage);
    setMode(Mode::CheckError);
}

void UpdateDialog::onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    if (m_mode != Mode::Downloading) {
        return;
    }
    if (bytesTotal > 0) {
        const int pct = static_cast<int>(qBound(qint64(0), (bytesReceived * 100) / bytesTotal, qint64(100)));
        m_progressBar->setRange(0, 100);
        m_progressBar->setValue(pct);
        m_statusLabel->setText(
            tr("Descargando %1 (%2 / %3)…")
                .arg(m_release.assetName, formatSize(bytesReceived), formatSize(bytesTotal)));
    } else {
        m_progressBar->setRange(0, 0);
        m_statusLabel->setText(
            tr("Descargando %1 (%2)…").arg(m_release.assetName, formatSize(bytesReceived)));
    }
}

void UpdateDialog::onStatusMessage(const QString &message)
{
    m_statusLabel->setText(message);
    m_statusLabel->setVisible(true);
}

void UpdateDialog::onInstallSucceeded(const ReleaseInfo &release)
{
    m_release = release;
    m_titleLabel->setText(tr("Actualización completada"));
    m_subtitleLabel->setTextFormat(Qt::PlainText);
    m_subtitleLabel->setText(
        tr("Mklipper se actualizó correctamente a la versión %1. Reiniciando…").arg(release.version));
    setMode(Mode::InstallSuccess);
}

void UpdateDialog::onInstallFailed(const QString &errorMessage, const QString &details)
{
    m_titleLabel->setText(tr("No se pudo completar la actualización"));
    m_subtitleLabel->setTextFormat(Qt::PlainText);
    m_subtitleLabel->setText(errorMessage);
    if (!details.isEmpty()) {
        m_detailsEdit->setPlainText(details);
        m_detailsEdit->setVisible(true);
    } else {
        m_detailsEdit->clear();
        m_detailsEdit->setVisible(false);
    }
    setMode(Mode::InstallError);
}

void UpdateDialog::onCancelled()
{
    if (m_release.hasDebAsset()) {
        setMode(Mode::UpdateAvailable);
        m_statusLabel->setText(tr("Actualización cancelada."));
        m_statusLabel->setVisible(true);
    } else {
        QDialog::reject();
    }
}

void UpdateDialog::onUpdateClicked()
{
    if (!m_updater || !m_release.hasDebAsset()) {
        return;
    }
    m_detailsEdit->clear();
    m_detailsEdit->setVisible(false);
    m_updater->downloadAndInstall(m_release);
}

void UpdateDialog::onSkipVersionClicked()
{
    if (!m_release.version.isEmpty()) {
        auto *s = MklipperSettings::self();
        s->setSkippedUpdateVersion(m_release.version);
        s->save();
    }
    QDialog::reject();
}

void UpdateDialog::onOpenReleasePageClicked()
{
    const QString scheme = m_release.htmlUrl.scheme().toLower();
    if (m_release.htmlUrl.isValid() && !m_release.htmlUrl.host().isEmpty()
        && (scheme == u"https"_s || scheme == u"http"_s)) {
        QDesktopServices::openUrl(m_release.htmlUrl);
    }
}

void UpdateDialog::setMode(Mode mode)
{
    m_mode = mode;

    m_notesBrowser->setVisible(false);
    m_statusLabel->setVisible(false);
    m_progressBar->setVisible(false);
    m_detailsEdit->setVisible(mode == Mode::InstallError && !m_detailsEdit->toPlainText().isEmpty());

    m_openGithubBtn->setVisible(false);
    m_skipBtn->setVisible(false);
    m_retryBtn->setVisible(false);
    m_updateBtn->setVisible(false);
    m_updateBtn->setEnabled(true);
    m_closeBtn->setEnabled(true);

    switch (mode) {
    case Mode::Checking:
        m_titleLabel->setText(tr("Buscando actualizaciones…"));
        m_subtitleLabel->setTextFormat(Qt::PlainText);
        m_subtitleLabel->setText(
            tr("Versión instalada: %1 · Consultando GitHub Releases…").arg(Updater::currentVersion()));
        m_progressBar->setRange(0, 0);
        m_progressBar->setVisible(true);
        m_closeBtn->setText(tr("Cancelar"));
        break;

    case Mode::UpdateAvailable: {
        m_titleLabel->setText(tr("Nueva versión disponible: Mklipper %1").arg(m_release.version));
        QString sub = tr("Versión instalada: <b>%1</b> → Nueva versión: <b>%2</b>")
                          .arg(Updater::currentVersion().toHtmlEscaped(), m_release.version.toHtmlEscaped());
        if (m_release.assetSize > 0) {
            sub += tr(" · Descarga: %1").arg(formatSize(m_release.assetSize).toHtmlEscaped());
        }
        if (!m_release.hasDebAsset()) {
            sub += tr("<br><i>No se encontró un paquete .deb para %1 en este release; podés abrir GitHub para descargarlo manualmente.</i>")
                       .arg(m_release.architecture.toHtmlEscaped());
        }
        m_subtitleLabel->setTextFormat(Qt::RichText);
        m_subtitleLabel->setText(sub);

        const QString localizedNotes = MklipperTranslations::filterReleaseNotesForLanguage(m_release.releaseNotes);
        if (!localizedNotes.isEmpty()) {
            m_notesBrowser->setMarkdown(localizedNotes);
        } else {
            m_notesBrowser->setPlainText(tr("No se incluyeron notas de versión para este release."));
        }
        m_notesBrowser->setVisible(true);

        m_openGithubBtn->setVisible(m_release.htmlUrl.isValid());
        m_skipBtn->setVisible(true);
        m_updateBtn->setText(tr("Actualizar ahora"));
        m_updateBtn->setVisible(m_release.hasDebAsset());
        m_closeBtn->setText(tr("Más tarde"));
        break;
    }

    case Mode::UpToDate:
        m_closeBtn->setText(tr("Cerrar"));
        break;

    case Mode::CheckError:
        m_retryBtn->setVisible(true);
        m_closeBtn->setText(tr("Cerrar"));
        break;

    case Mode::Downloading:
        m_titleLabel->setText(tr("Descargando Mklipper %1…").arg(m_release.version));
        m_notesBrowser->setVisible(true);
        m_statusLabel->setVisible(true);
        m_progressBar->setRange(0, 0);
        m_progressBar->setVisible(true);
        m_closeBtn->setText(tr("Cancelar"));
        break;

    case Mode::Installing:
        m_titleLabel->setText(tr("Instalando Mklipper %1…").arg(m_release.version));
        m_subtitleLabel->setText(
            tr("Se abrirá el diálogo de contraseña de Linux Mint para actualizar el paquete .deb. No cierres esta ventana."));
        m_statusLabel->setVisible(true);
        m_progressBar->setRange(0, 0);
        m_progressBar->setVisible(true);
        m_closeBtn->setText(tr("Instalando…"));
        m_closeBtn->setEnabled(false);
        break;

    case Mode::InstallError:
        m_openGithubBtn->setVisible(m_release.htmlUrl.isValid());
        m_updateBtn->setVisible(m_release.hasDebAsset());
        m_updateBtn->setText(tr("Reintentar instalación"));
        m_closeBtn->setText(tr("Cerrar"));
        break;

    case Mode::InstallSuccess:
        m_progressBar->setRange(0, 100);
        m_progressBar->setValue(100);
        m_progressBar->setVisible(true);
        m_closeBtn->setText(tr("Cerrar"));
        m_closeBtn->setEnabled(false);
        break;
    }

    adjustSize();
}

QString UpdateDialog::formatSize(qint64 bytes)
{
    if (bytes >= 1024 * 1024) {
        return QStringLiteral("%1 MB").arg(static_cast<double>(bytes) / (1024.0 * 1024.0), 0, 'f', 2);
    }
    if (bytes >= 1024) {
        return QStringLiteral("%1 KB").arg(static_cast<double>(bytes) / 1024.0, 0, 'f', 1);
    }
    return QStringLiteral("%1 B").arg(bytes);
}
