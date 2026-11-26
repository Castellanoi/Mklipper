/*
    SPDX-FileCopyrightText: 2026 Mklipper contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "qrcodewidget.h"

#include "flaticons.h"
#include "themestyle.h"

#include <QApplication>
#include <QClipboard>
#include <QFileDialog>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QVBoxLayout>

#include <qrencode.h>

QRCodeWidget::QRCodeWidget(QWidget *parent)
    : QWidget(parent)
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 10, 12, 12);
    mainLayout->setSpacing(10);

    auto *headerLayout = new QHBoxLayout;
    headerLayout->setSpacing(6);
    m_backBtn = new QPushButton(tr("Volver"), this);
    m_backBtn->setProperty("ghost", true);
    m_backBtn->setIconSize(QSize(15, 15));
    m_backBtn->setCursor(Qt::PointingHandCursor);
    connect(m_backBtn, &QPushButton::clicked, this, &QRCodeWidget::backRequested);

    m_titleLabel = new QLabel(tr("Código QR"), this);
    QFont titleFont = m_titleLabel->font();
    titleFont.setBold(true);
    titleFont.setPointSizeF(titleFont.pointSizeF() + 0.5);
    m_titleLabel->setFont(titleFont);

    auto *rightSpacer = new QWidget(this);
    rightSpacer->setFixedWidth(m_backBtn->sizeHint().width());
    headerLayout->addWidget(m_backBtn);
    headerLayout->addStretch();
    headerLayout->addWidget(m_titleLabel);
    headerLayout->addStretch();
    headerLayout->addWidget(rightSpacer);
    mainLayout->addLayout(headerLayout);

    auto *divider = new QFrame(this);
    divider->setObjectName(QStringLiteral("mklipperDivider"));
    mainLayout->addWidget(divider);

    m_qrCard = new QFrame(this);
    m_qrCard->setObjectName(QStringLiteral("mklipperQrCard"));
    auto *qrCardLayout = new QVBoxLayout(m_qrCard);
    qrCardLayout->setContentsMargins(14, 14, 14, 14);

    m_qrLabel = new QLabel(m_qrCard);
    m_qrLabel->setAlignment(Qt::AlignCenter);
    m_qrLabel->setMinimumSize(220, 220);
    m_qrLabel->setStyleSheet(QStringLiteral("QLabel { color: #333333; background: transparent; }"));
    qrCardLayout->addWidget(m_qrLabel, 1);
    mainLayout->addWidget(m_qrCard, 1, Qt::AlignCenter);

    m_previewLabel = new QLabel(this);
    m_previewLabel->setAlignment(Qt::AlignCenter);
    m_previewLabel->setTextFormat(Qt::PlainText);
    m_previewLabel->setWordWrap(true);
    m_previewLabel->setMaximumHeight(44);
    mainLayout->addWidget(m_previewLabel);

    auto *btnLayout = new QHBoxLayout;
    btnLayout->setSpacing(8);
    m_saveBtn = new QPushButton(tr("Guardar PNG"), this);
    m_saveBtn->setProperty("flatAction", true);
    m_saveBtn->setIconSize(QSize(15, 15));
    m_saveBtn->setCursor(Qt::PointingHandCursor);

    m_copyBtn = new QPushButton(tr("Copiar QR"), this);
    m_copyBtn->setProperty("primary", true);
    m_copyBtn->setIconSize(QSize(15, 15));
    m_copyBtn->setCursor(Qt::PointingHandCursor);

    connect(m_copyBtn, &QPushButton::clicked, this, &QRCodeWidget::copyQrImage);
    connect(m_saveBtn, &QPushButton::clicked, this, &QRCodeWidget::saveQrImage);
    btnLayout->addStretch();
    btnLayout->addWidget(m_saveBtn);
    btnLayout->addWidget(m_copyBtn);
    btnLayout->addStretch();
    mainLayout->addLayout(btnLayout);

    refreshTheme();
}

void QRCodeWidget::refreshTheme()
{
    const QPalette appPal = QApplication::palette();
    const ThemeTokens t = ThemeTokens::fromPalette(appPal);

    m_backBtn->setIcon(FlatIcons::icon(FlatIcon::Back, t.textSecondary, t.textPrimary, t.textMuted, 15));
    m_saveBtn->setIcon(FlatIcons::icon(FlatIcon::Save, t.textPrimary, t.textPrimary, t.textMuted, 15));
    m_copyBtn->setIcon(FlatIcons::icon(FlatIcon::Copy, t.accentFg, t.accentFg, t.textMuted, 15));

    m_qrCard->setStyleSheet(
        QStringLiteral("QFrame#mklipperQrCard { background-color: #ffffff; border: 1px solid %1; border-radius: 12px; }")
            .arg(ThemeStyle::rgbaCss(t.borderSubtle)));

    if (m_titleLabel) {
        QPalette tp = appPal;
        tp.setColor(QPalette::WindowText, t.textPrimary);
        m_titleLabel->setPalette(tp);
    }
    QPalette pp = appPal;
    pp.setColor(QPalette::WindowText, t.textMuted);
    m_previewLabel->setPalette(pp);
    update();
}

void QRCodeWidget::setText(const QString &text)
{
    const QByteArray utf8 = text.toUtf8();
    m_qrImage = QImage();

    constexpr qsizetype kMaxQrPayloadBytes = 2331;
    QRcode *qr = (!utf8.isEmpty() && utf8.size() <= kMaxQrPayloadBytes)
        ? QRcode_encodeData(static_cast<int>(utf8.size()),
                            reinterpret_cast<const unsigned char *>(utf8.constData()),
                            0,
                            QR_ECLEVEL_M)
        : nullptr;
    if (!qr) {
        m_qrLabel->setPixmap(QPixmap());
        m_qrLabel->setText(tr("No se pudo generar el código QR\n(el texto es demasiado largo o está vacío)."));
        m_copyBtn->setEnabled(false);
        m_saveBtn->setEnabled(false);
        m_previewLabel->clear();
        return;
    }

    constexpr int quietZone = 4;
    const int modules = qr->width + quietZone * 2;
    const int scale = qMax(4, 240 / modules);
    const int imgSize = modules * scale;

    QImage img(imgSize, imgSize, QImage::Format_RGB32);
    img.fill(Qt::white);

    QPainter painter(&img);
    painter.setPen(Qt::NoPen);
    painter.setBrush(Qt::black);

    for (int y = 0; y < qr->width; ++y) {
        for (int x = 0; x < qr->width; ++x) {
            if (qr->data[y * qr->width + x] & 1) {
                painter.drawRect((x + quietZone) * scale, (y + quietZone) * scale, scale, scale);
            }
        }
    }
    painter.end();
    QRcode_free(qr);

    m_qrImage = img;
    m_qrLabel->setPixmap(QPixmap::fromImage(img).scaled(220, 220, Qt::KeepAspectRatio, Qt::FastTransformation));
    m_copyBtn->setEnabled(true);
    m_saveBtn->setEnabled(true);

    QString summary = text.simplified();
    if (summary.size() > 90) {
        summary = summary.left(87) + QStringLiteral("…");
    }
    m_previewLabel->setText(summary);
}

void QRCodeWidget::copyQrImage()
{
    if (!m_qrImage.isNull()) {
        QGuiApplication::clipboard()->setImage(m_qrImage, QClipboard::Clipboard);
    }
}

void QRCodeWidget::saveQrImage()
{
    if (m_qrImage.isNull()) {
        return;
    }
    const QString path = QFileDialog::getSaveFileName(this, tr("Guardar código QR"), QStringLiteral("mklipper-qr.png"), tr("Imagen PNG (*.png)"));
    if (!path.isEmpty()) {
        m_qrImage.save(path, "PNG");
    }
}
