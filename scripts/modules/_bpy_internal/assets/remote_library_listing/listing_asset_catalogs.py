# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""Parser for Blender's asset catalog files.

It would be better if there was an RNA API for this, but for now this is faster
to implement.
"""

from __future__ import annotations

import dataclasses
from pathlib import Path, PurePosixPath

from . import blender_asset_library_openapi as api_models

SUPPORTED_VERSION = 1


@dataclasses.dataclass(frozen=True)
class AssetCatalog:
    uuid: str
    path: PurePosixPath
    simple_name: str


def parse_catalogs(library_path: Path) -> list[api_models.CatalogV1]:
    """Parse all asset catalog files in the asset library.

    Returns a collection of all asset catalogs in the library, as a mapping from
    UUID to the catalog.

    If there are multiple catalog definition files, they will be merged
    together.
    """
    # First use a mapping from UUID to the AssetCatalog, to ensure that each
    # UUID only maps to a single path.
    catalogs_by_uuid: dict[str, AssetCatalog] = {}

    for file in library_path.rglob('*.cats.txt'):
        file_cats = _parse_catalog(file)
        catalogs_by_uuid.update(file_cats)

    # Group catalogs by their path, to make the returned list compatible with
    # the API model.
    asset_cats_by_path: dict[PurePosixPath, api_models.CatalogV1] = {}
    for cat in catalogs_by_uuid.values():
        try:
            api_catalog = asset_cats_by_path[cat.path]
        except KeyError:
            asset_cats_by_path[cat.path] = api_models.CatalogV1(
                path=cat.path.as_posix(),
                uuids=[cat.uuid],
                simple_name=cat.simple_name,
            )
        else:
            api_catalog.uuids.append(cat.uuid)

    return sorted(asset_cats_by_path.values(), key=lambda api_cat: api_cat.path)


def _parse_catalog(catalog_filepath: Path) -> dict[str, AssetCatalog]:
    # Mapping from UUID to the AssetCatalog.
    catalogs: dict[str, AssetCatalog] = {}

    with catalog_filepath.open('r', encoding='utf-8') as infile:
        for line in infile:
            line = line.strip()
            if not line or line.startswith('#'):
                continue

            # Check the declared version, and simply ignore the file if it is
            # not supported.
            if line.startswith('VERSION '):
                _, version_as_str = line.split(maxsplit=1)
                if version_as_str != str(SUPPORTED_VERSION):
                    msg = "{}: this version of Blender does not support catalog file version {!r}"
                    print(msg.format(catalog_filepath, version_as_str))
                    return {}
                continue

            parts = line.split(':', maxsplit=2)
            if len(parts) < 2:
                # It's ok for the 'simple name' part to be missing, but if more is missing, this is not a valid file.
                msg = "{}: this does not seem to be an asset catalog file, ignoring it (line {!r} is not as expected)"
                print(msg.format(catalog_filepath, line))
                return {}

            cat = AssetCatalog(
                uuid=parts[0],
                path=PurePosixPath(parts[1]),
                simple_name=parts[2] if len(parts) >= 3 else "",
            )
            catalogs[cat.uuid] = cat

    return catalogs


_ASSET_CATS_HEADER = """# This is an Asset Catalog Definition file for Blender.
#
# Empty lines and lines starting with `#` will be ignored.
# The first non-ignored line should be the version indicator.
# Other lines are of the format "UUID:catalog/path/for/assets:simple catalog name"
#
# Remote Asset Library: {library_name!s}

VERSION 1
"""


def write(catalogs: list[api_models.CatalogV1], catalog_filepath: Path,
          asset_library_meta: api_models.AssetLibraryMeta) -> None:
    """Create a catalog file from the list of catalogs."""

    # TODO: this really should be using an RNA API.

    header = _ASSET_CATS_HEADER.format(library_name=asset_library_meta.name)

    with catalog_filepath.open("w", encoding="utf8") as catfile:
        print(header, file=catfile)

        for cat in sorted(catalogs, key=lambda cat: cat.path):
            for uuid in cat.uuids:
                print("{}:{}:{}".format(uuid, cat.path, cat.simple_name), file=catfile)
