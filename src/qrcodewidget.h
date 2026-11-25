/*
    SPDX-FileCopyrightText: 2026 Mklipper contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QImage>
#include <QString>
#include <QWidget>

class QLabel;
class QPushButton;

class QRCodeWidget : public QWidget
{
    Q_OBJECT

public:
    explicit QRCodeWidget(QWidget *parent = nullptr);

    void setText(const QString &text);

Q_SIGNALS:
    void backRequested();

private Q_SLOTS:
    void copyQrImage();
    void saveQrImage();

private:
    QLabel *m_qrLabel = nullptr;
    QLabel *m_previewLabel = nullptr;
    QPushButton *m_copyBtn = nullptr;
    QPushButton *m_saveBtn = nullptr;
    QImage m_qrImage;
};
