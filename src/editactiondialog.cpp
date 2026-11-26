/*
    SPDX-FileCopyrightText: 2026 Mklipper contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "editactiondialog.h"

#include "flaticons.h"
#include "themestyle.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QTableWidget>
#include <QVBoxLayout>

namespace
{
class EditCommandDialog : public QDialog
{
public:
    explicit EditCommandDialog(const ClipCommand &cmd, QWidget *parent = nullptr)
        : QDialog(parent)
    {
        setWindowTitle(QObject::tr("Editar comando"));
        setMinimumWidth(460);

        const ThemeTokens t = ThemeTokens::fromPalette(palette());
        setStyleSheet(ThemeStyle::dialogStyleSheet(t));

        auto *layout = new QVBoxLayout(this);
        layout->setContentsMargins(16, 16, 16, 14);
        layout->setSpacing(12);

        auto *card = new QFrame(this);
        card->setObjectName(QStringLiteral("mklipperCard"));
        auto *form = new QFormLayout(card);
        form->setContentsMargins(14, 12, 14, 12);
        form->setSpacing(10);

        m_cmdEdit = new QLineEdit(cmd.command, card);
        m_cmdEdit->setPlaceholderText(QObject::tr("Ej: xdg-open %s"));
        m_descEdit = new QLineEdit(cmd.description, card);
        m_descEdit->setPlaceholderText(QObject::tr("Descripción del comando"));
        m_outputCombo = new QComboBox(card);
        m_outputCombo->addItem(QObject::tr("Ignorar salida"), ClipCommand::IGNORE);
        m_outputCombo->addItem(QObject::tr("Reemplazar elemento en el portapapeles"), ClipCommand::REPLACE);
        m_outputCombo->addItem(QObject::tr("Agregar salida como nuevo elemento"), ClipCommand::ADD);
        m_outputCombo->setCurrentIndex(static_cast<int>(cmd.output));
        m_enabledCheck = new QCheckBox(QObject::tr("Habilitado"), card);
        m_enabledCheck->setChecked(cmd.isEnabled);

        form->addRow(QObject::tr("Comando:"), m_cmdEdit);
        form->addRow(QObject::tr("Descripción:"), m_descEdit);
        form->addRow(QObject::tr("Salida:"), m_outputCombo);
        form->addRow(QString(), m_enabledCheck);
        layout->addWidget(card);

        auto *hint = new QLabel(QObject::tr("Variables disponibles: %s (texto), %0..%9 (grupos capturados por la regex)."), this);
        hint->setWordWrap(true);
        QPalette hp = hint->palette();
        hp.setColor(QPalette::WindowText, t.textMuted);
        hint->setPalette(hp);
        layout->addWidget(hint);

        auto *btnRow = new QHBoxLayout;
        btnRow->setSpacing(8);
        btnRow->addStretch();
        auto *cancelBtn = new QPushButton(QObject::tr("Cancelar"), this);
        cancelBtn->setCursor(Qt::PointingHandCursor);
        auto *okBtn = new QPushButton(
            FlatIcons::icon(FlatIcon::Check, t.accentFg, t.accentFg, t.textMuted, 15),
            QObject::tr("Aceptar"),
            this);
        okBtn->setProperty("primary", true);
        okBtn->setIconSize(QSize(15, 15));
        okBtn->setCursor(Qt::PointingHandCursor);
        okBtn->setDefault(true);
        connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
        connect(okBtn, &QPushButton::clicked, this, &QDialog::accept);
        btnRow->addWidget(cancelBtn);
        btnRow->addWidget(okBtn);
        layout->addLayout(btnRow);
    }

    ClipCommand command() const
    {
        return ClipCommand(m_cmdEdit->text().trimmed(),
                           m_descEdit->text().trimmed(),
                           m_enabledCheck->isChecked(),
                           QStringLiteral("system-run"),
                           static_cast<ClipCommand::Output>(m_outputCombo->currentData().toInt()));
    }

private:
    QLineEdit *m_cmdEdit = nullptr;
    QLineEdit *m_descEdit = nullptr;
    QComboBox *m_outputCombo = nullptr;
    QCheckBox *m_enabledCheck = nullptr;
};
} // namespace

EditActionDialog::EditActionDialog(const ClipAction *action, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(action ? tr("Editar acción") : tr("Nueva acción"));
    setMinimumSize(560, 420);

    const ThemeTokens t = ThemeTokens::fromPalette(palette());
    setStyleSheet(ThemeStyle::dialogStyleSheet(t));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 14);
    layout->setSpacing(12);

    auto *topCard = new QFrame(this);
    topCard->setObjectName(QStringLiteral("mklipperCard"));
    auto *form = new QFormLayout(topCard);
    form->setContentsMargins(14, 12, 14, 12);
    form->setSpacing(10);

    m_regexEdit = new QLineEdit(topCard);
    m_regexEdit->setPlaceholderText(tr("Ej: ^https?://.*"));
    m_descEdit = new QLineEdit(topCard);
    m_descEdit->setPlaceholderText(tr("Descripción de la acción"));
    m_autoCheck = new QCheckBox(tr("Incluir en el menú automático de acciones"), topCard);
    m_autoCheck->setChecked(true);

    if (action) {
        m_regexEdit->setText(action->actionRegexPattern());
        m_descEdit->setText(action->description());
        m_autoCheck->setChecked(action->automatic());
        m_commands = action->commands();
    }

    form->addRow(tr("Expresión regular:"), m_regexEdit);
    form->addRow(tr("Descripción:"), m_descEdit);
    form->addRow(QString(), m_autoCheck);
    layout->addWidget(topCard);

    m_cmdTable = new QTableWidget(0, 3, this);
    m_cmdTable->setHorizontalHeaderLabels({tr("Comando"), tr("Descripción"), tr("Salida")});
    m_cmdTable->horizontalHeader()->setStretchLastSection(true);
    m_cmdTable->verticalHeader()->setVisible(false);
    m_cmdTable->setShowGrid(false);
    m_cmdTable->setAlternatingRowColors(true);
    m_cmdTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_cmdTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_cmdTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    connect(m_cmdTable, &QTableWidget::cellDoubleClicked, this, [this](int, int) { editSelectedCommand(); });
    layout->addWidget(m_cmdTable, 1);

    auto *cmdBtns = new QHBoxLayout;
    cmdBtns->setSpacing(8);
    auto *addBtn = new QPushButton(
        FlatIcons::icon(FlatIcon::Plus, t.textPrimary, t.textPrimary, t.textMuted, 15),
        tr("Agregar comando"),
        this);
    addBtn->setIconSize(QSize(15, 15));
    addBtn->setCursor(Qt::PointingHandCursor);

    auto *editBtn = new QPushButton(
        FlatIcons::icon(FlatIcon::Edit, t.textPrimary, t.textPrimary, t.textMuted, 15),
        tr("Editar"),
        this);
    editBtn->setIconSize(QSize(15, 15));
    editBtn->setCursor(Qt::PointingHandCursor);

    auto *delBtn = new QPushButton(
        FlatIcons::icon(FlatIcon::Trash, t.textSecondary, t.danger, t.textMuted, 15),
        tr("Eliminar"),
        this);
    delBtn->setProperty("destructive", true);
    delBtn->setIconSize(QSize(15, 15));
    delBtn->setCursor(Qt::PointingHandCursor);

    connect(addBtn, &QPushButton::clicked, this, &EditActionDialog::addCommand);
    connect(editBtn, &QPushButton::clicked, this, &EditActionDialog::editSelectedCommand);
    connect(delBtn, &QPushButton::clicked, this, &EditActionDialog::removeSelectedCommand);
    cmdBtns->addWidget(addBtn);
    cmdBtns->addWidget(editBtn);
    cmdBtns->addWidget(delBtn);
    cmdBtns->addStretch();
    layout->addLayout(cmdBtns);

    auto *divider = new QFrame(this);
    divider->setObjectName(QStringLiteral("mklipperDivider"));
    layout->addWidget(divider);

    auto *btnRow = new QHBoxLayout;
    btnRow->setSpacing(8);
    btnRow->addStretch();
    auto *cancelBtn = new QPushButton(tr("Cancelar"), this);
    cancelBtn->setCursor(Qt::PointingHandCursor);
    auto *okBtn = new QPushButton(
        FlatIcons::icon(FlatIcon::Check, t.accentFg, t.accentFg, t.textMuted, 15),
        tr("Aceptar"),
        this);
    okBtn->setProperty("primary", true);
    okBtn->setIconSize(QSize(15, 15));
    okBtn->setCursor(Qt::PointingHandCursor);
    okBtn->setDefault(true);
    connect(okBtn, &QPushButton::clicked, this, [this]() {
        const QString pattern = m_regexEdit->text().trimmed();
        if (pattern.isEmpty() || !QRegularExpression(pattern).isValid()) {
            QMessageBox::warning(this, tr("Expresión regular inválida"), tr("Ingresa una expresión regular válida."));
            return;
        }
        accept();
    });
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    btnRow->addWidget(cancelBtn);
    btnRow->addWidget(okBtn);
    layout->addLayout(btnRow);

    refreshTable();
}

void EditActionDialog::refreshTable()
{
    m_cmdTable->setRowCount(m_commands.size());
    for (int i = 0; i < m_commands.size(); ++i) {
        const auto &c = m_commands.at(i);
        m_cmdTable->setItem(i, 0, new QTableWidgetItem(c.command));
        m_cmdTable->setItem(i, 1, new QTableWidgetItem(c.description));
        QString outText = tr("Ignorar");
        if (c.output == ClipCommand::REPLACE) {
            outText = tr("Reemplazar");
        } else if (c.output == ClipCommand::ADD) {
            outText = tr("Agregar");
        }
        m_cmdTable->setItem(i, 2, new QTableWidgetItem(outText));
    }
}

void EditActionDialog::addCommand()
{
    EditCommandDialog dlg(ClipCommand(), this);
    if (dlg.exec() == QDialog::Accepted) {
        ClipCommand cmd = dlg.command();
        if (!cmd.command.isEmpty()) {
            m_commands.append(cmd);
            refreshTable();
        }
    }
}

void EditActionDialog::editSelectedCommand()
{
    const int row = m_cmdTable->currentRow();
    if (row < 0 || row >= m_commands.size()) {
        return;
    }
    EditCommandDialog dlg(m_commands.at(row), this);
    if (dlg.exec() == QDialog::Accepted) {
        m_commands[row] = dlg.command();
        refreshTable();
    }
}

void EditActionDialog::removeSelectedCommand()
{
    const int row = m_cmdTable->currentRow();
    if (row < 0 || row >= m_commands.size()) {
        return;
    }
    m_commands.removeAt(row);
    refreshTable();
}

ClipAction *EditActionDialog::takeAction() const
{
    auto *act = new ClipAction(m_regexEdit->text().trimmed(), m_descEdit->text().trimmed(), m_autoCheck->isChecked());
    for (const auto &c : m_commands) {
        act->addCommand(c);
    }
    return act;
}
