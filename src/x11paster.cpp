/*
    SPDX-FileCopyrightText: 2026 Mklipper contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "x11paster.h"

#include "x11keyboard.h"

#include <QGuiApplication>
#include <QStringList>
#include <QTimer>

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XTest.h>
#include <X11/keysym.h>

namespace
{
int ignoreX11Error(Display *, XErrorEvent *)
{
    return 0;
}

class ScopedX11ErrorGuard
{
public:
    explicit ScopedX11ErrorGuard(Display *dpy)
        : m_dpy(dpy)
        , m_prev(XSetErrorHandler(ignoreX11Error))
    {
    }
    ~ScopedX11ErrorGuard()
    {
        if (m_dpy) {
            XSync(m_dpy, False);
        }
        XSetErrorHandler(m_prev);
    }

private:
    Display *m_dpy = nullptr;
    int (*m_prev)(Display *, XErrorEvent *) = nullptr;
};

Display *x11Display()
{
    auto *x11App = qGuiApp ? qGuiApp->nativeInterface<QNativeInterface::QX11Application>() : nullptr;
    return x11App ? x11App->display() : nullptr;
}

bool isTerminalClass(const QString &cls)
{
    static const QStringList kTerminals = {
        QStringLiteral("gnome-terminal"),
        QStringLiteral("konsole"),
        QStringLiteral("xfce4-terminal"),
        QStringLiteral("mate-terminal"),
        QStringLiteral("tilix"),
        QStringLiteral("terminator"),
        QStringLiteral("alacritty"),
        QStringLiteral("kitty"),
        QStringLiteral("xterm"),
        QStringLiteral("uxterm"),
        QStringLiteral("rxvt"),
        QStringLiteral("urxvt"),
        QStringLiteral("st-256color"),
        QStringLiteral("wezterm"),
        QStringLiteral("foot"),
        QStringLiteral("guake"),
        QStringLiteral("tilda"),
        QStringLiteral("qterminal"),
        QStringLiteral("lxterminal"),
        QStringLiteral("ptyxis"),
        QStringLiteral("blackbox"),
        QStringLiteral("hyper"),
        QStringLiteral("tabby"),
        QStringLiteral("sakura"),
        QStringLiteral("terminology"),
    };
    const QString lower = cls.toLower();
    for (const QString &t : kTerminals) {
        if (lower.contains(t)) {
            return true;
        }
    }
    return false;
}

bool queryWindowIsMklipper(Display *dpy, Window win)
{
    if (!dpy || win == 0 || win == None) {
        return false;
    }
    XClassHint hint{};
    if (XGetClassHint(dpy, win, &hint) != 0) {
        bool isOwn = false;
        if (hint.res_name) {
            isOwn = isOwn || (QString::fromLocal8Bit(hint.res_name).compare(QStringLiteral("mklipper"), Qt::CaseInsensitive) == 0);
            XFree(hint.res_name);
        }
        if (hint.res_class) {
            isOwn = isOwn || (QString::fromLocal8Bit(hint.res_class).compare(QStringLiteral("mklipper"), Qt::CaseInsensitive) == 0);
            XFree(hint.res_class);
        }
        return isOwn;
    }
    return false;
}

bool queryWindowIsTerminal(Display *dpy, Window win)
{
    if (!dpy || win == 0 || win == None) {
        return false;
    }
    XClassHint hint{};
    if (XGetClassHint(dpy, win, &hint) != 0) {
        bool terminal = false;
        if (hint.res_name) {
            terminal = terminal || isTerminalClass(QString::fromLocal8Bit(hint.res_name));
            XFree(hint.res_name);
        }
        if (hint.res_class) {
            terminal = terminal || isTerminalClass(QString::fromLocal8Bit(hint.res_class));
            XFree(hint.res_class);
        }
        return terminal;
    }
    return false;
}

bool anyBlockingModifierHeld(Display *dpy)
{
    return X11Keyboard::anyBlockingModifierHeld(dpy);
}

void activateWindow(Display *dpy, Window win)
{
    if (!dpy || win == 0 || win == None) {
        return;
    }
    const Atom netActive = XInternAtom(dpy, "_NET_ACTIVE_WINDOW", False);
    if (netActive == None) {
        return;
    }
    XEvent ev{};
    ev.xclient.type = ClientMessage;
    ev.xclient.window = win;
    ev.xclient.message_type = netActive;
    ev.xclient.format = 32;
    ev.xclient.data.l[0] = 1; // source: application
    ev.xclient.data.l[1] = CurrentTime;
    ev.xclient.data.l[2] = 0;

    const Window root = DefaultRootWindow(dpy);
    XSendEvent(dpy, root, False, SubstructureRedirectMask | SubstructureNotifyMask, &ev);
    XFlush(dpy);
}
} // namespace

X11Paster::X11Paster(QObject *parent)
    : QObject(parent)
{
}

void X11Paster::captureTargetWindow(WId ownWindowId)
{
    Display *dpy = x11Display();
    if (!dpy) {
        return;
    }

    ScopedX11ErrorGuard x11Guard(dpy);
    Window active = 0;
    const Atom netActive = XInternAtom(dpy, "_NET_ACTIVE_WINDOW", True);
    if (netActive != None) {
        Atom actualType = None;
        int actualFormat = 0;
        unsigned long nitems = 0;
        unsigned long bytesAfter = 0;
        unsigned char *prop = nullptr;
        if (XGetWindowProperty(dpy, DefaultRootWindow(dpy), netActive, 0, 1, False, XA_WINDOW,
                               &actualType, &actualFormat, &nitems, &bytesAfter, &prop) == Success
            && prop) {
            if (actualFormat == 32 && nitems > 0) {
                active = *reinterpret_cast<Window *>(prop);
            }
            XFree(prop);
        }
    }

    if (active == 0 || active == None) {
        int revert = 0;
        XGetInputFocus(dpy, &active, &revert);
    }

    if (active != 0 && active != None && active != static_cast<Window>(ownWindowId) && !queryWindowIsMklipper(dpy, active)) {
        m_targetWindow = static_cast<unsigned long>(active);
        m_targetIsTerminal = queryWindowIsTerminal(dpy, active);
    }
}

void X11Paster::schedulePaste()
{
    const quint64 serial = ++m_pasteSerial;
    if (Display *dpy = x11Display()) {
        if (m_targetWindow != 0) {
            activateWindow(dpy, static_cast<Window>(m_targetWindow));
        }
    }
    // Wait briefly for window focus restoration and key release
    QTimer::singleShot(90, this, [this, serial]() { attemptPaste(12, serial); });
}

void X11Paster::activateOwnWindow(WId ownWindowId) const
{
    if (Display *dpy = x11Display()) {
        if (ownWindowId != 0) {
            activateWindow(dpy, static_cast<Window>(ownWindowId));
        }
    }
}

void X11Paster::attemptPaste(int retriesLeft, quint64 serial)
{
    if (serial != m_pasteSerial) {
        return;
    }

    Display *dpy = x11Display();
    if (!dpy) {
        Q_EMIT pasteFinished();
        return;
    }

    if (retriesLeft > 0 && anyBlockingModifierHeld(dpy)) {
        QTimer::singleShot(25, this, [this, retriesLeft, serial]() { attemptPaste(retriesLeft - 1, serial); });
        return;
    }

    sendPasteKeystroke();
    // Give the target application a brief moment to receive Ctrl+V and read CLIPBOARD before signaling completion
    QTimer::singleShot(80, this, [this, serial]() {
        if (serial == m_pasteSerial) {
            Q_EMIT pasteFinished();
        }
    });
}

void X11Paster::sendPasteKeystroke() const
{
    Display *dpy = X11Keyboard::display();
    if (!dpy) {
        return;
    }

    const X11ModifierMap modMap = X11Keyboard::queryModifierMap(dpy);
    const KeyCode ctrl = !modMap.controlKeycodes.isEmpty() ? modMap.controlKeycodes.first() : XKeysymToKeycode(dpy, XK_Control_L);
    const KeyCode shift = !modMap.shiftKeycodes.isEmpty() ? modMap.shiftKeycodes.first() : XKeysymToKeycode(dpy, XK_Shift_L);
    const QList<KeyCode> vCodes = X11Keyboard::keycodesForKeysym(dpy, XK_v);
    const KeyCode vKey = !vCodes.isEmpty() ? vCodes.first() : XKeysymToKeycode(dpy, XK_v);
    if (ctrl == 0 || vKey == 0) {
        // Fallback to Shift+Insert on exotic layouts where Latin 'v' is unmapped
        const KeyCode insKey = XKeysymToKeycode(dpy, XK_Insert);
        if (shift != 0 && insKey != 0) {
            XTestFakeKeyEvent(dpy, shift, True, CurrentTime);
            XTestFakeKeyEvent(dpy, insKey, True, CurrentTime);
            XTestFakeKeyEvent(dpy, insKey, False, CurrentTime);
            XTestFakeKeyEvent(dpy, shift, False, CurrentTime);
            XFlush(dpy);
        }
        return;
    }

    XTestFakeKeyEvent(dpy, ctrl, True, CurrentTime);
    if (m_targetIsTerminal && shift != 0) {
        XTestFakeKeyEvent(dpy, shift, True, CurrentTime);
    }
    XTestFakeKeyEvent(dpy, vKey, True, CurrentTime);
    XTestFakeKeyEvent(dpy, vKey, False, CurrentTime);
    if (m_targetIsTerminal && shift != 0) {
        XTestFakeKeyEvent(dpy, shift, False, CurrentTime);
    }
    XTestFakeKeyEvent(dpy, ctrl, False, CurrentTime);
    XFlush(dpy);
}
