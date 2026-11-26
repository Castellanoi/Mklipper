/*
    SPDX-FileCopyrightText: 2026 Mklipper contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "flaticons.h"

#include <QByteArray>
#include <QCache>
#include <QHash>
#include <QPainter>
#include <QSvgRenderer>
#include <QtMath>

namespace
{
// Minimalist flat vector icons (24x24 grid, 1.8px rounded strokes, Cinnamon 6.6 / Adwaita symbolic style)
const char *svgBodyFor(FlatIcon id)
{
    switch (id) {
    case FlatIcon::Search:
        return "<circle cx='11' cy='11' r='6.5'/>"
               "<path d='m16 16 4.5 4.5'/>";

    case FlatIcon::StarOutline:
        return "<path d='M12 3.2 14.7 8.7l6.1.9-4.4 4.3 1 6.1L12 17.1 6.6 20l1-6.1-4.4-4.3 6.1-.9Z'/>";

    case FlatIcon::StarFilled:
        return "<path fill='COLOR' stroke='COLOR' stroke-width='1.2' "
               "d='M12 3.2 14.7 8.7l6.1.9-4.4 4.3 1 6.1L12 17.1 6.6 20l1-6.1-4.4-4.3 6.1-.9Z'/>";

    case FlatIcon::Trash:
        return "<path d='M4 6.5h16'/>"
               "<path d='M9.5 6.5V4.5a1.5 1.5 0 0 1 1.5-1.5h2a1.5 1.5 0 0 1 1.5 1.5v2'/>"
               "<path d='M6.5 6.5 7.4 19a2 2 0 0 0 2 1.8h5.2a2 2 0 0 0 2-1.8l.9-12.5'/>"
               "<path d='M10 11v5.5'/>"
               "<path d='M14 11v5.5'/>";

    case FlatIcon::Close:
        return "<path d='M6 6 18 18'/>"
               "<path d='M18 6 6 18'/>";

    case FlatIcon::Settings:
        return "<circle cx='12' cy='12' r='3'/>"
               "<path d='M19.4 15a1.65 1.65 0 0 0 .33 1.82l.06.06a2 2 0 1 1-2.83 2.83l-.06-.06a1.65 1.65 0 0 0-1.82-.33 "
               "1.65 1.65 0 0 0-1 1.51V21a2 2 0 1 1-4 0v-.09A1.65 1.65 0 0 0 9 19.4a1.65 1.65 0 0 0-1.82.33l-.06.06"
               "a2 2 0 1 1-2.83-2.83l.06-.06a1.65 1.65 0 0 0 .33-1.82 1.65 1.65 0 0 0-1.51-1H3a2 2 0 1 1 0-4h.09"
               "A1.65 1.65 0 0 0 4.6 9a1.65 1.65 0 0 0-.33-1.82l-.06-.06a2 2 0 1 1 2.83-2.83l.06.06a1.65 1.65 0 0 0 1.82.33H9"
               "a1.65 1.65 0 0 0 1-1.51V3a2 2 0 1 1 4 0v.09a1.65 1.65 0 0 0 1 1.51 1.65 1.65 0 0 0 1.82-.33l.06-.06"
               "a2 2 0 1 1 2.83 2.83l-.06.06a1.65 1.65 0 0 0-.33 1.82V9a1.65 1.65 0 0 0 1.51 1H21a2 2 0 1 1 0 4h-.09"
               "a1.65 1.65 0 0 0-1.51 1Z'/>";

    case FlatIcon::QrCode:
        return "<rect x='3.5' y='3.5' width='6.5' height='6.5' rx='1.2'/>"
               "<rect x='14' y='3.5' width='6.5' height='6.5' rx='1.2'/>"
               "<rect x='3.5' y='14' width='6.5' height='6.5' rx='1.2'/>"
               "<rect x='6' y='6' width='1.5' height='1.5' fill='COLOR' stroke='none'/>"
               "<rect x='16.5' y='6' width='1.5' height='1.5' fill='COLOR' stroke='none'/>"
               "<rect x='6' y='16.5' width='1.5' height='1.5' fill='COLOR' stroke='none'/>"
               "<path d='M14 14h2.5v2.5H14z' fill='COLOR' stroke='none'/>"
               "<path d='M18.2 14h2.3v2.3h-2.3z' fill='COLOR' stroke='none'/>"
               "<path d='M14 18.2h2.3v2.3H14z' fill='COLOR' stroke='none'/>"
               "<path d='M17.5 17.5h3v3h-3z' fill='COLOR' stroke='none'/>";

    case FlatIcon::Edit:
        return "<path d='M16.5 3.8a2.1 2.1 0 0 1 3 3L8.8 17.5 4.2 19l1.5-4.6Z'/>"
               "<path d='m14.2 6.1 3 3'/>";

    case FlatIcon::ExternalLink:
        return "<path d='M14 4h6v6'/>"
               "<path d='M10.5 13.5 20 4'/>"
               "<path d='M18 13.5v4.5a2 2 0 0 1-2 2H6a2 2 0 0 1-2-2V8a2 2 0 0 1 2-2h4.5'/>";

    case FlatIcon::Bolt:
        return "<path d='M13.2 2.8 4.8 13.2h6.3l-1.3 8 8.4-10.4h-6.3Z'/>";

    case FlatIcon::Lock:
        return "<rect x='5' y='11' width='14' height='9.5' rx='2.2'/>"
               "<path d='M8 11V7.8a4 4 0 0 1 8 0V11'/>";

    case FlatIcon::Clipboard:
        return "<rect x='8.5' y='3' width='7' height='3.5' rx='1.2'/>"
               "<path d='M8.5 4.8H6.5a2 2 0 0 0-2 2v12.4a2 2 0 0 0 2 2h11a2 2 0 0 0 2-2V6.8a2 2 0 0 0-2-2h-2'/>"
               "<path d='M8.5 11h7'/>"
               "<path d='M8.5 15h4.8'/>";

    case FlatIcon::Back:
        return "<path d='m15 5-7 7 7 7'/>";

    case FlatIcon::Copy:
        return "<rect x='9' y='9' width='11' height='11' rx='2'/>"
               "<path d='M5 15H4.8A1.8 1.8 0 0 1 3 13.2V4.8A1.8 1.8 0 0 1 4.8 3h8.4A1.8 1.8 0 0 1 15 4.8V5'/>";

    case FlatIcon::Save:
        return "<path d='M12 3.5v11.5'/>"
               "<path d='m7.5 10.8 4.5 4.5 4.5-4.5'/>"
               "<path d='M4.5 17.5v1.3A1.7 1.7 0 0 0 6.2 20.5h11.6a1.7 1.7 0 0 0 1.7-1.7v-1.3'/>";

    case FlatIcon::Check:
        return "<path d='m5 12.5 4.8 4.8L19.5 7.5'/>";

    case FlatIcon::Refresh:
        return "<path d='M20 12a8 8 0 1 1-2.3-5.6L20 8.5'/>"
               "<path d='M20 4v4.5h-4.5'/>";

    case FlatIcon::Update:
        return "<circle cx='12' cy='12' r='8.5'/>"
               "<path d='M12 8v7'/>"
               "<path d='m8.8 12.2 3.2 3.2 3.2-3.2'/>";

    case FlatIcon::Plus:
        return "<path d='M12 5v14'/>"
               "<path d='M5 12h14'/>";

    case FlatIcon::ClearKey:
        return "<path d='M20 6H9.5L4 12l5.5 6H20a1.5 1.5 0 0 0 1.5-1.5v-9A1.5 1.5 0 0 0 20 6Z'/>"
               "<path d='m12.5 9.8 4.4 4.4'/>"
               "<path d='m16.9 9.8-4.4 4.4'/>";

    case FlatIcon::Link:
        return "<path d='M10 14a4.5 4.5 0 0 0 6.4 0l2.4-2.4a4.5 4.5 0 0 0-6.4-6.4L11 6.6'/>"
               "<path d='M14 10a4.5 4.5 0 0 0-6.4 0l-2.4 2.4a4.5 4.5 0 0 0 6.4 6.4l1.4-1.4'/>";

    case FlatIcon::Exit:
        return "<path d='M10 4.5H6a1.8 1.8 0 0 0-1.8 1.8v11.4A1.8 1.8 0 0 0 6 19.5h4'/>"
               "<path d='m15 8 4.5 4L15 16'/>"
               "<path d='M19.5 12H9.5'/>";

    case FlatIcon::Info:
        return "<circle cx='12' cy='12' r='8.5'/>"
               "<path d='M12 11v5'/>"
               "<circle cx='12' cy='8' r='0.9' fill='COLOR' stroke='none'/>";

    case FlatIcon::Sliders:
        return "<path d='M4 7h4'/>"
               "<path d='M12 7h8'/>"
               "<circle cx='10' cy='7' r='2'/>"
               "<path d='M4 12h10'/>"
               "<path d='M18 12h2'/>"
               "<circle cx='16' cy='12' r='2'/>"
               "<path d='M4 17h6'/>"
               "<path d='M14 17h6'/>"
               "<circle cx='12' cy='17' r='2'/>";

    case FlatIcon::Keyboard:
        return "<rect x='3' y='5.5' width='18' height='13' rx='2.2'/>"
               "<path d='M7 9.5h.01M10.3 9.5h.01M13.7 9.5h.01M17 9.5h.01'/>"
               "<path d='M7 12.5h.01M10.3 12.5h.01M13.7 12.5h.01M17 12.5h.01'/>"
               "<path d='M8.5 15.5h7'/>";

    case FlatIcon::History:
        return "<path d='M4 12a8 8 0 1 0 2.3-5.6L4 8.5'/>"
               "<path d='M4 4v4.5h4.5'/>"
               "<path d='M12 7.8V12l3 2'/>";
    }
    return "";
}

QByteArray buildSvg(FlatIcon id, const QColor &color)
{
    const QByteArray col = color.name(QColor::HexRgb).toLatin1();
    QByteArray body(svgBodyFor(id));
    body.replace("COLOR", col);

    QByteArray svg;
    svg.reserve(260 + body.size());
    svg.append("<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' fill='none' stroke='");
    svg.append(col);
    svg.append("' stroke-width='1.85' stroke-linecap='round' stroke-linejoin='round'>");
    svg.append(body);
    svg.append("</svg>");
    return svg;
}

struct PixCacheKey {
    int id;
    QRgb rgba;
    int px;

    bool operator==(const PixCacheKey &o) const
    {
        return id == o.id && rgba == o.rgba && px == o.px;
    }
};

inline size_t qHash(const PixCacheKey &k, size_t seed = 0) noexcept
{
    return qHashMulti(seed, k.id, k.rgba, k.px);
}

QCache<PixCacheKey, QPixmap> &pixCache()
{
    static QCache<PixCacheKey, QPixmap> cache(256);
    return cache;
}
} // namespace

namespace FlatIcons
{
QPixmap pixmap(FlatIcon id, const QColor &color, int logicalSize, qreal dpr)
{
    const qreal safeDpr = qMax<qreal>(1.0, dpr);
    const int px = qMax(8, qRound(logicalSize * safeDpr));
    const PixCacheKey key{static_cast<int>(id), color.rgba(), px};

    if (const QPixmap *cached = pixCache().object(key)) {
        QPixmap copy = *cached;
        copy.setDevicePixelRatio(safeDpr);
        return copy;
    }

    QSvgRenderer renderer(buildSvg(id, color));
    QImage img(px, px, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);

    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    if (color.alpha() < 255) {
        p.setOpacity(color.alphaF());
    }
    renderer.render(&p, QRectF(0, 0, px, px));
    p.end();

    auto *pm = new QPixmap(QPixmap::fromImage(img));
    QPixmap copy = *pm;
    pixCache().insert(key, pm);
    copy.setDevicePixelRatio(safeDpr);
    return copy;
}

void paint(QPainter *painter, FlatIcon id, const QRectF &rect, const QColor &color)
{
    if (!painter || rect.isEmpty()) {
        return;
    }
    const qreal dpr = painter->device() ? painter->device()->devicePixelRatioF() : 1.0;
    const int side = qMax(8, qRound(qMin(rect.width(), rect.height())));
    const QPixmap pm = pixmap(id, color, side, dpr);
    const QPointF topLeft(rect.center().x() - side / 2.0, rect.center().y() - side / 2.0);
    painter->drawPixmap(topLeft, pm);
}

QIcon icon(FlatIcon id,
           const QColor &normalColor,
           const QColor &activeColor,
           const QColor &disabledColor,
           int logicalSize)
{
    QIcon ic;
    const QColor act = activeColor.isValid() ? activeColor : normalColor;
    QColor dis = disabledColor;
    if (!dis.isValid()) {
        dis = normalColor;
        dis.setAlpha(qRound(normalColor.alpha() * 0.42));
    }

    for (qreal dpr : {1.0, 2.0}) {
        ic.addPixmap(pixmap(id, normalColor, logicalSize, dpr), QIcon::Normal, QIcon::Off);
        ic.addPixmap(pixmap(id, act, logicalSize, dpr), QIcon::Active, QIcon::Off);
        ic.addPixmap(pixmap(id, act, logicalSize, dpr), QIcon::Selected, QIcon::Off);
        ic.addPixmap(pixmap(id, dis, logicalSize, dpr), QIcon::Disabled, QIcon::Off);
    }
    return ic;
}

QIcon toggleIcon(FlatIcon offId,
                 const QColor &offColor,
                 FlatIcon onId,
                 const QColor &onColor,
                 const QColor &offHoverColor,
                 int logicalSize)
{
    QIcon ic;
    const QColor offAct = offHoverColor.isValid() ? offHoverColor : offColor;
    QColor dis = offColor;
    dis.setAlpha(qRound(offColor.alpha() * 0.42));

    for (qreal dpr : {1.0, 2.0}) {
        ic.addPixmap(pixmap(offId, offColor, logicalSize, dpr), QIcon::Normal, QIcon::Off);
        ic.addPixmap(pixmap(offId, offAct, logicalSize, dpr), QIcon::Active, QIcon::Off);
        ic.addPixmap(pixmap(offId, dis, logicalSize, dpr), QIcon::Disabled, QIcon::Off);

        ic.addPixmap(pixmap(onId, onColor, logicalSize, dpr), QIcon::Normal, QIcon::On);
        ic.addPixmap(pixmap(onId, onColor, logicalSize, dpr), QIcon::Active, QIcon::On);
        ic.addPixmap(pixmap(onId, onColor, logicalSize, dpr), QIcon::Selected, QIcon::On);
    }
    return ic;
}

QPixmap appLogoPixmap(const QColor &accentColor, int logicalSize, qreal dpr)
{
    const qreal safeDpr = qMax<qreal>(1.0, dpr);
    const int px = qMax(16, qRound(logicalSize * safeDpr));
    const QColor acc = accentColor.isValid() ? accentColor : QColor(255, 113, 57);
    const QByteArray accHex = acc.name(QColor::HexRgb).toLatin1();

    QByteArray svg;
    svg.reserve(640);
    svg.append("<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 64 64'>"
               "<rect x='11' y='11' width='42' height='48' rx='9' ry='9' fill='#3a414c'/>"
               "<rect x='16' y='17' width='32' height='37' rx='5' ry='5' fill='#f5f7fa'/>"
               "<rect x='22' y='6' width='20' height='12' rx='6' ry='6' fill='");
    svg.append(accHex);
    svg.append("'/>"
               "<circle cx='32' cy='12' r='2.2' fill='#ffffff'/>"
               "<rect x='22' y='25' width='20' height='3.2' rx='1.6' fill='#3a414c'/>"
               "<rect x='22' y='32.5' width='15' height='3.2' rx='1.6' fill='#8b95a5'/>"
               "<rect x='22' y='40' width='18' height='3.2' rx='1.6' fill='#8b95a5'/>"
               "<rect x='22' y='47' width='11' height='3.2' rx='1.6' fill='");
    svg.append(accHex);
    svg.append("'/></svg>");

    QSvgRenderer renderer(svg);
    QImage img(px, px, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing, true);
    renderer.render(&p, QRectF(0, 0, px, px));
    p.end();

    QPixmap pm = QPixmap::fromImage(img);
    pm.setDevicePixelRatio(safeDpr);
    return pm;
}

QIcon appLogoIcon(const QColor &accentColor)
{
    QIcon ic;
    for (int sz : {16, 22, 24, 32, 48, 64}) {
        ic.addPixmap(appLogoPixmap(accentColor, sz, 1.0));
        ic.addPixmap(appLogoPixmap(accentColor, sz, 2.0));
    }
    return ic;
}
} // namespace FlatIcons
