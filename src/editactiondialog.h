/*
    SPDX-FileCopyrightText: 2026 Mklipper contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QDialog>
#include <QList>

#include "urlgrabber.h"

class QCheckBox;
class QLineEdit;
class QTableWidget;

class EditActionDialog : public QDialog
{
    Q_OBJECT

public:
    explicit EditActionDialog(const ClipAction *action = nullptr, QWidget *parent = nullptr);

    ClipAction *takeAction() const;

private Q_SLOTS:
    void addCommand();
    void editSelectedCommand();
    void removeSelectedCommand();

private:
    void refreshTable();

    QLineEdit *m_regexEdit = nullptr;
    QLineEdit *m_descEdit = nullptr;
    QCheckBox *m_autoCheck = nullptr;
    QTableWidget *m_cmdTable = nullptr;
    QList<ClipCommand> m_commands;
};
