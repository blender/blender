# SPDX-FileCopyrightText: 2025 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

from __future__ import annotations

try:
    # Introduced in Python 3.12:
    from itertools import batched  # type: ignore
except ImportError:
    import itertools

    # According to the itertools documentation, this code is equivalent:
    def batched(iterable, n):  # type: ignore
        if n < 1:
            raise ValueError('n must be at least one')
        iterator = iter(iterable)
        while batch := tuple(itertools.islice(iterator, n)):
            yield batch


from . import blender_asset_library_openapi as api_models


def paginate_asset_list(
        assets: list[api_models.AssetV1],
        num_assets_per_page: int = 0,
) -> list[api_models.AssetLibraryIndexPageV1]:
    """Return a list of asset pages.

    Each page is no longer than `num_assets_per_page` long. If zero, all assets
    are put in the same page.
    """

    if not num_assets_per_page:
        return [api_models.AssetLibraryIndexPageV1(
            asset_count=len(assets),
            assets=assets,
        )]

    pages = []
    for asset_batch in batched(assets, num_assets_per_page):
        page = api_models.AssetLibraryIndexPageV1(
            asset_count=len(asset_batch),
            assets=list(asset_batch),
        )
        pages.append(page)
    return pages
