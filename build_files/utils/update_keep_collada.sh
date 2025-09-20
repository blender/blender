#!/usr/bin/env bash

set -euo pipefail

# Update `keep-collada` with latest upstream Blender main and push to origin.

branch="keep-collada"

echo "==> Ensuring branch: ${branch}"
git checkout "${branch}"

echo "==> Fetching upstream"
git fetch upstream --tags --prune

echo "==> Rebasing onto upstream/main (skipping LFS smudge)"
GIT_LFS_SKIP_SMUDGE=1 git rebase upstream/main

echo "==> Pushing to origin (skipping pre-push LFS hook)"
git push --no-verify origin "${branch}" --force-with-lease

echo "==> Done"


