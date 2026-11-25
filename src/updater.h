/*
    SPDX-FileCopyrightText: 2026 Mklipper contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QObject>
#include <QPointer>
#include <QString>
#include <QUrl>

#include <memory>

class QFile;
class QJsonObject;
class QNetworkAccessManager;
class QNetworkReply;
class QProcess;
class QTemporaryDir;

struct ReleaseInfo {
    QString tagName;          // e.g. "v1.1.0"
    QString version;          // e.g. "1.1.0"
    QString title;            // Release name
    QString releaseNotes;     // Markdown body
    QUrl htmlUrl;             // GitHub release web URL
    QString assetName;        // e.g. "mklipper_1.1.0_amd64.deb"
    QUrl assetUrl;            // Direct download URL for .deb
    qint64 assetSize = 0;     // Bytes
    QString expectedSha256;   // Optional inline SHA-256 hex (from latest.json manifest)
    QUrl checksumUrl;         // Optional SHA256SUMS / .sha256 URL
    QString checksumFileName; // Name of checksum asset
    QString architecture;     // Detected Debian arch, e.g. "amd64"

    bool hasDebAsset() const { return assetUrl.isValid() && !assetName.isEmpty(); }
};

class Updater : public QObject
{
    Q_OBJECT

public:
    enum class State {
        Idle,
        Checking,
        Downloading,
        Installing,
    };
    Q_ENUM(State)

    explicit Updater(QObject *parent = nullptr);
    ~Updater() override;

    State state() const { return m_state; }
    bool isBusy() const { return m_state != State::Idle; }
    bool hasPendingUpdate() const { return m_hasPendingUpdate; }
    const ReleaseInfo &latestRelease() const { return m_latestRelease; }

    static QString currentVersion();
    static QString systemArchitecture();
    static QString normalizeVersionString(const QString &rawTag);
    static bool isNewerVersion(const QString &candidate, const QString &current);
    static QString parseSha256ForFile(const QByteArray &checksumContent, const QString &fileName);

public Q_SLOTS:
    void checkForUpdates(bool userInitiated);
    void downloadAndInstall(const ReleaseInfo &release);
    void cancel();

Q_SIGNALS:
    void stateChanged(Updater::State state);
    void updateAvailable(const ReleaseInfo &release, bool userInitiated);
    void alreadyUpToDate(const QString &currentVersion, bool userInitiated);
    void checkFailed(const QString &errorMessage, bool userInitiated);

    void downloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void statusMessage(const QString &message);
    void installSucceeded(const ReleaseInfo &release);
    void installFailed(const QString &errorMessage, const QString &details);
    void cancelled();

private:
    enum class CheckSource {
        Manifest,
        GitHubReleases,
        CustomOverride,
    };

    void setState(State newState);
    QUrl manifestUrl(bool bustCache) const;
    QUrl releasesApiUrl() const;
    bool isAllowedUrl(const QUrl &url) const;
    void startCheckRequest(const QUrl &url, CheckSource source);
    bool parseReleaseJson(const QJsonObject &obj, ReleaseInfo &outInfo) const;

    void onCheckReplyFinished();
    void startDebDownload();
    void onChecksumReplyFinished();
    void onDebReadyRead();
    void onDebReplyFinished();
    void launchPackageInstall(const QString &debPath);
    void cleanupDownload();

    QNetworkAccessManager *m_nam = nullptr;
    QPointer<QNetworkReply> m_activeReply;
    QPointer<QProcess> m_installProcess;

    State m_state = State::Idle;
    CheckSource m_checkSource = CheckSource::Manifest;
    QString m_primaryCheckError;
    bool m_primarySucceeded = false;
    bool m_userInitiatedCheck = false;
    bool m_hasPendingUpdate = false;
    ReleaseInfo m_latestRelease;
    ReleaseInfo m_pendingInstall;

    std::unique_ptr<QTemporaryDir> m_tempDir;
    std::unique_ptr<QFile> m_downloadFile;
    QString m_downloadedDebPath;
    QString m_expectedSha256;
    QString m_installOutput;
};
