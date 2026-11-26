/*
    SPDX-FileCopyrightText: 2026 Mklipper contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "helpdialog.h"

#include "flaticons.h"
#include "themestyle.h"
#include "translations.h"

#include <QApplication>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTextBrowser>
#include <QVBoxLayout>

HelpDialog::HelpDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Manual de usuario — Mklipper"));
    setMinimumSize(580, 520);
    resize(660, 600);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(18, 18, 18, 16);
    mainLayout->setSpacing(12);

    // Header
    auto *headerLayout = new QHBoxLayout;
    headerLayout->setSpacing(12);

    m_logoLabel = new QLabel(this);
    m_logoLabel->setFixedSize(38, 38);
    headerLayout->addWidget(m_logoLabel);

    auto *titleLayout = new QVBoxLayout;
    titleLayout->setSpacing(1);

    auto *titleLabel = new QLabel(tr("Manual de usuario de Mklipper"), this);
    QFont tf = titleLabel->font();
    tf.setBold(true);
    tf.setPointSizeF(tf.pointSizeF() + 2.0);
    titleLabel->setFont(tf);

    m_subtitleLabel = new QLabel(tr("Guía rápida para aprovechar tu historial del portapapeles en Linux Mint"), this);
    QFont sf = m_subtitleLabel->font();
    sf.setPointSizeF(qMax(8.5, sf.pointSizeF() - 1.0));
    m_subtitleLabel->setFont(sf);

    titleLayout->addWidget(titleLabel);
    titleLayout->addWidget(m_subtitleLabel);
    headerLayout->addLayout(titleLayout, 1);
    mainLayout->addLayout(headerLayout);

    // Manual content
    m_browser = new QTextBrowser(this);
    m_browser->setOpenExternalLinks(false);
    m_browser->setOpenLinks(false);
    m_browser->setMarkdown(manualMarkdown());
    mainLayout->addWidget(m_browser, 1);

    // Footer
    auto *divider = new QFrame(this);
    divider->setObjectName(QStringLiteral("mklipperDivider"));
    mainLayout->addWidget(divider);

    auto *btnRow = new QHBoxLayout;
    btnRow->addStretch();
    auto *closeBtn = new QPushButton(tr("Cerrar"), this);
    closeBtn->setProperty("primary", true);
    closeBtn->setCursor(Qt::PointingHandCursor);
    closeBtn->setDefault(true);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    btnRow->addWidget(closeBtn);
    mainLayout->addLayout(btnRow);

    refreshTheme();
}

void HelpDialog::refreshTheme()
{
    const QPalette appPal = QApplication::palette();
    setPalette(appPal);
    const ThemeTokens t = ThemeTokens::fromPalette(appPal);
    setWindowIcon(FlatIcons::appLogoIcon(t.accent));
    setStyleSheet(ThemeStyle::dialogStyleSheet(t));

    if (m_logoLabel) {
        m_logoLabel->setPixmap(FlatIcons::appLogoPixmap(t.accent, 38, devicePixelRatioF()));
    }
    if (m_subtitleLabel) {
        QPalette sp = appPal;
        sp.setColor(QPalette::WindowText, t.textMuted);
        m_subtitleLabel->setPalette(sp);
    }
    update();
}

QString HelpDialog::manualMarkdown()
{
    if (MklipperTranslations::activeLanguage() == QStringLiteral("en")) {
        return QStringLiteral(R"(
### 1. Getting Started
- **Open the clipboard history:** press **`Super + V`** (customizable) or **left-click** the Mklipper icon in the system tray.
- **Paste an item:** click any entry, press **`Enter`** on the selected row, or press keys **`1`–`0`** to paste the first 10 items immediately.
- **Right-click the tray icon:** quick access to edit the current item, generate a QR code, run actions, toggle automatic actions, clear history, open Settings, check for updates, or open this manual.

### 2. Search, Pin & Organize
- **Instant search:** start typing as soon as the popup opens to filter by text, source app, window title, or URL.
- **Typing numbers directly:** if you prefer typing numbers straight into the search bar instead of quick-pasting, disable **Use keys 1–0 to quick-paste** in **Settings → Shortcuts**.
- **Pin favorites:** hover any card and click the **star** icon so important snippets stay at the top and are never deleted when clearing history. Click the **star button** in the top bar to show pinned items only.

### 3. Card Action Buttons
Hovering or selecting any item reveals quick actions on the right:
- **Star:** pin or unpin the item.
- **External link:** reopen the webpage where the text was copied from (when available).
- **Lightning bolt (Actions):** open matching actions for that item (e.g., open URLs/files or run a custom regex rule).
- **QR Code:** generate a scannable QR code (you can copy it or save it as PNG).
- **Pencil (Edit):** edit the copied text inline before pasting (**`Ctrl + Enter`** saves).
- **Trash (Delete):** permanently remove that item (or press **`Del`** on the selected row).

### 4. Paste Without Formatting
- Turn on the **Paste without formatting** switch at the top of the popup (or assign a global shortcut) to strip HTML colors, fonts, and styles when pasting text.
- Images and copied files are never affected.

### 5. Actions (Automatic & Custom Rules)
- **Built-in link/file detection:** when you copy a web link (`https://...`) or an existing local file path, Mklipper can offer to open it with your default app via the lightning-bolt button or an automatic popup menu.
- **Custom Regex Actions (*Settings → Actions*):** power users can define rules matching a regular expression and run any command using `%s` for the copied text—optionally replacing or adding the command output back to the clipboard.

### 6. Shortcuts & Privacy
- **Global shortcuts (*Settings → Shortcuts*):** configure shortcuts for opening history, toggling plain-text paste, editing the current item, showing a QR code, invoking actions, cycling history, or clearing history. They sync automatically with Cinnamon's Keyboard settings.
- **Password protection:** secrets copied from compatible password managers (KeePassXC, Bitwarden, 1Password, etc.) are never saved to history.
- **Clean deletion:** deleted items are immediately zeroed and reclaimed from disk (`history3.sqlite` and `data/`), leaving no residual data behind.

### 7. Updates
- Mklipper updates automatically through **Linux Mint Update Manager** alongside your system updates, or anytime via **Tray Menu → Check for updates…**.
)");
    }

    return QStringLiteral(R"(
### 1. Primeros pasos
- **Abrir el historial:** presioná **`Super + V`** (personalizable) o hacé **clic izquierdo** en el ícono de Mklipper en la bandeja del sistema.
- **Pegar un elemento:** hacé clic sobre cualquier tarjeta, presioná **`Enter`** en la fila seleccionada o usá las teclas **`1` a `0`** para pegar los primeros 10 elementos al instante.
- **Clic derecho en la bandeja:** acceso rápido para editar el elemento actual, generar un código QR, ejecutar acciones, activar/desactivar el menú automático de acciones, vaciar el historial, abrir Configuración, buscar actualizaciones o abrir este manual.

### 2. Buscar, fijar y organizar
- **Búsqueda instantánea:** apenas abrís la ventana podés escribir para filtrar por texto, aplicación de origen, título de ventana o dirección web.
- **Escribir números directo en el buscador:** si preferís que al apretar números se busquen en lugar de pegar rápido, desactivá **Usar teclas 1–0 para pegar rápido** en **Configuración → Atajos**.
- **Fijar favoritos:** pasá el mouse sobre cualquier tarjeta y tocá la **estrella** para dejarla siempre arriba y protegerla al vaciar el historial. El botón de **estrella** en la barra superior filtra para ver solo los fijados.

### 3. Botones de cada tarjeta
Al pasar el mouse o seleccionar un elemento aparecen sus acciones a la derecha:
- **Estrella:** fija o desfija el elemento.
- **Enlace externo:** abre nuevamente la página web de donde copiaste ese texto (si está disponible).
- **Rayo (Acciones):** abre las acciones disponibles para ese elemento (abrir enlaces/archivos o ejecutar reglas personalizadas).
- **Código QR:** genera un código QR listo para escanear con el celular (se puede copiar o guardar como PNG).
- **Lápiz (Editar):** permite modificar el texto copiado antes de pegarlo (**`Ctrl + Enter`** guarda).
- **Papelera (Eliminar):** borra ese elemento de forma definitiva (también funciona la tecla **`Supr`**).

### 4. Pegar sin formato
- Activá el interruptor **Pegar sin formato** arriba del listado (o asignale un atajo global) para quitar fuentes, colores y estilos HTML al pegar texto.
- No afecta imágenes ni archivos copiados.

### 5. Acciones automáticas y personalizadas
- **Enlaces y archivos:** cuando copiás una dirección web (`https://...`) o la ruta de un archivo existente en tu PC, Mklipper ofrece abrirlo con la aplicación predeterminada desde el botón de rayo o mediante un menú automático (activable desde el clic derecho o en **Configuración → Acciones**).
- **Reglas por expresión regular (*Configuración → Acciones*):** permite crear automatizaciones propias que ejecutan comandos usando `%s` como el texto copiado y, si querés, reemplazan o agregan el resultado al portapapeles.

### 6. Atajos de teclado y privacidad
- **Atajos globales (*Configuración → Atajos*):** podés configurar combinaciones para abrir el historial, alternar *Pegar sin formato*, editar el contenido actual, mostrar el código QR, invocar acciones, rotar el historial o vaciarlo. Se sincronizan automáticamente con la configuración de teclado de Cinnamon.
- **Protección de contraseñas:** los secretos copiados desde gestores compatibles (KeePassXC, Bitwarden, 1Password, etc.) no se guardan en el historial.
- **Borrado limpio:** al eliminar elementos, el espacio en disco se libera al instante y los datos borrados se sobrescriben sin dejar residuos.

### 7. Actualizaciones
- Mklipper se actualiza automáticamente desde el **Gestor de actualizaciones de Linux Mint** junto con el resto del sistema, o cuando quieras desde **Clic derecho en la bandeja → Buscar actualizaciones…**.
)");
}
