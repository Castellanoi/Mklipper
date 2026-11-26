/*
    SPDX-FileCopyrightText: 2026 Mklipper contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QDialog>

class QLabel;
class QTextBrowser;

class HelpDialog : public QDialog
{
    Q_OBJECT

public:
    explicit HelpDialog(QWidget *parent = nullptr);

    void refreshTheme();

private:
    static QString manualMarkdown();

    QLabel *m_logoLabel = nullptr;
    QLabel *m_subtitleLabel = nullptr;
    QTextBrowser *m_browser = nullptr;
};
