/*
    SPDX-FileCopyrightText: 2026 Mklipper contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "globalshortcut.h"

#include <QDebug>
#include <QGuiApplication>
#include <QProcess>

#include <X11/Xlib.h>
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
    const qsizetype len = raw.size();
    qsizetype i = 0;
    while (i < len) {
        const QChar quote = raw.at(i);
        if (quote != QLatin1Char('\'') && quote != QLatin1Char('"')) {
            ++i;
            continue;
        }
        ++i;
        QString current;
        while (i < len) {
            const QChar ch = raw.at(i);
            if (ch == QLatin1Char('\\') && i + 1 < len) {
                current.append(raw.at(i + 1));
                i += 2;
                continue;
            }
            if (ch == quote) {
                ++i;
                break;
            }
            current.append(ch);
            ++i;
        }
        items.append(current);
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
} // namespace

GlobalShortcutManager::GlobalShortcutManager(QObject *parent)
    : QObject(parent)
{
    if (Display *dpy = X11Keyboard::display()) {
        m_modMap = X11Keyboard::queryModifierMap(dpy);
    }
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
        const QString accel = spec.key.isEmpty() ? QString() : X11Keyboard::qtCombinationToCinnamonAccel(spec.key[0]);
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

    // Reset any obsolete mklipper-* keybindings
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
    return registerShortcutInternal(id, sequence, std::move(callback), true);
}

bool GlobalShortcutManager::registerShortcutInternal(const QString &id,
                                                     const QKeySequence &sequence,
                                                     std::function<void()> callback,
                                                     bool refreshModMap)
{
    unregisterShortcut(id);

    if (sequence.isEmpty() || !callback) {
        return false;
    }

    Display *dpy = X11Keyboard::display();
    if (!dpy) {
        return false;
    }

    if (refreshModMap) {
        // Re-query the live X11 modifier map dynamically (no hardcoded Mod1/Mod2/Mod4/ScrollLock masks)
        m_modMap = X11Keyboard::queryModifierMap(dpy);
    }

    const QKeyCombination combo = sequence[0];
    const Qt::Key key = combo.key();
    const Qt::KeyboardModifiers mods = combo.keyboardModifiers();
    const KeySym sym = X11Keyboard::qtKeyToKeysym(key);
    if (sym == NoSymbol) {
        qWarning() << "Mklipper: Unsupported shortcut key:" << sequence.toString();
        return false;
    }

    const QList<KeyCode> codes = X11Keyboard::keycodesForKeysym(dpy, sym);
    if (codes.isEmpty()) {
        qWarning() << "Mklipper: Cannot map keysym for shortcut:" << sequence.toString();
        return false;
    }

    const unsigned int xMods = m_modMap.qtModsToX11(mods);
    const QList<unsigned int> ignoredMasks = m_modMap.ignoredLockCombinations();
    const Window root = DefaultRootWindow(dpy);

    s_x11GrabError = false;
    auto prevHandler = XSetErrorHandler(x11ErrorHandler);

    QList<unsigned int> grabbedCodes;
    for (KeyCode code : codes) {
        for (unsigned int ignored : ignoredMasks) {
            XGrabKey(dpy, code, xMods | ignored, root, True, GrabModeAsync, GrabModeAsync);
        }
        grabbedCodes.append(static_cast<unsigned int>(code));
    }
    XSync(dpy, False);
    XSetErrorHandler(prevHandler);

    if (s_x11GrabError) {
        qWarning() << "Mklipper: Could not grab global shortcut (already in use?):" << sequence.toString();
        for (unsigned int code : grabbedCodes) {
            for (unsigned int ignored : ignoredMasks) {
                XUngrabKey(dpy, static_cast<int>(code), xMods | ignored, root);
            }
        }
        XSync(dpy, False);
        return false;
    }

    m_shortcuts.append({id, sequence, grabbedCodes, xMods, ignoredMasks, std::move(callback)});
    return true;
}

void GlobalShortcutManager::ungrab(const GrabbedKey &entry)
{
    Display *dpy = X11Keyboard::display();
    if (!dpy) {
        return;
    }
    const Window root = DefaultRootWindow(dpy);
    for (unsigned int code : entry.keycodes) {
        for (unsigned int ignored : entry.ignoredMasks) {
            XUngrabKey(dpy, static_cast<int>(code), entry.modifiers | ignored, root);
        }
    }
    XFlush(dpy);
}

void GlobalShortcutManager::refreshAllGrabs()
{
    if (m_shortcuts.isEmpty()) {
        return;
    }
    QList<GrabbedKey> existing = m_shortcuts;
    unregisterAll();
    for (auto &entry : existing) {
        registerShortcutInternal(entry.id, entry.sequence, std::move(entry.callback), false);
    }
}

void GlobalShortcutManager::unregisterShortcut(const QString &id)
{
    for (int i = m_shortcuts.size() - 1; i >= 0; --i) {
        if (m_shortcuts.at(i).id == id) {
            ungrab(m_shortcuts.at(i));
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
    const uint8_t type = ev->response_type & ~0x80;

    if (type == XCB_MAPPING_NOTIFY) {
        // Keyboard plugged in / layout changed / ckb-next updated keymap: refresh dynamic grabs
        if (Display *dpy = X11Keyboard::display()) {
            m_modMap = X11Keyboard::queryModifierMap(dpy);
        }
        refreshAllGrabs();
        return false;
    }

    if (type != XCB_KEY_PRESS) {
        return false;
    }

    auto *keyEv = reinterpret_cast<xcb_key_press_event_t *>(ev);
    const unsigned int cleanMods = keyEv->state & m_modMap.relevantMask();

    for (const auto &entry : std::as_const(m_shortcuts)) {
        if (entry.modifiers == cleanMods && entry.keycodes.contains(keyEv->detail)) {
            if (entry.callback) {
                entry.callback();
            }
            return true;
        }
    }
    return false;
}
