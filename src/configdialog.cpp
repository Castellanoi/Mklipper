/*
    SPDX-FileCopyrightText: 2026 Mklipper contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "configdialog.h"

#include "editactiondialog.h"
#include "settings.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>

ConfigDialog::ConfigDialog(URLGrabber *urlGrabber, QWidget *parent)
    : QDialog(parent)
    , m_urlGrabber(urlGrabber)
{
    setWindowTitle(tr("Configurar Mklipper"));
    setWindowIcon(QIcon(QStringLiteral(":/icons/mklipper.svg")));
    setMinimumSize(560, 460);

    // Clone existing actions for safe editing
    if (m_urlGrabber) {
        for (const ClipAction *act : m_urlGrabber->actionList()) {
            auto *copy = new ClipAction(act->actionRegexPattern(), act->description(), act->automatic());
            for (const auto &cmd : act->commands()) {
                copy->addCommand(cmd);
            }
            m_editedActions.append(copy);
        }
    }

    auto *s = MklipperSettings::self();
    auto *mainLayout = new QVBoxLayout(this);
    auto *tabs = new QTabWidget(this);
    mainLayout->addWidget(tabs, 1);

    // ==================== Tab 1: General ====================
    auto *generalPage = new QWidget(this);
    auto *genLayout = new QVBoxLayout(generalPage);

    auto *histGroup = new QGroupBox(tr("Historial y comportamiento"), generalPage);
    auto *histForm = new QFormLayout(histGroup);

    m_keepContents = new QCheckBox(tr("Guardar historial entre sesiones"), histGroup);
    m_keepContents->setChecked(s->keepClipboardContents());

    m_preventEmpty = new QCheckBox(tr("Evitar que el portapapeles quede vacío al cerrar aplicaciones"), histGroup);
    m_preventEmpty->setChecked(s->preventEmptyClipboard());

    m_saveImages = new QCheckBox(tr("Guardar imágenes (capturas de pantalla) en el historial"), histGroup);
    m_saveImages->setChecked(s->saveImages());

    m_maxItems = new QSpinBox(histGroup);
    m_maxItems->setRange(1, 2048);
    m_maxItems->setValue(s->maxClipItems());
    m_maxItems->setSuffix(tr(" elementos"));

    m_confirmClear = new QCheckBox(tr("Pedir confirmación antes de vaciar el historial"), histGroup);
    m_confirmClear->setChecked(s->confirmClearHistory());

    m_autoPaste = new QCheckBox(tr("Pegar automáticamente al elegir un elemento (además de copiarlo)"), histGroup);
    m_autoPaste->setChecked(s->autoPaste());

    m_pastePlainText = new QCheckBox(tr("Pegar sin formato (solo texto plano, sin colores ni estilos HTML)"), histGroup);
    m_pastePlainText->setChecked(s->pastePlainText());

    m_keepPopupOpen = new QCheckBox(tr("Mantener la ventana flotante abierta al hacer clic en elementos"), histGroup);
    m_keepPopupOpen->setChecked(s->keepPopupOpen());

    auto *closeOutsideRow = new QWidget(histGroup);
    auto *closeOutsideLayout = new QHBoxLayout(closeOutsideRow);
    closeOutsideLayout->setContentsMargins(24, 0, 0, 0);
    m_closePopupOnFocusOut = new QCheckBox(tr("Cerrar la ventana al hacer clic fuera de ella"), closeOutsideRow);
    m_closePopupOnFocusOut->setChecked(s->closePopupOnFocusOut());
    m_closePopupOnFocusOut->setEnabled(m_keepPopupOpen->isChecked());
    connect(m_keepPopupOpen, &QCheckBox::toggled, m_closePopupOnFocusOut, &QCheckBox::setEnabled);
    closeOutsideLayout->addWidget(m_closePopupOnFocusOut);

    histForm->addRow(m_keepContents);
    histForm->addRow(m_preventEmpty);
    histForm->addRow(m_saveImages);
    histForm->addRow(m_autoPaste);
    histForm->addRow(m_pastePlainText);
    histForm->addRow(m_keepPopupOpen);
    histForm->addRow(closeOutsideRow);
    histForm->addRow(tr("Tamaño máximo del historial:"), m_maxItems);
    histForm->addRow(m_confirmClear);
    genLayout->addWidget(histGroup);

    auto *selGroup = new QGroupBox(tr("Selección de texto (X11)"), generalPage);
    auto *selLayout = new QVBoxLayout(selGroup);

    m_saveSelection = new QCheckBox(tr("Guardar texto seleccionado con el mouse en el historial"), selGroup);
    m_saveSelection->setChecked(s->saveSelection());

    m_syncClipboards = new QCheckBox(tr("Sincronizar selección del mouse y portapapeles (Ctrl+C)"), selGroup);
    m_syncClipboards->setChecked(s->syncClipboards());

    selLayout->addWidget(m_saveSelection);
    selLayout->addWidget(m_syncClipboards);
    genLayout->addWidget(selGroup);

    auto *sysGroup = new QGroupBox(tr("Sistema y actualizaciones"), generalPage);
    auto *sysLayout = new QVBoxLayout(sysGroup);
    m_autostart = new QCheckBox(tr("Iniciar Mklipper automáticamente al iniciar sesión"), sysGroup);
    m_autostart->setChecked(MklipperSettings::autostartEnabled());
    sysLayout->addWidget(m_autostart);

    m_autoCheckUpdates = new QCheckBox(tr("Buscar actualizaciones automáticamente al iniciar"), sysGroup);
    m_autoCheckUpdates->setChecked(s->autoCheckUpdates());
    sysLayout->addWidget(m_autoCheckUpdates);

    auto *verRow = new QHBoxLayout;
    auto *verLabel = new QLabel(tr("Versión instalada: <b>%1</b>").arg(QStringLiteral(MKLIPPER_VERSION)), sysGroup);
    auto *checkUpdatesBtn = new QPushButton(QIcon::fromTheme(QStringLiteral("system-software-update")),
                                            tr("Buscar actualizaciones ahora…"),
                                            sysGroup);
    connect(checkUpdatesBtn, &QPushButton::clicked, this, &ConfigDialog::checkForUpdatesRequested);
    verRow->addWidget(verLabel);
    verRow->addStretch();
    verRow->addWidget(checkUpdatesBtn);
    sysLayout->addLayout(verRow);

    genLayout->addWidget(sysGroup);
    genLayout->addStretch();

    tabs->addTab(generalPage, tr("General"));

    // ==================== Tab 2: Actions ====================
    auto *actionsPage = new QWidget(this);
    auto *actLayout = new QVBoxLayout(actionsPage);

    auto *actOptGroup = new QGroupBox(tr("Opciones de acciones"), actionsPage);
    auto *actOptForm = new QFormLayout(actOptGroup);

    m_urlGrabberEnabled = new QCheckBox(tr("Mostrar menú de acciones automáticamente al copiar"), actOptGroup);
    m_urlGrabberEnabled->setChecked(s->urlGrabberEnabled());

    m_actionOnSelection = new QCheckBox(tr("Mostrar menú de acciones al seleccionar texto"), actOptGroup);
    m_actionOnSelection->setChecked(s->actionOnSelection());

    m_replayInHistory = new QCheckBox(tr("Ejecutar acciones al elegir un elemento del historial"), actOptGroup);
    m_replayInHistory->setChecked(s->replayActionInHistory());

    m_stripWhiteSpace = new QCheckBox(tr("Recortar espacios en blanco al evaluar acciones"), actOptGroup);
    m_stripWhiteSpace->setChecked(s->stripWhiteSpace());

    m_magicMime = new QCheckBox(tr("Ofrecer abrir enlaces (http/https) y archivos existentes"), actOptGroup);
    m_magicMime->setChecked(s->enableMagicMimeActions());

    m_actionTimeout = new QSpinBox(actOptGroup);
    m_actionTimeout->setRange(0, 200);
    m_actionTimeout->setValue(s->timeoutForActionPopups());
    m_actionTimeout->setSuffix(tr(" s (0 = sin límite)"));

    actOptForm->addRow(m_urlGrabberEnabled);
    actOptForm->addRow(m_actionOnSelection);
    actOptForm->addRow(m_replayInHistory);
    actOptForm->addRow(m_stripWhiteSpace);
    actOptForm->addRow(m_magicMime);
    actOptForm->addRow(tr("Duración del menú automático:"), m_actionTimeout);
    actLayout->addWidget(actOptGroup);

    auto *customGroup = new QGroupBox(tr("Acciones personalizadas por expresión regular"), actionsPage);
    auto *customLayout = new QVBoxLayout(customGroup);
    m_actionsList = new QListWidget(customGroup);
    connect(m_actionsList, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *) { editAction(); });
    customLayout->addWidget(m_actionsList, 1);

    auto *actBtns = new QHBoxLayout;
    auto *addBtn = new QPushButton(tr("Agregar…"), customGroup);
    auto *editBtn = new QPushButton(tr("Editar…"), customGroup);
    auto *delBtn = new QPushButton(tr("Eliminar"), customGroup);
    connect(addBtn, &QPushButton::clicked, this, &ConfigDialog::addAction);
    connect(editBtn, &QPushButton::clicked, this, &ConfigDialog::editAction);
    connect(delBtn, &QPushButton::clicked, this, &ConfigDialog::deleteAction);
    actBtns->addWidget(addBtn);
    actBtns->addWidget(editBtn);
    actBtns->addWidget(delBtn);
    actBtns->addStretch();
    customLayout->addLayout(actBtns);
    actLayout->addWidget(customGroup, 1);

    tabs->addTab(actionsPage, tr("Acciones"));

    // ==================== Tab 3: Shortcuts ====================
    auto *keysPage = new QWidget(this);
    auto *keysLayout = new QVBoxLayout(keysPage);
    auto *keysForm = new QFormLayout;

    auto makeKeyRow = [keysPage](const QString &seq) {
        auto *w = new QWidget(keysPage);
        auto *hl = new QHBoxLayout(w);
        hl->setContentsMargins(0, 0, 0, 0);
        auto *edit = new QKeySequenceEdit(QKeySequence::fromString(seq), w);
        QObject::connect(edit, &QKeySequenceEdit::keySequenceChanged, edit, [edit](const QKeySequence &ks) {
            if (ks.count() > 1) {
                edit->setKeySequence(QKeySequence(ks[0]));
            }
        });
        auto *clearBtn = new QPushButton(QObject::tr("Limpiar"), w);
        clearBtn->setFixedWidth(72);
        QObject::connect(clearBtn, &QPushButton::clicked, edit, &QKeySequenceEdit::clear);
        hl->addWidget(edit, 1);
        hl->addWidget(clearBtn);
        return std::pair{w, edit};
    };

    auto [wPopup, ePopup] = makeKeyRow(s->shortcutPopup());
    auto [wNext, eNext] = makeKeyRow(s->shortcutCycleNext());
    auto [wPrev, ePrev] = makeKeyRow(s->shortcutCyclePrev());
    auto [wRepeat, eRepeat] = makeKeyRow(s->shortcutRepeatAction());
    auto [wClear, eClear] = makeKeyRow(s->shortcutClearHistory());

    m_keyPopup = ePopup;
    m_keyCycleNext = eNext;
    m_keyCyclePrev = ePrev;
    m_keyRepeatAction = eRepeat;
    m_keyClearHistory = eClear;

    keysForm->addRow(tr("Mostrar historial bajo el cursor:"), wPopup);
    keysForm->addRow(tr("Rotar historial (siguiente):"), wNext);
    keysForm->addRow(tr("Rotar historial (anterior):"), wPrev);
    keysForm->addRow(tr("Invocar acciones del elemento actual:"), wRepeat);
    keysForm->addRow(tr("Vaciar historial:"), wClear);

    keysLayout->addLayout(keysForm);

    auto *tip = new QLabel(
        tr("Consejo: También podés asignar un atajo nativo en Cinnamon (Configuración del sistema → Teclado → Atajos personalizados) "
           "apuntando al comando <b>mklipper --toggle</b>."),
        keysPage);
    tip->setWordWrap(true);
    keysLayout->addSpacing(12);
    keysLayout->addWidget(tip);
    keysLayout->addStretch();

    tabs->addTab(keysPage, tr("Atajos"));

    // Dialog buttons
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("Guardar"));
    buttons->button(QDialogButtonBox::Cancel)->setText(tr("Cancelar"));
    connect(buttons, &QDialogButtonBox::accepted, this, &ConfigDialog::saveAndAccept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttons);

    refreshActionList();
}

ConfigDialog::~ConfigDialog()
{
    qDeleteAll(m_editedActions);
    m_editedActions.clear();
}

void ConfigDialog::refreshActionList()
{
    m_actionsList->clear();
    for (const ClipAction *act : std::as_const(m_editedActions)) {
        const QString label = act->description().isEmpty()
            ? act->actionRegexPattern()
            : QStringLiteral("%1  [%2]").arg(act->description(), act->actionRegexPattern());
        m_actionsList->addItem(label);
    }
}

void ConfigDialog::addAction()
{
    EditActionDialog dlg(nullptr, this);
    if (dlg.exec() == QDialog::Accepted) {
        m_editedActions.append(dlg.takeAction());
        refreshActionList();
    }
}

void ConfigDialog::editAction()
{
    const int row = m_actionsList->currentRow();
    if (row < 0 || row >= m_editedActions.size()) {
        return;
    }
    EditActionDialog dlg(m_editedActions.at(row), this);
    if (dlg.exec() == QDialog::Accepted) {
        delete m_editedActions[row];
        m_editedActions[row] = dlg.takeAction();
        refreshActionList();
    }
}

void ConfigDialog::deleteAction()
{
    const int row = m_actionsList->currentRow();
    if (row < 0 || row >= m_editedActions.size()) {
        return;
    }
    delete m_editedActions.takeAt(row);
    refreshActionList();
}

void ConfigDialog::saveAndAccept()
{
    auto *s = MklipperSettings::self();
    s->setKeepClipboardContents(m_keepContents->isChecked());
    s->setPreventEmptyClipboard(m_preventEmpty->isChecked());
    s->setSaveImages(m_saveImages->isChecked());
    s->setSaveSelection(m_saveSelection->isChecked());
    s->setSyncClipboards(m_syncClipboards->isChecked());
    s->setMaxClipItems(m_maxItems->value());
    s->setConfirmClearHistory(m_confirmClear->isChecked());
    s->setAutoPaste(m_autoPaste->isChecked());
    s->setPastePlainText(m_pastePlainText->isChecked());
    s->setKeepPopupOpen(m_keepPopupOpen->isChecked());
    s->setClosePopupOnFocusOut(m_closePopupOnFocusOut->isChecked());

    s->setUrlGrabberEnabled(m_urlGrabberEnabled->isChecked());
    s->setActionOnSelection(m_actionOnSelection->isChecked());
    s->setReplayActionInHistory(m_replayInHistory->isChecked());
    s->setStripWhiteSpace(m_stripWhiteSpace->isChecked());
    s->setEnableMagicMimeActions(m_magicMime->isChecked());
    s->setTimeoutForActionPopups(m_actionTimeout->value());

    s->setShortcutPopup(m_keyPopup->keySequence().toString());
    s->setShortcutCycleNext(m_keyCycleNext->keySequence().toString());
    s->setShortcutCyclePrev(m_keyCyclePrev->keySequence().toString());
    s->setShortcutRepeatAction(m_keyRepeatAction->keySequence().toString());
    s->setShortcutClearHistory(m_keyClearHistory->keySequence().toString());

    s->setAutoCheckUpdates(m_autoCheckUpdates->isChecked());
    MklipperSettings::setAutostartEnabled(m_autostart->isChecked());

    if (m_urlGrabber) {
        ActionList toTransfer = m_editedActions;
        m_editedActions.clear();
        m_urlGrabber->setActionList(toTransfer);
        m_urlGrabber->saveSettings();
    }

    s->save();
    accept();
}
