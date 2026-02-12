# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

from __future__ import annotations

from itertools import batched

from . import blender_asset_library_openapi as api_models


def paginate_asset_list(
        assets: list[api_models.AssetV1],
        files: list[api_models.FileV1],
        num_assets_per_page: int = 0,
) -> list[api_models.AssetLibraryIndexPageV1]:
    """Return a list of asset pages.

    Each page is no longer than `num_assets_per_page` long. If zero, all assets
    are put in the same page.

    The files listed in each page are determined by the assets on that page.
    This means that it's possible for multiple pages to list the same file; this
    occurs when that file contains muliple assets, spread across multiple pages.
    """

    if not num_assets_per_page:
        return [api_models.AssetLibraryIndexPageV1(
            asset_count=len(assets),
            assets=assets,
            file_count=len(files),
            files=files,
        )]

    pages = []
    for asset_batch in batched(assets, num_assets_per_page):
        used_file_paths = {
            file
            for asset in asset_batch
            for file in asset.files
        }
        file_batch = [file for file in files
                      if file.path in used_file_paths]
        page = api_models.AssetLibraryIndexPageV1(
            asset_count=len(asset_batch),
            assets=list(asset_batch),
            file_count=len(file_batch),
            files=file_batch,
        )
        pages.append(page)
    return pages
