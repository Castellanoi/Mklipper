# Manual de usuario de Mklipper / Mklipper User Manual

- [🇪🇸 Español](#español)
- [🇬🇧 English](#english)

---

## Español

Guía rápida para aprovechar al máximo **Mklipper**, tu gestor de portapapeles nativo para Linux Mint (Cinnamon).

### 1. Primeros pasos
- **Abrir el historial:** presiona **`Super + V`** (personalizable) o haz **clic izquierdo** en el icono de Mklipper en la bandeja del sistema.
- **Pegar un elemento:** haz clic sobre cualquier tarjeta, presiona **`Enter`** en la fila seleccionada o usa las teclas **`1` a `0`** para pegar los primeros 10 elementos al instante.
- **Menú rápido (clic derecho en la bandeja):** permite abrir el historial, ejecutar acciones en el elemento actual, editar el contenido actual, generar un código QR, activar o desactivar el menú automático de acciones, vaciar el historial, abrir **Configuración**, abrir este **Manual de usuario** o buscar actualizaciones.

### 2. Buscar, fijar y organizar
- **Búsqueda instantánea:** apenas abras la ventana flotante, puedes escribir para filtrar por texto, aplicación de origen, título de ventana o enlace.
- **Escribir números directamente en el buscador:** si prefieres buscar números en lugar de usar el pegado rápido, desactiva **Usar teclas 1–0 para pegar rápido desde la ventana flotante** en **Configuración → Atajos**.
- **Fijar favoritos:** pasa el ratón sobre una tarjeta y haz clic en la **estrella** para fijarla arriba y protegerla al vaciar el historial. El botón de **estrella** en la barra superior filtra para ver solo los elementos fijados.

### 3. Botones de cada tarjeta
Al pasar el ratón o seleccionar un elemento aparecen sus botones rápidos a la derecha:
- **Estrella:** fija o desfija el elemento.
- **Enlace externo:** abre nuevamente la página web desde donde copiaste ese contenido (cuando está disponible).
- **Rayo (Acciones):** abre las acciones disponibles para ese elemento (abrir URL/archivo o ejecutar una regla personalizada).
- **Código QR:** genera un código QR escaneable (puedes copiarlo o guardarlo como imagen PNG).
- **Lápiz (Editar):** edita el texto copiado antes de pegarlo (**`Ctrl + Enter`** guarda).
- **Papelera (Eliminar):** borra el elemento de forma definitiva (también con la tecla **`Supr`**).

### 4. Pegar sin formato
- Activa el interruptor **Pegar sin formato** en la parte superior de la ventana emergente (o asígnale un atajo global) para quitar fuentes, colores y estilos HTML al pegar texto.
- No afecta a imágenes ni archivos copiados.

### 5. Acciones automáticas y personalizadas
- **Enlaces y archivos:** al copiar una dirección web (`https://...`) o la ruta de un archivo existente en tu equipo, Mklipper ofrece abrirlo con la aplicación predeterminada desde el botón de rayo o con un menú automático (activable desde el menú de la bandeja o en **Configuración → Acciones**).
- **Reglas por expresión regular (*Configuración → Acciones*):** puedes crear reglas avanzadas que ejecutan un comando usando `%s` como el texto copiado y, opcionalmente, reemplazan o agregan la salida del comando al portapapeles.

### 6. Atajos de teclado, estética y privacidad
- **Atajos globales (*Configuración → Atajos*):** configura combinaciones para abrir el historial, alternar *Pegar sin formato*, editar el contenido actual, mostrar el código QR, invocar acciones, rotar el historial o vaciarlo. Se sincronizan automáticamente con los atajos de teclado de Cinnamon.
- **Tema y puntero nativos:** Mklipper adopta automáticamente el tema claro/oscuro, el color de acento (incluso en vivo sin reiniciar) y el tamaño del puntero configurados en Cinnamon.
- **Protección de contraseñas y borrado limpio:** los secretos copiados desde gestores de contraseñas compatibles (KeePassXC, Bitwarden, 1Password, etc.) no se guardan en el historial, y cada elemento eliminado se sobrescribe y libera del disco al instante sin dejar residuos.

### 7. Actualizaciones
- Mklipper se actualiza automáticamente desde el **Gestor de actualizaciones de Linux Mint** junto con los paquetes del sistema, o cuando quieras desde **Clic derecho en la bandeja → Buscar actualizaciones…**.

---

## English

Quick guide to getting the most out of **Mklipper**, your native clipboard manager for Linux Mint (Cinnamon).

### 1. Getting Started
- **Open the clipboard history:** press **`Super + V`** (customizable) or **left-click** the Mklipper icon in the system tray.
- **Paste an item:** click any card, press **`Enter`** on the selected row, or press keys **`1`–`0`** to paste the first 10 items immediately.
- **Tray menu (right-click):** quick access to open history, run actions on the current item, edit current contents, show a QR code, toggle the automatic actions menu, clear history, open **Settings**, open this **User Manual**, or check for updates.

### 2. Search, Pin & Organize
- **Instant search:** start typing right after opening the popup to filter by text, source app, window title, or URL.
- **Typing numbers directly:** if you prefer typing numbers straight into the search box instead of quick-pasting, disable **Use keys 1–0 to quick-paste from the popup window** in **Settings → Shortcuts**.
- **Pin favorites:** hover any card and click the **star** icon to keep it at the top and protect it when clearing history. Click the **star button** in the top bar to show pinned items only.

### 3. Card Action Buttons
Hovering or selecting any entry reveals quick action buttons on the right:
- **Star:** pin or unpin the item.
- **External link:** reopen the webpage where the text was copied from (when available).
- **Lightning bolt (Actions):** open matching actions for that item (open URL/file or run a custom rule).
- **QR Code:** generate a scannable QR code (copy to clipboard or save as PNG).
- **Pencil (Edit):** edit copied text inline before pasting (**`Ctrl + Enter`** saves).
- **Trash (Delete):** permanently remove that item (also works with the **`Del`** key).

### 4. Paste Without Formatting
- Enable the **Paste without formatting** toggle at the top of the popup (or bind a global shortcut) to strip HTML fonts, colors, and styles when pasting text.
- Copied images and files are never affected.

### 5. Automatic & Custom Actions
- **Links and local files:** when you copy a web URL (`https://...`) or an existing local file path, Mklipper can offer to open it with your default app via the lightning-bolt button or an automatic popup menu (toggleable from the tray menu or **Settings → Actions**).
- **Custom Regex Rules (*Settings → Actions*):** power users can define regular-expression rules that run a command using `%s` for the copied text—optionally replacing or appending the command output back into the clipboard.

### 6. Shortcuts, Native Look & Privacy
- **Global shortcuts (*Settings → Shortcuts*):** bind shortcuts to open history, toggle plain-text paste, edit current contents, show a QR code, invoke actions, cycle history, or clear history. They sync automatically with Cinnamon's Keyboard settings.
- **Native theme & cursor:** Mklipper automatically follows your Cinnamon light/dark theme, accent color (live without restarting), and configured cursor size.
- **Password protection & clean deletion:** secrets from compatible password managers (KeePassXC, Bitwarden, 1Password, etc.) are never saved to history, and deleted items are immediately zeroed and reclaimed from disk with no residual files left behind.

### 7. Updates
- Mklipper updates automatically through **Linux Mint Update Manager** alongside system packages, or anytime via **Right-click tray icon → Check for updates…**.
