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

namespace
{
constexpr qsizetype kMaxCommandOutputChars = 10 * 1024 * 1024;

QString stripNulBytes(QString value)
{
    value.remove(QLatin1Char('\0'));
    return value;
}

QString escapeInsideSingleQuotes(const QString &value)
{
    QString escaped = stripNulBytes(value);
    escaped.replace(QLatin1Char('\''), QStringLiteral("'\\''"));
    return escaped;
}

QString escapeInsideDoubleQuotes(const QString &value)
{
    const QString clean = stripNulBytes(value);
    QString out;
    out.reserve(clean.size() + 16);
    for (const QChar ch : clean) {
        if (ch == QLatin1Char('\\') || ch == QLatin1Char('"')
            || ch == QLatin1Char('$') || ch == QLatin1Char('`')) {
            out.append(QLatin1Char('\\'));
        }
        out.append(ch);
    }
    return out;
}
} // namespace

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
    connect(this, &QProcess::errorOccurred, this, [this](QProcess::ProcessError err) {
        if (err == QProcess::FailedToStart) {
            deleteLater();
        }
    });
    if (command.output != ClipCommand::IGNORE) {
        connect(this, &QProcess::readyReadStandardOutput, this, &ClipCommandProcess::slotStdOutputAvailable);
    }
    if (command.output != ClipCommand::REPLACE) {
        m_historyItem.reset();
    }
}

QString ClipCommandProcess::shellQuote(const QString &value)
{
    return QLatin1Char('\'') + escapeInsideSingleQuotes(value) + QLatin1Char('\'');
}

QString ClipCommandProcess::expandMacros(const QString &command, const QHash<QChar, QString> &map)
{
    enum class QuoteState {
        None,
        Single,
        Double,
    };

    QuoteState state = QuoteState::None;
    bool escaped = false;
    QString out;
    out.reserve(command.size() + 32);

    for (int i = 0; i < command.size(); ++i) {
        const QChar ch = command[i];

        if (escaped) {
            out.append(ch);
            escaped = false;
            continue;
        }

        if (ch == QLatin1Char('\\') && state != QuoteState::Single) {
            out.append(ch);
            escaped = true;
            continue;
        }

        if (ch == QLatin1Char('\'') && state != QuoteState::Double) {
            state = (state == QuoteState::Single) ? QuoteState::None : QuoteState::Single;
            out.append(ch);
            continue;
        }

        if (ch == QLatin1Char('"') && state != QuoteState::Single) {
            state = (state == QuoteState::Double) ? QuoteState::None : QuoteState::Double;
            out.append(ch);
            continue;
        }

        if (ch == QLatin1Char('%') && i + 1 < command.size()) {
            const QChar next = command[i + 1];
            if (next == QLatin1Char('%')) {
                out.append(QLatin1Char('%'));
                ++i;
                continue;
            }
            auto it = map.constFind(next);
            if (it != map.constEnd()) {
                if (state == QuoteState::Single) {
                    out.append(escapeInsideSingleQuotes(it.value()));
                } else if (state == QuoteState::Double) {
                    out.append(escapeInsideDoubleQuotes(it.value()));
                } else {
                    out.append(shellQuote(it.value()));
                }
                ++i;
                continue;
            }
        }

        out.append(ch);
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
    const QByteArray chunk = readAllStandardOutput();
    if (m_newHistoryText.size() >= kMaxCommandOutputChars) {
        return;
    }
    const QString decoded = QString::fromLocal8Bit(chunk);
    const qsizetype remaining = kMaxCommandOutputChars - m_newHistoryText.size();
    m_newHistoryText.append(decoded.left(remaining));
}
