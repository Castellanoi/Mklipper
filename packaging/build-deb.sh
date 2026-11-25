#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build"
DIST_DIR="${ROOT_DIR}/dist"
STAGE_DIR="${BUILD_DIR}/deb-stage"

PKG_NAME="mklipper"
PKG_VERSION="$(sed -n 's/^project(mklipper VERSION \([0-9][^ ]*\).*/\1/p' "${ROOT_DIR}/CMakeLists.txt" | head -n 1)"
if [ -z "${PKG_VERSION}" ]; then
    echo "Error: no se pudo leer la versión desde CMakeLists.txt" >&2
    exit 1
fi
PKG_ARCH="$(dpkg --print-architecture)"
DEB_FILE="${DIST_DIR}/${PKG_NAME}_${PKG_VERSION}_${PKG_ARCH}.deb"

cmake -S "${ROOT_DIR}" -B "${BUILD_DIR}" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
cmake --build "${BUILD_DIR}" -j"$(nproc)"

rm -rf "${STAGE_DIR}"
mkdir -p "${STAGE_DIR}/DEBIAN" "${DIST_DIR}"

DESTDIR="${STAGE_DIR}" cmake --install "${BUILD_DIR}"
strip --strip-unneeded "${STAGE_DIR}/usr/bin/mklipper"

echo "/etc/xdg/autostart/mklipper.desktop" > "${STAGE_DIR}/DEBIAN/conffiles"

INSTALLED_SIZE="$(du -sk "${STAGE_DIR}/usr" "${STAGE_DIR}/etc" | awk '{s+=$1} END {print s}')"

cat > "${STAGE_DIR}/DEBIAN/control" <<EOF
Package: ${PKG_NAME}
Version: ${PKG_VERSION}
Section: utils
Priority: optional
Architecture: ${PKG_ARCH}
Maintainer: Ivan Castellano <castellano.ivan.93@gmail.com>
Installed-Size: ${INSTALLED_SIZE}
Depends: libqt6core6t64 (>= 6.4), libqt6gui6t64 (>= 6.4), libqt6widgets6t64 (>= 6.4), libqt6dbus6t64 (>= 6.4), libqt6network6t64 (>= 6.4), libqt6sql6t64 (>= 6.4), libqt6sql6-sqlite (>= 6.4), libqt6svg6 (>= 6.4), qt6-gtk-platformtheme (>= 6.4), libqrencode4, libx11-6, libxtst6, pkexec
Description: Native KDE Klipper clipboard manager port for Linux Mint
 Mklipper is a clean, lightweight, native Qt 6 port of KDE Plasma's Klipper
 clipboard manager designed for Linux Mint (Cinnamon / X11) without KDE or
 Plasma runtime dependencies.
 .
 Features include persistent SQLite history (text, images, URLs), pinned items,
 instant search popup, system tray icon, inline editing, QR code generation,
 password manager secret filtering, regex actions, D-Bus API, and global
 X11 shortcuts.
EOF

cat > "${STAGE_DIR}/DEBIAN/postinst" <<'EOF'
#!/bin/sh
set -e
if [ -x /usr/bin/gtk-update-icon-cache ] && [ -d /usr/share/icons/hicolor ]; then
    gtk-update-icon-cache -q -t -f /usr/share/icons/hicolor || true
fi
if [ -x /usr/bin/update-desktop-database ]; then
    update-desktop-database -q /usr/share/applications || true
fi
exit 0
EOF
chmod 0755 "${STAGE_DIR}/DEBIAN/postinst"

cat > "${STAGE_DIR}/DEBIAN/postrm" <<'EOF'
#!/bin/sh
set -e
if [ -x /usr/bin/gtk-update-icon-cache ] && [ -d /usr/share/icons/hicolor ]; then
    gtk-update-icon-cache -q -t -f /usr/share/icons/hicolor || true
fi
if [ -x /usr/bin/update-desktop-database ]; then
    update-desktop-database -q /usr/share/applications || true
fi
exit 0
EOF
chmod 0755 "${STAGE_DIR}/DEBIAN/postrm"

dpkg-deb --root-owner-group --build "${STAGE_DIR}" "${DEB_FILE}"
(cd "${DIST_DIR}" && sha256sum "$(basename "${DEB_FILE}")" > SHA256SUMS)
echo "Paquete generado: ${DEB_FILE}"
echo "Checksum generado: ${DIST_DIR}/SHA256SUMS"
