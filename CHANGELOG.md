# Changelog

## v1.5.0

### 🇪🇸 Español
- **Registro y visualización del origen de lo copiado (aplicación y enlace/ventana):**
  - Captura automáticamente desde qué aplicación y ventana (o enlace web exacto cuando el navegador lo provee en el portapapeles) se copió cada elemento, con migración transparente de la base SQLite sin perder tu historial anterior.
  - Diseño visual limpio y sutil: muestra una pequeña línea discreta al pie de cada tarjeta (`Firefox · github.com`, `VS Code · main.cpp`, `Terminal`), tooltip completo al pasar el mouse y botón rápido `↗` solo al pasar el cursor si hay un enlace web de origen.
  - También podés buscar en el historial por nombre de aplicación, título de ventana o dominio.
  - Nuevas casillas en *Configuración → General* para habilitar/deshabilitar el guardado del origen (privacidad) y/o ocultar la etiqueta visual en la lista.

### 🇬🇧 English
- **Source tracking for copied items (application & link/window):**
  - Automatically captures the source application and window title (plus exact source URL when exposed by browsers on the clipboard), with a seamless backwards-compatible SQLite migration that preserves existing history.
  - Clean, uncluttered UI: displays a subtle single-line source footer on each card (`Firefox · github.com`, `VS Code · main.cpp`, `Terminal`), full source details on hover tooltip, and a hover-only `↗` quick button whenever a clickable source link is available.
  - Search bar also filters by source app name, window title, or domain.
  - New checkboxes under *Settings → General* to enable/disable recording source info (for privacy) and/or toggle the inline source label in the list.

## v1.4.0

### 🇪🇸 Español
- **Soporte multilenguaje (Español / English):**
  - Detecta automáticamente el idioma del sistema por defecto (*Automático*), y permite elegir manualmente entre **Español** e **English** en *Configuración → General → Sistema y actualizaciones* con aplicación inmediata sin reiniciar.
- **Mejor respuesta ante escalado de pantalla (HiDPI / fuentes grandes):**
  - Corregido el campo numérico *«Tamaño máximo del historial»* (`50 elementos`) y *«Duración del menú automático»* para que nunca se vean aplastados ni recorten el texto con aumento de pantalla activo (125%–200%).
  - Las pestañas de configuración ahora cuentan con scroll automático y tamaño inicial adaptado al área disponible de la pantalla; los botones, filas del historial y el interruptor *«Pegar sin formato»* del popup se escalan dinámicamente con el tamaño de fuente del sistema.
  - Activada la política de redondeo `PassThrough` para escalado fraccional nítido en Linux Mint.

### 🇬🇧 English
- **Multilingual support (English / Spanish):**
  - Automatically follows the system language by default (*Automatic*), and lets you switch manually between **Español** and **English** under *Settings → General → System & Updates* with live switching (no restart required).
- **Improved display scaling / HiDPI & large font responsiveness:**
  - Fixed the *Maximum history size* (`50 items`) and *Automatic menu timeout* spinboxes so they never get squished or clip text under display scaling (125%–200%).
  - Settings tabs now wrap cleanly in scroll areas with screen-clamped initial sizing; popup dimensions, list row heights, toolbar buttons, and the *Paste without formatting* toggle switch dynamically scale with system font metrics.
  - Enabled `PassThrough` HiDPI rounding policy for crisp fractional scaling on Linux Mint.

## v1.3.0

### 🇪🇸 Español
- **Switch «Pegar sin formato» directo en la ventana flotante:**
  - Nuevo interruptor visual (*toggle switch*) en la ventana principal del portapapeles (y sincronizado en *Configuración → General*) para activar o desactivar el pegado como texto plano con un solo clic.
  - Al activarlo, quita colores, fuentes y estilos HTML/RTF tanto al elegir elementos del historial como al copiar texto con formato y pegarlo directamente con `Ctrl+V`, sin afectar imágenes ni archivos copiados.
  - Conserva el formato original en el historial SQLite por si desactivás el switch más adelante.

### 🇬🇧 English
- **"Paste without formatting" toggle switch in the floating popup:**
  - New toggle switch right in the popup window (and synced in *Settings → General*) to turn plain-text pasting on or off with a single click.
  - When enabled, strips HTML/RTF colors, fonts, and styles both when clicking history items and when copying formatted text for direct `Ctrl+V` pasting, while leaving images and copied files untouched.
  - Preserves original formatting in the SQLite history in case you turn the switch back off later.

## v1.2.1

### 🇪🇸 Español
- **Actualizador integrado más confiable (doble fuente):**
  - Ahora consulta primero el manifiesto directo `dist/latest.json` en GitHub (`raw.githubusercontent.com`, sin límite de consultas ni dependencia de GitHub Actions) con respaldo automático a **GitHub Releases**.
  - Evita falsos *«ya estás en la última versión»* cuando GitHub Releases sufre demoras o interrupciones, y desactiva caché HTTP al buscar actualizaciones manualmente.
  - Mantiene verificación estricta de integridad **SHA-256** e instalación nativa con `pkexec apt-get install`.

### 🇬🇧 English
- **More reliable in-app updater (dual-source):**
  - Now checks the direct `dist/latest.json` manifest on GitHub (`raw.githubusercontent.com`, no API rate limits or GitHub Actions dependency) with automatic fallback to **GitHub Releases**.
  - Prevents false "already up to date" results during GitHub Releases delays/outages and bypasses HTTP cache on manual checks.
  - Preserves strict **SHA-256** integrity verification and native installation via `pkexec apt-get install`.

## v1.2.0

### 🇪🇸 Español
- **Modo ventana persistente mejorado:**
  - Nueva sub-opción en *Configuración → General* (**«Cerrar la ventana al hacer clic fuera de ella»**, disponible al activar la ventana flotante persistente).
  - Permite mantener la ventana abierta mientras hacés clic en varios elementos seguidos (incluso con pegado automático) y cerrarla tanto con la cruz (`✕`) / `Esc` como haciendo clic fuera de ella.
  - Al actualizar desde versiones anteriores se conservan intactas todas tus configuraciones e historial.

### 🇬🇧 English
- **Improved persistent floating popup mode:**
  - New sub-option under *Settings → General* (**"Cerrar la ventana al hacer clic fuera de ella"**, enabled when persistent floating window mode is active).
  - Keeps the popup open while clicking multiple items in a row (including with auto-paste enabled) while letting you close it either via the `✕` button / `Esc` or by clicking outside the popup.
  - Existing user settings and clipboard history are fully preserved when upgrading.

## v1.1.0

### 🇪🇸 Español
- **Actualizador integrado en la aplicación:**
  - Comprobación automática (opcional) y manual desde el menú de la bandeja (*Buscar actualizaciones…*), la ventana de configuración o el comando `mklipper --check-updates`.
  - Consulta directa a **GitHub Releases**, muestra las notas de la versión, descarga el `.deb` con barra de progreso, verifica la integridad **SHA-256**, instala de forma nativa con `pkexec apt-get install` y reinicia Mklipper automáticamente.
- **Modo ventana flotante persistente:**
  - Nueva opción en *Configuración → General* (desactivada por defecto) para mantener abierta la ventana flotante del historial al hacer clic en un elemento o al perder el foco.
- **Automatización de releases:**
  - Workflow de GitHub Actions que compila el paquete `.deb` en Ubuntu 24.04 (base de Linux Mint 22.x), genera `SHA256SUMS` y publica el Release al subir un tag `v*`.

### 🇬🇧 English
- **Built-in in-app updater:**
  - Optional automatic check on startup plus manual check from the tray menu (*Buscar actualizaciones…*), Settings dialog, or `mklipper --check-updates`.
  - Queries **GitHub Releases**, displays Markdown release notes, downloads the `.deb` with progress reporting, verifies **SHA-256** integrity, installs natively via `pkexec apt-get install`, and restarts Mklipper automatically.
- **Persistent floating popup mode:**
  - New option under *Settings → General* (disabled by default) to keep the floating history popup open after clicking an item or losing focus.
- **Automated releases:**
  - GitHub Actions workflow that builds the `.deb` package on Ubuntu 24.04 (Linux Mint 22.x base), generates `SHA256SUMS`, and publishes the GitHub Release on `v*` tags.

## v1.0.0

- Initial standalone Qt 6 port of KDE Klipper for Linux Mint (Cinnamon / X11).
