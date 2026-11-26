/*
    SPDX-FileCopyrightText: 2026 Mklipper contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QObject>
#include <QSettings>
#include <QString>
#include <QStringList>

class MklipperSettings : public QObject
{
    Q_OBJECT

public:
    static MklipperSettings *self();

    void load();
    void save();

    // General
    bool keepClipboardContents() const { return m_keepClipboardContents; }
    void setKeepClipboardContents(bool v) { m_keepClipboardContents = v; }

    bool preventEmptyClipboard() const { return m_preventEmptyClipboard; }
    void setPreventEmptyClipboard(bool v) { m_preventEmptyClipboard = v; }

    bool syncClipboards() const { return m_syncClipboards; }
    void setSyncClipboards(bool v) { m_syncClipboards = v; }

    bool saveSelection() const { return m_saveSelection; }
    void setSaveSelection(bool v) { m_saveSelection = v; }

    bool saveImages() const { return m_saveImages; }
    void setSaveImages(bool v) { m_saveImages = v; }

    int maxClipItems() const { return m_maxClipItems; }
    void setMaxClipItems(int v) { m_maxClipItems = qBound(1, v, 2048); }

    bool confirmClearHistory() const { return m_confirmClearHistory; }
    void setConfirmClearHistory(bool v) { m_confirmClearHistory = v; }

    bool confirmRemoveStarred() const { return m_confirmRemoveStarred; }
    void setConfirmRemoveStarred(bool v) { m_confirmRemoveStarred = v; }

    bool autoPaste() const { return m_autoPaste; }
    void setAutoPaste(bool v) { m_autoPaste = v; }

    bool keepPopupOpen() const { return m_keepPopupOpen; }
    void setKeepPopupOpen(bool v) { m_keepPopupOpen = v; }

    bool closePopupOnFocusOut() const { return m_closePopupOnFocusOut; }
    void setClosePopupOnFocusOut(bool v) { m_closePopupOnFocusOut = v; }

    bool pastePlainText() const { return m_pastePlainText; }
    void setPastePlainText(bool v) { m_pastePlainText = v; }

    bool saveSourceInfo() const { return m_saveSourceInfo; }
    void setSaveSourceInfo(bool v) { m_saveSourceInfo = v; }

    bool showSourceInfo() const { return m_showSourceInfo; }
    void setShowSourceInfo(bool v) { m_showSourceInfo = v; }

    QString language() const { return m_language; }
    void setLanguage(const QString &v) { m_language = v; }

    QStringList ignoredApps() const { return m_ignoredApps; }
    void setIgnoredApps(const QStringList &apps);

    // Actions (URLGrabber)
    bool urlGrabberEnabled() const { return m_urlGrabberEnabled; }
    void setUrlGrabberEnabled(bool v) { m_urlGrabberEnabled = v; }

    bool actionOnSelection() const { return m_actionOnSelection; }
    void setActionOnSelection(bool v) { m_actionOnSelection = v; }

    bool replayActionInHistory() const { return m_replayActionInHistory; }
    void setReplayActionInHistory(bool v) { m_replayActionInHistory = v; }

    bool stripWhiteSpace() const { return m_stripWhiteSpace; }
    void setStripWhiteSpace(bool v) { m_stripWhiteSpace = v; }

    bool enableMagicMimeActions() const { return m_enableMagicMimeActions; }
    void setEnableMagicMimeActions(bool v) { m_enableMagicMimeActions = v; }

    int timeoutForActionPopups() const { return m_timeoutForActionPopups; }
    void setTimeoutForActionPopups(int v) { m_timeoutForActionPopups = qBound(0, v, 200); }

    // Shortcuts (popup & global X11 / Cinnamon)
    bool quickSelectDigits() const { return m_quickSelectDigits; }
    void setQuickSelectDigits(bool v) { m_quickSelectDigits = v; }

    QString shortcutPopup() const { return m_shortcutPopup; }
    void setShortcutPopup(const QString &v) { m_shortcutPopup = v; }

    QString shortcutTogglePlainText() const { return m_shortcutTogglePlainText; }
    void setShortcutTogglePlainText(const QString &v) { m_shortcutTogglePlainText = v; }

    QString shortcutTogglePause() const { return m_shortcutTogglePause; }
    void setShortcutTogglePause(const QString &v) { m_shortcutTogglePause = v; }

    QString shortcutEditCurrent() const { return m_shortcutEditCurrent; }
    void setShortcutEditCurrent(const QString &v) { m_shortcutEditCurrent = v; }

    QString shortcutShowQr() const { return m_shortcutShowQr; }
    void setShortcutShowQr(const QString &v) { m_shortcutShowQr = v; }

    QString shortcutRepeatAction() const { return m_shortcutRepeatAction; }
    void setShortcutRepeatAction(const QString &v) { m_shortcutRepeatAction = v; }

    QString shortcutCycleNext() const { return m_shortcutCycleNext; }
    void setShortcutCycleNext(const QString &v) { m_shortcutCycleNext = v; }

    QString shortcutCyclePrev() const { return m_shortcutCyclePrev; }
    void setShortcutCyclePrev(const QString &v) { m_shortcutCyclePrev = v; }

    QString shortcutClearHistory() const { return m_shortcutClearHistory; }
    void setShortcutClearHistory(const QString &v) { m_shortcutClearHistory = v; }

    // Updates
    bool autoCheckUpdates() const { return m_autoCheckUpdates; }
    void setAutoCheckUpdates(bool v) { m_autoCheckUpdates = v; }

    QString skippedUpdateVersion() const { return m_skippedUpdateVersion; }
    void setSkippedUpdateVersion(const QString &v) { m_skippedUpdateVersion = v; }

    // Autostart helper (user-level ~/.config/autostart/mklipper.desktop)
    static bool autostartEnabled();
    static void setAutostartEnabled(bool enabled);

    // Direct access to underlying QSettings for custom Actions storage
    QSettings &rawSettings() { return m_settings; }

Q_SIGNALS:
    void settingsChanged();

private:
    explicit MklipperSettings(QObject *parent = nullptr);

    QSettings m_settings;

    bool m_keepClipboardContents = true;
    bool m_preventEmptyClipboard = true;
    bool m_syncClipboards = false;
    bool m_saveSelection = false;
    bool m_saveImages = true;
    int m_maxClipItems = 50;
    bool m_confirmClearHistory = true;
    bool m_confirmRemoveStarred = true;
    bool m_autoPaste = true;
    bool m_keepPopupOpen = false;
    bool m_closePopupOnFocusOut = false;
    bool m_pastePlainText = false;
    bool m_saveSourceInfo = true;
    bool m_showSourceInfo = true;
    QString m_language = QStringLiteral("system");
    QStringList m_ignoredApps;

    bool m_urlGrabberEnabled = false;
    bool m_actionOnSelection = false;
    bool m_replayActionInHistory = false;
    bool m_stripWhiteSpace = true;
    bool m_enableMagicMimeActions = true;
    int m_timeoutForActionPopups = 8;

    bool m_quickSelectDigits = true;
    QString m_shortcutPopup = QStringLiteral("Meta+V");
    QString m_shortcutTogglePlainText;
    QString m_shortcutTogglePause;
    QString m_shortcutEditCurrent;
    QString m_shortcutShowQr;
    QString m_shortcutRepeatAction;
    QString m_shortcutCycleNext;
    QString m_shortcutCyclePrev;
    QString m_shortcutClearHistory;

    bool m_autoCheckUpdates = true;
    QString m_skippedUpdateVersion;
};
