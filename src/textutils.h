/*
    SPDX-FileCopyrightText: 2026 Mklipper contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QString>

namespace MklipperText
{
// Strips diacritics/accents (Unicode NFD) and lowercases text for accent-insensitive matching
QString foldSearchText(const QString &input);

// Multi-token accent- and case-insensitive match (all whitespace-separated tokens in foldedQuery must appear)
bool matchesFoldedQuery(const QString &haystack, const QString &foldedQuery);

// Returns true if text contains an http://, https://, or ftp:// URL
bool containsUrl(const QString &text);

// Formats a Unix epoch timestamp (seconds) as a localized relative time string.
// Pass nowMs >= 0 for deterministic unit testing; defaults to current wall-clock time.
QString formatRelativeTime(qreal epochSeconds, qint64 nowMs = -1);
} // namespace MklipperText
