/*
    SPDX-FileCopyrightText: 2026 Mklipper contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QFrame>
#include <QList>
#include <QString>

#include "hovertooltip.h"

class QAbstractButton;
class QAction;
class QButtonGroup;
class QLabel;
class QLineEdit;
class QPushButton;
class QToolButton;
class PreviewSampleList;

class TooltipPreviewWidget : public QFrame
{
    Q_OBJECT

public:
    explicit TooltipPreviewWidget(QWidget *parent = nullptr);
    ~TooltipPreviewWidget() override = default;

    void setTooltipConfig(const TooltipConfig &config);
    TooltipConfig tooltipConfig() const;

    void refreshTheme();

    HoverTooltipController *tooltipController() const { return m_tooltipController; }

protected:
    bool event(QEvent *ev) override;
    void changeEvent(QEvent *ev) override;

private:
    void applyTheme();
    void updatePauseUi();
    void updatePlainTextUi();
    void updateStatusLabel();

    HoverTooltipController *m_tooltipController = nullptr;

    QLineEdit *m_searchEdit = nullptr;
    QAction *m_searchIconAction = nullptr;
    QToolButton *m_pauseBtn = nullptr;
    QToolButton *m_clearBtn = nullptr;
    QToolButton *m_configBtn = nullptr;
    QToolButton *m_closeBtn = nullptr;
    QButtonGroup *m_categoryGroup = nullptr;
    QAbstractButton *m_plainTextSwitch = nullptr;
    QLabel *m_plainTextHintLabel = nullptr;
    QFrame *m_pauseBanner = nullptr;
    QLabel *m_pauseIconLabel = nullptr;
    QLabel *m_pauseLabel = nullptr;
    PreviewSampleList *m_sampleList = nullptr;
    QLabel *m_statusLabel = nullptr;

    bool m_paused = false;
    bool m_plainText = false;
    bool m_applyingTheme = false;
};
