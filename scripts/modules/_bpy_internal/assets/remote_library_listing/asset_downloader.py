# SPDX-FileCopyrightText: 2025 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

from __future__ import annotations

__all__ = [
    "download_asset",
    "downloader_status",
    "DownloadStatus",
]

import enum
import logging
import urllib.parse
from pathlib import Path
from typing import Callable, TypeAlias

import bpy

from _bpy_internal.http import downloader as http_dl
from _bpy_internal.assets.remote_library_listing.listing_downloader import RemoteAssetListingLocator

logger = logging.getLogger(__name__)


_asset_downloaders: dict[str, AssetDownloader] = {}

_asset_downloaders_last_status: dict[str, DownloadStatus] = {}
"""Last-known downloader status, for those downloaders that have stopped already.

_asset_downloaders contains only the running downloaders, so when they're
finished, their status can be obtained here.
"""


def download_asset(asset_library_url: str, asset_library_local_path: Path, asset_url: str, save_to: Path) -> None:
    """Download an asset to a file on disk.

    :param asset_library_url: Root URL of the remote asset library. Used as an
        identifier of this library (to create a downloader per library), as well
        as for resolving relative URLs.

    :param asset_library_local_path: Root path of the local asset cache. Used to
        resolve relative `save_to` paths, but also to find the HTTP metadata
        cache for this asset library (for conditional downloads).

    :param asset_url: the URL to download. Can be absolute or relative.

    :param save_to: the path on disk where to download to. While the download is
        pending, ".part" will be appended to the filename. When the download
        finishes succesfully, it is renamed to the final path.
    """
    try:
        downloader = _asset_downloaders[asset_library_url]
    except KeyError:
        downloader = AssetDownloader(asset_library_url, asset_library_local_path,
                                     lambda x: None,
                                     _download_done,
                                     _destroy_asset_downloader)
        downloader.start()
        _asset_downloaders[asset_library_url] = downloader
        _asset_downloaders_last_status.pop(asset_library_url, None)

    downloader.download_asset(asset_url, save_to)


def _download_done(downloader: AssetDownloader) -> None:
    wm = bpy.context.window_manager
    wm.asset_library_status_ping_loaded_new_assets(downloader.remote_url)


def _destroy_asset_downloader(downloader: AssetDownloader) -> None:
    """Delete the reference to this downloader, as it has been shut down."""
    _asset_downloaders_last_status[downloader.remote_url] = downloader.status
    del _asset_downloaders[downloader.remote_url]


def downloader_status(asset_library_url: str) -> DownloadStatus:
    """Returns the downloader status.

    This is either the actual status (if the downloader is still running), or
    the last-known status (if it has shut down by now).

    Raises a KeyError if there never was a downloader for this URL.
    """
    if asset_library_url in _asset_downloaders:
        return _asset_downloaders[asset_library_url].status
    return _asset_downloaders_last_status[asset_library_url]


class DownloadStatus(enum.Enum):
    IDLE = 'idle'
    DOWNLOADING = 'downloading'

    FINISHED = 'finished'
    """The downloader has downloaded everything that was queued.

    Note: this does NOT mean that all downloads were perfect. It just means that
    there were no exceptions raised.
    """

    FAILED = 'failed'
    """Unexpected exceptions occurred."""


class AssetDownloader:
    _locator: RemoteAssetListingLocator

    OnUpdateCallback: TypeAlias = Callable[['AssetDownloader'], None]
    _on_update_callback: OnUpdateCallback
    OnDoneCallback: TypeAlias = Callable[['AssetDownloader'], None]
    _on_done_callback: OnDoneCallback
    OnAssetDoneCallback: TypeAlias = Callable[['AssetDownloader'], None]
    _on_asset_done_callback: OnAssetDoneCallback | None

    _bgdownloader: http_dl.BackgroundDownloader
    _num_assets_pending: int

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

    def __init__(
        self,
        remote_url: str,
        local_path: Path | str,
        on_update_callback: OnUpdateCallback,
        on_asset_done_callback: OnAssetDoneCallback,
        on_done_callback: OnDoneCallback,
    ) -> None:
        """Create a downloader for assets of a specific asset library.

        :param remote_url: Base URL of the remote asset library server.

        :param local_path: The directory to download the index files to.

        :param on_update_callback: Called with one parameter (this
            AssetDownloader) in short, regular intervals
            (_DOWNLOAD_POLL_INTERVAL) while the download is ongoing, and once
            just after the download is done.

        :param on_done_callback: called with one parameter (this
            AssetDownloader) whenever the downloader is "done".

            Here "done" does not imply "successful", as cancellations, network
            errors, or other issues can cause things to abort. In that case,
            this function is still called.

        :param on_asset_done_callback: called with one parameter (this
            AssetDownloader) when at least one new asset finished downloading
            and was put in its final location, ready to be picked up by the
            asset system.
        """
        self._locator = RemoteAssetListingLocator(remote_url, local_path)

        self._on_done_callback = on_done_callback
        self._on_update_callback = on_update_callback
        self._on_asset_done_callback = on_asset_done_callback

        self._num_assets_pending = 0

        self._status = DownloadStatus.IDLE
        self._error_message = ""

        # Work around a limitation of Blender, see bug report #139720 for details.
        self.on_timer_event = self.on_timer_event  # type: ignore[method-assign]

        self._http_metadata_provider = http_dl.MetadataProviderFilesystem(
            cache_location=self._locator.http_metadata_cache_location,
        )

        # Create the background downloader object now, so that it
        # (hypothetically in some future) can be adjusted before the actual
        # downloading begins.
        self._bg_downloader = http_dl.BackgroundDownloader(
            options=http_dl.DownloaderOptions(
                metadata_provider=self._http_metadata_provider,
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

    def start(self) -> None:
        """Start the background process."""
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

    def download_asset(self, asset_url: str, save_to: Path) -> None:
        """Download an asset to a local file."""
        self._status = DownloadStatus.DOWNLOADING
        self._queue_download(asset_url, save_to)

    def _shutdown_if_done(self) -> None:
        if self._num_assets_pending == 0 and self._bg_downloader.all_downloads_done:
            # Done downloading everything, let's shut down.

            # TODO: delay this for a few minutes, so that we don't need a new
            # background process for every asset.
            self.shutdown(DownloadStatus.FINISHED)

    def _on_callback_error(
            self,
            http_req_descr: http_dl.RequestDescription,
            local_file: Path,
            exception: Exception) -> None:
        logger.exception(
            "exception while handling downloaded file ({!r}, saved to {!r})".format(
                http_req_descr, local_file))
        self.report({'ERROR'}, "Asset download had an issue, download aborted")
        self.shutdown(DownloadStatus.FAILED)

    def _queue_download(self, asset_url: str, download_to_path: Path | str) -> Path:
        """Queue up this download, returning the path to which it will be downloaded."""
        remote_url = urllib.parse.urljoin(self._locator.remote_url, asset_url)
        download_to_path = self._locator.local_path / download_to_path

        logger.info("downloading %s to %s", remote_url, download_to_path)

        self._bg_downloader.queue_download(
            remote_url,
            download_to_path,
            self._on_asset_done,)
        return download_to_path

    def _on_asset_done(self,
                       http_req_descr: http_dl.RequestDescription,
                       unsafe_local_file: Path,
                       ) -> None:
        if self._on_asset_done_callback:
            self._on_asset_done_callback(self)

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

        # Automatically switch between IDLE and DOWNLOADING, but never overwrite
        # FAILED or FINISHED_SUCCESFULLY.
        if self._status in {DownloadStatus.DOWNLOADING, DownloadStatus.IDLE}:
            if self._bg_downloader.num_pending_downloads > 0:
                self._status = DownloadStatus.DOWNLOADING
            else:
                self._status = DownloadStatus.IDLE

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
        # TODO: tell Blender this file is done.
        self._shutdown_if_done()

    def download_error(
        self,
        http_req_descr: http_dl.RequestDescription,
        local_file: Path,
        error: Exception,
    ) -> None:
        if isinstance(error, http_dl.DownloadCancelled):
            if self._num_assets_pending:
                self.report({'WARNING'}, "Cancelled {} pending download".format(self._num_assets_pending))
            logger.warning("Download cancelled: %s", http_req_descr)
            self.shutdown(DownloadStatus.FAILED)
            return

        # TODO: tell Blender there was an error downloading.

        # Contrary to the RemoteAssetListingDownloader, this downloader treats
        # all downloads as independent, and thus a failure to download one
        # should never completely shut down the downloader.
        logger.warning("Could not download file %s: %s", http_req_descr, error)

        # This could have been the last to-be-downloaded file, so better
        # check if there's anything left to do.
        self._shutdown_if_done()

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

        # TODO: tell Blender the download is done.
        self._shutdown_if_done()
