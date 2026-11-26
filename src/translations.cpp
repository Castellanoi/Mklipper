/*
    SPDX-FileCopyrightText: 2026 Mklipper contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "translations.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QHash>
#include <QLocale>
#include <QTranslator>

#include <memory>

using namespace Qt::StringLiterals;

namespace
{
class MklipperEnglishTranslator : public QTranslator
{
public:
    MklipperEnglishTranslator()
    {
        // Complete Spanish -> English translation table covering all tr() calls in Mklipper
        m_map = {
            // ConfigDialog
            {"Configurar Mklipper", u"Configure Mklipper"_s},
            {"Gestor de portapapeles para Linux Mint", u"Clipboard manager for Linux Mint"_s},
            {"Historial y comportamiento", u"History & Behavior"_s},
            {"Guardar historial entre sesiones", u"Save clipboard history across sessions"_s},
            {"Evitar que el portapapeles quede vacío al cerrar aplicaciones", u"Prevent clipboard from becoming empty when apps close"_s},
            {"Guardar imágenes (capturas de pantalla) en el historial", u"Save images (screenshots) in history"_s},
            {" elementos", u" items"_s},
            {"Pedir confirmación antes de vaciar el historial", u"Ask for confirmation before clearing history"_s},
            {"Pegar automáticamente al elegir un elemento (además de copiarlo)", u"Auto-paste when selecting an item (in addition to copying)"_s},
            {"Pegar sin formato (solo texto plano, sin colores ni estilos HTML)", u"Paste without formatting (plain text only, no HTML colors or styles)"_s},
            {"Mantener la ventana flotante abierta al hacer clic en elementos", u"Keep floating window open when clicking items"_s},
            {"Cerrar la ventana al hacer clic fuera de ella", u"Close window when clicking outside"_s},
            {"Guardar origen de los elementos copiados (aplicación y enlace/ventana)", u"Save source of copied items (application and link/window)"_s},
            {"Mostrar etiqueta de origen en la lista del portapapeles", u"Show source label in the clipboard list"_s},
            {"Tamaño máximo del historial:", u"Maximum history size:"_s},
            {"Selección de texto (X11)", u"Text Selection (X11)"_s},
            {"Guardar texto seleccionado con el mouse en el historial", u"Save mouse-selected text in history"_s},
            {"Sincronizar selección del mouse y portapapeles (Ctrl+C)", u"Synchronize mouse selection and clipboard (Ctrl+C)"_s},
            {"Sistema y actualizaciones", u"System & Updates"_s},
            {"Iniciar Mklipper automáticamente al iniciar sesión", u"Start Mklipper automatically at login"_s},
            {"Buscar actualizaciones automáticamente al iniciar", u"Check for updates automatically on startup"_s},
            {"Idioma:", u"Language:"_s},
            {"Automático (idioma del sistema)", u"Automatic (system language)"_s},
            {"Versión instalada: <b>%1</b>", u"Installed version: <b>%1</b>"_s},
            {"Buscar actualizaciones ahora…", u"Check for updates now…"_s},
            {"General", u"General"_s},
            {"Opciones de acciones", u"Action Options"_s},
            {"Mostrar menú de acciones automáticamente al copiar", u"Show actions menu automatically on copy"_s},
            {"Mostrar menú de acciones al seleccionar texto", u"Show actions menu when selecting text"_s},
            {"Ejecutar acciones al elegir un elemento del historial", u"Run actions when choosing an item from history"_s},
            {"Recortar espacios en blanco al evaluar acciones", u"Trim whitespace when evaluating actions"_s},
            {"Ofrecer abrir enlaces (http/https) y archivos existentes", u"Offer to open links (http/https) and existing files"_s},
            {" s (0 = sin límite)", u" s (0 = unlimited)"_s},
            {"Duración del menú automático:", u"Automatic menu timeout:"_s},
            {"Acciones personalizadas por expresión regular", u"Custom Regular Expression Actions"_s},
            {"Agregar…", u"Add…"_s},
            {"Editar…", u"Edit…"_s},
            {"Eliminar", u"Delete"_s},
            {"Acciones", u"Actions"_s},
            {"Limpiar", u"Clear"_s},
            {"Mostrar historial bajo el cursor:", u"Show history at cursor:"_s},
            {"Rotar historial (siguiente):", u"Cycle history (next):"_s},
            {"Rotar historial (anterior):", u"Cycle history (previous):"_s},
            {"Invocar acciones del elemento actual:", u"Invoke actions for current item:"_s},
            {"Vaciar historial:", u"Clear history:"_s},
            {"Consejo: También podés asignar un atajo nativo en Cinnamon (Configuración del sistema → Teclado → Atajos personalizados) apuntando al comando <b>mklipper --toggle</b>.",
             u"Tip: You can also bind a native Cinnamon shortcut (System Settings → Keyboard → Custom Shortcuts) pointing to the command <b>mklipper --toggle</b>."_s},
            {"Atajos", u"Shortcuts"_s},
            {"Guardar", u"Save"_s},
            {"Cancelar", u"Cancel"_s},
            {"Aceptar", u"OK"_s},

            // EditActionDialog
            {"Editar comando", u"Edit Command"_s},
            {"Ej: xdg-open %s", u"e.g. xdg-open %s"_s},
            {"Descripción del comando", u"Command description"_s},
            {"Ignorar salida", u"Ignore output"_s},
            {"Reemplazar elemento en el portapapeles", u"Replace clipboard item"_s},
            {"Agregar salida como nuevo elemento", u"Append output as new item"_s},
            {"Habilitado", u"Enabled"_s},
            {"Variables disponibles: %s (texto), %0..%9 (grupos capturados por la regex).",
             u"Available variables: %s (text), %0..%9 (regex capture groups)."_s},
            {"Comando:", u"Command:"_s},
            {"Descripción:", u"Description:"_s},
            {"Salida:", u"Output:"_s},
            {"Editar acción", u"Edit Action"_s},
            {"Nueva acción", u"New Action"_s},
            {"Ej: ^https?://.*", u"e.g. ^https?://.*"_s},
            {"Descripción de la acción", u"Action description"_s},
            {"Incluir en el menú automático de acciones", u"Include in automatic actions menu"_s},
            {"Expresión regular:", u"Regular expression:"_s},
            {"Comando", u"Command"_s},
            {"Descripción", u"Description"_s},
            {"Salida", u"Output"_s},
            {"Agregar comando", u"Add Command"_s},
            {"Editar", u"Edit"_s},
            {"Expresión regular inválida", u"Invalid Regular Expression"_s},
            {"Ingresá una expresión regular válida.", u"Please enter a valid regular expression."_s},
            {"Ignorar", u"Ignore"_s},
            {"Reemplazar", u"Replace"_s},
            {"Agregar", u"Add"_s},

            // HistoryModel
            {"Vaciar historial del portapapeles", u"Clear Clipboard History"_s},
            {"¿Seguro que querés vaciar el historial del portapapeles?\n(Los elementos fijados se conservarán).",
             u"Are you sure you want to clear the clipboard history?\n(Pinned items will be preserved)."_s},
            {"No volver a preguntar", u"Do not ask again"_s},
            {"Vaciar", u"Clear"_s},
            {"Imagen %1×%2", u"Image %1×%2"_s},
            {"Imagen", u"Image"_s},
            {"Eliminar elemento fijado", u"Delete Pinned Item"_s},
            {"Este elemento está fijado. ¿Seguro que querés eliminarlo del historial?",
             u"This item is pinned. Are you sure you want to delete it from history?"_s},

            // KlipperApp
            {"Actualizar Mklipper…", u"Update Mklipper…"_s},
            {"Abrir historial del portapapeles", u"Open clipboard history"_s},
            {"Ejecutar acciones en elemento actual", u"Run actions on current item"_s},
            {"Editar contenido actual…", u"Edit current contents…"_s},
            {"Mostrar código QR actual…", u"Show current QR code…"_s},
            {"Menú automático de acciones", u"Automatic actions menu"_s},
            {"Vaciar historial…", u"Clear history…"_s},
            {"Configurar Mklipper…", u"Configure Mklipper…"_s},
            {"Buscar actualizaciones…", u"Check for updates…"_s},
            {"Salir", u"Quit"_s},
            {"Mklipper — Mostrar historial", u"Mklipper — Show history"_s},
            {"Mklipper — Rotar historial (siguiente)", u"Mklipper — Cycle history (next)"_s},
            {"Mklipper — Rotar historial (anterior)", u"Mklipper — Cycle history (previous)"_s},
            {"Mklipper — Invocar acciones", u"Mklipper — Invoke actions"_s},
            {"Mklipper — Vaciar historial", u"Mklipper — Clear history"_s},
            {"Mklipper — Portapapeles vacío", u"Mklipper — Clipboard empty"_s},
            {"Mklipper — Actual:\n%1", u"Mklipper — Current:\n%1"_s},
            {"Actualizar a Mklipper %1…", u"Update to Mklipper %1…"_s},
            {"Nueva versión de Mklipper disponible", u"New version of Mklipper available"_s},
            {"Mklipper %1 ya está disponible. Hacé clic en el ícono de la bandeja → «Actualizar a Mklipper %1…» para instalarla.",
             u"Mklipper %1 is now available. Click the tray icon → \"Update to Mklipper %1…\" to install it."_s},
            {"Mklipper actualizado", u"Mklipper updated"_s},
            {"Se instaló Mklipper %1 correctamente. Reiniciando…", u"Mklipper %1 was installed successfully. Restarting…"_s},
            {"Historial del portapapeles", u"Clipboard History"_s},
            {"Más reciente: %1", u"Newer: %1"_s},
            {"Actual: %1", u"Current: %1"_s},
            {"Anterior: %1", u"Previous: %1"_s},

            // KlipperPopup
            {"Aplicación: %1", u"Application: %1"_s},
            {"Ventana: %1", u"Window: %1"_s},
            {"Enlace: %1", u"Link: %1"_s},
            {"Abrir enlace de origen", u"Open source link"_s},
            {"Editar texto", u"Edit text"_s},
            {"Mostrar código QR", u"Show QR code"_s},
            {"Ejecutar acción", u"Run action"_s},
            {"Desfijar", u"Unpin"_s},
            {"Fijar elemento", u"Pin item"_s},
            {"Mklipper — Portapapeles", u"Mklipper — Clipboard"_s},
            {"Buscar en el historial…", u"Search history…"_s},
            {"Mostrar solo elementos fijados", u"Show pinned items only"_s},
            {"Vaciar historial", u"Clear history"_s},
            {"Cerrar ventana", u"Close window"_s},
            {"Pegar sin formato", u"Paste without formatting"_s},
            {"Quita colores, fuentes y estilos HTML al pegar texto (no afecta imágenes ni archivos)",
             u"Strips HTML colors, fonts, and styles when pasting text (does not affect images or files)"_s},
            {"🔒 El portapapeles contiene una contraseña (no guardada)",
             u"🔒 Clipboard contains a password (not saved)"_s},
            {"El historial está vacío.", u"History is empty."_s},
            {"Volver", u"Back"_s},
            {"Editar contenido", u"Edit Contents"_s},
            {"Solo texto plano", u"Plain text only"_s},
            {"Con formato original", u"Original formatting"_s},
            {"No hay coincidencias.", u"No matches found."_s},
            {"El historial del portapapeles está vacío.", u"Clipboard history is empty."_s},
            {"Copiá texto, enlaces o imágenes para verlos aquí.", u"Copy text, links, or images to see them here."_s},
            {"Probá con otro término o quitá el filtro de fijados.", u"Try a different search term or clear the pinned filter."_s},
            {"%1 elementos", u"%1 items"_s},
            {" (%1 fijados)", u" (%1 pinned)"_s},
            {"  ·  1–0 / Clic: pegar  ·  Supr: borrar  ·  Esc: cerrar",
             u"  ·  1–0 / Click: paste  ·  Del: delete  ·  Esc: close"_s},

            // main.cpp
            {"Mklipper — Gestor de portapapeles nativo para Linux Mint (port de KDE Klipper)",
             u"Mklipper — Native clipboard manager for Linux Mint (KDE Klipper port)"_s},
            {"Mostrar el popup del historial bajo el cursor", u"Show history popup under the cursor"_s},
            {"Mostrar u ocultar el popup del historial bajo el cursor", u"Toggle history popup under the cursor"_s},
            {"Rotar al siguiente elemento del historial", u"Cycle to the next history item"_s},
            {"Rotar al elemento anterior del historial", u"Cycle to the previous history item"_s},
            {"Invocar acciones sobre el elemento actual", u"Invoke actions on the current item"_s},
            {"Vaciar el historial del portapapeles", u"Clear clipboard history"_s},
            {"Abrir la ventana de configuración", u"Open the settings window"_s},
            {"Buscar actualizaciones de Mklipper", u"Check for Mklipper updates"_s},
            {"Cerrar la instancia en ejecución de Mklipper", u"Quit the running Mklipper instance"_s},

            // QRCodeWidget
            {"Código QR", u"QR Code"_s},
            {"Copiar QR", u"Copy QR"_s},
            {"Guardar PNG", u"Save PNG"_s},
            {"No se pudo generar el código QR\n(el texto es demasiado largo o está vacío).",
             u"Could not generate QR code\n(text is too long or empty)."_s},
            {"Guardar código QR", u"Save QR Code"_s},
            {"Imagen PNG (*.png)", u"PNG Image (*.png)"_s},

            // UpdateDialog
            {"Actualizar Mklipper", u"Update Mklipper"_s},
            {"Ver en GitHub", u"View on GitHub"_s},
            {"Omitir esta versión", u"Skip this version"_s},
            {"Reintentar", u"Retry"_s},
            {"Actualizar ahora", u"Update now"_s},
            {"Cerrar", u"Close"_s},
            {"Mklipper está actualizado", u"Mklipper is up to date"_s},
            {"Ya tenés instalada la última versión disponible (%1).",
             u"You already have the latest available version installed (%1)."_s},
            {"No se pudo buscar actualizaciones", u"Could not check for updates"_s},
            {"Descargando %1 (%2 / %3)…", u"Downloading %1 (%2 / %3)…"_s},
            {"Descargando %1 (%2)…", u"Downloading %1 (%2)…"_s},
            {"Actualización completada", u"Update complete"_s},
            {"Mklipper se actualizó correctamente a la versión %1. Reiniciando…",
             u"Mklipper was successfully updated to version %1. Restarting…"_s},
            {"No se pudo completar la actualización", u"Could not complete update"_s},
            {"Actualización cancelada.", u"Update cancelled."_s},
            {"Buscando actualizaciones…", u"Checking for updates…"_s},
            {"Versión instalada: %1 · Consultando GitHub Releases…",
             u"Installed version: %1 · Checking GitHub…"_s},
            {"Nueva versión disponible: Mklipper %1", u"New version available: Mklipper %1"_s},
            {"Versión instalada: <b>%1</b> → Nueva versión: <b>%2</b>",
             u"Installed version: <b>%1</b> → New version: <b>%2</b>"_s},
            {" · Descarga: %1", u" · Download: %1"_s},
            {"<br><i>No se encontró un paquete .deb para %1 en este release; podés abrir GitHub para descargarlo manualmente.</i>",
             u"<br><i>No .deb package for %1 was found in this release; you can open GitHub to download it manually.</i>"_s},
            {"No se incluyeron notas de versión para este release.",
             u"No release notes were included for this release."_s},
            {"Más tarde", u"Later"_s},
            {"Descargando Mklipper %1…", u"Downloading Mklipper %1…"_s},
            {"Instalando Mklipper %1…", u"Installing Mklipper %1…"_s},
            {"Se abrirá el diálogo de contraseña de Linux Mint para actualizar el paquete .deb. No cierres esta ventana.",
             u"Linux Mint's password prompt will appear to install the .deb package. Please do not close this window."_s},
            {"Instalando…", u"Installing…"_s},
            {"Reintentar instalación", u"Retry installation"_s},

            // Updater
            {"La URL de actualización configurada no es válida o no usa HTTPS.",
             u"The configured update URL is invalid or does not use HTTPS."_s},
            {"La URL de actualización redirigió a un dominio no permitido.",
             u"The update URL redirected to a disallowed domain."_s},
            {"Se alcanzó el límite temporal de consultas de GitHub. Intentá de nuevo en unos minutos.",
             u"GitHub API rate limit reached. Please try again in a few minutes."_s},
            {"No se pudo conectar con GitHub para buscar actualizaciones (%1).",
             u"Could not connect to GitHub to check for updates (%1)."_s},
            {"Respuesta inválida del servidor de actualizaciones.",
             u"Invalid response from the update server."_s},
            {"No se encontró un número de versión válido en el último release.",
             u"No valid version number found in the latest release."_s},
            {"No se encontró un paquete .deb compatible con tu arquitectura (%1) en este release.",
             u"No compatible .deb package for your architecture (%1) was found in this release."_s},
            {"No se encontró una suma de verificación SHA-256 para este release; la instalación fue cancelada por seguridad.",
             u"No SHA-256 checksum was found for this release; installation was aborted for security."_s},
            {"Descargando suma de verificación SHA-256…", u"Downloading SHA-256 checksum…"_s},
            {"No se pudo descargar el archivo de verificación SHA-256.",
             u"Could not download the SHA-256 verification file."_s},
            {"La descarga de verificación SHA-256 redirigió a un dominio no permitido.",
             u"The SHA-256 checksum download redirected to a disallowed domain."_s},
            {"El archivo %1 no contiene una suma SHA-256 válida para %2.",
             u"File %1 does not contain a valid SHA-256 checksum for %2."_s},
            {"El nombre del archivo de actualización no es válido.",
             u"The update package filename is not valid."_s},
            {"No se pudo crear un directorio temporal para descargar la actualización.",
             u"Could not create a temporary directory to download the update."_s},
            {"No se pudo abrir el archivo temporal para escribir la actualización.",
             u"Could not open temporary file to write the update."_s},
            {"Descargando %1…", u"Downloading %1…"_s},
            {"El paquete descargado excede el tamaño máximo permitido.",
             u"The downloaded package exceeds the maximum allowed size."_s},
            {"Error al descargar el paquete de actualización.",
             u"Failed to download the update package."_s},
            {"La descarga del paquete redirigió a un dominio no permitido.",
             u"The package download redirected to a disallowed domain."_s},
            {"No se pudo leer el paquete descargado para verificarlo.",
             u"Could not read the downloaded package for verification."_s},
            {"El archivo descargado no es un paquete Debian (.deb) válido.",
             u"The downloaded file is not a valid Debian (.deb) package."_s},
            {"Verificando integridad (SHA-256)…", u"Verifying integrity (SHA-256)…"_s},
            {"No se pudo calcular la suma SHA-256 del paquete descargado.",
             u"Could not compute SHA-256 checksum of the downloaded package."_s},
            {"Error de verificación de integridad: la suma SHA-256 no coincide.",
             u"Integrity verification failed: SHA-256 checksum mismatch."_s},
            {"Esperada: %1\nObtenida: %2", u"Expected: %1\nActual: %2"_s},
            {"El paquete descargado no corresponde a Mklipper para tu arquitectura.",
             u"The downloaded package does not match Mklipper for your architecture."_s},
            {"No se encontró 'pkexec' en el sistema para solicitar permisos de administrador.",
             u"'pkexec' was not found on the system to request administrator privileges."_s},
            {"Podés instalar el paquete manualmente con: sudo apt install ./%1",
             u"You can install the package manually with: sudo apt install ./%1"_s},
            {"Instalando actualización… Ingresá tu contraseña si el sistema la solicita.",
             u"Installing update… Enter your password if prompted by the system."_s},
            {"No se pudo iniciar el instalador del sistema (pkexec).",
             u"Could not start the system installer (pkexec)."_s},
            {"La instalación del paquete falló (código %1).",
             u"Package installation failed (exit code %1)."_s},

            // URLGrabber
            {"Abrir enlace / archivo", u"Open link / file"_s},
            {"Abrir con la aplicación predeterminada", u"Open with default application"_s},
        };
    }

    bool isEmpty() const override { return m_map.isEmpty(); }

    QString translate(const char * /*context*/, const char *sourceText, const char * /*disambiguation*/, int /*n*/) const override
    {
        if (!sourceText) {
            return {};
        }
        return m_map.value(QByteArray(sourceText));
    }

private:
    QHash<QByteArray, QString> m_map;
};

std::unique_ptr<MklipperEnglishTranslator> s_translator;
QString s_activeLang = u"es"_s;
} // namespace

namespace MklipperTranslations
{
QString resolveLanguage(const QString &configured)
{
    const QString norm = configured.trimmed().toLower();
    if (norm == u"es"_s || norm.startsWith(u"es_"_s) || norm.startsWith(u"es-"_s)) {
        return u"es"_s;
    }
    if (norm == u"en"_s || norm.startsWith(u"en_"_s) || norm.startsWith(u"en-"_s)) {
        return u"en"_s;
    }

    // "system" (or unrecognized): follow system locale, defaulting to English for non-Spanish locales
    const QLocale sysLocale = QLocale::system();
    if (sysLocale.language() == QLocale::Spanish) {
        return u"es"_s;
    }
    const QStringList uiLangs = sysLocale.uiLanguages();
    if (!uiLangs.isEmpty()) {
        const QString primary = uiLangs.first().toLower();
        if (primary == u"es"_s || primary.startsWith(u"es-"_s) || primary.startsWith(u"es_"_s)) {
            return u"es"_s;
        }
    }
    return u"en"_s;
}

QString activeLanguage()
{
    return s_activeLang;
}

bool applyLanguage(const QString &configured)
{
    const QString target = resolveLanguage(configured);
    const bool changed = (target != s_activeLang);

    if (auto *app = QCoreApplication::instance()) {
        if (s_translator) {
            app->removeTranslator(s_translator.get());
        }
        if (target == u"en"_s) {
            if (!s_translator) {
                s_translator = std::make_unique<MklipperEnglishTranslator>();
            }
            app->installTranslator(s_translator.get());
        }
    }

    s_activeLang = target;
    return changed;
}
} // namespace MklipperTranslations
