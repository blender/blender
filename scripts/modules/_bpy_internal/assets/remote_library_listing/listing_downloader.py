# SPDX-FileCopyrightText: 2025 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

from __future__ import annotations

import copy
import enum
import functools
import hashlib
import logging
import time
import unicodedata
import urllib.parse
from pathlib import Path, PurePosixPath
from typing import Callable, TypeAlias, TypeVar, Type

import bpy

from _bpy_internal.http import downloader as http_dl
from _bpy_internal.assets.remote_library_listing import blender_asset_library_openapi as api_models
from _bpy_internal.assets.remote_library_listing import listing_common
from _bpy_internal.assets.remote_library_listing import http_metadata
from _bpy_internal.assets.remote_library_listing import asset_catalogs
from _bpy_internal.assets.remote_library_listing import json_parsing

logger = logging.getLogger(__name__)

# TODO: discuss & adjust this value, as this was just picked more or less randomly:
THUMBNAIL_FRESH_DURATION_MINS = 60
"""If a thumbnail was downloaded this many seconds ago, do not download it again.

If the local timestamp of the thumbnail file indicates that it was downloaded
less that this duration ago, do not attempt a re-download. Even though the
downloader supports conditional downloads, and won't actually re-download when
the server responds with a `304 Not Modified`, doing that request still takes
time and requires resources on the server.
"""

# TODO: discuss & adjust this value, as this was just picked more or less randomly:
THUMBNAIL_FAILED_REDOWNLOAD_MINS = 60 * 24  # 1 day
"""Thumbnails that failed to download will only be re-downloaded after this."""


class RemoteAssetListingLocator:
    """Construct paths for various components of a remote asset library.

    Basically this determines where assets and their thumbnails are downloaded,
    and what their filenames will be.
    """

    _remote_url: str
    _local_path: Path

    _remote_url_split: urllib.parse.SplitResult
    _remote_url_path: PurePosixPath

    _thumbs_root: Path
    _thumbs_subpath_large = "large"

    def __init__(
        self,
        remote_url: str,
        local_path: Path | str,
    ) -> None:
        self._remote_url = remote_url
        self._local_path = Path(local_path)

        # Parse the remote URL. As this URL should never change, this can just
        # happen once here, instead of every time when this info is necessary.
        self._remote_url_split = urllib.parse.urlsplit(self._remote_url)
        self._remote_url_path = _sanitize_path_from_url(self._remote_url_split.path)

        self._thumbs_root = self._local_path / "_thumbs"

    @property
    def remote_url(self) -> str:
        return self._remote_url

    @property
    def local_path(self) -> Path:
        return self._local_path

    @property
    @functools.lru_cache()
    def http_metadata_cache_location(self) -> Path:
        return self._local_path / "_local-meta-cache"

    @property
    @functools.lru_cache()
    def catalogs_file(self) -> Path:
        return self._local_path / "blender_assets.cats.txt"

    def asset_download_path(self, asset_file: api_models.FileV1) -> Path:
        """Construct the absolute download path for this asset.

        This can raise a ValueError if the file path is not suitable (either
        downright invalid, or not ending in `.blend`).

        >>> loc = RemoteAssetListingLocator("https://localhost:8000/", Path("/tmp/dl"))
        >>> asset_file = api_models.FileV1(
        ...     path="monkeys/suzanne.blend",
        ...     url="http://localhost:8000/does/not/matter",
        ...     size_in_bytes=327,
        ...     hash='010203040506',
        ...     blender_version="1.2.3",
        ... )
        >>> loc.asset_download_path(asset_file)
        PosixPath('/tmp/dl/monkeys/suzanne.blend')
        """
        assert asset_file.path

        relpath = Path(asset_file.path)

        # TODO: support non-.blend downloads as well.
        path_suffix = relpath.suffix.lower()
        if path_suffix != '.blend':
            raise ValueError(
                "asset file path ({!s}) does not end in .blend (but in {!r})".format(
                    relpath, path_suffix))

        return self._local_path / relpath

    # TODO: add lru_cache decorator here too, once we can measure its impact.
    def thumbnail_download_path(self, asset: api_models.AssetV1, asset_file: api_models.FileV1) -> Path | None:
        """Construct the download path suitable for this asset's thumbnail.

        This assumes that the URL already ends in the correct extension. This is
        not always the case, and using the content type in the HTTP response
        headers is technically a possibility. Keeping this limitation makes the
        code considerably simpler, as now the download filename is known
        beforehand.
        """
        assert asset.thumbnail_url

        try:
            url_path, _ = self._path_from_url(asset.thumbnail_url)
        except ValueError as ex:
            logger.warning("thumbnail has invalid URL: %s", ex)
            return None

        url_suffix = url_path.suffix.lower()
        if url_suffix not in {'.webp', '.png'}:
            logger.warning(
                "thumbnail url (%s) does not end in .webp or .png (but in %r), ignoring",
                asset.thumbnail_url,
                url_suffix)
            return None

        asset_download_path = self.asset_download_path(asset_file)
        assert asset_download_path.is_absolute()

        id_type = asset.id_type.value.title()  # turn 'object' into 'Object'.
        hash = hashlib.md5(f"{asset_download_path}/{id_type}/{asset.name}".encode()).hexdigest()

        relative_path = Path(hash[:2], hash[2:]).with_suffix(url_path.suffix)
        return self._thumbs_root / self._thumbs_subpath_large / relative_path

    def _path_from_url(self, url: str) -> tuple[PurePosixPath, urllib.parse.SplitResult]:
        """Construct a PurePosixPath from the path component of this URL.

        This function can throw a ValueError if the URL is not valid.
        """
        # An URL can only be correctly interpreted if it's a full URL, and not
        # just a relative path.
        abs_url = urllib.parse.urljoin(self._remote_url, url)
        try:
            split_url = urllib.parse.urlsplit(abs_url)
        except ValueError as ex:
            raise ValueError("invalid URL ({!s}): {!s}".format(abs_url, ' '.join(ex.args)))

        return _sanitize_path_from_url(split_url.path), split_url

    def thumbnail_failed_path(self, thumbnail_path: Path) -> Path:
        """Return the 'failed' path for this thumbnail.

        Raises a ValueError if `thumbnail_path` is not actually in the thumbnail
        root directory.

        >>> loc = RemoteAssetListingLocator("https://localhost:8000/", Path("/tmp"))
        >>> loc.thumbnail_failed_path(Path("/tmp/_thumbs/large/01/020304.webp"))
        PosixPath('/tmp/_thumbs/failed/01/020304.webp')
        >>> loc.thumbnail_failed_path(Path("/somewhere/else/020304.webp"))
        Traceback (most recent call last):
            ...
        ValueError: ...
        """

        # Big assumption #1: the thumbnail path was constructed with the
        # thumbnail_download_path() function above, and thus is inside our
        # _thumbs_root. If it is not, it'll raise a ValueError.
        rel_path = thumbnail_path.relative_to(self._thumbs_root)

        # Big assumption #2: for the same reason, it's sitting in the "large"
        # subdirectory of the thumbs root.
        assert rel_path.parts[0] == self._thumbs_subpath_large
        return self._thumbs_root / "failed" / Path(*rel_path.parts[1:])


class DownloadStatus(enum.Enum):
    LOADING = 'loading'
    FINISHED_SUCCESSFULLY = 'finished successfully'
    FAILED = 'failed'


class RemoteAssetListingDownloader:
    _locator: RemoteAssetListingLocator

    OnUpdateCallback: TypeAlias = Callable[['RemoteAssetListingDownloader'], None]
    _on_update_callback: OnUpdateCallback
    OnDoneCallback: TypeAlias = Callable[['RemoteAssetListingDownloader'], None]
    _on_done_callback: OnDoneCallback
    OnMetafilesDoneCallback: TypeAlias = Callable[['RemoteAssetListingDownloader'], None]
    _on_metafiles_done_callback: OnMetafilesDoneCallback | None
    OnPageDoneCallback: TypeAlias = Callable[['RemoteAssetListingDownloader'], None]
    _on_page_done_callback: OnPageDoneCallback | None

    _bgdownloader: http_dl.BackgroundDownloader
    _num_asset_pages_pending: int

    _status: DownloadStatus
    _error_message: str
    """An error message to show to the user.

    Should be set on errors to communicate a message to users. Calling report()
    with 'ERROR' as the level will set this to the given message.
    """

    _DOWNLOAD_POLL_INTERVAL: float = 0.01
    """How often the background download process is polled, in seconds.

    Each 'poll' involves sending queued messages back & forth between the main
    Blender process and the background download process.
    """

    _referenced_local_files: list[Path]
    """Paths of actually-referenced index page files.

    This makes it possible to delete once-downloaded index pages that now no
    longer exist.
    """

    _library_meta: api_models.AssetLibraryMeta | None

    _noncritical_downloads: set[Path]
    """Failing downloads to any of these files will not cause a complete shutdown."""

    _parser: json_parsing.ValidatingParser

    def __init__(
        self,
        remote_url: str,
        local_path: Path | str,
        on_update_callback: OnUpdateCallback,
        on_done_callback: OnDoneCallback,
        on_metafiles_done_callback: OnMetafilesDoneCallback | None = None,
        on_page_done_callback: OnPageDoneCallback | None = None,
    ) -> None:
        """Create a downloader for the remote index of this library.

        :param remote_url: Base URL of the remote asset library server. See
           blender_asset_library_openapi.yaml for the files downloaded from
           there.

        :param local_path: The directory to download the index files to.

        :param on_update_callback: Called with one parameter (this
            RemoteAssetListingDownloader) in short, regular intervals
            (_DOWNLOAD_POLL_INTERVAL) while the download is ongoing, and once
            just after the download is done.

        :param on_done_callback: called with one parameter (this
            RemoteAssetListingDownloader) whenever the downloader is "done".

            Here "done" does not imply "successful", as cancellations, network
            errors, or other issues can cause things to abort. In that case,
            this function is still called.

        :param on_metafiles_done_callback: called with one parameter (this
            RemoteAssetListingDownloader) whenever the meta files
            (ASSET_TOP_METADATA_FILENAME, ASSET_INDEX_JSON_FILENAME, and
            blender_assets.cats.txt) are in their final location and ready to
            be picked up by the asset system.

        :param on_page_done_callback: called with one parameter (this
            RemoteAssetListingDownloader) when at least one new page of the
            asset listing finished downloading and verification, and was put in
            its final location, ready to be picked up by the asset system.
        """

        self._locator = RemoteAssetListingLocator(remote_url, local_path)

        self._on_done_callback = on_done_callback
        self._on_update_callback = on_update_callback
        self._on_metafiles_done_callback = on_metafiles_done_callback
        self._on_page_done_callback = on_page_done_callback

        self._num_asset_pages_pending = 0
        self._referenced_local_files = []
        self._library_meta = None
        self._noncritical_downloads = set()

        self._status = DownloadStatus.LOADING
        self._error_message = ""

        self._parser = json_parsing.ValidatingParser(api_models.OPENAPI_SPEC)

        # Work around a limitation of Blender, see bug report #139720 for details.
        self.on_timer_event = self.on_timer_event  # type: ignore[method-assign]

        self._http_metadata_provider = http_metadata.ExtraFileMetadataProvider(
            http_dl.MetadataProviderFilesystem(
                cache_location=self._locator.http_metadata_cache_location,
            ))

        # Create the background downloader object now, so that it
        # (hypothetically in some future) can be adjusted before the actual
        # downloading begins.
        self._bg_downloader = http_dl.BackgroundDownloader(
            options=http_dl.DownloaderOptions(
                metadata_provider=self._http_metadata_provider,
                http_headers={'Accept': 'application/json'},
                timeout=300,
            ),
            on_callback_error=self._on_callback_error,
        )
        self._bg_downloader.add_reporter(self)

    def __repr__(self) -> str:
        return "{!s}(remote_url={!r}, local_path={!r})".format(
            type(self),
            self._locator.remote_url,
            self._locator.local_path,
        )

    def download_and_process(self) -> None:
        """Download and process the remote library index."""

        self._bg_downloader.start()

        # Register the timer for periodic message passing between the main and
        # background processes.
        if not bpy.app.timers.is_registered(self.on_timer_event):
            bpy.app.timers.register(
                self.on_timer_event,
                first_interval=self._DOWNLOAD_POLL_INTERVAL,
                persistent=True,
            )
            # Double-check the registration worked, see #139720 for details.
            assert bpy.app.timers.is_registered(self.on_timer_event)

        # Kickstart the download process by downloading the remote asset meta file:
        self._queue_download(
            listing_common.ASSET_TOP_METADATA_FILENAME,
            http_metadata.safe_to_unsafe_filename(listing_common.ASSET_TOP_METADATA_FILENAME),
            self.parse_asset_lib_metadata,
        )

    def parse_asset_lib_metadata(self,
                                 http_req_descr: http_dl.RequestDescription,
                                 unsafe_local_file: Path,
                                 ) -> None:
        metadata, used_unsafe_file = self._parse_api_model(unsafe_local_file, api_models.AssetLibraryMeta)

        # Show what we downloaded.
        logger.info("    API versions      : %s", metadata.api_versions)
        logger.info("    Asset Library Name: %s", metadata.name)
        if metadata.contact:
            logger.info(
                "    Contact           : %s | %s | %s",
                metadata.contact.name,
                metadata.contact.url,
                metadata.contact.email,
            )

        # Check API version.
        api_key = "v{:d}".format(listing_common.API_VERSION)
        try:
            main_index_url = metadata.api_versions[api_key]
        except KeyError:
            # Abort, the API version for this Blender is not supported by the library.
            library_versions = ", ".join(metadata.api_versions.keys())
            msg = "This asset library supports API versions {!s}, but this Blender uses version {!s}".format(
                library_versions,
                listing_common.API_VERSION,
            )
            self.report({'ERROR'}, msg)
            logger.error(msg)

            self._status = DownloadStatus.FAILED
            self._bg_downloader.shutdown()
            return

        # The file passed validation, so can be marked safe.
        if used_unsafe_file:
            self._rename_to_safe(unsafe_local_file)

        self._library_meta = metadata

        # Download the asset index.
        main_index_filepath = listing_common.api_versioned(listing_common.ASSET_INDEX_JSON_FILENAME)
        self._queue_download(
            main_index_url,
            http_metadata.safe_to_unsafe_filename(main_index_filepath),
            self.parse_asset_lib_index,
        )

    def parse_asset_lib_index(self,
                              http_req_descr: http_dl.RequestDescription,
                              unsafe_local_file: Path,
                              ) -> None:
        asset_index, used_unsafe_file = self._parse_api_model(unsafe_local_file, api_models.AssetLibraryIndexV1)

        page_urls = asset_index.page_urls or []
        logger.info("    Schema version    : %s", asset_index.schema_version)
        logger.info("    Asset count       : %d", asset_index.asset_count)
        logger.info("    Pages             : %d", len(page_urls))

        # The file passed validation, so can be marked safe.
        if used_unsafe_file:
            local_file = self._rename_to_safe(unsafe_local_file)
        else:
            local_file = unsafe_local_file

        # Write the catalogs file. Even when there are no catalogs, this should
        # be done, because catalogs might have existed previously.
        assert self._library_meta, "By now the asset library metadata should be known"
        catalogs_file = self._locator.catalogs_file
        logger.info("Writing catalogs to %s", catalogs_file)
        asset_catalogs.write(asset_index.catalogs or [], catalogs_file, self._library_meta)

        # Construct a "processed" version of the asset index file. This will be
        # what Blender reads, and thus it should reference local files, and not
        # the URLs where they were downloaded from.
        processed_asset_index = copy.deepcopy(asset_index)
        # Catalogs are not read from here, but from the above-generated file. So
        # no need to store them again.
        processed_asset_index.catalogs = []
        # The code below will re-fill the list with the relative file paths.
        processed_asset_index.page_urls = []

        # Download the asset pages.
        self._num_asset_pages_pending = len(page_urls)
        for page_index, page_url in enumerate(page_urls):
            # These URLs may be absolute or they may be relative. In any case,
            # do not assume that they can be used direclty as local filesystem path.
            local_path = listing_common.api_versioned(f"assets-{page_index:05}.json")
            download_to = self._queue_download(
                page_url,
                http_metadata.safe_to_unsafe_filename(local_path),
                self.on_asset_page_downloaded)

            self._referenced_local_files.append(http_metadata.unsafe_to_safe_filename(download_to))
            processed_asset_index.page_urls.append(local_path.as_posix())

        # Save the processed index to a JSON file for Blender to pick up.
        json_path = local_file.with_suffix(".processed{!s}".format(local_file.suffix))

        as_json = self._parser.dumps(processed_asset_index)
        json_path.parent.mkdir(exist_ok=True, parents=True)
        with json_path.open("w") as json_file:
            json_file.write(as_json)

        # Meta files are ready to be picked up by the asset system.
        if self._on_metafiles_done_callback:
            self._on_metafiles_done_callback(self)

    def on_asset_page_downloaded(self,
                                 http_req_descr: http_dl.RequestDescription,
                                 unsafe_local_file: Path,
                                 ) -> None:
        asset_page, used_unsafe_file = self._parse_api_model(unsafe_local_file, api_models.AssetLibraryIndexPageV1)

        self._queue_thumbnail_downloads(asset_page)

        # The file passed validation, so can be marked safe.
        if used_unsafe_file:
            local_file = self._rename_to_safe(unsafe_local_file)
        else:
            local_file = http_metadata.unsafe_to_safe_filename(unsafe_local_file)

        self._num_asset_pages_pending -= 1
        assert self._num_asset_pages_pending >= 0

        if self._on_page_done_callback:
            self._on_page_done_callback(self)

        logger.debug("Asset index page downloaded: %s", local_file)

        if self._num_asset_pages_pending > 0:
            # Wait until all files have downloaded.
            self.report(
                {'INFO'},
                "Asset library index page downloaded; needs {:d} more".format(
                    self._num_asset_pages_pending))
            return

        # Remove any dangling pages of assets (downloaded before, no longer referenced).
        asset_page_dir = self._locator.local_path / listing_common.API_VERSIONED_SUBDIR
        # TODO: when upgrading to Python 3.12+, add `case_sensitive=False` to the glob() call.
        for asset_page_file in asset_page_dir.glob("assets-*.json"):
            abs_path = asset_page_dir / asset_page_file
            if abs_path in self._referenced_local_files:
                continue
            abs_path.unlink()

        self.report({'INFO'}, "Asset library index downloaded")
        self._shutdown_if_done()

    def on_thumbnail_downloaded(self,
                                http_req_descr: http_dl.RequestDescription,
                                local_file: Path,
                                ) -> None:
        try:
            # Check whether the file was actually an image.
            assert http_req_descr.response_headers
            content_type = http_req_descr.response_headers.get('content-type', "")

            # Only check the content type if the server sends it back. Otherwise
            # just trust that it's valid. For example, when sending a `304 Not
            # Modified`, the server may actually skip the Content-Type header.
            if content_type and not content_type.startswith('image/'):
                logger.warning("Thumbnail URL %r has content type %r, expected an image",
                               http_req_descr.url, content_type)
                # TODO: mark as 'failed' so that this file isn't repeatedly
                # downloaded and rejected. For now I'll just keep the file
                # around, so that at least the timestamping works to prevent
                # hammering the server.

            # Indicate to a future run that we just confirmed this file is still fresh.
            local_file.touch()

            # Poke Blender so it knows there's a thumbnail update.
            wm: bpy.types.WindowManager = bpy.context.window_manager
            wm.asset_library_status_ping_loaded_new_preview(self.remote_url, str(local_file))
        finally:
            self._shutdown_if_done()

    def _on_thumbnail_failed(
        self,
        http_req_descr: http_dl.RequestDescription,
        local_file: Path,
        error: Exception,
    ) -> None:
        """Create a 'failure' marker file, to prevent re-downloading this over and over again."""
        try:
            failure_path = self._locator.thumbnail_failed_path(local_file)
        except ValueError:
            # Just log the error. There is no need to disrupt the download process any further.
            logger.exception("constructing 'failure' path from thumbnail path")
            return

        # The failure path existing, with a timestamp newer than the local file
        # (if that exists in the first place), should be enough to prevent
        # further downloads. It should also prevent Blender from trying to
        # render the thumbnail.
        failure_path.parent.mkdir(parents=True, exist_ok=True)
        failure_path.touch(exist_ok=True)

    def _shutdown_if_done(self) -> None:
        if self._num_asset_pages_pending == 0 and self._bg_downloader.all_downloads_done:
            # Done downloading everything, let's shut down.
            self.shutdown(DownloadStatus.FINISHED_SUCCESSFULLY)

    def _parse_api_model(self, unsafe_local_file: Path,
                         api_model: Type[json_parsing.APIModel]) -> tuple[json_parsing.APIModel, bool]:
        """Use the OpenAPI schema to parse & validate a JSON file.

        :param unsafe_local_file: Path to load, parse, and validate. If this
            file does not exist, the 'safe' version of the filepath is tried. If
            that doesn't exist either, a FileNotFoundError is raised.
        :param api_model: the data class itself, to use for parsing & validating.

        :returns: the parsed+validated data, and a boolean that indicates
            whether the input file was used directly (True), or its 'safe'
            version (False).
        """
        if unsafe_local_file.exists():
            path_to_load = unsafe_local_file
            used_unsafe_file = True
        else:
            safe_local_file = http_metadata.unsafe_to_safe_filename(unsafe_local_file)
            if safe_local_file == unsafe_local_file:
                # There is no 'safe' version of this path.
                raise FileNotFoundError(unsafe_local_file)
            if not safe_local_file.exists():
                raise ValueError("Both {!s} and {!s} do not exist, what was downloaded?".format(
                    unsafe_local_file, safe_local_file))
            path_to_load = safe_local_file
            used_unsafe_file = False

        logger.info("Validating %s", path_to_load)
        json_data = path_to_load.read_bytes()
        parsed_data = self._parser.parse_and_validate(api_model, json_data)

        # The file has been parsed & validated, so 'touch' it to let other
        # Blender processes know when this was last downloaded/validated.
        path_to_load.touch()

        return parsed_data, used_unsafe_file

    def _rename_to_safe(self, unsafe_filepath: Path) -> Path:
        safe_filepath = http_metadata.unsafe_to_safe_filename(unsafe_filepath)

        if safe_filepath == unsafe_filepath:
            raise ValueError("filepath cannot be transformed to 'safe' filepath: {!s}".format(unsafe_filepath))

        # AFAIK on Windows you cannot atomically overwrite a file by renaming.
        safe_filepath.unlink(missing_ok=True)
        unsafe_filepath.rename(safe_filepath)
        return safe_filepath

    def _on_callback_error(
            self,
            http_req_descr: http_dl.RequestDescription,
            local_file: Path,
            exception: Exception) -> None:
        logger.exception(
            "exception while handling downloaded file ({!r}, saved to {!r})".format(
                http_req_descr, local_file))
        self.report({'ERROR'}, "Asset library index had an issue, download aborted")
        self.shutdown(DownloadStatus.FAILED)

    def _queue_download(self, relative_url: str, download_to_path: Path | str,
                        on_done: Callable[[http_dl.RequestDescription, Path], None]) -> Path:
        """Queue up this download, returning the path to which it will be downloaded."""
        remote_url = urllib.parse.urljoin(self._locator.remote_url, relative_url)
        download_to_path = self._locator.local_path / download_to_path

        self._bg_downloader.queue_download(remote_url, download_to_path, on_done)

        return download_to_path

    def _queue_thumbnail_downloads(self, asset_page: api_models.AssetLibraryIndexPageV1) -> None:
        """Queue the download of all the thumbnails of all the assets in this page."""

        # TODO: after all thumbnails of all asset listing pages have been
        # processed, delete the ones that are no longer referenced.

        # Create a lookup table for the asset files.
        file_map: dict[str, api_models.FileV1] = {file.path: file for file in asset_page.files}

        num_queued_thumbs = 0
        for asset in asset_page.assets:
            if not asset.thumbnail_url:
                # TODO: delete any existing thumbnail?
                continue

            asset_file = file_map[asset.file]

            thumbnail_path = self._locator.thumbnail_download_path(asset, asset_file)
            if not thumbnail_path:
                continue

            failure_path = self._locator.thumbnail_failed_path(thumbnail_path)

            # See if this download should happen at all.
            should_download = _compare_thumbnail_filepath_timestamps(thumbnail_path, failure_path)
            if not should_download:
                continue

            # Queue the thumbnail download, and remember that this is a
            # non-critical download (so that errors don't cancel the entire download
            # queue).
            download_path = self._queue_download(asset.thumbnail_url, thumbnail_path, self.on_thumbnail_downloaded)
            self._noncritical_downloads.add(download_path)

            num_queued_thumbs += 1

            # Because there can be a _lot_ of thumbnails in one page, call the
            # update function periodically. Without this, the pipe between
            # processes can deadlock.
            if num_queued_thumbs % 100 == 0:
                self._bg_downloader.update()

    # TODO: implement this in a more useful way:
    def report(self, level: set[str], message: str) -> None:
        # logger.info("Report: {:s}: {:s}".format("/".join(level), message))
        if 'ERROR' in level:
            self._error_message = message

    def shutdown(self, status: DownloadStatus) -> None:
        """Stop the background downloader, update the status and call the 'done' callback."""

        self._status = status

        # The timer is no longer necessary, the bg_downloader.shutdown() call
        # takes care of the last queued messages.
        if bpy.app.timers.is_registered(self.on_timer_event):
            bpy.app.timers.unregister(self.on_timer_event)

        try:
            # Only report if this is actually triggering a shutdown. If that was
            # already triggered somehow, don't bother.
            if not self._bg_downloader.is_shutdown_requested:
                # It may be tempting to call self.report(...) here, and report on the
                # cancellation. However, this should be done by the caller, when they know
                # of the reason of the cancellation and thus can provide more info.
                num_pending = self._bg_downloader.num_pending_downloads
                if num_pending:
                    logger.warning("Shutting down background downloader, %d downloads pending", num_pending)

            self._bg_downloader.shutdown()
        finally:
            # Regardless of whether the shutdown had some issues, the timer has
            # been unregistered, so there will be no more message handling, and
            # so for all intents and purposes, the downloader is done.
            self._on_done_callback(self)

    def on_timer_event(self) -> float:
        try:
            self._bg_downloader.update()
        except http_dl.BackgroundProcessNotRunningError:
            logger.error("Background downloader subprocess died, aborting.")
            self.shutdown(DownloadStatus.FAILED)
            return 0  # Deactivate the timer.
        except Exception:
            logger.exception(
                "Unexpected error downloading remote asset library ilisting from %s to %s",
                self._locator.remote_url,
                self._locator.local_path)

        self._on_update_callback(self)

        return self._DOWNLOAD_POLL_INTERVAL

    @property
    def remote_url(self) -> str:
        return self._locator.remote_url

    @property
    def status(self) -> DownloadStatus:
        return self._status

    @property
    def error_message(self) -> str:
        return self._error_message

    # Below here: CachingDownloadReporter functions:

    def download_starts(self, http_req_descr: http_dl.RequestDescription) -> None:
        self.report({'INFO'}, "Download starting: {}".format(http_req_descr.url))
        logger.debug("Download starting: %s", http_req_descr)

    def already_downloaded(
        self,
        http_req_descr: http_dl.RequestDescription,
        local_file: Path,
    ) -> None:
        logger.debug("Download unnecessary, file already downloaded: %s", http_req_descr.url)

    def download_error(
        self,
        http_req_descr: http_dl.RequestDescription,
        local_file: Path,
        error: Exception,
    ) -> None:
        if isinstance(error, http_dl.DownloadCancelled):
            if self._num_asset_pages_pending:
                self.report({'WARNING'}, "Cancelled {} pending download".format(self._num_asset_pages_pending))
            logger.warning("Download cancelled: %s", http_req_descr)
            self.shutdown(DownloadStatus.FAILED)
            return

        if local_file in self._noncritical_downloads:
            logger.warning("Could not download non-critical file %s: %s", http_req_descr, error)
            # TODO: distinguish between "thumbnail" and "other non-critical". For now
            # the only non-critical downloads are the thumbnails, though.
            self._on_thumbnail_failed(http_req_descr, local_file, error)

            # This could have been the last to-be-downloaded file, so better
            # check if there's anything left to do.
            self._shutdown_if_done()
            return

        self.report({'ERROR'}, "Error downloading {}: {}".format(http_req_descr.url, error))
        logger.error("Error downloading %s: %s", http_req_descr, error)
        self.shutdown(DownloadStatus.FAILED)

    def download_progress(
        self,
        http_req_descr: http_dl.RequestDescription,
        content_length_bytes: int,
        downloaded_bytes: int,
    ) -> None:
        percentage = downloaded_bytes / content_length_bytes * 100
        self.report({'INFO'}, "File download progress: {:.0f}%".format(percentage))
        # logger.info("File download progress: %.0f%%", percentage)

    def download_finished(
        self,
        http_req_descr: http_dl.RequestDescription,
        local_file: Path,
    ) -> None:
        if local_file in self._noncritical_downloads:
            # Don't spam the reports & logs with all the thumbnail download.
            logger.debug("Download finished: %s", http_req_descr)
        else:
            self.report({'INFO'}, "Download finished: {}".format(http_req_descr.url))
            logger.info("Download finished: %s", http_req_descr)


def _sanitize_path_from_url(urlpath: PurePosixPath | str) -> PurePosixPath:
    """Safely convert some path (assumed from a URL) to a relative path.

    Directory up-references ('/../') are removed.

    >>> _sanitize_path_from_url(PurePosixPath('/normal/path/as/expected.blend'))
    PurePosixPath('normal/path/as/expected.blend')

    >>> _sanitize_path_from_url(PurePosixPath(''))
    PurePosixPath('.')

    >>> _sanitize_path_from_url(PurePosixPath('/path/sub/../filename.blend'))
    PurePosixPath('path/filename.blend')

    >>> _sanitize_path_from_url('/path/sub%2F%2E%2e/filename.blend')
    PurePosixPath('path/filename.blend')

    >>> _sanitize_path_from_url('path/filename.blend')
    PurePosixPath('path/filename.blend')

    >>> _sanitize_path_from_url(PurePosixPath('/longer/faster/path/../../filename.blend'))
    PurePosixPath('longer/filename.blend')

    >>> _sanitize_path_from_url(PurePosixPath('/faster/path/../../filename.blend'))
    PurePosixPath('filename.blend')

    >>> _sanitize_path_from_url('/faster/path/../../filename.blend')
    PurePosixPath('filename.blend')

    >>> _sanitize_path_from_url(PurePosixPath('/../../../../../etc/passwd'))
    PurePosixPath('etc/passwd')
    """

    if isinstance(urlpath, str):
        # Assumption: this string comes directly from urllib.parse.urlsplit(url).path
        unquoted = urllib.parse.unquote(urlpath)
        normalized = unicodedata.normalize('NFKC', unquoted)
        urlpath = PurePosixPath(normalized)

    # The URL could have entries like `..` in there, which should be removed.
    # However, PurePosixPath does not have functionality for this (for good
    # reason), but since this is about URL paths and not real filesystem paths
    # (yet) we can just go ahead and do this ourselves.

    parts = list(urlpath.parts)

    if urlpath.is_absolute():
        parts = parts[1:]

    i = 0
    while i < len(parts):
        if parts[i] != '..':
            i += 1
            continue

        if i == 0:
            parts = parts[1:]
            continue

        parts = parts[:i - 1] + parts[i + 1:]
        i -= 1

    return PurePosixPath(*parts)


def _compare_thumbnail_filepath_timestamps(thumbnail_path: Path, failure_path: Path) -> bool:
    """Check the thumbnail and 'failure' file timestamps.

    If the thumbnail already exists, and is relatively new, it won't be
    re-downloaded. If the failure path exists, and it's newer than the
    thumbnail, the thumbnail also shouldn't be downloaded.

    :return: whether this thumbnail should be downloaded or not.
    """

    now = time.time()

    # Timestamp that indicates 'file does not exist'. Having this a valid number
    # but smaller than any actual timestamp makes the logic in this function a
    # bit easier.
    DOES_NOT_EXIST = 0
    try:
        failure_mtime = failure_path.stat().st_mtime
        failure_age_mins = (now - failure_mtime) // 60
    except FileNotFoundError:
        failure_mtime = DOES_NOT_EXIST
        failure_age_mins = DOES_NOT_EXIST

    try:
        thumbnail_mtime = thumbnail_path.stat().st_mtime
    except FileNotFoundError:
        # The thumbnail does not exist, so it should be downloaded unless the
        # failure file tells us not to.
        return failure_mtime == DOES_NOT_EXIST or failure_age_mins > THUMBNAIL_FAILED_REDOWNLOAD_MINS

    # If the failure file newer than the thumbnail, its age determines whether
    # another attempt at downloading is ok.
    if failure_mtime > thumbnail_mtime:
        return failure_age_mins > THUMBNAIL_FAILED_REDOWNLOAD_MINS

    # Check the age of the existing thumbnail file. If it's too young, don't
    # try a conditional download to avoid DOSsing the server.
    #
    # To illustrate an experiment on my (Sybren) computer: doing ~1700 requests
    # via Blender's conditional downloader, and getting `304 Not Modified` in
    # return from a fairly efficient web server, still took about 2 seconds.
    thumbnail_age_mins = (now - thumbnail_mtime) / 60
    return thumbnail_age_mins > THUMBNAIL_FRESH_DURATION_MINS


if __name__ == '__main__':
    import doctest

    # Run the doctests. Note: these only work on Posix systems for now, due to
    # how pathlib.Path is an alias for either PosixPath or WindowsPath,
    # depending on the platform.
    doctest.testmod(optionflags=doctest.ELLIPSIS)
