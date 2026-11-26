/*
    SPDX-FileCopyrightText: 2026 Mklipper contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "configdialog.h"

#include "editactiondialog.h"
#include "flaticons.h"
#include "settings.h"
#include "themestyle.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFontMetrics>
#include <QFormLayout>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>

namespace
{
QWidget *wrapSpinBoxRow(QSpinBox *spin, const QString &maxSampleNumber, QWidget *parent)
{
    const QFontMetrics fm(spin->font());
    const int textW = fm.horizontalAdvance(maxSampleNumber + spin->suffix());
    const int minW = textW + qMax(96, fm.height() * 5);
    const int minH = qMax(spin->sizeHint().height(), fm.height() + 14);
    spin->setMinimumSize(minW, minH);
    spin->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);

    auto *container = new QWidget(parent);
    auto *hl = new QHBoxLayout(container);
    hl->setContentsMargins(0, 0, 0, 0);
    hl->addWidget(spin);
    hl->addStretch();
    return container;
}

QScrollArea *makeScrollTab(QWidget *content, QWidget *parent)
{
    auto *scroll = new QScrollArea(parent);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidget(content);
    return scroll;
}

// Modern Cinnamon 6.6 / GTK4-style grouped preference section (section label + flat rounded card)
QFrame *addSectionCard(QVBoxLayout *parentLayout, const QString &title, const ThemeTokens &t, QWidget *parent)
{
    auto *titleLbl = new QLabel(title, parent);
    QFont tf = titleLbl->font();
    tf.setBold(true);
    tf.setPointSizeF(qMax(9.0, tf.pointSizeF() - 0.3));
    titleLbl->setFont(tf);
    QPalette tp = titleLbl->palette();
    tp.setColor(QPalette::WindowText, t.textSecondary);
    titleLbl->setPalette(tp);
    titleLbl->setContentsMargins(4, 4, 0, 0);
    parentLayout->addWidget(titleLbl);

    auto *card = new QFrame(parent);
    card->setObjectName(QStringLiteral("mklipperCard"));
    parentLayout->addWidget(card);
    return card;
}
} // namespace

ConfigDialog::ConfigDialog(URLGrabber *urlGrabber, QWidget *parent)
    : QDialog(parent)
    , m_urlGrabber(urlGrabber)
{
    setWindowTitle(tr("Configurar Mklipper"));
    setMinimumSize(520, 420);

    const ThemeTokens t = ThemeTokens::fromPalette(palette());
    setWindowIcon(FlatIcons::appLogoIcon(t.accent));
    setStyleSheet(ThemeStyle::dialogStyleSheet(t));

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
    mainLayout->setContentsMargins(16, 16, 16, 14);
    mainLayout->setSpacing(12);

    // Clean minimalist header banner
    auto *headerLayout = new QHBoxLayout;
    headerLayout->setSpacing(12);
    auto *logoLabel = new QLabel(this);
    logoLabel->setPixmap(FlatIcons::appLogoPixmap(t.accent, 38, devicePixelRatioF()));
    logoLabel->setFixedSize(38, 38);
    headerLayout->addWidget(logoLabel);

    auto *headerTextLayout = new QVBoxLayout;
    headerTextLayout->setSpacing(1);
    auto *titleRow = new QHBoxLayout;
    titleRow->setSpacing(8);
    auto *appTitle = new QLabel(QStringLiteral("Mklipper"), this);
    QFont atFont = appTitle->font();
    atFont.setBold(true);
    atFont.setPointSizeF(atFont.pointSizeF() + 2.5);
    appTitle->setFont(atFont);

    auto *verPill = new QLabel(QStringLiteral("v%1").arg(QStringLiteral(MKLIPPER_VERSION)), this);
    verPill->setTextFormat(Qt::PlainText);
    QFont vpFont = verPill->font();
    vpFont.setPointSizeF(qMax(8.0, vpFont.pointSizeF() - 1.2));
    vpFont.setBold(true);
    verPill->setFont(vpFont);
    verPill->setStyleSheet(
        QStringLiteral("QLabel { color: %1; background-color: %2; "
                       "border: none; border-radius: 9px; padding: 2px 9px; }")
            .arg(ThemeStyle::rgbaCss(t.accent), ThemeStyle::rgbaCss(t.accentSoft)));

    titleRow->addWidget(appTitle);
    titleRow->addWidget(verPill);
    titleRow->addStretch();

    auto *appSubtitle = new QLabel(tr("Gestor de portapapeles para Linux Mint"), this);
    QFont subFont = appSubtitle->font();
    subFont.setPointSizeF(qMax(8.5, subFont.pointSizeF() - 1.0));
    appSubtitle->setFont(subFont);
    QPalette subPal = appSubtitle->palette();
    subPal.setColor(QPalette::WindowText, t.textMuted);
    appSubtitle->setPalette(subPal);

    headerTextLayout->addLayout(titleRow);
    headerTextLayout->addWidget(appSubtitle);
    headerLayout->addLayout(headerTextLayout, 1);
    mainLayout->addLayout(headerLayout);

    auto *tabs = new QTabWidget(this);
    tabs->setIconSize(QSize(15, 15));
    mainLayout->addWidget(tabs, 1);

    // ==================== Tab 1: General ====================
    auto *generalPage = new QWidget(this);
    auto *genLayout = new QVBoxLayout(generalPage);
    genLayout->setContentsMargins(2, 6, 6, 8);
    genLayout->setSpacing(8);

    auto *histCard = addSectionCard(genLayout, tr("Historial y comportamiento"), t, generalPage);
    auto *histForm = new QFormLayout(histCard);
    histForm->setContentsMargins(14, 12, 14, 12);
    histForm->setSpacing(8);
    histForm->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    histForm->setRowWrapPolicy(QFormLayout::DontWrapRows);

    m_keepContents = new QCheckBox(tr("Guardar historial entre sesiones"), histCard);
    m_keepContents->setChecked(s->keepClipboardContents());

    m_preventEmpty = new QCheckBox(tr("Evitar que el portapapeles quede vacío al cerrar aplicaciones"), histCard);
    m_preventEmpty->setChecked(s->preventEmptyClipboard());

    m_saveImages = new QCheckBox(tr("Guardar imágenes (capturas de pantalla) en el historial"), histCard);
    m_saveImages->setChecked(s->saveImages());

    m_maxItems = new QSpinBox(histCard);
    m_maxItems->setRange(1, 2048);
    m_maxItems->setValue(s->maxClipItems());
    m_maxItems->setSuffix(tr(" elementos"));

    m_confirmClear = new QCheckBox(tr("Pedir confirmación antes de vaciar el historial"), histCard);
    m_confirmClear->setChecked(s->confirmClearHistory());

    m_autoPaste = new QCheckBox(tr("Pegar automáticamente al elegir un elemento (además de copiarlo)"), histCard);
    m_autoPaste->setChecked(s->autoPaste());

    m_pastePlainText = new QCheckBox(tr("Pegar sin formato (solo texto plano, sin colores ni estilos HTML)"), histCard);
    m_pastePlainText->setChecked(s->pastePlainText());

    m_keepPopupOpen = new QCheckBox(tr("Mantener la ventana flotante abierta al hacer clic en elementos"), histCard);
    m_keepPopupOpen->setChecked(s->keepPopupOpen());

    auto *closeOutsideRow = new QWidget(histCard);
    auto *closeOutsideLayout = new QHBoxLayout(closeOutsideRow);
    closeOutsideLayout->setContentsMargins(24, 0, 0, 0);
    m_closePopupOnFocusOut = new QCheckBox(tr("Cerrar la ventana al hacer clic fuera de ella"), closeOutsideRow);
    m_closePopupOnFocusOut->setChecked(s->closePopupOnFocusOut());
    m_closePopupOnFocusOut->setEnabled(m_keepPopupOpen->isChecked());
    connect(m_keepPopupOpen, &QCheckBox::toggled, m_closePopupOnFocusOut, &QCheckBox::setEnabled);
    closeOutsideLayout->addWidget(m_closePopupOnFocusOut);

    m_saveSourceInfo = new QCheckBox(tr("Guardar origen de los elementos copiados (aplicación y enlace/ventana)"), histCard);
    m_saveSourceInfo->setChecked(s->saveSourceInfo());

    auto *showSourceRow = new QWidget(histCard);
    auto *showSourceLayout = new QHBoxLayout(showSourceRow);
    showSourceLayout->setContentsMargins(24, 0, 0, 0);
    m_showSourceInfo = new QCheckBox(tr("Mostrar etiqueta de origen en la lista del portapapeles"), showSourceRow);
    m_showSourceInfo->setChecked(s->showSourceInfo());
    m_showSourceInfo->setEnabled(m_saveSourceInfo->isChecked());
    connect(m_saveSourceInfo, &QCheckBox::toggled, m_showSourceInfo, &QCheckBox::setEnabled);
    showSourceLayout->addWidget(m_showSourceInfo);

    histForm->addRow(m_keepContents);
    histForm->addRow(m_preventEmpty);
    histForm->addRow(m_saveImages);
    histForm->addRow(m_autoPaste);
    histForm->addRow(m_pastePlainText);
    histForm->addRow(m_keepPopupOpen);
    histForm->addRow(closeOutsideRow);
    histForm->addRow(m_saveSourceInfo);
    histForm->addRow(showSourceRow);
    histForm->addRow(tr("Tamaño máximo del historial:"), wrapSpinBoxRow(m_maxItems, QStringLiteral("2048"), histCard));
    histForm->addRow(m_confirmClear);

    auto *selCard = addSectionCard(genLayout, tr("Selección de texto (X11)"), t, generalPage);
    auto *selLayout = new QVBoxLayout(selCard);
    selLayout->setContentsMargins(14, 12, 14, 12);
    selLayout->setSpacing(8);

    m_saveSelection = new QCheckBox(tr("Guardar texto seleccionado con el mouse en el historial"), selCard);
    m_saveSelection->setChecked(s->saveSelection());

    m_syncClipboards = new QCheckBox(tr("Sincronizar selección del mouse y portapapeles (Ctrl+C)"), selCard);
    m_syncClipboards->setChecked(s->syncClipboards());

    selLayout->addWidget(m_saveSelection);
    selLayout->addWidget(m_syncClipboards);

    auto *sysCard = addSectionCard(genLayout, tr("Sistema y actualizaciones"), t, generalPage);
    auto *sysLayout = new QVBoxLayout(sysCard);
    sysLayout->setContentsMargins(14, 12, 14, 12);
    sysLayout->setSpacing(8);

    m_autostart = new QCheckBox(tr("Iniciar Mklipper automáticamente al iniciar sesión"), sysCard);
    m_autostart->setChecked(MklipperSettings::autostartEnabled());
    sysLayout->addWidget(m_autostart);

    m_autoCheckUpdates = new QCheckBox(tr("Buscar actualizaciones automáticamente al iniciar"), sysCard);
    m_autoCheckUpdates->setChecked(s->autoCheckUpdates());
    sysLayout->addWidget(m_autoCheckUpdates);

    auto *langRow = new QHBoxLayout;
    auto *langLabel = new QLabel(tr("Idioma:"), sysCard);
    m_languageCombo = new QComboBox(sysCard);
    m_languageCombo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    m_languageCombo->addItem(tr("Automático (idioma del sistema)"), QStringLiteral("system"));
    m_languageCombo->addItem(QStringLiteral("Español"), QStringLiteral("es"));
    m_languageCombo->addItem(QStringLiteral("English"), QStringLiteral("en"));
    const int langIdx = m_languageCombo->findData(s->language().trimmed().toLower());
    m_languageCombo->setCurrentIndex(langIdx >= 0 ? langIdx : 0);
    const QFontMetrics langFm(m_languageCombo->font());
    m_languageCombo->setMinimumHeight(qMax(m_languageCombo->sizeHint().height(), langFm.height() + 12));
    langRow->addWidget(langLabel);
    langRow->addWidget(m_languageCombo);
    langRow->addStretch();
    sysLayout->addLayout(langRow);

    auto *verRow = new QHBoxLayout;
    auto *verLabel = new QLabel(tr("Versión instalada: <b>%1</b>").arg(QStringLiteral(MKLIPPER_VERSION)), sysCard);
    auto *checkUpdatesBtn = new QPushButton(
        FlatIcons::icon(FlatIcon::Update, t.textPrimary, t.textPrimary, t.textMuted, 15),
        tr("Buscar actualizaciones ahora…"),
        sysCard);
    checkUpdatesBtn->setIconSize(QSize(15, 15));
    checkUpdatesBtn->setCursor(Qt::PointingHandCursor);
    connect(checkUpdatesBtn, &QPushButton::clicked, this, &ConfigDialog::checkForUpdatesRequested);
    verRow->addWidget(verLabel);
    verRow->addStretch();
    verRow->addWidget(checkUpdatesBtn);
    sysLayout->addLayout(verRow);

    genLayout->addStretch();

    tabs->addTab(makeScrollTab(generalPage, this),
                 FlatIcons::icon(FlatIcon::Sliders, t.textSecondary, t.textPrimary, t.textMuted, 15),
                 tr("General"));

    // ==================== Tab 2: Actions ====================
    auto *actionsPage = new QWidget(this);
    auto *actLayout = new QVBoxLayout(actionsPage);
    actLayout->setContentsMargins(2, 6, 6, 8);
    actLayout->setSpacing(8);

    auto *actOptCard = addSectionCard(actLayout, tr("Opciones de acciones"), t, actionsPage);
    auto *actOptForm = new QFormLayout(actOptCard);
    actOptForm->setContentsMargins(14, 12, 14, 12);
    actOptForm->setSpacing(8);
    actOptForm->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

    m_urlGrabberEnabled = new QCheckBox(tr("Mostrar menú de acciones automáticamente al copiar"), actOptCard);
    m_urlGrabberEnabled->setChecked(s->urlGrabberEnabled());

    m_actionOnSelection = new QCheckBox(tr("Mostrar menú de acciones al seleccionar texto"), actOptCard);
    m_actionOnSelection->setChecked(s->actionOnSelection());

    m_replayInHistory = new QCheckBox(tr("Ejecutar acciones al elegir un elemento del historial"), actOptCard);
    m_replayInHistory->setChecked(s->replayActionInHistory());

    m_stripWhiteSpace = new QCheckBox(tr("Recortar espacios en blanco al evaluar acciones"), actOptCard);
    m_stripWhiteSpace->setChecked(s->stripWhiteSpace());

    m_magicMime = new QCheckBox(tr("Ofrecer abrir enlaces (http/https) y archivos existentes"), actOptCard);
    m_magicMime->setChecked(s->enableMagicMimeActions());

    m_actionTimeout = new QSpinBox(actOptCard);
    m_actionTimeout->setRange(0, 200);
    m_actionTimeout->setValue(s->timeoutForActionPopups());
    m_actionTimeout->setSuffix(tr(" s (0 = sin límite)"));

    actOptForm->addRow(m_urlGrabberEnabled);
    actOptForm->addRow(m_actionOnSelection);
    actOptForm->addRow(m_replayInHistory);
    actOptForm->addRow(m_stripWhiteSpace);
    actOptForm->addRow(m_magicMime);
    actOptForm->addRow(tr("Duración del menú automático:"), wrapSpinBoxRow(m_actionTimeout, QStringLiteral("200"), actOptCard));

    auto *customCard = addSectionCard(actLayout, tr("Acciones personalizadas por expresión regular"), t, actionsPage);
    auto *customLayout = new QVBoxLayout(customCard);
    customLayout->setContentsMargins(14, 12, 14, 12);
    customLayout->setSpacing(10);

    m_actionsList = new QListWidget(customCard);
    m_actionsList->setMinimumHeight(110);
    connect(m_actionsList, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *) { editAction(); });
    customLayout->addWidget(m_actionsList, 1);

    auto *actBtns = new QHBoxLayout;
    actBtns->setSpacing(8);
    auto *addBtn = new QPushButton(
        FlatIcons::icon(FlatIcon::Plus, t.textPrimary, t.textPrimary, t.textMuted, 15),
        tr("Agregar…"),
        customCard);
    addBtn->setIconSize(QSize(15, 15));
    addBtn->setCursor(Qt::PointingHandCursor);

    auto *editBtn = new QPushButton(
        FlatIcons::icon(FlatIcon::Edit, t.textPrimary, t.textPrimary, t.textMuted, 15),
        tr("Editar…"),
        customCard);
    editBtn->setIconSize(QSize(15, 15));
    editBtn->setCursor(Qt::PointingHandCursor);

    auto *delBtn = new QPushButton(
        FlatIcons::icon(FlatIcon::Trash, t.textSecondary, t.danger, t.textMuted, 15),
        tr("Eliminar"),
        customCard);
    delBtn->setProperty("destructive", true);
    delBtn->setIconSize(QSize(15, 15));
    delBtn->setCursor(Qt::PointingHandCursor);

    connect(addBtn, &QPushButton::clicked, this, &ConfigDialog::addAction);
    connect(editBtn, &QPushButton::clicked, this, &ConfigDialog::editAction);
    connect(delBtn, &QPushButton::clicked, this, &ConfigDialog::deleteAction);
    actBtns->addWidget(addBtn);
    actBtns->addWidget(editBtn);
    actBtns->addWidget(delBtn);
    actBtns->addStretch();
    customLayout->addLayout(actBtns);

    tabs->addTab(makeScrollTab(actionsPage, this),
                 FlatIcons::icon(FlatIcon::Bolt, t.textSecondary, t.textPrimary, t.textMuted, 15),
                 tr("Acciones"));

    // ==================== Tab 3: Shortcuts ====================
    auto *keysPage = new QWidget(this);
    auto *keysLayout = new QVBoxLayout(keysPage);
    keysLayout->setContentsMargins(2, 6, 6, 8);
    keysLayout->setSpacing(10);

    auto *keysCard = addSectionCard(keysLayout, tr("Atajos"), t, keysPage);
    auto *keysForm = new QFormLayout(keysCard);
    keysForm->setContentsMargins(14, 12, 14, 12);
    keysForm->setSpacing(10);
    keysForm->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

    auto makeKeyRow = [keysCard, &t](const QString &seq) {
        auto *w = new QWidget(keysCard);
        auto *hl = new QHBoxLayout(w);
        hl->setContentsMargins(0, 0, 0, 0);
        hl->setSpacing(6);
        auto *edit = new QKeySequenceEdit(QKeySequence::fromString(seq), w);
        QObject::connect(edit, &QKeySequenceEdit::keySequenceChanged, edit, [edit](const QKeySequence &ks) {
            if (ks.count() > 1) {
                edit->setKeySequence(QKeySequence(ks[0]));
            }
        });
        auto *clearBtn = new QPushButton(
            FlatIcons::icon(FlatIcon::ClearKey, t.textSecondary, t.textPrimary, t.textMuted, 14),
            QObject::tr("Limpiar"),
            w);
        clearBtn->setIconSize(QSize(14, 14));
        clearBtn->setCursor(Qt::PointingHandCursor);
        clearBtn->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
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

    auto *tipCard = new QFrame(keysPage);
    tipCard->setObjectName(QStringLiteral("mklipperTipCard"));
    auto *tipLayout = new QHBoxLayout(tipCard);
    tipLayout->setContentsMargins(14, 12, 14, 12);
    tipLayout->setSpacing(10);

    auto *tipIcon = new QLabel(tipCard);
    tipIcon->setPixmap(FlatIcons::pixmap(FlatIcon::Info, t.accent, 18, devicePixelRatioF()));
    tipIcon->setFixedSize(18, 18);
    tipLayout->addWidget(tipIcon, 0, Qt::AlignTop);

    auto *tip = new QLabel(
        tr("Consejo: También podés asignar un atajo nativo en Cinnamon (Configuración del sistema → Teclado → Atajos personalizados) "
           "apuntando al comando <b>mklipper --toggle</b>."),
        tipCard);
    tip->setWordWrap(true);
    tipLayout->addWidget(tip, 1);
    keysLayout->addWidget(tipCard);
    keysLayout->addStretch();

    tabs->addTab(makeScrollTab(keysPage, this),
                 FlatIcons::icon(FlatIcon::Keyboard, t.textSecondary, t.textPrimary, t.textMuted, 15),
                 tr("Atajos"));

    // Dialog action footer
    auto *footerDivider = new QFrame(this);
    footerDivider->setObjectName(QStringLiteral("mklipperDivider"));
    mainLayout->addWidget(footerDivider);

    auto *btnRow = new QHBoxLayout;
    btnRow->setSpacing(8);
    btnRow->addStretch();

    auto *cancelBtn = new QPushButton(tr("Cancelar"), this);
    cancelBtn->setCursor(Qt::PointingHandCursor);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);

    auto *saveBtn = new QPushButton(
        FlatIcons::icon(FlatIcon::Check, t.accentFg, t.accentFg, t.textMuted, 15),
        tr("Guardar"),
        this);
    saveBtn->setProperty("primary", true);
    saveBtn->setIconSize(QSize(15, 15));
    saveBtn->setCursor(Qt::PointingHandCursor);
    saveBtn->setDefault(true);
    connect(saveBtn, &QPushButton::clicked, this, &ConfigDialog::saveAndAccept);

    btnRow->addWidget(cancelBtn);
    btnRow->addWidget(saveBtn);
    mainLayout->addLayout(btnRow);

    refreshActionList();

    // Pick a comfortable initial size that fits scaled displays without squishing widgets
    const QSize preferred = generalPage->sizeHint() + QSize(68, 130);
    if (QScreen *scr = QGuiApplication::primaryScreen()) {
        const QRect avail = scr->availableGeometry();
        const int targetW = qBound(520, preferred.width(), static_cast<int>(avail.width() * 0.85));
        const int targetH = qBound(440, preferred.height(), static_cast<int>(avail.height() * 0.85));
        resize(targetW, targetH);
    } else {
        resize(qMax(620, preferred.width()), qMax(560, preferred.height()));
    }
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
    s->setSaveSourceInfo(m_saveSourceInfo->isChecked());
    s->setShowSourceInfo(m_showSourceInfo->isChecked());

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
    if (m_languageCombo) {
        s->setLanguage(m_languageCombo->currentData().toString());
    }
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
