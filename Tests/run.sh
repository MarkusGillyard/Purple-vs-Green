#!/usr/bin/env bash
set -euo pipefail
repo=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd -P)
binary=$(mktemp)
trap 'rm -f -- "$binary"' EXIT
"${CXX:-c++}" -std=c++20 -Wall -Wextra -Werror -pedantic \
  -I "$repo/Source/PurpleVsGreen" "$repo/Tests/ArenaRules.cpp" -o "$binary"
"$binary"
