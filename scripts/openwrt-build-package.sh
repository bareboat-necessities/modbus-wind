#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OPENWRT_VERSION="${OPENWRT_VERSION:-24.10.6}"
OPENWRT_TARGET="${OPENWRT_TARGET:-x86/64}"
OPENWRT_JOBS="${OPENWRT_JOBS:-$(nproc 2>/dev/null || echo 2)}"
OPENWRT_WORK_DIR="${OPENWRT_WORK_DIR:-${REPO_ROOT}/.openwrt-work}"
PKG_NAME="${OPENWRT_PACKAGE_NAME:-sen0658-pc}"
PKG_VERSION="${OPENWRT_PACKAGE_VERSION:-$(git -C "${REPO_ROOT}" describe --tags --match 'v[0-9]*' --abbrev=0 2>/dev/null | sed 's/^v//' || awk '/^[[:space:]]*VERSION[[:space:]]+/ { print $2; exit }' "${REPO_ROOT}/CMakeLists.txt")}"
PKG_RELEASE="${OPENWRT_PACKAGE_RELEASE:-1}"
BASE_URL="https://downloads.openwrt.org/releases/${OPENWRT_VERSION}/targets/${OPENWRT_TARGET}"
TARGET_SAFE="${OPENWRT_TARGET//\//-}"
WORK_DIR="${OPENWRT_WORK_DIR}/${OPENWRT_VERSION}/${TARGET_SAFE}"
SDK_ARCHIVE="${WORK_DIR}/sdk.tar"
SDK_ROOT="${WORK_DIR}/sdk"
DIST_DIR="${REPO_ROOT}/dist/openwrt/${OPENWRT_VERSION}/${TARGET_SAFE}"

if [[ -z "${PKG_VERSION}" ]]; then
  echo "Unable to determine package version" >&2
  exit 1
fi

require_tool() {
  if ! command -v "$1" >/dev/null 2>&1; then
    echo "Required tool '$1' is not installed" >&2
    exit 1
  fi
}

require_tool curl
require_tool python3
require_tool tar
require_tool gzip
require_tool make

rm -rf "${WORK_DIR}" "${DIST_DIR}"
mkdir -p "${WORK_DIR}" "${SDK_ROOT}" "${DIST_DIR}"

SDK_URL="$({
  OPENWRT_INDEX_URL="${BASE_URL}/" python3 - <<'PY'
import html.parser
import os
import sys
import urllib.request

index_url = os.environ["OPENWRT_INDEX_URL"]

class LinkParser(html.parser.HTMLParser):
    def __init__(self):
        super().__init__()
        self.links = []
    def handle_starttag(self, tag, attrs):
        if tag != "a":
            return
        for key, value in attrs:
            if key == "href" and value:
                self.links.append(value)

parser = LinkParser()
with urllib.request.urlopen(index_url, timeout=60) as response:
    parser.feed(response.read().decode("utf-8", errors="replace"))

matches = [
    link for link in parser.links
    if link.startswith("openwrt-sdk-")
    and ".Linux-x86_64" in link
    and (link.endswith(".tar.xz") or link.endswith(".tar.zst"))
]
if not matches:
    sys.exit(f"No Linux x86_64 SDK archive found at {index_url}")
print(index_url + matches[0])
PY
} )"

ARCHIVE_NAME="${SDK_URL##*/}"
echo "Downloading OpenWrt SDK: ${SDK_URL}"
curl -fL "${SDK_URL}" -o "${WORK_DIR}/${ARCHIVE_NAME}"

case "${ARCHIVE_NAME}" in
  *.tar.xz)
    tar -xJf "${WORK_DIR}/${ARCHIVE_NAME}" -C "${SDK_ROOT}" --strip-components=1
    ;;
  *.tar.zst)
    if command -v zstd >/dev/null 2>&1; then
      zstd -dc "${WORK_DIR}/${ARCHIVE_NAME}" | tar -xf - -C "${SDK_ROOT}" --strip-components=1
    elif tar --help 2>/dev/null | grep -q -- --zstd; then
      tar --zstd -xf "${WORK_DIR}/${ARCHIVE_NAME}" -C "${SDK_ROOT}" --strip-components=1
    else
      echo "Extracting .tar.zst SDK archives requires zstd or tar --zstd support" >&2
      exit 1
    fi
    ;;
  *)
    echo "Unsupported SDK archive extension: ${ARCHIVE_NAME}" >&2
    exit 1
    ;;
esac

mkdir -p "${SDK_ROOT}/package/${PKG_NAME}" "${SDK_ROOT}/dl"
cp "${REPO_ROOT}/package/openwrt/${PKG_NAME}/Makefile" "${SDK_ROOT}/package/${PKG_NAME}/Makefile"

SOURCE_ARCHIVE="${SDK_ROOT}/dl/${PKG_NAME}-${PKG_VERSION}.tar.gz"
echo "Creating source archive: ${SOURCE_ARCHIVE}"
tar -czf "${SOURCE_ARCHIVE}" \
  --exclude='.git' \
  --exclude='build' \
  --exclude='Build' \
  --exclude='build-*' \
  --exclude='dist' \
  --exclude='.openwrt-work' \
  --exclude='.openwrt-*' \
  --exclude='*.tmp' \
  --transform "s#^#${PKG_NAME}-${PKG_VERSION}/#" \
  -C "${REPO_ROOT}" .

SOURCE_HASH="$(sha256sum "${SOURCE_ARCHIVE}" | awk '{print $1}')"

echo "Building ${PKG_NAME} ${PKG_VERSION}-${PKG_RELEASE} for OpenWrt ${OPENWRT_VERSION} ${OPENWRT_TARGET}"
make -C "${SDK_ROOT}" defconfig
make -C "${SDK_ROOT}" \
  "package/${PKG_NAME}/compile" \
  -j"${OPENWRT_JOBS}" \
  V=s \
  PKG_VERSION="${PKG_VERSION}" \
  PKG_RELEASE="${PKG_RELEASE}" \
  PKG_HASH="${SOURCE_HASH}"

find "${SDK_ROOT}/bin/packages" -type f \( -name "${PKG_NAME}_*.ipk" -o -name "${PKG_NAME}_*.apk" \) -print -exec cp {} "${DIST_DIR}/" \;

if ! find "${DIST_DIR}" -type f \( -name '*.ipk' -o -name '*.apk' \) | grep -q .; then
  echo "No OpenWrt package was produced" >&2
  exit 1
fi

echo "OpenWrt packages copied to ${DIST_DIR}:"
find "${DIST_DIR}" -type f -maxdepth 1 -print
