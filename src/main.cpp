/*
    SPDX-FileCopyrightText: 2026 Mklipper contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "flaticons.h"
#include "klipperapp.h"
#include "settings.h"
#include "themestyle.h"
#include "translations.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QGuiApplication>
#include <QIcon>
#include <QProcess>
#include <cstring>

using namespace Qt::StringLiterals;

namespace
{
// Ultra-fast D-Bus forwarding without initializing QtWidgets/GTK when invoked via hotkey (e.g. Super+V -> mklipper --toggle)
bool tryFastForwardToExistingInstance(int argc, char *argv[])
{
    const char *method = nullptr;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--toggle") == 0) {
            method = "toggleKlipperPopupMenu";
        } else if (std::strcmp(argv[i], "--popup") == 0) {
            method = "showKlipperPopupMenu";
        } else if (std::strcmp(argv[i], "--cycle-next") == 0) {
            method = "slotCycleNext";
        } else if (std::strcmp(argv[i], "--cycle-prev") == 0) {
            method = "slotCyclePrev";
        } else if (std::strcmp(argv[i], "--action") == 0) {
            method = "showKlipperManuallyInvokeActionMenu";
        } else if (std::strcmp(argv[i], "--clear") == 0) {
            method = "clearClipboardHistory";
        } else if (std::strcmp(argv[i], "--configure") == 0) {
            method = "configure";
        } else if (std::strcmp(argv[i], "--check-updates") == 0) {
            method = "checkForUpdates";
        } else if (std::strcmp(argv[i], "--quit") == 0) {
            method = "quit";
        } else if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0
                   || std::strcmp(argv[i], "--version") == 0 || std::strcmp(argv[i], "-v") == 0) {
            return false;
        }
    }
    if (!method) {
        return false;
    }

    QCoreApplication coreApp(argc, argv);
    QDBusInterface existing(u"org.mklipper.App"_s, u"/klipper"_s, u"org.kde.klipper.klipper"_s, QDBusConnection::sessionBus());
    if (existing.isValid()) {
        existing.call(QDBus::NoBlock, QString::fromLatin1(method));
        QDBusConnection::sessionBus().call(
            QDBusMessage::createMethodCall(u"org.mklipper.App"_s, u"/klipper"_s, u"org.freedesktop.DBus.Peer"_s, u"Ping"_s));
        return true;
    }

    if (std::strcmp(method, "quit") == 0) {
        return true;
    }

    // No existing instance running yet: launch full GUI instance with same args
    QStringList args;
    for (int i = 1; i < argc; ++i) {
        args.append(QString::fromLocal8Bit(argv[i]));
    }
    args.append(u"--no-fast-path"_s);
    QProcess::startDetached(QCoreApplication::applicationFilePath(), args);
    return true;
}
} // namespace

int main(int argc, char *argv[])
{
    bool skipFastPath = false;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--no-fast-path") == 0) {
            skipFastPath = true;
            break;
        }
    }
    if (!skipFastPath && tryFastForwardToExistingInstance(argc, argv)) {
        return 0;
    }

    // Adopt Linux Mint's native GTK theme if user didn't override QT_QPA_PLATFORMTHEME
    if (!qEnvironmentVariableIsSet("QT_QPA_PLATFORMTHEME")) {
        qputenv("QT_QPA_PLATFORMTHEME", "gtk3");
    }

    // Inherit Cinnamon's configured cursor size & theme before Qt XCB initializes libXcursor
    ThemeStyle::syncCursorEnvironment();

    // Crisp fractional display scaling (125%, 150%, 175%, etc.) on HiDPI Linux Mint setups
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    QApplication app(argc, argv);
    app.setApplicationName(u"mklipper"_s);
    app.setApplicationDisplayName(u"Mklipper"_s);
    app.setApplicationVersion(QStringLiteral(MKLIPPER_VERSION));
    app.setOrganizationName(u"mklipper"_s);
    app.setDesktopFileName(u"mklipper"_s);
    app.setWindowIcon(QIcon(u":/icons/mklipper.svg"_s));
    app.setQuitOnLastWindowClosed(false);

    // Sync exact Linux Mint Cinnamon GTK3 theme & accent color into Qt's palette
    ThemeStyle::syncApplicationPalette();
    app.setWindowIcon(FlatIcons::appLogoIcon(ThemeTokens::fromPalette(app.palette()).accent));

    MklipperTranslations::applyLanguage(MklipperSettings::self()->language());

    QCommandLineParser parser;
    parser.setApplicationDescription(QObject::tr("Mklipper — Gestor de portapapeles nativo para Linux Mint (port de KDE Klipper)"));
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption optPopup(u"popup"_s, QObject::tr("Mostrar el popup del historial bajo el cursor"));
    QCommandLineOption optToggle(u"toggle"_s, QObject::tr("Mostrar u ocultar el popup del historial bajo el cursor"));
    QCommandLineOption optCycleNext(u"cycle-next"_s, QObject::tr("Rotar al siguiente elemento del historial"));
    QCommandLineOption optCyclePrev(u"cycle-prev"_s, QObject::tr("Rotar al elemento anterior del historial"));
    QCommandLineOption optAction(u"action"_s, QObject::tr("Invocar acciones sobre el elemento actual"));
    QCommandLineOption optClear(u"clear"_s, QObject::tr("Vaciar el historial del portapapeles"));
    QCommandLineOption optConfigure(u"configure"_s, QObject::tr("Abrir la ventana de configuración"));
    QCommandLineOption optCheckUpdates(u"check-updates"_s, QObject::tr("Buscar actualizaciones de Mklipper"));
    QCommandLineOption optQuit(u"quit"_s, QObject::tr("Cerrar la instancia en ejecución de Mklipper"));
    QCommandLineOption optNoFast(u"no-fast-path"_s);
    optNoFast.setFlags(QCommandLineOption::HiddenFromHelp);

    parser.addOption(optPopup);
    parser.addOption(optToggle);
    parser.addOption(optCycleNext);
    parser.addOption(optCyclePrev);
    parser.addOption(optAction);
    parser.addOption(optClear);
    parser.addOption(optConfigure);
    parser.addOption(optCheckUpdates);
    parser.addOption(optQuit);
    parser.addOption(optNoFast);
    parser.process(app);

    // Single-instance check via session D-Bus
    QDBusInterface existing(u"org.mklipper.App"_s, u"/klipper"_s, u"org.kde.klipper.klipper"_s, QDBusConnection::sessionBus());
    if (existing.isValid()) {
        if (parser.isSet(optQuit)) {
            existing.call(u"quit"_s);
        } else if (parser.isSet(optClear)) {
            existing.call(u"clearClipboardHistory"_s);
        } else if (parser.isSet(optConfigure)) {
            existing.call(u"configure"_s);
        } else if (parser.isSet(optCheckUpdates)) {
            existing.call(u"checkForUpdates"_s);
        } else if (parser.isSet(optCycleNext)) {
            existing.call(u"slotCycleNext"_s);
        } else if (parser.isSet(optCyclePrev)) {
            existing.call(u"slotCyclePrev"_s);
        } else if (parser.isSet(optAction)) {
            existing.call(u"showKlipperManuallyInvokeActionMenu"_s);
        } else if (parser.isSet(optToggle)) {
            existing.call(u"toggleKlipperPopupMenu"_s);
        } else {
            existing.call(u"showKlipperPopupMenu"_s);
        }
        return 0;
    }

    if (parser.isSet(optQuit)) {
        return 0;
    }

    KlipperApp klipper;

    if (parser.isSet(optPopup) || parser.isSet(optToggle)) {
        klipper.showKlipperPopupMenu();
    } else if (parser.isSet(optConfigure)) {
        klipper.configure();
    } else if (parser.isSet(optCheckUpdates)) {
        klipper.checkForUpdates();
    } else if (parser.isSet(optCycleNext)) {
        klipper.slotCycleNext();
    } else if (parser.isSet(optCyclePrev)) {
        klipper.slotCyclePrev();
    } else if (parser.isSet(optAction)) {
        klipper.showKlipperManuallyInvokeActionMenu();
    }

    return app.exec();
}
