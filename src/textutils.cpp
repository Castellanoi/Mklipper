/*
    SPDX-FileCopyrightText: 2026 Mklipper contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "textutils.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QLocale>
#include <QRegularExpression>
#include <QStringList>
#include <QUrl>

using namespace Qt::StringLiterals;

namespace MklipperText
{
QString foldSearchText(const QString &input)
{
    if (input.isEmpty()) {
        return {};
    }
    const QString decomposed = input.normalized(QString::NormalizationForm_D);
    QString out;
    out.reserve(decomposed.size());
    for (const QChar ch : decomposed) {
        const auto cat = ch.category();
        if (cat == QChar::Mark_NonSpacing || cat == QChar::Mark_SpacingCombining) {
            continue;
        }
        out.append(ch.toLower());
    }
    return out;
}

QStringList splitSearchTokens(const QString &foldedQuery)
{
    QStringList tokens;
    qsizetype start = -1;
    const qsizetype len = foldedQuery.size();
    for (qsizetype i = 0; i < len; ++i) {
        if (foldedQuery.at(i).isSpace()) {
            if (start >= 0) {
                tokens.append(foldedQuery.mid(start, i - start));
                start = -1;
            }
        } else if (start < 0) {
            start = i;
        }
    }
    if (start >= 0) {
        tokens.append(foldedQuery.mid(start));
    }
    return tokens;
}

bool matchesFoldedTokens(const QString &haystack, const QStringList &foldedTokens)
{
    if (foldedTokens.isEmpty()) {
        return true;
    }
    if (haystack.isEmpty()) {
        return false;
    }
    const QString foldedHaystack = foldSearchText(haystack);
    for (const QString &token : foldedTokens) {
        if (!foldedHaystack.contains(token)) {
            return false;
        }
    }
    return true;
}

bool matchesFoldedQuery(const QString &haystack, const QString &foldedQuery)
{
    return matchesFoldedTokens(haystack, splitSearchTokens(foldedQuery));
}

bool containsUrl(const QString &text)
{
    if (text.size() < 7) {
        return false;
    }
    static const QRegularExpression kUrlRe(
        QStringLiteral(R"(\b(?:https?|ftp)://[^\s<>"]+)"),
        QRegularExpression::CaseInsensitiveOption);
    return kUrlRe.match(text).hasMatch();
}

bool isExactUrl(const QString &text)
{
    const QString trimmed = text.trimmed();
    if (trimmed.size() < 8) {
        return false;
    }
    static const QRegularExpression kExactUrlRe(
        QStringLiteral(R"(^(?:https?|ftp)://[^\s<>"]+$)"),
        QRegularExpression::CaseInsensitiveOption);
    if (!kExactUrlRe.match(trimmed).hasMatch()) {
        return false;
    }
    const QUrl url(trimmed, QUrl::StrictMode);
    if (!url.isValid() || url.host().isEmpty()) {
        // Fallback to tolerant mode for URLs with unencoded unicode/brackets while still requiring host & scheme
        const QUrl tolerant(trimmed);
        return tolerant.isValid() && !tolerant.host().isEmpty();
    }
    return true;
}

QString formatRelativeTime(qreal epochSeconds, qint64 nowMs)
{
    if (epochSeconds <= 0.0) {
        return {};
    }
    const qint64 thenMs = static_cast<qint64>(epochSeconds * 1000.0);
    const qint64 currentMs = (nowMs >= 0) ? nowMs : QDateTime::currentMSecsSinceEpoch();
    const qint64 diffSec = (currentMs - thenMs) / 1000;

    if (diffSec < 60) {
        return QCoreApplication::translate("MklipperText", "Justo ahora");
    }
    if (diffSec < 3600) {
        const qint64 mins = diffSec / 60;
        return QCoreApplication::translate("MklipperText", "Hace %1 min").arg(mins);
    }
    if (diffSec < 86400) {
        const qint64 hours = diffSec / 3600;
        return QCoreApplication::translate("MklipperText", "Hace %1 h").arg(hours);
    }

    const QDateTime thenDt = QDateTime::fromMSecsSinceEpoch(thenMs);
    const QDateTime nowDt = QDateTime::fromMSecsSinceEpoch(currentMs);
    if (thenDt.date().daysTo(nowDt.date()) == 1) {
        return QCoreApplication::translate("MklipperText", "Ayer, %1").arg(thenDt.toString(u"HH:mm"_s));
    }
    return QLocale().toString(thenDt, QLocale::ShortFormat);
}
} // namespace MklipperText
