/*
    SPDX-FileCopyrightText: 2026 Mklipper contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QColor>
#include <QPalette>
#include <QString>

struct ThemeTokens {
    bool isDark = false;

    QColor windowBg;
    QColor surfaceBg;
    QColor surfaceHoverBg;
    QColor surfaceActiveBg;
    QColor inputBg;

    QColor hairline;
    QColor borderSubtle;

    QColor textPrimary;
    QColor textSecondary;
    QColor textMuted;

    QColor accent;
    QColor accentFg;
    QColor accentHover;
    QColor accentPressed;
    QColor accentSoft;
    QColor accentSoftStrong;

    QColor danger;
    QColor dangerSoft;

    QColor warning;
    QColor warningSoft;

    static ThemeTokens fromPalette(const QPalette &pal);
};

namespace ThemeStyle
{
void syncCursorEnvironment();
bool syncApplicationPalette();
QString rgbaCss(const QColor &c);
QString popupStyleSheet(const ThemeTokens &t);
QString dialogStyleSheet(const ThemeTokens &t);
} // namespace ThemeStyle
