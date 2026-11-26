# Changelog

## v1.8.0

### 🇪🇸 Español
- **Modo «Pausar captura» (incógnito):**
  - Nuevo interruptor en la ventana flotante, en el menú de la bandeja y como atajo global (`--toggle-pause` en CLI/D-Bus) para pausar temporalmente el registro del portapapeles. El icono de la bandeja muestra una insignia de pausa y el popup muestra un aviso discreto con botón *Reanudar*.
- **Filtros rápidos por categoría y búsqueda insensible a acentos:**
  - Nueva barra de filtros compactos (*Todos · Texto · Imágenes · Enlaces · Fijados*) navegables con clic o la tecla `Tab`.
  - La búsqueda ahora ignora acentos/diacríticos y admite múltiples palabras en cualquier orden (por ejemplo, `configuracion codigo` encuentra *«Configuración del código»*).
- **Vista previa ampliada, guardado de imágenes y pegado puntual con `Shift`:**
  - Pulsa `Espacio` (o el botón de ojo en la tarjeta) para ver imágenes a tamaño grande o textos largos completos sin tener que pegarlos.
  - Nuevo botón **Guardar imagen como PNG…** directamente en las tarjetas de capturas de pantalla y en la vista previa.
  - Mantén **`Shift`** al pulsar `Enter`, hacer clic o usar `Alt/Ctrl + 1–0` para invertir puntualmente el modo *«Pegar sin formato»* solo para ese pegado.
  - Las tarjetas y tooltips muestran ahora la fecha y hora relativa de copiado (*«Justo ahora»*, *«Hace 5 min»*, *«Ayer, 18:30»*).
- **Privacidad avanzada y respaldo de elementos fijados:**
  - Nueva sección **Privacidad y respaldo** en *Configuración → General* con lista editable de **aplicaciones ignoradas** y botones para **exportar e importar elementos fijados** en formato JSON (incluyendo imágenes y con permisos privados `0600`).
- **Español neutro en toda la aplicación y suite de pruebas automatizadas:**
  - Todo el español de la interfaz, el manual integrado, el `README` y los registros de cambios se unificó a **español neutro** internacional.
  - Incorporada suite oficial de pruebas con `CTest` (`tests/test_mklipper.cpp` + `tests/validate_i18n.py`) y flujo de integración continua en GitHub Actions.

### 🇬🇧 English
- **"Pause capture" (incognito) mode:**
  - New toggle in the floating popup, tray menu, and global shortcuts (`--toggle-pause` via CLI/D-Bus) to temporarily suspend clipboard recording, complete with a paused tray icon badge and a quick *Resume* banner.
- **Category filter chips & accent-insensitive search:**
  - New compact filter bar (*All · Text · Images · Links · Pinned*) accessible via click or `Tab`.
  - Instant search now ignores diacritics/accents and supports multi-word queries across text and source metadata.
- **Quick preview, image PNG export & one-shot `Shift` format toggle:**
  - Press `Space` (or click the eye button) to inspect full-size screenshots or long multi-line text without pasting.
  - New **Save image as PNG…** button directly on image cards and in the preview pane.
  - Hold **`Shift`** while clicking or pressing `Enter` to invert *"Paste without formatting"* for that single paste without changing the global toggle.
  - Cards and tooltips now display localized relative copy timestamps (*"Just now"*, *"5 min ago"*, *"Yesterday, 18:30"*).
- **Custom ignored apps list & pinned items backup:**
  - New **Privacy & Backup** section in *Settings → General* with an editable **Ignored applications** list and JSON **Export / Import pinned items** (with private `0600` permissions and deduplication).
- **Neutral international Spanish & automated `CTest` CI suite:**
  - Unified all Spanish UI strings, manual, README, and changelogs to neutral international Spanish.
  - Added automated `CTest` unit tests (`tests/test_mklipper.cpp` + `tests/validate_i18n.py`) and GitHub Actions CI.

### 🇧🇷 Português (Brasil)
- **Modo «Pausar captura» (anônimo):**
  - Novo botão na janela flutuante, no menu da bandeja e como atalho global (`--toggle-pause`) para suspender temporariamente o registro da área de transferência, com ícone de pausa na bandeja e aviso rápido para retomar.
- **Filtros rápidos por categoria e busca sem acentos:**
  - Nova barra de filtros (*Todos · Texto · Imagens · Links · Fixados*) navegável por clique ou tecla `Tab`, além de busca tolerante a acentos e com várias palavras.
- **Prévia ampliada, salvamento de imagens em PNG e inversão com `Shift`:**
  - Pressione `Espaço` (ou clique no ícone de olho) para visualizar capturas em tamanho maior ou textos longos; salve imagens do histórico direto em PNG; segure **`Shift`** ao colar para inverter pontualmente *«Colar sem formatação»*.
  - Cartões e dicas agora exibem a data/hora relativa da cópia.
- **Aplicativos ignorados, backup de fixados e testes automatizados:**
  - Nova seção **Privacidade e backup** em *Configurações → Geral* para definir aplicativos ignorados e exportar/importar itens fixados em JSON, além de suíte de testes automatizados (`CTest` + GitHub Actions).

### 🇫🇷 Français
- **Mode « Pause de la capture » (privé) :**
  - Nouveau bouton dans la fenêtre flottante, le menu de la zone de notification et les raccourcis globaux (`--toggle-pause`) pour suspendre temporairement l'enregistrement, avec badge sur l'icône et bannière *Reprendre*.
- **Filtres par catégorie et recherche insensible aux accents :**
  - Nouvelle barre de filtres (*Tous · Texte · Images · Liens · Épinglés*) accessible au clic ou via `Tab`, et recherche multi-mots insensible aux accents.
- **Aperçu rapide, export PNG et inversion ponctuelle avec `Maj` :**
  - Appuyez sur `Espace` (ou l'icône œil) pour afficher les captures en grand ou les textes longs ; enregistrez les images en PNG ; maintenez **`Maj`** lors du collage pour inverser ponctuellement *« Coller sans mise en forme »*.
  - Horodatage relatif affiché sur les cartes et infobulles.
- **Applications ignorées, sauvegarde des épinglés et tests CI :**
  - Nouvelle section **Confidentialité et sauvegarde** dans *Paramètres → Général* pour exclure des applications et exporter/importer les éléments épinglés en JSON, plus une suite de tests automatisés (`CTest` + GitHub Actions).

### 🇮🇹 Italiano
- **Modalità «Pausa cattura» (in incognito):**
  - Nuovo comando nella finestra mobile, nel menu dell'area di notifica e come scorciatoia globale (`--toggle-pause`) per sospendere temporaneamente la registrazione degli appunti, con icona di pausa e banner *Riprendi*.
- **Filtri rapidi per categoria e ricerca senza accenti:**
  - Nuova barra filtri (*Tutti · Testo · Immagini · Link · Fissati*) navigabile con clic o `Tab`, più ricerca multi-parola insensibile agli accenti.
- **Anteprima rapida, salvataggio PNG e inversione con `Maiusc`:**
  - Premi `Spazio` (o l'icona occhio) per visualizzare schermate in grande o testi lunghi; salva le immagini direttamente in PNG; tieni premuto **`Maiusc`** mentre incolli per invertire una volta *«Incolla senza formattazione»*.
  - Data e ora relative visibili su schede e suggerimenti.
- **Applicazioni ignorate, backup dei fissati e test automatici:**
  - Nuova sezione **Privacy e backup** in *Impostazioni → Generale* per escludere applicazioni ed esportare/importare gli elementi fissati in JSON, più suite di test automatici (`CTest` + GitHub Actions).

### 🇩🇪 Deutsch
- **Modus „Erfassung pausieren“ (Inkognito):**
  - Neuer Schalter im schwebenden Fenster, im Tray-Menü und als globales Tastenkürzel (`--toggle-pause`) zum vorübergehenden Pausieren der Aufzeichnung – inklusive pausiertem Tray-Symbol und *Fortsetzen*-Banner.
- **Kategoriefilter & akzentunabhängige Suche:**
  - Neue Filterleiste (*Alle · Text · Bilder · Links · Angeheftet*) per Klick oder `Tab` sowie akzentunabhängige Mehrwortsuche.
- **Schnellvorschau, PNG-Export & einmaliges Umkehren mit `Umschalt`:**
  - Mit `Leertaste` (oder Augensymbol) Bilder in voller Größe oder lange Texte ansehen; Verlaufsbilder direkt als PNG speichern; **`Umschalt`** beim Einfügen gedrückt halten, um *„Ohne Formatierung einfügen“* einmalig umzukehren.
  - Relative Zeitstempel direkt auf Karten und in Tooltips.
- **Ignorierte Anwendungen, Sicherung angehefteter Einträge & automatische Tests:**
  - Neuer Bereich **Datenschutz & Sicherung** unter *Einstellungen → Allgemein* für eigene Ausschlusslisten sowie JSON-Export/-Import angehefteter Einträge, ergänzt durch `CTest`-Unit-Tests und GitHub Actions CI.

### 🇷🇺 Русский
- **Режим «Пауза захвата» (инкогнито):**
  - Новый переключатель во всплывающем окне, меню трея и глобальных клавишах (`--toggle-pause`) для временной приостановки записи буфера обмена — со значком паузы в трее и кнопкой *Возобновить*.
- **Быстрые фильтры по категориям и поиск без учёта диакритики:**
  - Новая панель фильтров (*Все · Текст · Изображения · Ссылки · Закреплённые*) с переключением по клику или `Tab`, а также многословный поиск без учёта акцентов.
- **Быстрый предпросмотр, сохранение PNG и разовое переключение по `Shift`:**
  - Нажмите `Пробел` (или значок глаза) для просмотра изображений в полном размере или длинных текстов; сохраняйте снимки прямо в PNG; удерживайте **`Shift`** при вставке, чтобы временно инвертировать *«Вставлять без форматирования»*.
  - Относительное время копирования отображается на карточках и в подсказках.
- **Список игнорируемых приложений, резервная копия закреплённых и автотесты:**
  - Новый раздел **Приватность и резервное копирование** в *Настройки → Основные* со списком исключённых приложений и экспортом/импортом закреплённых записей в JSON, плюс автоматические тесты (`CTest` + GitHub Actions).

## v1.7.1

### 🇪🇸 Español
- **Filtro automático de salvapantallas, bloqueos de pantalla y capturas vacías:**
  - Al bloquear la pantalla (por ejemplo con `Super + L`), Mklipper ignora ahora cualquier evento de portapapeles originado por `cinnamon-screensaver`, otros bloqueadores de pantalla (`gnome-screensaver`, `mate-screensaver`, `xfce4-screensaver`, `xscreensaver`, `light-locker`, `kscreenlocker`, `i3lock`, `slock`, etc.) y agentes de autenticación `polkit`.
  - Se descartan también entradas vacías o compuestas únicamente por espacios en blanco para guardar solo contenido real copiado por el usuario.
- **Notas de actualización filtradas al idioma del usuario:**
  - La ventana de actualización muestra ahora únicamente la sección del *changelog* correspondiente al idioma activo del usuario (con respaldo automático a inglés/español en versiones anteriores), mientras que el registro de cambios incluye los 7 idiomas soportados.
- **Documentación multilingüe actualizada:**
  - [`README.md`](README.md) actualizado con las 7 traducciones completas (Español, English, Português (Brasil), Français, Italiano, Deutsch y Русский) y todas las funciones incorporadas hasta la v1.7.1.

### 🇬🇧 English
- **Automatic filtering of lockscreens, screensavers, and empty clipboard entries:**
  - When locking the screen (e.g. via `Super + L`), Mklipper now ignores clipboard events originating from `cinnamon-screensaver`, other screen lockers (`gnome-screensaver`, `mate-screensaver`, `xfce4-screensaver`, `xscreensaver`, `light-locker`, `kscreenlocker`, `i3lock`, `slock`, etc.), and `polkit` authentication agents.
  - Empty or whitespace-only clipboard entries are also discarded so only real user-copied content is recorded.
- **Language-filtered release notes in the Update dialog:**
  - The in-app Update window now displays only the changelog section matching the user's active language (with automatic fallback to English/Spanish for older releases), while the changelog itself includes all 7 supported languages.
- **Updated multilingual documentation:**
  - [`README.md`](README.md) updated with complete documentation in all 7 supported languages (Spanish, English, Portuguese (Brazil), French, Italian, German, and Russian) and all features up to v1.7.1.

### 🇧🇷 Português (Brasil)
- **Filtro automático para bloqueio de tela, protetores de tela e capturas vazias:**
  - Ao bloquear a tela (por exemplo com `Super + L`), o Mklipper agora ignora eventos da área de transferência originados por `cinnamon-screensaver`, outros bloqueadores de tela (`gnome-screensaver`, `mate-screensaver`, `xfce4-screensaver`, `xscreensaver`, `light-locker`, `kscreenlocker`, `i3lock`, `slock`, etc.) e agentes de autenticação `polkit`.
  - Entradas vazias ou contendo apenas espaços em branco também são descartadas para manter somente o que o usuário realmente copiou.
- **Notas de atualização filtradas no idioma do usuário:**
  - A janela de atualização agora exibe apenas a seção do *changelog* correspondente ao idioma ativo do usuário (com fallback automático para inglês/espanhol em versões anteriores), enquanto o registro de alterações inclui todos os 7 idiomas suportados.
- **Documentação multilíngue atualizada:**
  - [`README.md`](README.md) atualizado com todos os 7 idiomas suportados e todas as funcionalidades adicionadas até a v1.7.1.

### 🇫🇷 Français
- **Filtrage automatique des écrans de verrouillage, économiseurs d'écran et captures vides :**
  - Lors du verrouillage de l'écran (par exemple avec `Super + L`), Mklipper ignore désormais les événements du presse-papiers provenant de `cinnamon-screensaver`, d'autres verrouilleurs d'écran (`gnome-screensaver`, `mate-screensaver`, `xfce4-screensaver`, `xscreensaver`, `light-locker`, `kscreenlocker`, `i3lock`, `slock`, etc.) et des agents d'authentification `polkit`.
  - Les entrées vides ou composées uniquement d'espaces sont également ignorées afin de ne conserver que ce que l'utilisateur copie réellement.
- **Notes de version filtrées selon la langue de l'utilisateur :**
  - La fenêtre de mise à jour n'affiche désormais que la section du journal des modifications correspondant à la langue active (avec repli automatique vers l'anglais/l'espagnol pour les anciennes versions), tandis que le changelog complet inclut les 7 langues prises en charge.
- **Documentation multilingue mise à jour :**
  - [`README.md`](README.md) mis à jour dans les 7 langues prises en charge avec l'ensemble des fonctionnalités jusqu'à la v1.7.1.

### 🇮🇹 Italiano
- **Filtro automatico per blocco schermo, salvaschermo e catture vuote:**
  - Quando blocchi lo schermo (ad esempio con `Super + L`), Mklipper ora ignora gli eventi degli appunti generati da `cinnamon-screensaver`, altri blocchi schermo (`gnome-screensaver`, `mate-screensaver`, `xfce4-screensaver`, `xscreensaver`, `light-locker`, `kscreenlocker`, `i3lock`, `slock`, ecc.) e agenti di autenticazione `polkit`.
  - Anche gli elementi vuoti o composti solo da spazi bianchi vengono scartati per salvare solo i contenuti realmente copiati dall'utente.
- **Note di rilascio filtrate nella lingua dell'utente:**
  - La finestra di aggiornamento mostra ora solo la sezione del *changelog* nella lingua attiva (con fallback automatico all'inglese/spagnolo per le versioni precedenti), mentre il changelog include tutte le 7 lingue supportate.
- **Documentazione multilingue aggiornata:**
  - [`README.md`](README.md) aggiornato in tutte le 7 lingue supportate con tutte le funzionalità introdotte fino alla v1.7.1.

### 🇩🇪 Deutsch
- **Automatischer Filter für Bildschirmsperren, Bildschirmschoner und leere Einträge:**
  - Beim Sperren des Bildschirms (z. B. mit `Super + L`) ignoriert Mklipper nun Zwischenablage-Ereignisse von `cinnamon-screensaver`, anderen Bildschirmsperren (`gnome-screensaver`, `mate-screensaver`, `xfce4-screensaver`, `xscreensaver`, `light-locker`, `kscreenlocker`, `i3lock`, `slock` usw.) sowie `polkit`-Authentifizierungsagenten.
  - Leere Einträge oder reine Leerzeichen werden ebenfalls verworfen, sodass nur wirklich vom Benutzer kopierte Inhalte gespeichert werden.
- **Sprachgefilterte Versionshinweise im Aktualisierungsdialog:**
  - Das Aktualisierungsfenster zeigt nun ausschließlich den Abschnitt der Versionshinweise in der aktiven Sprache an (mit automatischem Fallback auf Englisch/Spanisch bei älteren Versionen), während das vollständige Changelog alle 7 unterstützten Sprachen enthält.
- **Mehrsprachige Dokumentation aktualisiert:**
  - [`README.md`](README.md) in allen 7 unterstützten Sprachen sowie mit allen Funktionen bis v1.7.1 aktualisiert.

### 🇷🇺 Русский
- **Автоматическая фильтрация экранов блокировки, хранителей экрана и пустых записей:**
  - При блокировке экрана (например, через `Super + L`) Mklipper теперь игнорирует события буфера обмена от `cinnamon-screensaver`, других блокировщиков экрана (`gnome-screensaver`, `mate-screensaver`, `xfce4-screensaver`, `xscreensaver`, `light-locker`, `kscreenlocker`, `i3lock`, `slock` и др.) и агентов аутентификации `polkit`.
  - Пустые записи и строки, состоящие только из пробелов, также отбрасываются, чтобы сохранялось только реально скопированное пользователем содержимое.
- **Список изменений на языке пользователя в окне обновлений:**
  - Окно обновлений теперь показывает только раздел списка изменений на активном языке интерфейса (с автоматическим откатом на английский/испанский для старых версий), при этом сам журнал изменений включает все 7 поддерживаемых языков.
- **Обновлённая многоязычная документация:**
  - Файл [`README.md`](README.md) обновлён на всех 7 поддерживаемых языках с описанием всех возможностей вплоть до v1.7.1.

## v1.7.0

### 🇪🇸 Español
- **5 nuevos paquetes de idiomas completos (7 idiomas en total):**
  - Agregado soporte completo para **Português (Brasil)** ([`pt_br.json`](data/i18n/pt_br.json)), **Français** ([`fr.json`](data/i18n/fr.json)), **Italiano** ([`it.json`](data/i18n/it.json)), **Deutsch** ([`de.json`](data/i18n/de.json)) y **Русский** ([`ru.json`](data/i18n/ru.json)), sumándose a **Español** y **English**.
  - Cada paquete incluye el 100 % de los textos de la interfaz, tooltips de configuración, notificaciones y las 5 pestañas completas del **Manual de usuario**, con detección automática del idioma del sistema y selección manual inmediata desde *Configuración → General*.
- **Reinicio automático al actualizar desde el Gestor de actualizaciones de Linux Mint (`mintupdate` / `apt` / `dpkg`):**
  - Mklipper detecta ahora automáticamente cuando el binario instalado (`/usr/bin/mklipper`) es reemplazado por una actualización del sistema (verificación de dispositivo/inode vía `QFileSystemWatcher` con debounce de 2 s y respaldo ligero), guarda el historial de forma segura, muestra una notificación y reinicia limpiamente la instancia para que la nueva versión quede activa al instante sin tener que cerrarla manualmente. *(Activo para todas las actualizaciones a partir de v1.7.0).*

### 🇬🇧 English
- **5 new complete language packs (7 languages total):**
  - Added full support for **Português (Brasil)** ([`pt_br.json`](data/i18n/pt_br.json)), **Français** ([`fr.json`](data/i18n/fr.json)), **Italiano** ([`it.json`](data/i18n/it.json)), **Deutsch** ([`de.json`](data/i18n/de.json)), and **Русский** ([`ru.json`](data/i18n/ru.json)), alongside **Español** and **English**.
  - Every pack translates 100% of UI strings, settings tooltips, notifications, and all 5 tabs of the built-in **User Manual**, with automatic system locale detection and instant switching in *Settings → General*.
- **Automatic restart after Linux Mint Update Manager (`mintupdate` / `apt` / `dpkg`) upgrades:**
  - Mklipper now automatically detects when the installed binary (`/usr/bin/mklipper`) is replaced by a system package upgrade (inode/device watcher via `QFileSystemWatcher` with a 2 s debounce and lightweight fallback), safely saves history, notifies the user, and cleanly restarts itself so the newly installed version takes effect immediately without manual relaunch. *(Takes effect for all upgrades starting from v1.7.0).*

## v1.6.4

### 🇪🇸 Español
- **Compatibilidad universal con todo tipo de teclados (estándar y gamer, sin valores hardcodeados):**
  - Nueva capa dinámica X11 ([`x11keyboard.cpp`](src/x11keyboard.cpp)) que consulta en vivo `XGetModifierMapping` del servidor X11 para descubrir dinámicamente las máscaras y keycodes reales de `Shift`, `Ctrl`, `Alt`, `Super`/`Meta`/`Hyper`, `CapsLock`, `NumLock` y `ScrollLock`, eliminando cualquier máscara fija.
  - Soporte completo para teclas extendidas **F1–F35** (incluidas **F13–F24** usadas por teclas macro en teclados como Corsair K95), teclado numérico (`KP_*`), layouts no latinos y re-sincronización automática ante eventos `XCB_MAPPING_NOTIFY` (al conectar teclados, iniciar `ckb-next` o cambiar de distribución).
  - Nuevo capturador propio de atajos (`ShortcutRecorder`) con IME desactivado (evita que `ibus`/`fcitx` traguen modificadores), detección combinada de 3 capas (Qt + `nativeModifiers` X11 + `XQueryKeymap` en vivo), resolución de tecla base por `nativeScanCode()`, vista previa en tiempo real al mantener modificadores y **selector manual visual** (`Ctrl`/`Alt`/`Super`/`Shift` + tecla) para casos donde el sistema o el teclado interceptan la tecla durante la grabación.
- **Manual de usuario rediseñado en pestañas visuales y amigables:**
  - Reemplazada la vista plana de texto por **5 pestañas organizadas** (*Inicio*, *Tarjetas*, *Acciones*, *Atajos* y *Privacidad*) con tarjetas visuales, iconos flat con el color de acento y textos breves y claros.
- **Arquitectura de traducciones migrada a JSON dinámico:**
  - Todas las traducciones y las secciones del Manual de usuario se cargan ahora dinámicamente desde archivos JSON autocontenidos ([`data/i18n/es.json`](data/i18n/es.json) y [`data/i18n/en.json`](data/i18n/en.json)), dejando la base lista para incorporar nuevos idiomas sin modificar código C++.

### 🇬🇧 English
- **Universal keyboard & shortcut compatibility (standard and gaming keyboards, zero hardcoding):**
  - New dynamic X11 keyboard layer ([`x11keyboard.cpp`](src/x11keyboard.cpp)) querying live `XGetModifierMapping` to dynamically discover active masks and keycodes for `Shift`, `Ctrl`, `Alt`, `Super`/`Meta`/`Hyper`, `CapsLock`, `NumLock`, and `ScrollLock` with no hardcoded modifier masks.
  - Full support for extended **F1–F35** keys (including **F13–F24** macro keys on gaming keyboards such as Corsair K95), keypad keys (`KP_*`), non-Latin layouts, and automatic re-grabbing on `XCB_MAPPING_NOTIFY` (plugging in keyboards, starting `ckb-next`, or switching layouts).
  - New custom `ShortcutRecorder` widget with IME disabled (preventing `ibus`/`fcitx` from swallowing modifiers), 3-layer modifier detection (Qt + X11 `nativeModifiers` + live `XQueryKeymap`), physical level-0 key resolution via `nativeScanCode()`, live held-modifier preview, and a **manual visual key picker** (`Ctrl`/`Alt`/`Super`/`Shift` + key) whenever the OS or hardware intercepts a modifier during capture.
- **Redesigned friendly tabbed User Manual:**
  - Replaced the single wall-of-text view with **5 clean, bite-sized tabs** (*Start*, *Cards*, *Actions*, *Shortcuts*, and *Privacy*) featuring flat accent icon badges and concise visual cards.
- **Dynamic JSON internationalization architecture:**
  - All UI strings and User Manual sections are now dynamically discovered and loaded from self-contained JSON packs ([`data/i18n/es.json`](data/i18n/es.json) and [`data/i18n/en.json`](data/i18n/en.json)), making future community languages plug-and-play with zero C++ changes.

## v1.6.3

### 🇪🇸 Español
- **Opción para deshabilitar los atajos rápidos `1–0`:**
  - Nuevo ajuste en **Configuración → Atajos** (*«Usar teclas 1–0 para pegar rápido desde la ventana flotante»*): al desactivarlo, escribir números en la ventana flotante va directamente al buscador, se ocultan las insignias numéricas de las tarjetas y se actualiza la barra de ayuda inferior.
- **Tooltips explicativos en toda la configuración:**
  - Todas las opciones, selectores, botones y campos de **Configuración** (pestañas *General*, *Acciones* y *Atajos*) muestran ahora un tooltip estilizado al pasar el ratón explicando brevemente qué hace cada ajuste.
- **Manual de usuario integrado y en el repositorio:**
  - Nuevo **Manual de usuario** breve, claro y bilingüe accesible desde **Clic derecho en la bandeja → Manual de usuario…**, desde el botón **Manual de usuario** en **Configuración** y en [`MANUAL.md`](MANUAL.md).
- **Atajos globales ampliados y accesos de Acciones completos:**
  - Agregados 3 nuevos atajos globales útiles para Mklipper (con soporte D-Bus, CLI y sincronización automática con Cinnamon): **Alternar «Pegar sin formato»** (`--toggle-plain-text`), **Editar contenido actual** (`--edit`) y **Mostrar código QR actual** (`--qr`).
  - Conservados y restaurados todos los accesos funcionales de **Acciones** (botón *Ejecutar acciones en elemento actual* y casilla *Menú automático de acciones* en la bandeja, más el atajo *Invocar acciones del elemento actual* en Atajos).
  - El atajo global **Vaciar historial** ahora usa `--clear-interactive` para respetar la confirmación antes de borrar.
- **Soporte completo en inglés:**
  - Todas las nuevas opciones, atajos, notificaciones, el manual de usuario integrado y todos los tooltips de configuración cuentan con traducción completa al inglés.

### 🇬🇧 English
- **Option to disable quick-paste `1–0` shortcuts:**
  - New toggle in **Settings → Shortcuts** ("Use keys 1–0 to quick-paste from the popup window"): disabling it lets you type numbers directly into the search bar, hides the number badges on cards, and updates the bottom shortcut hint.
- **Helpful hover tooltips across all settings:**
  - Every checkbox, spinbox, dropdown, button, and shortcut field in **Settings** (*General*, *Actions*, and *Shortcuts* tabs) now displays a styled tooltip on hover briefly explaining what it does.
- **Built-in & repository User Manual:**
  - New concise, easy-to-read bilingual **User Manual** accessible from **Tray Menu → User Manual…**, from the **User Manual** button in **Settings**, and in [`MANUAL.md`](MANUAL.md).
- **Expanded global shortcuts & complete Actions access:**
  - Added 3 practical global shortcuts for Mklipper (with full D-Bus, CLI, and Cinnamon sync support): **Toggle "Paste without formatting"** (`--toggle-plain-text`), **Edit current contents** (`--edit`), and **Show current QR code** (`--qr`).
  - Restored and kept all working **Actions** shortcuts and tray controls (*Run actions on current item*, *Automatic actions menu* toggle, and *Invoke actions for current item* shortcut).
  - The **Clear history** global shortcut now uses `--clear-interactive` so confirmation prompts are respected.
- **Full English translation:**
  - Every new option, shortcut, notification, the built-in User Manual, and all settings tooltips are fully translated into English.

## v1.6.2

### 🇪🇸 Español
- **Rediseño minimalista y flat inspirado en Cinnamon 6.6:**
  - Nuevo motor propio de iconos vectoriales flat monocromáticos (estilo simbólico de Cinnamon 6.6 / Adwaita) en toda la aplicación: búsqueda, fijar, papelera, ajustes, cerrar, editar, código QR, enlaces, diálogo de configuración, actualizador y menú de bandeja.
  - Botones minimalistas sin bordes pesados, barra superior limpia, eliminación del recuadro anidado alrededor del historial, jerarquía visual por capas tonales, pestañas tipo *pill*, checkboxes/spinboxes planos y botones primarios con el color de acento del sistema.
  - Icono de aplicación modernizado y completamente flat cuyo clip adopta dinámicamente el color de acento activo del usuario.
- **Integración nativa con el tema, color de acento y puntero de Cinnamon:**
  - Lectura directa del tema GTK3/Cinnamon activo (`org.cinnamon.desktop.interface gtk-theme`) y sus `@define-color` reales (fondo, superficies, texto y color de acento), resolviendo el fallo de `qt6-gtk-platformtheme` que caía en el azul genérico de Qt Fusion.
  - **Actualización en vivo sin reiniciar:** al cambiar el tema o el color de acento en **Configuración → Temas** de Cinnamon, Mklipper actualiza al instante el popup, los diálogos abiertos, el menú y el icono de la bandeja.
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
  - Ventana flotante refinada con barra de búsqueda y botones redondeados alineados, contenedor enmarcado del historial, barra de desplazamiento minimalista, insignia de estado para *«Pegar sin formato»* y tarjetas del historial con mejores márgenes, miniaturas de imágenes con bordes redondeados y cápsula flotante translúcida para los botones de acción al pasar el ratón.
  - Estado vacío mejorado con icono, título y texto guía centrado manteniendo exactamente la misma estructura y alineación del popup.
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
  - Diseño visual limpio y sutil: muestra una pequeña línea discreta al pie de cada tarjeta (`Firefox · github.com`, `VS Code · main.cpp`, `Terminal`), tooltip completo al pasar el ratón y botón rápido `↗` solo al pasar el cursor si hay un enlace web de origen.
  - También puedes buscar en el historial por nombre de aplicación, título de ventana o dominio.
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
  - Conserva el formato original en el historial SQLite por si desactivas el interruptor más adelante.

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
  - Permite mantener la ventana abierta mientras haces clic en varios elementos seguidos (incluso con pegado automático) y cerrarla tanto con la cruz (`✕`) / `Esc` como haciendo clic fuera de ella.
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
