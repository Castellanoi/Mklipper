/*
    SPDX-FileCopyrightText: 2026 Mklipper contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QColor>
#include <QIcon>
#include <QPixmap>
#include <QRectF>

class QPainter;

enum class FlatIcon {
    Search,
    StarOutline,
    StarFilled,
    Trash,
    Close,
    Settings,
    QrCode,
    Edit,
    ExternalLink,
    Bolt,
    Lock,
    Clipboard,
    Back,
    Copy,
    Save,
    Check,
    Refresh,
    Update,
    Plus,
    ClearKey,
    Link,
    Exit,
    Info,
    Sliders,
    Keyboard,
    History,
    Book,
    Pause,
    Play,
    Eye,
    Image,
    FileText,
    ChevronDown,
    ChevronUp,
};

namespace FlatIcons
{
QPixmap pixmap(FlatIcon id, const QColor &color, int logicalSize, qreal dpr = 1.0);
void paint(QPainter *painter, FlatIcon id, const QRectF &rect, const QColor &color);

QIcon icon(FlatIcon id,
           const QColor &normalColor,
           const QColor &activeColor = {},
           const QColor &disabledColor = {},
           int logicalSize = 16);

QIcon toggleIcon(FlatIcon offId,
                 const QColor &offColor,
                 FlatIcon onId,
                 const QColor &onColor,
                 const QColor &offHoverColor = {},
                 int logicalSize = 16);

QPixmap appLogoPixmap(const QColor &accentColor, int logicalSize, qreal dpr = 1.0, bool paused = false, int badgeCount = 0);
QIcon appLogoIcon(const QColor &accentColor, bool paused = false, int badgeCount = 0);
} // namespace FlatIcons
