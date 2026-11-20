#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
build=$(mktemp -d .test.XXXXXXXX)
trap 'rm -rf "$build"' EXIT
${CXX:-c++} -std=c++17 -Wall -Wextra -Werror -Iinclude test/host.cpp -o "$build/test"
"$build/test"
