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

bool matchesFoldedQuery(const QString &haystack, const QString &foldedQuery)
{
    const QString trimmedQuery = foldedQuery.trimmed();
    if (trimmedQuery.isEmpty()) {
        return true;
    }
    if (haystack.isEmpty()) {
        return false;
    }
    const QString foldedHaystack = foldSearchText(haystack);
    const QStringList tokens = trimmedQuery.split(QRegularExpression(u"\\s+"_s), Qt::SkipEmptyParts);
    for (const QString &token : tokens) {
        if (!foldedHaystack.contains(token)) {
            return false;
        }
    }
    return true;
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
