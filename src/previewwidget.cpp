/*
    SPDX-FileCopyrightText: 2026 Mklipper contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "previewwidget.h"

#include "flaticons.h"
#include "themestyle.h"

#include <QApplication>
#include <QFrame>
#include <QHBoxLayout>
#include <QImageReader>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QResizeEvent>
#include <QStackedWidget>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;

PreviewWidget::PreviewWidget(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 10, 12, 12);
    layout->setSpacing(10);

    // Header
    auto *header = new QHBoxLayout;
    header->setSpacing(6);

    m_backBtn = new QPushButton(tr("Volver"), this);
    m_backBtn->setProperty("ghost", true);
    m_backBtn->setIconSize(QSize(15, 15));
    m_backBtn->setCursor(Qt::PointingHandCursor);
    connect(m_backBtn, &QPushButton::clicked, this, &PreviewWidget::backRequested);

    m_titleLabel = new QLabel(tr("Vista previa"), this);
    QFont titleFont = m_titleLabel->font();
    titleFont.setBold(true);
    titleFont.setPointSizeF(titleFont.pointSizeF() + 0.5);
    m_titleLabel->setFont(titleFont);

    auto *rightSpacer = new QWidget(this);
    rightSpacer->setFixedWidth(m_backBtn->sizeHint().width());

    header->addWidget(m_backBtn);
    header->addStretch();
    header->addWidget(m_titleLabel);
    header->addStretch();
    header->addWidget(rightSpacer);
    layout->addLayout(header);

    auto *divider = new QFrame(this);
    divider->setObjectName(u"mklipperDivider"_s);
    layout->addWidget(divider);

    m_metaLabel = new QLabel(this);
    m_metaLabel->setTextFormat(Qt::PlainText);
    m_metaLabel->setWordWrap(true);
    QFont metaFont = m_metaLabel->font();
    metaFont.setPointSizeF(qMax(8.0, metaFont.pointSizeF() - 1.3));
    m_metaLabel->setFont(metaFont);
    layout->addWidget(m_metaLabel);

    m_contentStack = new QStackedWidget(this);

    // Content page 0: Text / Link preview
    m_textView = new QPlainTextEdit(m_contentStack);
    m_textView->setObjectName(u"mklipperEditArea"_s);
    m_textView->setReadOnly(true);
    m_contentStack->addWidget(m_textView);

    // Content page 1: Image preview
    m_imageCard = new QFrame(m_contentStack);
    m_imageCard->setObjectName(u"mklipperPreviewCard"_s);
    m_imageCard->setFrameShape(QFrame::NoFrame);
    auto *imgLayout = new QVBoxLayout(m_imageCard);
    imgLayout->setContentsMargins(10, 10, 10, 10);
    m_imageLabel = new QLabel(m_imageCard);
    m_imageLabel->setAlignment(Qt::AlignCenter);
    m_imageLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    imgLayout->addWidget(m_imageLabel, 1);
    m_contentStack->addWidget(m_imageCard);

    layout->addWidget(m_contentStack, 1);

    // Action buttons
    auto *btnRow = new QHBoxLayout;
    btnRow->setSpacing(8);

    m_editBtn = new QPushButton(tr("Editar"), this);
    m_editBtn->setProperty("flatAction", true);
    m_editBtn->setIconSize(QSize(15, 15));
    m_editBtn->setCursor(Qt::PointingHandCursor);
    connect(m_editBtn, &QPushButton::clicked, this, [this]() {
        if (!m_uuid.isEmpty()) {
            Q_EMIT editRequested(m_uuid);
        }
    });

    m_saveImageBtn = new QPushButton(tr("Guardar PNG"), this);
    m_saveImageBtn->setProperty("flatAction", true);
    m_saveImageBtn->setIconSize(QSize(15, 15));
    m_saveImageBtn->setCursor(Qt::PointingHandCursor);
    connect(m_saveImageBtn, &QPushButton::clicked, this, [this]() {
        if (!m_uuid.isEmpty()) {
            Q_EMIT saveImageRequested(m_uuid);
        }
    });

    m_useBtn = new QPushButton(tr("Usar elemento"), this);
    m_useBtn->setProperty("primary", true);
    m_useBtn->setIconSize(QSize(15, 15));
    m_useBtn->setCursor(Qt::PointingHandCursor);
    connect(m_useBtn, &QPushButton::clicked, this, [this]() {
        if (!m_uuid.isEmpty()) {
            Q_EMIT activateRequested(m_uuid);
        }
    });

    btnRow->addStretch();
    btnRow->addWidget(m_editBtn);
    btnRow->addWidget(m_saveImageBtn);
    btnRow->addWidget(m_useBtn);
    layout->addLayout(btnRow);

    refreshTheme();
}

void PreviewWidget::setTextPreview(const QString &uuid,
                                   HistoryItemType type,
                                   const QString &text,
                                   const QString &metaLine)
{
    m_uuid = uuid;
    m_type = type;
    m_sourceImage = {};
    m_imageLabel->clear();

    const int chars = text.size();
    const int lines = text.isEmpty() ? 0 : text.count(QLatin1Char('\n')) + 1;
    QString fullMeta = tr("%1 caracteres · %2 líneas").arg(chars).arg(lines);
    if (!metaLine.isEmpty()) {
        fullMeta += u"  ·  "_s + metaLine;
    }
    m_metaLabel->setText(fullMeta);
    m_metaLabel->setVisible(true);

    m_textView->setPlainText(text);
    m_contentStack->setCurrentIndex(0);

    m_editBtn->setVisible(type == HistoryItemType::Text);
    m_saveImageBtn->setVisible(false);
}

void PreviewWidget::setImagePreview(const QString &uuid,
                                    const QString &imagePath,
                                    const QString &metaLine)
{
    m_uuid = uuid;
    m_type = HistoryItemType::Image;
    m_textView->clear();

    QImageReader reader(imagePath);
    reader.setAutoTransform(true);
    m_sourceImage = reader.read();

    QString fullMeta;
    if (!m_sourceImage.isNull()) {
        fullMeta = tr("Imagen %1×%2").arg(m_sourceImage.width()).arg(m_sourceImage.height());
    } else {
        fullMeta = tr("Imagen");
    }
    if (!metaLine.isEmpty()) {
        fullMeta += u"  ·  "_s + metaLine;
    }
    m_metaLabel->setText(fullMeta);
    m_metaLabel->setVisible(true);

    m_contentStack->setCurrentIndex(1);
    m_editBtn->setVisible(false);
    m_saveImageBtn->setVisible(!m_sourceImage.isNull());

    updateScaledImage();
}

void PreviewWidget::clearPreview()
{
    m_uuid.clear();
    m_sourceImage = {};
    if (m_imageLabel) {
        m_imageLabel->clear();
    }
    if (m_textView) {
        m_textView->clear();
    }
    if (m_metaLabel) {
        m_metaLabel->clear();
    }
}

void PreviewWidget::refreshTheme()
{
    const QPalette appPal = QApplication::palette();
    const ThemeTokens t = ThemeTokens::fromPalette(appPal);

    if (m_backBtn) {
        m_backBtn->setIcon(FlatIcons::icon(FlatIcon::Back, t.textSecondary, t.textPrimary, t.textMuted, 15));
    }
    if (m_titleLabel) {
        QPalette tp = appPal;
        tp.setColor(QPalette::WindowText, t.textPrimary);
        m_titleLabel->setPalette(tp);
    }
    if (m_metaLabel) {
        QPalette mp = appPal;
        mp.setColor(QPalette::WindowText, t.textMuted);
        m_metaLabel->setPalette(mp);
    }
    if (m_imageCard) {
        m_imageCard->setStyleSheet(
            QStringLiteral("QFrame#mklipperPreviewCard { background-color: %1; border: 1px solid %2; border-radius: 10px; }")
                .arg(ThemeStyle::rgbaCss(t.surfaceBg), ThemeStyle::rgbaCss(t.hairline)));
    }
    if (m_editBtn) {
        m_editBtn->setIcon(FlatIcons::icon(FlatIcon::Edit, t.textPrimary, t.textPrimary, t.textMuted, 15));
    }
    if (m_saveImageBtn) {
        m_saveImageBtn->setIcon(FlatIcons::icon(FlatIcon::Save, t.textPrimary, t.textPrimary, t.textMuted, 15));
    }
    if (m_useBtn) {
        m_useBtn->setIcon(FlatIcons::icon(FlatIcon::Copy, t.accentFg, t.accentFg, t.textMuted, 15));
    }
}

void PreviewWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (m_type == HistoryItemType::Image && !m_sourceImage.isNull()) {
        updateScaledImage();
    }
}

void PreviewWidget::updateScaledImage()
{
    if (m_sourceImage.isNull() || !m_imageLabel) {
        return;
    }
    const qreal dpr = qMax<qreal>(1.0, devicePixelRatioF());
    const QSize avail = m_imageLabel->size().expandedTo(QSize(240, 220));
    const QSize targetPx(qRound(avail.width() * dpr), qRound(avail.height() * dpr));
    QImage scaled = m_sourceImage.scaled(targetPx, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    QPixmap pm = QPixmap::fromImage(scaled);
    pm.setDevicePixelRatio(dpr);
    m_imageLabel->setPixmap(pm);
}
