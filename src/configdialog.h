/*
    SPDX-FileCopyrightText: 2026 Mklipper contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QDialog>
#include <QList>

#include "urlgrabber.h"

class QCheckBox;
class QKeySequenceEdit;
class QListWidget;
class QSpinBox;

class ConfigDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ConfigDialog(URLGrabber *urlGrabber, QWidget *parent = nullptr);
    ~ConfigDialog() override;

private Q_SLOTS:
    void saveAndAccept();
    void addAction();
    void editAction();
    void deleteAction();

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
    QCheckBox *m_confirmClear = nullptr;
    QCheckBox *m_autoPaste = nullptr;
    QSpinBox *m_maxItems = nullptr;

    // Actions tab
    QCheckBox *m_urlGrabberEnabled = nullptr;
    QCheckBox *m_actionOnSelection = nullptr;
    QCheckBox *m_replayInHistory = nullptr;
    QCheckBox *m_stripWhiteSpace = nullptr;
    QCheckBox *m_magicMime = nullptr;
    QSpinBox *m_actionTimeout = nullptr;
    QListWidget *m_actionsList = nullptr;

    // Shortcuts tab
    QKeySequenceEdit *m_keyPopup = nullptr;
    QKeySequenceEdit *m_keyCycleNext = nullptr;
    QKeySequenceEdit *m_keyCyclePrev = nullptr;
    QKeySequenceEdit *m_keyRepeatAction = nullptr;
    QKeySequenceEdit *m_keyClearHistory = nullptr;
};
