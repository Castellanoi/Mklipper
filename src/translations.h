/*
    SPDX-FileCopyrightText: 2026 Mklipper contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QList>
#include <QString>

struct LanguageInfo {
    QString code;
    QString nativeName;
    bool isSource = false;
};

struct ManualCard {
    QString icon;
    QString title;
    QString body;
};

struct ManualSection {
    QString icon;
    QString title;
    QString intro;
    QList<ManualCard> cards;
};

namespace MklipperTranslations
{
// Discovers all installed JSON language packs (embedded :/i18n/*.json + optional system/user i18n dirs)
QList<LanguageInfo> availableLanguages();

// Resolves "system" or a configured language code against available language packs
QString resolveLanguage(const QString &configured);

// Returns currently applied concrete language code (e.g. "es", "en", "pt_br")
QString activeLanguage();

// Applies the language to QCoreApplication; returns true if the concrete language changed
bool applyLanguage(const QString &configured);

// Returns structured, friendly User Manual tabs & cards for the active language (falling back to English / source)
QList<ManualSection> userManualSections();
} // namespace MklipperTranslations
