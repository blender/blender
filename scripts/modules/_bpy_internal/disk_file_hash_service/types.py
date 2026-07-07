# SPDX-FileCopyrightText: 2025 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

from pathlib import Path
from typing import Protocol, Callable
import dataclasses


__all__ = (
    'DiskFileHashBackend',
    'FileHashInfo',
)


@dataclasses.dataclass
class FileHashInfo:
    hexhash: str
    file_size_bytes: int
    file_stat_mtime: float


class DiskFileHashBackend(Protocol):
    def open(self) -> None:
        """Prepare the back-end for use."""

    def close(self) -> None:
        """Close the back-end.

        After calling this, the back-end is not expected to work any more.
        """

    def fetch_hash(self, filepath: Path, hash_algorithm: str) -> FileHashInfo | None:
        """Return the cached hash info of a given file.

        If no info is cached for this path/algorithm combo, returns None.
        """

    def store_hash(
        self,
        filepath: Path,
        hash_algorithm: str,
        hash_info: FileHashInfo,
        pre_write_callback: Callable[[], None] | None = None,
    ) -> None:
        """Store a pre-computed hash for the given file path.

        See DiskFileHashService.store_hash() for an explanation of the parameters.
        """

    def mark_hash_as_fresh(self, filepath: Path, hash_algorithm: str) -> None:
        """Store that the hash is still considered 'fresh'.

        See `remove_older_than()`.
        """

    def remove_older_than(self, *, days: int) -> None:
        """Remove all hash entries that are older than this many days.

        When this removes all known hashes for a file, the file entry itself is
        also removed.
        """
