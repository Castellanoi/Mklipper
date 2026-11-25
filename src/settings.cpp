/*
    SPDX-FileCopyrightText: 2026 Mklipper contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "settings.h"

#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QTextStream>

MklipperSettings *MklipperSettings::self()
{
    static MklipperSettings instance;
    return &instance;
}

MklipperSettings::MklipperSettings(QObject *parent)
    : QObject(parent)
    , m_settings(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("mklipper"), QStringLiteral("mklipper"))
{
    load();
}

void MklipperSettings::load()
{
    m_settings.sync();

    m_settings.beginGroup(QStringLiteral("General"));
    m_keepClipboardContents = m_settings.value(QStringLiteral("KeepClipboardContents"), true).toBool();
    m_preventEmptyClipboard = m_settings.value(QStringLiteral("NoEmptyClipboard"), true).toBool();
    m_syncClipboards = m_settings.value(QStringLiteral("SyncClipboards"), false).toBool();
    m_saveSelection = m_settings.value(QStringLiteral("SaveSelection"), false).toBool();
    m_saveImages = m_settings.value(QStringLiteral("SaveImages"), true).toBool();
    m_maxClipItems = qBound(1, m_settings.value(QStringLiteral("MaxClipItems"), 50).toInt(), 2048);
    m_confirmClearHistory = m_settings.value(QStringLiteral("ConfirmClearHistory"), true).toBool();
    m_confirmRemoveStarred = m_settings.value(QStringLiteral("ConfirmRemoveStarred"), true).toBool();
    m_autoPaste = m_settings.value(QStringLiteral("AutoPaste"), true).toBool();
    m_keepPopupOpen = m_settings.value(QStringLiteral("KeepPopupOpen"), false).toBool();
    m_urlGrabberEnabled = m_settings.value(QStringLiteral("URLGrabberEnabled"), false).toBool();
    m_actionOnSelection = m_settings.value(QStringLiteral("ActionOnSelection"), false).toBool();
    m_timeoutForActionPopups = qBound(0, m_settings.value(QStringLiteral("TimeoutForActionPopups"), 8).toInt(), 200);
    m_settings.endGroup();

    m_settings.beginGroup(QStringLiteral("Actions"));
    m_stripWhiteSpace = m_settings.value(QStringLiteral("StripWhiteSpace"), true).toBool();
    m_replayActionInHistory = m_settings.value(QStringLiteral("ReplayActionInHistory"), false).toBool();
    m_enableMagicMimeActions = m_settings.value(QStringLiteral("EnableMagicMimeActions"), true).toBool();
    m_settings.endGroup();

    m_settings.beginGroup(QStringLiteral("Shortcuts"));
    m_shortcutPopup = m_settings.value(QStringLiteral("ShowPopup"), QStringLiteral("Meta+V")).toString();
    m_shortcutCycleNext = m_settings.value(QStringLiteral("CycleNext"), QString()).toString();
    m_shortcutCyclePrev = m_settings.value(QStringLiteral("CyclePrev"), QString()).toString();
    m_shortcutRepeatAction = m_settings.value(QStringLiteral("RepeatAction"), QString()).toString();
    m_shortcutClearHistory = m_settings.value(QStringLiteral("ClearHistory"), QString()).toString();
    m_settings.endGroup();

    m_settings.beginGroup(QStringLiteral("Updates"));
    m_autoCheckUpdates = m_settings.value(QStringLiteral("AutoCheck"), true).toBool();
    m_skippedUpdateVersion = m_settings.value(QStringLiteral("SkippedVersion"), QString()).toString();
    m_settings.endGroup();
}

void MklipperSettings::save()
{
    m_settings.beginGroup(QStringLiteral("General"));
    m_settings.setValue(QStringLiteral("Version"), QStringLiteral(MKLIPPER_VERSION));
    m_settings.setValue(QStringLiteral("KeepClipboardContents"), m_keepClipboardContents);
    m_settings.setValue(QStringLiteral("NoEmptyClipboard"), m_preventEmptyClipboard);
    m_settings.setValue(QStringLiteral("SyncClipboards"), m_syncClipboards);
    m_settings.setValue(QStringLiteral("SaveSelection"), m_saveSelection);
    m_settings.setValue(QStringLiteral("SaveImages"), m_saveImages);
    m_settings.setValue(QStringLiteral("MaxClipItems"), m_maxClipItems);
    m_settings.setValue(QStringLiteral("ConfirmClearHistory"), m_confirmClearHistory);
    m_settings.setValue(QStringLiteral("ConfirmRemoveStarred"), m_confirmRemoveStarred);
    m_settings.setValue(QStringLiteral("AutoPaste"), m_autoPaste);
    m_settings.setValue(QStringLiteral("KeepPopupOpen"), m_keepPopupOpen);
    m_settings.setValue(QStringLiteral("URLGrabberEnabled"), m_urlGrabberEnabled);
    m_settings.setValue(QStringLiteral("ActionOnSelection"), m_actionOnSelection);
    m_settings.setValue(QStringLiteral("TimeoutForActionPopups"), m_timeoutForActionPopups);
    m_settings.endGroup();

    m_settings.beginGroup(QStringLiteral("Actions"));
    m_settings.setValue(QStringLiteral("StripWhiteSpace"), m_stripWhiteSpace);
    m_settings.setValue(QStringLiteral("ReplayActionInHistory"), m_replayActionInHistory);
    m_settings.setValue(QStringLiteral("EnableMagicMimeActions"), m_enableMagicMimeActions);
    m_settings.endGroup();

    m_settings.beginGroup(QStringLiteral("Shortcuts"));
    m_settings.setValue(QStringLiteral("ShowPopup"), m_shortcutPopup);
    m_settings.setValue(QStringLiteral("CycleNext"), m_shortcutCycleNext);
    m_settings.setValue(QStringLiteral("CyclePrev"), m_shortcutCyclePrev);
    m_settings.setValue(QStringLiteral("RepeatAction"), m_shortcutRepeatAction);
    m_settings.setValue(QStringLiteral("ClearHistory"), m_shortcutClearHistory);
    m_settings.endGroup();

    m_settings.beginGroup(QStringLiteral("Updates"));
    m_settings.setValue(QStringLiteral("AutoCheck"), m_autoCheckUpdates);
    m_settings.setValue(QStringLiteral("SkippedVersion"), m_skippedUpdateVersion);
    m_settings.endGroup();

    m_settings.sync();
    Q_EMIT settingsChanged();
}

static QString userAutostartPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + QStringLiteral("/autostart/mklipper.desktop");
}

bool MklipperSettings::autostartEnabled()
{
    const QString userFile = userAutostartPath();
    if (QFile::exists(userFile)) {
        QFile f(userFile);
        if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            const QString content = QString::fromUtf8(f.readAll());
            if (content.contains(QStringLiteral("Hidden=true"))
                || content.contains(QStringLiteral("X-GNOME-Autostart-enabled=false"))) {
                return false;
            }
            return true;
        }
    }
    return QFile::exists(QStringLiteral("/etc/xdg/autostart/mklipper.desktop"));
}

void MklipperSettings::setAutostartEnabled(bool enabled)
{
    const QString userFile = userAutostartPath();
    QDir().mkpath(QFileInfo(userFile).absolutePath());
    QFile f(userFile);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        return;
    }
    QTextStream out(&f);
    out << "[Desktop Entry]\n"
        << "Type=Application\n"
        << "Name=Mklipper\n"
        << "Comment=Clipboard Manager (Mklipper)\n"
        << "Exec=mklipper\n"
        << "Icon=mklipper\n"
        << "Terminal=false\n"
        << "StartupNotify=false\n"
        << "Hidden=" << (enabled ? "false" : "true") << "\n"
        << "X-GNOME-Autostart-enabled=" << (enabled ? "true" : "false") << "\n";
}
