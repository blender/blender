# SPDX-FileCopyrightText: 2025 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

from __future__ import annotations

import hashlib
import logging
import os
import re
import shutil
import unicodedata
import urllib.parse
from pathlib import Path

import bpy

from . import blender_asset_library_openapi as api_models

log = logging.getLogger(__name__)


def list_assets(blendfile: Path, asset_library_root: Path) -> tuple[api_models.FileV1, list[api_models.AssetV1]]:
    # Start by erasing everything from memory.
    bpy.ops.wm.read_homefile(use_factory_startup=True, use_empty=True, load_ui=False)

    blendfile_info = _blendfile_info(blendfile, asset_library_root)

    # TODO: when this is possible, get the version of Blender used to write this
    # blendfile (bpy.data.version when you open it 'for real'), rather than the
    # currently-running version.
    blendfile_info.blender_version = ".".join(map(str, bpy.app.version))

    # Tell Blender to only load asset data-blocks.
    with bpy.data.libraries.load(str(blendfile), assets_only=True) as (
        data_from,
        data_to,
    ):
        for attr in dir(data_to):
            setattr(data_to, attr, getattr(data_from, attr))

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
            thumbnail_url = urllib.parse.quote(as_posix)
        else:
            thumbnail_url = ""

        asset = api_models.AssetV1(
            name=datablock.name,
            id_type=datablock.id_type.lower(),
            file=file.path,
            thumbnail_url=thumbnail_url,
        )

        # Only set the fields that have a value. That way we can detect whether
        # none of them are set, and prevent the empty metadata from being
        # included.
        meta = api_models.AssetMetadataV1()
        if asset_data.catalog_id:
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
        if meta.model_fields_set:
            asset.meta = meta

        assets.append(asset)
    return assets


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
    file_url = urllib.parse.quote(relative_posix)

    if file_url == relative_posix:
        # Optimization: if the file path is URL-safe, it can be used as the URL
        # and there is no need to include this URL explicitly.
        file_url = None

    return api_models.FileV1(
        path=relative_posix,
        url=file_url,
        hash=_sha256_file(filepath),
        size_in_bytes=stat.st_size,
        blender_version="",  # Determined later when the file is opened to find assets.
    )


def _sha256_file(filepath: Path) -> str:
    """Computes and returns the SHA256 hash of the file."""
    sha256_hash = hashlib.sha256()

    file_size_bytes = 0
    with open(filepath, "rb") as f:
        for byte_block in iter(lambda: f.read(4096), b""):
            file_size_bytes += len(byte_block)
            sha256_hash.update(byte_block)
    return sha256_hash.hexdigest()
