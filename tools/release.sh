#!/bin/sh
set -eu
if [ "$#" -ne 2 ]; then
 echo "Usage: sh tools/release.sh IMAGE_SIGNEE.bin DOSSIER_NOUVEAU" >&2
 exit 2
fi
test -s "$1"
mkdir "$2"
cp "$1" "$2/firmware.bin"
(cd "$2" && sha256sum firmware.bin > SHA256SUMS && sha256sum -c SHA256SUMS)
