# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import functools
import typing
from pathlib import Path

if typing.TYPE_CHECKING:
    from _bpy_internal.assets.remote_library_listing.blender_asset_library_openapi import URLWithHash as _URLWithHash
else:
    _URLWithHash = object


def hash_file(filepath: Path) -> str:
    """Computes and returns the hash of the file.

    The returned string is prefixed with the hash type, like "{TYPE}:{HASH}".
    """
    return 'SHA256:' + _sha256_file(filepath)


@functools.lru_cache
def _dfhs_storage_path() -> Path:
    """Return the storage path of the disk file hash service."""
    import bpy

    hashes_dir = Path(bpy.app.cachedir) / "{:d}.{:d}/file_hashes".format(*bpy.app.version)
    hashes_dir.mkdir(parents=True, exist_ok=True)
    return hashes_dir / "dfhs"


def _sha256_file(filepath: Path) -> str:
    """Computes and returns the SHA256 hash of the file."""
    from _bpy_internal import disk_file_hash_service

    dfhs = disk_file_hash_service.get_service(_dfhs_storage_path())
    return dfhs.get_hash(filepath, 'sha256')


def url(url_with_hash: _URLWithHash | tuple[str, str]) -> str:
    """Return the url, with the hash on the query string.

    >>> url(URLWithHash(url="http://localhost/", hash="sha256:the-hash"))
    'http://localhost/?hash=the-hash'
    >>> url(("http://localhost/", "sha256:the-hash"))
    'http://localhost/?hash=the-hash'
    """

    import urllib.parse

    # Get the URL and the hash.
    if isinstance(url_with_hash, tuple):
        url, hash_with_type = url_with_hash
    else:
        url = url_with_hash.url
        hash_with_type = url_with_hash.hash

    # Without a hash, it's simple.
    if not hash_with_type:
        return url

    # Remove the hash type from the hash string.
    try:
        _, hash_value = hash_with_type.split(':', 1)
    except ValueError:
        # This means the hash is not in the form '{TYPE}:{HASH}'; just use it as-is.
        hash_value = hash_with_type

    # Append to the URL with the correct separator.
    sep = '&' if '?' in url else '?'
    return url + sep + 'hash=' + urllib.parse.quote(hash_value)
