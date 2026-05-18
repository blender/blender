# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

from __future__ import annotations

import logging
from pathlib import Path

from _bpy_internal.http import downloader as http_dl


class ExtraFileMetadataProvider(http_dl.MetadataProvider):
    """HTTP Metadata provider that can check an extra file.

    This is to support the following file sets:

    - `file.json`: Actual JSON file read by Blender. Is assumed to be validated.
    - `file-unsafe.json`: JSON file as downloaded. Must be validated before use.
    - `file-unsafe.json~`: The above file while it's being downloaded. Not yet
      complete JSON.

    The downloader will get the request to download to `file-unsafe.json`.
    However, if `file.json` is still fresh (i.e. the HTTP metadata for the URL
    is applicable to that file), the downloader should be able to do a
    conditional download (instead of an unconditional one).

    This is implemented as a wrapper for any other MetadataProvider, rather than
    subclassing a specific one, so that it's independent of the underlying
    logic.
    """

    _wrapped: http_dl.MetadataProvider
    _logger: logging.Logger

    def __init__(self, wrapped: http_dl.MetadataProvider) -> None:
        self._wrapped = wrapped
        self._logger = logging.getLogger(__name__ + ".ExtraFileMetadataProvider")

    def save(self, http_req_descr: http_dl.RequestDescription, meta: http_dl.HTTPMetadata) -> None:
        self._wrapped.save(http_req_descr, meta)

    def load(self, http_req_descr: http_dl.RequestDescription) -> http_dl.HTTPMetadata | None:
        return self._wrapped.load(http_req_descr)

    def is_valid(
            self,
            meta: http_dl.HTTPMetadata,
            http_req_descr: http_dl.RequestDescription,
            local_path: Path) -> bool:
        # This assumes that the download is saved to the "unsafe" location, and
        # we have to check the metadata on the "safe" location as well.

        if self._wrapped.is_valid(meta, http_req_descr, local_path):
            self._logger.info("HTTP metadata is valid for %s", local_path)
            return True

        safe_filename = unsafe_to_safe_filename(local_path)
        if safe_filename == local_path:
            # There is no different filename to check, so let's stick to the
            # result of the first is_valid() call.
            self._logger.info("HTTP metadata is invalid for %s", local_path)
            return False

        if self._wrapped.is_valid(meta, http_req_descr, safe_filename):
            self._logger.info("HTTP metadata is valid for %s", safe_filename)
            return True

        self._logger.info("HTTP metadata is valid for neither %s nor %s", local_path, safe_filename)
        return False

    def forget(self, http_req_descr: http_dl.RequestDescription) -> None:
        self._wrapped.forget(http_req_descr)


def unsafe_to_safe_filename(unsafe_file_path: Path) -> Path:
    """path/to/some_file.unsafe-json -> path/to/some_file.json"""
    # The suffix is changed, and not the stem, so that globs like "*.json" do not see the unsafe files.
    return unsafe_file_path.with_suffix(unsafe_file_path.suffix.replace('unsafe-', ''))


def safe_to_unsafe_filename(safe_file_path: Path | str) -> Path:
    """path/to/some_file.json -> path/to/some_file.unsafe-json"""
    if isinstance(safe_file_path, str):
        safe_file_path = Path(safe_file_path)

    # path.suffix includes the leading period, so it's something like ".json".
    return safe_file_path.with_suffix('.unsafe-' + safe_file_path.suffix[1:])
