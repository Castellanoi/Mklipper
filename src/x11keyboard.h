/*
    SPDX-FileCopyrightText: 2026 Mklipper contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QKeySequence>
#include <QList>
#include <QString>
#include <Qt>

struct _XDisplay;

using X11Display = _XDisplay;
using X11KeyCode = unsigned char;
using X11KeySym = unsigned long;

struct X11ModifierMap {
    unsigned int shiftMask = (1u << 0);
    unsigned int controlMask = (1u << 2);
    unsigned int altMask = (1u << 3);
    unsigned int superMask = (1u << 6);

    unsigned int capsLockMask = (1u << 1);
    unsigned int numLockMask = 0;
    unsigned int scrollLockMask = 0;
    unsigned int level3Mask = 0;

    // All active X11 keycodes mapped to each logical modifier (discovered live via XGetModifierMapping)
    QList<X11KeyCode> shiftKeycodes;
    QList<X11KeyCode> controlKeycodes;
    QList<X11KeyCode> altKeycodes;
    QList<X11KeyCode> superKeycodes;

    unsigned int relevantMask() const
    {
        return shiftMask | controlMask | altMask | superMask;
    }

    QList<unsigned int> ignoredLockCombinations() const;
    unsigned int qtModsToX11(Qt::KeyboardModifiers mods) const;
    Qt::KeyboardModifiers x11MaskToQt(unsigned int mask) const;
};

namespace X11Keyboard
{
X11Display *display();

// Queries the X11 server's live modifier mapping (zero hardcoded Mod1/Mod2/Mod4/ScrollLock assumptions)
X11ModifierMap queryModifierMap(X11Display *dpy);

// Queries physical/virtual keyboard state right now via XQueryKeymap across all active modifier keycodes
Qt::KeyboardModifiers queryLiveModifiers(X11Display *dpy, const X11ModifierMap &map);

// Returns true if any non-lock modifier (Shift/Ctrl/Alt/Super/Meta/Hyper) is physically held on any keyboard
bool anyBlockingModifierHeld(X11Display *dpy);

// Finds all keycodes mapped to a given X11 KeySym across the current keyboard map
QList<X11KeyCode> keycodesForKeysym(X11Display *dpy, X11KeySym sym);

// Resolves the base (level 0, unshifted) KeySym for an X11 scanCode (keycode), independent of Shift/non-Latin group
X11KeySym baseKeysymForKeycode(X11Display *dpy, unsigned int scanCode);

// Bidirectional conversion supporting letters, digits, F1–F35 (incl. gaming macro F13–F24), KP_*, navigation, and punctuation
X11KeySym qtKeyToKeysym(Qt::Key key);
Qt::Key keysymToQtKey(X11KeySym sym);
bool isModifierQtKey(int key);
bool isModifierKeysym(X11KeySym sym);

// Formats a single QKeyCombination cleanly for Cinnamon gsettings (<Primary><Alt><Shift><Super>key)
QString qtCombinationToCinnamonAccel(const QKeyCombination &combo);
} // namespace X11Keyboard
