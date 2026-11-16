#!/usr/bin/env bash
# SPDX-License-Identifier: MPL-2.0
# dist/evilgenius-module.tar.gz: one evilgenius/ folder, as Schwung Manager expects (schwung docs/MODULES.md).
set -euo pipefail
DIR="$(cd "$(dirname "$0")/.." && pwd)"
cd "$DIR"
for f in build/move/evilgenius-host build/move/evilgenius-heal; do
    [[ -x "$f" ]] || { echo "ERROR: $f is missing: run make move" >&2; exit 1; }
done
rm -rf dist/evilgenius
mkdir -p dist/evilgenius
cp module/module.json module/standalone build/move/evilgenius-host build/move/evilgenius-heal LICENSE THIRD_PARTY_LICENSES.md dist/evilgenius/
chmod 755 dist/evilgenius/{standalone,evilgenius-host,evilgenius-heal}
# COPYFILE_DISABLE: no macOS ._ files in the archive
COPYFILE_DISABLE=1 tar -C dist -czf dist/evilgenius-module.tar.gz evilgenius
ls -l dist/evilgenius-module.tar.gz
