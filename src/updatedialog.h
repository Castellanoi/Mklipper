/*
    SPDX-FileCopyrightText: 2026 Mklipper contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QDialog>

#include "updater.h"

class QLabel;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;
class QTextBrowser;

class UpdateDialog : public QDialog
{
    Q_OBJECT

public:
    explicit UpdateDialog(Updater *updater, QWidget *parent = nullptr);

    void startCheck();
    void showRelease(const ReleaseInfo &release);

protected:
    void closeEvent(QCloseEvent *event) override;
    void reject() override;

private Q_SLOTS:
    void onStateChanged(Updater::State state);
    void onUpdateAvailable(const ReleaseInfo &release, bool userInitiated);
    void onAlreadyUpToDate(const QString &currentVersion, bool userInitiated);
    void onCheckFailed(const QString &errorMessage, bool userInitiated);
    void onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void onStatusMessage(const QString &message);
    void onInstallSucceeded(const ReleaseInfo &release);
    void onInstallFailed(const QString &errorMessage, const QString &details);
    void onCancelled();

    void onUpdateClicked();
    void onSkipVersionClicked();
    void onOpenReleasePageClicked();

private:
    enum class Mode {
        Checking,
        UpdateAvailable,
        UpToDate,
        CheckError,
        Downloading,
        Installing,
        InstallError,
        InstallSuccess,
    };

    void setMode(Mode mode);
    static QString formatSize(qint64 bytes);

    Updater *m_updater = nullptr;
    ReleaseInfo m_release;
    Mode m_mode = Mode::Checking;

    QLabel *m_titleLabel = nullptr;
    QLabel *m_subtitleLabel = nullptr;
    QTextBrowser *m_notesBrowser = nullptr;
    QLabel *m_statusLabel = nullptr;
    QProgressBar *m_progressBar = nullptr;
    QPlainTextEdit *m_detailsEdit = nullptr;

    QPushButton *m_openGithubBtn = nullptr;
    QPushButton *m_skipBtn = nullptr;
    QPushButton *m_retryBtn = nullptr;
    QPushButton *m_updateBtn = nullptr;
    QPushButton *m_closeBtn = nullptr;
};
