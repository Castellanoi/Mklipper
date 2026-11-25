/*
    SPDX-FileCopyrightText: 2026 Noah Davis <noahadvs@gmail.com>
    SPDX-FileCopyrightText: 2026 Mklipper contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QLatin1StringView>
#include <QString>

namespace MimeTypes
{
using namespace Qt::StringLiterals;

inline constexpr auto imagePrefix = "image/"_L1;
inline constexpr auto textPrefix = "text/"_L1;

inline constexpr auto png = "image/png"_L1;
inline constexpr auto jpeg = "image/jpeg"_L1;
inline constexpr auto qtImage = "application/x-qt-image"_L1;

inline constexpr auto uriList = "text/uri-list"_L1;
inline constexpr auto plainText = "text/plain"_L1;
inline constexpr auto plainUtf8Text = "text/plain;charset=utf-8"_L1;
inline constexpr auto html = "text/html"_L1;
inline constexpr auto json = "application/json"_L1;
inline constexpr auto xml = "application/xml"_L1;

inline constexpr auto kdeSuggestedFileName = "application/x-kde-suggestedfilename"_L1;
inline constexpr auto kdeForceImageCopy = "x-kde-force-image-copy"_L1;

inline bool isImage(const QString &mimetype)
{
    return mimetype.startsWith(imagePrefix) || mimetype == qtImage;
}

inline bool isPlainText(const QString &mimetype)
{
    return mimetype == plainText || mimetype == plainUtf8Text;
}
}
