#!/usr/bin/env bash
set -euo pipefail

INPUT_DIR="${1:-dist/openwrt}"
OUTPUT_DIR="${2:-dist/openwrt-repo}"

require_tool() {
  if ! command -v "$1" >/dev/null 2>&1; then
    echo "Required tool '$1' is not installed" >&2
    exit 1
  fi
}

require_tool ar
require_tool awk
require_tool gzip
require_tool sha256sum
require_tool tar

CONTROL_TMP="$(mktemp -d)"
trap 'rm -rf "${CONTROL_TMP}"' EXIT

control_member_name() {
  awk '/^(\.\/)?control\.tar\.(gz|xz|zst)$/ { print; exit }'
}

extract_from_ar() {
  local package_file="$1"
  local control_archive="$2"

  ar p "${package_file}" "${control_archive}"
}

extract_from_tar() {
  local package_file="$1"
  local control_archive="$2"

  tar -xOf "${package_file}" "${control_archive}"
}

extract_control_file() {
  local control_archive="$1"

  rm -rf "${CONTROL_TMP:?}"/*
  case "${control_archive}" in
    control.tar.gz|./control.tar.gz)
      tar -xzf - -C "${CONTROL_TMP}"
      ;;
    control.tar.xz|./control.tar.xz)
      tar -xJf - -C "${CONTROL_TMP}"
      ;;
    control.tar.zst|./control.tar.zst)
      if command -v zstd >/dev/null 2>&1; then
        zstd -dc | tar -xf - -C "${CONTROL_TMP}"
      elif tar --help 2>/dev/null | grep -q -- --zstd; then
        tar --zstd -xf - -C "${CONTROL_TMP}"
      else
        echo "Extracting zstd-compressed package metadata requires zstd or tar --zstd support" >&2
        exit 1
      fi
      ;;
  esac
}

extract_control() {
  local package_file="$1"
  local control_archive=""

  if control_archive="$(ar t "${package_file}" 2>/dev/null | control_member_name)" && [[ -n "${control_archive}" ]]; then
    extract_from_ar "${package_file}" "${control_archive}" | extract_control_file "${control_archive}"
  elif control_archive="$(tar -tf "${package_file}" 2>/dev/null | control_member_name)" && [[ -n "${control_archive}" ]]; then
    extract_from_tar "${package_file}" "${control_archive}" | extract_control_file "${control_archive}"
  else
    echo "Unable to find control archive in ${package_file}" >&2
    return 1
  fi

  cat "${CONTROL_TMP}/control"
}

rm -rf "${OUTPUT_DIR}"
mkdir -p "${OUTPUT_DIR}"

mapfile -t packages < <(find "${INPUT_DIR}" -type f -name '*.ipk' | sort)
if (( ${#packages[@]} == 0 )); then
  echo "No .ipk packages found under ${INPUT_DIR}" >&2
  exit 1
fi

for package_file in "${packages[@]}"; do
  control="$(extract_control "${package_file}")"
  arch="$(awk -F': ' '$1 == "Architecture" { print $2; exit }' <<<"${control}")"
  if [[ -z "${arch}" ]]; then
    echo "Unable to determine architecture for ${package_file}" >&2
    exit 1
  fi

  arch_dir="${OUTPUT_DIR}/${arch}"
  mkdir -p "${arch_dir}"
  cp "${package_file}" "${arch_dir}/"
  filename="$(basename "${package_file}")"
  size="$(stat -c '%s' "${package_file}")"
  sha256="$(sha256sum "${package_file}" | awk '{print $1}')"

  {
    printf '%s\n' "${control}"
    printf 'Filename: %s\n' "${filename}"
    printf 'Size: %s\n' "${size}"
    printf 'SHA256sum: %s\n\n' "${sha256}"
  } >> "${arch_dir}/Packages"
done

while IFS= read -r -d '' packages_file; do
  gzip -9c "${packages_file}" > "${packages_file}.gz"
done < <(find "${OUTPUT_DIR}" -type f -name Packages -print0)

echo "OpenWrt OPKG repository written to ${OUTPUT_DIR}:"
find "${OUTPUT_DIR}" -type f | sort
