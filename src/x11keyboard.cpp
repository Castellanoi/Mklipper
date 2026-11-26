/*
    SPDX-FileCopyrightText: 2026 Mklipper contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "x11keyboard.h"

#include <QGuiApplication>
#include <QSet>

#include <X11/XKBlib.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>

static_assert(std::is_same_v<X11Display, Display>, "X11Display must match Xlib Display");
static_assert(std::is_same_v<X11KeyCode, KeyCode>, "X11KeyCode must match Xlib KeyCode");
static_assert(std::is_same_v<X11KeySym, KeySym>, "X11KeySym must match Xlib KeySym");

namespace
{
bool isKeycodeHeld(const char keys[32], KeyCode code)
{
    if (code == 0) {
        return false;
    }
    return (keys[code >> 3] & (1 << (code & 7))) != 0;
}

bool anyKeycodeHeld(const char keys[32], const QList<KeyCode> &codes)
{
    for (KeyCode c : codes) {
        if (isKeycodeHeld(keys, c)) {
            return true;
        }
    }
    return false;
}

void appendUnique(QList<KeyCode> &list, KeyCode code)
{
    if (code != 0 && !list.contains(code)) {
        list.append(code);
    }
}
} // namespace

QList<unsigned int> X11ModifierMap::ignoredLockCombinations() const
{
    QList<unsigned int> lockBits;
    if (capsLockMask != 0) {
        lockBits.append(capsLockMask);
    }
    if (numLockMask != 0 && !lockBits.contains(numLockMask)) {
        lockBits.append(numLockMask);
    }
    if (scrollLockMask != 0 && !lockBits.contains(scrollLockMask)) {
        lockBits.append(scrollLockMask);
    }

    QList<unsigned int> combos;
    const int count = 1 << lockBits.size();
    combos.reserve(count);
    for (int maskIdx = 0; maskIdx < count; ++maskIdx) {
        unsigned int combined = 0;
        for (int bit = 0; bit < lockBits.size(); ++bit) {
            if (maskIdx & (1 << bit)) {
                combined |= lockBits.at(bit);
            }
        }
        if (!combos.contains(combined)) {
            combos.append(combined);
        }
    }
    return combos;
}

unsigned int X11ModifierMap::qtModsToX11(Qt::KeyboardModifiers mods) const
{
    unsigned int mask = 0;
    if (mods & Qt::ShiftModifier) {
        mask |= shiftMask;
    }
    if (mods & Qt::ControlModifier) {
        mask |= controlMask;
    }
    if (mods & Qt::AltModifier) {
        mask |= altMask;
    }
    if (mods & Qt::MetaModifier) {
        mask |= superMask;
    }
    return mask;
}

Qt::KeyboardModifiers X11ModifierMap::x11MaskToQt(unsigned int mask) const
{
    Qt::KeyboardModifiers mods = Qt::NoModifier;
    if (shiftMask && (mask & shiftMask)) {
        mods |= Qt::ShiftModifier;
    }
    if (controlMask && (mask & controlMask)) {
        mods |= Qt::ControlModifier;
    }
    if (altMask && (mask & altMask)) {
        mods |= Qt::AltModifier;
    }
    if (superMask && (mask & superMask)) {
        mods |= Qt::MetaModifier;
    }
    return mods;
}

namespace X11Keyboard
{
Display *display()
{
    auto *x11App = qGuiApp ? qGuiApp->nativeInterface<QNativeInterface::QX11Application>() : nullptr;
    return x11App ? x11App->display() : nullptr;
}

X11ModifierMap queryModifierMap(Display *dpy)
{
    X11ModifierMap map;
    if (!dpy) {
        return map;
    }

    map.shiftMask = 0;
    map.controlMask = 0;
    map.altMask = 0;
    map.superMask = 0;
    map.capsLockMask = 0;
    map.numLockMask = 0;
    map.scrollLockMask = 0;
    map.level3Mask = 0;

    XModifierKeymap *modmap = XGetModifierMapping(dpy);
    if (!modmap) {
        map.shiftMask = ShiftMask;
        map.controlMask = ControlMask;
        map.altMask = Mod1Mask;
        map.superMask = Mod4Mask;
        map.capsLockMask = LockMask;
        map.numLockMask = Mod2Mask;
        return map;
    }

    unsigned int metaFallbackMask = 0;
    QList<KeyCode> metaFallbackCodes;

    for (int modIdx = 0; modIdx < 8; ++modIdx) {
        const unsigned int modBit = (1u << modIdx);
        for (int k = 0; k < modmap->max_keypermod; ++k) {
            const KeyCode code = modmap->modifiermap[modIdx * modmap->max_keypermod + k];
            if (code == 0) {
                continue;
            }
            // Inspect keysyms across levels 0 and 1 for this modifier keycode
            for (int level = 0; level < 2; ++level) {
                const KeySym sym = XkbKeycodeToKeysym(dpy, code, 0, level);
                if (sym == NoSymbol) {
                    continue;
                }
                switch (sym) {
                case XK_Shift_L:
                case XK_Shift_R:
                    map.shiftMask |= modBit;
                    appendUnique(map.shiftKeycodes, code);
                    break;
                case XK_Control_L:
                case XK_Control_R:
                    map.controlMask |= modBit;
                    appendUnique(map.controlKeycodes, code);
                    break;
                case XK_Alt_L:
                case XK_Alt_R:
                    map.altMask |= modBit;
                    appendUnique(map.altKeycodes, code);
                    break;
                case XK_Super_L:
                case XK_Super_R:
                case XK_Hyper_L:
                case XK_Hyper_R:
                    map.superMask |= modBit;
                    appendUnique(map.superKeycodes, code);
                    break;
                case XK_Meta_L:
                case XK_Meta_R:
                    metaFallbackMask |= modBit;
                    appendUnique(metaFallbackCodes, code);
                    break;
                case XK_Caps_Lock:
                case XK_Shift_Lock:
                    map.capsLockMask |= modBit;
                    break;
                case XK_Num_Lock:
                    map.numLockMask |= modBit;
                    break;
                case XK_Scroll_Lock:
                    map.scrollLockMask |= modBit;
                    break;
                case XK_ISO_Level3_Shift:
                case XK_Mode_switch:
                    map.level3Mask |= modBit;
                    break;
                default:
                    break;
                }
            }
        }
    }

    XFreeModifiermap(modmap);

    // Fallback defaults only if a specific modifier wasn't advertised by the X server
    if (map.shiftMask == 0) {
        map.shiftMask = ShiftMask;
    }
    if (map.controlMask == 0) {
        map.controlMask = ControlMask;
    }
    if (map.altMask == 0) {
        map.altMask = metaFallbackMask ? metaFallbackMask : Mod1Mask;
        if (map.altKeycodes.isEmpty()) {
            map.altKeycodes = metaFallbackCodes;
        }
    }
    if (map.superMask == 0) {
        map.superMask = (metaFallbackMask && metaFallbackMask != map.altMask) ? metaFallbackMask : Mod4Mask;
        if (map.superKeycodes.isEmpty() && metaFallbackMask != map.altMask) {
            map.superKeycodes = metaFallbackCodes;
        }
    }
    if (map.capsLockMask == 0) {
        map.capsLockMask = LockMask;
    }

    // Ensure standard left/right keycodes are always included for live XQueryKeymap polling
    auto addSymCodes = [&](QList<KeyCode> &dest, std::initializer_list<KeySym> syms) {
        for (KeySym sym : syms) {
            for (KeyCode c : keycodesForKeysym(dpy, sym)) {
                appendUnique(dest, c);
            }
        }
    };
    addSymCodes(map.shiftKeycodes, {XK_Shift_L, XK_Shift_R});
    addSymCodes(map.controlKeycodes, {XK_Control_L, XK_Control_R});
    addSymCodes(map.altKeycodes, {XK_Alt_L, XK_Alt_R});
    addSymCodes(map.superKeycodes, {XK_Super_L, XK_Super_R, XK_Meta_L, XK_Meta_R, XK_Hyper_L, XK_Hyper_R});

    return map;
}

Qt::KeyboardModifiers queryLiveModifiers(Display *dpy, const X11ModifierMap &map)
{
    if (!dpy) {
        return Qt::NoModifier;
    }
    char keys[32] = {};
    XQueryKeymap(dpy, keys);

    Qt::KeyboardModifiers mods = Qt::NoModifier;
    if (anyKeycodeHeld(keys, map.shiftKeycodes)) {
        mods |= Qt::ShiftModifier;
    }
    if (anyKeycodeHeld(keys, map.controlKeycodes)) {
        mods |= Qt::ControlModifier;
    }
    if (anyKeycodeHeld(keys, map.altKeycodes)) {
        mods |= Qt::AltModifier;
    }
    if (anyKeycodeHeld(keys, map.superKeycodes)) {
        mods |= Qt::MetaModifier;
    }
    return mods;
}

bool anyBlockingModifierHeld(Display *dpy)
{
    if (!dpy) {
        return false;
    }
    const X11ModifierMap map = queryModifierMap(dpy);
    return queryLiveModifiers(dpy, map) != Qt::NoModifier;
}

QList<KeyCode> keycodesForKeysym(Display *dpy, KeySym sym)
{
    QList<KeyCode> result;
    if (!dpy || sym == NoSymbol) {
        return result;
    }

    int minKeycode = 0;
    int maxKeycode = 0;
    XDisplayKeycodes(dpy, &minKeycode, &maxKeycode);
    if (minKeycode <= 0 || maxKeycode < minKeycode) {
        if (const KeyCode single = XKeysymToKeycode(dpy, sym); single != 0) {
            result.append(single);
        }
        return result;
    }

    int symsPerKeycode = 0;
    KeySym *mapping = XGetKeyboardMapping(dpy, static_cast<KeyCode>(minKeycode),
                                          maxKeycode - minKeycode + 1, &symsPerKeycode);
    if (!mapping || symsPerKeycode <= 0) {
        if (mapping) {
            XFree(mapping);
        }
        if (const KeyCode single = XKeysymToKeycode(dpy, sym); single != 0) {
            result.append(single);
        }
        return result;
    }

    const int numCodes = maxKeycode - minKeycode + 1;
    for (int i = 0; i < numCodes; ++i) {
        // Check unshifted (level 0) across primary/secondary groups
        const int maxCol = qMin(symsPerKeycode, 4);
        for (int col = 0; col < maxCol; ++col) {
            if (mapping[i * symsPerKeycode + col] == sym) {
                appendUnique(result, static_cast<KeyCode>(minKeycode + i));
                break;
            }
        }
    }
    XFree(mapping);

    if (result.isEmpty()) {
        if (const KeyCode single = XKeysymToKeycode(dpy, sym); single != 0) {
            result.append(single);
        }
    }
    return result;
}

KeySym baseKeysymForKeycode(Display *dpy, unsigned int scanCode)
{
    if (!dpy || scanCode == 0 || scanCode > 255) {
        return NoSymbol;
    }
    // Always inspect group 0, level 0 first so Shift or non-Latin groups still yield the base physical key
    KeySym sym = XkbKeycodeToKeysym(dpy, static_cast<KeyCode>(scanCode), 0, 0);
    if (sym == NoSymbol) {
        int symsPer = 0;
        if (KeySym *m = XGetKeyboardMapping(dpy, static_cast<KeyCode>(scanCode), 1, &symsPer)) {
            if (symsPer > 0) {
                sym = m[0];
            }
            XFree(m);
        }
    }
    return sym;
}

KeySym qtKeyToKeysym(Qt::Key key)
{
    if (key >= Qt::Key_A && key <= Qt::Key_Z) {
        return static_cast<KeySym>(XK_a + (key - Qt::Key_A));
    }
    if (key >= Qt::Key_0 && key <= Qt::Key_9) {
        return static_cast<KeySym>(XK_0 + (key - Qt::Key_0));
    }
    if (key >= Qt::Key_F1 && key <= Qt::Key_F35) {
        return static_cast<KeySym>(XK_F1 + (key - Qt::Key_F1));
    }

    switch (key) {
    case Qt::Key_Space: return XK_space;
    case Qt::Key_Tab: return XK_Tab;
    case Qt::Key_Backtab: return XK_Tab;
    case Qt::Key_Return: return XK_Return;
    case Qt::Key_Enter: return XK_KP_Enter;
    case Qt::Key_Escape: return XK_Escape;
    case Qt::Key_Backspace: return XK_BackSpace;
    case Qt::Key_Delete: return XK_Delete;
    case Qt::Key_Insert: return XK_Insert;
    case Qt::Key_Pause: return XK_Pause;
    case Qt::Key_Print: return XK_Print;
    case Qt::Key_Home: return XK_Home;
    case Qt::Key_End: return XK_End;
    case Qt::Key_PageUp: return XK_Page_Up;
    case Qt::Key_PageDown: return XK_Page_Down;
    case Qt::Key_Left: return XK_Left;
    case Qt::Key_Right: return XK_Right;
    case Qt::Key_Up: return XK_Up;
    case Qt::Key_Down: return XK_Down;
    case Qt::Key_Minus: return XK_minus;
    case Qt::Key_Equal: return XK_equal;
    case Qt::Key_Plus: return XK_plus;
    case Qt::Key_BracketLeft: return XK_bracketleft;
    case Qt::Key_BracketRight: return XK_bracketright;
    case Qt::Key_Semicolon: return XK_semicolon;
    case Qt::Key_Apostrophe: return XK_apostrophe;
    case Qt::Key_Comma: return XK_comma;
    case Qt::Key_Period: return XK_period;
    case Qt::Key_Slash: return XK_slash;
    case Qt::Key_Backslash: return XK_backslash;
    case Qt::Key_QuoteLeft: return XK_grave;
    case Qt::Key_Menu: return XK_Menu;
    default: break;
    }

    // Dynamic fallback via XStringToKeysym for any additional named key
    const QString name = QKeySequence(key).toString(QKeySequence::PortableText);
    if (!name.isEmpty()) {
        const KeySym sym = XStringToKeysym(name.toLatin1().constData());
        if (sym != NoSymbol) {
            return sym;
        }
    }
    return NoSymbol;
}

Qt::Key keysymToQtKey(KeySym sym)
{
    if (sym == NoSymbol) {
        return Qt::Key_unknown;
    }
    if (sym >= XK_a && sym <= XK_z) {
        return static_cast<Qt::Key>(Qt::Key_A + (sym - XK_a));
    }
    if (sym >= XK_A && sym <= XK_Z) {
        return static_cast<Qt::Key>(Qt::Key_A + (sym - XK_A));
    }
    if (sym >= XK_0 && sym <= XK_9) {
        return static_cast<Qt::Key>(Qt::Key_0 + (sym - XK_0));
    }
    if (sym >= XK_KP_0 && sym <= XK_KP_9) {
        return static_cast<Qt::Key>(Qt::Key_0 + (sym - XK_KP_0));
    }
    if (sym >= XK_F1 && sym <= XK_F35) {
        return static_cast<Qt::Key>(Qt::Key_F1 + (sym - XK_F1));
    }

    switch (sym) {
    case XK_space:
    case XK_KP_Space: return Qt::Key_Space;
    case XK_Tab:
    case XK_ISO_Left_Tab:
    case XK_KP_Tab: return Qt::Key_Tab;
    case XK_Return: return Qt::Key_Return;
    case XK_KP_Enter: return Qt::Key_Enter;
    case XK_Escape: return Qt::Key_Escape;
    case XK_BackSpace: return Qt::Key_Backspace;
    case XK_Delete:
    case XK_KP_Delete: return Qt::Key_Delete;
    case XK_Insert:
    case XK_KP_Insert: return Qt::Key_Insert;
    case XK_Pause: return Qt::Key_Pause;
    case XK_Print: return Qt::Key_Print;
    case XK_Home:
    case XK_KP_Home: return Qt::Key_Home;
    case XK_End:
    case XK_KP_End: return Qt::Key_End;
    case XK_Page_Up:
    case XK_KP_Page_Up: return Qt::Key_PageUp;
    case XK_Page_Down:
    case XK_KP_Page_Down: return Qt::Key_PageDown;
    case XK_Left:
    case XK_KP_Left: return Qt::Key_Left;
    case XK_Right:
    case XK_KP_Right: return Qt::Key_Right;
    case XK_Up:
    case XK_KP_Up: return Qt::Key_Up;
    case XK_Down:
    case XK_KP_Down: return Qt::Key_Down;
    case XK_minus:
    case XK_KP_Subtract: return Qt::Key_Minus;
    case XK_equal:
    case XK_KP_Equal: return Qt::Key_Equal;
    case XK_plus:
    case XK_KP_Add: return Qt::Key_Plus;
    case XK_bracketleft: return Qt::Key_BracketLeft;
    case XK_bracketright: return Qt::Key_BracketRight;
    case XK_semicolon: return Qt::Key_Semicolon;
    case XK_apostrophe: return Qt::Key_Apostrophe;
    case XK_comma: return Qt::Key_Comma;
    case XK_period:
    case XK_KP_Decimal: return Qt::Key_Period;
    case XK_slash:
    case XK_KP_Divide: return Qt::Key_Slash;
    case XK_backslash: return Qt::Key_Backslash;
    case XK_grave: return Qt::Key_QuoteLeft;
    case XK_Menu: return Qt::Key_Menu;
    default: break;
    }

    return Qt::Key_unknown;
}

bool isModifierQtKey(int key)
{
    switch (key) {
    case Qt::Key_Shift:
    case Qt::Key_Control:
    case Qt::Key_Meta:
    case Qt::Key_Alt:
    case Qt::Key_AltGr:
    case Qt::Key_CapsLock:
    case Qt::Key_NumLock:
    case Qt::Key_ScrollLock:
    case Qt::Key_Super_L:
    case Qt::Key_Super_R:
    case Qt::Key_Hyper_L:
    case Qt::Key_Hyper_R:
    case Qt::Key_Mode_switch:
    case Qt::Key_Multi_key:
        return true;
    default:
        return false;
    }
}

bool isModifierKeysym(KeySym sym)
{
    switch (sym) {
    case XK_Shift_L:
    case XK_Shift_R:
    case XK_Control_L:
    case XK_Control_R:
    case XK_Alt_L:
    case XK_Alt_R:
    case XK_Meta_L:
    case XK_Meta_R:
    case XK_Super_L:
    case XK_Super_R:
    case XK_Hyper_L:
    case XK_Hyper_R:
    case XK_Caps_Lock:
    case XK_Shift_Lock:
    case XK_Num_Lock:
    case XK_Scroll_Lock:
    case XK_ISO_Level3_Shift:
    case XK_ISO_Level5_Shift:
    case XK_Mode_switch:
        return true;
    default:
        return false;
    }
}

QString qtCombinationToCinnamonAccel(const QKeyCombination &combo)
{
    const Qt::Key key = combo.key();
    if (key == Qt::Key_unknown || isModifierQtKey(key)) {
        return {};
    }

    const Qt::KeyboardModifiers mods = combo.keyboardModifiers();
    QString out;
    if (mods & Qt::ControlModifier) {
        out += QStringLiteral("<Primary>");
    }
    if (mods & Qt::AltModifier) {
        out += QStringLiteral("<Alt>");
    }
    if (mods & Qt::ShiftModifier) {
        out += QStringLiteral("<Shift>");
    }
    if (mods & Qt::MetaModifier) {
        out += QStringLiteral("<Super>");
    }

    const KeySym sym = qtKeyToKeysym(key);
    if (sym == NoSymbol) {
        return {};
    }
    const char *symName = XKeysymToString(sym);
    if (!symName || !*symName) {
        return {};
    }
    out += QString::fromLatin1(symName);
    return out;
}
} // namespace X11Keyboard
