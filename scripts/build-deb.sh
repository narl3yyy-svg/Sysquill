#!/usr/bin/env bash
# Build Sysquill and package it as build/sysquill_<version>_amd64.deb
#
#   ./scripts/build-deb.sh                 build only
#   ./scripts/build-deb.sh --install-deps  install the build tools first (asks for your password)
set -euo pipefail
cd "$(dirname "$0")/.."

if [[ "${1:-}" == "--install-deps" ]]; then
    sudo apt-get update
    sudo apt-get install -y \
        build-essential cmake dpkg-dev file \
        qt6-base-dev qt6-base-dev-tools \
        qt6-declarative-dev qt6-declarative-dev-tools \
        qml6-module-qtquick qml6-module-qtquick-controls qml6-module-qtquick-layouts \
        qml6-module-qtquick-templates qml6-module-qtquick-window qml6-module-qtqml-workerscript
fi

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr -Wno-dev
cmake --build build -j"$(nproc)"
(cd build && cpack -G DEB)

deb=$(ls -t build/sysquill_*.deb | head -1)
echo
echo "Built: $deb"
echo "Install it with:   sudo apt install ./$deb"
