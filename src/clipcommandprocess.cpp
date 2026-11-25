/*
    SPDX-FileCopyrightText: 2009 Esben Mose Hansen <kde@mosehansen.dk>
    SPDX-FileCopyrightText: 2026 Mklipper contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "clipcommandprocess.h"

#include "historymodel.h"
#include "systemclipboard.h"
#include "urlgrabber.h"

#include <QHash>
#include <QMimeData>

ClipCommandProcess::ClipCommandProcess(const ClipAction &action,
                                       const ClipCommand &command,
                                       const QString &clip,
                                       HistoryItemConstPtr originalItem)
    : QProcess()
    , m_model(HistoryModel::self())
    , m_clip(SystemClipboard::self())
    , m_historyItem(std::move(originalItem))
{
    QHash<QChar, QString> map;
    map.insert(QLatin1Char('s'), clip);
    map.insert(QLatin1Char('u'), clip);
    map.insert(QLatin1Char('U'), clip);
    map.insert(QLatin1Char('f'), clip);
    map.insert(QLatin1Char('F'), clip);

    const QStringList matches = action.actionCapturedTexts();
    const int numMatches = qMin(10, matches.count());
    for (int i = 0; i < numMatches; ++i) {
        map.insert(QChar('0' + i), matches.at(i));
    }

    m_shellCommand = expandMacros(command.command, map).trimmed();

    connect(this, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, &ClipCommandProcess::slotFinished);
    if (command.output != ClipCommand::IGNORE) {
        connect(this, &QProcess::readyReadStandardOutput, this, &ClipCommandProcess::slotStdOutputAvailable);
    }
    if (command.output != ClipCommand::REPLACE) {
        m_historyItem.reset();
    }
}

QString ClipCommandProcess::shellQuote(const QString &value)
{
    QString quoted = value;
    quoted.replace(QLatin1Char('\''), QStringLiteral("'\\''"));
    return QLatin1Char('\'') + quoted + QLatin1Char('\'');
}

QString ClipCommandProcess::expandMacros(const QString &command, const QHash<QChar, QString> &map)
{
    QString out;
    out.reserve(command.size() + 32);
    for (int i = 0; i < command.size(); ++i) {
        if (command[i] == QLatin1Char('%') && i + 1 < command.size()) {
            const QChar next = command[i + 1];
            if (next == QLatin1Char('%')) {
                out.append(QLatin1Char('%'));
                ++i;
                continue;
            }
            auto it = map.constFind(next);
            if (it != map.constEnd()) {
                out.append(shellQuote(it.value()));
                ++i;
                continue;
            }
        }
        out.append(command[i]);
    }
    return out;
}

void ClipCommandProcess::launch()
{
    if (m_shellCommand.isEmpty()) {
        deleteLater();
        return;
    }
    start(QStringLiteral("/bin/sh"), {QStringLiteral("-c"), m_shellCommand});
}

void ClipCommandProcess::slotFinished(int /*exitCode*/, QProcess::ExitStatus /*exitStatus*/)
{
    if (!m_newHistoryText.isEmpty()) {
        auto data = std::make_unique<QMimeData>();
        data->setText(m_newHistoryText);
        m_clip->setMimeData(data.get(), SystemClipboard::SelectionMode(SystemClipboard::Selection | SystemClipboard::Clipboard));
        m_clip->checkClipData(QClipboard::Clipboard, data.get());
    }
    if (m_historyItem) {
        m_model->remove(m_historyItem->uuid(), false);
    }
    deleteLater();
}

void ClipCommandProcess::slotStdOutputAvailable()
{
    m_newHistoryText.append(QString::fromLocal8Bit(readAllStandardOutput()));
}
