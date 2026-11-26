/*
    SPDX-FileCopyrightText: 2026 Mklipper contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QDialog>
#include <QList>

#include "flaticons.h"

class QFrame;
class QLabel;
class QTabWidget;

class HelpDialog : public QDialog
{
    Q_OBJECT

public:
    explicit HelpDialog(QWidget *parent = nullptr);

    void refreshTheme();

private:
    struct CardIconBinding {
        QFrame *badge = nullptr;
        QLabel *iconLabel = nullptr;
        FlatIcon icon = FlatIcon::Info;
    };

    struct TabIconBinding {
        int index = 0;
        FlatIcon icon = FlatIcon::Info;
    };

    QLabel *m_logoLabel = nullptr;
    QLabel *m_subtitleLabel = nullptr;
    QTabWidget *m_tabs = nullptr;
    QList<QLabel *> m_mutedLabels;
    QList<CardIconBinding> m_cardIcons;
    QList<TabIconBinding> m_tabIcons;
};
