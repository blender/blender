#!/usr/bin/env bash

set -euo pipefail

# Update `keep-collada` with latest lfs-fallback Blender blender-v5.0-release and push to origin.

branch="keep-collada"

echo "==> Ensuring branch: ${branch}"
git checkout "${branch}"

echo "==> Fetching lfs-fallback/main"
git fetch lfs-fallback blender-v5.0-release --prune

echo "==> Rebasing onto lfs-fallback/main (skipping LFS smudge)"
GIT_LFS_SKIP_SMUDGE=1 git rebase lfs-fallback/blender-v5.0-release

echo "==> Initializing/updating COLLADA submodules (all platforms if present)"
for sm in lib/linux_x64_collada lib/windows_x64_collada lib/windows_arm64_collada lib/macos_arm64_collada; do
  if git config --file .gitmodules --get-regexp ".*${sm}" >/dev/null 2>&1; then
    echo "  -> updating ${sm}"
    GIT_LFS_SKIP_SMUDGE=1 git submodule update --init --progress "${sm}" || true
    git -C "${sm}" lfs pull || true
  fi
done

echo "==> Pushing to origin (skipping pre-push LFS hook)"
git push --no-verify origin "${branch}" --force-with-lease

echo "==> Done"


