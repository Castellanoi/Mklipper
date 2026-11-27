/*
    SPDX-FileCopyrightText: 2004 Esben Mose Hansen <kde@mosehansen.dk>
    SPDX-FileCopyrightText: 2024 Fushan Wen <qydwhotmail@gmail.com>
    SPDX-FileCopyrightText: 2026 Mklipper contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "klipperpopup.h"

#include "flaticons.h"
#include "historyitem.h"
#include "historymodel.h"
#include "hovertooltip.h"
#include "previewwidget.h"
#include "qrcodewidget.h"
#include "settings.h"
#include "sourceinfo.h"
#include "textutils.h"
#include "themestyle.h"
#include "x11paster.h"

#include <QAbstractButton>
#include <QAction>
#include <QApplication>
#include <QButtonGroup>
#include <QCache>
#include <QCursor>
#include <QDateTime>
#include <QDesktopServices>
#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QImageReader>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScreen>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QStyle>
#include <QStyledItemDelegate>
#include <QTimer>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;

namespace
{
constexpr int kPopupWidth = 424;
constexpr int kPopupHeight = 508;
constexpr int kRowHeightText = 54;
constexpr int kRowHeightImage = 76;
constexpr int kActionBtnSize = 26;
constexpr int kActionBtnGap = 2;
constexpr qint64 kHighlightHoldMs = 650;
constexpr qint64 kHighlightFadeMs = 2000;
constexpr qint64 kHighlightTotalMs = kHighlightHoldMs + kHighlightFadeMs;
constexpr qint64 kActionBarAnimDurationMs = 180;

enum class RowAction {
    None,
    ToggleMenu,
    OpenSource,
    Star,
    Action,
    Preview,
    SaveImage,
    Qr,
    Edit,
    Delete,
};

qreal easeOutCubic(qreal t)
{
    const qreal clamped = qBound<qreal>(0.0, t, 1.0);
    const qreal inv = 1.0 - clamped;
    return 1.0 - inv * inv * inv;
}

bool isSafeExternalHttpUrl(const QUrl &url)
{
    if (!url.isValid() || url.host().isEmpty()) {
        return false;
    }
    const QString scheme = url.scheme().toLower();
    return scheme == u"http"_s || scheme == u"https"_s;
}

// Fast O(1)-bounded extraction of first 2 non-empty preview lines (avoids splitting multi-MB strings on paint)
void extractFirstTwoLines(const QString &rawText, QString &line1, QString &line2)
{
    constexpr qsizetype kMaxScanChars = 2048;
    constexpr qsizetype kMaxLineChars = 256;
    const QStringView view(rawText.constData(), qMin(rawText.size(), kMaxScanChars));

    qsizetype pos = 0;
    const qsizetype len = view.size();
    while (pos < len && line2.isEmpty()) {
        const qsizetype nl = view.indexOf(QLatin1Char('\n'), pos);
        const qsizetype end = (nl < 0) ? len : nl;
        const QStringView slice = view.sliced(pos, qMin(end - pos, kMaxLineChars));
        const QString simplified = slice.toString().simplified();
        if (!simplified.isEmpty()) {
            if (line1.isEmpty()) {
                line1 = simplified;
            } else {
                line2 = simplified;
                break;
            }
        }
        if (nl < 0) {
            break;
        }
        pos = nl + 1;
    }
}
} // namespace

// ---------------------------------------------------------------------------
// HistoryFilterProxyModel
// ---------------------------------------------------------------------------
HistoryFilterProxyModel::HistoryFilterProxyModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{
    setFilterCaseSensitivity(Qt::CaseInsensitive);
    setDynamicSortFilter(true);
}

void HistoryFilterProxyModel::setCategoryFilter(CategoryFilter filter)
{
    if (m_category == filter) {
        return;
    }
    m_category = filter;
    invalidateFilter();
}

void HistoryFilterProxyModel::setStarredOnly(bool starredOnly)
{
    setCategoryFilter(starredOnly ? CategoryFilter::Starred : CategoryFilter::All);
}

void HistoryFilterProxyModel::setSearchQuery(const QString &query)
{
    if (m_rawQuery == query) {
        return;
    }
    m_rawQuery = query;
    m_foldedQuery = MklipperText::foldSearchText(query);
    m_foldedTokens = MklipperText::splitSearchTokens(m_foldedQuery);
    invalidateFilter();
}

bool HistoryFilterProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    const QModelIndex idx = sourceModel()->index(sourceRow, 0, sourceParent);
    if (!idx.isValid()) {
        return false;
    }

    const auto type = static_cast<HistoryItemType>(idx.data(HistoryModel::TypeIntRole).toInt());
    const QString text = idx.data(Qt::DisplayRole).toString();

    switch (m_category) {
    case CategoryFilter::All:
        break;
    case CategoryFilter::Text:
        if (type == HistoryItemType::Image) {
            return false;
        }
        break;
    case CategoryFilter::Images:
        if (type != HistoryItemType::Image) {
            return false;
        }
        break;
    case CategoryFilter::Links:
        if (type != HistoryItemType::Url
            && (type == HistoryItemType::Image || !MklipperText::isExactUrl(text))) {
            return false;
        }
        break;
    case CategoryFilter::Starred:
        if (!idx.data(HistoryModel::StarredRole).toBool()) {
            return false;
        }
        break;
    }

    if (m_foldedTokens.isEmpty()) {
        return true;
    }

    // Accent- and case-insensitive match across text + source metadata (fold text ONCE)
    const QString foldedText = MklipperText::foldSearchText(text);
    bool allInText = true;
    for (const QString &token : m_foldedTokens) {
        if (!foldedText.contains(token)) {
            allInText = false;
            break;
        }
    }
    if (allInText) {
        return true;
    }

    const QString srcApp = idx.data(HistoryModel::SourceAppRole).toString();
    const QString srcTitle = idx.data(HistoryModel::SourceTitleRole).toString();
    const QString srcUrl = idx.data(HistoryModel::SourceUrlRole).toString();
    if (srcApp.isEmpty() && srcTitle.isEmpty() && srcUrl.isEmpty()) {
        return false;
    }
    const QString foldedMeta = MklipperText::foldSearchText(
        srcApp + QLatin1Char(' ') + srcTitle + QLatin1Char(' ') + srcUrl);
    for (const QString &token : m_foldedTokens) {
        if (!foldedText.contains(token) && !foldedMeta.contains(token)) {
            return false;
        }
    }
    return true;
}

// ---------------------------------------------------------------------------
// ClipboardItemDelegate
// ---------------------------------------------------------------------------
class ClipboardItemDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    explicit ClipboardItemDelegate(QObject *parent = nullptr)
        : QStyledItemDelegate(parent)
        , m_tokens(ThemeTokens::fromPalette(QApplication::palette()))
        , m_thumbCache(64)
    {
    }

    void refreshTheme(const ThemeTokens &tokens)
    {
        m_tokens = tokens;
    }

    static QRect cardRectFor(const QRect &itemRect)
    {
        return itemRect.adjusted(4, 2, -4, -3);
    }

    QModelIndex hoveredIndex() const { return m_hoveredIndex; }

    void setHoveredIndex(const QModelIndex &idx, const QPoint &pos)
    {
        m_hoveredIndex = idx;
        m_hoverPos = pos;
    }

    void setNewHighlights(QHash<QString, qreal> intensities)
    {
        m_highlightIntensities = std::move(intensities);
    }

    qreal highlightIntensityFor(const QString &uuid) const
    {
        return m_highlightIntensities.value(uuid, 0.0);
    }

    static bool usesMenuMode(ActionBarMode mode)
    {
        return mode == ActionBarMode::MenuHoverOrClick || mode == ActionBarMode::MenuClickOnly;
    }

    void setActionBarMode(ActionBarMode mode)
    {
        m_actionBarMode = mode;
    }

    ActionBarMode actionBarMode() const { return m_actionBarMode; }

    void setActionBarProgress(const QHash<QString, qreal> &progress)
    {
        m_actionBarProgress = progress;
    }

    void setPinnedMenuUuids(const QSet<QString> &pinned)
    {
        m_pinnedMenuUuids = pinned;
    }

    bool isMenuPinned(const QString &uuid) const
    {
        return !uuid.isEmpty() && m_pinnedMenuUuids.contains(uuid);
    }

    qreal openProgressFor(const QString &uuid) const
    {
        if (m_actionBarMode == ActionBarMode::AlwaysVisible) {
            return 1.0;
        }
        return qBound<qreal>(0.0, m_actionBarProgress.value(uuid, 0.0), 1.0);
    }

    static QRect menuButtonRect(const QRect &cardRect)
    {
        const int right = cardRect.right() - 8;
        const int cy = cardRect.center().y() - kActionBtnSize / 2;
        return QRect(right - kActionBtnSize, cy, kActionBtnSize, kActionBtnSize);
    }

    static qreal captureTimestampFor(const QModelIndex &index)
    {
        const qreal added = index.data(HistoryModel::AddedTimeRole).toDouble();
        return added > 0.0 ? added : index.data(HistoryModel::LastUsedTimeRole).toDouble();
    }

    static QString compactSourceFor(const QModelIndex &index)
    {
        QStringList parts;
        if (MklipperSettings::self()->showSourceInfo()) {
            const QString src = formatCompactSourceLabel(index.data(HistoryModel::SourceAppRole).toString(),
                                                         index.data(HistoryModel::SourceTitleRole).toString(),
                                                         index.data(HistoryModel::SourceUrlRole).toString());
            if (!src.isEmpty()) {
                parts.append(src);
            }
        }
        const QString rel = MklipperText::formatRelativeTime(captureTimestampFor(index));
        if (!rel.isEmpty()) {
            parts.append(rel);
        }
        return parts.join(u" · "_s);
    }

    static QString sourceTooltipFor(const QModelIndex &index)
    {
        const QString app = index.data(HistoryModel::SourceAppRole).toString();
        const QString title = index.data(HistoryModel::SourceTitleRole).toString();
        const QString url = index.data(HistoryModel::SourceUrlRole).toString();
        const QString rel = MklipperText::formatRelativeTime(captureTimestampFor(index));
        return HoverTooltip::formatSourceTooltip(app, title, url, rel);
    }

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        const auto type = static_cast<HistoryItemType>(index.data(HistoryModel::TypeIntRole).toInt());
        const int lineSp = option.fontMetrics.lineSpacing();
        const bool hasMeta = !compactSourceFor(index).isEmpty();
        const int metaExtra = hasMeta ? qMax(16, lineSp) : 0;
        const int h = (type == HistoryItemType::Image)
            ? qMax(kRowHeightImage, lineSp * 2 + 34) + (hasMeta ? qMax(14, lineSp - 2) : 0)
            : qMax(kRowHeightText, lineSp * 2 + 20) + metaExtra;
        return QSize(option.rect.width(), h);
    }

    struct ButtonRect {
        RowAction action;
        QRect rect;
        QString tooltip;
        FlatIcon icon;
        bool active = false;
    };

    QList<ButtonRect> buttonRects(const QRect &cardRect, const QModelIndex &index) const
    {
        QList<ButtonRect> buttons;
        const auto type = static_cast<HistoryItemType>(index.data(HistoryModel::TypeIntRole).toInt());
        const bool starred = index.data(HistoryModel::StarredRole).toBool();
        const bool hasAction = index.data(HistoryModel::HasActionRole).toBool();
        const QString sourceUrl = index.data(HistoryModel::SourceUrlRole).toString();

        const bool hasMenu = usesMenuMode(m_actionBarMode);
        int right = hasMenu ? (menuButtonRect(cardRect).left() - kActionBtnGap) : (cardRect.right() - 8);
        const int cy = cardRect.center().y() - kActionBtnSize / 2;

        auto pushBtn = [&](RowAction act, const QString &tip, FlatIcon icon, bool active = false) {
            QRect r(right - kActionBtnSize, cy, kActionBtnSize, kActionBtnSize);
            buttons.append({act, r, tip, icon, active});
            right -= (kActionBtnSize + kActionBtnGap);
        };

        pushBtn(RowAction::Delete, tr("Eliminar"), FlatIcon::Trash);
        if (type == HistoryItemType::Image) {
            pushBtn(RowAction::SaveImage, tr("Guardar imagen como PNG…"), FlatIcon::Save);
            pushBtn(RowAction::Preview, tr("Vista previa (Espacio)"), FlatIcon::Eye);
        } else {
            const QString text = index.data(Qt::DisplayRole).toString();
            if (type == HistoryItemType::Text) {
                pushBtn(RowAction::Edit, tr("Editar texto"), FlatIcon::Edit);
            }
            if (text.size() > 60 || text.contains(QLatin1Char('\n'))) {
                pushBtn(RowAction::Preview, tr("Vista previa (Espacio)"), FlatIcon::Eye);
            }
            if (type == HistoryItemType::Text || type == HistoryItemType::Url) {
                pushBtn(RowAction::Qr, tr("Mostrar código QR"), FlatIcon::QrCode);
            }
        }
        if (hasAction) {
            pushBtn(RowAction::Action, tr("Ejecutar acción"), FlatIcon::Bolt);
        }
        if (isSafeExternalHttpUrl(QUrl(sourceUrl))) {
            pushBtn(RowAction::OpenSource, tr("Abrir enlace de origen"), FlatIcon::ExternalLink);
        }
        pushBtn(RowAction::Star,
                starred ? tr("Desfijar") : tr("Fijar elemento"),
                starred ? FlatIcon::StarFilled : FlatIcon::StarOutline,
                starred);

        return buttons;
    }

    QRect expandedPillRect(const QRect &cardRect, const QModelIndex &index) const
    {
        const auto btns = buttonRects(cardRect, index);
        if (btns.isEmpty()) {
            return {};
        }
        const int left = btns.last().rect.left() - 3;
        const int right = usesMenuMode(m_actionBarMode)
            ? (menuButtonRect(cardRect).right() + 3)
            : (btns.first().rect.right() + 3);
        const int top = btns.first().rect.top() - 2;
        const int height = btns.first().rect.height() + 4;
        return QRect(left, top, right - left + 1, height);
    }

    RowAction hitTestButton(const QRect &cardRect,
                            const QModelIndex &index,
                            const QPoint &pos,
                            QString *tooltipOut = nullptr,
                            QRect *rectOut = nullptr) const
    {
        const QString uuid = index.data(HistoryModel::UuidRole).toString();
        const bool hasMenu = usesMenuMode(m_actionBarMode);
        if (hasMenu) {
            const QRect menuRect = menuButtonRect(cardRect);
            if (menuRect.contains(pos)) {
                if (tooltipOut) {
                    *tooltipOut = isMenuPinned(uuid) ? tr("Ocultar acciones") : tr("Acciones del elemento");
                }
                if (rectOut) {
                    *rectOut = menuRect;
                }
                return RowAction::ToggleMenu;
            }
        }

        if (openProgressFor(uuid) < 0.30) {
            return RowAction::None;
        }

        const auto btns = buttonRects(cardRect, index);
        for (const auto &b : btns) {
            if (b.rect.contains(pos)) {
                if (tooltipOut) {
                    *tooltipOut = b.tooltip;
                }
                if (rectOut) {
                    *rectOut = b.rect;
                }
                return b.action;
            }
        }
        return RowAction::None;
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);

        const QRect cardRect = cardRectFor(option.rect);
        const bool isSelected = option.state & QStyle::State_Selected;
        const bool isHovered = (index == m_hoveredIndex) || (option.state & QStyle::State_MouseOver);
        const bool isStarred = index.data(HistoryModel::StarredRole).toBool();
        const bool isTopItem = (index.row() == 0);
        const auto type = static_cast<HistoryItemType>(index.data(HistoryModel::TypeIntRole).toInt());
        const QString itemUuid = index.data(HistoryModel::UuidRole).toString();
        const qreal newHi = m_highlightIntensities.isEmpty() ? 0.0 : m_highlightIntensities.value(itemUuid, 0.0);

        const ThemeTokens &t = m_tokens;

        // Minimalist Cinnamon 6.6 flat surface layering (no heavy wireframe borders)
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
        painter->fillPath(cardPath, bg);

        // Subtle fading highlight wash + border for newly added items
        if (newHi > 0.01) {
            QColor wash(t.accent.red(), t.accent.green(), t.accent.blue(), qRound(newHi * (t.isDark ? 62.0 : 44.0)));
            painter->fillPath(cardPath, wash);
            QColor glowEdge(t.accent.red(), t.accent.green(), t.accent.blue(), qRound(newHi * (t.isDark ? 195.0 : 170.0)));
            painter->setPen(QPen(glowEdge, 1.5));
            painter->drawPath(cardPath);
        } else if (isSelected) {
            QColor selEdge = t.accent;
            selEdge.setAlpha(t.isDark ? 120 : 95);
            painter->setPen(QPen(selEdge, 1.0));
            painter->drawPath(cardPath);
        } else if (!t.isDark && !isHovered) {
            // Subtle card edge on light surfaces only
            painter->setPen(QPen(t.hairline, 1.0));
            painter->drawPath(cardPath);
        }

        // Sleek accent pill on the left edge for the active top clipboard item
        if (isTopItem) {
            QPainterPath accentBar;
            accentBar.addRoundedRect(QRectF(cardRect.left() + 2.5, cardRect.top() + 9.0, 3.0, cardRect.height() - 18.0), 1.5, 1.5);
            painter->fillPath(accentBar, t.accent);
        }

        // Reserve right margin cleanly according to action bar mode
        const bool hasMenu = usesMenuMode(m_actionBarMode);
        const qreal openProg = openProgressFor(itemUuid);
        const bool paintActionBar = (openProg > 0.01);
        int rightReserved = 12;
        QList<ButtonRect> btns;
        if (m_actionBarMode == ActionBarMode::AlwaysVisible) {
            btns = buttonRects(cardRect, index);
            if (!btns.isEmpty()) {
                rightReserved = cardRect.right() - btns.last().rect.left() + 12;
            }
        } else if (hasMenu) {
            const QRect menuRect = menuButtonRect(cardRect);
            rightReserved = cardRect.right() - menuRect.left() + (isStarred ? 28 : 10);
            if (paintActionBar) {
                btns = buttonRects(cardRect, index);
            }
        } else {
            if (paintActionBar) {
                btns = buttonRects(cardRect, index);
            }
            if (isStarred) {
                rightReserved = 30;
            }
        }

        QRect contentRect = cardRect.adjusted(12, 7, -rightReserved, -7);

        // Minimalist borderless shortcut chip (1..9, 0 for 10th) when quick digit shortcuts are enabled
        if (MklipperSettings::self()->quickSelectDigits() && index.row() >= 0 && index.row() < 10) {
            const int rowNum = index.row();
            const QString badgeText = (rowNum < 9) ? QString::number(rowNum + 1) : u"0"_s;
            const int badgeW = 20;
            const int badgeH = 20;
            const QRect badgeRect(contentRect.left(),
                                  contentRect.center().y() - badgeH / 2,
                                  badgeW,
                                  badgeH);

            QPainterPath badgePath;
            badgePath.addRoundedRect(QRectF(badgeRect), 6.0, 6.0);
            QColor badgeBg = isTopItem ? t.accent
                                       : QColor(t.textPrimary.red(), t.textPrimary.green(), t.textPrimary.blue(), t.isDark ? 24 : 18);
            painter->fillPath(badgePath, badgeBg);

            QFont badgeFont = option.font;
            badgeFont.setPointSizeF(qMax(8.0, badgeFont.pointSizeF() - 1.4));
            badgeFont.setBold(true);
            painter->setFont(badgeFont);
            painter->setPen(isTopItem ? t.accentFg : t.textSecondary);
            painter->drawText(badgeRect, Qt::AlignCenter, badgeText);

            contentRect.setLeft(badgeRect.right() + 10);
        }

        // Optional compact source metadata row
        const QString sourceLabel = compactSourceFor(index);
        const bool hasSourceUrl = isSafeExternalHttpUrl(QUrl(index.data(HistoryModel::SourceUrlRole).toString()));
        QRect metaRect;
        if (!sourceLabel.isEmpty()) {
            const int metaH = qMax(15, option.fontMetrics.height() - 1);
            metaRect = QRect(contentRect.left(), contentRect.bottom() - metaH + 1, contentRect.width(), metaH);
            contentRect.setBottom(metaRect.top() - 2);
        }

        if (type == HistoryItemType::Image) {
            const QUrl imgUrl = index.data(HistoryModel::ImageUrlRole).toUrl();
            const QString uuid = index.data(HistoryModel::UuidRole).toString();
            const int maxThumbH = qMax(36, contentRect.height() - 2);
            const qreal dpr = painter->device() ? qMax<qreal>(1.0, painter->device()->devicePixelRatioF()) : 1.0;
            QPixmap thumb = loadThumbnail(uuid, imgUrl.toLocalFile(), QSize(92, maxThumbH), dpr);

            if (!thumb.isNull()) {
                const QSize logicalThumb = thumb.deviceIndependentSize().toSize();
                const QRect thumbRect(contentRect.left(),
                                      contentRect.center().y() - logicalThumb.height() / 2,
                                      logicalThumb.width(),
                                      logicalThumb.height());
                painter->save();
                QPainterPath thumbClip;
                thumbClip.addRoundedRect(QRectF(thumbRect), 6.0, 6.0);
                painter->setClipPath(thumbClip);
                painter->drawPixmap(thumbRect, thumb);
                painter->restore();
                painter->setPen(QPen(t.hairline, 1.0));
                painter->drawRoundedRect(QRectF(thumbRect).adjusted(0.5, 0.5, -0.5, -0.5), 6.0, 6.0);
                contentRect.setLeft(thumbRect.right() + 10);
            }

            painter->setPen(t.textPrimary);
            QFont f = option.font;
            f.setBold(isTopItem);
            painter->setFont(f);
            const QString label = index.data(Qt::DisplayRole).toString();
            painter->drawText(contentRect, Qt::AlignVCenter | Qt::AlignLeft,
                              painter->fontMetrics().elidedText(label, Qt::ElideRight, contentRect.width()));
        } else {
            const QString rawText = index.data(Qt::DisplayRole).toString();
            if (type == HistoryItemType::Url) {
                const QRectF iconRect(contentRect.left(), contentRect.center().y() - 7.0, 14.0, 14.0);
                FlatIcons::paint(painter, FlatIcon::Link, iconRect, t.accent);
                contentRect.setLeft(qRound(iconRect.right()) + 7);
            }

            QString line1;
            QString line2;
            extractFirstTwoLines(rawText, line1, line2);

            painter->setPen(t.textPrimary);
            QFont mainFont = option.font;
            if (isTopItem) {
                mainFont.setBold(true);
            }
            painter->setFont(mainFont);
            const QFontMetrics fm1(mainFont);

            if (line2.isEmpty()) {
                painter->drawText(contentRect, Qt::AlignVCenter | Qt::AlignLeft,
                                  fm1.elidedText(line1, Qt::ElideRight, contentRect.width()));
            } else {
                const int halfH = contentRect.height() / 2;
                QRect r1(contentRect.left(), contentRect.top(), contentRect.width(), halfH);
                QRect r2(contentRect.left(), contentRect.top() + halfH, contentRect.width(), contentRect.height() - halfH);
                painter->drawText(r1, Qt::AlignVCenter | Qt::AlignLeft,
                                  fm1.elidedText(line1, Qt::ElideRight, r1.width()));

                QFont subFont = option.font;
                subFont.setPointSizeF(qMax(8.0, subFont.pointSizeF() - 1.0));
                painter->setFont(subFont);
                painter->setPen(t.textSecondary);
                const QFontMetrics fm2(subFont);
                painter->drawText(r2, Qt::AlignVCenter | Qt::AlignLeft,
                                  fm2.elidedText(line2, Qt::ElideRight, r2.width()));
            }
        }

        // Subtle source metadata line
        if (!sourceLabel.isEmpty() && metaRect.isValid() && metaRect.width() > 24) {
            QFont metaFont = option.font;
            metaFont.setPointSizeF(qMax(7.5, metaFont.pointSizeF() - 1.7));
            painter->setFont(metaFont);

            const int dotSize = 5;
            const QRectF dotRect(metaRect.left(), metaRect.center().y() - dotSize / 2.0, dotSize, dotSize);
            QColor dotColor = hasSourceUrl ? t.accent : t.textMuted;
            painter->setPen(Qt::NoPen);
            painter->setBrush(dotColor);
            painter->drawEllipse(dotRect);
            painter->setBrush(Qt::NoBrush);

            painter->setPen(t.textMuted);
            const QRect textMetaRect = metaRect.adjusted(dotSize + 5, 0, 0, 0);
            const QFontMetrics fmMeta(metaFont);
            painter->drawText(textMetaRect, Qt::AlignVCenter | Qt::AlignLeft,
                              fmMeta.elidedText(sourceLabel, Qt::ElideRight, textMetaRect.width()));
        }

        // Persistent star & new-item indicators when action bar is collapsed
        if (!paintActionBar) {
            const qreal rightEdge = hasMenu ? static_cast<qreal>(menuButtonRect(cardRect).left() - 4)
                                            : static_cast<qreal>(cardRect.right() - 10);
            if (isStarred) {
                const QRectF starRect(rightEdge - 14.0, cardRect.center().y() - 7.0, 14.0, 14.0);
                FlatIcons::paint(painter, FlatIcon::StarFilled, starRect, t.warning);
            }
            if (newHi > 0.02) {
                const qreal dotX = isStarred ? (rightEdge - 26.0) : (rightEdge - 8.0);
                const QRectF newDotRect(dotX, cardRect.center().y() - 3.5, 7.0, 7.0);
                QColor dotCol = t.accent;
                dotCol.setAlpha(qRound(newHi * 235.0));
                painter->setPen(Qt::NoPen);
                painter->setBrush(dotCol);
                painter->drawEllipse(newDotRect);
                painter->setBrush(Qt::NoBrush);
            }
        }

        // Animated sweeping flat action bar (slides smoothly leftward out of the 3-dot trigger or right edge)
        const QRect menuRect = menuButtonRect(cardRect);
        if (paintActionBar && !btns.isEmpty()) {
            const qreal eased = easeOutCubic(openProg);
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
            pillPath.addRoundedRect(QRectF(animPillRect).adjusted(0.5, 0.5, -0.5, -0.5), 8.0, 8.0);
            painter->fillPath(pillPath, pillBg);
            painter->strokePath(pillPath, QPen(t.borderSubtle, 1.0));

            // Clip action buttons inside the opening tray (to the left of the 3-dot trigger) and sweep them leftward
            const int clipRight = hasMenu ? menuRect.left() : (animPillRect.right() + 1);
            if (clipRight > animPillRect.left()) {
                painter->save();
                QPainterPath clipPath;
                clipPath.addRoundedRect(QRectF(animPillRect).adjusted(1.0, 1.0, -1.0, -1.0), 7.5, 7.5);
                painter->setClipPath(clipPath);
                painter->setClipRect(QRect(animPillRect.left(), animPillRect.top(), clipRight - animPillRect.left(), animPillRect.height()),
                                     Qt::IntersectClip);

                const int maxSlide = qMin(48, qMax(0, collapsedLeft - expandedLeft));
                const int slideDx = qRound((1.0 - eased) * maxSlide);
                painter->setOpacity(qBound<qreal>(0.0, openProg * 1.35, 1.0));

                for (const auto &b : btns) {
                    const QRect drawRect = b.rect.translated(slideDx, 0);
                    const bool btnHover = (openProg >= 0.30) && isHovered && b.rect.contains(m_hoverPos);
                    if (btnHover) {
                        QPainterPath bp;
                        bp.addRoundedRect(QRectF(drawRect), 6.0, 6.0);
                        QColor hbg = (b.action == RowAction::Delete) ? t.dangerSoft
                                   : (b.action == RowAction::Star)   ? t.warningSoft
                                                                     : t.surfaceHoverBg;
                        painter->fillPath(bp, hbg);
                    }

                    QColor iconColor = t.textSecondary;
                    if (b.active) {
                        iconColor = t.warning;
                    } else if (btnHover && b.action == RowAction::Delete) {
                        iconColor = t.danger;
                    } else if (btnHover && b.action == RowAction::Star) {
                        iconColor = t.warning;
                    } else if (btnHover) {
                        iconColor = t.textPrimary;
                    }

                    const QRectF iconRect(drawRect.center().x() - 7.5, drawRect.center().y() - 7.5, 15.0, 15.0);
                    FlatIcons::paint(painter, b.icon, iconRect, iconColor);
                }
                painter->restore();
            }
        }

        // Vertical 3-dot menu button anchored on the right edge in menu modes
        if (hasMenu) {
            const bool pinned = isMenuPinned(itemUuid);
            const bool menuHover = isHovered && menuRect.contains(m_hoverPos);
            if (pinned || menuHover) {
                QPainterPath mp;
                mp.addRoundedRect(QRectF(menuRect), 6.0, 6.0);
                painter->fillPath(mp, pinned ? t.accentSoft : t.surfaceHoverBg);
            }
            const QColor dotColor = pinned                   ? t.accent
                                  : menuHover                ? t.textPrimary
                                  : (isHovered || isSelected) ? t.textSecondary
                                                              : t.textMuted;
            const QRectF dotIconRect(menuRect.center().x() - 7.5, menuRect.center().y() - 7.5, 15.0, 15.0);
            FlatIcons::paint(painter, FlatIcon::MoreVertical, dotIconRect, dotColor);
        }

        painter->restore();
    }

private:
    QPixmap loadThumbnail(const QString &uuid, const QString &filePath, const QSize &maxLogicalSize, qreal dpr) const
    {
        if (filePath.isEmpty()) {
            return {};
        }
        const qreal safeDpr = qMax<qreal>(1.0, dpr);
        const QSize targetPx(qMax(1, qRound(maxLogicalSize.width() * safeDpr)),
                             qMax(1, qRound(maxLogicalSize.height() * safeDpr)));
        const QString cacheKey = uuid + QLatin1Char('@') + QString::number(targetPx.width()) + QLatin1Char('x') + QString::number(targetPx.height());
        if (auto *cached = m_thumbCache.object(cacheKey)) {
            QPixmap copy = *cached;
            copy.setDevicePixelRatio(safeDpr);
            return copy;
        }
        QImageReader reader(filePath);
        reader.setAutoTransform(true);
        const QSize orig = reader.size();
        if (orig.isValid()) {
            reader.setScaledSize(orig.scaled(targetPx, Qt::KeepAspectRatio));
        }
        const QImage img = reader.read();
        if (img.isNull()) {
            return {};
        }
        auto *pix = new QPixmap(QPixmap::fromImage(img.scaled(targetPx, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
        QPixmap copy = *pix;
        m_thumbCache.insert(cacheKey, pix);
        copy.setDevicePixelRatio(safeDpr);
        return copy;
    }

    ThemeTokens m_tokens;
    QPersistentModelIndex m_hoveredIndex;
    QPoint m_hoverPos;
    QHash<QString, qreal> m_highlightIntensities;
    ActionBarMode m_actionBarMode = ActionBarMode::MenuHoverOrClick;
    QHash<QString, qreal> m_actionBarProgress;
    QSet<QString> m_pinnedMenuUuids;
    mutable QCache<QString, QPixmap> m_thumbCache;
};

// ---------------------------------------------------------------------------
// PlainTextSwitch — modern Cinnamon 6.6 flat toggle switch
// ---------------------------------------------------------------------------
class PlainTextSwitch : public QAbstractButton
{
public:
    explicit PlainTextSwitch(const QString &text, QWidget *parent = nullptr)
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
    static int trackHeightFor(const QFontMetrics &fm) { return qBound(18, fm.height() + 1, 24); }
    static int trackWidthFor(int trackH) { return qRound(trackH * 1.8); }
    static constexpr int kSpacing = 9;
};

// ---------------------------------------------------------------------------
// KlipperPopup
// ---------------------------------------------------------------------------
KlipperPopup::KlipperPopup(QWidget *parent)
    : QFrame(parent, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint)
    , m_model(HistoryModel::self())
    , m_paster(new X11Paster(this))
{
    setObjectName(u"mklipperPopup"_s);
    setWindowTitle(tr("Mklipper — Portapapeles"));
    setFrameShape(QFrame::NoFrame);
    setFixedSize(kPopupWidth, kPopupHeight);

    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(1, 1, 1, 1);
    rootLayout->setSpacing(0);

    m_stack = new QStackedWidget(this);
    rootLayout->addWidget(m_stack);

    // ======================== Page 0: History List ========================
    auto *listPage = new QWidget(this);
    auto *listLayout = new QVBoxLayout(listPage);
    listLayout->setContentsMargins(12, 12, 12, 10);
    listLayout->setSpacing(8);

    // Top bar: minimalist search input + borderless flat icon buttons
    auto *topBar = new QHBoxLayout;
    topBar->setSpacing(4);

    const int toolBtnSide = qMax(34, fontMetrics().height() + 16);

    m_searchEdit = new QLineEdit(listPage);
    m_searchEdit->setObjectName(u"mklipperSearch"_s);
    m_searchEdit->setPlaceholderText(tr("Buscar en el historial…"));
    m_searchEdit->setClearButtonEnabled(true);
    m_searchIconAction = m_searchEdit->addAction(QIcon(), QLineEdit::LeadingPosition);
    m_searchEdit->setFixedHeight(toolBtnSide);
    m_searchEdit->installEventFilter(this);

    m_pauseBtn = new QToolButton(listPage);
    m_pauseBtn->setObjectName(u"mklipperStarBtn"_s);
    m_pauseBtn->setCheckable(true);
    m_pauseBtn->setIconSize(QSize(16, 16));
    m_pauseBtn->setCursor(Qt::PointingHandCursor);
    m_pauseBtn->setFixedSize(toolBtnSide, toolBtnSide);

    m_clearBtn = new QToolButton(listPage);
    m_clearBtn->setObjectName(u"mklipperDangerBtn"_s);
    m_clearBtn->setIconSize(QSize(16, 16));
    m_clearBtn->setCursor(Qt::PointingHandCursor);
    m_clearBtn->setFixedSize(toolBtnSide, toolBtnSide);

    m_configBtn = new QToolButton(listPage);
    m_configBtn->setObjectName(u"mklipperIconBtn"_s);
    m_configBtn->setIconSize(QSize(16, 16));
    m_configBtn->setCursor(Qt::PointingHandCursor);
    m_configBtn->setFixedSize(toolBtnSide, toolBtnSide);

    m_closeBtn = new QToolButton(listPage);
    m_closeBtn->setObjectName(u"mklipperDangerBtn"_s);
    m_closeBtn->setIconSize(QSize(16, 16));
    m_closeBtn->setCursor(Qt::PointingHandCursor);
    m_closeBtn->setFixedSize(toolBtnSide, toolBtnSide);
    connect(m_closeBtn, &QToolButton::clicked, this, &QWidget::hide);

    topBar->addWidget(m_searchEdit, 1);
    topBar->addWidget(m_pauseBtn);
    topBar->addWidget(m_clearBtn);
    topBar->addWidget(m_configBtn);
    topBar->addWidget(m_closeBtn);
    listLayout->addLayout(topBar);

    // Quick category filter pills (All · Text · Images · Links · Pinned)
    auto *filterBar = new QHBoxLayout;
    filterBar->setContentsMargins(0, 0, 0, 0);
    filterBar->setSpacing(4);

    m_categoryGroup = new QButtonGroup(this);
    m_categoryGroup->setExclusive(true);

    auto makeFilterChip = [&](const QString &label, HistoryFilterProxyModel::CategoryFilter cat) {
        auto *btn = new QPushButton(label, listPage);
        btn->setProperty("filterChip", true);
        btn->setCheckable(true);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setFocusPolicy(Qt::NoFocus);
        QFont cf = btn->font();
        cf.setPointSizeF(qMax(8.0, cf.pointSizeF() - 1.2));
        btn->setFont(cf);
        m_categoryGroup->addButton(btn, static_cast<int>(cat));
        filterBar->addWidget(btn);
        return btn;
    };

    m_filterAllBtn = makeFilterChip(tr("Todos"), HistoryFilterProxyModel::CategoryFilter::All);
    m_filterTextBtn = makeFilterChip(tr("Texto"), HistoryFilterProxyModel::CategoryFilter::Text);
    m_filterImagesBtn = makeFilterChip(tr("Imágenes"), HistoryFilterProxyModel::CategoryFilter::Images);
    m_filterLinksBtn = makeFilterChip(tr("Enlaces"), HistoryFilterProxyModel::CategoryFilter::Links);
    m_filterStarredBtn = makeFilterChip(tr("Fijados"), HistoryFilterProxyModel::CategoryFilter::Starred);
    m_filterAllBtn->setChecked(true);
    filterBar->addStretch();
    listLayout->addLayout(filterBar);

    // Quick options strip: Plain-text paste toggle switch
    auto *modeBar = new QHBoxLayout;
    modeBar->setContentsMargins(2, 0, 2, 0);
    modeBar->setSpacing(6);

    m_plainTextSwitch = new PlainTextSwitch(tr("Pegar sin formato"), listPage);
    m_plainTextSwitch->setChecked(MklipperSettings::self()->pastePlainText());
    connect(m_plainTextSwitch, &QAbstractButton::toggled, this, [](bool checked) {
        auto *s = MklipperSettings::self();
        if (s->pastePlainText() != checked) {
            s->setPastePlainText(checked);
            s->save();
        }
    });

    m_plainTextHintLabel = new QLabel(listPage);
    QFont hintFont = m_plainTextHintLabel->font();
    hintFont.setPointSizeF(qMax(8.0, hintFont.pointSizeF() - 1.5));
    m_plainTextHintLabel->setFont(hintFont);
    m_plainTextHintLabel->setTextFormat(Qt::PlainText);
    m_plainTextHintLabel->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);

    modeBar->addWidget(m_plainTextSwitch);
    modeBar->addStretch();
    modeBar->addWidget(m_plainTextHintLabel);
    listLayout->addLayout(modeBar);

    // Capture paused banner (incognito mode)
    auto *pauseBanner = new QFrame(listPage);
    pauseBanner->setObjectName(u"mklipperPauseBanner"_s);
    pauseBanner->setFrameShape(QFrame::NoFrame);
    pauseBanner->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
    m_pauseBanner = pauseBanner;
    auto *pauseLayout = new QHBoxLayout(m_pauseBanner);
    pauseLayout->setContentsMargins(10, 6, 8, 6);
    pauseLayout->setSpacing(8);

    m_pauseIconLabel = new QLabel(m_pauseBanner);
    m_pauseIconLabel->setFixedSize(16, 16);
    m_pauseLabel = new QLabel(tr("Captura en pausa (modo incógnito)"), m_pauseBanner);
    m_pauseLabel->setWordWrap(true);
    auto *resumeBtn = new QPushButton(tr("Reanudar"), m_pauseBanner);
    resumeBtn->setProperty("ghost", true);
    resumeBtn->setCursor(Qt::PointingHandCursor);
    connect(resumeBtn, &QPushButton::clicked, this, [this]() {
        m_model->setCapturePaused(false);
    });
    pauseLayout->addWidget(m_pauseIconLabel);
    pauseLayout->addWidget(m_pauseLabel, 1);
    pauseLayout->addWidget(resumeBtn);
    m_pauseBanner->setVisible(false);
    listLayout->addWidget(m_pauseBanner);

    // Password warning banner (flat vector lock icon, no emoji)
    auto *pwBanner = new QFrame(listPage);
    pwBanner->setObjectName(u"mklipperPasswordBanner"_s);
    pwBanner->setFrameShape(QFrame::NoFrame);
    pwBanner->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
    m_passwordBanner = pwBanner;
    auto *pwLayout = new QHBoxLayout(m_passwordBanner);
    pwLayout->setContentsMargins(10, 6, 8, 6);
    pwLayout->setSpacing(8);

    m_pwIconLabel = new QLabel(m_passwordBanner);
    m_pwIconLabel->setFixedSize(16, 16);
    m_pwLabel = new QLabel(tr("El portapapeles contiene una contraseña (no guardada)"), m_passwordBanner);
    m_pwLabel->setWordWrap(true);
    auto *pwClearBtn = new QPushButton(tr("Limpiar"), m_passwordBanner);
    pwClearBtn->setProperty("ghost", true);
    pwClearBtn->setCursor(Qt::PointingHandCursor);
    connect(pwClearBtn, &QPushButton::clicked, this, [this]() {
        m_model->clearPasswords();
    });
    pwLayout->addWidget(m_pwIconLabel);
    pwLayout->addWidget(m_pwLabel, 1);
    pwLayout->addWidget(pwClearBtn);
    m_passwordBanner->setVisible(false);
    listLayout->addWidget(m_passwordBanner);

    // Subtle hairline separating header controls from history list (no nested box frame)
    auto *topDivider = new QFrame(listPage);
    topDivider->setObjectName(u"mklipperDivider"_s);
    listLayout->addWidget(topDivider);

    // Proxy & ListView / Empty placeholder inside QStackedWidget so header/footer never shift
    m_proxy = new HistoryFilterProxyModel(this);
    m_proxy->setSourceModel(m_model.get());

    m_delegate = new ClipboardItemDelegate(this);

    m_listStack = new QStackedWidget(listPage);
    m_listStack->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    m_listView = new QListView(m_listStack);
    m_listView->setModel(m_proxy);
    m_listView->setItemDelegate(m_delegate);
    m_listView->setMouseTracking(true);
    m_listView->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_listView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_listView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_listView->setFrameShape(QFrame::NoFrame);
    m_listView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_listView->setStyleSheet(u"QListView { background: transparent; border: none; outline: none; }"_s);
    m_listView->viewport()->setMouseTracking(true);
    m_listView->viewport()->installEventFilter(this);
    m_listView->installEventFilter(this);

    auto *emptyPage = new QWidget(m_listStack);
    emptyPage->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    auto *emptyLayout = new QVBoxLayout(emptyPage);
    emptyLayout->setContentsMargins(24, 24, 24, 24);
    emptyLayout->setSpacing(8);
    emptyLayout->addStretch();

    m_emptyBadge = new QFrame(emptyPage);
    m_emptyBadge->setFixedSize(56, 56);
    auto *badgeLayout = new QVBoxLayout(m_emptyBadge);
    badgeLayout->setContentsMargins(0, 0, 0, 0);
    m_emptyIconLabel = new QLabel(m_emptyBadge);
    m_emptyIconLabel->setAlignment(Qt::AlignCenter);
    badgeLayout->addWidget(m_emptyIconLabel);
    emptyLayout->addWidget(m_emptyBadge, 0, Qt::AlignHCenter);
    emptyLayout->addSpacing(4);

    m_emptyLabel = new QLabel(tr("El historial está vacío."), emptyPage);
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_emptyLabel->setWordWrap(true);
    m_emptyLabel->setTextFormat(Qt::PlainText);
    QFont emptyTitleFont = m_emptyLabel->font();
    emptyTitleFont.setBold(true);
    m_emptyLabel->setFont(emptyTitleFont);
    emptyLayout->addWidget(m_emptyLabel);

    m_emptySubLabel = new QLabel(tr("Copia texto, enlaces o imágenes para verlos aquí."), emptyPage);
    m_emptySubLabel->setAlignment(Qt::AlignCenter);
    m_emptySubLabel->setWordWrap(true);
    m_emptySubLabel->setTextFormat(Qt::PlainText);
    QFont emptySubFont = m_emptySubLabel->font();
    emptySubFont.setPointSizeF(qMax(8.0, emptySubFont.pointSizeF() - 1.2));
    m_emptySubLabel->setFont(emptySubFont);
    emptyLayout->addWidget(m_emptySubLabel);

    emptyLayout->addStretch();

    m_listStack->addWidget(m_listView);
    m_listStack->addWidget(emptyPage);
    listLayout->addWidget(m_listStack, 1);

    // Subtle footer hairline + status
    auto *bottomDivider = new QFrame(listPage);
    bottomDivider->setObjectName(u"mklipperDivider"_s);
    listLayout->addWidget(bottomDivider);

    m_statusLabel = new QLabel(listPage);
    QFont statusFont = m_statusLabel->font();
    statusFont.setPointSizeF(qMax(8.0, statusFont.pointSizeF() - 1.5));
    m_statusLabel->setFont(statusFont);
    m_statusLabel->setTextFormat(Qt::PlainText);
    m_statusLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    listLayout->addWidget(m_statusLabel);

    m_stack->addWidget(listPage);

    // ======================== Page 1: Edit Page ========================
    auto *editPage = new QWidget(this);
    auto *editLayout = new QVBoxLayout(editPage);
    editLayout->setContentsMargins(12, 10, 12, 12);
    editLayout->setSpacing(10);

    auto *editHeader = new QHBoxLayout;
    editHeader->setSpacing(6);
    m_editBackBtn = new QPushButton(tr("Volver"), editPage);
    m_editBackBtn->setProperty("ghost", true);
    m_editBackBtn->setIconSize(QSize(15, 15));
    m_editBackBtn->setCursor(Qt::PointingHandCursor);
    connect(m_editBackBtn, &QPushButton::clicked, this, &KlipperPopup::returnToListPage);
    m_editTitleLabel = new QLabel(tr("Editar contenido"), editPage);
    QFont etFont = m_editTitleLabel->font();
    etFont.setBold(true);
    etFont.setPointSizeF(etFont.pointSizeF() + 0.5);
    m_editTitleLabel->setFont(etFont);
    auto *editRightSpacer = new QWidget(editPage);
    editRightSpacer->setFixedWidth(m_editBackBtn->sizeHint().width());
    editHeader->addWidget(m_editBackBtn);
    editHeader->addStretch();
    editHeader->addWidget(m_editTitleLabel);
    editHeader->addStretch();
    editHeader->addWidget(editRightSpacer);
    editLayout->addLayout(editHeader);

    auto *editDivider = new QFrame(editPage);
    editDivider->setObjectName(u"mklipperDivider"_s);
    editLayout->addWidget(editDivider);

    m_editArea = new QPlainTextEdit(editPage);
    m_editArea->setObjectName(u"mklipperEditArea"_s);
    editLayout->addWidget(m_editArea, 1);

    auto *editBtns = new QHBoxLayout;
    editBtns->setSpacing(8);
    auto *cancelEditBtn = new QPushButton(tr("Cancelar"), editPage);
    cancelEditBtn->setProperty("flatAction", true);
    cancelEditBtn->setCursor(Qt::PointingHandCursor);

    m_saveEditBtn = new QPushButton(tr("Guardar"), editPage);
    m_saveEditBtn->setProperty("primary", true);
    m_saveEditBtn->setIconSize(QSize(15, 15));
    m_saveEditBtn->setCursor(Qt::PointingHandCursor);
    m_saveEditBtn->setDefault(true);

    connect(cancelEditBtn, &QPushButton::clicked, this, &KlipperPopup::returnToListPage);
    connect(m_saveEditBtn, &QPushButton::clicked, this, &KlipperPopup::saveEditedText);
    editBtns->addStretch();
    editBtns->addWidget(cancelEditBtn);
    editBtns->addWidget(m_saveEditBtn);
    editLayout->addLayout(editBtns);

    m_stack->addWidget(editPage);

    // ======================== Page 2: QR Page ========================
    m_qrWidget = new QRCodeWidget(this);
    connect(m_qrWidget, &QRCodeWidget::backRequested, this, &KlipperPopup::returnToListPage);
    m_stack->addWidget(m_qrWidget);

    // ======================== Page 3: Preview Page ========================
    m_previewWidget = new PreviewWidget(this);
    connect(m_previewWidget, &PreviewWidget::backRequested, this, &KlipperPopup::returnToListPage);
    connect(m_previewWidget, &PreviewWidget::activateRequested, this, [this](const QString &uuid) {
        activateItemByUuid(uuid, QApplication::keyboardModifiers().testFlag(Qt::ShiftModifier));
    });
    connect(m_previewWidget, &PreviewWidget::editRequested, this, &KlipperPopup::openEditPageByUuid);
    connect(m_previewWidget, &PreviewWidget::saveImageRequested, this, &KlipperPopup::saveImageByUuid);
    m_stack->addWidget(m_previewWidget);

    // Connections
    connect(m_searchEdit, &QLineEdit::textChanged, this, [this](const QString &text) {
        m_proxy->setSearchQuery(text);
        if (m_proxy->rowCount() > 0) {
            m_listView->setCurrentIndex(m_proxy->index(0, 0));
        }
        updateStatusLabel();
    });
    connect(m_categoryGroup, &QButtonGroup::idClicked, this, [this](int id) {
        setActiveCategory(static_cast<HistoryFilterProxyModel::CategoryFilter>(id));
    });
    connect(m_pauseBtn, &QToolButton::toggled, this, [this](bool checked) {
        m_model->setCapturePaused(checked);
    });
    connect(m_model.get(), &HistoryModel::capturePausedChanged, this, &KlipperPopup::updatePauseUi);
    connect(m_clearBtn, &QToolButton::clicked, this, [this]() {
        m_suppressAutoHide = true;
        m_model->clearHistoryInteractive(this);
        m_suppressAutoHide = false;
        updateStatusLabel();
        raise();
        activateWindow();
    });
    connect(m_configBtn, &QToolButton::clicked, this, [this]() {
        const auto *s = MklipperSettings::self();
        if (!s->keepPopupOpen() || s->closePopupOnFocusOut()) {
            hide();
        }
        Q_EMIT configureRequested();
    });

    connect(m_paster, &X11Paster::pasteFinished, this, &KlipperPopup::onPasteFinished);
    connect(MklipperSettings::self(), &MklipperSettings::settingsChanged, this, &KlipperPopup::updatePersistentUi);

    connect(m_model.get(), &HistoryModel::hasPasswordChanged, this, &KlipperPopup::updatePasswordBanner);
    connect(m_model.get(), &HistoryModel::rowsInserted, this, &KlipperPopup::updateStatusLabel);
    connect(m_model.get(), &HistoryModel::rowsRemoved, this, &KlipperPopup::updateStatusLabel);
    connect(m_model.get(), &HistoryModel::modelReset, this, &KlipperPopup::updateStatusLabel);
    connect(m_model.get(), &HistoryModel::starredCountChanged, this, &KlipperPopup::updateStatusLabel);

    m_relTimeTimer = new QTimer(this);
    m_relTimeTimer->setInterval(30000);
    connect(m_relTimeTimer, &QTimer::timeout, this, [this]() {
        if (isVisible() && m_listView) {
            m_listView->viewport()->update();
        }
    });

    m_highlightTimer = new QTimer(this);
    m_highlightTimer->setInterval(33);
    connect(m_highlightTimer, &QTimer::timeout, this, &KlipperPopup::tickNewItemHighlights);

    m_actionAnimTimer = new QTimer(this);
    m_actionAnimTimer->setInterval(16);
    connect(m_actionAnimTimer, &QTimer::timeout, this, &KlipperPopup::tickActionBarAnimations);

    if (m_listView && m_listView->selectionModel()) {
        connect(m_listView->selectionModel(), &QItemSelectionModel::currentChanged, this, [this]() {
            if (MklipperSettings::self()->actionBarMode() == ActionBarMode::AlwaysOnHover) {
                syncActionBarTargets();
            }
        });
    }

    m_tooltipController = new HoverTooltipController(this);
    m_tooltipController->registerWidget(
        m_pauseBtn,
        TooltipCategory::HeaderButtons,
        [this]() {
            return m_model->isCapturePaused()
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

    applyTheme();
    updatePasswordBanner();
    updatePauseUi();
    updateStatusLabel();
    if (m_listView) {
        m_listView->viewport()->update();
    }
    updatePersistentUi();
}

void KlipperPopup::refreshTheme()
{
    applyTheme();
}

void KlipperPopup::highlightNewItems(const QSet<QString> &uuids)
{
    if (uuids.isEmpty()) {
        return;
    }
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    for (const QString &uuid : uuids) {
        if (!uuid.isEmpty()) {
            m_highlightStartMs.insert(uuid, nowMs);
        }
    }
    tickNewItemHighlights();
    if (!m_highlightStartMs.isEmpty() && m_highlightTimer && !m_highlightTimer->isActive()) {
        m_highlightTimer->start();
    }
}

qreal KlipperPopup::highlightIntensityFor(const QString &uuid) const
{
    return m_delegate ? m_delegate->highlightIntensityFor(uuid) : 0.0;
}

void KlipperPopup::tickNewItemHighlights()
{
    if (m_highlightStartMs.isEmpty()) {
        if (m_highlightTimer) {
            m_highlightTimer->stop();
        }
        if (m_delegate) {
            m_delegate->setNewHighlights({});
        }
        return;
    }

    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    QHash<QString, qreal> intensities;
    intensities.reserve(m_highlightStartMs.size());

    for (auto it = m_highlightStartMs.begin(); it != m_highlightStartMs.end();) {
        const qint64 elapsed = nowMs - it.value();
        if (elapsed >= kHighlightTotalMs) {
            it = m_highlightStartMs.erase(it);
            continue;
        }
        qreal intensity = 1.0;
        if (elapsed > kHighlightHoldMs) {
            const qreal progress = qBound<qreal>(0.0, static_cast<qreal>(elapsed - kHighlightHoldMs) / static_cast<qreal>(kHighlightFadeMs), 1.0);
            const qreal inv = 1.0 - progress;
            intensity = inv * inv; // Smooth quadratic ease-out
        }
        intensities.insert(it.key(), intensity);
        ++it;
    }

    if (m_delegate) {
        m_delegate->setNewHighlights(std::move(intensities));
    }
    if (m_listView) {
        m_listView->viewport()->update();
    }
    if (m_highlightStartMs.isEmpty() && m_highlightTimer) {
        m_highlightTimer->stop();
    }
}

void KlipperPopup::syncActionBarTargets()
{
    if (!m_delegate) {
        return;
    }

    const auto *s = MklipperSettings::self();
    const ActionBarMode mode = s->actionBarMode();
    const bool animated = s->actionBarAnimation();
    m_delegate->setActionBarMode(mode);
    m_delegate->setPinnedMenuUuids(m_pinnedMenuUuids);

    if (mode == ActionBarMode::AlwaysVisible) {
        if (m_actionAnimTimer) {
            m_actionAnimTimer->stop();
        }
        m_actionBarProgress.clear();
        m_delegate->setActionBarProgress({});
        if (m_listView) {
            m_listView->viewport()->update();
        }
        return;
    }

    QSet<QString> openTargets;
    if (ClipboardItemDelegate::usesMenuMode(mode)) {
        openTargets = m_pinnedMenuUuids;
        if (mode == ActionBarMode::MenuHoverOrClick && !m_hoverMenuUuid.isEmpty()) {
            openTargets.insert(m_hoverMenuUuid);
        }
    } else if (mode == ActionBarMode::AlwaysOnHover) {
        const QModelIndex hoverIdx = m_delegate->hoveredIndex();
        if (hoverIdx.isValid()) {
            const QString u = hoverIdx.data(HistoryModel::UuidRole).toString();
            if (!u.isEmpty()) {
                openTargets.insert(u);
            }
        }
        if (m_listView) {
            const QModelIndex curIdx = m_listView->currentIndex();
            if (curIdx.isValid()) {
                const QString u = curIdx.data(HistoryModel::UuidRole).toString();
                if (!u.isEmpty()) {
                    openTargets.insert(u);
                }
            }
        }
    }

    if (!animated) {
        if (m_actionAnimTimer) {
            m_actionAnimTimer->stop();
        }
        m_actionBarProgress.clear();
        for (const QString &u : openTargets) {
            m_actionBarProgress.insert(u, 1.0);
        }
        m_delegate->setActionBarProgress(m_actionBarProgress);
        if (m_listView) {
            m_listView->viewport()->update();
        }
        return;
    }

    for (const QString &u : openTargets) {
        if (!m_actionBarProgress.contains(u)) {
            m_actionBarProgress.insert(u, 0.0);
        }
    }

    bool needsAnim = false;
    for (auto it = m_actionBarProgress.constBegin(); it != m_actionBarProgress.constEnd(); ++it) {
        const qreal target = openTargets.contains(it.key()) ? 1.0 : 0.0;
        if (qAbs(it.value() - target) > 0.001) {
            needsAnim = true;
            break;
        }
    }

    if (needsAnim && m_actionAnimTimer && !m_actionAnimTimer->isActive()) {
        m_lastActionAnimMs = QDateTime::currentMSecsSinceEpoch();
        m_actionAnimTimer->start();
    }
    m_delegate->setActionBarProgress(m_actionBarProgress);
}

void KlipperPopup::tickActionBarAnimations()
{
    if (!m_delegate) {
        if (m_actionAnimTimer) {
            m_actionAnimTimer->stop();
        }
        return;
    }

    const auto *s = MklipperSettings::self();
    const ActionBarMode mode = s->actionBarMode();
    if (mode == ActionBarMode::AlwaysVisible) {
        if (m_actionAnimTimer) {
            m_actionAnimTimer->stop();
        }
        return;
    }

    QSet<QString> openTargets;
    if (ClipboardItemDelegate::usesMenuMode(mode)) {
        openTargets = m_pinnedMenuUuids;
        if (mode == ActionBarMode::MenuHoverOrClick && !m_hoverMenuUuid.isEmpty()) {
            openTargets.insert(m_hoverMenuUuid);
        }
    } else if (mode == ActionBarMode::AlwaysOnHover) {
        const QModelIndex hoverIdx = m_delegate->hoveredIndex();
        if (hoverIdx.isValid()) {
            const QString u = hoverIdx.data(HistoryModel::UuidRole).toString();
            if (!u.isEmpty()) {
                openTargets.insert(u);
            }
        }
        if (m_listView) {
            const QModelIndex curIdx = m_listView->currentIndex();
            if (curIdx.isValid()) {
                const QString u = curIdx.data(HistoryModel::UuidRole).toString();
                if (!u.isEmpty()) {
                    openTargets.insert(u);
                }
            }
        }
    }

    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    const qint64 dt = qBound(qint64(1), nowMs - m_lastActionAnimMs, qint64(60));
    m_lastActionAnimMs = nowMs;
    const qreal step = static_cast<qreal>(dt) / static_cast<qreal>(kActionBarAnimDurationMs);

    bool anyActive = false;
    for (auto it = m_actionBarProgress.begin(); it != m_actionBarProgress.end();) {
        const qreal target = openTargets.contains(it.key()) ? 1.0 : 0.0;
        qreal cur = it.value();
        if (cur < target) {
            cur = qMin(target, cur + step);
        } else if (cur > target) {
            cur = qMax(target, cur - step);
        }
        if (cur <= 0.001 && target == 0.0) {
            it = m_actionBarProgress.erase(it);
            continue;
        }
        it.value() = cur;
        if (qAbs(cur - target) > 0.001) {
            anyActive = true;
        }
        ++it;
    }

    m_delegate->setActionBarProgress(m_actionBarProgress);
    if (m_listView) {
        m_listView->viewport()->update();
    }
    if (!anyActive && m_actionAnimTimer) {
        m_actionAnimTimer->stop();
    }
}

void KlipperPopup::applyTheme()
{
    if (m_applyingTheme) {
        return;
    }
    m_applyingTheme = true;

    const QPalette appPal = QApplication::palette();
    setPalette(appPal);
    const ThemeTokens t = ThemeTokens::fromPalette(appPal);
    setStyleSheet(ThemeStyle::popupStyleSheet(t));
    if (style()) {
        style()->unpolish(this);
        style()->polish(this);
    }

    // Crisp monochrome flat SVG icons across the entire header & subpages
    if (m_searchIconAction) {
        m_searchIconAction->setIcon(FlatIcons::icon(FlatIcon::Search, t.textMuted, t.textSecondary, t.textMuted, 15));
    }
    if (m_pauseBtn) {
        m_pauseBtn->setIcon(FlatIcons::toggleIcon(
            FlatIcon::Pause, t.textSecondary,
            FlatIcon::Play, t.warning,
            t.textPrimary, 16));
    }
    if (m_pauseIconLabel) {
        m_pauseIconLabel->setPixmap(FlatIcons::pixmap(FlatIcon::Pause, t.warning, 16, devicePixelRatioF()));
    }
    if (m_pauseLabel) {
        QPalette pp = appPal;
        pp.setColor(QPalette::WindowText, t.textPrimary);
        m_pauseLabel->setPalette(pp);
    }
    if (m_clearBtn) {
        m_clearBtn->setIcon(FlatIcons::icon(FlatIcon::Trash, t.textSecondary, t.danger, t.textMuted, 16));
    }
    if (m_configBtn) {
        m_configBtn->setIcon(FlatIcons::icon(FlatIcon::Settings, t.textSecondary, t.textPrimary, t.textMuted, 16));
    }
    if (m_closeBtn) {
        m_closeBtn->setIcon(FlatIcons::icon(FlatIcon::Close, t.textSecondary, t.danger, t.textMuted, 16));
    }
    if (m_pwIconLabel) {
        m_pwIconLabel->setPixmap(FlatIcons::pixmap(FlatIcon::Lock, t.warning, 16, devicePixelRatioF()));
    }
    if (m_pwLabel) {
        QPalette pwp = appPal;
        pwp.setColor(QPalette::WindowText, t.textPrimary);
        m_pwLabel->setPalette(pwp);
    }
    if (m_emptyBadge) {
        m_emptyBadge->setStyleSheet(
            QStringLiteral("QFrame { background-color: %1; border: none; border-radius: 28px; }")
                .arg(ThemeStyle::rgbaCss(t.surfaceBg)));
    }
    if (m_emptyIconLabel) {
        m_emptyIconLabel->setPixmap(FlatIcons::pixmap(FlatIcon::Clipboard, t.textMuted, 26, devicePixelRatioF()));
    }
    if (m_emptyLabel) {
        QPalette ep = appPal;
        ep.setColor(QPalette::WindowText, t.textPrimary);
        m_emptyLabel->setPalette(ep);
    }
    if (m_emptySubLabel) {
        QPalette esp = appPal;
        esp.setColor(QPalette::WindowText, t.textMuted);
        m_emptySubLabel->setPalette(esp);
    }
    if (m_statusLabel) {
        QPalette sp = appPal;
        sp.setColor(QPalette::WindowText, t.textMuted);
        m_statusLabel->setPalette(sp);
    }
    if (m_editBackBtn) {
        m_editBackBtn->setIcon(FlatIcons::icon(FlatIcon::Back, t.textSecondary, t.textPrimary, t.textMuted, 15));
    }
    if (m_editTitleLabel) {
        QPalette etp = appPal;
        etp.setColor(QPalette::WindowText, t.textPrimary);
        m_editTitleLabel->setPalette(etp);
    }
    if (m_saveEditBtn) {
        m_saveEditBtn->setIcon(FlatIcons::icon(FlatIcon::Check, t.accentFg, t.accentFg, t.textMuted, 15));
    }
    if (m_qrWidget) {
        m_qrWidget->refreshTheme();
    }
    if (m_previewWidget) {
        m_previewWidget->refreshTheme();
    }
    if (m_delegate) {
        m_delegate->refreshTheme(t);
    }
    if (m_plainTextSwitch) {
        m_plainTextSwitch->update();
    }

    m_applyingTheme = false;
    updatePersistentUi();
    update();
}

void KlipperPopup::changeEvent(QEvent *ev)
{
    QFrame::changeEvent(ev);
    if (ev->type() == QEvent::PaletteChange || ev->type() == QEvent::ApplicationPaletteChange) {
        applyTheme();
    }
}

void KlipperPopup::showPositioned(const QPoint &anchorPos, bool anchorAbove)
{
    if (!isVisible()) {
        m_paster->captureTargetWindow(winId());
    }

    m_stack->setCurrentIndex(0);
    m_searchEdit->clear();
    m_proxy->setSearchQuery(QString());
    setActiveCategory(HistoryFilterProxyModel::CategoryFilter::All);

    if (m_proxy->rowCount() > 0) {
        m_listView->setCurrentIndex(m_proxy->index(0, 0));
        m_listView->scrollToTop();
    }
    updatePasswordBanner();
    updatePauseUi();
    updateStatusLabel();
    if (m_listView) {
        m_listView->viewport()->update();
    }

    QScreen *screen = QGuiApplication::screenAt(anchorPos);
    if (!screen) {
        screen = QGuiApplication::primaryScreen();
    }
    const QRect avail = screen ? screen->availableGeometry() : QRect(0, 0, 1920, 1080);

    // Scale popup dimensions proportionally with system text size while staying within screen bounds
    const qreal fontScale = qBound(1.0, fontMetrics().height() / 17.0, 1.75);
    const int targetW = qMin(qRound(kPopupWidth * fontScale), qMax(320, avail.width() - 24));
    const int targetH = qMin(qRound(kPopupHeight * fontScale), qMax(300, avail.height() - 24));
    setFixedSize(targetW, targetH);

    int x = anchorPos.x() - width() / 2;
    int y = anchorAbove ? (anchorPos.y() - height() - 8) : (anchorPos.y() + 8);

    // If anchoring above doesn't fit, place below (e.g. top panel)
    if (anchorAbove && y < avail.top()) {
        y = anchorPos.y() + 8;
    }
    if (!anchorAbove && y + height() > avail.bottom()) {
        y = anchorPos.y() - height() - 8;
    }

    x = qBound(avail.left() + 6, x, avail.right() - width() - 6);
    y = qBound(avail.top() + 6, y, avail.bottom() - height() - 6);

    move(x, y);
    show();
    raise();
    activateWindow();
    m_searchEdit->setFocus(Qt::PopupFocusReason);
}

void KlipperPopup::showAtCursor()
{
    showPositioned(QCursor::pos(), false);
}

void KlipperPopup::showNearTray(const QRect &trayRect)
{
    if (trayRect.isValid() && !trayRect.isNull()) {
        const QPoint center = trayRect.center();
        QScreen *screen = QGuiApplication::screenAt(center);
        const QRect avail = screen ? screen->availableGeometry() : QRect(0, 0, 1920, 1080);
        const bool isBottomHalf = center.y() > avail.center().y();
        const QPoint anchor(center.x(), isBottomHalf ? trayRect.top() : trayRect.bottom());
        showPositioned(anchor, isBottomHalf);
    } else {
        const QPoint cursor = QCursor::pos();
        QScreen *screen = QGuiApplication::screenAt(cursor);
        const QRect avail = screen ? screen->availableGeometry() : QRect(0, 0, 1920, 1080);
        showPositioned(cursor, cursor.y() > avail.center().y());
    }
}

void KlipperPopup::toggleAtCursor()
{
    if (isVisible()) {
        hide();
    } else {
        showAtCursor();
    }
}

void KlipperPopup::toggleNearTray(const QRect &trayRect)
{
    // Avoid immediate re-open when clicking the tray icon caused WindowDeactivate < 250ms ago
    if (isVisible()) {
        hide();
        return;
    }
    if (QDateTime::currentMSecsSinceEpoch() - m_lastHideTimeMs < 250) {
        return;
    }
    showNearTray(trayRect);
}

void KlipperPopup::editCurrentClipboard()
{
    if (!isVisible()) {
        showAtCursor();
    }
    if (m_proxy->rowCount() > 0) {
        openEditPage(m_proxy->index(0, 0));
    }
}

void KlipperPopup::showCurrentBarcode()
{
    if (!isVisible()) {
        showAtCursor();
    }
    if (m_proxy->rowCount() > 0) {
        openQrPage(m_proxy->index(0, 0));
    }
}

bool KlipperPopup::event(QEvent *ev)
{
    const auto *s = MklipperSettings::self();
    if (ev->type() == QEvent::Show) {
        if (m_relTimeTimer) {
            m_relTimeTimer->start();
        }
        if (m_listView) {
            m_listView->viewport()->update();
        }
        Q_EMIT popupShown();
    } else if (ev->type() == QEvent::Hide) {
        if (m_relTimeTimer) {
            m_relTimeTimer->stop();
        }
        if (m_highlightTimer) {
            m_highlightTimer->stop();
        }
        if (m_actionAnimTimer) {
            m_actionAnimTimer->stop();
        }
        m_hoverMenuUuid.clear();
        m_suppressedHoverMenuUuid.clear();
        syncActionBarTargets();
        if (!m_highlightStartMs.isEmpty()) {
            m_highlightStartMs.clear();
            if (m_delegate) {
                m_delegate->setNewHighlights({});
            }
        }
        if (m_previewWidget) {
            m_previewWidget->clearPreview();
        }
        if (m_editArea) {
            m_editArea->clear();
        }
        m_editingUuid.clear();
        if (m_tooltipController) {
            m_tooltipController->cancel();
        }
        m_pastingInProgress = false;
        ++m_pasteGeneration;
    } else if (ev->type() == QEvent::Enter && s->keepPopupOpen() && !m_pastingInProgress) {
        // Refresh target window when user switches to another app and hovers back over persistent popup
        m_paster->captureTargetWindow(winId());
    } else if (ev->type() == QEvent::WindowDeactivate) {
        if (m_tooltipController) {
            m_tooltipController->cancel();
        }
        if (!m_suppressAutoHide
            && !m_pastingInProgress
            && (!s->keepPopupOpen() || s->closePopupOnFocusOut())
            && QApplication::activeModalWidget() == nullptr
            && QApplication::activePopupWidget() == nullptr) {
            m_lastHideTimeMs = QDateTime::currentMSecsSinceEpoch();
            hide();
        }
    }
    return QFrame::event(ev);
}

bool KlipperPopup::eventFilter(QObject *watched, QEvent *ev)
{
    if (watched == m_searchEdit && ev->type() == QEvent::KeyPress) {
        auto *ke = static_cast<QKeyEvent *>(ev);
        if (m_stack->currentIndex() == 0 && tryQuickSelectDigit(ke, m_searchEdit->text().isEmpty())) {
            return true;
        }
        if (ke->key() == Qt::Key_Down) {
            if (m_proxy->rowCount() > 0) {
                QModelIndex cur = m_listView->currentIndex();
                int nextRow = cur.isValid() ? qMin(cur.row() + 1, m_proxy->rowCount() - 1) : 0;
                m_listView->setCurrentIndex(m_proxy->index(nextRow, 0));
                m_listView->setFocus();
            }
            return true;
        }
        if (ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) {
            QModelIndex cur = m_listView->currentIndex();
            if (!cur.isValid() && m_proxy->rowCount() > 0) {
                cur = m_proxy->index(0, 0);
            }
            if (cur.isValid()) {
                activateItem(cur, ke->modifiers().testFlag(Qt::ShiftModifier));
            }
            return true;
        }
        if (ke->key() == Qt::Key_Tab && m_stack->currentIndex() == 0) {
            const int nextCat = (static_cast<int>(m_proxy->categoryFilter()) + 1) % 5;
            setActiveCategory(static_cast<HistoryFilterProxyModel::CategoryFilter>(nextCat));
            return true;
        }
    }

    if (watched == m_listView && ev->type() == QEvent::KeyPress) {
        auto *ke = static_cast<QKeyEvent *>(ev);
        if (m_stack->currentIndex() == 0 && tryQuickSelectDigit(ke, true)) {
            return true;
        }
        const QModelIndex cur = m_listView->currentIndex();
        if ((ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) && cur.isValid()) {
            activateItem(cur, ke->modifiers().testFlag(Qt::ShiftModifier));
            return true;
        }
        if (ke->key() == Qt::Key_Space && cur.isValid() && m_stack->currentIndex() == 0) {
            openPreviewPage(cur);
            return true;
        }
        if (ke->key() == Qt::Key_Tab && m_stack->currentIndex() == 0) {
            const int nextCat = (static_cast<int>(m_proxy->categoryFilter()) + 1) % 5;
            setActiveCategory(static_cast<HistoryFilterProxyModel::CategoryFilter>(nextCat));
            return true;
        }
        if (ke->key() == Qt::Key_Delete && cur.isValid()) {
            deleteItem(cur);
            return true;
        }
        if (ke->key() == Qt::Key_Up && cur.isValid() && cur.row() == 0) {
            m_searchEdit->setFocus();
            return true;
        }
        if (ke->key() == Qt::Key_F && (ke->modifiers() & Qt::ControlModifier)) {
            m_searchEdit->setFocus();
            m_searchEdit->selectAll();
            return true;
        }
    }

    if (watched == m_listView->viewport()) {
        if (ev->type() == QEvent::ToolTip) {
            return true;
        }
        if (ev->type() == QEvent::MouseMove) {
            auto *me = static_cast<QMouseEvent *>(ev);
            const QPoint pos = me->position().toPoint();
            const QModelIndex prevIdx = m_delegate->hoveredIndex();
            const QModelIndex idx = m_listView->indexAt(pos);
            m_delegate->setHoveredIndex(idx, pos);

            // Repaint only affected row(s) instead of the entire viewport on every pixel move
            if (prevIdx.isValid() && prevIdx != idx) {
                m_listView->viewport()->update(m_listView->visualRect(prevIdx));
            }
            const ActionBarMode barMode = MklipperSettings::self()->actionBarMode();
            const QString prevHoverMenu = m_hoverMenuUuid;
            if (idx.isValid()) {
                const QRect cardRect = ClipboardItemDelegate::cardRectFor(m_listView->visualRect(idx));
                const QString uuid = idx.data(HistoryModel::UuidRole).toString();

                if (barMode == ActionBarMode::MenuHoverOrClick) {
                    const QRect menuRect = ClipboardItemDelegate::menuButtonRect(cardRect);
                    const QRect pillRect = m_delegate->expandedPillRect(cardRect, idx).adjusted(0, -2, 0, 2);
                    const bool overMenu = menuRect.contains(pos);
                    if (!overMenu && m_suppressedHoverMenuUuid == uuid) {
                        m_suppressedHoverMenuUuid.clear();
                    }
                    if (!uuid.isEmpty() && m_suppressedHoverMenuUuid != uuid) {
                        const bool alreadyOpen = (m_hoverMenuUuid == uuid) || (m_delegate->openProgressFor(uuid) > 0.05);
                        if (overMenu || (alreadyOpen && pillRect.contains(pos))) {
                            m_hoverMenuUuid = uuid;
                        } else {
                            m_hoverMenuUuid.clear();
                        }
                    } else {
                        m_hoverMenuUuid.clear();
                    }
                } else {
                    m_hoverMenuUuid.clear();
                    m_suppressedHoverMenuUuid.clear();
                }

                if (m_hoverMenuUuid != prevHoverMenu || (barMode == ActionBarMode::AlwaysOnHover && prevIdx != idx)) {
                    syncActionBarTargets();
                }

                m_listView->viewport()->update(m_listView->visualRect(idx));
                QString tip;
                QRect btnRect;
                const RowAction act = m_delegate->hitTestButton(cardRect, idx, pos, &tip, &btnRect);
                if (m_tooltipController) {
                    const QPoint gPos = me->globalPosition().toPoint();
                    if (act != RowAction::None && !tip.isEmpty()) {
                        const QString key = QStringLiteral("row:%1:act:%2").arg(idx.row()).arg(static_cast<int>(act));
                        m_tooltipController->hoverTarget(key,
                                                         TooltipCategory::ItemActions,
                                                         tip,
                                                         gPos,
                                                         m_listView->viewport(),
                                                         btnRect);
                    } else {
                        const QString infoTip = ClipboardItemDelegate::sourceTooltipFor(idx);
                        if (!infoTip.isEmpty()) {
                            const QString key = QStringLiteral("row:%1:info").arg(idx.row());
                            m_tooltipController->hoverTarget(key,
                                                             TooltipCategory::ItemInfo,
                                                             infoTip,
                                                             gPos,
                                                             m_listView->viewport(),
                                                             cardRect);
                        } else {
                            m_tooltipController->cancel();
                        }
                    }
                }
            } else {
                m_hoverMenuUuid.clear();
                m_suppressedHoverMenuUuid.clear();
                if (!prevHoverMenu.isEmpty() || barMode == ActionBarMode::AlwaysOnHover) {
                    syncActionBarTargets();
                }
                if (m_tooltipController) {
                    m_tooltipController->cancel();
                }
            }
        } else if (ev->type() == QEvent::Leave) {
            const QModelIndex prevIdx = m_delegate->hoveredIndex();
            m_delegate->setHoveredIndex(QModelIndex(), QPoint(-1, -1));
            m_hoverMenuUuid.clear();
            m_suppressedHoverMenuUuid.clear();
            syncActionBarTargets();
            if (m_tooltipController) {
                m_tooltipController->cancel();
            }
            if (prevIdx.isValid()) {
                m_listView->viewport()->update(m_listView->visualRect(prevIdx));
            }
        } else if (ev->type() == QEvent::MouseButtonPress || ev->type() == QEvent::Wheel) {
            if (m_tooltipController) {
                m_tooltipController->cancel();
            }
        } else if (ev->type() == QEvent::MouseButtonRelease) {
            auto *me = static_cast<QMouseEvent *>(ev);
            if (me->button() == Qt::LeftButton) {
                const QPoint pos = me->position().toPoint();
                const QModelIndex idx = m_listView->indexAt(pos);
                if (idx.isValid()) {
                    const QRect cardRect = ClipboardItemDelegate::cardRectFor(m_listView->visualRect(idx));
                    const QString uuid = idx.data(HistoryModel::UuidRole).toString();
                    const RowAction act = m_delegate->hitTestButton(cardRect, idx, pos);
                    switch (act) {
                    case RowAction::ToggleMenu: {
                        if (!uuid.isEmpty()) {
                            if (m_pinnedMenuUuids.contains(uuid)) {
                                m_pinnedMenuUuids.remove(uuid);
                                if (m_hoverMenuUuid == uuid) {
                                    m_hoverMenuUuid.clear();
                                }
                                m_suppressedHoverMenuUuid = uuid;
                            } else {
                                m_pinnedMenuUuids.insert(uuid);
                                if (m_suppressedHoverMenuUuid == uuid) {
                                    m_suppressedHoverMenuUuid.clear();
                                }
                            }
                            syncActionBarTargets();
                            m_listView->viewport()->update(m_listView->visualRect(idx));
                        }
                        return true;
                    }
                    case RowAction::OpenSource: {
                        const QUrl srcUrl(idx.data(HistoryModel::SourceUrlRole).toString());
                        if (isSafeExternalHttpUrl(srcUrl)) {
                            if (!MklipperSettings::self()->keepPopupOpen()) {
                                hide();
                            }
                            QDesktopServices::openUrl(srcUrl);
                        }
                        return true;
                    }
                    case RowAction::Star:
                        toggleStar(idx);
                        return true;
                    case RowAction::Action:
                        invokeItemAction(idx);
                        return true;
                    case RowAction::Preview:
                        openPreviewPage(idx);
                        return true;
                    case RowAction::SaveImage:
                        saveImageByUuid(sourceIndexFor(idx).data(HistoryModel::UuidRole).toString());
                        return true;
                    case RowAction::Qr:
                        openQrPage(idx);
                        return true;
                    case RowAction::Edit:
                        openEditPage(idx);
                        return true;
                    case RowAction::Delete:
                        deleteItem(idx);
                        return true;
                    case RowAction::None:
                        if (m_delegate->openProgressFor(uuid) >= 0.30
                            && m_delegate->expandedPillRect(cardRect, idx).contains(pos)) {
                            // Ignore stray clicks on the narrow padding between action buttons
                            return true;
                        }
                        activateItem(idx, me->modifiers().testFlag(Qt::ShiftModifier));
                        return true;
                    }
                }
            }
        }
    }

    return QFrame::eventFilter(watched, ev);
}

bool KlipperPopup::tryQuickSelectDigit(QKeyEvent *ke, bool allowBareDigit)
{
    if (!MklipperSettings::self()->quickSelectDigits()) {
        return false;
    }

    const Qt::KeyboardModifiers rawMods = ke->modifiers() & ~Qt::KeypadModifier;
    const bool invertPlainText = rawMods.testFlag(Qt::ShiftModifier);
    const Qt::KeyboardModifiers mods = rawMods & ~Qt::ShiftModifier;
    const bool hasQuickMod = (mods == Qt::AltModifier || mods == Qt::ControlModifier);
    const bool isBare = (mods == Qt::NoModifier && !invertPlainText);

    if (!hasQuickMod && !(allowBareDigit && isBare)) {
        return false;
    }

    int targetRow = -1;
    if (ke->key() >= Qt::Key_1 && ke->key() <= Qt::Key_9) {
        targetRow = ke->key() - Qt::Key_1; // 1..9 -> rows 0..8
    } else if (ke->key() == Qt::Key_0) {
        targetRow = 9; // 0 -> row 9 (10th item)
    } else {
        return false;
    }

    if (targetRow >= 0 && targetRow < m_proxy->rowCount()) {
        activateItem(m_proxy->index(targetRow, 0), invertPlainText);
        return true;
    }
    return false;
}

void KlipperPopup::keyPressEvent(QKeyEvent *ev)
{
    if (ev->key() == Qt::Key_Escape) {
        if (m_stack->currentIndex() != 0) {
            returnToListPage();
        } else {
            hide();
        }
        ev->accept();
        return;
    }
    if (m_stack->currentIndex() == 0 && tryQuickSelectDigit(ev, m_searchEdit->text().isEmpty())) {
        ev->accept();
        return;
    }
    if (m_stack->currentIndex() == 1 && (ev->modifiers() & Qt::ControlModifier)
        && (ev->key() == Qt::Key_Return || ev->key() == Qt::Key_Enter)) {
        saveEditedText();
        ev->accept();
        return;
    }
    QFrame::keyPressEvent(ev);
}

void KlipperPopup::mousePressEvent(QMouseEvent *ev)
{
    if (ev->button() == Qt::LeftButton) {
        m_dragging = true;
        m_dragOffset = ev->globalPosition().toPoint() - frameGeometry().topLeft();
        ev->accept();
        return;
    }
    QFrame::mousePressEvent(ev);
}

void KlipperPopup::mouseMoveEvent(QMouseEvent *ev)
{
    if (m_dragging && (ev->buttons() & Qt::LeftButton)) {
        move(ev->globalPosition().toPoint() - m_dragOffset);
        ev->accept();
        return;
    }
    QFrame::mouseMoveEvent(ev);
}

void KlipperPopup::mouseReleaseEvent(QMouseEvent *ev)
{
    if (ev->button() == Qt::LeftButton && m_dragging) {
        m_dragging = false;
        ev->accept();
        return;
    }
    QFrame::mouseReleaseEvent(ev);
}

QModelIndex KlipperPopup::sourceIndexFor(const QModelIndex &proxyIndex) const
{
    return m_proxy->mapToSource(proxyIndex);
}

void KlipperPopup::activateItem(const QModelIndex &proxyIndex, bool invertPlainText)
{
    const QModelIndex src = sourceIndexFor(proxyIndex);
    if (!src.isValid()) {
        return;
    }
    activateItemByUuid(src.data(HistoryModel::UuidRole).toString(), invertPlainText);
}

void KlipperPopup::activateItemByUuid(const QString &uuid, bool invertPlainText)
{
    if (uuid.isEmpty()) {
        return;
    }
    const auto *s = MklipperSettings::self();
    const std::optional<bool> plainOverride = invertPlainText
        ? std::optional<bool>(!s->pastePlainText())
        : std::nullopt;
    m_model->moveToTop(uuid, plainOverride);
    if (!s->keepPopupOpen()) {
        hide();
    } else if (m_stack->currentIndex() != 0) {
        returnToListPage();
    }
    if (s->autoPaste()) {
        if (s->keepPopupOpen()) {
            const quint64 gen = ++m_pasteGeneration;
            m_pastingInProgress = true;
            // Safety fallback in case paste never finishes
            QTimer::singleShot(1500, this, [this, gen]() {
                if (gen == m_pasteGeneration) {
                    m_pastingInProgress = false;
                }
            });
        }
        m_paster->schedulePaste();
    }
}

void KlipperPopup::onPasteFinished()
{
    const auto *s = MklipperSettings::self();
    if (!isVisible() || !s->keepPopupOpen()) {
        m_pastingInProgress = false;
        return;
    }

    if (s->closePopupOnFocusOut()) {
        const quint64 gen = m_pasteGeneration;
        raise();
        activateWindow();
        m_paster->activateOwnWindow(winId());
        QTimer::singleShot(140, this, [this, gen]() {
            if (gen == m_pasteGeneration) {
                m_pastingInProgress = false;
            }
        });
    } else {
        m_pastingInProgress = false;
    }
}

void KlipperPopup::openEditPage(const QModelIndex &proxyIndex)
{
    const QModelIndex src = sourceIndexFor(proxyIndex);
    if (!src.isValid()) {
        return;
    }
    openEditPageByUuid(src.data(HistoryModel::UuidRole).toString());
}

void KlipperPopup::openEditPageByUuid(const QString &uuid)
{
    const int row = m_model->indexOf(uuid);
    if (row < 0) {
        return;
    }
    const QModelIndex src = m_model->index(row);
    const auto type = static_cast<HistoryItemType>(src.data(HistoryModel::TypeIntRole).toInt());
    if (type != HistoryItemType::Text) {
        return;
    }
    m_editingUuid = uuid;
    m_editArea->setPlainText(src.data(Qt::DisplayRole).toString());
    m_stack->setCurrentIndex(1);
    m_editArea->setFocus();
}

void KlipperPopup::saveEditedText()
{
    const int row = m_model->indexOf(m_editingUuid);
    if (row >= 0) {
        m_model->setData(m_model->index(row), m_editArea->toPlainText(), Qt::DisplayRole);
    }
    returnToListPage();
}

void KlipperPopup::openQrPage(const QModelIndex &proxyIndex)
{
    const QModelIndex src = sourceIndexFor(proxyIndex);
    if (!src.isValid()) {
        return;
    }
    m_qrWidget->setText(src.data(Qt::DisplayRole).toString());
    m_stack->setCurrentIndex(2);
}

void KlipperPopup::openPreviewPage(const QModelIndex &proxyIndex)
{
    const QModelIndex src = sourceIndexFor(proxyIndex);
    if (!src.isValid() || !m_previewWidget) {
        return;
    }
    const QString uuid = src.data(HistoryModel::UuidRole).toString();
    const auto type = static_cast<HistoryItemType>(src.data(HistoryModel::TypeIntRole).toInt());
    const QString metaLine = ClipboardItemDelegate::compactSourceFor(src);

    if (type == HistoryItemType::Image) {
        const QUrl imgUrl = src.data(HistoryModel::ImageUrlRole).toUrl();
        m_previewWidget->setImagePreview(uuid, imgUrl.toLocalFile(), metaLine);
    } else {
        m_previewWidget->setTextPreview(uuid, type, src.data(Qt::DisplayRole).toString(), metaLine);
    }
    m_stack->setCurrentIndex(3);
}

void KlipperPopup::saveImageByUuid(const QString &uuid)
{
    if (uuid.isEmpty()) {
        return;
    }
    const QString picsDir = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    const QString defaultName = (picsDir.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::HomeLocation) : picsDir)
        + u"/mklipper-"_s + QDateTime::currentDateTime().toString(u"yyyyMMdd-HHmmss"_s) + u".png"_s;

    m_suppressAutoHide = true;
    const QString dest = QFileDialog::getSaveFileName(
        this,
        tr("Guardar imagen del historial"),
        defaultName,
        tr("Imagen PNG (*.png)"));
    m_suppressAutoHide = false;

    if (!dest.isEmpty()) {
        m_model->exportImageToFile(uuid, dest);
    }
    raise();
    activateWindow();
}

void KlipperPopup::setActiveCategory(HistoryFilterProxyModel::CategoryFilter cat)
{
    m_proxy->setCategoryFilter(cat);
    if (m_categoryGroup) {
        if (auto *btn = m_categoryGroup->button(static_cast<int>(cat))) {
            QSignalBlocker blk(m_categoryGroup);
            btn->setChecked(true);
        }
    }
    if (m_proxy->rowCount() > 0) {
        m_listView->setCurrentIndex(m_proxy->index(0, 0));
    }
    updateStatusLabel();
}

void KlipperPopup::updatePauseUi()
{
    const bool paused = m_model->isCapturePaused();
    if (m_pauseBtn) {
        QSignalBlocker blk(m_pauseBtn);
        m_pauseBtn->setChecked(paused);
    }
    if (m_pauseBanner) {
        m_pauseBanner->setVisible(paused);
    }
}

void KlipperPopup::toggleStar(const QModelIndex &proxyIndex)
{
    const QModelIndex src = sourceIndexFor(proxyIndex);
    if (!src.isValid()) {
        return;
    }
    const bool current = src.data(HistoryModel::StarredRole).toBool();
    m_model->setData(src, !current, HistoryModel::StarredRole);
}

void KlipperPopup::deleteItem(const QModelIndex &proxyIndex)
{
    const QModelIndex src = sourceIndexFor(proxyIndex);
    if (!src.isValid()) {
        return;
    }
    const QString uuid = src.data(HistoryModel::UuidRole).toString();
    m_pinnedMenuUuids.remove(uuid);
    m_actionBarProgress.remove(uuid);
    if (m_hoverMenuUuid == uuid) {
        m_hoverMenuUuid.clear();
    }
    m_suppressAutoHide = true;
    m_model->remove(uuid, true, this);
    m_suppressAutoHide = false;
    updateStatusLabel();
    raise();
    activateWindow();
}

void KlipperPopup::invokeItemAction(const QModelIndex &proxyIndex)
{
    const QModelIndex src = sourceIndexFor(proxyIndex);
    if (!src.isValid()) {
        return;
    }
    const QString uuid = src.data(HistoryModel::UuidRole).toString();
    hide();
    Q_EMIT invokeActionRequested(uuid);
}

void KlipperPopup::returnToListPage()
{
    if (m_previewWidget) {
        m_previewWidget->clearPreview();
    }
    if (m_editArea) {
        m_editArea->clear();
    }
    m_editingUuid.clear();
    m_stack->setCurrentIndex(0);
    m_searchEdit->setFocus();
}

void KlipperPopup::updatePasswordBanner()
{
    m_passwordBanner->setVisible(m_model->clipboardHasPassword() || m_model->selectionHasPassword());
}

void KlipperPopup::updatePersistentUi()
{
    const auto *s = MklipperSettings::self();
    const bool persistent = s->keepPopupOpen();
    if (m_closeBtn) {
        m_closeBtn->setVisible(persistent);
    }

    const bool plainText = s->pastePlainText();
    if (m_plainTextSwitch && m_plainTextSwitch->isChecked() != plainText) {
        QSignalBlocker blocker(m_plainTextSwitch);
        m_plainTextSwitch->setChecked(plainText);
    }
    if (m_plainTextHintLabel) {
        const ThemeTokens t = ThemeTokens::fromPalette(QApplication::palette());
        m_plainTextHintLabel->setText(plainText ? tr("Solo texto plano") : tr("Con formato original"));
        if (plainText) {
            m_plainTextHintLabel->setStyleSheet(
                QStringLiteral("QLabel { color: %1; background-color: %2; "
                               "border: none; border-radius: 9px; padding: 2px 9px; font-weight: 600; }")
                    .arg(ThemeStyle::rgbaCss(t.accent), ThemeStyle::rgbaCss(t.accentSoft)));
        } else {
            m_plainTextHintLabel->setStyleSheet(
                QStringLiteral("QLabel { color: %1; background-color: transparent; "
                               "border: none; border-radius: 9px; padding: 2px 6px; }")
                    .arg(ThemeStyle::rgbaCss(t.textMuted)));
        }
    }
    if (m_listView) {
        m_listView->doItemsLayout();
        m_listView->viewport()->update();
    }
    if (m_tooltipController) {
        m_tooltipController->setConfig(TooltipConfig::fromSettings());
    }
    syncActionBarTargets();
    updateStatusLabel();
}

void KlipperPopup::updateStatusLabel()
{
    const int visible = m_proxy->rowCount();
    const int total = m_model->rowCount();
    const int starred = m_model->starredCount();

    m_listStack->setCurrentIndex(visible > 0 ? 0 : 1);
    if (visible == 0) {
        const bool hasFilter = !m_searchEdit->text().isEmpty()
            || m_proxy->categoryFilter() != HistoryFilterProxyModel::CategoryFilter::All;
        if (hasFilter) {
            m_emptyLabel->setText(tr("No hay coincidencias."));
            if (m_emptySubLabel) {
                m_emptySubLabel->setText(tr("Prueba con otro término o cambia el filtro activo."));
            }
        } else {
            m_emptyLabel->setText(tr("El historial del portapapeles está vacío."));
            if (m_emptySubLabel) {
                m_emptySubLabel->setText(tr("Copia texto, enlaces o imágenes para verlos aquí."));
            }
        }
    }

    QString text = tr("%1 elementos").arg(total);
    if (starred > 0) {
        text += tr(" (%1 fijados)").arg(starred);
    }
    if (MklipperSettings::self()->quickSelectDigits()) {
        text += tr("  ·  1–0: pegar  ·  Espacio: ver  ·  Supr: borrar");
    } else {
        text += tr("  ·  Enter: pegar  ·  Espacio: ver  ·  Supr: borrar");
    }
    m_statusLabel->setText(text);
}

#include "klipperpopup.moc"
