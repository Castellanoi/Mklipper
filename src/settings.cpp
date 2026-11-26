/*
    SPDX-FileCopyrightText: 2026 Mklipper contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "settings.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTextStream>

namespace
{
void enforcePrivateConfigPermissions(const QString &iniPath)
{
    if (iniPath.isEmpty()) {
        return;
    }
    const QFileInfo fi(iniPath);
    const QString dirPath = fi.absolutePath();
    if (!dirPath.isEmpty() && QFile::exists(dirPath)) {
        QFile::setPermissions(dirPath,
                              QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
    }
    if (QFile::exists(iniPath)) {
        QFile::setPermissions(iniPath, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    }
}
} // namespace

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
    enforcePrivateConfigPermissions(m_settings.fileName());

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
    m_closePopupOnFocusOut = m_settings.value(QStringLiteral("ClosePopupOnFocusOut"), false).toBool();
    m_pastePlainText = m_settings.value(QStringLiteral("PastePlainText"), false).toBool();
    m_saveSourceInfo = m_settings.value(QStringLiteral("SaveSourceInfo"), true).toBool();
    m_showSourceInfo = m_settings.value(QStringLiteral("ShowSourceInfo"), true).toBool();
    m_showTrayBadge = m_settings.value(QStringLiteral("ShowTrayBadge"), true).toBool();
    m_language = m_settings.value(QStringLiteral("Language"), QStringLiteral("system")).toString();
    setIgnoredApps(m_settings.value(QStringLiteral("IgnoredApps")).toStringList());
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
    m_quickSelectDigits = m_settings.value(QStringLiteral("QuickSelectDigits"), true).toBool();
    m_shortcutPopup = m_settings.value(QStringLiteral("ShowPopup"), QStringLiteral("Meta+V")).toString();
    m_shortcutTogglePlainText = m_settings.value(QStringLiteral("TogglePlainText"), QString()).toString();
    m_shortcutTogglePause = m_settings.value(QStringLiteral("TogglePause"), QString()).toString();
    m_shortcutEditCurrent = m_settings.value(QStringLiteral("EditCurrent"), QString()).toString();
    m_shortcutShowQr = m_settings.value(QStringLiteral("ShowQr"), QString()).toString();
    m_shortcutRepeatAction = m_settings.value(QStringLiteral("RepeatAction"), QString()).toString();
    m_shortcutCycleNext = m_settings.value(QStringLiteral("CycleNext"), QString()).toString();
    m_shortcutCyclePrev = m_settings.value(QStringLiteral("CyclePrev"), QString()).toString();
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
    m_settings.setValue(QStringLiteral("ClosePopupOnFocusOut"), m_closePopupOnFocusOut);
    m_settings.setValue(QStringLiteral("PastePlainText"), m_pastePlainText);
    m_settings.setValue(QStringLiteral("SaveSourceInfo"), m_saveSourceInfo);
    m_settings.setValue(QStringLiteral("ShowSourceInfo"), m_showSourceInfo);
    m_settings.setValue(QStringLiteral("ShowTrayBadge"), m_showTrayBadge);
    m_settings.setValue(QStringLiteral("Language"), m_language);
    m_settings.setValue(QStringLiteral("IgnoredApps"), m_ignoredApps);
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
    m_settings.setValue(QStringLiteral("QuickSelectDigits"), m_quickSelectDigits);
    m_settings.setValue(QStringLiteral("ShowPopup"), m_shortcutPopup);
    m_settings.setValue(QStringLiteral("TogglePlainText"), m_shortcutTogglePlainText);
    m_settings.setValue(QStringLiteral("TogglePause"), m_shortcutTogglePause);
    m_settings.setValue(QStringLiteral("EditCurrent"), m_shortcutEditCurrent);
    m_settings.setValue(QStringLiteral("ShowQr"), m_shortcutShowQr);
    m_settings.setValue(QStringLiteral("RepeatAction"), m_shortcutRepeatAction);
    m_settings.setValue(QStringLiteral("CycleNext"), m_shortcutCycleNext);
    m_settings.setValue(QStringLiteral("CyclePrev"), m_shortcutCyclePrev);
    m_settings.setValue(QStringLiteral("ClearHistory"), m_shortcutClearHistory);
    m_settings.endGroup();

    m_settings.beginGroup(QStringLiteral("Updates"));
    m_settings.setValue(QStringLiteral("AutoCheck"), m_autoCheckUpdates);
    m_settings.setValue(QStringLiteral("SkippedVersion"), m_skippedUpdateVersion);
    m_settings.endGroup();

    m_settings.sync();
    enforcePrivateConfigPermissions(m_settings.fileName());
    Q_EMIT settingsChanged();
}

void MklipperSettings::setIgnoredApps(const QStringList &apps)
{
    QStringList cleaned;
    cleaned.reserve(apps.size());
    for (const QString &raw : apps) {
        const QString item = raw.trimmed();
        if (item.isEmpty()) {
            continue;
        }
        bool dup = false;
        for (const QString &existing : std::as_const(cleaned)) {
            if (existing.compare(item, Qt::CaseInsensitive) == 0) {
                dup = true;
                break;
            }
        }
        if (!dup) {
            cleaned.append(item);
        }
    }
    m_ignoredApps = std::move(cleaned);
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
