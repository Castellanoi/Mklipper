/*
    SPDX-FileCopyrightText: 2000 Carsten Pfeiffer <pfeiffer@kde.org>
    SPDX-FileCopyrightText: 2026 Mklipper contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <memory>
#include <vector>

#include <QHash>
#include <QList>
#include <QMimeType>
#include <QObject>
#include <QRegularExpressionMatch>
#include <QSettings>
#include <QString>
#include <QStringList>

#include "historyitem.h"

class QAction;
class QMenu;
class QTimer;

struct ClipCommand {
    enum Output {
        IGNORE = 0,
        REPLACE = 1,
        ADD = 2,
    };

    ClipCommand(QString cmd = {},
                QString desc = {},
                bool enabled = true,
                QString iconName = {},
                Output out = IGNORE,
                bool openViaDesktop = false)
        : command(std::move(cmd))
        , description(std::move(desc))
        , isEnabled(enabled)
        , icon(std::move(iconName))
        , output(out)
        , openUrl(openViaDesktop)
    {
    }

    QString command;
    QString description;
    bool isEnabled = true;
    QString icon;
    Output output = IGNORE;
    bool openUrl = false;
};

class ClipAction
{
public:
    explicit ClipAction(QString regExp = {}, QString description = {}, bool automagic = true);
    ClipAction(QSettings &settings, const QString &groupName);
    ~ClipAction() = default;

    QString actionRegexPattern() const { return m_regexPattern; }
    void setActionRegexPattern(const QString &pattern) { m_regexPattern = pattern; }

    QRegularExpressionMatch match(const QString &text) const;

    QStringList actionCapturedTexts() const { return m_regexCapturedTexts; }
    void setActionCapturedTexts(const QStringList &captured) { m_regexCapturedTexts = captured; }

    QString description() const { return m_description; }
    void setDescription(const QString &desc) { m_description = desc; }

    bool automatic() const { return m_automatic; }
    void setAutomatic(bool automatic) { m_automatic = automatic; }

    void clearCommands() { m_commands.clear(); }
    void addCommand(const ClipCommand &cmd) { m_commands.append(cmd); }
    void replaceCommand(int idx, const ClipCommand &cmd);
    ClipCommand command(int idx) const { return m_commands.at(idx); }
    QList<ClipCommand> commands() const { return m_commands; }
    bool hasEnabledCommand() const;

    void save(QSettings &settings, const QString &groupName) const;

private:
    QString m_regexPattern;
    QStringList m_regexCapturedTexts;
    QString m_description;
    QList<ClipCommand> m_commands;
    bool m_automatic = true;
};

using ActionList = QList<ClipAction *>;

class URLGrabber : public QObject
{
    Q_OBJECT

public:
    explicit URLGrabber(QObject *parent = nullptr);
    ~URLGrabber() override;

    void checkNewData(HistoryItemConstPtr item);
    void invokeAction(HistoryItemConstPtr item);
    bool hasMatchingActions(const HistoryItemConstPtr &item) const;

    ActionList actionList() const { return m_actions; }
    void setActionList(const ActionList &list);

    void loadSettings();
    void saveSettings() const;

Q_SIGNALS:
    void sigPopup(QMenu *menu);

private Q_SLOTS:
    void slotItemSelected(QAction *action);
    void slotKillPopupMenu();

private:
    void actionMenu(HistoryItemConstPtr item, bool automaticallyInvoked);
    QList<ClipAction *> matchingActions(const QString &clipData, bool automaticallyInvoked, std::vector<std::unique_ptr<ClipAction>> &ephemeral);
    void execute(const ClipAction *action, int commandIdx) const;
    QMimeType urlMimeType(const QString &clipData) const;
    QString actionText(const HistoryItemConstPtr &item) const;

    ActionList m_actions;
    std::vector<std::unique_ptr<ClipAction>> m_ephemeralMatches;
    HistoryItemConstPtr m_clipItem;
    QHash<QString, QPair<ClipAction *, int>> m_commandMapper;
    std::unique_ptr<QMenu> m_menu;
    QTimer *m_popupKillTimer = nullptr;
    int m_popupKillTimeout = 8;
    bool m_stripWhiteSpace = true;
};
