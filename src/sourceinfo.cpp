/*
    SPDX-FileCopyrightText: 2026 Mklipper contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "sourceinfo.h"

#include <QByteArray>
#include <QFile>
#include <QGuiApplication>
#include <QMimeData>
#include <QRegularExpression>
#include <QStringConverter>
#include <QUrl>

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

using namespace Qt::StringLiterals;

namespace
{
Display *x11Display()
{
    auto *x11App = qGuiApp ? qGuiApp->nativeInterface<QNativeInterface::QX11Application>() : nullptr;
    return x11App ? x11App->display() : nullptr;
}

bool isHttpUrl(const QString &candidate)
{
    const QUrl u(candidate.trimmed());
    const QString scheme = u.scheme().toLower();
    return u.isValid() && (scheme == u"http"_s || scheme == u"https"_s) && !u.host().isEmpty();
}

QString decodeUtf16LeOrUtf8(const QByteArray &raw)
{
    if (raw.isEmpty()) {
        return {};
    }
    // Mozilla's text/x-moz-url is UTF-16LE
    if (raw.size() >= 4 && (raw.size() % 2 == 0) && raw.at(1) == '\0') {
        auto toUtf16 = QStringDecoder(QStringDecoder::Utf16LE);
        return QString(toUtf16(raw));
    }
    return QString::fromUtf8(raw);
}

QString readX11StringProp(Display *dpy, Window win, Atom propAtom)
{
    if (!dpy || win == 0 || win == None || propAtom == None) {
        return {};
    }
    Atom actualType = None;
    int actualFormat = 0;
    unsigned long nitems = 0;
    unsigned long bytesAfter = 0;
    unsigned char *prop = nullptr;
    if (XGetWindowProperty(dpy, win, propAtom, 0, 256, False, AnyPropertyType,
                           &actualType, &actualFormat, &nitems, &bytesAfter, &prop) == Success
        && prop) {
        QString result;
        if (actualFormat == 8 && nitems > 0) {
            result = QString::fromUtf8(reinterpret_cast<const char *>(prop), static_cast<qsizetype>(nitems)).trimmed();
            // Strip embedded null bytes if any
            const int nul = result.indexOf(QLatin1Char('\0'));
            if (nul >= 0) {
                result.truncate(nul);
            }
        }
        XFree(prop);
        return result;
    }
    return {};
}

unsigned long readX11Pid(Display *dpy, Window win)
{
    if (!dpy || win == 0 || win == None) {
        return 0;
    }
    const Atom pidAtom = XInternAtom(dpy, "_NET_WM_PID", True);
    if (pidAtom == None) {
        return 0;
    }
    Atom actualType = None;
    int actualFormat = 0;
    unsigned long nitems = 0;
    unsigned long bytesAfter = 0;
    unsigned char *prop = nullptr;
    unsigned long pid = 0;
    if (XGetWindowProperty(dpy, win, pidAtom, 0, 1, False, XA_CARDINAL,
                           &actualType, &actualFormat, &nitems, &bytesAfter, &prop) == Success
        && prop) {
        if (nitems > 0) {
            pid = *reinterpret_cast<unsigned long *>(prop);
        }
        XFree(prop);
    }
    return pid;
}

QString readProcComm(unsigned long pid)
{
    if (pid == 0) {
        return {};
    }
    QFile f(QStringLiteral("/proc/%1/comm").arg(pid));
    if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QString::fromUtf8(f.readAll()).trimmed();
    }
    return {};
}

struct X11WindowMeta {
    QString resName;
    QString resClass;
    QString title;
    unsigned long pid = 0;
    bool isMklipper = false;
};

X11WindowMeta inspectX11Window(Display *dpy, Window win)
{
    X11WindowMeta meta;
    if (!dpy || win == 0 || win == None) {
        return meta;
    }

    XClassHint hint;
    if (XGetClassHint(dpy, win, &hint) != 0) {
        if (hint.res_name) {
            meta.resName = QString::fromLocal8Bit(hint.res_name).trimmed();
            XFree(hint.res_name);
        }
        if (hint.res_class) {
            meta.resClass = QString::fromLocal8Bit(hint.res_class).trimmed();
            XFree(hint.res_class);
        }
    }

    if (meta.resName.compare(u"mklipper"_s, Qt::CaseInsensitive) == 0
        || meta.resClass.compare(u"mklipper"_s, Qt::CaseInsensitive) == 0) {
        meta.isMklipper = true;
        return meta;
    }

    const Atom netWmName = XInternAtom(dpy, "_NET_WM_NAME", True);
    meta.title = readX11StringProp(dpy, win, netWmName);
    if (meta.title.isEmpty()) {
        meta.title = readX11StringProp(dpy, win, XA_WM_NAME);
    }

    meta.pid = readX11Pid(dpy, win);
    if (meta.resClass.isEmpty() && meta.resName.isEmpty() && meta.pid > 0) {
        meta.resName = readProcComm(meta.pid);
        if (meta.resName.compare(u"mklipper"_s, Qt::CaseInsensitive) == 0) {
            meta.isMklipper = true;
        }
    }

    return meta;
}

Window queryActiveWindow(Display *dpy)
{
    if (!dpy) {
        return None;
    }
    const Atom netActive = XInternAtom(dpy, "_NET_ACTIVE_WINDOW", True);
    if (netActive != None) {
        Atom actualType = None;
        int actualFormat = 0;
        unsigned long nitems = 0;
        unsigned long bytesAfter = 0;
        unsigned char *prop = nullptr;
        Window active = None;
        if (XGetWindowProperty(dpy, DefaultRootWindow(dpy), netActive, 0, 1, False, XA_WINDOW,
                               &actualType, &actualFormat, &nitems, &bytesAfter, &prop) == Success
            && prop) {
            if (nitems > 0) {
                active = *reinterpret_cast<Window *>(prop);
            }
            XFree(prop);
        }
        if (active != 0 && active != None) {
            return active;
        }
    }
    Window focused = None;
    int revert = 0;
    XGetInputFocus(dpy, &focused, &revert);
    return focused;
}
} // namespace

QString extractSourceUrlFromMime(const QMimeData *mimeData)
{
    if (!mimeData) {
        return {};
    }

    // 1. Chromium / Chrome / Brave / Edge / Vivaldi / Electron exact page URL
    if (mimeData->hasFormat(u"chromium/x-source-url"_s)) {
        const QString u = QString::fromUtf8(mimeData->data(u"chromium/x-source-url"_s)).trimmed();
        if (isHttpUrl(u)) {
            return u;
        }
    }

    // 2. Firefox / Gecko URL metadata (UTF-16LE "<url>\n<title>")
    for (const QString &mozFmt : {u"text/x-moz-url"_s, u"text/x-moz-url-priv"_s}) {
        if (mimeData->hasFormat(mozFmt)) {
            const QString decoded = decodeUtf16LeOrUtf8(mimeData->data(mozFmt));
            const QString firstLine = decoded.section(QLatin1Char('\n'), 0, 0).trimmed();
            if (isHttpUrl(firstLine)) {
                return firstLine;
            }
        }
    }

    // 3. HTML SourceURL: header (CF_HTML / Electron / browsers)
    if (mimeData->hasHtml()) {
        static const QRegularExpression reSourceUrl(QStringLiteral(R"(SourceURL:\s*(https?://[^\s<>"']+))"),
                                                    QRegularExpression::CaseInsensitiveOption);
        const QString html = mimeData->html();
        const auto m = reSourceUrl.match(html.left(1024));
        if (m.hasMatch() && isHttpUrl(m.captured(1))) {
            return m.captured(1).trimmed();
        }
    }

    // 4. Direct HTTP/HTTPS URL list (not local file://)
    if (mimeData->hasUrls()) {
        for (const QUrl &u : mimeData->urls()) {
            const QString s = u.toString(QUrl::FullyEncoded);
            if (isHttpUrl(s)) {
                return s;
            }
        }
    }

    return {};
}

QString prettifyAppName(const QString &rawClass, const QString &rawName)
{
    const QString key = (!rawClass.isEmpty() ? rawClass : rawName).trimmed().toLower();
    if (key.isEmpty()) {
        return {};
    }

    static const QHash<QString, QString> kKnownApps = {
        {u"firefox"_s, u"Firefox"_s},
        {u"firefox-esr"_s, u"Firefox"_s},
        {u"navigator"_s, u"Firefox"_s},
        {u"google-chrome"_s, u"Chrome"_s},
        {u"chromium"_s, u"Chromium"_s},
        {u"chromium-browser"_s, u"Chromium"_s},
        {u"brave-browser"_s, u"Brave"_s},
        {u"microsoft-edge"_s, u"Edge"_s},
        {u"vivaldi-stable"_s, u"Vivaldi"_s},
        {u"opera"_s, u"Opera"_s},
        {u"librewolf"_s, u"LibreWolf"_s},
        {u"zen"_s, u"Zen Browser"_s},
        {u"code"_s, u"VS Code"_s},
        {u"code-oss"_s, u"VS Code"_s},
        {u"vscodium"_s, u"VSCodium"_s},
        {u"cursor"_s, u"Cursor"_s},
        {u"gnome-terminal"_s, u"Terminal"_s},
        {u"gnome-terminal-server"_s, u"Terminal"_s},
        {u"xfce4-terminal"_s, u"Terminal"_s},
        {u"konsole"_s, u"Konsole"_s},
        {u"alacritty"_s, u"Alacritty"_s},
        {u"kitty"_s, u"Kitty"_s},
        {u"tilix"_s, u"Tilix"_s},
        {u"terminator"_s, u"Terminator"_s},
        {u"nemo"_s, u"Nemo"_s},
        {u"xed"_s, u"Xed"_s},
        {u"soffice.bin"_s, u"LibreOffice"_s},
        {u"libreoffice-writer"_s, u"LibreOffice Writer"_s},
        {u"libreoffice-calc"_s, u"LibreOffice Calc"_s},
        {u"libreoffice-impress"_s, u"LibreOffice Impress"_s},
        {u"telegram-desktop"_s, u"Telegram"_s},
        {u"telegramdesktop"_s, u"Telegram"_s},
        {u"discord"_s, u"Discord"_s},
        {u"slack"_s, u"Slack"_s},
        {u"thunderbird"_s, u"Thunderbird"_s},
        {u"obsidian"_s, u"Obsidian"_s},
        {u"gimp"_s, u"GIMP"_s},
        {u"inkscape"_s, u"Inkscape"_s},
        {u"evince"_s, u"Document Viewer"_s},
        {u"xreader"_s, u"Xreader"_s},
        {u"pix"_s, u"Pix"_s},
    };

    if (const auto it = kKnownApps.constFind(key); it != kKnownApps.constEnd()) {
        return *it;
    }
    if (key.startsWith(u"libreoffice"_s)) {
        return u"LibreOffice"_s;
    }

    QString clean = !rawClass.isEmpty() ? rawClass : rawName;
    if (clean.endsWith(u".bin"_s, Qt::CaseInsensitive)) {
        clean.chop(4);
    }
    if (clean.endsWith(u"-server"_s, Qt::CaseInsensitive)) {
        clean.chop(7);
    }
    clean.replace(QLatin1Char('_'), QLatin1Char(' '));
    clean.replace(QLatin1Char('-'), QLatin1Char(' '));
    clean = clean.simplified();
    if (!clean.isEmpty() && clean.at(0).isLower()) {
        clean[0] = clean.at(0).toUpper();
    }
    return clean;
}

QString cleanWindowTitle(const QString &rawTitle, const QString &appName)
{
    QString title = rawTitle.simplified();
    if (title.isEmpty()) {
        return {};
    }

    // Remove common trailing browser/editor branding suffixes so the card stays clean
    static const QStringList kSuffixes = {
        u" — Mozilla Firefox"_s,
        u" - Mozilla Firefox"_s,
        u" — Firefox"_s,
        u" - Google Chrome"_s,
        u" — Google Chrome"_s,
        u" - Chromium"_s,
        u" - Brave"_s,
        u" - Microsoft Edge"_s,
        u" - Vivaldi"_s,
        u" - Opera"_s,
        u" - Visual Studio Code"_s,
        u" — LibreOffice Writer"_s,
        u" — LibreOffice Calc"_s,
        u" — LibreOffice Impress"_s,
        u" - LibreOffice Writer"_s,
        u" - LibreOffice Calc"_s,
        u" - LibreOffice"_s,
    };
    for (const QString &suf : kSuffixes) {
        if (title.endsWith(suf, Qt::CaseInsensitive)) {
            title.chop(suf.size());
            title = title.trimmed();
            break;
        }
    }

    if (!appName.isEmpty()) {
        for (const QString &sep : {u" — "_s, u" - "_s}) {
            const QString dynSuffix = sep + appName;
            if (title.endsWith(dynSuffix, Qt::CaseInsensitive)) {
                title.chop(dynSuffix.size());
                title = title.trimmed();
                break;
            }
        }
        if (title.compare(appName, Qt::CaseInsensitive) == 0) {
            return {};
        }
    }

    if (title.size() > 140) {
        title = title.left(137) + u"…"_s;
    }
    return title;
}

ClipboardSourceInfo captureClipboardSource(QClipboard::Mode mode, const QMimeData *mimeData)
{
    ClipboardSourceInfo info;
    info.url = extractSourceUrlFromMime(mimeData);

    Display *dpy = x11Display();
    if (!dpy) {
        return info;
    }

    const Atom selAtom = XInternAtom(dpy, mode == QClipboard::Selection ? "PRIMARY" : "CLIPBOARD", False);
    const Window ownerWin = (selAtom != None) ? XGetSelectionOwner(dpy, selAtom) : None;
    const Window activeWin = queryActiveWindow(dpy);

    X11WindowMeta ownerMeta = inspectX11Window(dpy, ownerWin);
    if (ownerMeta.isMklipper) {
        return {};
    }

    X11WindowMeta activeMeta = (activeWin != ownerWin) ? inspectX11Window(dpy, activeWin) : ownerMeta;

    QString rawClass = ownerMeta.resClass;
    QString rawName = ownerMeta.resName;
    if (rawClass.isEmpty() && rawName.isEmpty() && !activeMeta.isMklipper) {
        rawClass = activeMeta.resClass;
        rawName = activeMeta.resName;
    }

    info.app = prettifyAppName(rawClass, rawName);

    QString rawTitle = ownerMeta.title;
    if ((rawTitle.isEmpty() || rawTitle.compare(info.app, Qt::CaseInsensitive) == 0)
        && !activeMeta.isMklipper
        && !activeMeta.title.isEmpty()) {
        const bool samePid = (ownerMeta.pid != 0 && ownerMeta.pid == activeMeta.pid);
        const bool sameClass = (!rawClass.isEmpty() && rawClass.compare(activeMeta.resClass, Qt::CaseInsensitive) == 0);
        if (samePid || sameClass || (ownerMeta.resClass.isEmpty() && ownerMeta.resName.isEmpty())) {
            rawTitle = activeMeta.title;
        }
    }

    info.title = cleanWindowTitle(rawTitle, info.app);
    return info;
}

QString formatCompactSourceLabel(const QString &app, const QString &title, const QString &url)
{
    QString context;
    if (!url.isEmpty()) {
        const QUrl parsed(url);
        QString host = parsed.host().toLower();
        if (host.startsWith(u"www."_s)) {
            host.remove(0, 4);
        }
        context = host.isEmpty() ? url : host;
    } else if (!title.isEmpty()) {
        context = title;
    }

    if (!app.isEmpty() && !context.isEmpty()) {
        return QStringLiteral("%1 · %2").arg(app, context);
    }
    if (!app.isEmpty()) {
        return app;
    }
    return context;
}
