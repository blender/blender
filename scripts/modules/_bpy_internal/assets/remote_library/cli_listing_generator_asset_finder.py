# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

from __future__ import annotations

import dataclasses
import logging
import os
import re
import shutil
import unicodedata
import urllib.parse
from collections import defaultdict
from collections.abc import Iterable
from pathlib import Path

import bpy

from . import blender_asset_library_openapi as api_models
from . import hashing

log = logging.getLogger(__name__)


# Empty tuple: no version.
# Two-item tuple: (major, minor)
# Other variations are not allowed.
type BlenderVersion = tuple[int, ...]


@dataclasses.dataclass(order=True)
class FileVersionInfo:
    """Version information of a single blend file.

    It contains information not just based on the file itself, but also based
    on the other discovered files. For example, `asset.blend` will get a
    `blender_version_until = (6, 0)` when a file `asset@b6_0.blend` is found.
    """

    # Make these keyword-only, so that they can be listed first, and thus are the first key when sorting.
    # The tuples are (major, minor) versions; micro/patch release numbers are ignored.
    #
    # These fields have the same semantics as #api_model.AssetBlenderVersionsV1, so `min` is inclusive and `until` is
    # exclusive. In other words, an asset is shown if the current Blender version is in [min, until).
    blender_version_min: BlenderVersion = dataclasses.field(default=(), kw_only=True)
    blender_version_until: BlenderVersion = dataclasses.field(default=(), kw_only=True)

    path: Path


# Match numbers after `@b`, but do not allow leading zeroes. This prevents having different markers (and thus different
# filenames) for the same version. Case-sensitive filesystems can still have two files (lower & upper case `b`) so this
# still has to be taken into account.
_filename_re = re.compile('(.*?)@b(0|[1-9][0-9]*)_(0|[1-9][0-9]*)', flags=re.IGNORECASE)


def filenames_group_by_version_metadata(paths: Iterable[Path]) -> list[FileVersionInfo]:
    """Determine min/until versions for the given filenames."""

    def _extract_version(file_stem: str) -> tuple[str, BlenderVersion]:
        """'file_name@b5_4' -> ('file_name', (5, 4))

        If there is no (valid) '@bX_Y' marker, returns (file_stem, ()).
        """

        match = _filename_re.fullmatch(file_stem)
        if not match:
            return file_stem, ()

        name_without_marker, marker_major, marker_minor = match.groups()
        if not name_without_marker:
            # The marker was not put _after_ an existing filename, and thus doesn't follow the specs.
            return file_stem, ()

        # int() will work because of the regexp.
        version_major = int(marker_major, 10)
        version_minor = int(marker_minor, 10)

        return name_without_marker, (version_major, version_minor)

    # Cluster the paths by their base name, so excluding any `@bX_Y` marker.
    #
    # Keys: Base names, so the path without `@bX_Y` marker and without `.blend` suffix.
    # Values: List of FileVersionInfo of all paths that have the same base name.
    clusters: dict[Path, list[FileVersionInfo]] = defaultdict(list)
    for path in paths:
        base_name, version = _extract_version(path.stem)
        base_path = path.with_name(base_name)
        version_info = FileVersionInfo(path, blender_version_min=version)
        clusters[base_path].append(version_info)

    # For each cluster, determine the 'until' Blender version fields.
    version_infos: list[FileVersionInfo] = []
    for cluster in clusters.values():
        cluster.sort()  # Sort cluster by version.

        # Set the 'until' version based on the 'min' version of the next item in the cluster.
        for index, version_info in enumerate(cluster[:-1]):
            # Duplicates are anticipated, and so the 'next item' is the next one with a different version_min than the
            # current item, not just the item with the next index.
            for next_info in cluster[index + 1:]:
                if version_info.blender_version_min == next_info.blender_version_min:
                    # This is a duplicate, keep going.
                    log.warning("Blend files %s and %s have the same version marker",
                                version_info.path, next_info.path)
                    continue
                version_info.blender_version_until = next_info.blender_version_min
                break

        version_infos.extend(cluster)

    return version_infos


def list_assets(blendfile_version_info: FileVersionInfo,
                asset_library_root: Path) -> tuple[api_models.FileV1,
                                                   list[api_models.AssetV1]]:
    blendfile = blendfile_version_info.path

    # Start by erasing everything from memory.
    bpy.ops.wm.read_homefile(use_factory_startup=True, use_empty=True, load_ui=False)

    blendfile_info = _blendfile_info(blendfile, asset_library_root)

    # Load asset datablocks from the blend file, and check the version of Blender used to write it.
    with bpy.data.libraries.load(str(blendfile), assets_only=True) as (
        data_from,
        data_to,
    ):
        for attr in dir(data_to):
            setattr(data_to, attr, getattr(data_from, attr))

        # For the assets' min version only (major, minor) is relevant.
        file_written_version = tuple(data_from.version[:2])

        # Store the entire version (including patch) in the api_models.FileV1.
        blendfile_info.blender_version = _version_to_string(data_from.version)

    # TODO: when multiple files are supported, take the maximum of the files.
    bl_versions = _asset_versions_min_until(blendfile_version_info, file_written_version)

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
            blendfile_info.path,
            bl_versions,
            datablocks,
            thumbnail_dir,
            should_write_thumbnails,
        )
        assets.extend(datablocks_assets)

    # After processing is done, set the thumbnail dir mtime to that of the
    # blendfile. By tracking the mtime of the directory itself, not every
    # individual thumbnail needs to be time-checked.
    thumbnail_dir.mkdir(exist_ok=True, parents=True)
    thumbnail_timestamper.touch(exist_ok=True)
    os.utime(thumbnail_timestamper, (blend_stat.st_atime, blend_stat.st_mtime))

    return blendfile_info, assets


def _find_assets(
    asset_library_root: Path,
    blendfile_relpath: str,
    bl_versions: api_models.AssetBlenderVersionsV1,
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

        if thumbnail_path and thumbnail_path.exists():
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
            files=[blendfile_relpath],
            thumbnail=thumbnail,
            bl_versions=bl_versions,
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
    if asset_data.use_preferred_import_method:
        meta.preferred_import_method = asset_data.preferred_import_method

    # Convert custom properties.
    import rna_prop_ui

    custom_props: api_models.CustomPropertiesV1 = []
    for prop_name, prop_value in asset_data.items():
        is_array = isinstance(prop_value, rna_prop_ui.ARRAY_TYPES) and len(prop_value) > 0
        item_value = prop_value[0] if is_array else prop_value

        match item_value:
            case bool():
                value_type = api_models.CustomPropertyTypeV1.IDP_BOOL
            case int():
                value_type = api_models.CustomPropertyTypeV1.IDP_INT
            case str():
                value_type = api_models.CustomPropertyTypeV1.IDP_STRING
            case float():
                value_type = api_models.CustomPropertyTypeV1.IDP_FLOAT
            case _:
                # Unsupported type, just ignore it.
                continue

        if is_array:
            custom_prop = api_models.CustomPropertyV1(
                name=prop_name,
                type=api_models.CustomPropertyTypeV1.IDP_ARRAY,
                value=list(prop_value),
                itemtype=value_type,
            )
        else:
            custom_prop = api_models.CustomPropertyV1(
                name=prop_name, type=value_type, value=prop_value
            )

        custom_props.append(custom_prop)

    if custom_props:
        meta.properties = custom_props

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


def _version_to_string(version: Iterable[int]) -> str:
    """(6, 40) -> '6.40'."""
    return ".".join(map(str, version))


def _asset_versions_min_until(
    filename_version_info: FileVersionInfo,
    file_written_version: BlenderVersion,
) -> api_models.AssetBlenderVersionsV1:
    """Determine min/until Blender versions for assets in a blend file."""

    if filename_version_info.blender_version_min and file_written_version > filename_version_info.blender_version_min:
        log.warning(
            "blend file %s was written with newer version of Blender (%s) than its marker suggests (%s)",
            filename_version_info.path,
            _version_to_string(file_written_version),
            _version_to_string(filename_version_info.blender_version_min),
        )

    # The minimal version of Blender to use for this asset is limited both by the version info from the filename and the
    # blender version that wrote the file.
    version_min = max(filename_version_info.blender_version_min, file_written_version)
    assert version_min

    bl_versions = api_models.AssetBlenderVersionsV1(
        min=_version_to_string(version_min),
    )
    if filename_version_info.blender_version_until:
        bl_versions.until = _version_to_string(filename_version_info.blender_version_until)

        # Warn about version weirdnesses.
        ver_until = filename_version_info.blender_version_until
        if ver_until and ver_until <= version_min:
            log.warning(
                "Assets from %s will not be visible, because their 'until' version %s is before or "
                "at the 'min' version %s",
                filename_version_info.path,
                _version_to_string(ver_until),
                _version_to_string(version_min),
            )

    return bl_versions
