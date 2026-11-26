#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -lt 2 ]; then
    echo "Uso: $0 <directorio-salida-apt> <paquete1.deb> [paquete2.deb ...]" >&2
    exit 1
fi

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT_DIR="$(mkdir -p "$1" && cd "$1" && pwd)"
shift

KEYRING_SRC="${ROOT_DIR}/packaging/mklipper-archive-keyring.gpg"
if [ ! -f "${KEYRING_SRC}" ]; then
    echo "Error: no se encontró la clave pública APT en ${KEYRING_SRC}" >&2
    exit 1
fi

GPG_KEY_ID="${APT_GPG_KEY_ID:-Mklipper APT Repository <castellano.ivan.93@gmail.com>}"

mkdir -p "${OUT_DIR}/pool/main/m/mklipper"

for deb in "$@"; do
    if [ ! -f "${deb}" ]; then
        echo "Error: no existe el archivo ${deb}" >&2
        exit 1
    fi
    pkg_name="$(dpkg-deb --field "${deb}" Package)"
    if [ "${pkg_name}" != "mklipper" ]; then
        echo "Error: ${deb} no pertenece al paquete mklipper (Package=${pkg_name})" >&2
        exit 1
    fi
    cp -f "${deb}" "${OUT_DIR}/pool/main/m/mklipper/"
done

# Collect unique architectures present in pool
ARCHES=()
while IFS= read -r arch; do
    [ -n "${arch}" ] && ARCHES+=("${arch}")
done < <(
    for f in "${OUT_DIR}/pool/main/m/mklipper/"*.deb; do
        dpkg-deb --field "${f}" Architecture
    done | sort -u
)

if [ "${#ARCHES[@]}" -eq 0 ]; then
    echo "Error: no se encontraron paquetes .deb en ${OUT_DIR}/pool/main/m/mklipper" >&2
    exit 1
fi

ARCH_LIST="${ARCHES[*]}"

rm -rf "${OUT_DIR}/dists"
for arch in "${ARCHES[@]}"; do
    BIN_DIR="${OUT_DIR}/dists/stable/main/binary-${arch}"
    mkdir -p "${BIN_DIR}"
    (
        cd "${OUT_DIR}"
        dpkg-scanpackages --multiversion --arch "${arch}" pool/main > "${BIN_DIR}/Packages"
    )
    gzip -9nc "${BIN_DIR}/Packages" > "${BIN_DIR}/Packages.gz"
done

RELEASE_FILE="${OUT_DIR}/dists/stable/Release"
apt-ftparchive \
    -o "APT::FTPArchive::Release::Origin=Mklipper" \
    -o "APT::FTPArchive::Release::Label=Mklipper" \
    -o "APT::FTPArchive::Release::Suite=stable" \
    -o "APT::FTPArchive::Release::Codename=stable" \
    -o "APT::FTPArchive::Release::Architectures=${ARCH_LIST}" \
    -o "APT::FTPArchive::Release::Components=main" \
    -o "APT::FTPArchive::Release::Description=Mklipper APT repository for Linux Mint and Ubuntu" \
    release "${OUT_DIR}/dists/stable" > "${RELEASE_FILE}"

GPG_ARGS=(--batch --yes --digest-algo SHA512 --default-key "${GPG_KEY_ID}")
if [ -n "${APT_GPG_PASSPHRASE:-}" ]; then
    printf '%s' "${APT_GPG_PASSPHRASE}" | gpg "${GPG_ARGS[@]}" --pinentry-mode loopback --passphrase-fd 0 \
        --clearsign -o "${OUT_DIR}/dists/stable/InRelease" "${RELEASE_FILE}"
    printf '%s' "${APT_GPG_PASSPHRASE}" | gpg "${GPG_ARGS[@]}" --pinentry-mode loopback --passphrase-fd 0 \
        --armor --detach-sign -o "${OUT_DIR}/dists/stable/Release.gpg" "${RELEASE_FILE}"
else
    gpg "${GPG_ARGS[@]}" --clearsign -o "${OUT_DIR}/dists/stable/InRelease" "${RELEASE_FILE}"
    gpg "${GPG_ARGS[@]}" --armor --detach-sign -o "${OUT_DIR}/dists/stable/Release.gpg" "${RELEASE_FILE}"
fi

cp -f "${KEYRING_SRC}" "${OUT_DIR}/mklipper-archive-keyring.gpg"
chmod 0644 "${OUT_DIR}/mklipper-archive-keyring.gpg"

cat > "${OUT_DIR}/mklipper.sources" <<EOF
X-Repolib-Name: Mklipper
Types: deb
URIs: https://raw.githubusercontent.com/Castellanoi/Mklipper/apt-repo
Suites: stable
Components: main
Architectures: ${ARCH_LIST}
Signed-By: /usr/share/keyrings/mklipper-archive-keyring.gpg
EOF

# Verify generated APT signature against the public keyring
if command -v gpgv >/dev/null 2>&1; then
    gpgv --keyring "${OUT_DIR}/mklipper-archive-keyring.gpg" \
        "${OUT_DIR}/dists/stable/Release.gpg" "${RELEASE_FILE}"
    gpgv --keyring "${OUT_DIR}/mklipper-archive-keyring.gpg" \
        "${OUT_DIR}/dists/stable/InRelease"
fi

echo "Repositorio APT firmado generado correctamente en: ${OUT_DIR}"
