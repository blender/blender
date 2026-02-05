# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

from __future__ import annotations

import logging
import os
import re
import shutil
import unicodedata
import urllib.parse
from pathlib import Path

import bpy

from . import blender_asset_library_openapi as api_models
from . import hashing

log = logging.getLogger(__name__)


def list_assets(blendfile: Path, asset_library_root: Path) -> tuple[api_models.FileV1, list[api_models.AssetV1]]:
    # Start by erasing everything from memory.
    bpy.ops.wm.read_homefile(use_factory_startup=True, use_empty=True, load_ui=False)

    blendfile_info = _blendfile_info(blendfile, asset_library_root)

    # Tell Blender to only load asset data-blocks.
    with bpy.data.libraries.load(str(blendfile), assets_only=True) as (
        data_from,
        data_to,
    ):
        for attr in dir(data_to):
            setattr(data_to, attr, getattr(data_from, attr))

        # Convert the Blender version to a string.
        blend_version = ".".join(map(str, data_from.version))
        blendfile_info.blender_version = blend_version

    # Get the last modification timestamp of the blend file, to compare against
    # the thumbnails.
    thumbnail_dir = blendfile.with_name(blendfile.stem + "_thumbnails")
    blend_stat = blendfile.stat()

    thumbnail_timestamper = thumbnail_dir / ".last_modified"
    if thumbnail_timestamper.exists():
        thumb_mtime = thumbnail_timestamper.stat().st_mtime
        should_write_thumbnails = abs(blend_stat.st_mtime - thumb_mtime) > 0.001
    else:
        should_write_thumbnails = True

    if should_write_thumbnails:
        # Remove the entire thumbnail tree, so that thumbnails of deleted assets
        # are also deleted. All thumbnails are going to be re-written anyway.
        log.debug("thumbnails will be exported to %s", thumbnail_dir)
        assert thumbnail_dir
        if Path(thumbnail_dir.root) == thumbnail_dir:
            raise RuntimeError(f"Refusing to remove a root directory: {thumbnail_dir}")
        if thumbnail_dir.exists():
            shutil.rmtree(thumbnail_dir)

    # Collect the asset data.
    assets: list[api_models.AssetV1] = []
    for attr in dir(data_to):
        if attr == 'version':
            continue
        datablocks = getattr(data_from, attr)
        datablocks_assets = _find_assets(
            asset_library_root,
            blendfile_info,
            datablocks,
            thumbnail_dir,
            should_write_thumbnails,
        )
        assets.extend(datablocks_assets)

    # After processing is done, set the thumbnail dir mtime to that of the
    # blendfile. By tracking the mtime of the directory itself, not every
    # individual thumbnail needs to be time-checked.
    thumbnail_timestamper.touch(exist_ok=True)
    os.utime(thumbnail_timestamper, (blend_stat.st_atime, blend_stat.st_mtime))

    return blendfile_info, assets


def _find_assets(
    asset_library_root: Path,
    file: api_models.FileV1,
    datablocks: bpy.types.BlendData,
    thumbnail_dir: Path,
    should_write_thumbnails: bool,
) -> list[api_models.AssetV1]:

    assets = []
    for datablock in datablocks:
        asset_data: bpy.types.AssetData = datablock.asset_data
        if not asset_data:
            continue

        thumbnail_path = _thumbnail_path(datablock, thumbnail_dir)
        if thumbnail_path and should_write_thumbnails:
            _save_thumbnail(datablock, thumbnail_path)

        if thumbnail_path:
            as_posix = thumbnail_path.relative_to(asset_library_root).as_posix()
            thumbnail = api_models.URLWithHash(
                url=urllib.parse.quote(as_posix),
                hash=hashing.hash_file(thumbnail_path),
            )
        else:
            thumbnail = None

        asset = api_models.AssetV1(
            name=datablock.name,
            id_type=datablock.id_type,
            files=[file.path],
            thumbnail=thumbnail,
            meta=_get_asset_meta(asset_data),
        )

        assets.append(asset)
    return assets


def _get_asset_meta(asset_data: bpy.types.AssetData) -> api_models.AssetMetadataV1 | None:
    # Only set the fields that have a value. That way we can detect whether
    # none of them are set, and prevent the empty metadata from being
    # included.
    meta = api_models.AssetMetadataV1()
    if asset_data.catalog_id and asset_data.catalog_id != "00000000-0000-0000-0000-000000000000":
        meta.catalog_id = asset_data.catalog_id
    if asset_data.tags:
        meta.tags = [tag.name for tag in asset_data.tags]
    if asset_data.author:
        meta.author = asset_data.author
    if asset_data.description:
        meta.description = asset_data.description
    if asset_data.license:
        meta.license = asset_data.license
    if asset_data.copyright:
        meta.copyright = asset_data.copyright

    # Convert custom properties.
    import rna_prop_ui

    custom_props: api_models.CustomPropertiesV1 = {}
    for prop_name, prop_value in asset_data.items():
        is_array = isinstance(prop_value, rna_prop_ui.ARRAY_TYPES) and len(prop_value) > 0
        item_value = prop_value[0] if is_array else prop_value

        match item_value:
            case bool():
                value_type = api_models.CustomPropertyTypeV1.BOOLEAN
            case int():
                value_type = api_models.CustomPropertyTypeV1.INT
            case str():
                value_type = api_models.CustomPropertyTypeV1.STRING
            case float():
                value_type = api_models.CustomPropertyTypeV1.FLOAT
            case _:
                # Unsupported type, just ignore it.
                continue

        if is_array:
            custom_prop = api_models.CustomPropertyV1(
                type=api_models.CustomPropertyTypeV1.ARRAY,
                value=list(prop_value),
                itemtype=value_type,
            )
        else:
            custom_prop = api_models.CustomPropertyV1(type=value_type, value=prop_value)

        custom_props[prop_name] = custom_prop

    if custom_props:
        meta.custom = custom_props

    if meta == api_models.AssetMetadataV1():
        return None
    return meta


def _save_thumbnail(datablock: bpy.types.ID, thumbnail_path: Path) -> None:
    """Save the internal preview thumbnail as a WebP image."""

    # Get the preview image size.
    width: int = datablock.preview.image_size[0]
    height: int = datablock.preview.image_size[1]

    if not (width > 0 and height > 0):
        return

    thumbnail_path.parent.mkdir(exist_ok=True, parents=True)

    log.debug("Writing thumbnail: %s", thumbnail_path)
    try:
        # Create a new image in Blender to store the preview.
        image: bpy.types.Image = bpy.data.images.new(
            thumbnail_path.stem, width, height, alpha=True
        )

        # Assign the pixel data from the preview to the new image.
        # image.pixels = [p for p in datablock.preview.image_pixels_float]
        image.pixels[:] = datablock.preview.image_pixels_float

        # Save the image to disk.
        image.file_format = "WEBP"
        image.save(filepath=str(thumbnail_path), quality=80)

        # Remove the image from Blender data after saving to free memory.
        bpy.data.images.remove(image)
    except Exception as e:
        print(f"Failed to save thumbnail for {datablock.name}: {e}")


def _thumbnail_path(datablock: bpy.types.ID, thumbnail_dir: Path) -> Path | None:
    """Return the path for this datablock's thumbnail, or None if it has none."""

    if not datablock.preview:
        return None

    datablock_safe = _name_to_filename(datablock.name)
    thumbnail_path: Path = (
        thumbnail_dir / datablock.id_type.title() / f"{datablock_safe}.webp"
    )

    return thumbnail_path


_re_safe_filename_nonword = re.compile(r'[^\w\s_-]')
_re_safe_filename_dashspace = re.compile(r'[-\s]+')


def _name_to_filename(value: str) -> str:
    """Convert a string into something that should be safe as filename."""

    value = unicodedata.normalize('NFKD', value).encode('ascii', 'ignore').decode('ascii')
    value = _re_safe_filename_nonword.sub('', value.lower())
    return _re_safe_filename_dashspace.sub('-', value).strip('-_')


def _blendfile_info(filepath: Path, asset_library_root: Path) -> api_models.FileV1:
    stat = filepath.stat()

    relative_posix = filepath.relative_to(asset_library_root).as_posix()
    file_url: str | None = urllib.parse.quote(relative_posix)

    if file_url == relative_posix:
        # Optimization: if the file path is URL-safe, it can be used as the URL
        # and there is no need to include this URL explicitly.
        file_url = None

    return api_models.FileV1(
        path=relative_posix,
        url=file_url,
        hash=hashing.hash_file(filepath),
        size_in_bytes=stat.st_size,
        blender_version="",  # Determined later when the file is opened to find assets.
    )
