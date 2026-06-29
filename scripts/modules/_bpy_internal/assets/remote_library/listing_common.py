# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

from __future__ import annotations

"""Shared code for dealing with an asset library index.

Basically this is shared code between the index generator and index downloader.
"""

from pathlib import Path

API_VERSION = 1
"""The API version supported and produced by this version of Blender."""

API_VERSIONED_SUBDIR = f"_v{API_VERSION}"
"""Sub-directory for all the asset index data except the top level metadata."""

ASSET_TOP_METADATA_FILENAME = "_asset-library-meta.json"
"""Filename for the top-level asset index file.

This is the entry point for an asset library, and is expected to be at the root
of the configured URL for the remote asset library.
"""

ASSET_INDEX_JSON_FILENAME = "asset-index.json"
"""Filename for the asset index.

This is expected to sit in the `API_VERSIONED_SUBDIR`, and reference other files
in the same directory.
"""


def api_versioned(subpath: Path | str) -> Path:
    "Return the subpath, prefixed with API_VERSIONED_SUBDIR."
    return Path(API_VERSIONED_SUBDIR) / subpath


API_VERSIONED_ASSET_INDEX_JSON_PATH = api_versioned(ASSET_INDEX_JSON_FILENAME).as_posix()
