# SPDX-FileCopyrightText: 2025 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

from __future__ import annotations

import logging
import urllib.parse
from pathlib import Path
from typing import Callable, TypeAlias

import bpy

from _bpy_internal.http import downloader as http_dl
from _bpy_internal.assets.remote_library_index import blender_asset_library_openapi as api_models
from _bpy_internal.assets.remote_library_index import index_common

logger = logging.getLogger(__name__)


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

        # Work around a limitation of Blender, see bug report #139720 for details.
        self.on_timer_event = self.on_timer_event

        self._http_metadata_provider = http_dl.MetadataProviderFilesystem(
            cache_location=self._local_path / "_local-meta-cache")

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
            index_common.ASSET_TOP_METADATA_FILENAME,
            self.parse_asset_lib_metadata,
        )

    def parse_asset_lib_metadata(self,
                                 http_req_descr: http_dl.RequestDescription,
                                 local_file: Path,
                                 ) -> None:
        logger.info("Parsing %s", local_file)
        json_data = local_file.read_bytes()
        metadata = api_models.AssetLibraryMeta.model_validate_json(json_data)

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

        # Download the asset index.
        main_index_filepath = index_common.api_versioned(index_common.ASSET_INDEX_JSON_FILENAME)
        self._queue_download(
            main_index_url,
            main_index_filepath,
            self.parse_asset_lib_index,
        )

    def parse_asset_lib_index(self,
                              http_req_descr: http_dl.RequestDescription,
                              local_file: Path,
                              ) -> None:
        json_data = local_file.read_bytes()
        asset_index = api_models.AssetLibraryIndexV1.model_validate_json(json_data)

        page_urls = asset_index.page_urls or []

        logger.info("    Schema version    : %s", asset_index.schema_version)
        logger.info("    Asset count       : %d", asset_index.asset_count)
        logger.info("    Pages             : %d", len(page_urls))

        # Download the asset pages.
        self._num_asset_pages_pending = len(page_urls)
        referenced_local_files: list[Path] = []
        for page_index, page_url in enumerate(page_urls):
            # These URLs may be absolute or they may be relative. In any case,
            # do not assume that they can be used direclty as local filesystem path.
            local_path = index_common.api_versioned(f"assets-{page_index:05}.json")
            download_to = self._queue_download(page_url, local_path, self.on_asset_page_downloaded)

            referenced_local_files.append(download_to)

        # Remove any dangling pages of assets (downloaded before, no longer referenced).
        asset_page_dir = self._local_path / index_common.API_VERSIONED_SUBDIR
        # TODO: when upgrading to Python 3.12+, add `case_sensitive=False` to the glob() call.
        for asset_page_file in asset_page_dir.glob("assets-*.json"):
            abs_path = asset_page_dir / asset_page_file
            if abs_path in referenced_local_files:
                continue
            abs_path.unlink()

    def on_asset_page_downloaded(self,
                                 http_req_descr: http_dl.RequestDescription,
                                 local_file: Path,
                                 ) -> None:
        self._num_asset_pages_pending -= 1
        assert self._num_asset_pages_pending >= 0

        logger.info("Asset index page downloaded: %s", local_file)

        if self._num_asset_pages_pending > 0:
            # Wait until all files have downloaded.
            self.report(
                {'INFO'},
                "Asset library index page downloaded; needs {:d} more".format(
                    self._num_asset_pages_pending))
            return

        self.report({'INFO'}, "Asset library index downloaded")
        self.shutdown()

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

    def _queue_download(self, relative_url: str, relative_path: Path | str,
                        on_done: Callable[[http_dl.RequestDescription, Path], None]) -> Path:
        """Queue up this download, returning the path to which it will be downloaded."""
        remote_url = urllib.parse.urljoin(self._remote_url, relative_url)
        download_to_path = self._local_path / relative_path

        self._bg_downloader.queue_download(remote_url, download_to_path, on_done)

        return download_to_path

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
                    logger.info("Shutting down background downloader, %d downloads pending", num_pending)

            self._bg_downloader.shutdown()
        finally:
            # Regardless of whether the shutdown had some issues, the timer has
            # been unregistered, so there will be no more message handling, and
            # so for all intents and purposes, the downloader is done.
            self._call_on_done_callback()

    def _call_on_done_callback(self) -> None:
        """Call the on-done callback function, if there is one."""
        if not self._on_done_callback:
            return
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
        logger.info("Download starting: %s", http_req_descr)

    def already_downloaded(
        self,
        http_req_descr: http_dl.RequestDescription,
        local_file: Path,
    ) -> None:
        logger.info("Download unnecessary, file already downloaded: %s", http_req_descr.url)

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
