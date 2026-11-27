/*
    SPDX-FileCopyrightText: 2026 Mklipper contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "tooltippreviewwidget.h"

#include "flaticons.h"
#include "historyitem.h"
#include "sourceinfo.h"
#include "textutils.h"
#include "themestyle.h"

#include <QAbstractButton>
#include <QAction>
#include <QApplication>
#include <QButtonGroup>
#include <QDateTime>
#include <QEvent>
#include <QHash>
#include <QHBoxLayout>
#include <QLabel>
#include <QLinearGradient>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPushButton>
#include <QSet>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;

namespace
{
constexpr int kPreviewWidth = 372;
constexpr int kActionBtnSize = 24;
constexpr int kActionBtnGap = 2;
constexpr qint64 kPreviewAnimDurationMs = 180;

qreal previewEaseOutCubic(qreal t)
{
    const qreal clamped = qBound<qreal>(0.0, t, 1.0);
    const qreal inv = 1.0 - clamped;
    return 1.0 - inv * inv * inv;
}

bool previewUsesMenuMode(ActionBarMode mode)
{
    return mode == ActionBarMode::MenuHoverOrClick || mode == ActionBarMode::MenuClickOnly;
}

enum class PreviewCategory {
    All = 0,
    Text = 1,
    Images = 2,
    Links = 3,
    Starred = 4,
};

enum class PreviewRowAction {
    None,
    ToggleMenu,
    Star,
    OpenSource,
    Qr,
    Preview,
    Edit,
    SaveImage,
    Delete,
};

struct SampleItem {
    HistoryItemType type = HistoryItemType::Text;
    QString line1;
    QString line2;
    QString sourceApp;
    QString sourceTitle;
    QString sourceUrl;
    QString relTime;
    bool starred = false;
};

class PreviewPlainTextSwitch : public QAbstractButton
{
public:
    explicit PreviewPlainTextSwitch(const QString &text, QWidget *parent = nullptr)
        : QAbstractButton(parent)
    {
        setText(text);
        setCheckable(true);
        setCursor(Qt::PointingHandCursor);
        setFocusPolicy(Qt::NoFocus);
        setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    }

    QSize sizeHint() const override
    {
        const QFontMetrics fm(font());
        const int trackH = trackHeightFor(fm);
        const int trackW = trackWidthFor(trackH);
        const int textW = fm.horizontalAdvance(text());
        return QSize(trackW + kSpacing + textW + 6, qMax(trackH + 4, fm.height() + 4));
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);

        const ThemeTokens t = ThemeTokens::fromPalette(QApplication::palette());
        const QFontMetrics fm(font());
        const int trackH = trackHeightFor(fm);
        const int trackW = trackWidthFor(trackH);
        const int trackY = (height() - trackH) / 2;
        const QRectF trackRect(0, trackY, trackW, trackH);

        QColor trackColor;
        if (isChecked()) {
            trackColor = underMouse() ? t.accentHover : t.accent;
        } else {
            trackColor = QColor(t.textPrimary.red(), t.textPrimary.green(), t.textPrimary.blue(),
                                underMouse() ? (t.isDark ? 72 : 66) : (t.isDark ? 50 : 44));
        }

        p.setPen(Qt::NoPen);
        p.setBrush(trackColor);
        p.drawRoundedRect(trackRect, trackH / 2.0, trackH / 2.0);

        const qreal margin = 2.5;
        const qreal knobSize = trackH - 2.0 * margin;
        const qreal knobX = isChecked() ? (trackW - knobSize - margin) : margin;
        const QRectF knobRect(knobX, trackY + margin, knobSize, knobSize);

        p.setBrush(isChecked() ? t.accentFg : QColor(255, 255, 255));
        p.drawEllipse(knobRect);

        p.setPen(isChecked() ? t.textPrimary : t.textSecondary);
        const QRect textRect(trackW + kSpacing, 0, width() - trackW - kSpacing, height());
        p.drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, text());
    }

private:
    static int trackHeightFor(const QFontMetrics &fm) { return qBound(16, fm.height(), 22); }
    static int trackWidthFor(int trackH) { return qRound(trackH * 1.8); }
    static constexpr int kSpacing = 8;
};
} // namespace

// ---------------------------------------------------------------------------
// PreviewSampleList — interactive sample cards matching ClipboardItemDelegate
// ---------------------------------------------------------------------------
class PreviewSampleList : public QWidget
{
    Q_OBJECT

public:
    explicit PreviewSampleList(HoverTooltipController *controller, QWidget *parent = nullptr)
        : QWidget(parent)
        , m_tooltipController(controller)
        , m_tokens(ThemeTokens::fromPalette(QApplication::palette()))
    {
        setMouseTracking(true);
        setCursor(Qt::PointingHandCursor);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

        m_items = {
            SampleItem{
                HistoryItemType::Url,
                u"https://github.com/Castellanoi/Mklipper/releases"_s,
                QString(),
                u"Firefox"_s,
                tr("Lanzamientos · Castellanoi/Mklipper — GitHub"),
                u"https://github.com/Castellanoi/Mklipper/releases/download/latest/mklipper_amd64_linux_mint_cinnamon_very_long_example_link_preview"_s,
                tr("Hace %1 min").arg(2),
                false,
            },
            SampleItem{
                HistoryItemType::Text,
                tr("Notas de la reunión del equipo"),
                tr("Revisar mejoras de interfaz y preparar nueva versión"),
                u"LibreOffice Writer"_s,
                tr("notas-reunion.odt — LibreOffice Writer"),
                QString(),
                tr("Hace %1 min").arg(15),
                true,
            },
            SampleItem{
                HistoryItemType::Image,
                tr("Imagen (%1×%2)").arg(1280).arg(720),
                QString(),
                tr("Captura de pantalla"),
                QString(),
                QString(),
                tr("Hace %1 h").arg(1),
                false,
            },
        };

        m_animTimer = new QTimer(this);
        m_animTimer->setInterval(16);
        connect(m_animTimer, &QTimer::timeout, this, &PreviewSampleList::tickAnimations);

        rebuildSampleThumbnail();
        rebuildVisibleIndices();
    }

    void setActionBarConfig(ActionBarMode mode, bool animated)
    {
        const bool modeChanged = (m_actionBarMode != mode);
        m_actionBarMode = mode;
        m_actionBarAnimated = animated;
        if (modeChanged) {
            m_hoverMenuSrcIdx = -1;
            m_suppressedHoverSrcIdx = -1;
            if (!previewUsesMenuMode(mode)) {
                m_pinnedSrcIndices.clear();
            }
        }
        syncAnimationTargets();
        update();
    }

    void resetTransientHover()
    {
        m_hoveredRow = -1;
        m_hoverPos = QPoint(-1, -1);
        m_hoverMenuSrcIdx = -1;
        m_suppressedHoverSrcIdx = -1;
        syncAnimationTargets();
        update();
    }

    void refreshTheme(const ThemeTokens &tokens)
    {
        m_tokens = tokens;
        rebuildSampleThumbnail();
        update();
    }

    void setCategory(PreviewCategory cat)
    {
        if (m_category == cat) {
            return;
        }
        m_category = cat;
        rebuildVisibleIndices();
        update();
        Q_EMIT itemsChanged();
    }

    void setSearchQuery(const QString &query)
    {
        if (m_query == query) {
            return;
        }
        m_query = query;
        rebuildVisibleIndices();
        update();
        Q_EMIT itemsChanged();
    }

    int totalCount() const { return static_cast<int>(m_items.size()); }
    int visibleCount() const { return static_cast<int>(m_visibleIndices.size()); }
    int starredCount() const
    {
        int count = 0;
        for (const auto &it : m_items) {
            if (it.starred) {
                ++count;
            }
        }
        return count;
    }

    QSize sizeHint() const override
    {
        int totalH = 0;
        const int lineSp = fontMetrics().lineSpacing();
        const int textH = qMax(68, lineSp * 3 + 18);
        const int imgH = qMax(74, lineSp * 3 + 24);
        for (const auto &it : m_items) {
            totalH += (it.type == HistoryItemType::Image) ? imgH : textH;
        }
        return QSize(kPreviewWidth - 24, qMax(198, totalH + 4));
    }

    QSize minimumSizeHint() const override
    {
        return sizeHint();
    }

Q_SIGNALS:
    void itemsChanged();

protected:
    bool event(QEvent *ev) override
    {
        if (ev->type() == QEvent::ToolTip) {
            return true;
        }
        return QWidget::event(ev);
    }

    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);

        const ThemeTokens &t = m_tokens;

        if (m_visibleIndices.isEmpty()) {
            painter.setPen(t.textMuted);
            painter.drawText(rect().adjusted(12, 12, -12, -12),
                             Qt::AlignCenter | Qt::TextWordWrap,
                             tr("No hay coincidencias."));
            return;
        }

        const auto rects = rowRects();
        for (int row = 0; row < m_visibleIndices.size(); ++row) {
            const int srcIdx = m_visibleIndices.at(row);
            const SampleItem &item = m_items.at(srcIdx);
            const QRect cardRect = rects.at(row).adjusted(2, 2, -2, -3);
            const bool isSelected = (row == m_selectedRow);
            const bool isHovered = (row == m_hoveredRow);
            const bool isTopItem = (row == 0);

            QColor bg = t.surfaceBg;
            if (isSelected) {
                bg = t.accentSoft;
            } else if (isHovered) {
                bg = t.surfaceHoverBg;
            } else if (isTopItem) {
                bg = QColor(t.accent.red(), t.accent.green(), t.accent.blue(), t.isDark ? 24 : 18);
            }

            QPainterPath cardPath;
            cardPath.addRoundedRect(QRectF(cardRect).adjusted(0.5, 0.5, -0.5, -0.5), 8.0, 8.0);
            painter.fillPath(cardPath, bg);

            if (isSelected) {
                QColor selEdge = t.accent;
                selEdge.setAlpha(t.isDark ? 120 : 95);
                painter.setPen(QPen(selEdge, 1.0));
                painter.drawPath(cardPath);
            } else if (!t.isDark && !isHovered) {
                painter.setPen(QPen(t.hairline, 1.0));
                painter.drawPath(cardPath);
            }

            if (isTopItem) {
                QPainterPath accentBar;
                accentBar.addRoundedRect(QRectF(cardRect.left() + 2.5, cardRect.top() + 8.0, 3.0, cardRect.height() - 16.0), 1.5, 1.5);
                painter.fillPath(accentBar, t.accent);
            }

            const bool hasMenu = previewUsesMenuMode(m_actionBarMode);
            const qreal openProg = openProgressFor(srcIdx);
            const bool paintActionBar = (openProg > 0.01);
            int rightReserved = 10;
            QList<BtnInfo> btns;
            if (m_actionBarMode == ActionBarMode::AlwaysVisible) {
                btns = buttonRects(cardRect, item);
                if (!btns.isEmpty()) {
                    rightReserved = cardRect.right() - btns.last().rect.left() + 10;
                }
            } else if (hasMenu) {
                const QRect menuRect = menuButtonRect(cardRect);
                rightReserved = cardRect.right() - menuRect.left() + (item.starred ? 24 : 8);
                if (paintActionBar) {
                    btns = buttonRects(cardRect, item);
                }
            } else {
                if (paintActionBar) {
                    btns = buttonRects(cardRect, item);
                }
                if (item.starred) {
                    rightReserved = 28;
                }
            }

            QRect contentRect = cardRect.adjusted(11, 6, -rightReserved, -6);

            // Shortcut digit chip
            const QRect badgeRect(contentRect.left(), contentRect.center().y() - 9, 18, 18);
            QPainterPath badgePath;
            badgePath.addRoundedRect(QRectF(badgeRect), 5.0, 5.0);
            QColor badgeBg = isTopItem ? t.accent
                                       : QColor(t.textPrimary.red(), t.textPrimary.green(), t.textPrimary.blue(), t.isDark ? 24 : 18);
            painter.fillPath(badgePath, badgeBg);

            QFont badgeFont = font();
            badgeFont.setPointSizeF(qMax(7.5, badgeFont.pointSizeF() - 1.5));
            badgeFont.setBold(true);
            painter.setFont(badgeFont);
            painter.setPen(isTopItem ? t.accentFg : t.textSecondary);
            painter.drawText(badgeRect, Qt::AlignCenter, QString::number(row + 1));
            contentRect.setLeft(badgeRect.right() + 9);

            // Compact source metadata row
            const QString metaLabel = compactMetaFor(item);
            QRect metaRect;
            if (!metaLabel.isEmpty()) {
                const int metaH = qMax(14, fontMetrics().height() - 2);
                metaRect = QRect(contentRect.left(), contentRect.bottom() - metaH + 1, contentRect.width(), metaH);
                contentRect.setBottom(metaRect.top() - 2);
            }

            if (item.type == HistoryItemType::Image) {
                if (!m_sampleThumb.isNull()) {
                    const QSize sz = m_sampleThumb.deviceIndependentSize().toSize();
                    const QRect thumbRect(contentRect.left(),
                                          contentRect.center().y() - sz.height() / 2,
                                          sz.width(),
                                          sz.height());
                    painter.save();
                    QPainterPath clip;
                    clip.addRoundedRect(QRectF(thumbRect), 5.0, 5.0);
                    painter.setClipPath(clip);
                    painter.drawPixmap(thumbRect, m_sampleThumb);
                    painter.restore();
                    painter.setPen(QPen(t.hairline, 1.0));
                    painter.drawRoundedRect(QRectF(thumbRect).adjusted(0.5, 0.5, -0.5, -0.5), 5.0, 5.0);
                    contentRect.setLeft(thumbRect.right() + 8);
                }
                painter.setPen(t.textPrimary);
                QFont f = font();
                f.setPointSizeF(qMax(8.5, f.pointSizeF() - 0.5));
                f.setBold(isTopItem);
                painter.setFont(f);
                painter.drawText(contentRect, Qt::AlignVCenter | Qt::AlignLeft,
                                 painter.fontMetrics().elidedText(item.line1, Qt::ElideRight, contentRect.width()));
            } else {
                if (item.type == HistoryItemType::Url) {
                    const QRectF iconRect(contentRect.left(), contentRect.center().y() - 6.5, 13.0, 13.0);
                    FlatIcons::paint(&painter, FlatIcon::Link, iconRect, t.accent);
                    contentRect.setLeft(qRound(iconRect.right()) + 6);
                }

                QFont mainFont = font();
                mainFont.setPointSizeF(qMax(8.5, mainFont.pointSizeF() - 0.5));
                mainFont.setBold(isTopItem);
                painter.setFont(mainFont);
                painter.setPen(t.textPrimary);
                const QFontMetrics fm1(mainFont);

                if (item.line2.isEmpty()) {
                    painter.drawText(contentRect, Qt::AlignVCenter | Qt::AlignLeft,
                                     fm1.elidedText(item.line1, Qt::ElideRight, contentRect.width()));
                } else {
                    const int halfH = contentRect.height() / 2;
                    const QRect r1(contentRect.left(), contentRect.top(), contentRect.width(), halfH);
                    const QRect r2(contentRect.left(), contentRect.top() + halfH, contentRect.width(), contentRect.height() - halfH);
                    painter.drawText(r1, Qt::AlignVCenter | Qt::AlignLeft,
                                     fm1.elidedText(item.line1, Qt::ElideRight, r1.width()));

                    QFont subFont = font();
                    subFont.setPointSizeF(qMax(7.5, subFont.pointSizeF() - 1.3));
                    painter.setFont(subFont);
                    painter.setPen(t.textSecondary);
                    const QFontMetrics fm2(subFont);
                    painter.drawText(r2, Qt::AlignVCenter | Qt::AlignLeft,
                                     fm2.elidedText(item.line2, Qt::ElideRight, r2.width()));
                }
            }

            // Source metadata footer inside card
            if (!metaLabel.isEmpty() && metaRect.isValid() && metaRect.width() > 24) {
                QFont metaFont = font();
                metaFont.setPointSizeF(qMax(7.2, metaFont.pointSizeF() - 1.8));
                painter.setFont(metaFont);

                const int dotSize = 5;
                const QRectF dotRect(metaRect.left(), metaRect.center().y() - dotSize / 2.0, dotSize, dotSize);
                painter.setPen(Qt::NoPen);
                painter.setBrush(item.sourceUrl.isEmpty() ? t.textMuted : t.accent);
                painter.drawEllipse(dotRect);
                painter.setBrush(Qt::NoBrush);

                painter.setPen(t.textMuted);
                const QRect textMetaRect = metaRect.adjusted(dotSize + 5, 0, 0, 0);
                const QFontMetrics fmMeta(metaFont);
                painter.drawText(textMetaRect, Qt::AlignVCenter | Qt::AlignLeft,
                                 fmMeta.elidedText(metaLabel, Qt::ElideRight, textMetaRect.width()));
            }

            // Persistent star indicator when action bar is collapsed
            if (!paintActionBar && item.starred) {
                const qreal rightEdge = hasMenu ? static_cast<qreal>(menuButtonRect(cardRect).left() - 4)
                                                : static_cast<qreal>(cardRect.right() - 9);
                const QRectF starRect(rightEdge - 13.0, cardRect.center().y() - 6.5, 13.0, 13.0);
                FlatIcons::paint(&painter, FlatIcon::StarFilled, starRect, t.warning);
            }

            // Sweeping animated flat action pill
            const QRect menuRect = menuButtonRect(cardRect);
            if (paintActionBar && !btns.isEmpty()) {
                const qreal eased = previewEaseOutCubic(openProg);
                const int expandedLeft = btns.last().rect.left() - 3;
                const int pillRight = hasMenu ? (menuRect.right() + 3) : (btns.first().rect.right() + 3);
                const int collapsedLeft = hasMenu ? (menuRect.left() - 3) : pillRight;
                const int animLeft = qRound(collapsedLeft + (expandedLeft - collapsedLeft) * eased);
                const int pillTop = btns.first().rect.top() - 2;
                const int pillHeight = btns.first().rect.height() + 4;
                const QRect animPillRect(animLeft, pillTop, qMax(1, pillRight - animLeft + 1), pillHeight);

                QColor pillBg = t.isDark ? t.surfaceActiveBg : t.surfaceBg;
                pillBg.setAlpha(250);
                QPainterPath pillPath;
                pillPath.addRoundedRect(QRectF(animPillRect).adjusted(0.5, 0.5, -0.5, -0.5), 7.0, 7.0);
                painter.fillPath(pillPath, pillBg);
                painter.strokePath(pillPath, QPen(t.borderSubtle, 1.0));

                const int clipRight = hasMenu ? menuRect.left() : (animPillRect.right() + 1);
                if (clipRight > animPillRect.left()) {
                    painter.save();
                    QPainterPath clipPath;
                    clipPath.addRoundedRect(QRectF(animPillRect).adjusted(1.0, 1.0, -1.0, -1.0), 6.5, 6.5);
                    painter.setClipPath(clipPath);
                    painter.setClipRect(QRect(animPillRect.left(), animPillRect.top(), clipRight - animPillRect.left(), animPillRect.height()),
                                        Qt::IntersectClip);

                    const int maxSlide = qMin(44, qMax(0, collapsedLeft - expandedLeft));
                    const int slideDx = qRound((1.0 - eased) * maxSlide);
                    painter.setOpacity(qBound<qreal>(0.0, openProg * 1.35, 1.0));

                    for (const auto &b : btns) {
                        const QRect drawRect = b.rect.translated(slideDx, 0);
                        const bool btnHover = (openProg >= 0.30) && isHovered && b.rect.contains(m_hoverPos);
                        if (btnHover) {
                            QPainterPath bp;
                            bp.addRoundedRect(QRectF(drawRect), 5.0, 5.0);
                            QColor hbg = (b.action == PreviewRowAction::Delete) ? t.dangerSoft
                                       : (b.action == PreviewRowAction::Star)   ? t.warningSoft
                                                                                : t.surfaceHoverBg;
                            painter.fillPath(bp, hbg);
                        }

                        QColor iconColor = t.textSecondary;
                        if (b.active) {
                            iconColor = t.warning;
                        } else if (btnHover && b.action == PreviewRowAction::Delete) {
                            iconColor = t.danger;
                        } else if (btnHover && b.action == PreviewRowAction::Star) {
                            iconColor = t.warning;
                        } else if (btnHover) {
                            iconColor = t.textPrimary;
                        }

                        const QRectF iconRect(drawRect.center().x() - 7.0, drawRect.center().y() - 7.0, 14.0, 14.0);
                        FlatIcons::paint(&painter, b.icon, iconRect, iconColor);
                    }
                    painter.restore();
                }
            }

            // Vertical 3-dot trigger button anchored on the right edge
            if (hasMenu) {
                const bool pinned = m_pinnedSrcIndices.contains(srcIdx);
                const bool menuHover = isHovered && menuRect.contains(m_hoverPos);
                if (pinned || menuHover) {
                    QPainterPath mp;
                    mp.addRoundedRect(QRectF(menuRect), 5.0, 5.0);
                    painter.fillPath(mp, pinned ? t.accentSoft : t.surfaceHoverBg);
                }
                const QColor dotColor = pinned                    ? t.accent
                                      : menuHover                 ? t.textPrimary
                                      : (isHovered || isSelected) ? t.textSecondary
                                                                  : t.textMuted;
                const QRectF dotRect(menuRect.center().x() - 7.0, menuRect.center().y() - 7.0, 14.0, 14.0);
                FlatIcons::paint(&painter, FlatIcon::MoreVertical, dotRect, dotColor);
            }
        }
    }

    void mouseMoveEvent(QMouseEvent *ev) override
    {
        const QPoint pos = ev->position().toPoint();
        m_hoverPos = pos;

        const int prevRow = m_hoveredRow;
        const int prevHoverMenu = m_hoverMenuSrcIdx;
        m_hoveredRow = rowAt(pos);

        if (m_hoveredRow >= 0 && m_hoveredRow < m_visibleIndices.size()) {
            const int srcIdx = m_visibleIndices.at(m_hoveredRow);
            const SampleItem &item = m_items.at(srcIdx);
            const QRect cardRect = rowRects().at(m_hoveredRow).adjusted(2, 2, -2, -3);
            const QRect menuRect = menuButtonRect(cardRect);
            const QPoint gPos = ev->globalPosition().toPoint();

            if (m_actionBarMode == ActionBarMode::MenuHoverOrClick) {
                const QRect pillRect = expandedPillRect(cardRect, item).adjusted(0, -2, 0, 2);
                const bool overMenu = menuRect.contains(pos);
                if (!overMenu && m_suppressedHoverSrcIdx == srcIdx) {
                    m_suppressedHoverSrcIdx = -1;
                }
                if (m_suppressedHoverSrcIdx != srcIdx) {
                    const bool alreadyOpen = (m_hoverMenuSrcIdx == srcIdx) || (openProgressFor(srcIdx) > 0.05);
                    if (overMenu || (alreadyOpen && pillRect.contains(pos))) {
                        m_hoverMenuSrcIdx = srcIdx;
                    } else {
                        m_hoverMenuSrcIdx = -1;
                    }
                } else {
                    m_hoverMenuSrcIdx = -1;
                }
            } else {
                m_hoverMenuSrcIdx = -1;
                m_suppressedHoverSrcIdx = -1;
            }

            if (m_hoverMenuSrcIdx != prevHoverMenu || (m_actionBarMode == ActionBarMode::AlwaysOnHover && prevRow != m_hoveredRow)) {
                syncAnimationTargets();
            }

            update();

            if (previewUsesMenuMode(m_actionBarMode) && menuRect.contains(pos)) {
                if (m_tooltipController) {
                    const QString tip = m_pinnedSrcIndices.contains(srcIdx)
                        ? tr("Ocultar acciones")
                        : tr("Acciones del elemento");
                    const QString key = QStringLiteral("prev:%1:menu").arg(srcIdx);
                    m_tooltipController->hoverTarget(key,
                                                     TooltipCategory::ItemActions,
                                                     tip,
                                                     gPos,
                                                     this,
                                                     menuRect);
                }
                return;
            }

            if (openProgressFor(srcIdx) >= 0.30) {
                const auto btns = buttonRects(cardRect, item);
                for (const auto &b : btns) {
                    if (b.rect.contains(pos)) {
                        if (m_tooltipController) {
                            const QString key = QStringLiteral("prev:%1:act:%2").arg(srcIdx).arg(static_cast<int>(b.action));
                            m_tooltipController->hoverTarget(key,
                                                             TooltipCategory::ItemActions,
                                                             b.tooltip,
                                                             gPos,
                                                             this,
                                                             b.rect);
                        }
                        return;
                    }
                }
            }

            if (m_tooltipController) {
                const QString tip = HoverTooltip::formatSourceTooltip(item.sourceApp,
                                                                      item.sourceTitle,
                                                                      item.sourceUrl,
                                                                      item.relTime);
                if (!tip.isEmpty()) {
                    const QString key = QStringLiteral("prev:%1:info").arg(srcIdx);
                    m_tooltipController->hoverTarget(key,
                                                     TooltipCategory::ItemInfo,
                                                     tip,
                                                     gPos,
                                                     this,
                                                     cardRect);
                } else {
                    m_tooltipController->cancel();
                }
            }
        } else {
            m_hoverMenuSrcIdx = -1;
            m_suppressedHoverSrcIdx = -1;
            if (prevHoverMenu != -1 || m_actionBarMode == ActionBarMode::AlwaysOnHover) {
                syncAnimationTargets();
            }
            if (m_hoveredRow != prevRow) {
                update();
            }
            if (m_tooltipController) {
                m_tooltipController->cancel();
            }
        }
    }

    void leaveEvent(QEvent *ev) override
    {
        QWidget::leaveEvent(ev);
        resetTransientHover();
        if (m_tooltipController) {
            m_tooltipController->cancel();
        }
    }

    void mousePressEvent(QMouseEvent *ev) override
    {
        if (m_tooltipController) {
            m_tooltipController->cancel();
        }
        if (ev->button() == Qt::LeftButton) {
            const QPoint pos = ev->position().toPoint();
            const int row = rowAt(pos);
            if (row >= 0 && row < m_visibleIndices.size()) {
                const int srcIdx = m_visibleIndices.at(row);
                const SampleItem &item = m_items.at(srcIdx);
                const QRect cardRect = rowRects().at(row).adjusted(2, 2, -2, -3);

                if (previewUsesMenuMode(m_actionBarMode) && menuButtonRect(cardRect).contains(pos)) {
                    if (m_pinnedSrcIndices.contains(srcIdx)) {
                        m_pinnedSrcIndices.remove(srcIdx);
                        if (m_hoverMenuSrcIdx == srcIdx) {
                            m_hoverMenuSrcIdx = -1;
                        }
                        m_suppressedHoverSrcIdx = srcIdx;
                    } else {
                        m_pinnedSrcIndices.insert(srcIdx);
                        if (m_suppressedHoverSrcIdx == srcIdx) {
                            m_suppressedHoverSrcIdx = -1;
                        }
                    }
                    syncAnimationTargets();
                    update();
                    return;
                }

                if (openProgressFor(srcIdx) >= 0.30) {
                    const auto btns = buttonRects(cardRect, item);
                    for (const auto &b : btns) {
                        if (b.rect.contains(pos)) {
                            if (b.action == PreviewRowAction::Star) {
                                m_items[srcIdx].starred = !m_items[srcIdx].starred;
                                rebuildVisibleIndices();
                                update();
                                Q_EMIT itemsChanged();
                            }
                            return;
                        }
                    }
                    if (expandedPillRect(cardRect, item).contains(pos)) {
                        return;
                    }
                }

                m_selectedRow = row;
                if (m_actionBarMode == ActionBarMode::AlwaysOnHover) {
                    syncAnimationTargets();
                }
                update();
            }
        }
        QWidget::mousePressEvent(ev);
    }

private:
    struct BtnInfo {
        PreviewRowAction action;
        QRect rect;
        QString tooltip;
        FlatIcon icon;
        bool active = false;
    };

    static QString compactMetaFor(const SampleItem &item)
    {
        QStringList parts;
        const QString src = formatCompactSourceLabel(item.sourceApp, item.sourceTitle, item.sourceUrl);
        if (!src.isEmpty()) {
            parts.append(src);
        }
        if (!item.relTime.isEmpty()) {
            parts.append(item.relTime);
        }
        return parts.join(u" · "_s);
    }

    static QRect menuButtonRect(const QRect &cardRect)
    {
        const int right = cardRect.right() - 7;
        const int cy = cardRect.center().y() - kActionBtnSize / 2;
        return QRect(right - kActionBtnSize, cy, kActionBtnSize, kActionBtnSize);
    }

    QList<BtnInfo> buttonRects(const QRect &cardRect, const SampleItem &item) const
    {
        QList<BtnInfo> btns;
        const bool hasMenu = previewUsesMenuMode(m_actionBarMode);
        int right = hasMenu ? (menuButtonRect(cardRect).left() - kActionBtnGap) : (cardRect.right() - 7);
        const int cy = cardRect.center().y() - kActionBtnSize / 2;

        auto push = [&](PreviewRowAction act, const QString &tip, FlatIcon icon, bool active = false) {
            QRect r(right - kActionBtnSize, cy, kActionBtnSize, kActionBtnSize);
            btns.append({act, r, tip, icon, active});
            right -= (kActionBtnSize + kActionBtnGap);
        };

        push(PreviewRowAction::Delete, tr("Eliminar"), FlatIcon::Trash);
        if (item.type == HistoryItemType::Image) {
            push(PreviewRowAction::SaveImage, tr("Guardar imagen como PNG…"), FlatIcon::Save);
            push(PreviewRowAction::Preview, tr("Vista previa (Espacio)"), FlatIcon::Eye);
        } else {
            if (item.type == HistoryItemType::Text) {
                push(PreviewRowAction::Edit, tr("Editar texto"), FlatIcon::Edit);
            }
            if (item.type == HistoryItemType::Url) {
                push(PreviewRowAction::Preview, tr("Vista previa (Espacio)"), FlatIcon::Eye);
            }
            push(PreviewRowAction::Qr, tr("Mostrar código QR"), FlatIcon::QrCode);
        }
        if (!item.sourceUrl.isEmpty()) {
            push(PreviewRowAction::OpenSource, tr("Abrir enlace de origen"), FlatIcon::ExternalLink);
        }
        push(PreviewRowAction::Star,
             item.starred ? tr("Desfijar") : tr("Fijar elemento"),
             item.starred ? FlatIcon::StarFilled : FlatIcon::StarOutline,
             item.starred);
        return btns;
    }

    QRect expandedPillRect(const QRect &cardRect, const SampleItem &item) const
    {
        const auto btns = buttonRects(cardRect, item);
        if (btns.isEmpty()) {
            return {};
        }
        const int left = btns.last().rect.left() - 3;
        const int right = previewUsesMenuMode(m_actionBarMode)
            ? (menuButtonRect(cardRect).right() + 3)
            : (btns.first().rect.right() + 3);
        const int top = btns.first().rect.top() - 2;
        const int height = btns.first().rect.height() + 4;
        return QRect(left, top, right - left + 1, height);
    }

    qreal openProgressFor(int srcIdx) const
    {
        if (m_actionBarMode == ActionBarMode::AlwaysVisible) {
            return 1.0;
        }
        return qBound<qreal>(0.0, m_openProgress.value(srcIdx, 0.0), 1.0);
    }

    QSet<int> openTargets() const
    {
        QSet<int> targets;
        if (previewUsesMenuMode(m_actionBarMode)) {
            targets = m_pinnedSrcIndices;
            if (m_actionBarMode == ActionBarMode::MenuHoverOrClick && m_hoverMenuSrcIdx >= 0) {
                targets.insert(m_hoverMenuSrcIdx);
            }
        } else if (m_actionBarMode == ActionBarMode::AlwaysOnHover) {
            if (m_hoveredRow >= 0 && m_hoveredRow < m_visibleIndices.size()) {
                targets.insert(m_visibleIndices.at(m_hoveredRow));
            }
            if (m_selectedRow >= 0 && m_selectedRow < m_visibleIndices.size()) {
                targets.insert(m_visibleIndices.at(m_selectedRow));
            }
        }
        return targets;
    }

    void syncAnimationTargets()
    {
        if (m_actionBarMode == ActionBarMode::AlwaysVisible) {
            if (m_animTimer) {
                m_animTimer->stop();
            }
            m_openProgress.clear();
            return;
        }

        const QSet<int> targets = openTargets();
        if (!m_actionBarAnimated) {
            if (m_animTimer) {
                m_animTimer->stop();
            }
            m_openProgress.clear();
            for (int id : targets) {
                m_openProgress.insert(id, 1.0);
            }
            return;
        }

        for (int id : targets) {
            if (!m_openProgress.contains(id)) {
                m_openProgress.insert(id, 0.0);
            }
        }

        bool needsAnim = false;
        for (auto it = m_openProgress.constBegin(); it != m_openProgress.constEnd(); ++it) {
            const qreal target = targets.contains(it.key()) ? 1.0 : 0.0;
            if (qAbs(it.value() - target) > 0.001) {
                needsAnim = true;
                break;
            }
        }

        if (needsAnim && m_animTimer && !m_animTimer->isActive()) {
            m_lastAnimMs = QDateTime::currentMSecsSinceEpoch();
            m_animTimer->start();
        }
    }

    void tickAnimations()
    {
        if (m_actionBarMode == ActionBarMode::AlwaysVisible) {
            if (m_animTimer) {
                m_animTimer->stop();
            }
            return;
        }

        const QSet<int> targets = openTargets();
        const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
        const qint64 dt = qBound(qint64(1), nowMs - m_lastAnimMs, qint64(60));
        m_lastAnimMs = nowMs;
        const qreal step = static_cast<qreal>(dt) / static_cast<qreal>(kPreviewAnimDurationMs);

        bool anyActive = false;
        for (auto it = m_openProgress.begin(); it != m_openProgress.end();) {
            const qreal target = targets.contains(it.key()) ? 1.0 : 0.0;
            qreal cur = it.value();
            if (cur < target) {
                cur = qMin(target, cur + step);
            } else if (cur > target) {
                cur = qMax(target, cur - step);
            }
            if (cur <= 0.001 && target == 0.0) {
                it = m_openProgress.erase(it);
                continue;
            }
            it.value() = cur;
            if (qAbs(cur - target) > 0.001) {
                anyActive = true;
            }
            ++it;
        }

        update();
        if (!anyActive && m_animTimer) {
            m_animTimer->stop();
        }
    }

    QList<QRect> rowRects() const
    {
        QList<QRect> rects;
        const int lineSp = fontMetrics().lineSpacing();
        const int textH = qMax(68, lineSp * 3 + 18);
        const int imgH = qMax(74, lineSp * 3 + 24);
        int y = 0;
        for (int idx : m_visibleIndices) {
            const int h = (m_items.at(idx).type == HistoryItemType::Image) ? imgH : textH;
            rects.append(QRect(0, y, width(), h));
            y += h;
        }
        return rects;
    }

    int rowAt(const QPoint &pos) const
    {
        const auto rects = rowRects();
        for (int i = 0; i < rects.size(); ++i) {
            if (rects.at(i).contains(pos)) {
                return i;
            }
        }
        return -1;
    }

    void rebuildVisibleIndices()
    {
        m_visibleIndices.clear();
        for (int i = 0; i < m_items.size(); ++i) {
            const SampleItem &it = m_items.at(i);
            bool catOk = true;
            switch (m_category) {
            case PreviewCategory::All:
                break;
            case PreviewCategory::Text:
                catOk = (it.type != HistoryItemType::Image);
                break;
            case PreviewCategory::Images:
                catOk = (it.type == HistoryItemType::Image);
                break;
            case PreviewCategory::Links:
                catOk = (it.type == HistoryItemType::Url);
                break;
            case PreviewCategory::Starred:
                catOk = it.starred;
                break;
            }
            if (!catOk) {
                continue;
            }
            if (!m_query.trimmed().isEmpty()) {
                const QString combined = it.line1 + QLatin1Char(' ') + it.line2 + QLatin1Char(' ')
                    + it.sourceApp + QLatin1Char(' ') + it.sourceTitle + QLatin1Char(' ') + it.sourceUrl;
                if (!MklipperText::matchesFoldedQuery(combined, m_query)) {
                    continue;
                }
            }
            m_visibleIndices.append(i);
        }
        if (m_selectedRow >= m_visibleIndices.size()) {
            m_selectedRow = m_visibleIndices.isEmpty() ? -1 : 0;
        }
        if (m_hoveredRow >= m_visibleIndices.size()) {
            m_hoveredRow = -1;
        }
    }

    void rebuildSampleThumbnail()
    {
        const qreal dpr = qMax<qreal>(1.0, devicePixelRatioF());
        const QSize logical(56, 36);
        QPixmap pm(qRound(logical.width() * dpr), qRound(logical.height() * dpr));
        pm.setDevicePixelRatio(dpr);
        pm.fill(Qt::transparent);

        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing, true);

        QLinearGradient sky(0, 0, logical.width(), logical.height());
        sky.setColorAt(0.0, m_tokens.accent);
        sky.setColorAt(1.0, m_tokens.accentHover.darker(m_tokens.isDark ? 140 : 115));
        p.fillRect(QRect(QPoint(0, 0), logical), sky);

        // Minimalist mountain + sun illustration
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(255, 255, 255, 210));
        p.drawEllipse(QPointF(14, 11), 4.2, 4.2);

        QPainterPath m1;
        m1.moveTo(0, logical.height());
        m1.lineTo(18, 18);
        m1.lineTo(35, logical.height());
        m1.closeSubpath();
        p.setBrush(QColor(255, 255, 255, 95));
        p.drawPath(m1);

        QPainterPath m2;
        m2.moveTo(20, logical.height());
        m2.lineTo(39, 13);
        m2.lineTo(logical.width(), logical.height());
        m2.closeSubpath();
        p.setBrush(QColor(255, 255, 255, 145));
        p.drawPath(m2);

        m_sampleThumb = pm;
    }

    HoverTooltipController *m_tooltipController = nullptr;
    ThemeTokens m_tokens;
    QList<SampleItem> m_items;
    QList<int> m_visibleIndices;
    PreviewCategory m_category = PreviewCategory::All;
    QString m_query;
    int m_selectedRow = 0;
    int m_hoveredRow = -1;
    QPoint m_hoverPos{-1, -1};
    ActionBarMode m_actionBarMode = ActionBarMode::MenuHoverOrClick;
    bool m_actionBarAnimated = true;
    QSet<int> m_pinnedSrcIndices;
    int m_hoverMenuSrcIdx = -1;
    int m_suppressedHoverSrcIdx = -1;
    QHash<int, qreal> m_openProgress;
    QTimer *m_animTimer = nullptr;
    qint64 m_lastAnimMs = 0;
    QPixmap m_sampleThumb;
};

// ---------------------------------------------------------------------------
// TooltipPreviewWidget
// ---------------------------------------------------------------------------
TooltipPreviewWidget::TooltipPreviewWidget(QWidget *parent)
    : QFrame(parent)
{
    setObjectName(u"mklipperPopup"_s);
    setFrameShape(QFrame::NoFrame);
    const qreal fontScale = qBound(1.0, fontMetrics().height() / 17.0, 1.35);
    setFixedWidth(qRound(kPreviewWidth * fontScale));
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);

    m_tooltipController = new HoverTooltipController(this);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 10, 10, 8);
    layout->setSpacing(6);

    // Top bar matching KlipperPopup
    auto *topBar = new QHBoxLayout;
    topBar->setSpacing(4);
    const int toolBtnSide = qMax(30, fontMetrics().height() + 12);

    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setObjectName(u"mklipperSearch"_s);
    m_searchEdit->setPlaceholderText(tr("Buscar en el historial…"));
    m_searchEdit->setClearButtonEnabled(true);
    m_searchIconAction = m_searchEdit->addAction(QIcon(), QLineEdit::LeadingPosition);
    m_searchEdit->setFixedHeight(toolBtnSide);

    m_pauseBtn = new QToolButton(this);
    m_pauseBtn->setObjectName(u"mklipperStarBtn"_s);
    m_pauseBtn->setCheckable(true);
    m_pauseBtn->setIconSize(QSize(15, 15));
    m_pauseBtn->setCursor(Qt::PointingHandCursor);
    m_pauseBtn->setFixedSize(toolBtnSide, toolBtnSide);

    m_clearBtn = new QToolButton(this);
    m_clearBtn->setObjectName(u"mklipperDangerBtn"_s);
    m_clearBtn->setIconSize(QSize(15, 15));
    m_clearBtn->setCursor(Qt::PointingHandCursor);
    m_clearBtn->setFixedSize(toolBtnSide, toolBtnSide);

    m_configBtn = new QToolButton(this);
    m_configBtn->setObjectName(u"mklipperIconBtn"_s);
    m_configBtn->setIconSize(QSize(15, 15));
    m_configBtn->setCursor(Qt::PointingHandCursor);
    m_configBtn->setFixedSize(toolBtnSide, toolBtnSide);

    m_closeBtn = new QToolButton(this);
    m_closeBtn->setObjectName(u"mklipperDangerBtn"_s);
    m_closeBtn->setIconSize(QSize(15, 15));
    m_closeBtn->setCursor(Qt::PointingHandCursor);
    m_closeBtn->setFixedSize(toolBtnSide, toolBtnSide);

    topBar->addWidget(m_searchEdit, 1);
    topBar->addWidget(m_pauseBtn);
    topBar->addWidget(m_clearBtn);
    topBar->addWidget(m_configBtn);
    topBar->addWidget(m_closeBtn);
    layout->addLayout(topBar);

    // Filter chips
    auto *filterBar = new QHBoxLayout;
    filterBar->setContentsMargins(0, 0, 0, 0);
    filterBar->setSpacing(3);
    m_categoryGroup = new QButtonGroup(this);
    m_categoryGroup->setExclusive(true);

    auto makeChip = [&](const QString &label, PreviewCategory cat) {
        auto *btn = new QPushButton(label, this);
        btn->setProperty("filterChip", true);
        btn->setCheckable(true);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setFocusPolicy(Qt::NoFocus);
        QFont cf = btn->font();
        cf.setPointSizeF(qMax(7.8, cf.pointSizeF() - 1.4));
        btn->setFont(cf);
        m_categoryGroup->addButton(btn, static_cast<int>(cat));
        filterBar->addWidget(btn);
        return btn;
    };

    auto *allBtn = makeChip(tr("Todos"), PreviewCategory::All);
    makeChip(tr("Texto"), PreviewCategory::Text);
    makeChip(tr("Imágenes"), PreviewCategory::Images);
    makeChip(tr("Enlaces"), PreviewCategory::Links);
    makeChip(tr("Fijados"), PreviewCategory::Starred);
    allBtn->setChecked(true);
    filterBar->addStretch();
    layout->addLayout(filterBar);

    // Plain-text toggle strip
    auto *modeBar = new QHBoxLayout;
    modeBar->setContentsMargins(2, 0, 2, 0);
    modeBar->setSpacing(6);

    m_plainTextSwitch = new PreviewPlainTextSwitch(tr("Pegar sin formato"), this);
    QFont swFont = m_plainTextSwitch->font();
    swFont.setPointSizeF(qMax(8.2, swFont.pointSizeF() - 0.8));
    m_plainTextSwitch->setFont(swFont);
    connect(m_plainTextSwitch, &QAbstractButton::toggled, this, [this](bool checked) {
        m_plainText = checked;
        updatePlainTextUi();
    });

    m_plainTextHintLabel = new QLabel(this);
    QFont hintFont = m_plainTextHintLabel->font();
    hintFont.setPointSizeF(qMax(7.5, hintFont.pointSizeF() - 1.6));
    m_plainTextHintLabel->setFont(hintFont);
    m_plainTextHintLabel->setTextFormat(Qt::PlainText);
    m_plainTextHintLabel->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);

    modeBar->addWidget(m_plainTextSwitch);
    modeBar->addStretch();
    modeBar->addWidget(m_plainTextHintLabel);
    layout->addLayout(modeBar);

    // Pause banner (toggled when user clicks Pause in the interactive preview)
    m_pauseBanner = new QFrame(this);
    m_pauseBanner->setObjectName(u"mklipperPauseBanner"_s);
    m_pauseBanner->setFrameShape(QFrame::NoFrame);
    auto *pauseLayout = new QHBoxLayout(m_pauseBanner);
    pauseLayout->setContentsMargins(8, 4, 6, 4);
    pauseLayout->setSpacing(6);

    m_pauseIconLabel = new QLabel(m_pauseBanner);
    m_pauseIconLabel->setFixedSize(14, 14);
    m_pauseLabel = new QLabel(tr("Captura en pausa (modo incógnito)"), m_pauseBanner);
    QFont plFont = m_pauseLabel->font();
    plFont.setPointSizeF(qMax(7.8, plFont.pointSizeF() - 1.2));
    m_pauseLabel->setFont(plFont);
    m_pauseLabel->setWordWrap(true);
    auto *resumeBtn = new QPushButton(tr("Reanudar"), m_pauseBanner);
    resumeBtn->setProperty("ghost", true);
    resumeBtn->setCursor(Qt::PointingHandCursor);
    connect(resumeBtn, &QPushButton::clicked, this, [this]() {
        m_paused = false;
        updatePauseUi();
    });
    pauseLayout->addWidget(m_pauseIconLabel);
    pauseLayout->addWidget(m_pauseLabel, 1);
    pauseLayout->addWidget(resumeBtn);
    m_pauseBanner->setVisible(false);
    layout->addWidget(m_pauseBanner);

    auto *topDivider = new QFrame(this);
    topDivider->setObjectName(u"mklipperDivider"_s);
    layout->addWidget(topDivider);

    // Interactive sample item list
    m_sampleList = new PreviewSampleList(m_tooltipController, this);
    layout->addWidget(m_sampleList, 1);

    auto *bottomDivider = new QFrame(this);
    bottomDivider->setObjectName(u"mklipperDivider"_s);
    layout->addWidget(bottomDivider);

    m_statusLabel = new QLabel(this);
    QFont stFont = m_statusLabel->font();
    stFont.setPointSizeF(qMax(7.5, stFont.pointSizeF() - 1.6));
    m_statusLabel->setFont(stFont);
    m_statusLabel->setTextFormat(Qt::PlainText);
    m_statusLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    layout->addWidget(m_statusLabel);

    // Register interactive tooltip widgets with the preview controller
    m_tooltipController->registerWidget(
        m_pauseBtn,
        TooltipCategory::HeaderButtons,
        [this]() {
            return m_paused
                ? tr("Reanudar captura del portapapeles")
                : tr("Pausar captura del portapapeles (modo incógnito)");
        });
    m_tooltipController->registerWidget(
        m_clearBtn,
        TooltipCategory::HeaderButtons,
        []() { return tr("Vaciar historial"); });
    m_tooltipController->registerWidget(
        m_configBtn,
        TooltipCategory::HeaderButtons,
        []() { return tr("Configurar Mklipper"); });
    m_tooltipController->registerWidget(
        m_closeBtn,
        TooltipCategory::HeaderButtons,
        []() { return tr("Cerrar ventana"); });
    m_tooltipController->registerWidget(
        m_plainTextSwitch,
        TooltipCategory::PlainTextSwitch,
        []() {
            return tr("Quita colores, fuentes y estilos HTML al pegar texto (no afecta imágenes ni archivos). Consejo: mantén Shift al pegar para invertirlo puntualmente.");
        },
        true);

    connect(m_pauseBtn, &QToolButton::toggled, this, [this](bool checked) {
        m_paused = checked;
        updatePauseUi();
    });
    connect(m_searchEdit, &QLineEdit::textChanged, this, [this](const QString &txt) {
        m_sampleList->setSearchQuery(txt);
    });
    connect(m_categoryGroup, &QButtonGroup::idClicked, this, [this](int id) {
        m_sampleList->setCategory(static_cast<PreviewCategory>(id));
    });
    connect(m_sampleList, &PreviewSampleList::itemsChanged, this, &TooltipPreviewWidget::updateStatusLabel);

    applyTheme();
    updatePauseUi();
    updatePlainTextUi();
    updateStatusLabel();
}

void TooltipPreviewWidget::setTooltipConfig(const TooltipConfig &config)
{
    if (m_tooltipController) {
        m_tooltipController->setConfig(config);
    }
    if (m_sampleList) {
        m_sampleList->setActionBarConfig(config.actionBarMode, config.actionBarAnimation);
    }
}

TooltipConfig TooltipPreviewWidget::tooltipConfig() const
{
    return m_tooltipController ? m_tooltipController->config() : TooltipConfig{};
}

void TooltipPreviewWidget::refreshTheme()
{
    applyTheme();
}

bool TooltipPreviewWidget::event(QEvent *ev)
{
    if (ev->type() == QEvent::Hide || ev->type() == QEvent::WindowDeactivate) {
        if (m_tooltipController) {
            m_tooltipController->cancel();
        }
        if (m_sampleList) {
            m_sampleList->resetTransientHover();
        }
    }
    return QFrame::event(ev);
}

void TooltipPreviewWidget::changeEvent(QEvent *ev)
{
    QFrame::changeEvent(ev);
    if (ev->type() == QEvent::PaletteChange || ev->type() == QEvent::ApplicationPaletteChange) {
        applyTheme();
    }
}

void TooltipPreviewWidget::applyTheme()
{
    if (m_applyingTheme) {
        return;
    }
    m_applyingTheme = true;

    const QPalette appPal = QApplication::palette();
    const ThemeTokens t = ThemeTokens::fromPalette(appPal);
    QString css = ThemeStyle::popupStyleSheet(t);
    css += u"\nQFrame#mklipperPopup { border-radius: 10px; }\n"_s;
    setStyleSheet(css);

    if (m_searchIconAction) {
        m_searchIconAction->setIcon(FlatIcons::icon(FlatIcon::Search, t.textMuted, t.textSecondary, t.textMuted, 14));
    }
    if (m_pauseBtn) {
        m_pauseBtn->setIcon(FlatIcons::toggleIcon(
            FlatIcon::Pause, t.textSecondary,
            FlatIcon::Play, t.warning,
            t.textPrimary, 15));
    }
    if (m_pauseIconLabel) {
        m_pauseIconLabel->setPixmap(FlatIcons::pixmap(FlatIcon::Pause, t.warning, 14, devicePixelRatioF()));
    }
    if (m_pauseLabel) {
        QPalette pp = appPal;
        pp.setColor(QPalette::WindowText, t.textPrimary);
        m_pauseLabel->setPalette(pp);
    }
    if (m_clearBtn) {
        m_clearBtn->setIcon(FlatIcons::icon(FlatIcon::Trash, t.textSecondary, t.danger, t.textMuted, 15));
    }
    if (m_configBtn) {
        m_configBtn->setIcon(FlatIcons::icon(FlatIcon::Settings, t.textSecondary, t.textPrimary, t.textMuted, 15));
    }
    if (m_closeBtn) {
        m_closeBtn->setIcon(FlatIcons::icon(FlatIcon::Close, t.textSecondary, t.danger, t.textMuted, 15));
    }
    if (m_statusLabel) {
        QPalette sp = appPal;
        sp.setColor(QPalette::WindowText, t.textMuted);
        m_statusLabel->setPalette(sp);
    }
    if (m_sampleList) {
        m_sampleList->refreshTheme(t);
    }
    if (m_plainTextSwitch) {
        m_plainTextSwitch->update();
    }

    m_applyingTheme = false;
    updatePlainTextUi();
    update();
}

void TooltipPreviewWidget::updatePauseUi()
{
    if (m_pauseBtn) {
        QSignalBlocker blk(m_pauseBtn);
        m_pauseBtn->setChecked(m_paused);
    }
    if (m_pauseBanner) {
        m_pauseBanner->setVisible(m_paused);
    }
}

void TooltipPreviewWidget::updatePlainTextUi()
{
    if (!m_plainTextHintLabel) {
        return;
    }
    const ThemeTokens t = ThemeTokens::fromPalette(QApplication::palette());
    m_plainTextHintLabel->setText(m_plainText ? tr("Solo texto plano") : tr("Con formato original"));
    if (m_plainText) {
        m_plainTextHintLabel->setStyleSheet(
            QStringLiteral("QLabel { color: %1; background-color: %2; "
                           "border: none; border-radius: 9px; padding: 2px 8px; font-weight: 600; }")
                .arg(ThemeStyle::rgbaCss(t.accent), ThemeStyle::rgbaCss(t.accentSoft)));
    } else {
        m_plainTextHintLabel->setStyleSheet(
            QStringLiteral("QLabel { color: %1; background-color: transparent; "
                           "border: none; border-radius: 9px; padding: 2px 6px; }")
                .arg(ThemeStyle::rgbaCss(t.textMuted)));
    }
}

void TooltipPreviewWidget::updateStatusLabel()
{
    if (!m_statusLabel || !m_sampleList) {
        return;
    }
    QString text = tr("%1 elementos").arg(m_sampleList->totalCount());
    const int starred = m_sampleList->starredCount();
    if (starred > 0) {
        text += tr(" (%1 fijados)").arg(starred);
    }
    text += u"  ·  "_s + tr("Pasa el cursor para probar");
    m_statusLabel->setText(text);
}

#include "tooltippreviewwidget.moc"
