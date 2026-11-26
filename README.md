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
  <img src="https://img.shields.io/badge/Version-1.8.3-blue.svg" alt="Version: 1.8.3" />
  <img src="https://img.shields.io/badge/Qt-6.4%2B-41cd52.svg" alt="Qt 6.4+" />
  <img src="https://img.shields.io/badge/Languages-7-orange.svg" alt="Languages: 7" />
  <img src="https://img.shields.io/badge/Tested%20on-Linux%20Mint%20Cinnamon%20(X11)-87cf3e.svg" alt="Tested on Linux Mint Cinnamon (X11)" />
</p>

<p align="center">
  <a href="#es">🇪🇸 Español</a> &nbsp;|&nbsp;
  <a href="#en">🇬🇧 English</a> &nbsp;|&nbsp;
  <a href="#pt-br">🇧🇷 Português (BR)</a> &nbsp;|&nbsp;
  <a href="#fr">🇫🇷 Français</a> &nbsp;|&nbsp;
  <a href="#it">🇮🇹 Italiano</a> &nbsp;|&nbsp;
  <a href="#de">🇩🇪 Deutsch</a> &nbsp;|&nbsp;
  <a href="#ru">🇷🇺 Русский</a> &nbsp;|&nbsp;
  <a href="MANUAL.md">📖 Manual</a>
</p>

---

<a id="es"></a>

## 🇪🇸 Español

### ¿Qué es Mklipper?

**Mklipper** es un gestor de portapapeles nativo para **Linux Mint**, basado en la arquitectura y lógica del clásico **Klipper** de KDE Plasma 6, pero porteado a **Qt 6 puro (C++20)** sin ninguna dependencia de KDE Frameworks ni Plasma.

Está diseñado para integrarse de forma limpia y liviana en **Cinnamon (X11)**: adopta en vivo el tema GTK3, el color de acento y el tamaño de puntero del sistema, vive en la bandeja del sistema, sincroniza atajos globales con Cinnamon (`Super + V` por defecto) y ofrece pegado automático directo con teclado o ratón.

> [!WARNING]
> **Aviso importante:** Este proyecto fue **vibecodeado** y **no se han realizado pruebas exhaustivas**. Únicamente fue probado en **Linux Mint (Cinnamon sobre X11)**. Puede contener errores o comportamientos inesperados en otros escritorios o sesiones Wayland.

### Características principales

- **Diseño flat estilo Cinnamon 6.6 y sincronización en vivo:** iconos vectoriales flat monocromáticos, jerarquía tonal limpia, adopción real del tema GTK3 y color de acento activo sin reiniciar al cambiar de tema, y herencia del tamaño y tema del puntero configurado en Cinnamon.
- **Ventana flotante estilo Klipper con filtros y vista previa:** búsqueda instantánea insensible a acentos, filtros rápidos (*Todos · Texto · Imágenes · Enlaces · Fijados* con clic o `Tab`), vista previa ampliada (`Espacio`), guardado de capturas en PNG, edición en línea, generador de códigos QR, marcas de tiempo relativas y **modo ventana persistente** opcional.
- **Pegado rápido (`1`–`0` opcional) y «Pegar sin formato» (con inversión por `Shift`):** los elementos aparecen numerados (`1`–`9`, `0`) para pegarlos en un toque (o puedes desactivar las teclas `1–0` en *Configuración → Atajos* para escribir números directamente en el buscador). Incluye pegado automático inteligente (`Ctrl+V` / `Ctrl+Shift+V` en terminales), interruptor de **Pegar sin formato** e inversión puntual manteniendo **`Shift`** al pegar.
- **Compatibilidad universal con teclados estándar y gaming:** detección dinámica de modificadores en X11 (`XGetModifierMapping`), soporte completo de teclas extendidas **F1–F35** (incluidas teclas macro **F13–F24**), IME desactivado al grabar atajos y **selector manual visual** de teclas si el sistema o teclado interceptan `Super`/`Ctrl`/`Alt`.
- **Actualizaciones nativas en el Gestor de actualizaciones de Linux Mint (`mintupdate` / APT) con reinicio automático:** el paquete `.deb` configura el repositorio APT oficial firmado con GPG para recibir actualizaciones junto al sistema (`mintupdate` / `sudo apt upgrade`). Al actualizar el paquete, Mklipper detecta automáticamente el nuevo binario, guarda el historial y se reinicia solo. También incluye actualizador integrado con verificación obligatoria **SHA-256**, notas de versión filtradas al idioma del usuario e instalación vía `pkexec`.
- **Historial persistente en SQLite con origen del copiado, respaldo y cero residuos:** guarda texto, imágenes y enlaces en `~/.local/share/mklipper/history3.sqlite` (permisos `0700`/`0600`), registra opcionalmente la aplicación/ventana/URL de origen, permite **exportar e importar elementos fijados** en JSON y usa `PRAGMA auto_vacuum = FULL`, `PRAGMA secure_delete = ON` y limpieza de huérfanos para no dejar residuos en disco.
- **Privacidad, modo incógnito (pausa) y aplicaciones ignoradas:** botón y atajo para **pausar temporalmente la captura** (con insignia en la bandeja), lista personalizable de **aplicaciones ignoradas**, filtro automático de gestores de contraseñas (KeePassXC, Bitwarden, 1Password, `x-kde-passwordManagerHint`), pantallas de bloqueo (`cinnamon-screensaver`, etc.) y entradas vacías, además de compilación endurecida (`PIE`, `Full RELRO`, `stack-protector-strong`, `_FORTIFY_SOURCE=2`).
- **Acciones por regex (`URLGrabber`) y Manual de usuario integrado:** apertura de enlaces/archivos en un clic, reglas personalizadas por expresión regular, descripciones emergentes en toda la configuración y **Manual de usuario visual en 5 pestañas**.
- **7 idiomas dinámicos (JSON) y soporte HiDPI:** incluye **Español, English, Português (Brasil), Français, Italiano, Deutsch y Русский** con detección automática del idioma del sistema y cambio instantáneo desde *Configuración → General*.

### Instalación

#### Opción 1: Instalar el paquete `.deb` (recomendado para Linux Mint 22.x / Ubuntu 24.04)

1. Descarga el último paquete `.deb` desde [GitHub Releases](https://github.com/Castellanoi/Mklipper/releases/latest) o directamente [`dist/mklipper_1.8.3_amd64.deb`](dist/mklipper_1.8.3_amd64.deb).
2. Instálalo con `apt` (o doble clic en Linux Mint) para resolver automáticamente las dependencias de Qt 6:

```bash
sudo apt install ./mklipper_1.8.3_amd64.deb
```

3. Inicia **Mklipper** desde el menú de aplicaciones o ejecutando `mklipper`. Arrancará automáticamente en los próximos inicios de sesión (`/etc/xdg/autostart/mklipper.desktop`) y **recibirás las futuras actualizaciones directamente en el Gestor de actualizaciones de Linux Mint**.

#### Opción 2: Instalar directamente desde el repositorio APT firmado

```bash
curl -fsSL https://raw.githubusercontent.com/Castellanoi/Mklipper/apt-repo/mklipper-archive-keyring.gpg \
  | sudo tee /usr/share/keyrings/mklipper-archive-keyring.gpg >/dev/null
curl -fsSL https://raw.githubusercontent.com/Castellanoi/Mklipper/apt-repo/mklipper.sources \
  | sudo tee /etc/apt/sources.list.d/mklipper.sources >/dev/null
sudo apt update && sudo apt install mklipper
```

#### Opción 3: Compilar desde el código fuente y generar el `.deb`

```bash
sudo apt update && sudo apt install -y \
  build-essential cmake ninja-build pkg-config \
  qt6-base-dev libqt6sql6-sqlite qt6-svg-dev qt6-gtk-platformtheme \
  libx11-dev libxfixes-dev libxtst-dev libqrencode-dev zlib1g-dev

git clone https://github.com/Castellanoi/Mklipper.git
cd Mklipper
chmod +x packaging/build-deb.sh
./packaging/build-deb.sh
sudo apt install ./dist/mklipper_1.8.3_amd64.deb
```

### Desinstalación

```bash
sudo apt remove mklipper
# O eliminar también la configuración del repositorio APT:
sudo apt purge mklipper
```

### Licencia

Software libre bajo licencia **GNU GPL v3**. Consulta [LICENSE](LICENSE) para más detalles.

---

<a id="en"></a>

## 🇬🇧 English

### What is Mklipper?

**Mklipper** is a native, lightweight clipboard manager for **Linux Mint**, ported from KDE Plasma 6's **Klipper** engine into **pure Qt 6 (C++20)** with zero KDE Frameworks or Plasma runtime dependencies.

It blends naturally into **Cinnamon (X11)**: it syncs live with your active GTK3 theme, accent color, and cursor size, sits in the system tray, integrates global shortcuts natively with Cinnamon (`Super + V` by default), and auto-pastes seamlessly via keyboard or mouse.

> [!WARNING]
> **Disclaimer:** This project was **vibecoded** and has **not undergone exhaustive testing**. It has **only been tested on Linux Mint (Cinnamon / X11)**. Expect rough edges or untested behavior on other desktop environments or Wayland sessions.

### Key Features

- **Cinnamon 6.6-inspired flat UI & live desktop sync:** custom monochrome flat SVG icons, clean tonal surfaces, live GTK3 theme & accent color updates without restarting, and automatic cursor size/theme inheritance from Cinnamon.
- **Klipper-style floating popup with category filters & quick preview:** accent-insensitive instant search, quick filter chips (*All · Text · Images · Links · Pinned* via click or `Tab`), full-size image & long-text preview (`Space`), direct PNG saving on image cards, inline editing, QR code generator, relative timestamps, and optional **persistent window mode**.
- **Quick `1`–`0` paste (toggleable) & "Paste without formatting" (`Shift` override):** numbered items (`1`–`9`, `0`) paste in a single tap—or disable `1–0` quick-paste in *Settings → Shortcuts* to type numbers directly into search. Auto-pastes intelligently (`Ctrl+V` / `Ctrl+Shift+V` in terminals), with a **Paste without formatting** toggle and one-shot inversion by holding **`Shift`** while pasting.
- **Universal standard & gaming keyboard compatibility:** live X11 modifier discovery (`XGetModifierMapping`), full **F1–F35** extended key support (including **F13–F24** macro keys), IME disabled during capture, and a **visual manual key picker** whenever the OS or hardware intercepts `Super`/`Ctrl`/`Alt`.
- **Native Linux Mint Update Manager (`mintupdate` / signed APT repo) with auto-restart:** installing the `.deb` registers Mklipper's GPG-signed APT repository so updates arrive alongside system packages. When upgraded via APT/Mint Update Manager, Mklipper automatically detects the new binary, saves history, and cleanly restarts itself. Also includes a built-in updater with mandatory **SHA-256** verification, language-filtered release notes, and `pkexec` installation.
- **Persistent SQLite history with source tracking, backup & zero disk residue:** stores text, screenshots/images, and URLs in `~/.local/share/mklipper/history3.sqlite` (`0700`/`0600` permissions), optionally records source app/window/URL, lets you **export & import pinned items** as JSON, and uses `PRAGMA auto_vacuum = FULL`, `PRAGMA secure_delete = ON`, and orphan cleanup so deleted items leave no disk trace.
- **Hardened privacy, incognito pause mode & ignored apps list:** toggleable **Pause capture** (tray badge + global shortcut), customizable **Ignored applications** list, automatic filtering of password managers (KeePassXC, Bitwarden, 1Password, `x-kde-passwordManagerHint`), lockscreens (`cinnamon-screensaver`, etc.), and empty entries, plus binary hardening (`PIE`, `Full RELRO`, `stack-protector-strong`, `_FORTIFY_SOURCE=2`).
- **Custom regex actions (`URLGrabber`) & built-in User Manual:** one-click link/file opening, custom regex command rules, helpful hover tooltips on every setting, and a friendly **5-tab User Manual**.
- **7 dynamic JSON languages & HiDPI-ready:** ships with **Español, English, Português (Brasil), Français, Italiano, Deutsch, and Русский** with automatic system locale detection and instant switching in *Settings → General*.

### Installation

#### Option 1: Install the `.deb` package (recommended for Linux Mint 22.x / Ubuntu 24.04)

1. Download the latest `.deb` from [GitHub Releases](https://github.com/Castellanoi/Mklipper/releases/latest) or directly [`dist/mklipper_1.8.3_amd64.deb`](dist/mklipper_1.8.3_amd64.deb).
2. Install via `apt` (or double-click in Linux Mint):

```bash
sudo apt install ./mklipper_1.8.3_amd64.deb
```

3. Launch **Mklipper** from the application menu or by running `mklipper`. It starts automatically on login (`/etc/xdg/autostart/mklipper.desktop`) and **future updates appear directly in Linux Mint's Update Manager**.

#### Option 2: Install directly from the signed APT repository

```bash
curl -fsSL https://raw.githubusercontent.com/Castellanoi/Mklipper/apt-repo/mklipper-archive-keyring.gpg \
  | sudo tee /usr/share/keyrings/mklipper-archive-keyring.gpg >/dev/null
curl -fsSL https://raw.githubusercontent.com/Castellanoi/Mklipper/apt-repo/mklipper.sources \
  | sudo tee /etc/apt/sources.list.d/mklipper.sources >/dev/null
sudo apt update && sudo apt install mklipper
```

#### Option 3: Build from source

```bash
sudo apt update && sudo apt install -y \
  build-essential cmake ninja-build pkg-config \
  qt6-base-dev libqt6sql6-sqlite qt6-svg-dev qt6-gtk-platformtheme \
  libx11-dev libxfixes-dev libxtst-dev libqrencode-dev zlib1g-dev

git clone https://github.com/Castellanoi/Mklipper.git
cd Mklipper
chmod +x packaging/build-deb.sh
./packaging/build-deb.sh
sudo apt install ./dist/mklipper_1.8.3_amd64.deb
```

### Uninstall

```bash
sudo apt remove mklipper
# Or also remove the APT repository configuration:
sudo apt purge mklipper
```

### License

Free software under the **GNU GPL v3**. See [LICENSE](LICENSE) for details.

---

<a id="pt-br"></a>

## 🇧🇷 Português (Brasil)

### O que é o Mklipper?

**Mklipper** é um gerenciador de área de transferência nativo e leve para o **Linux Mint**, portado do clássico **Klipper** do KDE Plasma 6 para **Qt 6 puro (C++20)**, sem nenhuma dependência do KDE Frameworks ou Plasma.

Ele se integra de forma natural ao **Cinnamon (X11)**: sincroniza ao vivo com o tema GTK3, a cor de destaque e o tamanho do cursor do sistema, fica na bandeja do sistema, registra atalhos globais nativos no Cinnamon (`Super + V` por padrão) e cola automaticamente com teclado ou mouse.

> [!WARNING]
> **Aviso:** Este projeto foi **vibecodado** e **não passou por testes exaustivos**. Foi testado apenas no **Linux Mint (Cinnamon / X11)** e pode apresentar comportamentos inesperados em outros ambientes gráficos ou Wayland.

### Principais recursos

- **Visual flat estilo Cinnamon 6.6 com sincronização em tempo real:** ícones vetoriais monocromáticos, adaptação imediata ao tema GTK3 e à cor de destaque sem reiniciar, além de herdar o tamanho e o tema do cursor do Cinnamon.
- **Janela flutuante completa:** busca instantânea, miniaturas de imagens, edição de texto inline, gerador de código QR (copiar ou salvar em PNG) e modo de janela persistente opcional.
- **Colagem rápida (`1`–`0` opcional) e «Colar sem formatação»:** cole com um toque usando `1`–`0` (ou desative em *Configurações → Atalhos* para digitar números direto na busca). Cola automaticamente (`Ctrl+V` em apps comuns e `Ctrl+Shift+V` em terminais) e permite remover estilos HTML com um clique.
- **Compatibilidade universal com teclados comuns e gamer:** descoberta dinâmica de modificadores no X11, suporte a teclas **F1–F35** (incluindo macros **F13–F24**) e seletor manual de atalhos.
- **Atualizações nativas pelo Gerenciador de Atualizações do Linux Mint (`mintupdate` / APT) com reinício automático:** instala o repositório APT assinado com GPG e reinicia o Mklipper automaticamente ao atualizar o pacote, preservando o histórico. Também traz atualizador interno com verificação **SHA-256** e notas de versão no seu idioma.
- **Privacidade e histórico persistente sem resíduos:** banco SQLite privado (`0700`/`0600`) com `auto_vacuum = FULL` e `secure_delete = ON`, itens fixados (`★`), proteção automática de senhas (KeePassXC, Bitwarden, 1Password) e filtro contra capturas vazias ou de bloqueio de tela (`cinnamon-screensaver`, etc.).
- **Ações personalizadas por regex, dicas em todas as opções e Manual do usuário em 5 abas** + **7 idiomas** (Español, English, Português (Brasil), Français, Italiano, Deutsch e Русский).

### Instalação rápida

```bash
# Opção 1: Pacote .deb (baixe dist/mklipper_1.8.3_amd64.deb ou em GitHub Releases)
sudo apt install ./mklipper_1.8.3_amd64.deb

# Opção 2: Diretamente pelo repositório APT assinado
curl -fsSL https://raw.githubusercontent.com/Castellanoi/Mklipper/apt-repo/mklipper-archive-keyring.gpg \
  | sudo tee /usr/share/keyrings/mklipper-archive-keyring.gpg >/dev/null
curl -fsSL https://raw.githubusercontent.com/Castellanoi/Mklipper/apt-repo/mklipper.sources \
  | sudo tee /etc/apt/sources.list.d/mklipper.sources >/dev/null
sudo apt update && sudo apt install mklipper
```

Licença: **GNU GPL v3** ([LICENSE](LICENSE)).

---

<a id="fr"></a>

## 🇫🇷 Français

### Qu'est-ce que Mklipper ?

**Mklipper** est un gestionnaire de presse-papiers natif et léger pour **Linux Mint**, porté depuis le **Klipper** de KDE Plasma 6 vers **Qt 6 pur (C++20)** sans aucune dépendance à KDE Frameworks ou Plasma.

Il s'intègre naturellement à **Cinnamon (X11)** : synchronisation en direct avec votre thème GTK3, votre couleur d'accentuation et la taille du curseur, icône dans la zone de notification, raccourcis globaux natifs (`Super + V` par défaut) et collage automatique direct.

> [!WARNING]
> **Avertissement :** Ce projet a été développé en *vibecoding* et n'a **pas subi de tests exhaustifs**. Il a uniquement été testé sous **Linux Mint (Cinnamon / X11)**.

### Fonctionnalités principales

- **Interface flat inspirée de Cinnamon 6.6 et synchronisation en direct :** icônes vectorielles monochromes, adaptation instantanée au thème et à la couleur d'accentuation sans redémarrage, et respect de la taille du pointeur configurée dans Cinnamon.
- **Fenêtre flottante complète :** recherche instantanée, miniatures d'images, édition de texte intégrée, générateur de code QR (copier ou enregistrer en PNG) et mode fenêtre persistante en option.
- **Collage rapide (`1`–`0` désactivable) et « Coller sans mise en forme » :** collez instantanément avec `1`–`0` (ou désactivez l'option dans *Paramètres → Raccourcis* pour taper des chiffres directement dans la recherche). Collage automatique intelligent (`Ctrl+V` / `Ctrl+Maj+V` dans les terminaux) et interrupteur de texte brut.
- **Compatibilité universelle claviers standard et gaming :** détection dynamique X11 des modificateurs, prise en charge **F1–F35** (touches macro **F13–F24** incluses) et sélecteur manuel de raccourcis.
- **Mises à jour natives via le Gestionnaire de mises à jour de Linux Mint (`mintupdate` / APT) avec redémarrage automatique :** dépôt APT signé GPG configuré automatiquement et redémarrage propre après mise à jour. Inclut aussi un outil interne vérifié par **SHA-256** avec notes de version dans votre langue.
- **Confidentialité et historique SQLite sans résidus :** stockage privé (`0700`/`0600`) avec `auto_vacuum = FULL` et `secure_delete = ON`, éléments épinglés (`★`), filtrage des gestionnaires de mots de passe (KeePassXC, Bitwarden, 1Password), et filtrage des écrans de verrouillage (`cinnamon-screensaver`, etc.) et entrées vides.
- **Actions par expressions régulières, infobulles détaillées et Manuel d'utilisation en 5 onglets** + **7 langues** (Español, English, Português (Brasil), Français, Italiano, Deutsch, Русский).

### Installation rapide

```bash
# Option 1 : Paquet .deb (téléchargez dist/mklipper_1.8.3_amd64.deb ou via GitHub Releases)
sudo apt install ./mklipper_1.8.3_amd64.deb

# Option 2 : Depuis le dépôt APT signé
curl -fsSL https://raw.githubusercontent.com/Castellanoi/Mklipper/apt-repo/mklipper-archive-keyring.gpg \
  | sudo tee /usr/share/keyrings/mklipper-archive-keyring.gpg >/dev/null
curl -fsSL https://raw.githubusercontent.com/Castellanoi/Mklipper/apt-repo/mklipper.sources \
  | sudo tee /etc/apt/sources.list.d/mklipper.sources >/dev/null
sudo apt update && sudo apt install mklipper
```

Licence : **GNU GPL v3** ([LICENSE](LICENSE)).

---

<a id="it"></a>

## 🇮🇹 Italiano

### Cos'è Mklipper?

**Mklipper** è un gestore degli appunti nativo e leggero per **Linux Mint**, nato come porting del classico **Klipper** di KDE Plasma 6 in **Qt 6 puro (C++20)** senza alcuna dipendenza da KDE Frameworks o Plasma.

Si integra perfettamente in **Cinnamon (X11)**: si adatta dal vivo al tema GTK3, al colore di accento e alla dimensione del cursore, risiede nell'area di notifica, sincronizza le scorciatoie globali con Cinnamon (`Super + V` predefinito) e incolla automaticamente con tastiera o mouse.

> [!WARNING]
> **Avviso:** Questo progetto è stato realizzato in *vibecoding* e **non è stato sottoposto a test esaustivi**. È stato testato esclusivamente su **Linux Mint (Cinnamon / X11)**.

### Caratteristiche principali

- **Interfaccia flat stile Cinnamon 6.6 e sincronizzazione dal vivo:** icone vettoriali monocromatiche, aggiornamento istantaneo di tema e colore di accento senza riavvio e rispetto della dimensione del puntatore di Cinnamon.
- **Finestra mobile completa:** ricerca istantanea, miniature delle immagini, modifica del testo sul posto, generatore di codici QR (copia o salva in PNG) e modalità finestra persistente opzionale.
- **Incollaggio rapido (`1`–`0` disattivabile) e «Incolla senza formattazione»:** incolla con un tasto (`1`–`0`) oppure disattivalo in *Impostazioni → Scorciatoie* per digitare numeri direttamente nella ricerca. Incollaggio automatico intelligente (`Ctrl+V` / `Ctrl+Maiusc+V` nei terminali) e interruttore per testo semplice.
- **Compatibilità universale con tastiere standard e gaming:** rilevamento dinamico X11 dei modificatori, supporto completo **F1–F35** (inclusi tasti macro **F13–F24**) e selettore manuale delle scorciatoie.
- **Aggiornamenti nativi dal Gestore aggiornamenti di Linux Mint (`mintupdate` / APT) con riavvio automatico:** configura automaticamente il repository APT firmato GPG e riavvia Mklipper in modo pulito dopo ogni aggiornamento. Include anche un aggiornamento integrato verificato con **SHA-256** e note di rilascio nella tua lingua.
- **Privacy e cronologia SQLite senza residui:** archivio privato (`0700`/`0600`) con `auto_vacuum = FULL` e `secure_delete = ON`, elementi fissati (`★`), filtro automatico per gestori di password (KeePassXC, Bitwarden, 1Password), schermate di blocco (`cinnamon-screensaver`, ecc.) ed elementi vuoti.
- **Azioni personalizzate con regex, suggerimenti in tutte le opzioni e Manuale utente in 5 schede** + **7 lingue** (Español, English, Português (Brasil), Français, Italiano, Deutsch, Русский).

### Installazione rapida

```bash
# Opzione 1: Pacchetto .deb (scarica dist/mklipper_1.8.3_amd64.deb o da GitHub Releases)
sudo apt install ./mklipper_1.8.3_amd64.deb

# Opzione 2: Direttamente dal repository APT firmato
curl -fsSL https://raw.githubusercontent.com/Castellanoi/Mklipper/apt-repo/mklipper-archive-keyring.gpg \
  | sudo tee /usr/share/keyrings/mklipper-archive-keyring.gpg >/dev/null
curl -fsSL https://raw.githubusercontent.com/Castellanoi/Mklipper/apt-repo/mklipper.sources \
  | sudo tee /etc/apt/sources.list.d/mklipper.sources >/dev/null
sudo apt update && sudo apt install mklipper
```

Licenza: **GNU GPL v3** ([LICENSE](LICENSE)).

---

<a id="de"></a>

## 🇩🇪 Deutsch

### Was ist Mklipper?

**Mklipper** ist eine native, schlanke Zwischenablage-Verwaltung für **Linux Mint**, portiert vom klassischen **Klipper** aus KDE Plasma 6 auf **reines Qt 6 (C++20)** – ganz ohne Abhängigkeiten zu KDE Frameworks oder Plasma.

Es fügt sich nahtlos in **Cinnamon (X11)** ein: synchronisiert sich live mit GTK3-Theme, Akzentfarbe und Mauszeigergröße, läuft im Systemabschnitt, bindet globale Tastenkürzel direkt in Cinnamon ein (standardmäßig `Super + V`) und fügt automatisch per Tastatur oder Maus ein.

> [!WARNING]
> **Hinweis:** Dieses Projekt wurde per *Vibecoding* erstellt und **nicht umfassend getestet**. Es wurde ausschließlich unter **Linux Mint (Cinnamon / X11)** getestet.

### Hauptfunktionen

- **Flat-Design im Cinnamon-6.6-Stil & Live-Synchronisation:** monochrome Vektor-Icons, sofortige Übernahme von Theme- und Akzentfarbenwechseln ohne Neustart sowie Übernahme der Cinnamon-Mauszeigergröße.
- **Schwebendes Verlaufsfenster:** Sofortsuche, Bild-Miniaturen, direkte Textbearbeitung, QR-Code-Generator (kopieren oder als PNG speichern) und optionaler dauerhafter Fenstermodus.
- **Schnelleinfügen (`1`–`0` umschaltbar) & „Ohne Formatierung einfügen“:** Einträge per `1`–`0` sofort einfügen – oder unter *Einstellungen → Tastenkürzel* deaktivieren, um Ziffern direkt im Suchfeld zu tippen. Intelligentes automatisches Einfügen (`Strg+V` / `Strg+Umschalt+V` in Terminals) und Klartext-Schalter.
- **Universelle Tastatur- & Gaming-Kompatibilität:** dynamische X11-Modifikator-Erkennung, volle Unterstützung für **F1–F35** (inkl. **F13–F24**-Makrotasten) und manuelle Tastenauswahl.
- **Native Updates über die Linux-Mint-Aktualisierungsverwaltung (`mintupdate` / APT) mit automatischem Neustart:** richtet das GPG-signierte APT-Repository automatisch ein und startet Mklipper nach Paket-Updates sauber neu. Inkl. internem Updater mit **SHA-256**-Prüfung und Versionshinweisen in deiner Sprache.
- **Datenschutz & rückstandsfreier SQLite-Verlauf:** geschützter Speicher (`0700`/`0600`) mit `auto_vacuum = FULL` und `secure_delete = ON`, angeheftete Einträge (`★`), Passwortmanager-Schutz (KeePassXC, Bitwarden, 1Password) sowie automatischer Filter gegen Bildschirmsperren (`cinnamon-screensaver` usw.) und leere Einträge.
- **Regex-Aktionen, Tooltips für alle Optionen & Benutzerhandbuch in 5 Tabs** + **7 Sprachen** (Español, English, Português (Brasil), Français, Italiano, Deutsch, Русский).

### Schnellinstallation

```bash
# Option 1: .deb-Paket (dist/mklipper_1.8.3_amd64.deb oder über GitHub Releases laden)
sudo apt install ./mklipper_1.8.3_amd64.deb

# Option 2: Direkt über das signierte APT-Repository
curl -fsSL https://raw.githubusercontent.com/Castellanoi/Mklipper/apt-repo/mklipper-archive-keyring.gpg \
  | sudo tee /usr/share/keyrings/mklipper-archive-keyring.gpg >/dev/null
curl -fsSL https://raw.githubusercontent.com/Castellanoi/Mklipper/apt-repo/mklipper.sources \
  | sudo tee /etc/apt/sources.list.d/mklipper.sources >/dev/null
sudo apt update && sudo apt install mklipper
```

Lizenz: **GNU GPL v3** ([LICENSE](LICENSE)).

---

<a id="ru"></a>

## 🇷🇺 Русский

### Что такое Mklipper?

**Mklipper** — это нативный и лёгкий менеджер буфера обмена для **Linux Mint**, портированный из классического **Klipper** (KDE Plasma 6) на **чистый Qt 6 (C++20)** без каких-либо зависимостей от KDE Frameworks или Plasma.

Он органично интегрируется в **Cinnamon (X11)**: на лету синхронизируется с темой GTK3, акцентным цветом и размером курсора, работает в системном лотке, регистрирует глобальные клавиши в Cinnamon (по умолчанию `Super + V`) и поддерживает мгновенную автовставку с клавиатуры или мыши.

> [!WARNING]
> **Примечание:** Проект был создан методом *vibecoding* и **не проходил исчерпывающего тестирования**. Тестировался только на **Linux Mint (Cinnamon / X11)**.

### Основные возможности

- **Минималистичный flat-интерфейс в стиле Cinnamon 6.6 и живая синхронизация:** векторные монохромные значки, мгновенное применение смены темы и акцентного цвета без перезапуска, а также наследование размера и темы курсора Cinnamon.
- **Удобное всплывающее окно:** мгновенный поиск, миниатюры изображений, встроенное редактирование текста, генератор QR-кодов (копирование или сохранение в PNG) и опциональный режим закреплённого окна.
- **Быстрая вставка (`1`–`0` отключаема) и «Вставлять без форматирования»:** вставка в одно нажатие клавишами `1`–`0` (или отключите её в *Настройки → Клавиши*, чтобы вводить цифры сразу в поиск). Умная автовставка (`Ctrl+V` / `Ctrl+Shift+V` в терминалах) и переключатель обычного текста.
- **Универсальная поддержка обычных и игровых клавиатур:** динамическое определение модификаторов X11, полная поддержка клавиш **F1–F35** (включая макроклавиши **F13–F24**) и ручной выбор комбинаций.
- **Нативные обновления через Менеджер обновлений Linux Mint (`mintupdate` / APT) с автоперезапуском:** автоматически подключает подписанный GPG APT-репозиторий и чисто перезапускает Mklipper после обновления пакета. Также встроен собственный установщик обновлений с проверкой **SHA-256** и списком изменений на вашем языке.
- **Приватность и история SQLite без следов:** защищённое хранилище (`0700`/`0600`) с `auto_vacuum = FULL` и `secure_delete = ON`, закреплённые записи (`★`), защита паролей (KeePassXC, Bitwarden, 1Password), а также фильтрация экранов блокировки (`cinnamon-screensaver` и др.) и пустых записей.
- **Действия по регулярным выражениям, подсказки ко всем параметрам и Руководство пользователя в 5 вкладках** + **7 языков** (Español, English, Português (Brasil), Français, Italiano, Deutsch, Русский).

### Быстрая установка

```bash
# Вариант 1: Пакет .deb (скачайте dist/mklipper_1.8.3_amd64.deb или из GitHub Releases)
sudo apt install ./mklipper_1.8.3_amd64.deb

# Вариант 2: Напрямую из подписанного APT-репозитория
curl -fsSL https://raw.githubusercontent.com/Castellanoi/Mklipper/apt-repo/mklipper-archive-keyring.gpg \
  | sudo tee /usr/share/keyrings/mklipper-archive-keyring.gpg >/dev/null
curl -fsSL https://raw.githubusercontent.com/Castellanoi/Mklipper/apt-repo/mklipper.sources \
  | sudo tee /etc/apt/sources.list.d/mklipper.sources >/dev/null
sudo apt update && sudo apt install mklipper
```

Лицензия: **GNU GPL v3** ([LICENSE](LICENSE)).
