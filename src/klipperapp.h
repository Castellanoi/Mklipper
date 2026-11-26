/*
    SPDX-FileCopyrightText: Andrew Stanley-Jones <asj@cban.com>
    SPDX-FileCopyrightText: 2004 Esben Mose Hansen <kde@mosehansen.dk>
    SPDX-FileCopyrightText: 2026 Mklipper contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <memory>

#include <QDBusContext>
#include <QObject>
#include <QPointer>
#include <QStringList>
#include <QSystemTrayIcon>

#include "historyitem.h"

class ConfigDialog;
class GlobalShortcutManager;
class HelpDialog;
class HistoryCycler;
class HistoryModel;
class KlipperPopup;
class QAction;
class QEvent;
class QFileSystemWatcher;
class QMenu;
class QModelIndex;
class QTimer;
class SystemClipboard;
class URLGrabber;
class UpdateDialog;
class Updater;
struct ReleaseInfo;

class KlipperApp : public QObject, public QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.klipper.klipper")

public:
    explicit KlipperApp(QObject *parent = nullptr);
    ~KlipperApp() override;

protected:
    bool eventFilter(QObject *watched, QEvent *ev) override;

public Q_SLOTS:
    Q_SCRIPTABLE QString getClipboardContents();
    Q_SCRIPTABLE void setClipboardContents(const QString &s);
    Q_SCRIPTABLE void clearClipboardContents();
    Q_SCRIPTABLE void clearClipboardHistory();
    Q_SCRIPTABLE void clearClipboardHistoryInteractive();
    Q_SCRIPTABLE void saveClipboardHistory();
    Q_SCRIPTABLE QStringList getClipboardHistoryMenu();
    Q_SCRIPTABLE QString getClipboardHistoryItem(int i);
    Q_SCRIPTABLE void showKlipperPopupMenu();
    Q_SCRIPTABLE void toggleKlipperPopupMenu();
    Q_SCRIPTABLE void togglePastePlainText();
    Q_SCRIPTABLE void toggleCapturePaused();
    Q_SCRIPTABLE void editCurrentClipboard();
    Q_SCRIPTABLE void showCurrentBarcode();
    Q_SCRIPTABLE void showKlipperManuallyInvokeActionMenu();
    Q_SCRIPTABLE void configure();
    Q_SCRIPTABLE void showManual();
    Q_SCRIPTABLE void checkForUpdates();
    Q_SCRIPTABLE void reloadConfig();
    Q_SCRIPTABLE void quit();
    Q_SCRIPTABLE void slotCycleNext();
    Q_SCRIPTABLE void slotCyclePrev();
    void slotRepeatAction(bool fromHistory = false);
    void setURLGrabberEnabled(bool enable);

Q_SIGNALS:
    Q_SCRIPTABLE void clipboardHistoryUpdated();

private Q_SLOTS:
    void onTrayActivated(QSystemTrayIcon::ActivationReason reason);
    void onHistoryChanged(bool isTop);
    void onHistoryRowsInserted(const QModelIndex &parent, int first, int last);
    void onHistoryRowsRemoved();
    void resetUnreadCount();
    void onInvokeActionByUuid(const QString &uuid);
    void showActionPopupMenu(QMenu *menu);
    void updateTrayTooltip();
    void onUpdateAvailable(const ReleaseInfo &release, bool userInitiated);
    void onUpdateInstallSucceeded(const ReleaseInfo &release);
    void onDconfNotify(const QString &prefix, const QStringList &changes, const QString &tag);

private:
    void setupTrayIcon();
    void updateTrayIcon();
    void rebuildTrayMenu();
    void syncThemeLive();
    void recreatePopup();
    void registerShortcuts();
    void offerActions(const HistoryItemConstPtr &item, bool fromSelection);
    QString cycleNotificationText() const;
    UpdateDialog *ensureUpdateDialog();
    void setupUpgradeWatcher();
    void checkInstalledBinaryUpdated();
    void restartAfterUpdate();

    std::shared_ptr<SystemClipboard> m_clip;
    std::shared_ptr<HistoryModel> m_historyModel;
    HistoryCycler *m_historyCycler = nullptr;
    URLGrabber *m_urlGrabber = nullptr;
    GlobalShortcutManager *m_shortcuts = nullptr;
    Updater *m_updater = nullptr;
    std::unique_ptr<KlipperPopup> m_popup;
    QPointer<ConfigDialog> m_configDialog;
    QPointer<HelpDialog> m_helpDialog;
    QPointer<UpdateDialog> m_updateDialog;

    QSystemTrayIcon *m_trayIcon = nullptr;
    QMenu *m_trayMenu = nullptr;
    QAction *m_togglePauseAct = nullptr;
    QAction *m_toggleActionsAct = nullptr;
    QTimer *m_themeSyncTimer = nullptr;
    bool m_syncingTheme = false;

    QFileSystemWatcher *m_exeWatcher = nullptr;
    QTimer *m_upgradeDebounceTimer = nullptr;
    QString m_watchedExePath;
    quint64 m_exeDev = 0;
    quint64 m_exeIno = 0;
    bool m_restarting = false;

    bool m_urlGrabberEnabled = false;
    QString m_lastUrlGrabberText;
    int m_unreadCount = 0;
};
