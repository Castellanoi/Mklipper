/*
    SPDX-FileCopyrightText: 2026 Mklipper contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "qrcodewidget.h"

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
    auto *backBtn = new QPushButton(QIcon::fromTheme(QStringLiteral("go-previous")), tr("Volver"), this);
    backBtn->setFlat(true);
    backBtn->setCursor(Qt::PointingHandCursor);
    connect(backBtn, &QPushButton::clicked, this, &QRCodeWidget::backRequested);

    auto *titleLabel = new QLabel(tr("Código QR"), this);
    QFont titleFont = titleLabel->font();
    titleFont.setBold(true);
    titleFont.setPointSizeF(titleFont.pointSizeF() + 0.5);
    titleLabel->setFont(titleFont);

    headerLayout->addWidget(backBtn);
    headerLayout->addStretch();
    headerLayout->addWidget(titleLabel);
    headerLayout->addStretch();
    mainLayout->addLayout(headerLayout);

    auto *qrCard = new QFrame(this);
    qrCard->setObjectName(QStringLiteral("mklipperQrCard"));
    qrCard->setStyleSheet(QStringLiteral(
        "QFrame#mklipperQrCard { background-color: #ffffff; border: 1px solid palette(mid); border-radius: 12px; }"));
    auto *qrCardLayout = new QVBoxLayout(qrCard);
    qrCardLayout->setContentsMargins(14, 14, 14, 14);

    m_qrLabel = new QLabel(qrCard);
    m_qrLabel->setAlignment(Qt::AlignCenter);
    m_qrLabel->setMinimumSize(220, 220);
    m_qrLabel->setStyleSheet(QStringLiteral("QLabel { color: #333333; background: transparent; }"));
    qrCardLayout->addWidget(m_qrLabel, 1);
    mainLayout->addWidget(qrCard, 1, Qt::AlignCenter);

    m_previewLabel = new QLabel(this);
    m_previewLabel->setAlignment(Qt::AlignCenter);
    m_previewLabel->setTextFormat(Qt::PlainText);
    m_previewLabel->setWordWrap(true);
    m_previewLabel->setMaximumHeight(44);
    QPalette pp = m_previewLabel->palette();
    QColor pCol = pp.color(QPalette::WindowText);
    pCol.setAlpha(170);
    pp.setColor(QPalette::WindowText, pCol);
    m_previewLabel->setPalette(pp);
    mainLayout->addWidget(m_previewLabel);

    auto *btnLayout = new QHBoxLayout;
    btnLayout->setSpacing(8);
    m_copyBtn = new QPushButton(QIcon::fromTheme(QStringLiteral("edit-copy")), tr("Copiar QR"), this);
    m_copyBtn->setCursor(Qt::PointingHandCursor);
    m_saveBtn = new QPushButton(QIcon::fromTheme(QStringLiteral("document-save")), tr("Guardar PNG"), this);
    m_saveBtn->setCursor(Qt::PointingHandCursor);
    connect(m_copyBtn, &QPushButton::clicked, this, &QRCodeWidget::copyQrImage);
    connect(m_saveBtn, &QPushButton::clicked, this, &QRCodeWidget::saveQrImage);
    btnLayout->addStretch();
    btnLayout->addWidget(m_copyBtn);
    btnLayout->addWidget(m_saveBtn);
    btnLayout->addStretch();
    mainLayout->addLayout(btnLayout);
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
