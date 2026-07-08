#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# macOS build environment setup for Blender dependencies
# Combines: Homebrew, Xcode (with Metal Toolchain), CMake, brew packages
set -euo pipefail

if [ "$(uname -m)" != "arm64" ]; then
  echo "Only ARM64 is supported"
  exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Versions and packages
XCODE_VERSION="26.1.1" # This version should be aligned with the Xcode version configured in build_files/config/pipeline_config.yaml
CMAKE_VERSION="3.31.6" # Last CMake 3.x published to Homebrew
BREW_PACKAGES=(
  autoconf
  automake
  bison
  dos2unix
  flex
  libtool
  meson
  ninja
  pkg-config
  yasm
)

# XIP location resolve order: env/flag > script dir
XCODE_XIP_DIR="${XCODE_XIP_DIR:-${SCRIPT_DIR}}"
ASSUME_YES="${ASSUME_YES:-0}"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --xcode-xip-dir)
      XCODE_XIP_DIR="$2"
      shift 2
      ;;
    -y|--yes)
      ASSUME_YES=1
      shift
      ;;
    *)
      echo "Unknown arg: $1"
      exit 1
      ;;
  esac
done

# Warning prompt
if [[ "${ASSUME_YES}" != "1" ]]; then
  cat <<EOF
############################################################
WARNING
This script will install software on your system:
  - Xcode ${XCODE_VERSION} (with Metal Toolchain)
  - Homebrew (+ shellenv in ~/.zprofile)
  - CMake ${CMAKE_VERSION}
  - Brew packages: ${BREW_PACKAGES[*]}
############################################################
EOF
  read -r -p "Continue? [y/N] " CONFIRM
  [[ "${CONFIRM}" =~ ^[Yy]$ ]] || { echo "Aborted."; exit 1; }
fi

# Homebrew
install_homebrew() {
  if command -v brew >/dev/null 2>&1; then
    echo "[brew]:  Homebrew already installed"
    return 0
  fi
  echo "[brew]:  Installing Homebrew..."
  sudo -v
  NONINTERACTIVE=1 /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
  local SHELLENV_LINE='eval "$(/opt/homebrew/bin/brew shellenv zsh)"'
  if grep -qxF "${SHELLENV_LINE}" "${HOME}/.zprofile" 2>/dev/null; then
    echo "[brew]:  .zprofile already set"
  else
    echo "[brew]:  Adding shellenv to .zprofile"
    echo >> "${HOME}/.zprofile"
    echo "${SHELLENV_LINE}" >> "${HOME}/.zprofile"
  fi
  eval "${SHELLENV_LINE}"
  echo "[brew]:  Homebrew installed"
}

# Xcode
install_xcode() {
  local VERSION="${1:?Usage: install_xcode <version>}"
  local XIP="${XCODE_XIP_DIR}/Xcode_${VERSION}.xip"
  local APP="/Applications/Xcode-${VERSION}.app"
  local MAJOR="${VERSION%%.*}"
  local DL_URL="https://developer.apple.com/services-account/download?path=/Developer_Tools/Xcode_${MAJOR}/Xcode_${MAJOR}.xip"

  if [ ! -d "${APP}" ]; then
    if [ ! -f "${XIP}" ]; then
      echo "[xcode]: Missing ${XIP}"
      echo "[xcode]: Download URL (Apple account needed):"
      echo "[xcode]:  ${DL_URL}"
      echo "[xcode]: Save file here: ${XIP}"
      echo "[xcode]: Waiting for file..."
      until [ -f "${XIP}" ]; do
        sleep 5
      done
      echo "[xcode]: Found file. Continuing."
    fi

    echo "[xcode]: Extracting xip..."
    rm -rf /tmp/Xcode.app
    cd /tmp && xip -x "${XIP}"

    echo "[xcode]: Moving to ${APP}"
    sudo mv /tmp/Xcode.app "${APP}"
    rm -f "${XIP}"
    echo "[xcode]: Xcode ${VERSION} installed"
  else
    echo "[xcode]: Xcode ${VERSION} already installed"
  fi

  echo "[xcode]: Selecting Xcode ${VERSION}"
  sudo xcode-select -s "${APP}/Contents/Developer"
  sudo xcodebuild -license accept
  echo "[xcode]: Running first launch"
  sudo xcodebuild -runFirstLaunch

  echo "[xcode]: Ensure MetalToolchain"
  for i in 1 2 3; do
    sudo xcodebuild -downloadComponent MetalToolchain && break || true
    echo "[xcode]: Download failed. Retrying.. ($i)"
    sleep 10
  done
}

# CMake
install_cmake() {
  local VERSION="${1:?Usage: install_cmake <version>}"

  if command -v cmake >/dev/null 2>&1 && cmake --version | grep -q "${VERSION}"; then
    echo "[cmake]: CMake ${VERSION} already here"
    return 0
  fi

  echo "[cmake]: Setup local tap"
  brew tap-new "${USER}/local-tap" 2>/dev/null || true
  brew tap homebrew/core --force

  echo "[cmake]: Extract CMake ${VERSION} into local tap"
  brew extract --version="${VERSION}" cmake "${USER}/local-tap"

  if brew list --formula | grep -qx "cmake"; then
    echo "[cmake]: Unlink existing cmake"
    brew unlink cmake
  fi

  echo "[cmake]: Install CMake ${VERSION}"
  brew install "${USER}/local-tap/cmake@${VERSION}"
  brew link "${USER}/local-tap/cmake@${VERSION}"

  echo "[cmake]: CMake ${VERSION} installed"
}

install_brew_packages() {
  echo "[brew]:  Installing:"
  printf '  - %s\n' "${BREW_PACKAGES[@]}"
  brew install -y "${BREW_PACKAGES[@]}"

  echo "[brew]:  Packages installed"
}

install_xcode "${XCODE_VERSION}"
install_homebrew
install_cmake "${CMAKE_VERSION}"
install_brew_packages
