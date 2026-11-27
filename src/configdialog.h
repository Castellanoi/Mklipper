/*
    SPDX-FileCopyrightText: 2026 Mklipper contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QDialog>
#include <QList>

#include "urlgrabber.h"

class QCheckBox;
class QComboBox;
class QLineEdit;
class QListWidget;
class QSlider;
class QSpinBox;
class ShortcutRecorder;
class TooltipPreviewWidget;

class ConfigDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ConfigDialog(URLGrabber *urlGrabber, QWidget *parent = nullptr);
    ~ConfigDialog() override;

Q_SIGNALS:
    void checkForUpdatesRequested();
    void showManualRequested();

private Q_SLOTS:
    void saveAndAccept();
    void addAction();
    void editAction();
    void deleteAction();
    void addIgnoredApp();
    void removeIgnoredApp();
    void exportPinnedItems();
    void importPinnedItems();

private:
    void refreshActionList();

    URLGrabber *m_urlGrabber = nullptr;
    ActionList m_editedActions;

    // General tab
    QCheckBox *m_keepContents = nullptr;
    QCheckBox *m_preventEmpty = nullptr;
    QCheckBox *m_saveImages = nullptr;
    QCheckBox *m_saveSelection = nullptr;
    QCheckBox *m_syncClipboards = nullptr;
    QCheckBox *m_autostart = nullptr;
    QCheckBox *m_autoCheckUpdates = nullptr;
    QComboBox *m_languageCombo = nullptr;
    QCheckBox *m_confirmClear = nullptr;
    QCheckBox *m_autoPaste = nullptr;
    QCheckBox *m_pastePlainText = nullptr;
    QCheckBox *m_keepPopupOpen = nullptr;
    QCheckBox *m_closePopupOnFocusOut = nullptr;
    QCheckBox *m_saveSourceInfo = nullptr;
    QCheckBox *m_showSourceInfo = nullptr;
    QCheckBox *m_showTrayBadge = nullptr;
    QSpinBox *m_maxItems = nullptr;
    QLineEdit *m_ignoredAppEdit = nullptr;
    QListWidget *m_ignoredAppsList = nullptr;

    // Actions tab
    QCheckBox *m_urlGrabberEnabled = nullptr;
    QCheckBox *m_actionOnSelection = nullptr;
    QCheckBox *m_replayInHistory = nullptr;
    QCheckBox *m_stripWhiteSpace = nullptr;
    QCheckBox *m_magicMime = nullptr;
    QSpinBox *m_actionTimeout = nullptr;
    QListWidget *m_actionsList = nullptr;

    // Shortcuts tab
    QCheckBox *m_quickSelectDigits = nullptr;
    ShortcutRecorder *m_keyPopup = nullptr;
    ShortcutRecorder *m_keyTogglePlainText = nullptr;
    ShortcutRecorder *m_keyTogglePause = nullptr;
    ShortcutRecorder *m_keyEditCurrent = nullptr;
    ShortcutRecorder *m_keyShowQr = nullptr;
    ShortcutRecorder *m_keyRepeatAction = nullptr;
    ShortcutRecorder *m_keyCycleNext = nullptr;
    ShortcutRecorder *m_keyCyclePrev = nullptr;
    ShortcutRecorder *m_keyClearHistory = nullptr;

    // Tooltips & action bar tab
    QComboBox *m_actionBarModeCombo = nullptr;
    QCheckBox *m_actionBarAnimation = nullptr;
    QCheckBox *m_tipItemInfo = nullptr;
    QCheckBox *m_tipItemActions = nullptr;
    QCheckBox *m_tipHeaderButtons = nullptr;
    QCheckBox *m_tipPlainTextSwitch = nullptr;
    QSlider *m_tipDelaySlider = nullptr;
    QSpinBox *m_tipDelaySpin = nullptr;
    TooltipPreviewWidget *m_tooltipPreview = nullptr;
};
