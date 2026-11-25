/*
    SPDX-FileCopyrightText: 2004 Esben Mose Hansen <kde@mosehansen.dk>
    SPDX-FileCopyrightText: 2024 Fushan Wen <qydwhotmail@gmail.com>
    SPDX-FileCopyrightText: 2026 Mklipper contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "klipperpopup.h"

#include "historyitem.h"
#include "historymodel.h"
#include "qrcodewidget.h"
#include "settings.h"
#include "x11paster.h"

#include <QApplication>
#include <QCache>
#include <QCursor>
#include <QDateTime>
#include <QEvent>
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
#include <QStyledItemDelegate>
#include <QTimer>
#include <QToolButton>
#include <QToolTip>
#include <QUrl>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;

namespace
{
constexpr int kPopupWidth = 420;
constexpr int kPopupHeight = 500;
constexpr int kRowHeightText = 54;
constexpr int kRowHeightImage = 76;
constexpr int kActionBtnSize = 24;
constexpr int kActionBtnGap = 4;

enum class RowAction {
    None,
    Star,
    Action,
    Qr,
    Edit,
    Delete,
};
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

void HistoryFilterProxyModel::setStarredOnly(bool starredOnly)
{
    if (m_starredOnly == starredOnly) {
        return;
    }
    m_starredOnly = starredOnly;
    invalidateFilter();
}

bool HistoryFilterProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    const QModelIndex idx = sourceModel()->index(sourceRow, 0, sourceParent);
    if (!idx.isValid()) {
        return false;
    }

    if (m_starredOnly && !idx.data(HistoryModel::StarredRole).toBool()) {
        return false;
    }

    const QRegularExpression re = filterRegularExpression();
    if (re.pattern().isEmpty()) {
        return true;
    }

    const QString text = idx.data(Qt::DisplayRole).toString();
    return text.contains(re);
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
        , m_thumbCache(64)
    {
    }

    void setHoveredIndex(const QModelIndex &idx, const QPoint &pos)
    {
        m_hoveredIndex = idx;
        m_hoverPos = pos;
    }

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        const auto type = static_cast<HistoryItemType>(index.data(HistoryModel::TypeIntRole).toInt());
        const int h = (type == HistoryItemType::Image) ? kRowHeightImage : kRowHeightText;
        return QSize(option.rect.width(), h);
    }

    struct ButtonRect {
        RowAction action;
        QRect rect;
        QString tooltip;
        QString iconName;
        QString fallbackGlyph;
        bool active = false;
    };

    QList<ButtonRect> buttonRects(const QRect &itemRect, const QModelIndex &index) const
    {
        QList<ButtonRect> buttons;
        const auto type = static_cast<HistoryItemType>(index.data(HistoryModel::TypeIntRole).toInt());
        const bool starred = index.data(HistoryModel::StarredRole).toBool();
        const bool hasAction = index.data(HistoryModel::HasActionRole).toBool();

        int right = itemRect.right() - 8;
        const int cy = itemRect.center().y() - kActionBtnSize / 2;

        auto pushBtn = [&](RowAction act, const QString &tip, const QString &icon, const QString &glyph, bool active = false) {
            QRect r(right - kActionBtnSize, cy, kActionBtnSize, kActionBtnSize);
            buttons.append({act, r, tip, icon, glyph, active});
            right -= (kActionBtnSize + kActionBtnGap);
        };

        pushBtn(RowAction::Delete, tr("Eliminar"), u"edit-delete"_s, u"✕"_s);
        if (type == HistoryItemType::Text) {
            pushBtn(RowAction::Edit, tr("Editar texto"), u"document-edit"_s, u"✎"_s);
        }
        if (type == HistoryItemType::Text || type == HistoryItemType::Url) {
            pushBtn(RowAction::Qr, tr("Mostrar código QR"), u"view-barcode-qr"_s, u"▦"_s);
        }
        if (hasAction) {
            pushBtn(RowAction::Action, tr("Ejecutar acción"), u"system-run"_s, u"⚡"_s);
        }
        pushBtn(RowAction::Star, starred ? tr("Desfijar") : tr("Fijar elemento"),
                starred ? u"starred"_s : u"non-starred"_s,
                starred ? u"★"_s : u"☆"_s,
                starred);

        return buttons;
    }

    RowAction hitTestButton(const QRect &itemRect, const QModelIndex &index, const QPoint &pos, QString *tooltipOut = nullptr) const
    {
        const auto btns = buttonRects(itemRect, index);
        for (const auto &b : btns) {
            if (b.rect.contains(pos)) {
                if (tooltipOut) {
                    *tooltipOut = b.tooltip;
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

        const QRect cardRect = option.rect.adjusted(6, 3, -6, -3);
        const bool isSelected = option.state & QStyle::State_Selected;
        const bool isHovered = (index == m_hoveredIndex) || (option.state & QStyle::State_MouseOver);
        const bool isStarred = index.data(HistoryModel::StarredRole).toBool();
        const bool isTopItem = (index.row() == 0);
        const auto type = static_cast<HistoryItemType>(index.data(HistoryModel::TypeIntRole).toInt());

        const QPalette &pal = option.palette;
        QColor bg = pal.color(QPalette::Base);
        if (isSelected) {
            bg = pal.color(QPalette::Highlight);
            bg.setAlpha(48);
        } else if (isHovered) {
            bg = pal.color(QPalette::Highlight);
            bg.setAlpha(26);
        } else {
            bg = pal.color(QPalette::AlternateBase);
        }

        QColor border = isSelected ? pal.color(QPalette::Highlight) : pal.color(QPalette::Mid);
        border.setAlpha(isSelected ? 190 : 80);

        QPainterPath path;
        path.addRoundedRect(cardRect, 7, 7);
        painter->fillPath(path, bg);
        painter->setPen(QPen(border, 1.0));
        painter->drawPath(path);

        // Active top item subtle accent strip on the left
        if (isTopItem) {
            QPainterPath accentBar;
            accentBar.addRoundedRect(QRectF(cardRect.left() + 1, cardRect.top() + 6, 3, cardRect.height() - 12), 1.5, 1.5);
            painter->fillPath(accentBar, pal.color(QPalette::Highlight));
        }

        // Compute right margin depending on whether hover buttons or star badge are shown
        const bool showButtons = isHovered || isSelected;
        int rightReserved = 10;
        QList<ButtonRect> btns;
        if (showButtons) {
            btns = buttonRects(cardRect, index);
            if (!btns.isEmpty()) {
                rightReserved = cardRect.right() - btns.last().rect.left() + 8;
            }
        } else if (isStarred) {
            rightReserved = 28;
        }

        QRect contentRect = cardRect.adjusted(10, 6, -rightReserved, -6);

        // Numbered badge (1..9, 0 for 10th shortcut, 11+ plain)
        const int rowNum = index.row();
        const bool hasDigitShortcut = (rowNum >= 0 && rowNum < 10);
        const QString badgeText = (rowNum < 9) ? QString::number(rowNum + 1)
                                : (rowNum == 9) ? u"0"_s
                                : QString::number(rowNum + 1);
        const int badgeW = (rowNum >= 99) ? 28 : 22;
        const int badgeH = 22;
        const QRect badgeRect(contentRect.left(),
                              contentRect.center().y() - badgeH / 2,
                              badgeW,
                              badgeH);

        QPainterPath badgePath;
        badgePath.addRoundedRect(badgeRect, 5, 5);
        QColor badgeBg = hasDigitShortcut ? pal.color(QPalette::Highlight) : pal.color(QPalette::Mid);
        badgeBg.setAlpha(hasDigitShortcut ? (isTopItem ? 190 : 70) : 45);
        painter->fillPath(badgePath, badgeBg);

        QColor badgeBorder = hasDigitShortcut ? pal.color(QPalette::Highlight) : pal.color(QPalette::Mid);
        badgeBorder.setAlpha(hasDigitShortcut ? 140 : 75);
        painter->setPen(QPen(badgeBorder, 1.0));
        painter->drawPath(badgePath);

        QFont badgeFont = option.font;
        badgeFont.setPointSizeF(qMax(8.0, badgeFont.pointSizeF() - 1.0));
        badgeFont.setBold(hasDigitShortcut);
        painter->setFont(badgeFont);
        QColor badgeFg = (hasDigitShortcut && isTopItem) ? pal.color(QPalette::HighlightedText) : pal.color(QPalette::Text);
        if (!hasDigitShortcut) {
            badgeFg.setAlpha(155);
        }
        painter->setPen(badgeFg);
        painter->drawText(badgeRect, Qt::AlignCenter, badgeText);

        contentRect.setLeft(badgeRect.right() + 9);

        if (type == HistoryItemType::Image) {
            const QUrl imgUrl = index.data(HistoryModel::ImageUrlRole).toUrl();
            const QString uuid = index.data(HistoryModel::UuidRole).toString();
            QPixmap thumb = loadThumbnail(uuid, imgUrl.toLocalFile(), QSize(96, cardRect.height() - 14));

            if (!thumb.isNull()) {
                const QRect thumbRect(contentRect.left(),
                                      contentRect.center().y() - thumb.height() / 2,
                                      thumb.width(),
                                      thumb.height());
                painter->drawPixmap(thumbRect, thumb);
                contentRect.setLeft(thumbRect.right() + 10);
            }

            painter->setPen(pal.color(QPalette::Text));
            QFont f = option.font;
            f.setBold(isTopItem);
            painter->setFont(f);
            const QString label = index.data(Qt::DisplayRole).toString();
            painter->drawText(contentRect, Qt::AlignVCenter | Qt::AlignLeft,
                              painter->fontMetrics().elidedText(label, Qt::ElideRight, contentRect.width()));
        } else {
            QString rawText = index.data(Qt::DisplayRole).toString();
            if (type == HistoryItemType::Url) {
                const QIcon urlIcon = QIcon::fromTheme(u"text-html"_s, QIcon::fromTheme(u"document-open"_s));
                if (!urlIcon.isNull()) {
                    const QRect iconRect(contentRect.left(), contentRect.center().y() - 9, 18, 18);
                    urlIcon.paint(painter, iconRect);
                    contentRect.setLeft(iconRect.right() + 8);
                }
            }

            // Prepare clean 2-line preview
            QStringList lines = rawText.trimmed().split(QLatin1Char('\n'), Qt::SkipEmptyParts);
            QString line1 = lines.isEmpty() ? rawText.simplified() : lines.first().simplified();
            QString line2 = (lines.size() > 1) ? lines.at(1).simplified() : QString();

            painter->setPen(pal.color(QPalette::Text));
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
                QColor muted = pal.color(QPalette::Text);
                muted.setAlpha(165);
                painter->setPen(muted);
                const QFontMetrics fm2(subFont);
                painter->drawText(r2, Qt::AlignVCenter | Qt::AlignLeft,
                                  fm2.elidedText(line2, Qt::ElideRight, r2.width()));
            }
        }

        // Draw buttons or persistent star indicator
        if (showButtons) {
            for (const auto &b : btns) {
                const bool btnHover = isHovered && b.rect.contains(m_hoverPos);
                if (btnHover) {
                    QPainterPath bp;
                    bp.addRoundedRect(b.rect, 5, 5);
                    QColor hbg = (b.action == RowAction::Delete) ? QColor(210, 60, 60, 170) : pal.color(QPalette::Highlight);
                    if (b.action != RowAction::Delete) {
                        hbg.setAlpha(110);
                    }
                    painter->fillPath(bp, hbg);
                }
                QColor glyphColor = pal.color(QPalette::Text);
                if (b.active) {
                    glyphColor = QColor(240, 175, 45);
                } else if (btnHover && b.action == RowAction::Delete) {
                    glyphColor = Qt::white;
                }
                painter->setPen(glyphColor);
                QFont bf = option.font;
                bf.setPointSize(11);
                painter->setFont(bf);
                painter->drawText(b.rect, Qt::AlignCenter, b.fallbackGlyph);
            }
        } else if (isStarred) {
            QRect starRect(cardRect.right() - 24, cardRect.center().y() - 10, 20, 20);
            painter->setPen(QColor(240, 175, 45));
            QFont sf = option.font;
            sf.setPointSize(11);
            painter->setFont(sf);
            painter->drawText(starRect, Qt::AlignCenter, u"★"_s);
        }

        painter->restore();
    }

private:
    QPixmap loadThumbnail(const QString &uuid, const QString &filePath, const QSize &maxSize) const
    {
        if (filePath.isEmpty()) {
            return {};
        }
        if (auto *cached = m_thumbCache.object(uuid)) {
            return *cached;
        }
        QImageReader reader(filePath);
        reader.setAutoTransform(true);
        const QSize orig = reader.size();
        if (orig.isValid()) {
            reader.setScaledSize(orig.scaled(maxSize, Qt::KeepAspectRatio));
        }
        const QImage img = reader.read();
        if (img.isNull()) {
            return {};
        }
        auto *pix = new QPixmap(QPixmap::fromImage(img.scaled(maxSize, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
        QPixmap copy = *pix;
        m_thumbCache.insert(uuid, pix);
        return copy;
    }

    QModelIndex m_hoveredIndex;
    QPoint m_hoverPos;
    mutable QCache<QString, QPixmap> m_thumbCache;
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
    setFrameShape(QFrame::StyledPanel);
    setFrameShadow(QFrame::Plain);
    setLineWidth(1);
    setFixedSize(kPopupWidth, kPopupHeight);

    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(1, 1, 1, 1);
    rootLayout->setSpacing(0);

    m_stack = new QStackedWidget(this);
    rootLayout->addWidget(m_stack);

    // ======================== Page 0: History List ========================
    auto *listPage = new QWidget(this);
    auto *listLayout = new QVBoxLayout(listPage);
    listLayout->setContentsMargins(8, 8, 8, 6);
    listLayout->setSpacing(6);

    // Top bar
    auto *topBar = new QHBoxLayout;
    topBar->setSpacing(4);

    m_searchEdit = new QLineEdit(listPage);
    m_searchEdit->setPlaceholderText(tr("Buscar en el historial…"));
    m_searchEdit->setClearButtonEnabled(true);
    m_searchEdit->addAction(QIcon::fromTheme(u"edit-find"_s), QLineEdit::LeadingPosition);
    m_searchEdit->installEventFilter(this);

    m_starFilterBtn = new QToolButton(listPage);
    m_starFilterBtn->setCheckable(true);
    m_starFilterBtn->setText(u"★"_s);
    m_starFilterBtn->setToolTip(tr("Mostrar solo elementos fijados"));
    m_starFilterBtn->setFixedSize(30, 30);

    m_clearBtn = new QToolButton(listPage);
    m_clearBtn->setIcon(QIcon::fromTheme(u"edit-clear-history"_s, QIcon::fromTheme(u"edit-clear"_s)));
    m_clearBtn->setToolTip(tr("Vaciar historial"));
    m_clearBtn->setFixedSize(30, 30);

    m_configBtn = new QToolButton(listPage);
    m_configBtn->setIcon(QIcon::fromTheme(u"preferences-system"_s, QIcon::fromTheme(u"configure"_s)));
    m_configBtn->setToolTip(tr("Configurar Mklipper"));
    m_configBtn->setFixedSize(30, 30);

    m_closeBtn = new QToolButton(listPage);
    m_closeBtn->setText(u"✕"_s);
    m_closeBtn->setToolTip(tr("Cerrar ventana"));
    m_closeBtn->setFixedSize(30, 30);
    connect(m_closeBtn, &QToolButton::clicked, this, &QWidget::hide);

    topBar->addWidget(m_searchEdit, 1);
    topBar->addWidget(m_starFilterBtn);
    topBar->addWidget(m_clearBtn);
    topBar->addWidget(m_configBtn);
    topBar->addWidget(m_closeBtn);
    listLayout->addLayout(topBar);

    // Password warning banner
    auto *pwBanner = new QFrame(listPage);
    pwBanner->setFrameShape(QFrame::StyledPanel);
    m_passwordBanner = pwBanner;
    auto *pwLayout = new QHBoxLayout(m_passwordBanner);
    pwLayout->setContentsMargins(8, 4, 8, 4);
    auto *pwLabel = new QLabel(tr("🔒 El portapapeles contiene una contraseña (no guardada)"), m_passwordBanner);
    pwLabel->setWordWrap(true);
    auto *pwClearBtn = new QPushButton(tr("Limpiar"), m_passwordBanner);
    pwClearBtn->setFlat(true);
    connect(pwClearBtn, &QPushButton::clicked, this, [this]() {
        m_model->clearPasswords();
    });
    pwLayout->addWidget(pwLabel, 1);
    pwLayout->addWidget(pwClearBtn);
    m_passwordBanner->setVisible(false);
    listLayout->addWidget(m_passwordBanner);

    // Proxy & ListView
    m_proxy = new HistoryFilterProxyModel(this);
    m_proxy->setSourceModel(m_model.get());

    m_delegate = new ClipboardItemDelegate(this);

    m_listView = new QListView(listPage);
    m_listView->setModel(m_proxy);
    m_listView->setItemDelegate(m_delegate);
    m_listView->setMouseTracking(true);
    m_listView->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_listView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_listView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_listView->setFrameShape(QFrame::NoFrame);
    m_listView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_listView->viewport()->setMouseTracking(true);
    m_listView->viewport()->installEventFilter(this);
    m_listView->installEventFilter(this);

    listLayout->addWidget(m_listView, 1);

    m_emptyLabel = new QLabel(tr("El historial está vacío."), listPage);
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_emptyLabel->setVisible(false);
    listLayout->addWidget(m_emptyLabel);

    // Footer status
    m_statusLabel = new QLabel(listPage);
    QFont statusFont = m_statusLabel->font();
    statusFont.setPointSizeF(qMax(8.0, statusFont.pointSizeF() - 1.5));
    m_statusLabel->setFont(statusFont);
    QPalette sp = m_statusLabel->palette();
    QColor muted = sp.color(QPalette::WindowText);
    muted.setAlpha(155);
    sp.setColor(QPalette::WindowText, muted);
    m_statusLabel->setPalette(sp);
    listLayout->addWidget(m_statusLabel);

    m_stack->addWidget(listPage);

    // ======================== Page 1: Edit Page ========================
    auto *editPage = new QWidget(this);
    auto *editLayout = new QVBoxLayout(editPage);
    editLayout->setContentsMargins(10, 10, 10, 10);
    editLayout->setSpacing(8);

    auto *editHeader = new QHBoxLayout;
    auto *editBackBtn = new QPushButton(QIcon::fromTheme(u"go-previous"_s), tr("Volver"), editPage);
    editBackBtn->setFlat(true);
    connect(editBackBtn, &QPushButton::clicked, this, &KlipperPopup::returnToListPage);
    auto *editTitle = new QLabel(tr("Editar contenido"), editPage);
    QFont etFont = editTitle->font();
    etFont.setBold(true);
    editTitle->setFont(etFont);
    editHeader->addWidget(editBackBtn);
    editHeader->addStretch();
    editHeader->addWidget(editTitle);
    editHeader->addStretch();
    editLayout->addLayout(editHeader);

    m_editArea = new QPlainTextEdit(editPage);
    editLayout->addWidget(m_editArea, 1);

    auto *editBtns = new QHBoxLayout;
    auto *cancelEditBtn = new QPushButton(tr("Cancelar"), editPage);
    auto *saveEditBtn = new QPushButton(QIcon::fromTheme(u"document-save"_s), tr("Guardar"), editPage);
    saveEditBtn->setDefault(true);
    connect(cancelEditBtn, &QPushButton::clicked, this, &KlipperPopup::returnToListPage);
    connect(saveEditBtn, &QPushButton::clicked, this, &KlipperPopup::saveEditedText);
    editBtns->addStretch();
    editBtns->addWidget(cancelEditBtn);
    editBtns->addWidget(saveEditBtn);
    editLayout->addLayout(editBtns);

    m_stack->addWidget(editPage);

    // ======================== Page 2: QR Page ========================
    m_qrWidget = new QRCodeWidget(this);
    connect(m_qrWidget, &QRCodeWidget::backRequested, this, &KlipperPopup::returnToListPage);
    m_stack->addWidget(m_qrWidget);

    // Connections
    connect(m_searchEdit, &QLineEdit::textChanged, this, [this](const QString &text) {
        m_proxy->setFilterFixedString(text);
        if (m_proxy->rowCount() > 0) {
            m_listView->setCurrentIndex(m_proxy->index(0, 0));
        }
        updateStatusLabel();
    });
    connect(m_starFilterBtn, &QToolButton::toggled, this, [this](bool checked) {
        m_proxy->setStarredOnly(checked);
        if (m_proxy->rowCount() > 0) {
            m_listView->setCurrentIndex(m_proxy->index(0, 0));
        }
        updateStatusLabel();
    });
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

    updatePasswordBanner();
    updateStatusLabel();
    updatePersistentUi();
}

void KlipperPopup::showPositioned(const QPoint &anchorPos, bool anchorAbove)
{
    if (!isVisible()) {
        m_paster->captureTargetWindow(winId());
    }

    m_stack->setCurrentIndex(0);
    m_searchEdit->clear();
    m_starFilterBtn->setChecked(false);
    m_proxy->setStarredOnly(false);

    if (m_proxy->rowCount() > 0) {
        m_listView->setCurrentIndex(m_proxy->index(0, 0));
        m_listView->scrollToTop();
    }
    updatePasswordBanner();
    updateStatusLabel();

    QScreen *screen = QGuiApplication::screenAt(anchorPos);
    if (!screen) {
        screen = QGuiApplication::primaryScreen();
    }
    const QRect avail = screen ? screen->availableGeometry() : QRect(0, 0, 1920, 1080);

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
    if (ev->type() == QEvent::Hide) {
        m_pastingInProgress = false;
        ++m_pasteGeneration;
    } else if (ev->type() == QEvent::Enter && s->keepPopupOpen() && !m_pastingInProgress) {
        // Refresh target window when user switches to another app and hovers back over persistent popup
        m_paster->captureTargetWindow(winId());
    } else if (ev->type() == QEvent::WindowDeactivate
               && !m_suppressAutoHide
               && !m_pastingInProgress
               && (!s->keepPopupOpen() || s->closePopupOnFocusOut())
               && QApplication::activeModalWidget() == nullptr
               && QApplication::activePopupWidget() == nullptr) {
        m_lastHideTimeMs = QDateTime::currentMSecsSinceEpoch();
        hide();
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
                activateItem(cur);
            }
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
            activateItem(cur);
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
        if (ev->type() == QEvent::MouseMove) {
            auto *me = static_cast<QMouseEvent *>(ev);
            const QPoint pos = me->position().toPoint();
            const QModelIndex idx = m_listView->indexAt(pos);
            m_delegate->setHoveredIndex(idx, pos);
            m_listView->viewport()->update();

            if (idx.isValid()) {
                const QRect cardRect = m_listView->visualRect(idx).adjusted(6, 3, -6, -3);
                QString tip;
                if (m_delegate->hitTestButton(cardRect, idx, pos, &tip) != RowAction::None && !tip.isEmpty()) {
                    QToolTip::showText(me->globalPosition().toPoint(), tip, m_listView->viewport());
                } else {
                    QToolTip::hideText();
                }
            }
        } else if (ev->type() == QEvent::Leave) {
            m_delegate->setHoveredIndex(QModelIndex(), QPoint(-1, -1));
            m_listView->viewport()->update();
        } else if (ev->type() == QEvent::MouseButtonRelease) {
            auto *me = static_cast<QMouseEvent *>(ev);
            if (me->button() == Qt::LeftButton) {
                const QPoint pos = me->position().toPoint();
                const QModelIndex idx = m_listView->indexAt(pos);
                if (idx.isValid()) {
                    const QRect cardRect = m_listView->visualRect(idx).adjusted(6, 3, -6, -3);
                    const RowAction act = m_delegate->hitTestButton(cardRect, idx, pos);
                    switch (act) {
                    case RowAction::Star:
                        toggleStar(idx);
                        return true;
                    case RowAction::Action:
                        invokeItemAction(idx);
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
                        activateItem(idx);
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
    const Qt::KeyboardModifiers mods = ke->modifiers() & ~Qt::KeypadModifier;
    const bool hasQuickMod = (mods == Qt::AltModifier || mods == Qt::ControlModifier);
    const bool isBare = (mods == Qt::NoModifier);

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
        activateItem(m_proxy->index(targetRow, 0));
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

void KlipperPopup::activateItem(const QModelIndex &proxyIndex)
{
    const QModelIndex src = sourceIndexFor(proxyIndex);
    if (!src.isValid()) {
        return;
    }
    const auto *s = MklipperSettings::self();
    const QString uuid = src.data(HistoryModel::UuidRole).toString();
    m_model->moveToTop(uuid);
    if (!s->keepPopupOpen()) {
        hide();
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
        // Restore focus to the persistent popup so subsequent clicks outside it will trigger WindowDeactivate and close it
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
    const auto type = static_cast<HistoryItemType>(src.data(HistoryModel::TypeIntRole).toInt());
    if (type != HistoryItemType::Text) {
        return;
    }
    m_editingUuid = src.data(HistoryModel::UuidRole).toString();
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
    m_stack->setCurrentIndex(0);
    m_searchEdit->setFocus();
}

void KlipperPopup::updatePasswordBanner()
{
    m_passwordBanner->setVisible(m_model->clipboardHasPassword() || m_model->selectionHasPassword());
}

void KlipperPopup::updatePersistentUi()
{
    const bool persistent = MklipperSettings::self()->keepPopupOpen();
    if (m_closeBtn) {
        m_closeBtn->setVisible(persistent);
    }
}

void KlipperPopup::updateStatusLabel()
{
    const int visible = m_proxy->rowCount();
    const int total = m_model->rowCount();
    const int starred = m_model->starredCount();

    m_emptyLabel->setVisible(visible == 0);
    m_listView->setVisible(visible > 0);
    if (visible == 0) {
        if (!m_searchEdit->text().isEmpty() || m_starFilterBtn->isChecked()) {
            m_emptyLabel->setText(tr("No hay coincidencias."));
        } else {
            m_emptyLabel->setText(tr("El historial del portapapeles está vacío."));
        }
    }

    QString text = tr("%1 elementos").arg(total);
    if (starred > 0) {
        text += tr(" (%1 fijados)").arg(starred);
    }
    text += tr("  ·  1–0 / Clic: pegar  ·  Supr: borrar  ·  Esc: cerrar");
    m_statusLabel->setText(text);
}

#include "klipperpopup.moc"
