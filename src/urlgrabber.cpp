/*
    SPDX-FileCopyrightText: 2000, 2001, 2002 Carsten Pfeiffer <pfeiffer@kde.org>
    SPDX-FileCopyrightText: 2026 Mklipper contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "urlgrabber.h"

#include "clipcommandprocess.h"
#include "flaticons.h"
#include "settings.h"
#include "themestyle.h"

#include <QAction>
#include <QApplication>
#include <QDesktopServices>
#include <QFile>
#include <QIcon>
#include <QMenu>
#include <QMimeDatabase>
#include <QRegularExpression>
#include <QTimer>
#include <QUrl>
#include <QUuid>

using namespace Qt::StringLiterals;

ClipAction::ClipAction(QString regExp, QString description, bool automagic)
    : m_regexPattern(std::move(regExp))
    , m_compiledRegex(m_regexPattern)
    , m_description(std::move(description))
    , m_automatic(automagic)
{
}

ClipAction::ClipAction(QSettings &settings, const QString &groupName)
{
    settings.beginGroup(groupName);
    m_regexPattern = settings.value(u"Regexp"_s).toString();
    m_compiledRegex = QRegularExpression(m_regexPattern);
    m_description = settings.value(u"Description"_s).toString();
    m_automatic = settings.value(u"Automatic"_s, true).toBool();
    const int numCommands = qBound(0, settings.value(u"Number of commands"_s, 0).toInt(), 64);
    settings.endGroup();

    for (int i = 0; i < numCommands; ++i) {
        const QString cmdGroup = groupName + u"/Command_"_s + QString::number(i);
        settings.beginGroup(cmdGroup);
        ClipCommand cmd;
        cmd.command = settings.value(u"Commandline"_s).toString();
        cmd.description = settings.value(u"Description"_s).toString();
        cmd.isEnabled = settings.value(u"Enabled"_s, true).toBool();
        cmd.icon = settings.value(u"Icon"_s).toString();
        const int rawOut = qBound(static_cast<int>(ClipCommand::IGNORE),
                                  settings.value(u"Output"_s, 0).toInt(),
                                  static_cast<int>(ClipCommand::ADD));
        cmd.output = static_cast<ClipCommand::Output>(rawOut);
        settings.endGroup();
        m_commands.append(cmd);
    }
}

QRegularExpressionMatch ClipAction::match(const QString &text) const
{
    constexpr qsizetype kMaxRegexInputChars = 65536;
    if (m_regexPattern.isEmpty() || text.size() > kMaxRegexInputChars || !m_compiledRegex.isValid()) {
        return {};
    }
    return m_compiledRegex.match(text);
}

void ClipAction::replaceCommand(int idx, const ClipCommand &cmd)
{
    if (idx >= 0 && idx < m_commands.size()) {
        m_commands[idx] = cmd;
    }
}

bool ClipAction::hasEnabledCommand() const
{
    for (const auto &cmd : m_commands) {
        if (cmd.isEnabled) {
            return true;
        }
    }
    return false;
}

void ClipAction::save(QSettings &settings, const QString &groupName) const
{
    settings.beginGroup(groupName);
    settings.setValue(u"Regexp"_s, m_regexPattern);
    settings.setValue(u"Description"_s, m_description);
    settings.setValue(u"Automatic"_s, m_automatic);
    settings.setValue(u"Number of commands"_s, m_commands.size());
    settings.endGroup();

    for (int i = 0; i < m_commands.size(); ++i) {
        const auto &cmd = m_commands.at(i);
        const QString cmdGroup = groupName + u"/Command_"_s + QString::number(i);
        settings.beginGroup(cmdGroup);
        settings.setValue(u"Commandline"_s, cmd.command);
        settings.setValue(u"Description"_s, cmd.description);
        settings.setValue(u"Enabled"_s, cmd.isEnabled);
        settings.setValue(u"Icon"_s, cmd.icon);
        settings.setValue(u"Output"_s, static_cast<int>(cmd.output));
        settings.endGroup();
    }
}

URLGrabber::URLGrabber(QObject *parent)
    : QObject(parent)
    , m_popupKillTimer(new QTimer(this))
{
    m_popupKillTimer->setSingleShot(true);
    connect(m_popupKillTimer, &QTimer::timeout, this, &URLGrabber::slotKillPopupMenu);
}

URLGrabber::~URLGrabber()
{
    qDeleteAll(m_actions);
    m_actions.clear();
}

void URLGrabber::checkNewData(HistoryItemConstPtr item)
{
    actionMenu(std::move(item), true);
}

void URLGrabber::invokeAction(HistoryItemConstPtr item)
{
    actionMenu(std::move(item), false);
}

QString URLGrabber::actionText(const HistoryItemConstPtr &item) const
{
    if (!item) {
        return {};
    }
    QString text = item->text();
    if (m_stripWhiteSpace) {
        text = text.trimmed();
    }
    return text;
}

QMimeType URLGrabber::urlMimeType(const QString &clipData) const
{
    constexpr qsizetype kMaxUrlMimeCheckChars = 4096;
    if (!MklipperSettings::self()->enableMagicMimeActions()
        || clipData.isEmpty()
        || clipData.size() > kMaxUrlMimeCheckChars
        || clipData.contains(QLatin1Char('\n'))) {
        return {};
    }

    if (clipData.startsWith(u"http://"_s, Qt::CaseInsensitive)
        || clipData.startsWith(u"https://"_s, Qt::CaseInsensitive)) {
        if (clipData.contains(QLatin1Char(' ')) || clipData.contains(QLatin1Char('\t'))) {
            return {};
        }
        const QUrl httpUrl(clipData);
        if (!httpUrl.isValid() || httpUrl.host().isEmpty()) {
            return {};
        }
        QMimeDatabase db;
        return db.mimeTypeForName(u"text/html"_s);
    }

    QUrl rawUrl(clipData);
    if (!rawUrl.isValid() || rawUrl.isRelative()) {
        if (clipData.startsWith(u'/') && QFile::exists(clipData)) {
            QMimeDatabase db;
            return db.mimeTypeForFile(clipData);
        }
        return {};
    }

    if (rawUrl.isLocalFile()) {
        if (clipData == u"//"_s || !QFile::exists(rawUrl.toLocalFile())) {
            return {};
        }
        QMimeDatabase db;
        return db.mimeTypeForFile(rawUrl.toLocalFile());
    }

    const QString scheme = rawUrl.scheme().toLower();
    if (scheme == u"mailto"_s || scheme == u"ftp"_s) {
        if (clipData.contains(QLatin1Char(' ')) || clipData.contains(QLatin1Char('\t'))) {
            return {};
        }
        if ((scheme == u"mailto"_s && rawUrl.path().isEmpty())
            || (scheme == u"ftp"_s && rawUrl.host().isEmpty())) {
            return {};
        }
        QMimeDatabase db;
        QMimeType mt = db.mimeTypeForName(u"text/uri-list"_s);
        if (!mt.isValid()) {
            mt = db.mimeTypeForName(u"text/plain"_s);
        }
        return mt;
    }

    return {};
}

bool URLGrabber::hasMatchingActions(const HistoryItemConstPtr &item) const
{
    if (!item) {
        return false;
    }
    const QString text = actionText(item);
    if (urlMimeType(text).isValid()) {
        return true;
    }
    for (const ClipAction *action : m_actions) {
        if (action->hasEnabledCommand() && action->match(text).hasMatch()) {
            return true;
        }
    }
    return false;
}

void URLGrabber::setActionList(const ActionList &list)
{
    m_popupKillTimer->stop();
    m_commandMapper.clear();
    m_ephemeralMatches.clear();
    m_clipItem.reset();
    m_menu.reset();
    qDeleteAll(m_actions);
    m_actions = list;
}

QList<ClipAction *> URLGrabber::matchingActions(const QString &clipData,
                                                bool automaticallyInvoked,
                                                std::vector<std::unique_ptr<ClipAction>> &ephemeral)
{
    QList<ClipAction *> matches;
    for (ClipAction *action : std::as_const(m_actions)) {
        const QRegularExpressionMatch m = action->match(clipData);
        if (m.hasMatch() && action->hasEnabledCommand() && (action->automatic() || !automaticallyInvoked)) {
            action->setActionCapturedTexts(m.capturedTexts());
            matches.append(action);
        }
    }

    const QMimeType mime = urlMimeType(clipData);
    if (mime.isValid()) {
        auto act = std::make_unique<ClipAction>(QString(), mime.comment().isEmpty() ? tr("Abrir enlace / archivo") : mime.comment());
        act->addCommand(ClipCommand(QString(), tr("Abrir con la aplicación predeterminada"), true, u"system-run"_s, ClipCommand::IGNORE, true));
        matches.append(act.get());
        ephemeral.push_back(std::move(act));
    }

    return matches;
}

void URLGrabber::actionMenu(HistoryItemConstPtr item, bool automaticallyInvoked)
{
    if (!item) {
        return;
    }

    const QString text = actionText(item);
    m_ephemeralMatches.clear();
    const QList<ClipAction *> matches = matchingActions(text, automaticallyInvoked, m_ephemeralMatches);
    if (matches.isEmpty()) {
        return;
    }

    m_commandMapper.clear();
    m_popupKillTimer->stop();

    m_menu = std::make_unique<QMenu>();
    m_menu->setObjectName(u"mklipperActionPopup"_s);
    connect(m_menu.get(), &QMenu::triggered, this, &URLGrabber::slotItemSelected);

    const ThemeTokens t = ThemeTokens::fromPalette(QApplication::palette());

    for (ClipAction *clipAct : matches) {
        m_menu->addSection(FlatIcons::icon(FlatIcon::Clipboard, t.textSecondary, t.accentFg, t.textMuted, 15),
                           clipAct->description());
        const auto cmds = clipAct->commands();
        for (int i = 0; i < cmds.size(); ++i) {
            const ClipCommand &cmd = cmds.at(i);
            if (!cmd.isEnabled) {
                continue;
            }
            const QString id = QUuid::createUuid().toString();
            auto *act = new QAction(cmd.description.isEmpty() ? cmd.command : cmd.description, m_menu.get());
            act->setData(id);
            const FlatIcon cmdIcon = (cmd.icon.contains(u"browser"_s) || cmd.icon.contains(u"open"_s))
                ? FlatIcon::ExternalLink
                : FlatIcon::Bolt;
            act->setIcon(FlatIcons::icon(cmdIcon, t.textPrimary, t.accentFg, t.textMuted, 15));
            m_commandMapper.insert(id, {clipAct, i});
            m_menu->addAction(act);
        }
    }

    m_menu->addSeparator();
    auto *cancelAct = new QAction(
        FlatIcons::icon(FlatIcon::Close, t.textSecondary, t.accentFg, t.textMuted, 15),
        tr("Cancelar"),
        m_menu.get());
    m_menu->addAction(cancelAct);
    m_clipItem = std::move(item);

    if (m_popupKillTimeout > 0) {
        m_popupKillTimer->start(1000 * m_popupKillTimeout);
    }

    Q_EMIT sigPopup(m_menu.get());
}

void URLGrabber::slotItemSelected(QAction *action)
{
    if (!action) {
        return;
    }
    const QString id = action->data().toString();
    if (id.isEmpty()) {
        return;
    }
    const auto pair = m_commandMapper.value(id);
    if (pair.first) {
        execute(pair.first, pair.second);
    }
}

void URLGrabber::execute(const ClipAction *action, int commandIdx) const
{
    if (!action || !m_clipItem || commandIdx < 0 || commandIdx >= action->commands().size()) {
        return;
    }
    const ClipCommand cmd = action->command(commandIdx);
    if (!cmd.isEnabled) {
        return;
    }
    const QString text = actionText(m_clipItem);
    if (cmd.openUrl) {
        QUrl url(text);
        if (url.isRelative() && text.startsWith(u'/')) {
            url = QUrl::fromLocalFile(text);
        }
        const QString scheme = url.scheme().toLower();
        const bool allowedScheme = (scheme == u"http"_s || scheme == u"https"_s
                                    || scheme == u"mailto"_s || scheme == u"ftp"_s || scheme == u"file"_s);
        if (url.isValid() && allowedScheme) {
            QDesktopServices::openUrl(url);
        }
        return;
    }

    auto *proc = new ClipCommandProcess(*action, cmd, text, m_clipItem);
    proc->launch();
}

void URLGrabber::slotKillPopupMenu()
{
    if (m_menu) {
        m_menu->close();
    }
}

void URLGrabber::loadSettings()
{
    auto *s = MklipperSettings::self();
    m_stripWhiteSpace = s->stripWhiteSpace();
    m_popupKillTimeout = s->timeoutForActionPopups();

    m_popupKillTimer->stop();
    m_commandMapper.clear();
    m_ephemeralMatches.clear();
    m_clipItem.reset();
    m_menu.reset();
    qDeleteAll(m_actions);
    m_actions.clear();

    QSettings &raw = s->rawSettings();
    raw.beginGroup(u"CustomActions"_s);
    const int num = qBound(0, raw.value(u"Count"_s, 0).toInt(), 256);
    raw.endGroup();

    for (int i = 0; i < num; ++i) {
        const QString group = u"CustomActions/Action_"_s + QString::number(i);
        m_actions.append(new ClipAction(raw, group));
    }
}

void URLGrabber::saveSettings() const
{
    QSettings &raw = MklipperSettings::self()->rawSettings();
    raw.remove(u"CustomActions"_s);
    raw.beginGroup(u"CustomActions"_s);
    raw.setValue(u"Count"_s, m_actions.size());
    raw.endGroup();

    for (int i = 0; i < m_actions.size(); ++i) {
        const QString group = u"CustomActions/Action_"_s + QString::number(i);
        m_actions.at(i)->save(raw, group);
    }
    raw.sync();
}
