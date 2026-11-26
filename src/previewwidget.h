/*
    SPDX-FileCopyrightText: 2026 Mklipper contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QImage>
#include <QString>
#include <QWidget>

#include "historyitem.h"

class QFrame;
class QLabel;
class QPlainTextEdit;
class QPushButton;
class QStackedWidget;

class PreviewWidget : public QWidget
{
    Q_OBJECT

public:
    explicit PreviewWidget(QWidget *parent = nullptr);

    void setTextPreview(const QString &uuid,
                        HistoryItemType type,
                        const QString &text,
                        const QString &metaLine);
    void setImagePreview(const QString &uuid,
                         const QString &imagePath,
                         const QString &metaLine);
    void refreshTheme();

Q_SIGNALS:
    void backRequested();
    void activateRequested(const QString &uuid);
    void editRequested(const QString &uuid);
    void saveImageRequested(const QString &uuid);

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    void updateScaledImage();

    QString m_uuid;
    HistoryItemType m_type = HistoryItemType::Text;
    QImage m_sourceImage;

    QPushButton *m_backBtn = nullptr;
    QLabel *m_titleLabel = nullptr;
    QLabel *m_metaLabel = nullptr;
    QStackedWidget *m_contentStack = nullptr;
    QPlainTextEdit *m_textView = nullptr;
    QFrame *m_imageCard = nullptr;
    QLabel *m_imageLabel = nullptr;
    QPushButton *m_editBtn = nullptr;
    QPushButton *m_saveImageBtn = nullptr;
    QPushButton *m_useBtn = nullptr;
};
