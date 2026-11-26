/*
    SPDX-FileCopyrightText: 2026 Mklipper contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "configdialog.h"

#include "editactiondialog.h"
#include "flaticons.h"
#include "historymodel.h"
#include "settings.h"
#include "shortcutrecorder.h"
#include "themestyle.h"
#include "translations.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QFileDialog>
#include <QFontMetrics>
#include <QFormLayout>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QSpinBox>
#include <QStandardPaths>
#include <QTabWidget>
#include <QVBoxLayout>

namespace
{
QWidget *wrapSpinBoxRow(QSpinBox *spin, const QString &maxSampleNumber, const QString &tooltip, QWidget *parent)
{
    const QFontMetrics fm(spin->font());
    const int textW = fm.horizontalAdvance(maxSampleNumber + spin->suffix());
    const int minW = textW + qMax(96, fm.height() * 5);
    const int minH = qMax(spin->sizeHint().height(), fm.height() + 14);
    spin->setMinimumSize(minW, minH);
    spin->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    spin->setToolTip(tooltip);

    auto *container = new QWidget(parent);
    container->setToolTip(tooltip);
    auto *hl = new QHBoxLayout(container);
    hl->setContentsMargins(0, 0, 0, 0);
    hl->addWidget(spin);
    hl->addStretch();
    return container;
}

void addTooltipFormRow(QFormLayout *form, const QString &labelText, QWidget *field, const QString &tooltip, QWidget *parent)
{
    auto *lbl = new QLabel(labelText, parent);
    lbl->setToolTip(tooltip);
    field->setToolTip(tooltip);
    form->addRow(lbl, field);
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
    setMinimumSize(540, 440);

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
    m_keepContents->setToolTip(
        tr("Conserva el historial del portapapeles en disco para recuperarlo al reiniciar el equipo o cerrar sesión."));

    m_preventEmpty = new QCheckBox(tr("Evitar que el portapapeles quede vacío al cerrar aplicaciones"), histCard);
    m_preventEmpty->setChecked(s->preventEmptyClipboard());
    m_preventEmpty->setToolTip(
        tr("En X11, si cierras la aplicación desde donde copiaste algo, el portapapeles suele vaciarse. Esta opción conserva el contenido automáticamente."));

    m_saveImages = new QCheckBox(tr("Guardar imágenes (capturas de pantalla) en el historial"), histCard);
    m_saveImages->setChecked(s->saveImages());
    m_saveImages->setToolTip(
        tr("Almacena capturas de pantalla e imágenes copiadas en el historial con vista previa en miniatura."));

    m_maxItems = new QSpinBox(histCard);
    m_maxItems->setRange(1, 2048);
    m_maxItems->setValue(s->maxClipItems());
    m_maxItems->setSuffix(tr(" elementos"));

    m_confirmClear = new QCheckBox(tr("Pedir confirmación antes de vaciar el historial"), histCard);
    m_confirmClear->setChecked(s->confirmClearHistory());
    m_confirmClear->setToolTip(
        tr("Muestra un aviso antes de borrar todo el historial para evitar eliminaciones accidentales (los elementos fijados siempre se conservan)."));

    m_autoPaste = new QCheckBox(tr("Pegar automáticamente al elegir un elemento (además de copiarlo)"), histCard);
    m_autoPaste->setChecked(s->autoPaste());
    m_autoPaste->setToolTip(
        tr("Al seleccionar un elemento del historial, lo copia al portapapeles y lo pega automáticamente en la ventana activa."));

    m_pastePlainText = new QCheckBox(tr("Pegar sin formato (solo texto plano, sin colores ni estilos HTML)"), histCard);
    m_pastePlainText->setChecked(s->pastePlainText());
    m_pastePlainText->setToolTip(
        tr("Elimina fuentes, colores y formato HTML al pegar texto. No afecta imágenes ni archivos copiados."));

    m_keepPopupOpen = new QCheckBox(tr("Mantener la ventana flotante abierta al hacer clic en elementos"), histCard);
    m_keepPopupOpen->setChecked(s->keepPopupOpen());
    m_keepPopupOpen->setToolTip(
        tr("Mantiene visible la ventana del historial después de pegar un elemento, útil para pegar varios seguidos."));

    auto *closeOutsideRow = new QWidget(histCard);
    auto *closeOutsideLayout = new QHBoxLayout(closeOutsideRow);
    closeOutsideLayout->setContentsMargins(24, 0, 0, 0);
    m_closePopupOnFocusOut = new QCheckBox(tr("Cerrar la ventana al hacer clic fuera de ella"), closeOutsideRow);
    m_closePopupOnFocusOut->setChecked(s->closePopupOnFocusOut());
    m_closePopupOnFocusOut->setEnabled(m_keepPopupOpen->isChecked());
    m_closePopupOnFocusOut->setToolTip(
        tr("Cuando la ventana flotante permanece abierta al pegar, la oculta automáticamente si haces clic en otra aplicación."));
    connect(m_keepPopupOpen, &QCheckBox::toggled, m_closePopupOnFocusOut, &QCheckBox::setEnabled);
    closeOutsideLayout->addWidget(m_closePopupOnFocusOut);

    m_saveSourceInfo = new QCheckBox(tr("Guardar origen de los elementos copiados (aplicación y enlace/ventana)"), histCard);
    m_saveSourceInfo->setChecked(s->saveSourceInfo());
    m_saveSourceInfo->setToolTip(
        tr("Registra desde qué aplicación, ventana o sitio web se copió cada elemento para poder buscarlo o abrir el enlace original."));

    auto *showSourceRow = new QWidget(histCard);
    auto *showSourceLayout = new QHBoxLayout(showSourceRow);
    showSourceLayout->setContentsMargins(24, 0, 0, 0);
    m_showSourceInfo = new QCheckBox(tr("Mostrar etiqueta de origen en la lista del portapapeles"), showSourceRow);
    m_showSourceInfo->setChecked(s->showSourceInfo());
    m_showSourceInfo->setEnabled(m_saveSourceInfo->isChecked());
    m_showSourceInfo->setToolTip(
        tr("Muestra una línea discreta debajo de cada tarjeta indicando la aplicación o dominio de origen."));
    connect(m_saveSourceInfo, &QCheckBox::toggled, m_showSourceInfo, &QCheckBox::setEnabled);
    showSourceLayout->addWidget(m_showSourceInfo);

    m_showTrayBadge = new QCheckBox(tr("Mostrar contador de nuevos elementos en el icono de la bandeja"), histCard);
    m_showTrayBadge->setChecked(s->showTrayBadge());
    m_showTrayBadge->setToolTip(
        tr("Muestra un círculo de notificación en el icono de la bandeja con la cantidad de elementos nuevos agregados al historial. Se reinicia al abrir la ventana; desactívalo para mantener el icono clásico."));

    const QString maxItemsTip = tr("Cantidad máxima de elementos no fijados que se guardan en el historial. Al superar este límite se eliminan los más antiguos.");

    histForm->addRow(m_keepContents);
    histForm->addRow(m_preventEmpty);
    histForm->addRow(m_saveImages);
    histForm->addRow(m_autoPaste);
    histForm->addRow(m_pastePlainText);
    histForm->addRow(m_keepPopupOpen);
    histForm->addRow(closeOutsideRow);
    histForm->addRow(m_saveSourceInfo);
    histForm->addRow(showSourceRow);
    histForm->addRow(m_showTrayBadge);
    addTooltipFormRow(histForm,
                      tr("Tamaño máximo del historial:"),
                      wrapSpinBoxRow(m_maxItems, QStringLiteral("2048"), maxItemsTip, histCard),
                      maxItemsTip,
                      histCard);
    histForm->addRow(m_confirmClear);

    auto *selCard = addSectionCard(genLayout, tr("Selección de texto (X11)"), t, generalPage);
    auto *selLayout = new QVBoxLayout(selCard);
    selLayout->setContentsMargins(14, 12, 14, 12);
    selLayout->setSpacing(8);

    m_saveSelection = new QCheckBox(tr("Guardar texto seleccionado con el ratón en el historial"), selCard);
    m_saveSelection->setChecked(s->saveSelection());
    m_saveSelection->setToolTip(
        tr("Agrega automáticamente al historial cualquier texto que resaltes con el ratón (selección primaria de X11), sin necesidad de presionar Ctrl+C."));

    m_syncClipboards = new QCheckBox(tr("Sincronizar selección del ratón y portapapeles (Ctrl+C)"), selCard);
    m_syncClipboards->setChecked(s->syncClipboards());
    m_syncClipboards->setToolTip(
        tr("Mantiene sincronizado el texto seleccionado con el ratón (botón central) y el portapapeles estándar (Ctrl+C / Ctrl+V)."));

    selLayout->addWidget(m_saveSelection);
    selLayout->addWidget(m_syncClipboards);

    auto *privCard = addSectionCard(genLayout, tr("Privacidad y respaldo"), t, generalPage);
    auto *privLayout = new QVBoxLayout(privCard);
    privLayout->setContentsMargins(14, 12, 14, 12);
    privLayout->setSpacing(8);

    const QString ignoredAppsTip = tr("Mklipper no guardará contenido copiado desde las aplicaciones que aparezcan en esta lista (los bloqueos de pantalla y gestores de contraseñas compatibles ya se filtran automáticamente).");
    auto *ignoredLabel = new QLabel(tr("Aplicaciones ignoradas:"), privCard);
    ignoredLabel->setToolTip(ignoredAppsTip);
    privLayout->addWidget(ignoredLabel);

    auto *ignoredInputRow = new QHBoxLayout;
    ignoredInputRow->setSpacing(6);
    m_ignoredAppEdit = new QLineEdit(privCard);
    m_ignoredAppEdit->setPlaceholderText(tr("Ej: keepassxc, bitwarden, remmina…"));
    m_ignoredAppEdit->setToolTip(ignoredAppsTip);
    connect(m_ignoredAppEdit, &QLineEdit::returnPressed, this, &ConfigDialog::addIgnoredApp);

    auto *addIgnoredBtn = new QPushButton(
        FlatIcons::icon(FlatIcon::Plus, t.textPrimary, t.textPrimary, t.textMuted, 14),
        tr("Agregar"),
        privCard);
    addIgnoredBtn->setIconSize(QSize(14, 14));
    addIgnoredBtn->setCursor(Qt::PointingHandCursor);
    addIgnoredBtn->setToolTip(tr("Agrega el nombre de la aplicación a la lista de exclusión."));
    connect(addIgnoredBtn, &QPushButton::clicked, this, &ConfigDialog::addIgnoredApp);

    auto *removeIgnoredBtn = new QPushButton(
        FlatIcons::icon(FlatIcon::Trash, t.danger, t.danger, t.textMuted, 14),
        tr("Eliminar"),
        privCard);
    removeIgnoredBtn->setIconSize(QSize(14, 14));
    removeIgnoredBtn->setCursor(Qt::PointingHandCursor);
    removeIgnoredBtn->setToolTip(tr("Quita la aplicación seleccionada de la lista de exclusión."));
    connect(removeIgnoredBtn, &QPushButton::clicked, this, &ConfigDialog::removeIgnoredApp);

    ignoredInputRow->addWidget(m_ignoredAppEdit, 1);
    ignoredInputRow->addWidget(addIgnoredBtn);
    ignoredInputRow->addWidget(removeIgnoredBtn);
    privLayout->addLayout(ignoredInputRow);

    m_ignoredAppsList = new QListWidget(privCard);
    m_ignoredAppsList->setMaximumHeight(96);
    m_ignoredAppsList->setToolTip(ignoredAppsTip);
    m_ignoredAppsList->addItems(s->ignoredApps());
    privLayout->addWidget(m_ignoredAppsList);

    auto *backupRow = new QHBoxLayout;
    backupRow->setSpacing(8);
    auto *backupLabel = new QLabel(tr("Elementos fijados:"), privCard);
    auto *exportPinnedBtn = new QPushButton(
        FlatIcons::icon(FlatIcon::Save, t.textPrimary, t.textPrimary, t.textMuted, 14),
        tr("Exportar fijados…"),
        privCard);
    exportPinnedBtn->setIconSize(QSize(14, 14));
    exportPinnedBtn->setCursor(Qt::PointingHandCursor);
    exportPinnedBtn->setToolTip(tr("Guarda una copia de respaldo en formato JSON con todos tus elementos fijados."));
    connect(exportPinnedBtn, &QPushButton::clicked, this, &ConfigDialog::exportPinnedItems);

    auto *importPinnedBtn = new QPushButton(
        FlatIcons::icon(FlatIcon::FileText, t.textPrimary, t.textPrimary, t.textMuted, 14),
        tr("Importar fijados…"),
        privCard);
    importPinnedBtn->setIconSize(QSize(14, 14));
    importPinnedBtn->setCursor(Qt::PointingHandCursor);
    importPinnedBtn->setToolTip(tr("Restaura elementos fijados desde un archivo JSON exportado previamente."));
    connect(importPinnedBtn, &QPushButton::clicked, this, &ConfigDialog::importPinnedItems);

    backupRow->addWidget(backupLabel);
    backupRow->addStretch();
    backupRow->addWidget(exportPinnedBtn);
    backupRow->addWidget(importPinnedBtn);
    privLayout->addLayout(backupRow);

    auto *sysCard = addSectionCard(genLayout, tr("Sistema y actualizaciones"), t, generalPage);
    auto *sysLayout = new QVBoxLayout(sysCard);
    sysLayout->setContentsMargins(14, 12, 14, 12);
    sysLayout->setSpacing(8);

    m_autostart = new QCheckBox(tr("Iniciar Mklipper automáticamente al iniciar sesión"), sysCard);
    m_autostart->setChecked(MklipperSettings::autostartEnabled());
    m_autostart->setToolTip(
        tr("Lanza Mklipper en segundo plano en la bandeja del sistema cada vez que inicias sesión en Linux Mint."));
    sysLayout->addWidget(m_autostart);

    m_autoCheckUpdates = new QCheckBox(tr("Buscar actualizaciones automáticamente al iniciar"), sysCard);
    m_autoCheckUpdates->setChecked(s->autoCheckUpdates());
    m_autoCheckUpdates->setToolTip(
        tr("Comprueba si hay una versión nueva en GitHub al iniciar y te avisa con una notificación (también recibes actualizaciones desde el Gestor de actualizaciones de Mint)."));
    sysLayout->addWidget(m_autoCheckUpdates);

    const QString langTip = tr("Elige el idioma de la interfaz de Mklipper o sigue automáticamente el idioma configurado en tu sistema.");
    auto *langRow = new QHBoxLayout;
    auto *langLabel = new QLabel(tr("Idioma:"), sysCard);
    langLabel->setToolTip(langTip);
    m_languageCombo = new QComboBox(sysCard);
    m_languageCombo->setToolTip(langTip);
    m_languageCombo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    m_languageCombo->addItem(tr("Automático (idioma del sistema)"), QStringLiteral("system"));
    for (const LanguageInfo &lang : MklipperTranslations::availableLanguages()) {
        m_languageCombo->addItem(lang.nativeName, lang.code);
    }
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
    checkUpdatesBtn->setToolTip(tr("Consulta de inmediato si existe una versión más reciente de Mklipper disponible para instalar."));
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
    m_urlGrabberEnabled->setToolTip(
        tr("Abre un menú contextual automático cuando copias texto que coincide con una regla (por ejemplo, enlaces web o comandos personalizados)."));

    m_actionOnSelection = new QCheckBox(tr("Mostrar menú de acciones al seleccionar texto"), actOptCard);
    m_actionOnSelection->setChecked(s->actionOnSelection());
    m_actionOnSelection->setToolTip(
        tr("También evalúa las reglas de acciones cuando resaltas texto con el ratón (selección primaria de X11)."));

    m_replayInHistory = new QCheckBox(tr("Ejecutar acciones al elegir un elemento del historial"), actOptCard);
    m_replayInHistory->setChecked(s->replayActionInHistory());
    m_replayInHistory->setToolTip(
        tr("Vuelve a ofrecer el menú de acciones al seleccionar un elemento guardado previamente en el historial."));

    m_stripWhiteSpace = new QCheckBox(tr("Recortar espacios en blanco al evaluar acciones"), actOptCard);
    m_stripWhiteSpace->setChecked(s->stripWhiteSpace());
    m_stripWhiteSpace->setToolTip(
        tr("Ignora espacios y saltos de línea al inicio y al final del texto antes de comprobar si coincide con una expresión regular."));

    m_magicMime = new QCheckBox(tr("Ofrecer abrir enlaces (http/https) y archivos existentes"), actOptCard);
    m_magicMime->setChecked(s->enableMagicMimeActions());
    m_magicMime->setToolTip(
        tr("Detecta automáticamente URLs http/https y rutas de archivos locales existentes para ofrecer abrirlos con la aplicación predeterminada."));

    m_actionTimeout = new QSpinBox(actOptCard);
    m_actionTimeout->setRange(0, 200);
    m_actionTimeout->setValue(s->timeoutForActionPopups());
    m_actionTimeout->setSuffix(tr(" s (0 = sin límite)"));
    const QString timeoutTip = tr("Tiempo en segundos que permanece visible el menú automático de acciones antes de cerrarse solo. Usa 0 para que no se cierre automáticamente.");

    actOptForm->addRow(m_urlGrabberEnabled);
    actOptForm->addRow(m_actionOnSelection);
    actOptForm->addRow(m_replayInHistory);
    actOptForm->addRow(m_stripWhiteSpace);
    actOptForm->addRow(m_magicMime);
    addTooltipFormRow(actOptForm,
                      tr("Duración del menú automático:"),
                      wrapSpinBoxRow(m_actionTimeout, QStringLiteral("200"), timeoutTip, actOptCard),
                      timeoutTip,
                      actOptCard);

    auto *customCard = addSectionCard(actLayout, tr("Acciones personalizadas por expresión regular"), t, actionsPage);
    auto *customLayout = new QVBoxLayout(customCard);
    customLayout->setContentsMargins(14, 12, 14, 12);
    customLayout->setSpacing(10);

    m_actionsList = new QListWidget(customCard);
    m_actionsList->setMinimumHeight(110);
    m_actionsList->setToolTip(
        tr("Lista de reglas personalizadas que ejecutan comandos cuando el texto copiado coincide con una expresión regular. Haz doble clic para editar."));
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
    addBtn->setToolTip(tr("Crea una nueva regla de acción basada en una expresión regular."));

    auto *editBtn = new QPushButton(
        FlatIcons::icon(FlatIcon::Edit, t.textPrimary, t.textPrimary, t.textMuted, 15),
        tr("Editar…"),
        customCard);
    editBtn->setIconSize(QSize(15, 15));
    editBtn->setCursor(Qt::PointingHandCursor);
    editBtn->setToolTip(tr("Modifica la regla y los comandos de la acción seleccionada."));

    auto *delBtn = new QPushButton(
        FlatIcons::icon(FlatIcon::Trash, t.textSecondary, t.danger, t.textMuted, 15),
        tr("Eliminar"),
        customCard);
    delBtn->setProperty("destructive", true);
    delBtn->setIconSize(QSize(15, 15));
    delBtn->setCursor(Qt::PointingHandCursor);
    delBtn->setToolTip(tr("Elimina la acción personalizada seleccionada de la lista."));

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

    // Card 1: Popup keyboard shortcuts
    auto *popupKeysCard = addSectionCard(keysLayout, tr("Atajos dentro de la ventana flotante"), t, keysPage);
    auto *popupKeysLayout = new QVBoxLayout(popupKeysCard);
    popupKeysLayout->setContentsMargins(14, 12, 14, 12);
    popupKeysLayout->setSpacing(6);

    m_quickSelectDigits = new QCheckBox(tr("Usar teclas 1–0 para pegar rápido desde la ventana flotante"), popupKeysCard);
    m_quickSelectDigits->setChecked(s->quickSelectDigits());
    m_quickSelectDigits->setToolTip(
        tr("Permite presionar las teclas 1 a 0 (o Alt/Ctrl + 1–0) para pegar ese elemento al instante. Desactívalo si prefieres escribir números directamente en el buscador."));
    popupKeysLayout->addWidget(m_quickSelectDigits);

    // Card 2: Global system shortcuts (verified real Mklipper actions)
    auto *keysCard = addSectionCard(keysLayout, tr("Atajos globales del sistema"), t, keysPage);
    auto *keysForm = new QFormLayout(keysCard);
    keysForm->setContentsMargins(14, 12, 14, 12);
    keysForm->setSpacing(10);
    keysForm->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

    auto makeKeyRow = [keysCard](const QString &seq, const QString &tip) {
        auto *rec = new ShortcutRecorder(QKeySequence::fromString(seq), keysCard);
        rec->setRowToolTip(tip);
        return rec;
    };

    const QString tipPopup = tr("Abre u oculta la ventana flotante del historial del portapapeles justo debajo del cursor.");
    const QString tipPlainText = tr("Activa o desactiva el modo «Pegar sin formato» desde cualquier aplicación y muestra una notificación.");
    const QString tipPause = tr("Pausa o reanuda temporalmente la captura del portapapeles (modo incógnito) y muestra una notificación.");
    const QString tipEdit = tr("Abre directamente el editor de texto de Mklipper para modificar el elemento actual del portapapeles.");
    const QString tipQr = tr("Genera y muestra al instante el código QR del elemento actual del portapapeles.");
    const QString tipAction = tr("Abre el menú de acciones disponibles (abrir enlaces/archivos o reglas personalizadas) para el elemento actual del portapapeles.");
    const QString tipCycleNext = tr("Pasa al siguiente elemento guardado en el historial sin abrir la ventana flotante.");
    const QString tipCyclePrev = tr("Vuelve al elemento anterior del historial durante la rotación rápida.");
    const QString tipClear = tr("Vacía todo el historial del portapapeles (respetando la confirmación y los elementos fijados).");

    m_keyPopup = makeKeyRow(s->shortcutPopup(), tipPopup);
    m_keyTogglePlainText = makeKeyRow(s->shortcutTogglePlainText(), tipPlainText);
    m_keyTogglePause = makeKeyRow(s->shortcutTogglePause(), tipPause);
    m_keyEditCurrent = makeKeyRow(s->shortcutEditCurrent(), tipEdit);
    m_keyShowQr = makeKeyRow(s->shortcutShowQr(), tipQr);
    m_keyRepeatAction = makeKeyRow(s->shortcutRepeatAction(), tipAction);
    m_keyCycleNext = makeKeyRow(s->shortcutCycleNext(), tipCycleNext);
    m_keyCyclePrev = makeKeyRow(s->shortcutCyclePrev(), tipCyclePrev);
    m_keyClearHistory = makeKeyRow(s->shortcutClearHistory(), tipClear);

    addTooltipFormRow(keysForm, tr("Mostrar historial bajo el cursor:"), m_keyPopup, tipPopup, keysCard);
    addTooltipFormRow(keysForm, tr("Alternar «Pegar sin formato»:"), m_keyTogglePlainText, tipPlainText, keysCard);
    addTooltipFormRow(keysForm, tr("Pausar / reanudar captura:"), m_keyTogglePause, tipPause, keysCard);
    addTooltipFormRow(keysForm, tr("Editar contenido actual:"), m_keyEditCurrent, tipEdit, keysCard);
    addTooltipFormRow(keysForm, tr("Mostrar código QR actual:"), m_keyShowQr, tipQr, keysCard);
    addTooltipFormRow(keysForm, tr("Invocar acciones del elemento actual:"), m_keyRepeatAction, tipAction, keysCard);
    addTooltipFormRow(keysForm, tr("Rotar historial (siguiente):"), m_keyCycleNext, tipCycleNext, keysCard);
    addTooltipFormRow(keysForm, tr("Rotar historial (anterior):"), m_keyCyclePrev, tipCyclePrev, keysCard);
    addTooltipFormRow(keysForm, tr("Vaciar historial:"), m_keyClearHistory, tipClear, keysCard);

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
        tr("Consejo: Los atajos configurados aquí se sincronizan automáticamente con Cinnamon (Configuración del sistema → Teclado → Atajos personalizados)."),
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

    auto *manualBtn = new QPushButton(
        FlatIcons::icon(FlatIcon::Book, t.textPrimary, t.textPrimary, t.textMuted, 15),
        tr("Manual de usuario"),
        this);
    manualBtn->setIconSize(QSize(15, 15));
    manualBtn->setCursor(Qt::PointingHandCursor);
    manualBtn->setToolTip(tr("Abre la guía rápida de uso con todas las funciones y atajos de Mklipper."));
    connect(manualBtn, &QPushButton::clicked, this, &ConfigDialog::showManualRequested);
    btnRow->addWidget(manualBtn);
    btnRow->addStretch();

    auto *cancelBtn = new QPushButton(tr("Cancelar"), this);
    cancelBtn->setCursor(Qt::PointingHandCursor);
    cancelBtn->setToolTip(tr("Cierra la configuración descartando los cambios no guardados."));
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);

    auto *saveBtn = new QPushButton(
        FlatIcons::icon(FlatIcon::Check, t.accentFg, t.accentFg, t.textMuted, 15),
        tr("Guardar"),
        this);
    saveBtn->setProperty("primary", true);
    saveBtn->setIconSize(QSize(15, 15));
    saveBtn->setCursor(Qt::PointingHandCursor);
    saveBtn->setToolTip(tr("Guarda y aplica inmediatamente todas las opciones y atajos configurados."));
    saveBtn->setDefault(true);
    connect(saveBtn, &QPushButton::clicked, this, &ConfigDialog::saveAndAccept);

    btnRow->addWidget(cancelBtn);
    btnRow->addWidget(saveBtn);
    mainLayout->addLayout(btnRow);

    refreshActionList();

    // Pick a comfortable initial size across all tabs so no horizontal scrollbar appears on any language
    const int maxTabW = qMax(generalPage->sizeHint().width(),
                             qMax(actionsPage->sizeHint().width(), keysPage->sizeHint().width()));
    const int maxTabH = qMax(generalPage->sizeHint().height(), keysPage->sizeHint().height());
    const QSize preferred(maxTabW + 76, maxTabH + 136);
    if (QScreen *scr = QGuiApplication::primaryScreen()) {
        const QRect avail = scr->availableGeometry();
        const int targetW = qBound(580, preferred.width(), static_cast<int>(avail.width() * 0.85));
        const int targetH = qBound(500, preferred.height(), static_cast<int>(avail.height() * 0.85));
        resize(targetW, targetH);
    } else {
        resize(qMax(640, preferred.width()), qMax(580, preferred.height()));
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

void ConfigDialog::addIgnoredApp()
{
    if (!m_ignoredAppEdit || !m_ignoredAppsList) {
        return;
    }
    const QString raw = m_ignoredAppEdit->text().trimmed();
    if (raw.isEmpty()) {
        return;
    }
    const QStringList parts = raw.split(QLatin1Char(','), Qt::SkipEmptyParts);
    for (const QString &part : parts) {
        const QString name = part.trimmed();
        if (name.isEmpty()) {
            continue;
        }
        bool exists = false;
        for (int i = 0; i < m_ignoredAppsList->count(); ++i) {
            if (m_ignoredAppsList->item(i)->text().compare(name, Qt::CaseInsensitive) == 0) {
                exists = true;
                break;
            }
        }
        if (!exists) {
            m_ignoredAppsList->addItem(name);
        }
    }
    m_ignoredAppEdit->clear();
}

void ConfigDialog::removeIgnoredApp()
{
    if (!m_ignoredAppsList) {
        return;
    }
    const int row = m_ignoredAppsList->currentRow();
    if (row >= 0) {
        delete m_ignoredAppsList->takeItem(row);
    }
}

void ConfigDialog::exportPinnedItems()
{
    const QString docsDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    const QString defaultPath = (docsDir.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::HomeLocation) : docsDir)
        + QStringLiteral("/mklipper-pinned-") + QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd")) + QStringLiteral(".json");

    const QString dest = QFileDialog::getSaveFileName(
        this,
        tr("Exportar elementos fijados"),
        defaultPath,
        tr("Archivo JSON (*.json)"));
    if (dest.isEmpty()) {
        return;
    }

    int count = 0;
    if (HistoryModel::self()->exportStarredToJson(dest, &count)) {
        QMessageBox::information(
            this,
            tr("Exportar elementos fijados"),
            tr("Se exportaron %1 elementos fijados correctamente.").arg(count));
    } else {
        QMessageBox::warning(
            this,
            tr("Exportar elementos fijados"),
            tr("No se pudo guardar el archivo de respaldo."));
    }
}

void ConfigDialog::importPinnedItems()
{
    const QString docsDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    const QString src = QFileDialog::getOpenFileName(
        this,
        tr("Importar elementos fijados"),
        docsDir.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::HomeLocation) : docsDir,
        tr("Archivo JSON (*.json)"));
    if (src.isEmpty()) {
        return;
    }

    int count = 0;
    if (HistoryModel::self()->importStarredFromJson(src, &count)) {
        QMessageBox::information(
            this,
            tr("Importar elementos fijados"),
            tr("Se importaron %1 elementos fijados correctamente.").arg(count));
    } else {
        QMessageBox::warning(
            this,
            tr("Importar elementos fijados"),
            tr("El archivo seleccionado no es un respaldo válido de Mklipper."));
    }
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
    s->setShowTrayBadge(m_showTrayBadge->isChecked());

    if (m_ignoredAppsList) {
        QStringList ignored;
        ignored.reserve(m_ignoredAppsList->count());
        for (int i = 0; i < m_ignoredAppsList->count(); ++i) {
            ignored.append(m_ignoredAppsList->item(i)->text());
        }
        s->setIgnoredApps(ignored);
    }

    s->setUrlGrabberEnabled(m_urlGrabberEnabled->isChecked());
    s->setActionOnSelection(m_actionOnSelection->isChecked());
    s->setReplayActionInHistory(m_replayInHistory->isChecked());
    s->setStripWhiteSpace(m_stripWhiteSpace->isChecked());
    s->setEnableMagicMimeActions(m_magicMime->isChecked());
    s->setTimeoutForActionPopups(m_actionTimeout->value());

    s->setQuickSelectDigits(m_quickSelectDigits->isChecked());
    s->setShortcutPopup(m_keyPopup->keySequence().toString());
    s->setShortcutTogglePlainText(m_keyTogglePlainText->keySequence().toString());
    s->setShortcutTogglePause(m_keyTogglePause->keySequence().toString());
    s->setShortcutEditCurrent(m_keyEditCurrent->keySequence().toString());
    s->setShortcutShowQr(m_keyShowQr->keySequence().toString());
    s->setShortcutRepeatAction(m_keyRepeatAction->keySequence().toString());
    s->setShortcutCycleNext(m_keyCycleNext->keySequence().toString());
    s->setShortcutCyclePrev(m_keyCyclePrev->keySequence().toString());
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
