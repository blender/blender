# SPDX-FileCopyrightText: 2025 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""Service for computing hashes of files on disk.

The hashes are cached using a storage back-end (currently the SQLite back-end is
the only available one). The back-end manages concurrent access, so that
multiple Blender instances can use the same cache without conflict.

Service instances are obtained via `get_service(storage_path)`. They are cached
until a new blend file is loaded or Blender exits.
"""

__all__ = (
    'get_service',
)

import atexit
from typing import TYPE_CHECKING

import bpy

if TYPE_CHECKING:
    from pathlib import Path as _Path
    from _bpy_internal.disk_file_hash_service.hash_service import DiskFileHashService as _DiskFileHashService
else:
    _Path = object
    _DiskFileHashService = object


# Mapping from storage path to the service.
_services: dict[_Path, _DiskFileHashService] = {}


def get_service(storage_path: _Path) -> _DiskFileHashService:
    """Get a disk file hash service that stores its cache on the given path.

    Depending on the back-end (currently there is only the SQLite back-end, and
    thus there is no choice in which one is used), the storage_path can be used
    as directory or as file prefix. The SQLite back-end uses
    `{storage_path}_v{schema_version}.sqlite` as storage.

    Once a DiskFileHashService is constructed, it is cached for future
    invocations. These cached services are cleaned up when Blender loads another
    file or when it exits.
    """
    try:
        return _services[storage_path]
    except KeyError:
        pass

    from _bpy_internal.disk_file_hash_service import backend_sqlite, hash_service

    # Construct the service.
    backend = backend_sqlite.SQLiteBackend(storage_path)
    service = hash_service.DiskFileHashService(backend)

    # Register cleanup app handlers, if they haven't been registered yet.
    if _on_file_load_pre not in bpy.app.handlers.load_pre:
        bpy.app.handlers.load_pre.append(_on_file_load_pre)

    service.open()
    _services[storage_path] = service

    return service


@bpy.app.handlers.persistent
def _on_file_load_pre(_filename: str) -> None:
    _cleanup_all_services()


@atexit.register
def on_blender_exit() -> None:
    # Named without an underscore, to prevent code checkers from (incorrectly)
    # thinking this function is never used. VSCode/Pylance needs this.
    _cleanup_all_services()


def _cleanup_all_services() -> None:
    """Close & delete all known services."""

    while _services:
        _, service = _services.popitem()
        try:
            service.close()
        except Exception:
            # Print the exception, but keep running so that the next service can
            # be closed.
            import traceback
            traceback.print_exc()
