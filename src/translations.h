/*
    SPDX-FileCopyrightText: 2026 Mklipper contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QString>

namespace MklipperTranslations
{
// Resolves "system" / "es" / "en" to concrete "es" or "en"
QString resolveLanguage(const QString &configured);

// Returns currently applied concrete language ("es" or "en")
QString activeLanguage();

// Applies the language to QCoreApplication; returns true if the concrete language changed
bool applyLanguage(const QString &configured);
} // namespace MklipperTranslations
