/*
    SPDX-FileCopyrightText: 2026 Mklipper contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "helpdialog.h"

#include "themestyle.h"
#include "translations.h"

#include <QApplication>
#include <QFrame>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QScrollArea>
#include <QTabWidget>
#include <QVBoxLayout>

namespace
{
FlatIcon iconFromName(const QString &name)
{
    const QString k = name.trimmed().toLower();
    if (k == QStringLiteral("clipboard")) return FlatIcon::Clipboard;
    if (k == QStringLiteral("search")) return FlatIcon::Search;
    if (k == QStringLiteral("star")) return FlatIcon::StarOutline;
    if (k == QStringLiteral("trash")) return FlatIcon::Trash;
    if (k == QStringLiteral("settings")) return FlatIcon::Settings;
    if (k == QStringLiteral("qr")) return FlatIcon::QrCode;
    if (k == QStringLiteral("edit")) return FlatIcon::Edit;
    if (k == QStringLiteral("link")) return FlatIcon::ExternalLink;
    if (k == QStringLiteral("bolt")) return FlatIcon::Bolt;
    if (k == QStringLiteral("lock")) return FlatIcon::Lock;
    if (k == QStringLiteral("copy")) return FlatIcon::Copy;
    if (k == QStringLiteral("update")) return FlatIcon::Update;
    if (k == QStringLiteral("sliders")) return FlatIcon::Sliders;
    if (k == QStringLiteral("keyboard")) return FlatIcon::Keyboard;
    if (k == QStringLiteral("history")) return FlatIcon::History;
    if (k == QStringLiteral("book")) return FlatIcon::Book;
    if (k == QStringLiteral("pause")) return FlatIcon::Pause;
    if (k == QStringLiteral("play")) return FlatIcon::Play;
    if (k == QStringLiteral("eye")) return FlatIcon::Eye;
    if (k == QStringLiteral("save")) return FlatIcon::Save;
    if (k == QStringLiteral("image")) return FlatIcon::Image;
    return FlatIcon::Info;
}
} // namespace

// ---------------------------------------------------------------------------
// ExpandableManualCard — clickable card that reveals detailed guidance
// ---------------------------------------------------------------------------
class ExpandableManualCard : public QFrame
{
    Q_OBJECT

public:
    ExpandableManualCard(const ManualCard &card, QWidget *parent = nullptr)
        : QFrame(parent)
        , m_icon(iconFromName(card.icon))
        , m_hasDetails(!card.details.trimmed().isEmpty())
    {
        setObjectName(QStringLiteral("mklipperManualCard"));
        setFrameShape(QFrame::NoFrame);
        if (m_hasDetails) {
            setCursor(Qt::PointingHandCursor);
            setFocusPolicy(Qt::StrongFocus);
        }

        auto *rootLayout = new QVBoxLayout(this);
        rootLayout->setContentsMargins(14, 12, 14, 12);
        rootLayout->setSpacing(0);

        auto *headerRow = new QHBoxLayout;
        headerRow->setContentsMargins(0, 0, 0, 0);
        headerRow->setSpacing(12);

        m_badge = new QFrame(this);
        m_badge->setFixedSize(36, 36);
        m_badge->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        auto *badgeLayout = new QVBoxLayout(m_badge);
        badgeLayout->setContentsMargins(0, 0, 0, 0);
        m_iconLabel = new QLabel(m_badge);
        m_iconLabel->setAlignment(Qt::AlignCenter);
        m_iconLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        badgeLayout->addWidget(m_iconLabel);
        headerRow->addWidget(m_badge, 0, Qt::AlignTop);

        auto *textCol = new QVBoxLayout;
        textCol->setContentsMargins(0, 0, 0, 0);
        textCol->setSpacing(3);

        m_titleLabel = new QLabel(card.title, this);
        QFont ctf = m_titleLabel->font();
        ctf.setBold(true);
        m_titleLabel->setFont(ctf);
        m_titleLabel->setWordWrap(true);
        m_titleLabel->setTextInteractionFlags(Qt::NoTextInteraction);
        m_titleLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);

        m_bodyLabel = new QLabel(card.body, this);
        m_bodyLabel->setTextFormat(Qt::RichText);
        m_bodyLabel->setWordWrap(true);
        m_bodyLabel->setTextInteractionFlags(Qt::NoTextInteraction);
        m_bodyLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        QFont cbf = m_bodyLabel->font();
        cbf.setPointSizeF(qMax(8.8, cbf.pointSizeF() - 0.4));
        m_bodyLabel->setFont(cbf);

        textCol->addWidget(m_titleLabel);
        textCol->addWidget(m_bodyLabel);
        headerRow->addLayout(textCol, 1);

        if (m_hasDetails) {
            m_chevronLabel = new QLabel(this);
            m_chevronLabel->setFixedSize(18, 18);
            m_chevronLabel->setAlignment(Qt::AlignCenter);
            m_chevronLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);
            headerRow->addWidget(m_chevronLabel, 0, Qt::AlignTop);
        }

        rootLayout->addLayout(headerRow);

        if (m_hasDetails) {
            m_detailsContainer = new QWidget(this);
            auto *detLayout = new QVBoxLayout(m_detailsContainer);
            detLayout->setContentsMargins(48, 10, 6, 2);
            detLayout->setSpacing(8);

            m_detailsDivider = new QFrame(m_detailsContainer);
            m_detailsDivider->setFixedHeight(1);
            m_detailsDivider->setFrameShape(QFrame::NoFrame);
            detLayout->addWidget(m_detailsDivider);

            m_detailsLabel = new QLabel(card.details, m_detailsContainer);
            m_detailsLabel->setTextFormat(Qt::RichText);
            m_detailsLabel->setWordWrap(true);
            m_detailsLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
            QFont df = m_detailsLabel->font();
            df.setPointSizeF(qMax(8.8, df.pointSizeF() - 0.35));
            m_detailsLabel->setFont(df);
            detLayout->addWidget(m_detailsLabel);

            m_detailsContainer->setVisible(false);
            rootLayout->addWidget(m_detailsContainer);
        }

        updateTooltip();
    }

    bool isExpanded() const { return m_expanded; }

    void setExpanded(bool expanded)
    {
        if (!m_hasDetails || m_expanded == expanded) {
            return;
        }
        m_expanded = expanded;
        if (m_detailsContainer) {
            m_detailsContainer->setVisible(m_expanded);
        }
        updateTooltip();
        applyTheme(m_lastTokens, m_lastPalette);
        updateGeometry();
    }

    void toggleExpanded()
    {
        setExpanded(!m_expanded);
    }

    void applyTheme(const ThemeTokens &t, const QPalette &appPal)
    {
        m_lastTokens = t;
        m_lastPalette = appPal;

        const QColor borderCol = m_expanded
            ? QColor(t.accent.red(), t.accent.green(), t.accent.blue(), t.isDark ? 128 : 110)
            : t.hairline;
        const QColor hoverBorder = m_expanded ? borderCol : t.borderSubtle;
        const QString hoverBg = m_hasDetails ? ThemeStyle::rgbaCss(t.surfaceHoverBg) : ThemeStyle::rgbaCss(t.surfaceBg);

        setStyleSheet(QStringLiteral(
                          "QFrame#mklipperManualCard {"
                          "  background-color: %1;"
                          "  border: 1px solid %2;"
                          "  border-radius: 10px;"
                          "}"
                          "QFrame#mklipperManualCard:hover {"
                          "  background-color: %3;"
                          "  border: 1px solid %4;"
                          "}")
                          .arg(ThemeStyle::rgbaCss(t.surfaceBg),
                               ThemeStyle::rgbaCss(borderCol),
                               m_expanded ? ThemeStyle::rgbaCss(t.surfaceBg) : hoverBg,
                               ThemeStyle::rgbaCss(hoverBorder)));

        if (m_badge) {
            m_badge->setStyleSheet(
                QStringLiteral("QFrame { background-color: %1; border: none; border-radius: 18px; }")
                    .arg(ThemeStyle::rgbaCss(m_expanded ? t.accentSoftStrong : t.accentSoft)));
        }
        if (m_iconLabel) {
            m_iconLabel->setPixmap(FlatIcons::pixmap(m_icon, t.accent, 17, devicePixelRatioF()));
        }
        if (m_titleLabel) {
            QPalette tp = appPal;
            tp.setColor(QPalette::WindowText, t.textPrimary);
            m_titleLabel->setPalette(tp);
        }
        if (m_bodyLabel) {
            QPalette bp = appPal;
            bp.setColor(QPalette::WindowText, t.textSecondary);
            m_bodyLabel->setPalette(bp);
        }
        if (m_chevronLabel) {
            m_chevronLabel->setPixmap(FlatIcons::pixmap(
                m_expanded ? FlatIcon::ChevronUp : FlatIcon::ChevronDown,
                m_expanded ? t.accent : t.textMuted,
                15,
                devicePixelRatioF()));
        }
        if (m_detailsDivider) {
            m_detailsDivider->setStyleSheet(
                QStringLiteral("QFrame { background-color: %1; border: none; }")
                    .arg(ThemeStyle::rgbaCss(t.hairline)));
        }
        if (m_detailsLabel) {
            QPalette dp = appPal;
            dp.setColor(QPalette::WindowText, t.textPrimary);
            m_detailsLabel->setPalette(dp);
        }
    }

protected:
    void mouseReleaseEvent(QMouseEvent *ev) override
    {
        if (m_hasDetails && ev->button() == Qt::LeftButton && rect().contains(ev->position().toPoint())) {
            toggleExpanded();
            ev->accept();
            return;
        }
        QFrame::mouseReleaseEvent(ev);
    }

    void keyPressEvent(QKeyEvent *ev) override
    {
        if (m_hasDetails && (ev->key() == Qt::Key_Space || ev->key() == Qt::Key_Return || ev->key() == Qt::Key_Enter)) {
            toggleExpanded();
            ev->accept();
            return;
        }
        QFrame::keyPressEvent(ev);
    }

private:
    void updateTooltip()
    {
        if (!m_hasDetails) {
            return;
        }
        setToolTip(m_expanded ? tr("Haz clic para contraer") : tr("Haz clic para ver la explicación detallada"));
    }

    FlatIcon m_icon = FlatIcon::Info;
    bool m_hasDetails = false;
    bool m_expanded = false;
    ThemeTokens m_lastTokens;
    QPalette m_lastPalette;

    QFrame *m_badge = nullptr;
    QLabel *m_iconLabel = nullptr;
    QLabel *m_titleLabel = nullptr;
    QLabel *m_bodyLabel = nullptr;
    QLabel *m_chevronLabel = nullptr;
    QWidget *m_detailsContainer = nullptr;
    QFrame *m_detailsDivider = nullptr;
    QLabel *m_detailsLabel = nullptr;
};

// ---------------------------------------------------------------------------
// HelpDialog
// ---------------------------------------------------------------------------
HelpDialog::HelpDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Manual de usuario — Mklipper"));
    setMinimumSize(620, 500);
    resize(700, 620);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(18, 18, 18, 16);
    mainLayout->setSpacing(12);

    // Header
    auto *headerLayout = new QHBoxLayout;
    headerLayout->setSpacing(12);

    m_logoLabel = new QLabel(this);
    m_logoLabel->setFixedSize(38, 38);
    headerLayout->addWidget(m_logoLabel);

    auto *titleLayout = new QVBoxLayout;
    titleLayout->setSpacing(1);

    auto *titleLabel = new QLabel(tr("Manual de usuario de Mklipper"), this);
    QFont tf = titleLabel->font();
    tf.setBold(true);
    tf.setPointSizeF(tf.pointSizeF() + 2.0);
    titleLabel->setFont(tf);

    m_subtitleLabel = new QLabel(tr("Guía rápida para aprovechar tu historial del portapapeles en Linux Mint"), this);
    QFont sf = m_subtitleLabel->font();
    sf.setPointSizeF(qMax(8.5, sf.pointSizeF() - 1.0));
    m_subtitleLabel->setFont(sf);
    m_mutedLabels.append(m_subtitleLabel);

    titleLayout->addWidget(titleLabel);
    titleLayout->addWidget(m_subtitleLabel);
    headerLayout->addLayout(titleLayout, 1);
    mainLayout->addLayout(headerLayout);

    // Friendly sectioned tabs loaded dynamically from active language JSON
    m_tabs = new QTabWidget(this);
    m_tabs->setIconSize(QSize(15, 15));

    const QList<ManualSection> sections = MklipperTranslations::userManualSections();
    for (int i = 0; i < sections.size(); ++i) {
        const ManualSection &sec = sections.at(i);
        auto *page = new QWidget(m_tabs);
        auto *pageLayout = new QVBoxLayout(page);
        pageLayout->setContentsMargins(2, 8, 6, 8);
        pageLayout->setSpacing(10);

        if (!sec.intro.isEmpty()) {
            auto *introLabel = new QLabel(sec.intro, page);
            introLabel->setWordWrap(true);
            introLabel->setContentsMargins(4, 0, 4, 2);
            pageLayout->addWidget(introLabel);
            m_mutedLabels.append(introLabel);
        }

        for (const ManualCard &c : sec.cards) {
            auto *card = new ExpandableManualCard(c, page);
            m_cards.append(card);
            pageLayout->addWidget(card);
        }

        pageLayout->addStretch();

        auto *scroll = new QScrollArea(m_tabs);
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        scroll->setWidget(page);

        const int tabIdx = m_tabs->addTab(scroll, sec.title);
        m_tabIcons.append({tabIdx, iconFromName(sec.icon)});
    }

    mainLayout->addWidget(m_tabs, 1);

    // Footer
    auto *divider = new QFrame(this);
    divider->setObjectName(QStringLiteral("mklipperDivider"));
    mainLayout->addWidget(divider);

    auto *btnRow = new QHBoxLayout;
    btnRow->setSpacing(8);

    m_footerHintIcon = new QLabel(this);
    m_footerHintIcon->setFixedSize(15, 15);
    m_footerHintLabel = new QLabel(tr("Haz clic en cualquier tarjeta para expandir la explicación detallada."), this);
    QFont hf = m_footerHintLabel->font();
    hf.setPointSizeF(qMax(8.3, hf.pointSizeF() - 1.1));
    m_footerHintLabel->setFont(hf);
    m_mutedLabels.append(m_footerHintLabel);

    btnRow->addWidget(m_footerHintIcon);
    btnRow->addWidget(m_footerHintLabel, 1);

    auto *closeBtn = new QPushButton(tr("Cerrar"), this);
    closeBtn->setProperty("primary", true);
    closeBtn->setCursor(Qt::PointingHandCursor);
    closeBtn->setDefault(true);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    btnRow->addWidget(closeBtn);
    mainLayout->addLayout(btnRow);

    refreshTheme();
}

void HelpDialog::refreshTheme()
{
    const QPalette appPal = QApplication::palette();
    setPalette(appPal);
    const ThemeTokens t = ThemeTokens::fromPalette(appPal);
    setWindowIcon(FlatIcons::appLogoIcon(t.accent));
    setStyleSheet(ThemeStyle::dialogStyleSheet(t));

    if (m_logoLabel) {
        m_logoLabel->setPixmap(FlatIcons::appLogoPixmap(t.accent, 38, devicePixelRatioF()));
    }
    if (m_footerHintIcon) {
        m_footerHintIcon->setPixmap(FlatIcons::pixmap(FlatIcon::Info, t.accent, 15, devicePixelRatioF()));
    }
    for (QLabel *lbl : std::as_const(m_mutedLabels)) {
        if (!lbl) {
            continue;
        }
        QPalette lp = appPal;
        lp.setColor(QPalette::WindowText, t.textSecondary);
        lbl->setPalette(lp);
    }
    for (ExpandableManualCard *card : std::as_const(m_cards)) {
        if (card) {
            card->applyTheme(t, appPal);
        }
    }
    for (const auto &ti : std::as_const(m_tabIcons)) {
        if (m_tabs) {
            m_tabs->setTabIcon(ti.index, FlatIcons::icon(ti.icon, t.textSecondary, t.textPrimary, t.textMuted, 15));
        }
    }
    update();
}

#include "helpdialog.moc"
