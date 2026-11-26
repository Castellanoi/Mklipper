/*
    SPDX-FileCopyrightText: 2026 Mklipper contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "globalshortcut.h"

#include <QDebug>
#include <QGuiApplication>
#include <QProcess>
#include <QRegularExpression>

#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <xcb/xcb.h>

namespace
{
bool s_x11GrabError = false;

int x11ErrorHandler(Display *, XErrorEvent *ev)
{
    if (ev && (ev->error_code == BadAccess || ev->error_code == BadValue)) {
        s_x11GrabError = true;
    }
    return 0;
}

Display *x11Display()
{
    auto *x11App = qGuiApp ? qGuiApp->nativeInterface<QNativeInterface::QX11Application>() : nullptr;
    return x11App ? x11App->display() : nullptr;
}

unsigned int qtModsToX11(Qt::KeyboardModifiers mods)
{
    unsigned int mask = 0;
    if (mods & Qt::ShiftModifier) {
        mask |= ShiftMask;
    }
    if (mods & Qt::ControlModifier) {
        mask |= ControlMask;
    }
    if (mods & Qt::AltModifier) {
        mask |= Mod1Mask;
    }
    if (mods & Qt::MetaModifier) {
        mask |= Mod4Mask;
    }
    return mask;
}

KeySym qtKeyToX11KeySym(Qt::Key key)
{
    if (key >= Qt::Key_A && key <= Qt::Key_Z) {
        return static_cast<KeySym>(XK_a + (key - Qt::Key_A));
    }
    if (key >= Qt::Key_0 && key <= Qt::Key_9) {
        return static_cast<KeySym>(XK_0 + (key - Qt::Key_0));
    }
    if (key >= Qt::Key_F1 && key <= Qt::Key_F12) {
        return static_cast<KeySym>(XK_F1 + (key - Qt::Key_F1));
    }
    switch (key) {
    case Qt::Key_Space: return XK_space;
    case Qt::Key_Tab: return XK_Tab;
    case Qt::Key_Return: return XK_Return;
    case Qt::Key_Escape: return XK_Escape;
    case Qt::Key_Backspace: return XK_BackSpace;
    case Qt::Key_Delete: return XK_Delete;
    case Qt::Key_Insert: return XK_Insert;
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
    case Qt::Key_BracketLeft: return XK_bracketleft;
    case Qt::Key_BracketRight: return XK_bracketright;
    case Qt::Key_Semicolon: return XK_semicolon;
    case Qt::Key_Apostrophe: return XK_apostrophe;
    case Qt::Key_Comma: return XK_comma;
    case Qt::Key_Period: return XK_period;
    case Qt::Key_Slash: return XK_slash;
    case Qt::Key_Backslash: return XK_backslash;
    case Qt::Key_QuoteLeft: return XK_grave;
    default: break;
    }

    const QString name = QKeySequence(key).toString();
    if (!name.isEmpty()) {
        return XStringToKeysym(name.toLatin1().constData());
    }
    return NoSymbol;
}

QString qtKeyToCinnamonAccel(const QKeySequence &sequence)
{
    if (sequence.isEmpty()) {
        return {};
    }
    const QKeyCombination combo = sequence[0];
    const Qt::KeyboardModifiers mods = combo.keyboardModifiers();
    const Qt::Key key = combo.key();

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

    if (key >= Qt::Key_A && key <= Qt::Key_Z) {
        out += QChar('a' + (key - Qt::Key_A));
        return out;
    }
    if (key >= Qt::Key_0 && key <= Qt::Key_9) {
        out += QChar('0' + (key - Qt::Key_0));
        return out;
    }
    if (key >= Qt::Key_F1 && key <= Qt::Key_F12) {
        out += QStringLiteral("F%1").arg(1 + (key - Qt::Key_F1));
        return out;
    }

    const KeySym sym = qtKeyToX11KeySym(key);
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

QString runGSettings(const QStringList &args, bool *ok = nullptr)
{
    QProcess proc;
    proc.start(QStringLiteral("gsettings"), args);
    const bool finished = proc.waitForFinished(2000) && proc.exitStatus() == QProcess::NormalExit && proc.exitCode() == 0;
    if (ok) {
        *ok = finished;
    }
    return QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
}

QStringList parseGSettingsStringList(const QString &raw)
{
    QStringList items;
    static const QRegularExpression re(QStringLiteral("'([^']*)'"));
    auto it = re.globalMatch(raw);
    while (it.hasNext()) {
        items.append(it.next().captured(1));
    }
    return items;
}

QString formatGSettingsStringList(const QStringList &items)
{
    if (items.isEmpty()) {
        return QStringLiteral("@as []");
    }
    QStringList quoted;
    quoted.reserve(items.size());
    for (const QString &item : items) {
        QString escaped = item;
        escaped.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
        escaped.replace(QLatin1Char('\''), QStringLiteral("\\'"));
        quoted.append(QLatin1Char('\'') + escaped + QLatin1Char('\''));
    }
    return QLatin1Char('[') + quoted.join(QStringLiteral(", ")) + QLatin1Char(']');
}

constexpr unsigned int kIgnoredMasks[] = {
    0,
    LockMask,
    Mod2Mask,
    LockMask | Mod2Mask,
};
} // namespace

GlobalShortcutManager::GlobalShortcutManager(QObject *parent)
    : QObject(parent)
{
    if (qGuiApp) {
        qGuiApp->installNativeEventFilter(this);
    }
}

GlobalShortcutManager::~GlobalShortcutManager()
{
    unregisterAll();
    if (qGuiApp) {
        qGuiApp->removeNativeEventFilter(this);
    }
}

bool GlobalShortcutManager::isCinnamonDesktop()
{
    const QString desktop = qEnvironmentVariable("XDG_CURRENT_DESKTOP");
    return desktop.contains(QStringLiteral("Cinnamon"), Qt::CaseInsensitive);
}

bool GlobalShortcutManager::syncCinnamonShortcuts(const QList<CinnamonShortcutSpec> &specs)
{
    bool ok = false;
    const QString currentRaw = runGSettings({QStringLiteral("get"),
                                             QStringLiteral("org.cinnamon.desktop.keybindings"),
                                             QStringLiteral("custom-list")},
                                            &ok);
    if (!ok) {
        return false;
    }

    QStringList customList = parseGSettingsStringList(currentRaw);

    // Keep all non-mklipper user custom shortcuts untouched, and track old mklipper-* IDs for cleanup
    QStringList updatedList;
    QStringList previousMklipperIds;
    for (const QString &id : std::as_const(customList)) {
        if (!id.startsWith(QStringLiteral("mklipper-"))) {
            updatedList.append(id);
        } else {
            previousMklipperIds.append(id);
        }
    }

    QStringList activeMklipperIds;
    for (const auto &spec : specs) {
        const QString schemaPath = QStringLiteral("org.cinnamon.desktop.keybindings.custom-keybinding:/org/cinnamon/desktop/keybindings/custom-keybindings/%1/").arg(spec.id);
        const QString accel = qtKeyToCinnamonAccel(spec.key);
        if (accel.isEmpty()) {
            runGSettings({QStringLiteral("reset-recursively"), schemaPath});
            continue;
        }
        updatedList.append(spec.id);
        activeMklipperIds.append(spec.id);
        runGSettings({QStringLiteral("set"), schemaPath, QStringLiteral("name"), spec.label});
        runGSettings({QStringLiteral("set"), schemaPath, QStringLiteral("command"), spec.command});
        runGSettings({QStringLiteral("set"), schemaPath, QStringLiteral("binding"), formatGSettingsStringList({accel})});
    }

    // Reset any obsolete mklipper-* keybindings (e.g. removed legacy actions)
    for (const QString &oldId : std::as_const(previousMklipperIds)) {
        if (!activeMklipperIds.contains(oldId)) {
            const QString schemaPath = QStringLiteral("org.cinnamon.desktop.keybindings.custom-keybinding:/org/cinnamon/desktop/keybindings/custom-keybindings/%1/").arg(oldId);
            runGSettings({QStringLiteral("reset-recursively"), schemaPath});
        }
    }

    runGSettings({QStringLiteral("set"),
                  QStringLiteral("org.cinnamon.desktop.keybindings"),
                  QStringLiteral("custom-list"),
                  formatGSettingsStringList(updatedList)},
                 &ok);
    return ok;
}

bool GlobalShortcutManager::registerShortcut(const QString &id, const QKeySequence &sequence, std::function<void()> callback)
{
    unregisterShortcut(id);

    if (sequence.isEmpty() || !callback) {
        return false;
    }

    Display *dpy = x11Display();
    if (!dpy) {
        return false;
    }

    const QKeyCombination combo = sequence[0];
    const Qt::Key key = combo.key();
    const Qt::KeyboardModifiers mods = combo.keyboardModifiers();
    const KeySym sym = qtKeyToX11KeySym(key);
    if (sym == NoSymbol) {
        qWarning() << "Mklipper: Unsupported shortcut key:" << sequence.toString();
        return false;
    }

    const KeyCode code = XKeysymToKeycode(dpy, sym);
    if (code == 0) {
        qWarning() << "Mklipper: Cannot map keysym for shortcut:" << sequence.toString();
        return false;
    }

    const unsigned int xMods = qtModsToX11(mods);
    const Window root = DefaultRootWindow(dpy);

    s_x11GrabError = false;
    auto prevHandler = XSetErrorHandler(x11ErrorHandler);

    for (unsigned int ignored : kIgnoredMasks) {
        XGrabKey(dpy, code, xMods | ignored, root, True, GrabModeAsync, GrabModeAsync);
    }
    XSync(dpy, False);
    XSetErrorHandler(prevHandler);

    if (s_x11GrabError) {
        qWarning() << "Mklipper: Could not grab global shortcut (already in use?):" << sequence.toString();
        for (unsigned int ignored : kIgnoredMasks) {
            XUngrabKey(dpy, code, xMods | ignored, root);
        }
        XSync(dpy, False);
        return false;
    }

    m_shortcuts.append({id, static_cast<unsigned int>(code), xMods, std::move(callback)});
    return true;
}

void GlobalShortcutManager::ungrab(const GrabbedKey &entry)
{
    Display *dpy = x11Display();
    if (!dpy) {
        return;
    }
    const Window root = DefaultRootWindow(dpy);
    for (unsigned int ignored : kIgnoredMasks) {
        XUngrabKey(dpy, static_cast<int>(entry.keycode), entry.modifiers | ignored, root);
    }
    XFlush(dpy);
}

void GlobalShortcutManager::unregisterShortcut(const QString &id)
{
    for (int i = m_shortcuts.size() - 1; i >= 0; --i) {
        if (m_shortcuts[i].id == id) {
            ungrab(m_shortcuts[i]);
            m_shortcuts.removeAt(i);
        }
    }
}

void GlobalShortcutManager::unregisterAll()
{
    for (const auto &entry : std::as_const(m_shortcuts)) {
        ungrab(entry);
    }
    m_shortcuts.clear();
}

bool GlobalShortcutManager::nativeEventFilter(const QByteArray &eventType, void *message, qintptr * /*result*/)
{
    if (eventType != "xcb_generic_event_t" || !message) {
        return false;
    }

    auto *ev = static_cast<xcb_generic_event_t *>(message);
    if ((ev->response_type & ~0x80) != XCB_KEY_PRESS) {
        return false;
    }

    auto *keyEv = reinterpret_cast<xcb_key_press_event_t *>(ev);
    const unsigned int cleanMods = keyEv->state & (ShiftMask | ControlMask | Mod1Mask | Mod4Mask);

    for (const auto &entry : std::as_const(m_shortcuts)) {
        if (entry.keycode == keyEv->detail && entry.modifiers == cleanMods) {
            auto cb = entry.callback;
            QMetaObject::invokeMethod(this, [cb = std::move(cb)]() { cb(); }, Qt::QueuedConnection);
            return true;
        }
    }
    return false;
}
