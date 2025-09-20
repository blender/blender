#!/usr/bin/env bash

set -euo pipefail

# Export collada-related sources into a standalone tree under ./_export/collada
# so it can be pushed to a separate GitHub repo.

EXPORT_DIR="${1:-_export/collada}"

mkdir -p "${EXPORT_DIR}"

echo "==> Exporting COLLADA sources to ${EXPORT_DIR}"
rsync -a --delete \
  source/blender/io/collada/ \
  "${EXPORT_DIR}/source/blender/io/collada/"

echo "==> Creating README with integration pointers"
cat >"${EXPORT_DIR}/README.md" <<'EOF'
# Blender COLLADA (standalone)

This tree contains Blender's COLLADA IO sources, extracted for reuse.

Integration outline:
- Copy `source/blender/io/collada` into your Blender source tree.
- Ensure `WITH_COLLADA` is enabled and the module is added in CMakeLists where needed.
- Provide platform libraries (OpenCOLLADA and friends) via your deps builds.

NOTE: This is an export snapshot; see the upstream fork for history.
EOF

echo "==> Done. You can now initialize a git repo in ${EXPORT_DIR} and push to GitHub."


