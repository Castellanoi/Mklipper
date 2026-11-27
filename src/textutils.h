/*
    SPDX-FileCopyrightText: 2026 Mklipper contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QString>
#include <QStringList>

namespace MklipperText
{
// Strips diacritics/accents (Unicode NFD) and lowercases text for accent-insensitive matching
QString foldSearchText(const QString &input);

// Splits a pre-folded search query into non-empty whitespace-delimited tokens
QStringList splitSearchTokens(const QString &foldedQuery);

// Multi-token accent- and case-insensitive match (all whitespace-separated tokens in foldedQuery must appear)
bool matchesFoldedQuery(const QString &haystack, const QString &foldedQuery);

// Fast match against pre-split folded tokens (avoids re-splitting per row)
bool matchesFoldedTokens(const QString &haystack, const QStringList &foldedTokens);

// Returns true if text contains an http://, https://, or ftp:// URL
bool containsUrl(const QString &text);

// Returns true if the entire (trimmed) text is a single valid http://, https://, or ftp:// URL
bool isExactUrl(const QString &text);

// Formats a Unix epoch timestamp (seconds) as a localized relative time string.
// Pass nowMs >= 0 for deterministic unit testing; defaults to current wall-clock time.
QString formatRelativeTime(qreal epochSeconds, qint64 nowMs = -1);
} // namespace MklipperText
