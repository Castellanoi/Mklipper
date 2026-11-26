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
#include <QLabel>
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
    return FlatIcon::Info;
}
} // namespace

HelpDialog::HelpDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Manual de usuario — Mklipper"));
    setMinimumSize(620, 500);
    resize(680, 595);

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
            auto *card = new QFrame(page);
            card->setObjectName(QStringLiteral("mklipperCard"));
            auto *cardLayout = new QHBoxLayout(card);
            cardLayout->setContentsMargins(14, 12, 14, 12);
            cardLayout->setSpacing(12);

            auto *badge = new QFrame(card);
            badge->setFixedSize(36, 36);
            auto *badgeLayout = new QVBoxLayout(badge);
            badgeLayout->setContentsMargins(0, 0, 0, 0);
            auto *iconLbl = new QLabel(badge);
            iconLbl->setAlignment(Qt::AlignCenter);
            badgeLayout->addWidget(iconLbl);

            m_cardIcons.append({badge, iconLbl, iconFromName(c.icon)});
            cardLayout->addWidget(badge, 0, Qt::AlignTop);

            auto *textCol = new QVBoxLayout;
            textCol->setSpacing(3);

            auto *cardTitle = new QLabel(c.title, card);
            QFont ctf = cardTitle->font();
            ctf.setBold(true);
            cardTitle->setFont(ctf);
            cardTitle->setWordWrap(true);

            auto *cardBody = new QLabel(c.body, card);
            cardBody->setTextFormat(Qt::RichText);
            cardBody->setWordWrap(true);
            cardBody->setTextInteractionFlags(Qt::TextSelectableByMouse);
            QFont cbf = cardBody->font();
            cbf.setPointSizeF(qMax(8.8, cbf.pointSizeF() - 0.4));
            cardBody->setFont(cbf);
            m_mutedLabels.append(cardBody);

            textCol->addWidget(cardTitle);
            textCol->addWidget(cardBody);
            cardLayout->addLayout(textCol, 1);

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
    btnRow->addStretch();
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
    for (QLabel *lbl : std::as_const(m_mutedLabels)) {
        if (!lbl) {
            continue;
        }
        QPalette lp = appPal;
        lp.setColor(QPalette::WindowText, t.textSecondary);
        lbl->setPalette(lp);
    }
    const QString badgeCss = QStringLiteral("QFrame { background-color: %1; border: none; border-radius: 18px; }")
                                 .arg(ThemeStyle::rgbaCss(t.accentSoft));
    for (const auto &ci : std::as_const(m_cardIcons)) {
        if (ci.badge) {
            ci.badge->setStyleSheet(badgeCss);
        }
        if (ci.iconLabel) {
            ci.iconLabel->setPixmap(FlatIcons::pixmap(ci.icon, t.accent, 17, devicePixelRatioF()));
        }
    }
    for (const auto &ti : std::as_const(m_tabIcons)) {
        if (m_tabs) {
            m_tabs->setTabIcon(ti.index, FlatIcons::icon(ti.icon, t.textSecondary, t.textPrimary, t.textMuted, 15));
        }
    }
    update();
}
