#!/bin/sh
# Creates shooter-upper-VERSION.tar.gz next to the PKGBUILD, for makepkg.
# The archive is written to a temporary place first, so tar never reads the
# file it is busy writing.
set -e
cd "$(dirname "$0")/.."
ver=$(sed -n 's/.*GAME_VERSION *"\(.*\)"/\1/p' src/config.h)
name=shooter-upper-$ver
tmp=$(mktemp -d)
tar --transform "s,^\.,$name," --exclude='./build*' --exclude='./.git' \
    --exclude='*.tar.gz' -czf "$tmp/$name.tar.gz" .
mv "$tmp/$name.tar.gz" "packaging/arch/$name.tar.gz"
rmdir "$tmp"
echo "wrote packaging/arch/$name.tar.gz"
