#!/usr/bin/env bash

set -euo pipefail

butter_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
butter_build_dir="${BUTTER_BUILD_DIR:-${butter_root}/build-release}"
butter_arch="$(uname -m)"

case "${butter_arch}" in
    x86_64)
        ;;
    aarch64 | arm64)
        butter_arch="aarch64"
        ;;
    *)
        echo "Butter currently supports x86_64 and aarch64; this machine reports ${butter_arch}."
        exit 1
        ;;
esac

missing_commands=()
for command_name in cmake ninja c++ rsync btrfs pkexec; do
    if ! command -v "${command_name}" >/dev/null 2>&1; then
        missing_commands+=("${command_name}")
    fi
done

if ((${#missing_commands[@]} > 0)) ||
   [[ ! -f /usr/lib/cmake/Qt6/Qt6Config.cmake ]] ||
   [[ ! -f /usr/lib/cmake/Qt6Quick/Qt6QuickConfig.cmake ]]; then
    echo "Butter needs its Arch build and runtime packages first:"
    echo
    echo "  sudo pacman -S --needed base-devel cmake ninja qt6-base qt6-declarative rsync btrfs-progs polkit"
    echo
    exit 1
fi

cmake -S "${butter_root}" -B "${butter_build_dir}" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr/local \
    -DBUTTER_EXPECTED_ARCHITECTURE="${butter_arch}" \
    -DBUTTER_POLKIT_ACTION_DIR=/usr/share/polkit-1/actions
cmake --build "${butter_build_dir}" --parallel

echo
echo "Installing Butter into /usr/local (one administrator prompt)..."
if ((EUID == 0)); then
    cmake --install "${butter_build_dir}"
else
    sudo cmake --install "${butter_build_dir}"
fi

echo
echo "Butter is installed. Open it from the app launcher or run: butter"
