# SPDX-FileCopyrightText: 2025 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

from __future__ import annotations

import logging
import time
import urllib.parse
import hashlib
from pathlib import Path, PurePosixPath
from typing import Callable, TypeAlias, TypeVar, Type

import bpy
import pydantic

from _bpy_internal.http import downloader as http_dl
from _bpy_internal.assets.remote_library_index import blender_asset_library_openapi as api_models
from _bpy_internal.assets.remote_library_index import index_common
from _bpy_internal.assets.remote_library_index import http_metadata
from _bpy_internal.assets.remote_library_index import asset_catalogs

logger = logging.getLogger(__name__)

PydanticModel = TypeVar("PydanticModel", bound=pydantic.BaseModel)

# TODO: discuss & adjust this value, as this was just picked more or less randomly:
THUMBNAIL_FRESH_DURATION_MINS = 60
"""If a thumbnail was downloaded this many seconds ago, do not download it again.

If the local timestamp of the thumbnail file indicates that it was downloaded
less that this duration ago, do not attempt a re-download. Even though the
downloader supports conditional downloads, and won't actually re-download when
the server responds with a `304 Not Modified`, doing that request still takes
time and requires resources on the server.
"""


class RemoteAssetListingDownloader:
    _remote_url: str
    _local_path: Path

    OnDoneCallback: TypeAlias = Callable[['RemoteAssetListingDownloader'], None]
    _on_done_callback: OnDoneCallback

    _bgdownloader: http_dl.BackgroundDownloader
    _num_asset_pages_pending: int

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

    def __init__(
        self,
        remote_url: str,
        local_path: Path | str,
        on_done_callback: OnDoneCallback,
    ) -> None:
        """Create a downloader for the remote index of this library.

        :param remote_url: Base URL of the remote asset library server. See
           blender_asset_library_openapi.yaml for the files downloaded from
           there.

        :param local_path: The directory to download the index files to.

        :param on_done_callback: called with one parameter (this
            RemoteAssetListingDownloader) whenever the downloader is "done".

            Here "done" does not imply "successful", as cancellations, network
            errors, or other issues can cause things to abort. In that case,
            this function is still called.
        """

        self._remote_url = remote_url
        self._local_path = Path(local_path)
        self._on_done_callback = on_done_callback

        self._num_asset_pages_pending = 0
        self._referenced_local_files = []
        self._library_meta = None

        # Work around a limitation of Blender, see bug report #139720 for details.
        self.on_timer_event = self.on_timer_event  # type: ignore[method-assign]

        self._http_metadata_provider = http_metadata.ExtraFileMetadataProvider(
            http_dl.MetadataProviderFilesystem(
                cache_location=self._local_path / "_local-meta-cache"))

        # Create the background downloader object now, so that it
        # (hypothetically in some future) can be adjusted before the actual
        # downloading begins.
        self._bg_downloader = http_dl.BackgroundDownloader(
            options=http_dl.DownloaderOptions(
                metadata_provider=self._http_metadata_provider,
                http_headers={'Accept': 'application/json'},
            ),
            on_callback_error=self._on_callback_error,
        )
        self._bg_downloader.add_reporter(self)

    def __repr__(self) -> str:
        return "{!s}(remote_url={!r}, local_path={!r})".format(type(self), self._remote_url, self._local_path)

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
            index_common.ASSET_TOP_METADATA_FILENAME,
            http_metadata.safe_to_unsafe_filename(index_common.ASSET_TOP_METADATA_FILENAME),
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
        api_key = "v{:d}".format(index_common.API_VERSION)
        try:
            main_index_url = metadata.api_versions[api_key]
        except KeyError:
            # Abort, the API version for this Blender is not supported by the library.
            library_versions = ", ".join(metadata.api_versions.keys())
            msg = "This asset library supports API versions {!s}, but this Blender uses version {!s}".format(
                library_versions,
                index_common.API_VERSION,
            )
            self.report({'ERROR'}, msg)
            logger.error(msg)

            self._bg_downloader.shutdown()
            return

        # The file passed validation, so can be marked safe.
        if used_unsafe_file:
            self._rename_to_safe(unsafe_local_file)

        self._library_meta = metadata

        # Download the asset index.
        main_index_filepath = index_common.api_versioned(index_common.ASSET_INDEX_JSON_FILENAME)
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
            self._rename_to_safe(unsafe_local_file)

        # Write the catalogs file. Even when there are no catalogs, this should
        # be done, because catalogs might have existed previously.
        assert self._library_meta, "By now the asset library metadata should be known"
        catalogs_file = self._local_path / "blender_assets.cats.txt"
        logger.info("Writing catalogs to %s", catalogs_file)
        asset_catalogs.write(asset_index.catalogs or [], catalogs_file, self._library_meta)

        # Download the asset pages.
        self._num_asset_pages_pending = len(page_urls)
        for page_index, page_url in enumerate(page_urls):
            # These URLs may be absolute or they may be relative. In any case,
            # do not assume that they can be used direclty as local filesystem path.
            local_path = index_common.api_versioned(f"assets-{page_index:05}.json")
            download_to = self._queue_download(
                page_url,
                http_metadata.safe_to_unsafe_filename(local_path),
                self.on_asset_page_downloaded)

            self._referenced_local_files.append(http_metadata.unsafe_to_safe_filename(download_to))

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

        logger.debug("Asset index page downloaded: %s", local_file)

        if self._num_asset_pages_pending > 0:
            # Wait until all files have downloaded.
            self.report(
                {'INFO'},
                "Asset library index page downloaded; needs {:d} more".format(
                    self._num_asset_pages_pending))
            return

        # Remove any dangling pages of assets (downloaded before, no longer referenced).
        asset_page_dir = self._local_path / index_common.API_VERSIONED_SUBDIR
        # TODO: when upgrading to Python 3.12+, add `case_sensitive=False` to the glob() call.
        for asset_page_file in asset_page_dir.glob("assets-*.json"):
            abs_path = asset_page_dir / asset_page_file
            if abs_path in self._referenced_local_files:
                continue
            abs_path.unlink()

        self.report({'INFO'}, "Asset library index downloaded")

        if self._bg_downloader.all_downloads_done:
            # There were no new downloads queued for this page (like thumbnails), so we're done.
            self.shutdown()

    def on_thumbnail_downloaded(self,
                                http_req_descr: http_dl.RequestDescription,
                                local_file: Path,
                                ) -> None:
        # Indicate to a future run that we just confirmed this file is still fresh.
        local_file.touch()

        # TODO: maybe poke the asset browser to load this thumbnail? Not sure if it's even necessary.
        # TODO: convert the file to the right format

        if self._num_asset_pages_pending == 0 and self._bg_downloader.all_downloads_done:
            # Done downloading everything, let's shut down.
            self.shutdown()

    def _parse_api_model(self, unsafe_local_file: Path, api_model: Type[PydanticModel]) -> tuple[PydanticModel, bool]:
        """Use a Pydantic model to parse & validate a JSON file.

        :param unsafe_local_file: Path to load, parse, and validate. If this
            file does not exist, the 'safe' version of the filepath is tried. If
            that doesn't exist either, a FileNotFoundError is raised.
        :param api_model: the Pydantic class itself, to use for parsing & validating.

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

        logger.info("Parsing %s", path_to_load)
        json_data = path_to_load.read_bytes()
        parsed_data = api_model.model_validate_json(json_data)

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
        self.shutdown()

    def _queue_download(self, relative_url: str, download_to_path: Path | str,
                        on_done: Callable[[http_dl.RequestDescription, Path], None]) -> Path:
        """Queue up this download, returning the path to which it will be downloaded."""
        remote_url = urllib.parse.urljoin(self._remote_url, relative_url)
        download_to_path = self._local_path / download_to_path

        self._bg_downloader.queue_download(remote_url, download_to_path, on_done)

        return download_to_path

    def _queue_thumbnail_downloads(self, asset_page: api_models.AssetLibraryIndexPageV1) -> None:
        """Queue the download of all the thumbnails of all the assets in this page."""

        # TODO: after all thumbnails of all asset listing pages have been
        # processed, delete the ones that are no longer referenced.

        num_queued_thumbs = 0
        for asset in asset_page.assets:
            if not asset.thumbnail_url:
                # TODO: delete any existing thumbnail?
                continue

            # TODO: check if there already is a local version of this asset. If so,
            # the thumbnail of that asset takes presence over any remote thumbnail.
            # and we don't have to bother downloading it.

            thumbnail_path = self._thumbnail_download_path(asset)
            if not thumbnail_path:
                continue

            # Check the age of the downloaded thumbnail. If it's too young,
            # don't try a conditional download to avoid DOSsing the server.
            if thumbnail_path.exists():
                stat = thumbnail_path.stat()
                now = time.time()
                if (now - stat.st_mtime) / 60 < THUMBNAIL_FRESH_DURATION_MINS:
                    continue

            self._queue_download(asset.thumbnail_url, thumbnail_path, self.on_thumbnail_downloaded)
            num_queued_thumbs += 1

            # Because there can be a _lot_ of thumbnails in one page, call the
            # update function periodically. Without this, the pipe between
            # processes can deadlock.
            if num_queued_thumbs % 100 == 0:
                self._bg_downloader.update()

    def _thumbnail_download_path(self, asset: api_models.AssetV1) -> Path | None:
        """Construct the download path suitable for this asset's thumbnail.

        This assumes that the URL already ends in the correct extension. This is
        not always the case, and using the content type in the HTTP response
        headers is technically a possibility. Keeping this limitation makes the
        code considerably simpler, as now the download filename is known
        beforehand.
        """

        assert asset.thumbnail_url

        abs_url = urllib.parse.urljoin(self._remote_url, asset.thumbnail_url)

        try:
            split_url = urllib.parse.urlsplit(abs_url)
        except ValueError as ex:
            logger.warning("thumbnail has invalid URL (%s): %s", abs_url, ex)
            return None

        url_path = PurePosixPath(urllib.parse.unquote(split_url.path))
        url_suffix = url_path.suffix.lower()
        if url_suffix not in {'.webp', '.png'}:
            logger.warning(
                "thumbnail url (%s) does not end in .webp or .png (but in %s), ignoring",
                abs_url,
                url_suffix)
            return None

        # Construct a directory & filename by hashing:
        hash = hashlib.sha256(f"{asset.archive_url}/{asset.id_type}/{asset.name}".encode()).hexdigest()
        relative_path = Path(hash[:2], hash[2:]).with_suffix(url_path.suffix)

        # Return the absolute path.
        absolute_path = self._local_path / "_thumbs" / relative_path
        return absolute_path

    # TODO: implement this in a more useful way:
    def report(self, level: set[str], message: str) -> None:
        # logger.info("Report: {:s}: {:s}".format("/".join(level), message))
        pass

    def shutdown(self) -> None:
        """Stop the background downloader and call the 'done' callback."""

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
            self.shutdown()
            return 0  # Deactivate the timer.
        except Exception:
            logger.exception(
                "Unexpected error downloading remote asset library ilisting from %s to %s",
                self._remote_url,
                self._local_path)

        return self._DOWNLOAD_POLL_INTERVAL

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
        error: Exception,
    ) -> None:
        if isinstance(error, http_dl.DownloadCancelled):
            if self._num_asset_pages_pending:
                self.report({'WARNING'}, "Cancelled {} pending download".format(self._num_asset_pages_pending))
            logger.warning("Download cancelled: %s", http_req_descr)
        else:
            self.report({'ERROR'}, "Error downloading {}: {}".format(http_req_descr.url, error))
            logger.warning("Error downloading %s: %s", http_req_descr, error)

        self.shutdown()

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
        self.report({'INFO'}, "Download finished: {}".format(http_req_descr.url))
        logger.info("Download finished: %s", http_req_descr)
