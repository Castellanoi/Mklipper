<p align="center">
  <img src="data/icons/mklipper.svg" alt="Mklipper icon" width="72" height="72" />
</p>

<h1 align="center">Mklipper</h1>

<p align="center">
  <strong>Port nativo y liviano de KDE Klipper para Linux Mint (Cinnamon / X11)</strong><br />
  <em>Native, lightweight port of KDE Klipper for Linux Mint (Cinnamon / X11)</em>
</p>

<p align="center">
  <a href="LICENSE"><img src="https://img.shields.io/badge/License-GPLv3-blue.svg" alt="License: GPL v3" /></a>
  <img src="https://img.shields.io/badge/Qt-6.4%2B-41cd52.svg" alt="Qt 6.4+" />
  <img src="https://img.shields.io/badge/Tested%20on-Linux%20Mint%20Cinnamon%20(X11)-87cf3e.svg" alt="Tested on Linux Mint Cinnamon (X11)" />
</p>

<p align="center">
  <a href="#-español">🇪🇸 Español</a> &nbsp;|&nbsp; <a href="#-english">🇬🇧 English</a>
</p>

---

## 🇪🇸 Español

### ¿Qué es Mklipper?

**Mklipper** es un gestor de portapapeles nativo para **Linux Mint**, basado en la arquitectura y lógica del clásico **Klipper** de KDE Plasma 6, pero porteado a **Qt 6 puro (C++20)** sin ninguna dependencia de KDE Frameworks ni Plasma.

Está pensado para integrarse de forma limpia y liviana en el escritorio **Cinnamon (X11)**: adopta automáticamente el tema GTK e íconos del sistema, incluye ícono en la barra de tareas, atajo global nativo (`Super + V`) y pegado directo con teclado o mouse.

> [!WARNING]
> **Aviso importante:** Este proyecto fue **vibecodeado** y **no se han realizado pruebas exhaustivas**. Únicamente fue testeado en **Linux Mint (Cinnamon sobre X11)**. Puede contener errores o comportamientos inesperados en otros escritorios o sesiones Wayland.

### Características principales

- **Ícono en la barra de tareas / bandeja del sistema:** clic izquierdo abre/cierra el popup junto al panel; clic derecho abre el menú rápido y la configuración.
- **Popup flotante estilo Klipper:** búsqueda instantánea, miniaturas de imágenes, edición de texto en línea y generador de códigos QR (con opción de copiar o guardar en PNG).
- **Numeración y pegado rápido (`1`–`0`):** los elementos aparecen numerados (`1` a `9`, `0` para el 10.º); con el popup abierto podés presionar la tecla numérica correspondiente (o `Alt+1`..`Alt+0` mientras buscás) para pegarlo al instante.
- **Pegado automático directo:** al hacer clic en un elemento (o elegirlo con `Enter` / `1`–`0`), además de dejarlo en el portapapeles, lo pega directamente en la ventana anterior (`Ctrl+V` en aplicaciones normales y `Ctrl+Shift+V` en terminales).
- **Historial persistente en SQLite:** guarda texto, imágenes/capturas y URLs en `~/.local/share/mklipper/history3.sqlite`.
- **Elementos fijados (`★`):** protegidos al vaciar el historial o alcanzar el límite máximo.
- **Privacidad:** ignora secretos marcados por gestores de contraseñas (`x-kde-passwordManagerHint`, KeePassXC, Bitwarden).
- **Acciones personalizadas por regex (`URLGrabber`)** y apertura rápida de enlaces/archivos.
- **Integración nativa con Cinnamon:** registra `Super + V` directamente en los atajos de Cinnamon, expone API D-Bus (`org.mklipper.App` / `org.kde.klipper`) y cuenta con comandos CLI (`mklipper --toggle`, `--popup`, `--configure`, `--clear`, `--quit`).

### Instalación

#### Opción 1: Instalar el paquete `.deb` (recomendado para Linux Mint 22.x / Ubuntu 24.04)

1. Descargá el paquete [`dist/mklipper_1.0.0_amd64.deb`](dist/mklipper_1.0.0_amd64.deb) (o cloná este repositorio).
2. Instalalo con `apt` para resolver automáticamente las dependencias de Qt 6:

```bash
sudo apt install ./dist/mklipper_1.0.0_amd64.deb
```

3. Iniciá **Mklipper** desde el menú de aplicaciones o ejecutando `mklipper` en una terminal. A partir del próximo inicio de sesión arrancará automáticamente (`/etc/xdg/autostart/mklipper.desktop`).

#### Opción 2: Compilar desde el código fuente y generar el `.deb`

1. Instalá las dependencias de compilación:

```bash
sudo apt update && sudo apt install -y \
  build-essential cmake ninja-build pkg-config \
  qt6-base-dev libqt6sql6-sqlite qt6-svg-dev qt6-gtk-platformtheme \
  libx11-dev libxfixes-dev libxtst-dev libqrencode-dev zlib1g-dev
```

2. Cloná el repositorio, compilá y generá el paquete `.deb`:

```bash
git clone https://github.com/Castellanoi/Mklipper.git
cd Mklipper
chmod +x packaging/build-deb.sh
./packaging/build-deb.sh
```

3. Instalá el paquete generado:

```bash
sudo apt install ./dist/mklipper_1.0.0_amd64.deb
```

### Desinstalación

```bash
sudo apt remove mklipper
```

### Licencia

Este software es libre y puede usarse, estudiarse, compartirse y modificarse a gusto bajo los términos de la **Licencia Pública General de GNU v3 (GNU GPL v3)**. Consultá el archivo [LICENSE](LICENSE) para más detalles.

---

## 🇬🇧 English

### What is Mklipper?

**Mklipper** is a native, lightweight clipboard manager for **Linux Mint**, ported from KDE Plasma 6's **Klipper** engine into **pure Qt 6 (C++20)** with zero KDE Frameworks or Plasma runtime dependencies.

It is designed to blend naturally into **Cinnamon (X11)**: it inherits your system GTK theme and icon theme, lives in the taskbar/system tray, integrates `Super + V` natively with Cinnamon's shortcut manager, and supports direct auto-pasting via keyboard or mouse.

> [!WARNING]
> **Disclaimer:** This project was **vibecoded** and has **not undergone exhaustive testing**. It has **only been tested on Linux Mint (Cinnamon / X11)**. Expect rough edges or untested behavior on other desktop environments or Wayland sessions.

### Key Features

- **System tray / taskbar icon:** left-click toggles the popup anchored near the panel; right-click opens the quick menu and settings.
- **Klipper-style floating popup:** instant search, image thumbnails, inline text editing, and built-in QR code generation (copy or save as PNG).
- **Numbered items & instant `1`–`0` paste:** entries are numbered (`1`–`9`, `0` for the 10th item); press `1`–`0` while the popup is open (or `Alt+1`..`Alt+0` while filtering) to paste immediately.
- **Direct auto-paste:** clicking an item (or pressing `Enter` / `1`–`0`) keeps it in the clipboard **and** automatically pastes it into the previously active window (`Ctrl+V` in regular apps, `Ctrl+Shift+V` in terminal emulators).
- **Persistent SQLite history:** stores text, images/screenshots, and URLs in `~/.local/share/mklipper/history3.sqlite`.
- **Pinned items (`★`):** protected from history clearing and max-size eviction.
- **Password privacy:** skips secrets flagged by password managers (`x-kde-passwordManagerHint`, KeePassXC, Bitwarden).
- **Custom regex actions (`URLGrabber`)** and quick URL/file opening.
- **Native Cinnamon integration:** registers `Super + V` directly in Cinnamon's custom keybindings, exposes a Klipper-compatible D-Bus API (`org.mklipper.App` / `org.kde.klipper`), and provides CLI flags (`mklipper --toggle`, `--popup`, `--configure`, `--clear`, `--quit`).

### Installation

#### Option 1: Install the `.deb` package (recommended for Linux Mint 22.x / Ubuntu 24.04)

1. Download [`dist/mklipper_1.0.0_amd64.deb`](dist/mklipper_1.0.0_amd64.deb) (or clone this repository).
2. Install it via `apt` so Qt 6 runtime dependencies are resolved automatically:

```bash
sudo apt install ./dist/mklipper_1.0.0_amd64.deb
```

3. Launch **Mklipper** from your application menu or by running `mklipper`. It will start automatically on subsequent logins via `/etc/xdg/autostart/mklipper.desktop`.

#### Option 2: Build from source and package the `.deb`

1. Install build dependencies:

```bash
sudo apt update && sudo apt install -y \
  build-essential cmake ninja-build pkg-config \
  qt6-base-dev libqt6sql6-sqlite qt6-svg-dev qt6-gtk-platformtheme \
  libx11-dev libxfixes-dev libxtst-dev libqrencode-dev zlib1g-dev
```

2. Clone the repo and build the `.deb` package:

```bash
git clone https://github.com/Castellanoi/Mklipper.git
cd Mklipper
chmod +x packaging/build-deb.sh
./packaging/build-deb.sh
```

3. Install the generated package:

```bash
sudo apt install ./dist/mklipper_1.0.0_amd64.deb
```

### Uninstall

```bash
sudo apt remove mklipper
```

### License

This project is free software — feel free to use, study, share, and modify it at will under the **GNU General Public License v3.0 (GPL-3.0)**. See [LICENSE](LICENSE) for details.
