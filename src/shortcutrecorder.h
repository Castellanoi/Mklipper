/*
    SPDX-FileCopyrightText: 2026 Mklipper contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QKeySequence>
#include <QWidget>

#include "x11keyboard.h"

class QPushButton;
class QToolButton;

class ShortcutRecorder : public QWidget
{
    Q_OBJECT

public:
    explicit ShortcutRecorder(const QKeySequence &initial = {}, QWidget *parent = nullptr);

    QKeySequence keySequence() const { return m_sequence; }
    void setKeySequence(const QKeySequence &sequence);
    void clear();

    void setRowToolTip(const QString &tip);

Q_SIGNALS:
    void keySequenceChanged(const QKeySequence &sequence);

protected:
    bool eventFilter(QObject *watched, QEvent *ev) override;

private:
    void startRecording();
    void stopRecording();
    void openManualPicker();
    void updateDisplay();
    QString formatFriendly(const QKeySequence &seq) const;
    QString formatModsPreview(Qt::KeyboardModifiers mods) const;

    QPushButton *m_captureBtn = nullptr;
    QToolButton *m_pickerBtn = nullptr;
    QToolButton *m_clearBtn = nullptr;

    QKeySequence m_sequence;
    bool m_recording = false;
    Qt::KeyboardModifiers m_heldMods = Qt::NoModifier;
    X11ModifierMap m_modMap;
};
