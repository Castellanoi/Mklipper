# Changelog

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
