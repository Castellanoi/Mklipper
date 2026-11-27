/*
    SPDX-FileCopyrightText: 2009 Esben Mose Hansen <kde@mosehansen.dk>
    SPDX-FileCopyrightText: 2026 Mklipper contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <memory>

#include <QByteArray>
#include <QHash>
#include <QProcess>
#include <QString>

#include "historyitem.h"

class ClipAction;
struct ClipCommand;
class HistoryModel;
class SystemClipboard;

class ClipCommandProcess : public QProcess
{
    Q_OBJECT

public:
    ClipCommandProcess(const ClipAction &action,
                       const ClipCommand &command,
                       const QString &clip,
                       HistoryItemConstPtr originalItem = nullptr);

    void launch();

    static QString shellQuote(const QString &value);
    static QString expandMacros(const QString &command, const QHash<QChar, QString> &map);

private Q_SLOTS:
    void slotFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void slotStdOutputAvailable();

private:
    std::shared_ptr<HistoryModel> m_model;
    std::shared_ptr<SystemClipboard> m_clip;
    HistoryItemConstPtr m_historyItem;
    QByteArray m_rawOutput;
    QString m_shellCommand;
};
