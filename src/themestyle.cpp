/*
    SPDX-FileCopyrightText: 2026 Mklipper contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "themestyle.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QRegularExpression>
#include <QSettings>
#include <QTextStream>

#include <fcntl.h>
#include <poll.h>
#include <sys/wait.h>
#include <unistd.h>

namespace
{
QColor blend(const QColor &base, const QColor &overlay, qreal ratio)
{
    const qreal inv = 1.0 - ratio;
    return QColor::fromRgbF(
        base.redF() * inv + overlay.redF() * ratio,
        base.greenF() * inv + overlay.greenF() * ratio,
        base.blueF() * inv + overlay.blueF() * ratio,
        1.0);
}

QString readGSetting(const QString &schema, const QString &key)
{
    int pipefd[2];
    if (pipe(pipefd) != 0) {
        return {};
    }

    const QByteArray schemaUtf8 = schema.toUtf8();
    const QByteArray keyUtf8 = key.toUtf8();

    const pid_t pid = fork();
    if (pid < 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        return {};
    }
    if (pid == 0) {
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);
        int devNull = open("/dev/null", O_WRONLY);
        if (devNull >= 0) {
            dup2(devNull, STDERR_FILENO);
            close(devNull);
        }
        char *const argv[] = {
            const_cast<char *>("gsettings"),
            const_cast<char *>("get"),
            const_cast<char *>(schemaUtf8.constData()),
            const_cast<char *>(keyUtf8.constData()),
            nullptr,
        };
        execvp("gsettings", argv);
        _exit(127);
    }

    close(pipefd[1]);
    QByteArray rawOut;
    char buf[256];
    struct pollfd pfd { pipefd[0], POLLIN, 0 };
    while (poll(&pfd, 1, 600) > 0) {
        if (pfd.revents & (POLLIN | POLLHUP)) {
            const ssize_t n = read(pipefd[0], buf, sizeof(buf));
            if (n <= 0) {
                break;
            }
            rawOut.append(buf, static_cast<qsizetype>(n));
            if (rawOut.size() > 1024) {
                break;
            }
        } else {
            break;
        }
    }
    close(pipefd[0]);

    int status = 0;
    waitpid(pid, &status, 0);
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        return {};
    }

    QString out = QString::fromUtf8(rawOut).trimmed();
    if (out.size() >= 2 && ((out.startsWith(QLatin1Char('\'')) && out.endsWith(QLatin1Char('\'')))
                            || (out.startsWith(QLatin1Char('"')) && out.endsWith(QLatin1Char('"'))))) {
        out = out.mid(1, out.size() - 2);
    }
    return out.trimmed();
}

QString detectGtkThemeName()
{
    QString name = readGSetting(QStringLiteral("org.cinnamon.desktop.interface"), QStringLiteral("gtk-theme"));
    if (name.isEmpty()) {
        name = readGSetting(QStringLiteral("org.gnome.desktop.interface"), QStringLiteral("gtk-theme"));
    }
    if (name.isEmpty()) {
        const QString iniPath = QDir::homePath() + QStringLiteral("/.config/gtk-3.0/settings.ini");
        if (QFileInfo::exists(iniPath)) {
            QSettings ini(iniPath, QSettings::IniFormat);
            name = ini.value(QStringLiteral("Settings/gtk-theme-name")).toString().trimmed();
        }
    }
    return name;
}

bool detectPreferDark()
{
    const QString portal = readGSetting(QStringLiteral("org.x.apps.portal"), QStringLiteral("color-scheme"));
    if (portal.contains(QStringLiteral("dark"), Qt::CaseInsensitive)) {
        return true;
    }
    const QString gnome = readGSetting(QStringLiteral("org.gnome.desktop.interface"), QStringLiteral("color-scheme"));
    if (gnome.contains(QStringLiteral("dark"), Qt::CaseInsensitive)) {
        return true;
    }
    const QString iniPath = QDir::homePath() + QStringLiteral("/.config/gtk-3.0/settings.ini");
    if (QFileInfo::exists(iniPath)) {
        QSettings ini(iniPath, QSettings::IniFormat);
        if (ini.value(QStringLiteral("Settings/gtk-application-prefer-dark-theme")).toBool()) {
            return true;
        }
    }
    return false;
}

bool isSafeThemeName(const QString &name)
{
    return !name.isEmpty()
        && !name.contains(QLatin1Char('/'))
        && !name.contains(QLatin1Char('\\'))
        && !name.contains(QStringLiteral(".."));
}

QString findGtkThemeCss(const QString &themeName, bool preferDark)
{
    if (!isSafeThemeName(themeName)) {
        return {};
    }
    const QString home = QDir::homePath();
    const QStringList roots = {
        home + QStringLiteral("/.themes"),
        home + QStringLiteral("/.local/share/themes"),
        QStringLiteral("/usr/local/share/themes"),
        QStringLiteral("/usr/share/themes"),
    };
    const QStringList candidates = preferDark
        ? QStringList{QStringLiteral("gtk-3.0/gtk-dark.css"), QStringLiteral("gtk-3.0/gtk.css")}
        : QStringList{QStringLiteral("gtk-3.0/gtk.css")};

    for (const QString &root : roots) {
        for (const QString &rel : candidates) {
            const QString full = root + QLatin1Char('/') + themeName + QLatin1Char('/') + rel;
            if (QFileInfo::exists(full)) {
                return full;
            }
        }
    }
    return {};
}

QColor parseCssColorValue(const QString &raw, const QHash<QString, QString> &rawDefs, int depth = 0)
{
    if (depth > 6) {
        return {};
    }
    const QString s = raw.trimmed();
    if (s.isEmpty()) {
        return {};
    }
    if (s.startsWith(QLatin1Char('@'))) {
        const QString ref = s.mid(1).trimmed();
        const auto it = rawDefs.constFind(ref);
        if (it != rawDefs.constEnd()) {
            return parseCssColorValue(it.value(), rawDefs, depth + 1);
        }
        return {};
    }
    if (s.startsWith(QLatin1Char('#'))) {
        QColor c(s);
        return c.isValid() ? c : QColor();
    }

    static const QRegularExpression rgbaRe(
        QStringLiteral(R"(^rgba?\(\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)(?:\s*,\s*([0-9]*\.?[0-9]+))?\s*\)$)"),
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch m = rgbaRe.match(s);
    if (m.hasMatch()) {
        const int r = qBound(0, m.captured(1).toInt(), 255);
        const int g = qBound(0, m.captured(2).toInt(), 255);
        const int b = qBound(0, m.captured(3).toInt(), 255);
        int a = 255;
        if (!m.captured(4).isEmpty()) {
            const double af = m.captured(4).toDouble();
            a = qBound(0, qRound((af <= 1.0 ? af * 255.0 : af)), 255);
        }
        return QColor(r, g, b, a);
    }

    QColor named(s);
    return named.isValid() ? named : QColor();
}

void collectDefineColorsFromFile(const QString &cssPath, QHash<QString, QString> &outDefs, int depth = 0)
{
    if (depth > 2 || cssPath.isEmpty()) {
        return;
    }
    QFile f(cssPath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return;
    }
    const QFileInfo fi(cssPath);
    const QDir baseDir = fi.dir();

    static const QRegularExpression importRe(
        QStringLiteral(R"(^\s*@import\s+url\(\s*["']?([^"')]+)["']?\s*\)\s*;)"));
    static const QRegularExpression defRe(
        QStringLiteral(R"(^\s*@define-color\s+([A-Za-z0-9_-]+)\s+([^;]+);)"));

    QTextStream in(&f);
    int linesRead = 0;
    while (!in.atEnd() && linesRead < 15000) {
        ++linesRead;
        const QString line = in.readLine();
        if (!line.contains(QLatin1Char('@'))) {
            continue;
        }
        if (const auto im = importRe.match(line); im.hasMatch()) {
            const QString target = im.captured(1).trimmed();
            if (!target.startsWith(QStringLiteral("resource://")) && !target.contains(QStringLiteral(".."))) {
                const QString resolved = QFileInfo(target).isAbsolute() ? target : baseDir.filePath(target);
                collectDefineColorsFromFile(resolved, outDefs, depth + 1);
            }
            continue;
        }
        if (const auto dm = defRe.match(line); dm.hasMatch()) {
            outDefs.insert(dm.captured(1).trimmed(), dm.captured(2).trimmed());
        }
    }
}
} // namespace

ThemeTokens ThemeTokens::fromPalette(const QPalette &pal)
{
    ThemeTokens t;
    const QColor rawText = pal.color(QPalette::WindowText);
    const QColor rawWindow = pal.color(QPalette::Window);
    const QColor rawBase = pal.color(QPalette::Base);

    // Derive dark/light reliably even if a child widget uses a transparent background stylesheet
    if (rawWindow.isValid() && rawWindow.alpha() > 0) {
        t.windowBg = rawWindow;
        t.isDark = t.windowBg.lightness() < 128;
    } else if (rawBase.isValid() && rawBase.alpha() > 0) {
        t.windowBg = rawBase;
        t.isDark = t.windowBg.lightness() < 128;
    } else {
        t.isDark = rawText.lightness() > 128;
        t.windowBg = t.isDark ? QColor(46, 46, 51) : QColor(248, 248, 249);
    }

    t.textPrimary = QColor(rawText.red(), rawText.green(), rawText.blue(), 242);
    t.textSecondary = QColor(rawText.red(), rawText.green(), rawText.blue(), t.isDark ? 178 : 168);
    t.textMuted = QColor(rawText.red(), rawText.green(), rawText.blue(), t.isDark ? 120 : 118);

    if (t.isDark) {
        // Cinnamon 6.6 dark surface layering (works cleanly even when Mint-Y Base == Window)
        t.surfaceBg = blend(t.windowBg, Qt::white, 0.055);
        t.surfaceHoverBg = blend(t.windowBg, Qt::white, 0.11);
        t.surfaceActiveBg = blend(t.windowBg, Qt::white, 0.16);
        t.inputBg = blend(t.windowBg, Qt::white, 0.07);
        t.hairline = QColor(255, 255, 255, 20);
        t.borderSubtle = QColor(255, 255, 255, 34);
    } else {
        // Cinnamon 6.6 / Mint-Y light surface layering
        const bool validLightBase = rawBase.isValid() && rawBase.alpha() > 0 && rawBase.lightness() > t.windowBg.lightness();
        t.surfaceBg = validLightBase ? rawBase : QColor(255, 255, 255);
        t.surfaceHoverBg = blend(t.windowBg, Qt::black, 0.055);
        t.surfaceActiveBg = blend(t.windowBg, Qt::black, 0.10);
        t.inputBg = t.surfaceBg;
        t.hairline = QColor(0, 0, 0, 18);
        t.borderSubtle = QColor(0, 0, 0, 32);
    }

    t.accent = pal.color(QPalette::Highlight);
    if (!t.accent.isValid() || t.accent.alpha() == 0) {
        t.accent = QColor(53, 132, 228);
    }
    t.accentFg = pal.color(QPalette::HighlightedText);
    if (!t.accentFg.isValid() || t.accentFg.alpha() == 0) {
        t.accentFg = Qt::white;
    }
    t.accentHover = t.isDark ? blend(t.accent, Qt::white, 0.12) : blend(t.accent, Qt::black, 0.08);
    t.accentPressed = t.isDark ? blend(t.accent, Qt::black, 0.12) : blend(t.accent, Qt::black, 0.18);
    t.accentSoft = QColor(t.accent.red(), t.accent.green(), t.accent.blue(), t.isDark ? 42 : 28);
    t.accentSoftStrong = QColor(t.accent.red(), t.accent.green(), t.accent.blue(), t.isDark ? 64 : 44);

    t.danger = t.isDark ? QColor(240, 92, 92) : QColor(210, 44, 48);
    t.dangerSoft = QColor(224, 27, 36, t.isDark ? 52 : 28);

    t.warning = t.isDark ? QColor(246, 185, 59) : QColor(204, 132, 12);
    t.warningSoft = QColor(246, 185, 59, t.isDark ? 46 : 36);

    return t;
}

namespace ThemeStyle
{
void syncCursorEnvironment()
{
    if (!qEnvironmentVariableIsSet("XCURSOR_SIZE")) {
        QString sizeStr = readGSetting(QStringLiteral("org.cinnamon.desktop.interface"), QStringLiteral("cursor-size"));
        if (sizeStr.isEmpty()) {
            sizeStr = readGSetting(QStringLiteral("org.gnome.desktop.interface"), QStringLiteral("cursor-size"));
        }
        if (sizeStr.isEmpty()) {
            const QString iniPath = QDir::homePath() + QStringLiteral("/.config/gtk-3.0/settings.ini");
            if (QFileInfo::exists(iniPath)) {
                QSettings ini(iniPath, QSettings::IniFormat);
                sizeStr = ini.value(QStringLiteral("Settings/gtk-cursor-theme-size")).toString().trimmed();
            }
        }
        bool ok = false;
        const int sz = sizeStr.toInt(&ok);
        if (ok && sz >= 8 && sz <= 128) {
            qputenv("XCURSOR_SIZE", QByteArray::number(sz));
        }
    }

    if (!qEnvironmentVariableIsSet("XCURSOR_THEME")) {
        QString theme = readGSetting(QStringLiteral("org.cinnamon.desktop.interface"), QStringLiteral("cursor-theme"));
        if (theme.isEmpty()) {
            theme = readGSetting(QStringLiteral("org.gnome.desktop.interface"), QStringLiteral("cursor-theme"));
        }
        if (theme.isEmpty()) {
            const QString iniPath = QDir::homePath() + QStringLiteral("/.config/gtk-3.0/settings.ini");
            if (QFileInfo::exists(iniPath)) {
                QSettings ini(iniPath, QSettings::IniFormat);
                theme = ini.value(QStringLiteral("Settings/gtk-cursor-theme-name")).toString().trimmed();
            }
        }
        if (isSafeThemeName(theme)) {
            qputenv("XCURSOR_THEME", theme.toUtf8());
        }
    }
}

bool syncApplicationPalette()
{
    if (!qApp) {
        return false;
    }

    const QString themeName = detectGtkThemeName();
    const bool preferDark = detectPreferDark();
    const QString cssPath = findGtkThemeCss(themeName, preferDark);

    QHash<QString, QString> defs;
    if (!cssPath.isEmpty()) {
        collectDefineColorsFromFile(cssPath, defs);
    }
    const QString userCss = QDir::homePath() + QStringLiteral("/.config/gtk-3.0/gtk.css");
    if (QFileInfo::exists(userCss)) {
        collectDefineColorsFromFile(userCss, defs);
    }

    auto resolve = [&defs](std::initializer_list<const char *> names) -> QColor {
        for (const char *n : names) {
            const auto it = defs.constFind(QString::fromLatin1(n));
            if (it != defs.constEnd()) {
                const QColor c = parseCssColorValue(it.value(), defs);
                if (c.isValid() && c.alpha() > 0) {
                    return c;
                }
            }
        }
        return {};
    };

    const QColor bg = resolve({"theme_bg_color", "window_bg_color"});
    const QColor fg = resolve({"theme_fg_color", "window_fg_color"});
    const QColor base = resolve({"theme_base_color", "view_bg_color"});
    const QColor text = resolve({"theme_text_color", "view_fg_color"});
    const QColor accent = resolve({"theme_selected_bg_color", "accent_bg_color", "accent_color"});
    const QColor accentFg = resolve({"theme_selected_fg_color", "accent_fg_color"});
    const QColor border = resolve({"borders", "unfocused_borders"});

    if (!bg.isValid() && !accent.isValid()) {
        return false;
    }

    QPalette pal = qApp->palette();

    if (bg.isValid()) {
        const QColor opaqueBg(bg.red(), bg.green(), bg.blue(), 255);
        const bool isDark = opaqueBg.lightness() < 128;
        pal.setColor(QPalette::Window, opaqueBg);
        pal.setColor(QPalette::Button, isDark ? blend(opaqueBg, Qt::white, 0.08) : blend(opaqueBg, Qt::black, 0.04));
        pal.setColor(QPalette::AlternateBase, isDark ? blend(opaqueBg, Qt::white, 0.05) : blend(opaqueBg, Qt::black, 0.03));
        pal.setColor(QPalette::ToolTipBase, isDark ? blend(opaqueBg, Qt::white, 0.10) : QColor(255, 255, 255));
    }
    if (base.isValid()) {
        pal.setColor(QPalette::Base, QColor(base.red(), base.green(), base.blue(), 255));
    }
    if (fg.isValid()) {
        const QColor opaqueFg(fg.red(), fg.green(), fg.blue(), 255);
        pal.setColor(QPalette::WindowText, opaqueFg);
        pal.setColor(QPalette::ButtonText, opaqueFg);
        pal.setColor(QPalette::ToolTipText, opaqueFg);
        QColor disabledFg = opaqueFg;
        disabledFg.setAlpha(120);
        pal.setColor(QPalette::Disabled, QPalette::WindowText, disabledFg);
        pal.setColor(QPalette::Disabled, QPalette::ButtonText, disabledFg);
    }
    if (text.isValid()) {
        const QColor opaqueText(text.red(), text.green(), text.blue(), 255);
        pal.setColor(QPalette::Text, opaqueText);
        QColor placeholder = opaqueText;
        placeholder.setAlpha(125);
        pal.setColor(QPalette::PlaceholderText, placeholder);
        QColor disabledText = opaqueText;
        disabledText.setAlpha(120);
        pal.setColor(QPalette::Disabled, QPalette::Text, disabledText);
    }
    if (accent.isValid()) {
        const QColor opaqueAccent(accent.red(), accent.green(), accent.blue(), 255);
        pal.setColor(QPalette::Highlight, opaqueAccent);
        pal.setColor(QPalette::Link, opaqueAccent);
    }
    if (accentFg.isValid()) {
        pal.setColor(QPalette::HighlightedText, QColor(accentFg.red(), accentFg.green(), accentFg.blue(), 255));
    } else if (accent.isValid()) {
        pal.setColor(QPalette::HighlightedText, accent.lightness() > 160 ? Qt::black : Qt::white);
    }
    if (border.isValid()) {
        pal.setColor(QPalette::Mid, QColor(border.red(), border.green(), border.blue(), 255));
    }

    qApp->setPalette(pal);
    return true;
}

QString rgbaCss(const QColor &c)
{
    return QStringLiteral("rgba(%1, %2, %3, %4)")
        .arg(c.red())
        .arg(c.green())
        .arg(c.blue())
        .arg(c.alpha());
}

QString popupStyleSheet(const ThemeTokens &t)
{
    QString css = QStringLiteral(R"(
        QFrame#mklipperPopup {
            background-color: %WINDOW%;
            border: 1px solid %BORDER_SUBTLE%;
        }
        QToolTip {
            background-color: %INPUT%;
            color: %TEXT%;
            border: 1px solid %BORDER_SUBTLE%;
            border-radius: 7px;
            padding: 5px 9px;
        }
        QFrame#mklipperDivider {
            background-color: %HAIRLINE%;
            border: none;
            max-height: 1px;
            min-height: 1px;
        }
        QLineEdit#mklipperSearch {
            background-color: %INPUT%;
            color: %TEXT%;
            border: 1px solid %BORDER_SUBTLE%;
            border-radius: 8px;
            padding: 2px 10px 2px 6px;
            selection-background-color: %ACCENT%;
            selection-color: %ACCENT_FG%;
        }
        QLineEdit#mklipperSearch:hover {
            background-color: %SURFACE_HOVER%;
        }
        QLineEdit#mklipperSearch:focus {
            background-color: %INPUT%;
            border: 1.5px solid %ACCENT%;
        }
        QToolButton#mklipperIconBtn {
            background-color: transparent;
            border: none;
            border-radius: 8px;
            padding: 0px;
        }
        QToolButton#mklipperIconBtn:hover {
            background-color: %SURFACE_HOVER%;
        }
        QToolButton#mklipperIconBtn:pressed {
            background-color: %SURFACE_ACTIVE%;
        }
        QToolButton#mklipperStarBtn {
            background-color: transparent;
            border: none;
            border-radius: 8px;
            padding: 0px;
        }
        QToolButton#mklipperStarBtn:hover {
            background-color: %SURFACE_HOVER%;
        }
        QToolButton#mklipperStarBtn:checked {
            background-color: %WARNING_SOFT%;
        }
        QToolButton#mklipperStarBtn:checked:hover {
            background-color: %WARNING_SOFT%;
        }
        QToolButton#mklipperDangerBtn {
            background-color: transparent;
            border: none;
            border-radius: 8px;
            padding: 0px;
        }
        QToolButton#mklipperDangerBtn:hover {
            background-color: %DANGER_SOFT%;
        }
        QToolButton#mklipperDangerBtn:pressed {
            background-color: %DANGER_SOFT%;
        }
        QFrame#mklipperPasswordBanner {
            background-color: %WARNING_SOFT%;
            border: none;
            border-radius: 8px;
        }
        QPushButton[flatAction="true"] {
            background-color: %SURFACE_HOVER%;
            color: %TEXT%;
            border: none;
            border-radius: 7px;
            padding: 6px 14px;
            font-weight: 500;
        }
        QPushButton[flatAction="true"]:hover {
            background-color: %SURFACE_ACTIVE%;
        }
        QPushButton[flatAction="true"]:pressed {
            background-color: %BORDER_SUBTLE%;
        }
        QPushButton[primary="true"] {
            background-color: %ACCENT%;
            color: %ACCENT_FG%;
            border: none;
            border-radius: 7px;
            padding: 6px 15px;
            font-weight: 600;
        }
        QPushButton[primary="true"]:hover {
            background-color: %ACCENT_HOVER%;
        }
        QPushButton[primary="true"]:pressed {
            background-color: %ACCENT_PRESSED%;
        }
        QPushButton[primary="true"]:disabled {
            background-color: %SURFACE_HOVER%;
            color: %TEXT_MUTED%;
        }
        QPushButton[ghost="true"] {
            background-color: transparent;
            color: %TEXT%;
            border: none;
            border-radius: 7px;
            padding: 5px 10px;
            font-weight: 500;
        }
        QPushButton[ghost="true"]:hover {
            background-color: %SURFACE_HOVER%;
        }
        QPushButton[ghost="true"]:pressed {
            background-color: %SURFACE_ACTIVE%;
        }
        QPlainTextEdit#mklipperEditArea {
            background-color: %INPUT%;
            color: %TEXT%;
            border: 1px solid %BORDER_SUBTLE%;
            border-radius: 9px;
            padding: 10px;
            selection-background-color: %ACCENT%;
            selection-color: %ACCENT_FG%;
        }
        QPlainTextEdit#mklipperEditArea:focus {
            border: 1.5px solid %ACCENT%;
        }
        QScrollBar:vertical {
            background: transparent;
            width: 6px;
            margin: 4px 1px 4px 0px;
        }
        QScrollBar::handle:vertical {
            background: %SCROLL_HANDLE%;
            min-height: 28px;
            border-radius: 3px;
        }
        QScrollBar::handle:vertical:hover {
            background: %SCROLL_HOVER%;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0px;
        }
        QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {
            background: none;
        }
    )");

    QColor scrollHandle = t.textPrimary;
    scrollHandle.setAlpha(t.isDark ? 55 : 48);
    QColor scrollHover = t.textPrimary;
    scrollHover.setAlpha(t.isDark ? 105 : 95);

    css.replace(QStringLiteral("%WINDOW%"), rgbaCss(t.windowBg));
    css.replace(QStringLiteral("%SURFACE_HOVER%"), rgbaCss(t.surfaceHoverBg));
    css.replace(QStringLiteral("%SURFACE_ACTIVE%"), rgbaCss(t.surfaceActiveBg));
    css.replace(QStringLiteral("%INPUT%"), rgbaCss(t.inputBg));
    css.replace(QStringLiteral("%HAIRLINE%"), rgbaCss(t.hairline));
    css.replace(QStringLiteral("%BORDER_SUBTLE%"), rgbaCss(t.borderSubtle));
    css.replace(QStringLiteral("%TEXT%"), rgbaCss(t.textPrimary));
    css.replace(QStringLiteral("%TEXT_MUTED%"), rgbaCss(t.textMuted));
    css.replace(QStringLiteral("%ACCENT%"), rgbaCss(t.accent));
    css.replace(QStringLiteral("%ACCENT_FG%"), rgbaCss(t.accentFg));
    css.replace(QStringLiteral("%ACCENT_HOVER%"), rgbaCss(t.accentHover));
    css.replace(QStringLiteral("%ACCENT_PRESSED%"), rgbaCss(t.accentPressed));
    css.replace(QStringLiteral("%DANGER_SOFT%"), rgbaCss(t.dangerSoft));
    css.replace(QStringLiteral("%WARNING_SOFT%"), rgbaCss(t.warningSoft));
    css.replace(QStringLiteral("%SCROLL_HANDLE%"), rgbaCss(scrollHandle));
    css.replace(QStringLiteral("%SCROLL_HOVER%"), rgbaCss(scrollHover));
    return css;
}

QString dialogStyleSheet(const ThemeTokens &t)
{
    QString css = QStringLiteral(R"(
        QDialog {
            background-color: %WINDOW%;
            color: %TEXT%;
        }
        QToolTip {
            background-color: %SURFACE%;
            color: %TEXT%;
            border: 1px solid %BORDER_SUBTLE%;
            border-radius: 7px;
            padding: 6px 10px;
        }
        QFrame#mklipperCard {
            background-color: %SURFACE%;
            border: 1px solid %HAIRLINE%;
            border-radius: 10px;
        }
        QFrame#mklipperTipCard {
            background-color: %ACCENT_SOFT%;
            border: none;
            border-radius: 10px;
        }
        QFrame#mklipperDivider {
            background-color: %HAIRLINE%;
            border: none;
            max-height: 1px;
            min-height: 1px;
        }
        QTabWidget::pane {
            border: none;
            background: transparent;
            margin-top: 6px;
        }
        QTabWidget::tab-bar {
            alignment: left;
        }
        QTabBar {
            background: transparent;
            qproperty-drawBase: 0;
        }
        QTabBar::tab {
            background-color: transparent;
            color: %TEXT_SEC%;
            border: none;
            border-radius: 8px;
            padding: 7px 14px;
            margin-right: 4px;
            font-weight: 500;
        }
        QTabBar::tab:hover {
            background-color: %SURFACE_HOVER%;
            color: %TEXT%;
        }
        QTabBar::tab:selected {
            background-color: %SURFACE_ACTIVE%;
            color: %TEXT%;
            font-weight: 600;
        }
        QPushButton {
            background-color: %SURFACE_HOVER%;
            color: %TEXT%;
            border: none;
            border-radius: 8px;
            padding: 7px 15px;
            font-weight: 500;
        }
        QPushButton:hover {
            background-color: %SURFACE_ACTIVE%;
        }
        QPushButton:pressed {
            background-color: %BORDER_SUBTLE%;
        }
        QPushButton:disabled {
            background-color: %SURFACE%;
            color: %TEXT_MUTED%;
        }
        QPushButton[primary="true"] {
            background-color: %ACCENT%;
            color: %ACCENT_FG%;
            font-weight: 600;
        }
        QPushButton[primary="true"]:hover {
            background-color: %ACCENT_HOVER%;
        }
        QPushButton[primary="true"]:pressed {
            background-color: %ACCENT_PRESSED%;
        }
        QPushButton[destructive="true"]:hover {
            background-color: %DANGER_SOFT%;
            color: %DANGER%;
        }
        QLineEdit, QSpinBox, QComboBox, QKeySequenceEdit {
            background-color: %INPUT%;
            color: %TEXT%;
            border: 1px solid %BORDER_SUBTLE%;
            border-radius: 7px;
            padding: 4px 8px;
            selection-background-color: %ACCENT%;
            selection-color: %ACCENT_FG%;
        }
        QLineEdit:focus, QSpinBox:focus, QComboBox:focus, QKeySequenceEdit:focus {
            border: 1.5px solid %ACCENT%;
        }
        QSpinBox {
            padding-right: 22px;
        }
        QSpinBox::up-button, QSpinBox::down-button {
            subcontrol-origin: border;
            width: 20px;
            border: none;
            background: transparent;
        }
        QSpinBox::up-button {
            subcontrol-position: top right;
            border-top-right-radius: 6px;
        }
        QSpinBox::down-button {
            subcontrol-position: bottom right;
            border-bottom-right-radius: 6px;
        }
        QSpinBox::up-button:hover, QSpinBox::down-button:hover {
            background-color: %SURFACE_HOVER%;
        }
        QSpinBox::up-arrow {
            image: url(:/icons/chevron-up.svg);
            width: 11px;
            height: 11px;
        }
        QSpinBox::down-arrow {
            image: url(:/icons/chevron-down.svg);
            width: 11px;
            height: 11px;
        }
        QComboBox::drop-down {
            subcontrol-origin: padding;
            subcontrol-position: center right;
            width: 24px;
            border: none;
            background: transparent;
        }
        QComboBox::down-arrow {
            image: url(:/icons/chevron-down.svg);
            width: 12px;
            height: 12px;
        }
        QCheckBox {
            spacing: 9px;
            color: %TEXT%;
        }
        QCheckBox:disabled {
            color: %TEXT_MUTED%;
        }
        QCheckBox::indicator {
            width: 17px;
            height: 17px;
            border-radius: 5px;
            border: 1.5px solid %BORDER_SUBTLE%;
            background-color: %INPUT%;
        }
        QCheckBox::indicator:hover {
            border-color: %ACCENT%;
        }
        QCheckBox::indicator:checked {
            background-color: %ACCENT%;
            border-color: %ACCENT%;
            image: url(:/icons/check.svg);
        }
        QCheckBox::indicator:checked:hover {
            background-color: %ACCENT_HOVER%;
            border-color: %ACCENT_HOVER%;
        }
        QCheckBox::indicator:disabled {
            background-color: %SURFACE%;
            border-color: %HAIRLINE%;
        }
        QListWidget, QTableWidget, QTextBrowser, QPlainTextEdit {
            background-color: %INPUT%;
            color: %TEXT%;
            border: 1px solid %HAIRLINE%;
            border-radius: 9px;
            padding: 6px;
            selection-background-color: %ACCENT_SOFT_STRONG%;
            selection-color: %TEXT%;
            outline: none;
        }
        QListWidget::item {
            padding: 6px 8px;
            border-radius: 6px;
        }
        QListWidget::item:selected {
            background-color: %ACCENT_SOFT_STRONG%;
            color: %TEXT%;
        }
        QListWidget::item:hover:!selected {
            background-color: %SURFACE_HOVER%;
        }
        QHeaderView::section {
            background-color: %SURFACE%;
            color: %TEXT_SEC%;
            border: none;
            border-bottom: 1px solid %HAIRLINE%;
            padding: 6px 8px;
            font-weight: 600;
        }
        QProgressBar {
            border: none;
            border-radius: 3px;
            background-color: %SURFACE_HOVER%;
            max-height: 6px;
            min-height: 6px;
            text-align: center;
        }
        QProgressBar::chunk {
            background-color: %ACCENT%;
            border-radius: 3px;
        }
        QScrollArea {
            background: transparent;
            border: none;
        }
        QScrollArea > QWidget > QWidget {
            background: transparent;
        }
        QScrollBar:vertical {
            background: transparent;
            width: 6px;
            margin: 4px 1px 4px 0px;
        }
        QScrollBar::handle:vertical {
            background: %SCROLL_HANDLE%;
            min-height: 28px;
            border-radius: 3px;
        }
        QScrollBar::handle:vertical:hover {
            background: %SCROLL_HOVER%;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0px;
        }
        QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {
            background: none;
        }
    )");

    QColor scrollHandle = t.textPrimary;
    scrollHandle.setAlpha(t.isDark ? 55 : 48);
    QColor scrollHover = t.textPrimary;
    scrollHover.setAlpha(t.isDark ? 105 : 95);

    css.replace(QStringLiteral("%WINDOW%"), rgbaCss(t.windowBg));
    css.replace(QStringLiteral("%SURFACE%"), rgbaCss(t.surfaceBg));
    css.replace(QStringLiteral("%SURFACE_HOVER%"), rgbaCss(t.surfaceHoverBg));
    css.replace(QStringLiteral("%SURFACE_ACTIVE%"), rgbaCss(t.surfaceActiveBg));
    css.replace(QStringLiteral("%INPUT%"), rgbaCss(t.inputBg));
    css.replace(QStringLiteral("%HAIRLINE%"), rgbaCss(t.hairline));
    css.replace(QStringLiteral("%BORDER_SUBTLE%"), rgbaCss(t.borderSubtle));
    css.replace(QStringLiteral("%TEXT%"), rgbaCss(t.textPrimary));
    css.replace(QStringLiteral("%TEXT_SEC%"), rgbaCss(t.textSecondary));
    css.replace(QStringLiteral("%TEXT_MUTED%"), rgbaCss(t.textMuted));
    css.replace(QStringLiteral("%ACCENT%"), rgbaCss(t.accent));
    css.replace(QStringLiteral("%ACCENT_FG%"), rgbaCss(t.accentFg));
    css.replace(QStringLiteral("%ACCENT_HOVER%"), rgbaCss(t.accentHover));
    css.replace(QStringLiteral("%ACCENT_PRESSED%"), rgbaCss(t.accentPressed));
    css.replace(QStringLiteral("%ACCENT_SOFT%"), rgbaCss(t.accentSoft));
    css.replace(QStringLiteral("%ACCENT_SOFT_STRONG%"), rgbaCss(t.accentSoftStrong));
    css.replace(QStringLiteral("%DANGER%"), rgbaCss(t.danger));
    css.replace(QStringLiteral("%DANGER_SOFT%"), rgbaCss(t.dangerSoft));
    css.replace(QStringLiteral("%SCROLL_HANDLE%"), rgbaCss(scrollHandle));
    css.replace(QStringLiteral("%SCROLL_HOVER%"), rgbaCss(scrollHover));
    return css;
}
} // namespace ThemeStyle
