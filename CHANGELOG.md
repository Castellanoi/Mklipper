# Changelog

## v1.6.2

### 🇪🇸 Español
- **Rediseño minimalista y flat inspirado en Cinnamon 6.6:**
  - Nuevo motor propio de íconos vectoriales flat monocromáticos (estilo simbólico de Cinnamon 6.6 / Adwaita) en toda la aplicación: búsqueda, fijar, papelera, ajustes, cerrar, editar, código QR, enlaces, diálogo de configuración, actualizador y menú de bandeja.
  - Botones minimalistas sin bordes pesados, barra superior limpia, eliminación del recuadro anidado alrededor del historial, jerarquía visual por capas tonales, pestañas tipo *pill*, checkboxes/spinboxes planos y botones primarios con el color de acento del sistema.
  - Ícono de aplicación modernizado y completamente flat cuyo clip adopta dinámicamente el color de acento activo del usuario.
- **Integración nativa con el tema, color de acento y puntero de Cinnamon:**
  - Lectura directa del tema GTK3/Cinnamon activo (`org.cinnamon.desktop.interface gtk-theme`) y sus `@define-color` reales (fondo, superficies, texto y color de acento), resolviendo el fallo de `qt6-gtk-platformtheme` que caía en el azul genérico de Qt Fusion.
  - **Actualización en vivo sin reiniciar:** al cambiar el tema o el color de acento en **Configuración → Temas** de Cinnamon, Mklipper actualiza al instante el popup, los diálogos abiertos, el menú y el ícono de la bandeja.
  - Herencia automática del **tamaño y tema del puntero** configurado en Cinnamon (`cursor-size` y `cursor-theme`) para que el cursor no vuelva al tamaño por defecto al pasar sobre las ventanas de Mklipper.
- **Menú de la bandeja más limpio y confiable:**
  - Corregida la sincronización del menú contextual D-Bus con `xapp-sn-watcher` reutilizando un único `QMenu` persistente sin ítems ocultos fantasma.
  - Retiradas las entradas heredadas de Klipper sin uso práctico en el menú contextual (*«Ejecutar acciones en elemento actual»* y *«Menú automático de acciones»*).

### 🇬🇧 English
- **Minimalist, flat Cinnamon 6.6-inspired UI redesign:**
  - New custom monochrome flat SVG vector icon engine (Cinnamon 6.6 / Adwaita symbolic style) across the entire app: search, pin/star, trash, settings, close, edit, QR code, links, Settings dialog, Updater, and tray menu.
  - Borderless minimalist toolbar buttons, cleaner header without nested list frames, tonal surface layering, pill-style settings tabs, flat accented checkboxes/spinboxes, and accent-filled primary action buttons.
  - Modernized flat app icon whose clip dynamically adapts to the user's active desktop accent color.
- **Native Cinnamon theme, accent color, and cursor integration:**
  - Reads the active Cinnamon/GTK3 theme (`org.cinnamon.desktop.interface gtk-theme`) and its actual `@define-color` tokens (background, surfaces, text, and accent color), fixing `qt6-gtk-platformtheme`'s fallback to generic Qt Fusion blue.
  - **Live theme switching without restart:** changing theme or accent color in Cinnamon's **Themes** settings immediately updates the popup, open dialogs, tray menu, and tray icon.
  - Automatically inherits Cinnamon's configured **cursor size and theme** (`cursor-size` and `cursor-theme`) so the pointer keeps its custom size when hovering over Mklipper windows.
- **Cleaner, more reliable system tray menu:**
  - Fixed D-Bus context menu synchronization with `xapp-sn-watcher` by keeping a single persistent `QMenu` with no hidden placeholder items.
  - Removed unused legacy Klipper entries from the tray context menu ("Run actions on current item" and "Automatic actions menu").

## v1.6.1

### 🇪🇸 Español
- **Renovación estética moderna acorde a Linux Mint Cinnamon:**
  - Ventana flotante refinada con barra de búsqueda y botones redondeados alineados, contenedor enmarcado del historial, barra de desplazamiento minimalista, insignia de estado para *«Pegar sin formato»* y tarjetas del historial con mejores márgenes, miniaturas de imágenes con bordes redondeados y cápsula flotante translúcida para los botones de acción al pasar el mouse.
  - Estado vacío mejorado con ícono, título y texto guía centrado manteniendo exactamente la misma estructura y alineación del popup.
  - Ventana de Configuración modernizada con encabezado visual con versión, tarjetas redondeadas por sección y tarjeta destacada de consejos; diálogo de actualización con barra de progreso y notas refinadas; vista de código QR en tarjeta blanca redondeada legible incluso en temas oscuros.
  - Totalmente adaptable a temas claros, oscuros y colores de acento de Linux Mint (Mint-Y / GTK3).

### 🇬🇧 English
- **Modern Cinnamon-native visual polish across the entire app:**
  - Refined floating popup with rounded search field and aligned toolbar buttons, framed history panel, slim modern scrollbar, status pill for "Paste without formatting", sleeker clipboard cards, rounded image thumbnails, and a translucent floating pill behind row hover action buttons.
  - Enhanced empty state with icon, title, and helpful subtitle while keeping exact popup layout alignment.
  - Modernized Settings dialog with a clean app header & version badge, rounded section cards, and styled tip banner; polished Update dialog with sleek progress bar and release notes frame; crisp white rounded card framing QR codes for dark-theme readability.
  - Fully palette-driven to adapt seamlessly to Linux Mint light/dark themes and accent colors (Mint-Y / GTK3).

## v1.6.0

### 🇪🇸 Español
- **Integración con el Gestor de actualizaciones de Linux Mint (`mintupdate` / repositorio APT firmado):**
  - El paquete `.deb` ahora instala automáticamente la clave pública GPG (`/usr/share/keyrings/mklipper-archive-keyring.gpg`) y la fuente APT DEB822 (`/etc/apt/sources.list.d/mklipper.sources`) apuntando al repositorio firmado de Mklipper.
  - A partir de esta versión, las futuras actualizaciones aparecerán directamente en el **Gestor de actualizaciones de Linux Mint** (y mediante `sudo apt update && sudo apt upgrade`), sin necesidad de abrir el actualizador manual.
  - Nuevo script `packaging/build-apt-repo.sh` y publicación automática de la rama `apt-repo` firmada con GPG desde GitHub Actions en cada release.
- **Corrección visual en estado vacío:**
  - Los elementos de la ventana flotante (barra de búsqueda, botones, switch *«Pegar sin formato»*, recuadro central del historial y pie de estado) ahora mantienen exactamente la misma posición y tamaño haya o no elementos en el historial o resultados de búsqueda.
- **Limpieza real de disco y borrado seguro sin residuos:**
  - Activados `PRAGMA auto_vacuum = FULL`, `PRAGMA secure_delete = ON` y truncado de WAL (`PRAGMA wal_checkpoint(TRUNCATE)`) en cada eliminación individual o expulsión por límite, recuperando espacio en disco al instante y sobrescribiendo con ceros los datos borrados.
  - Limpieza automática al inicio de carpetas o registros huérfanos en `data/` y `aux`, y escritura atómica de adjuntos.
- **Auditoría y endurecimiento integral de seguridad:**
  - Permisos privados estrictos (`0700` en directorios y `0600` en `history3.sqlite`, `data/` y `mklipper.ini`).
  - Corrección de inyección de shell en macros de acciones personalizadas (`ClipCommandProcess`) con soporte seguro para `%s`, `'%s'` y `"%s"`.
  - Actualizador integrado endurecido: verificación **SHA-256 obligatoria**, lista blanca de dominios de GitHub (incluso tras redirecciones), validación estricta de versión/nombre, límites de descarga, verificación de metadatos `.deb` (`Package=mklipper` y arquitectura) antes de invocar `pkexec`, y escape HTML en el diálogo.
  - Validación estricta de UUIDs SHA-1 (prevención de path traversal), límites de tamaño de 20 MB en payloads del portapapeles, detección ampliada de gestores de contraseñas (`ExcludeClipboardContentFromMonitorProcessing`, 1Password, GNOME), protección contra `BadWindow` y lecturas X11 fuera de límites, escape de notificaciones/etiquetas, y flags de endurecimiento del binario (`PIE`, `Full RELRO`, `stack-protector-strong`, `_FORTIFY_SOURCE=2`).

### 🇬🇧 English
- **Linux Mint Update Manager (`mintupdate`) integration via signed APT repository:**
  - The `.deb` package now automatically installs the GPG public keyring (`/usr/share/keyrings/mklipper-archive-keyring.gpg`) and DEB822 APT source (`/etc/apt/sources.list.d/mklipper.sources`) pointing to Mklipper's signed APT repository.
  - Future updates will appear automatically in **Linux Mint Update Manager** (and via `sudo apt update && sudo apt upgrade`), with no manual update steps required.
  - Added `packaging/build-apt-repo.sh` and automated GPG-signed `apt-repo` branch publishing in GitHub Actions on every tagged release.
- **Fixed empty-state popup alignment:**
  - All popup controls (search bar, toolbar buttons, "Paste without formatting" switch, central history area, and status footer) now stay locked in their exact positions whether the history/filter is populated or empty.
- **True disk space reclamation & secure deletion:**
  - Enabled `PRAGMA auto_vacuum = FULL`, `PRAGMA secure_delete = ON`, and WAL truncation (`PRAGMA wal_checkpoint(TRUNCATE)`) on individual deletions and max-size evictions so deleted clipboard data is zeroed out and disk space is reclaimed immediately.
  - Added startup orphan cleanup for `data/` and `aux` plus atomic attachment persistence.
- **Comprehensive security hardening:**
  - Enforced strict owner-only permissions (`0700` dirs / `0600` files) on `history3.sqlite`, `data/`, and `mklipper.ini`.
  - Fixed shell command injection in custom action macros (`ClipCommandProcess`) with quote-context-aware escaping for `%s`, `'%s'`, and `"%s"`.
  - Hardened the in-app updater: mandatory **SHA-256** verification, strict GitHub domain allowlist (including redirects), strict semver/filename checks, download size caps, `dpkg-deb` package/architecture verification before `pkexec`, and safe HTML/link handling in the update dialog.
  - Strict SHA-1 hex UUID validation (preventing path traversal), uniform 20 MB clipboard payload cap, expanded password manager secret hints (`ExcludeClipboardContentFromMonitorProcessing`, 1Password, GNOME), X11 `actualFormat == 32` bounds check and `BadWindow` guard, plain-text/HTML escaping in UI & notifications, and compiler hardening flags (`PIE`, `Full RELRO`, `stack-protector-strong`, `_FORTIFY_SOURCE=2`).

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
