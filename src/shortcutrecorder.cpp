/*
    SPDX-FileCopyrightText: 2026 Mklipper contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "shortcutrecorder.h"

#include "flaticons.h"
#include "themestyle.h"

#include <QApplication>
#include <QComboBox>
#include <QDialog>
#include <QEvent>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>

namespace
{
struct KeyChoice {
    QString label;
    Qt::Key key;
};

QList<KeyChoice> buildKeyChoices()
{
    QList<KeyChoice> items;
    items.reserve(72);

    // Letters A-Z
    for (int k = Qt::Key_A; k <= Qt::Key_Z; ++k) {
        items.append({QString(QChar('A' + (k - Qt::Key_A))), static_cast<Qt::Key>(k)});
    }
    // Digits 0-9
    for (int k = Qt::Key_0; k <= Qt::Key_9; ++k) {
        items.append({QString(QChar('0' + (k - Qt::Key_0))), static_cast<Qt::Key>(k)});
    }
    // Function keys F1-F24 (including F13-F24 used by gaming macro keyboards such as Corsair K95 G-keys)
    for (int i = 1; i <= 24; ++i) {
        items.append({QStringLiteral("F%1").arg(i), static_cast<Qt::Key>(Qt::Key_F1 + (i - 1))});
    }

    // Navigation & common special keys
    items.append({QStringLiteral("Space"), Qt::Key_Space});
    items.append({QStringLiteral("Tab"), Qt::Key_Tab});
    items.append({QStringLiteral("Return"), Qt::Key_Return});
    items.append({QStringLiteral("Insert"), Qt::Key_Insert});
    items.append({QStringLiteral("Delete"), Qt::Key_Delete});
    items.append({QStringLiteral("Home"), Qt::Key_Home});
    items.append({QStringLiteral("End"), Qt::Key_End});
    items.append({QStringLiteral("PageUp"), Qt::Key_PageUp});
    items.append({QStringLiteral("PageDown"), Qt::Key_PageDown});
    items.append({QStringLiteral("Up"), Qt::Key_Up});
    items.append({QStringLiteral("Down"), Qt::Key_Down});
    items.append({QStringLiteral("Left"), Qt::Key_Left});
    items.append({QStringLiteral("Right"), Qt::Key_Right});
    items.append({QStringLiteral("Print"), Qt::Key_Print});
    items.append({QStringLiteral("Pause"), Qt::Key_Pause});
    items.append({QStringLiteral("-"), Qt::Key_Minus});
    items.append({QStringLiteral("="), Qt::Key_Equal});
    items.append({QStringLiteral("["), Qt::Key_BracketLeft});
    items.append({QStringLiteral("]"), Qt::Key_BracketRight});
    items.append({QStringLiteral(";"), Qt::Key_Semicolon});
    items.append({QStringLiteral("'"), Qt::Key_Apostrophe});
    items.append({QStringLiteral(","), Qt::Key_Comma});
    items.append({QStringLiteral("."), Qt::Key_Period});
    items.append({QStringLiteral("/"), Qt::Key_Slash});
    items.append({QStringLiteral("\\"), Qt::Key_Backslash});
    items.append({QStringLiteral("`"), Qt::Key_QuoteLeft});

    return items;
}

Qt::Key resolvePrimaryKey(QKeyEvent *ev)
{
    if (X11Display *dpy = X11Keyboard::display()) {
        // 1. Prefer level-0 base KeySym from physical scanCode (immune to Shift symbols & non-Latin layouts)
        const X11KeySym baseSym = X11Keyboard::baseKeysymForKeycode(dpy, ev->nativeScanCode());
        if (baseSym != 0 && !X11Keyboard::isModifierKeysym(baseSym)) {
            const Qt::Key mapped = X11Keyboard::keysymToQtKey(baseSym);
            if (mapped != Qt::Key_unknown) {
                return mapped;
            }
        }
    }

    // 2. Fallback to event's native virtual KeySym
    const X11KeySym virtSym = static_cast<X11KeySym>(ev->nativeVirtualKey());
    if (virtSym != 0 && !X11Keyboard::isModifierKeysym(virtSym)) {
        const Qt::Key mapped = X11Keyboard::keysymToQtKey(virtSym);
        if (mapped != Qt::Key_unknown) {
            return mapped;
        }
    }

    // 3. Fallback to Qt's key value
    const int qtKey = ev->key();
    if (qtKey == Qt::Key_Backtab) {
        return Qt::Key_Tab;
    }
    if (qtKey == 0 || qtKey == Qt::Key_unknown || X11Keyboard::isModifierQtKey(qtKey)) {
        return Qt::Key_unknown;
    }
    return static_cast<Qt::Key>(qtKey);
}
} // namespace

ShortcutRecorder::ShortcutRecorder(const QKeySequence &initial, QWidget *parent)
    : QWidget(parent)
    , m_sequence(initial.count() > 1 ? QKeySequence(initial[0]) : initial)
{
    // Disable IME so ibus/fcitx never swallows Ctrl/Alt/Super/Shift from gaming/NKRO/ckb-next keyboards
    setAttribute(Qt::WA_InputMethodEnabled, false);

    const ThemeTokens t = ThemeTokens::fromPalette(QApplication::palette());

    auto *hl = new QHBoxLayout(this);
    hl->setContentsMargins(0, 0, 0, 0);
    hl->setSpacing(6);

    m_captureBtn = new QPushButton(this);
    m_captureBtn->setAttribute(Qt::WA_InputMethodEnabled, false);
    m_captureBtn->setFocusPolicy(Qt::StrongFocus);
    m_captureBtn->setCursor(Qt::PointingHandCursor);
    m_captureBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_captureBtn->setMinimumHeight(32);
    m_captureBtn->installEventFilter(this);
    connect(m_captureBtn, &QPushButton::clicked, this, [this]() {
        if (m_recording) {
            stopRecording();
        } else {
            startRecording();
        }
    });

    m_pickerBtn = new QToolButton(this);
    m_pickerBtn->setIcon(FlatIcons::icon(FlatIcon::Keyboard, t.textSecondary, t.textPrimary, t.textMuted, 15));
    m_pickerBtn->setIconSize(QSize(15, 15));
    m_pickerBtn->setFixedSize(32, 32);
    m_pickerBtn->setCursor(Qt::PointingHandCursor);
    m_pickerBtn->setToolTip(tr("Elegir modificadores y tecla manualmente (útil si el sistema o tu teclado interceptan Ctrl/Alt/Super)"));
    m_pickerBtn->setStyleSheet(
        QStringLiteral("QToolButton { background-color: %1; border: none; border-radius: 7px; }"
                       "QToolButton:hover { background-color: %2; }")
            .arg(ThemeStyle::rgbaCss(t.surfaceHoverBg), ThemeStyle::rgbaCss(t.surfaceActiveBg)));
    connect(m_pickerBtn, &QToolButton::clicked, this, &ShortcutRecorder::openManualPicker);

    m_clearBtn = new QToolButton(this);
    m_clearBtn->setIcon(FlatIcons::icon(FlatIcon::ClearKey, t.textSecondary, t.danger, t.textMuted, 15));
    m_clearBtn->setIconSize(QSize(15, 15));
    m_clearBtn->setFixedSize(32, 32);
    m_clearBtn->setCursor(Qt::PointingHandCursor);
    m_clearBtn->setToolTip(tr("Quita el atajo asignado a esta acción."));
    m_clearBtn->setStyleSheet(
        QStringLiteral("QToolButton { background-color: %1; border: none; border-radius: 7px; }"
                       "QToolButton:hover { background-color: %2; }")
            .arg(ThemeStyle::rgbaCss(t.surfaceHoverBg), ThemeStyle::rgbaCss(t.dangerSoft)));
    connect(m_clearBtn, &QToolButton::clicked, this, &ShortcutRecorder::clear);

    hl->addWidget(m_captureBtn, 1);
    hl->addWidget(m_pickerBtn);
    hl->addWidget(m_clearBtn);

    updateDisplay();
}

void ShortcutRecorder::setKeySequence(const QKeySequence &sequence)
{
    const QKeySequence normalized = (sequence.count() > 1) ? QKeySequence(sequence[0]) : sequence;
    if (m_sequence == normalized) {
        updateDisplay();
        return;
    }
    m_sequence = normalized;
    updateDisplay();
    Q_EMIT keySequenceChanged(m_sequence);
}

void ShortcutRecorder::clear()
{
    stopRecording();
    setKeySequence(QKeySequence());
}

void ShortcutRecorder::setRowToolTip(const QString &tip)
{
    setToolTip(tip);
    if (m_captureBtn) {
        m_captureBtn->setToolTip(tip);
    }
}

void ShortcutRecorder::startRecording()
{
    if (X11Display *dpy = X11Keyboard::display()) {
        m_modMap = X11Keyboard::queryModifierMap(dpy);
    }
    m_recording = true;
    m_heldMods = Qt::NoModifier;
    m_captureBtn->setFocus(Qt::MouseFocusReason);
    updateDisplay();
}

void ShortcutRecorder::stopRecording()
{
    if (!m_recording) {
        return;
    }
    m_recording = false;
    m_heldMods = Qt::NoModifier;
    updateDisplay();
}

bool ShortcutRecorder::eventFilter(QObject *watched, QEvent *ev)
{
    if (watched != m_captureBtn) {
        return QWidget::eventFilter(watched, ev);
    }

    if (ev->type() == QEvent::FocusOut && m_recording) {
        stopRecording();
        return false;
    }

    if (!m_recording) {
        return false;
    }

    // Prevent Tab/Shift+Tab from moving focus away while recording a shortcut
    if (ev->type() == QEvent::ShortcutOverride) {
        ev->accept();
        return true;
    }

    if (ev->type() == QEvent::KeyPress) {
        auto *ke = static_cast<QKeyEvent *>(ev);

        // Combine modifiers from 3 independent sources (Qt + X11 nativeModifiers + live XQueryKeymap hardware state)
        Qt::KeyboardModifiers mods = ke->modifiers() & (Qt::ShiftModifier | Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier);
        mods |= m_modMap.x11MaskToQt(ke->nativeModifiers());
        if (X11Display *dpy = X11Keyboard::display()) {
            mods |= X11Keyboard::queryLiveModifiers(dpy, m_modMap);
        }

        // Also fold in the currently pressed modifier key itself (XQueryKeymap/state may lag by 1 event)
        switch (ke->key()) {
        case Qt::Key_Control: mods |= Qt::ControlModifier; break;
        case Qt::Key_Shift: mods |= Qt::ShiftModifier; break;
        case Qt::Key_Alt: mods |= Qt::AltModifier; break;
        case Qt::Key_Meta:
        case Qt::Key_Super_L:
        case Qt::Key_Super_R:
        case Qt::Key_Hyper_L:
        case Qt::Key_Hyper_R: mods |= Qt::MetaModifier; break;
        default: break;
        }

        // Escape alone cancels recording without changing the current shortcut
        if (ke->key() == Qt::Key_Escape && mods == Qt::NoModifier) {
            stopRecording();
            return true;
        }
        // Backspace alone clears the shortcut
        if (ke->key() == Qt::Key_Backspace && mods == Qt::NoModifier) {
            clear();
            return true;
        }

        const Qt::Key primary = resolvePrimaryKey(ke);
        if (primary == Qt::Key_unknown) {
            // Modifier-only press: show live modifier preview so the user sees instant feedback
            m_heldMods = mods;
            updateDisplay();
            return true;
        }

        const QKeyCombination combo(mods, primary);
        stopRecording();
        setKeySequence(QKeySequence(combo));
        return true;
    }

    if (ev->type() == QEvent::KeyRelease) {
        if (X11Display *dpy = X11Keyboard::display()) {
            m_heldMods = X11Keyboard::queryLiveModifiers(dpy, m_modMap);
        } else {
            auto *ke = static_cast<QKeyEvent *>(ev);
            m_heldMods = ke->modifiers() & (Qt::ShiftModifier | Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier);
        }
        updateDisplay();
        return true;
    }

    return false;
}

void ShortcutRecorder::openManualPicker()
{
    stopRecording();

    QDialog dlg(this);
    dlg.setWindowTitle(tr("Configurar atajo manualmente"));
    dlg.setModal(true);
    dlg.setMinimumWidth(360);

    const ThemeTokens t = ThemeTokens::fromPalette(QApplication::palette());
    dlg.setStyleSheet(ThemeStyle::dialogStyleSheet(t));

    auto *vl = new QVBoxLayout(&dlg);
    vl->setContentsMargins(16, 16, 16, 14);
    vl->setSpacing(12);

    auto *hint = new QLabel(tr("Selecciona los modificadores y la tecla principal:"), &dlg);
    hint->setWordWrap(true);
    vl->addWidget(hint);

    // 4 clean toggleable modifier pills (works on 100% of keyboards even if OS/hardware blocks a modifier)
    auto *modRow = new QHBoxLayout;
    modRow->setSpacing(6);

    const Qt::KeyboardModifiers curMods = m_sequence.isEmpty() ? Qt::NoModifier : m_sequence[0].keyboardModifiers();
    const Qt::Key curKey = m_sequence.isEmpty() ? Qt::Key_unknown : m_sequence[0].key();

    const QString pillCss = QStringLiteral(
        "QPushButton { background-color: %1; color: %2; border: 1px solid %3; border-radius: 7px; padding: 6px 10px; font-weight: 600; }"
        "QPushButton:hover { background-color: %4; }"
        "QPushButton:checked { background-color: %5; color: %6; border-color: %5; }")
                                .arg(ThemeStyle::rgbaCss(t.inputBg),
                                     ThemeStyle::rgbaCss(t.textSecondary),
                                     ThemeStyle::rgbaCss(t.borderSubtle),
                                     ThemeStyle::rgbaCss(t.surfaceHoverBg),
                                     ThemeStyle::rgbaCss(t.accent),
                                     ThemeStyle::rgbaCss(t.accentFg));

    auto makeModPill = [&](const QString &label, Qt::KeyboardModifier mod) {
        auto *b = new QPushButton(label, &dlg);
        b->setCheckable(true);
        b->setChecked(curMods & mod);
        b->setCursor(Qt::PointingHandCursor);
        b->setStyleSheet(pillCss);
        modRow->addWidget(b, 1);
        return b;
    };

    auto *btnCtrl = makeModPill(QStringLiteral("Ctrl"), Qt::ControlModifier);
    auto *btnAlt = makeModPill(QStringLiteral("Alt"), Qt::AltModifier);
    auto *btnSuper = makeModPill(QStringLiteral("Super"), Qt::MetaModifier);
    auto *btnShift = makeModPill(QStringLiteral("Shift"), Qt::ShiftModifier);
    vl->addLayout(modRow);

    auto *keyRow = new QHBoxLayout;
    keyRow->setSpacing(8);
    auto *keyLbl = new QLabel(tr("Tecla:"), &dlg);
    auto *keyCombo = new QComboBox(&dlg);
    keyCombo->setMaxVisibleItems(14);
    const auto choices = buildKeyChoices();
    int selectedIdx = 0;
    for (int i = 0; i < choices.size(); ++i) {
        keyCombo->addItem(choices.at(i).label, static_cast<int>(choices.at(i).key));
        if (choices.at(i).key == curKey) {
            selectedIdx = i;
        }
    }
    keyCombo->setCurrentIndex(selectedIdx);
    keyRow->addWidget(keyLbl);
    keyRow->addWidget(keyCombo, 1);
    vl->addLayout(keyRow);

    auto *btnRow = new QHBoxLayout;
    btnRow->setSpacing(8);
    btnRow->addStretch();
    auto *cancelBtn = new QPushButton(tr("Cancelar"), &dlg);
    cancelBtn->setCursor(Qt::PointingHandCursor);
    auto *applyBtn = new QPushButton(
        FlatIcons::icon(FlatIcon::Check, t.accentFg, t.accentFg, t.textMuted, 15),
        tr("Aplicar"),
        &dlg);
    applyBtn->setProperty("primary", true);
    applyBtn->setIconSize(QSize(15, 15));
    applyBtn->setCursor(Qt::PointingHandCursor);
    applyBtn->setDefault(true);
    connect(cancelBtn, &QPushButton::clicked, &dlg, &QDialog::reject);
    connect(applyBtn, &QPushButton::clicked, &dlg, &QDialog::accept);
    btnRow->addWidget(cancelBtn);
    btnRow->addWidget(applyBtn);
    vl->addLayout(btnRow);

    if (dlg.exec() == QDialog::Accepted) {
        Qt::KeyboardModifiers chosenMods = Qt::NoModifier;
        if (btnCtrl->isChecked()) chosenMods |= Qt::ControlModifier;
        if (btnAlt->isChecked()) chosenMods |= Qt::AltModifier;
        if (btnSuper->isChecked()) chosenMods |= Qt::MetaModifier;
        if (btnShift->isChecked()) chosenMods |= Qt::ShiftModifier;
        const auto chosenKey = static_cast<Qt::Key>(keyCombo->currentData().toInt());
        if (chosenKey != Qt::Key_unknown) {
            setKeySequence(QKeySequence(QKeyCombination(chosenMods, chosenKey)));
        }
    }
}

void ShortcutRecorder::updateDisplay()
{
    const ThemeTokens t = ThemeTokens::fromPalette(QApplication::palette());

    if (m_recording) {
        const QString text = (m_heldMods != Qt::NoModifier)
            ? formatModsPreview(m_heldMods)
            : tr("Presiona el atajo… (Esc: cancelar)");
        m_captureBtn->setText(text);
        m_captureBtn->setStyleSheet(
            QStringLiteral("QPushButton { background-color: %1; color: %2; border: 1.5px solid %3; "
                           "border-radius: 7px; padding: 5px 10px; text-align: left; font-weight: 600; }")
                .arg(ThemeStyle::rgbaCss(t.accentSoft),
                     ThemeStyle::rgbaCss(t.accent),
                     ThemeStyle::rgbaCss(t.accent)));
        return;
    }

    const bool hasSeq = !m_sequence.isEmpty();
    m_captureBtn->setText(hasSeq ? formatFriendly(m_sequence) : tr("Sin asignar — clic para grabar"));
    m_captureBtn->setStyleSheet(
        QStringLiteral("QPushButton { background-color: %1; color: %2; border: 1px solid %3; "
                       "border-radius: 7px; padding: 5px 10px; text-align: left; font-weight: %4; }"
                       "QPushButton:hover { background-color: %5; border-color: %6; }")
            .arg(ThemeStyle::rgbaCss(t.inputBg),
                 ThemeStyle::rgbaCss(hasSeq ? t.textPrimary : t.textMuted),
                 ThemeStyle::rgbaCss(t.borderSubtle),
                 hasSeq ? QStringLiteral("600") : QStringLiteral("400"),
                 ThemeStyle::rgbaCss(t.surfaceHoverBg),
                 ThemeStyle::rgbaCss(t.accent)));

    if (m_clearBtn) {
        m_clearBtn->setEnabled(hasSeq);
    }
}

QString ShortcutRecorder::formatModsPreview(Qt::KeyboardModifiers mods) const
{
    QStringList parts;
    if (mods & Qt::ControlModifier) parts.append(QStringLiteral("Ctrl"));
    if (mods & Qt::AltModifier) parts.append(QStringLiteral("Alt"));
    if (mods & Qt::MetaModifier) parts.append(QStringLiteral("Super"));
    if (mods & Qt::ShiftModifier) parts.append(QStringLiteral("Shift"));
    parts.append(QStringLiteral("…"));
    return parts.join(QStringLiteral(" + "));
}

QString ShortcutRecorder::formatFriendly(const QKeySequence &seq) const
{
    if (seq.isEmpty()) {
        return {};
    }
    const QKeyCombination combo = seq[0];
    const Qt::KeyboardModifiers mods = combo.keyboardModifiers();
    QStringList parts;
    if (mods & Qt::ControlModifier) parts.append(QStringLiteral("Ctrl"));
    if (mods & Qt::AltModifier) parts.append(QStringLiteral("Alt"));
    if (mods & Qt::MetaModifier) parts.append(QStringLiteral("Super"));
    if (mods & Qt::ShiftModifier) parts.append(QStringLiteral("Shift"));
    const QString keyName = QKeySequence(combo.key()).toString(QKeySequence::PortableText);
    if (!keyName.isEmpty()) {
        parts.append(keyName);
    }
    return parts.join(QStringLiteral(" + "));
}
