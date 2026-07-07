# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

from __future__ import annotations

__all__ = [
    "download_asset_file",
    "downloader_status",
    "DownloadStatus",
]

from collections.abc import Callable
import dataclasses
import enum
import logging
import urllib.parse
from pathlib import Path

import bpy

from _bpy_internal.http import downloader as http_dl
from _bpy_internal.assets.remote_library.listing_downloader import RemoteAssetListingLocator
from _bpy_internal.assets.remote_library import hashing

logger = logging.getLogger(__name__)

# Preview images will NOT be downloaded if they already exist on disk AND their
# timestamp is younger than this age.
PREVIEW_DOWNLOAD_AGE_THRESHOLD_SEC = 7 * 24 * 3600  # 1 week

_asset_downloaders: dict[str, AssetDownloader] = {}
_preview_downloaders: dict[str, AssetDownloader] = {}


def download_asset_file(
        asset_library_url: str,
        asset_library_local_path: Path,
        asset_url: str,
        asset_hash: str,
        save_to: Path) -> str:
    """Download an asset file to a file on disk.

    :param asset_library_url: Root URL of the remote asset library. Used as an
        identifier of this library (to create a downloader per library), as well
        as for resolving relative URLs.

    :param asset_library_local_path: Root path of the local asset cache. Used to
        resolve relative `save_to` paths, but also to find the HTTP metadata
        cache for this asset library (for conditional downloads).

    :param asset_url: the URL to download. Can be absolute or relative to the
        asset library URL. If it is an empty string, the `save_to` path is used
        as the URL.
    :param asset_hash: the hash of the asset file, will be appended to the URL.

    :param save_to: the path on disk where to download to. While the download is
        pending, ".part" will be appended to the filename. When the download
        finishes successfully, it is renamed to the final path.

    :returns: the final URL that was queued for downloading.
    """
    try:
        downloader = _asset_downloaders[asset_library_url]
        assert downloader.local_path == asset_library_local_path, (
            "This code assumes that remote asset libraries do not move on the local disk"
        )
    except KeyError:
        downloader = AssetDownloader(
            asset_library_url,
            asset_library_local_path,
            reporter=AssetReporter(asset_library_url=asset_library_url),
            on_queue_empty_callback=on_asset_download_queue_empty,
        )
        downloader.start()
        _asset_downloaders[asset_library_url] = downloader

    # Construct the URL if not given explicitly.
    if not asset_url:
        if save_to.is_absolute():
            relative_path = save_to.relative_to(asset_library_local_path)
        else:
            relative_path = save_to
        asset_url = urllib.parse.quote(relative_path.as_posix())

    # Include the hash in the URL, and download the asset.
    download_url = hashing.url((asset_url, asset_hash))
    full_url = downloader.download_asset_file(download_url, save_to)
    return full_url


def download_preview(
        asset_library_url: str,
        asset_library_local_path: Path,
        preview_url: str,
        preview_hash: str,
        dst_filepath: Path) -> None:
    """Download an asset preview to a file on disk.

    :param asset_library_url: Root URL of the remote asset library. Used as an
        identifier of this library (to create a downloader per library), as well
        as for resolving relative URLs.

    :param asset_library_local_path: Root path of the local asset cache. Used to
        resolve relative `save_to` paths, but also to find the HTTP metadata
        cache for this asset library (for conditional downloads).

    :param preview_url: the URL to download. Can be absolute or relative.
    :param preview_hash: the hash of the thumbnail, will be appended to the URL.

    :param dst_filepath: the path on disk where to download to. While the
        download is pending, ".part" will be appended to the filename. When the
        download finishes successfully, it is renamed to the final path.
    """
    import time

    # Check if the file exists and is new enough. If it is, don't bother the server.
    try:
        stat = dst_filepath.stat()
    except FileNotFoundError:
        pass  # Fine, something new to download.
    else:
        # File exists, let's see if it's young enough to use as-is.
        age_in_seconds = time.time() - stat.st_mtime
        if age_in_seconds < PREVIEW_DOWNLOAD_AGE_THRESHOLD_SEC:
            # The local file is still fresh, just pretend we just downloaded it.
            bpy.types.WindowManager.asset_library_status_ping_loaded_new_preview(str(dst_filepath))
            return

    try:
        downloader = _preview_downloaders[asset_library_url]
        assert downloader.local_path == asset_library_local_path, (
            "This code assumes that remote asset libraries do not move on the local disk"
        )
    except KeyError:
        downloader = AssetDownloader(
            asset_library_url,
            asset_library_local_path,
            reporter=PreviewReporter(),
            on_queue_empty_callback=None,
        )
        downloader.start()
        _preview_downloaders[asset_library_url] = downloader

    # Include the hash in the URL, and download the preview.
    download_url = hashing.url((preview_url, preview_hash))
    downloader.download_asset_file(download_url, dst_filepath)


def cancel_download(asset_library_url: str, full_asset_url: str) -> None:
    """Cancel a running/queued asset download.

    Cancelling a URL that has already been fully downloaded, or one that was never
    queued is a no-op.

    :param asset_library_url: Root URL of the remote asset library. Used as an
        identifier of this library (to create a downloader per library).
        Contrary to the download function, this is NOT used to resolve relative
        URLs.
    :param full_asset_url: the URL that's queued for download. MUST be the final
        URL as returned by download_asset_file().
    """

    try:
        downloader = _asset_downloaders[asset_library_url]
    except KeyError:
        # No downloader could mean that the cancel came in just a millisecond
        # too late, and the download was already finished.
        return

    downloader.cancel_download(full_asset_url)


def cancel_download_all_assets() -> None:
    """Cancel all active/queued downloads of all assets.

    This shuts down all asset downloaders, effectively cancelling all their downloads.
    """

    for downloader in _asset_downloaders.values():
        downloader.cancel_and_shutdown()


def downloader_status(asset_library_url: str) -> DownloadStatus:
    """Returns the asset downloader status.

    Raises a KeyError if there never was a downloader for this URL.
    """
    return _asset_downloaders[asset_library_url].status


def on_asset_download_queue_empty() -> None:
    """Called by the asset downloader when its download queue emptied."""
    if any_asset_downloading():
        return
    bpy.types.WindowManager.asset_library_status_ping_finished_download_queue()


def any_asset_downloading() -> bool:
    """Returns true if there is any downloader currently downloading assets."""
    return any(
        downloader.status == DownloadStatus.DOWNLOADING
        for downloader in _asset_downloaders.values()
    )


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

    CANCELLED = 'cancelled'
    """There still were pending downloads when the downloader shut down."""


class AssetDownloader:
    """Downloader for asset files & their thumbnails."""

    _locator: RemoteAssetListingLocator
    _bg_downloader: http_dl.BackgroundDownloader | None
    _reporter: http_dl.DownloadReporter
    _num_assets_pending: int

    type QueueEmptyCallback = Callable[[], None]
    _on_queue_empty_callback: QueueEmptyCallback | None
    """Called when the download queue became empty."""

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

    _HTTP_METHOD = "GET"

    def __init__(
        self,
        remote_url: str,
        local_path: Path | str,
        *,
        reporter: http_dl.DownloadReporter,
        on_queue_empty_callback: QueueEmptyCallback | None,
    ) -> None:
        """Create a downloader for assets of a specific asset library.

        :param remote_url: Base URL of the remote asset library server.

        :param local_path: The directory to download the index files to.

        :param on_download_done_callback: called with one parameter (this
            AssetDownloader) when a file finished downloading and was put
            in its final location, ready to be picked up by the asset system.
        """
        self._locator = RemoteAssetListingLocator(remote_url, local_path)
        self._num_assets_pending = 0
        self._reporter = reporter
        self._on_queue_empty_callback = on_queue_empty_callback

        self._status = DownloadStatus.IDLE
        self._error_message = ""

        # Work around a limitation of Blender, see bug report #139720 for details.
        self.on_timer_event = self.on_timer_event  # type: ignore[method-assign]

        self._http_metadata_provider = http_dl.MetadataProviderFilesystem(
            cache_location=self._locator.http_metadata_cache_location,
        )

        self._bg_downloader = None

    def _create_bg_downloader(self) -> None:
        self._bg_downloader = http_dl.BackgroundDownloader(
            options=http_dl.DownloaderOptions(
                metadata_provider=self._http_metadata_provider,
                timeout=300,
                http_headers={
                    'X-Blender': "{:d}.{:d}".format(*bpy.app.version),
                },
            ),
            on_callback_error=self._on_callback_error,
        )

        # These are called in order. Doing things this way ensures that self._reporter.download_finished() is called for
        # every individual download, and after that our own function is called. That means that the
        # self._on_queue_empty_callback() function is called _after_ the individual downloads.
        #
        # Swapping this order would mean self._on_queue_empty_callback() is called _before_ the last call to
        # self._reporter.download_finished(), which would be confusing.
        self._bg_downloader.add_reporter(self._reporter)
        self._bg_downloader.add_reporter(self)

    def __repr__(self) -> str:
        return "{!s}(remote_url={!r}, local_path={!r})".format(
            type(self),
            self._locator.remote_url,
            self._locator.local_path,
        )

    def start(self) -> None:
        """Start the background process."""
        if not self._bg_downloader:
            self._create_bg_downloader()
            assert self._bg_downloader
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

    def download_asset_file(self, asset_url: str, save_to: Path) -> str:
        """Download an asset or preview file to a local file.

        Returns the URL that was queued. This is different than the given URL
        when the latter is relative.
        """

        # If the downloader was shut down, start it up again.
        if not self._bg_downloader:
            self.start()

        self._status = DownloadStatus.DOWNLOADING
        url = self._queue_download(asset_url, save_to)
        return url

    def cancel_download(self, full_asset_url: str) -> None:
        """Cancel downloading a URL.

        If the URL was never queued, or it has already been downloaded,
        this is a no-op.
        """
        if not self._bg_downloader:
            return

        logger.info("cancelling download of %s", full_asset_url)
        http_req_descr = http_dl.RequestDescription(self._HTTP_METHOD, full_asset_url)
        self._bg_downloader.cancel_download(http_req_descr)

    def _shutdown_if_done(self) -> None:
        if self._num_assets_pending > 0:
            return

        is_done = self._bg_downloader is None or self._bg_downloader.all_downloads_done
        if not is_done:
            return

        # Done downloading everything, let's shut down.
        self._status = DownloadStatus.FINISHED

        if self._on_queue_empty_callback is not None:
            # Call the callback _after_ setting the status, so that when
            # Blender is pinged about this, it can see it's finished.
            self._on_queue_empty_callback()

        # TODO: delay this for a few minutes, so that we don't need a new
        # background process for every asset.
        self.shutdown()

    def _on_callback_error(
            self,
            http_req_descr: http_dl.RequestDescription,
            local_file: Path,
            exception: Exception) -> None:
        logger.exception(
            "exception while handling downloaded file ({!r}, saved to {!r})".format(
                http_req_descr, local_file))
        self.report({'ERROR'}, "Resource download had an issue, download aborted")
        self._status = DownloadStatus.FAILED
        self.shutdown()

    def _queue_download(self, asset_url: str, download_to_path: Path | str) -> str:
        """Queue up this download.

        Returns the URL of the download, and the path to which it will be downloaded.
        """
        remote_url = urllib.parse.urljoin(self._locator.remote_url, asset_url)
        download_to_path = self._locator.local_path / download_to_path

        # Safety measure: refuse to download a file into the listing directory.
        if self._locator.is_system_path(download_to_path):
            raise ValueError(
                ("Asset at {!s} wants to be downloaded to {!s}, which would overwrite local asset system files. " +
                 "Notify the owner of the asset library about this.").format(
                    remote_url,
                    download_to_path))

        logger.info("downloading %s to %s", remote_url, download_to_path)

        assert self._bg_downloader, "downloads can only be queued when the bgdownloader is available"
        request_descr = self._bg_downloader.queue_download(
            remote_url,
            download_to_path,
            http_method=self._HTTP_METHOD,
        )
        return request_descr.url

    # TODO: implement this in a more useful way:
    def report(self, level: set[str], message: str) -> None:
        # logger.info("Report: {:s}: {:s}".format("/".join(level), message))
        if 'ERROR' in level:
            self._error_message = message

    def cancel_and_shutdown(self) -> None:
        """Cancel all downloads and shut down the background downloader."""

        # Only set to 'Cancelled' if the downloader was still downloading.
        if self._status == DownloadStatus.DOWNLOADING:
            if self._bg_downloader and self._bg_downloader.num_pending_downloads > 0:
                self._status = DownloadStatus.CANCELLED
            else:
                self._status = DownloadStatus.FINISHED

        # The downloads themselves don't have to be explicitly cancelled,
        # shutting down the downloader will do that implicitly.
        self.shutdown()

        # By now there is no more queue, so just treat it as 'empty' and let Blender know no downloads will happen any
        # more (at least not by this downloader).
        if self._on_queue_empty_callback is not None:
            # Call the callback _after_ setting the status, so that when
            # Blender is pinged about this, it can see it's finished.
            self._on_queue_empty_callback()

    def shutdown(self) -> None:
        """Stop the background downloader and call the 'done' callback."""

        # The timer is no longer necessary, the bg_downloader.shutdown() call
        # takes care of the last queued messages.
        if bpy.app.timers.is_registered(self.on_timer_event):
            bpy.app.timers.unregister(self.on_timer_event)

        try:
            if not self._bg_downloader:
                return

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
            self._bg_downloader = None

    def on_timer_event(self) -> float:
        assert self._bg_downloader, "timer events should only come in while the bgdownloader is available"

        try:
            self._bg_downloader.update()
        except http_dl.BackgroundProcessNotRunningError:
            logger.error("Background downloader subprocess died, aborting.")
            self._status = DownloadStatus.FAILED
            self.shutdown()
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

        return self._DOWNLOAD_POLL_INTERVAL

    @property
    def remote_url(self) -> str:
        return self._locator.remote_url

    @property
    def local_path(self) -> Path:
        return self._locator.local_path

    @property
    def status(self) -> DownloadStatus:
        return self._status

    @property
    def error_message(self) -> str:
        return self._error_message

    # Below here: http_dl.DownloadReporter protocol functions:

    def download_starts(self, http_req_descr: http_dl.RequestDescription) -> None:
        pass

    def already_downloaded(
        self,
        http_req_descr: http_dl.RequestDescription,
        local_file: Path,
    ) -> None:
        self._shutdown_if_done()

    def download_error(
        self,
        http_req_descr: http_dl.RequestDescription,
        local_file: Path,
        error: Exception,
    ) -> None:
        if isinstance(error, http_dl.DownloadCancelled):
            # Cancelling a download should cancel all queued-up downloads.
            if self._num_assets_pending:
                self.report({'WARNING'}, "Cancelled {} pending download".format(self._num_assets_pending))
            logger.warning("Download cancelled: %s", http_req_descr)
            self._status = DownloadStatus.FAILED
            self.shutdown()
            return

        self._shutdown_if_done()

    def download_progress(
        self,
        http_req_descr: http_dl.RequestDescription,
        progress: http_dl.DownloadProgress,
    ) -> None:
        pass

    def download_finished(
        self,
        http_req_descr: http_dl.RequestDescription,
        local_file: Path,
    ) -> None:
        self._shutdown_if_done()


@dataclasses.dataclass
class AssetReporter:
    """Implementation of the http_dl.DownloadReporter protocol."""

    asset_library_url: str

    def download_starts(self, http_req_descr: http_dl.RequestDescription) -> None:
        logger.debug("Download starting: %s", http_req_descr)

    def already_downloaded(
        self,
        http_req_descr: http_dl.RequestDescription,
        local_file: Path,
    ) -> None:
        logger.debug("Download unnecessary, file already downloaded: %s", http_req_descr.url)
        bpy.types.WindowManager.asset_library_status_ping_asset_file_succeeded(
            self.asset_library_url, http_req_descr.url, str(local_file))

    def download_error(
        self,
        http_req_descr: http_dl.RequestDescription,
        local_file: Path,
        error: Exception,
    ) -> None:
        logger.warning("Could not download file %s: %s", http_req_descr, error)
        bpy.types.WindowManager.asset_library_status_ping_asset_file_failed(
            self.asset_library_url, http_req_descr.url, str(local_file))

    def download_progress(
        self,
        http_req_descr: http_dl.RequestDescription,
        progress: http_dl.DownloadProgress,
    ) -> None:
        bpy.types.WindowManager.asset_library_status_ping_asset_file_progress(
            http_req_descr.url, progress.disk_bytes_written)

    def download_finished(
        self,
        http_req_descr: http_dl.RequestDescription,
        local_file: Path,
    ) -> None:
        logger.info("Download finished: %s to %s", http_req_descr, local_file)
        bpy.types.WindowManager.asset_library_status_ping_asset_file_succeeded(
            self.asset_library_url, http_req_descr.url, str(local_file))


@dataclasses.dataclass
class PreviewReporter:
    """Implementation of the http_dl.DownloadReporter protocol."""

    def download_starts(self, http_req_descr: http_dl.RequestDescription) -> None:
        logger.debug("Download starting: %s", http_req_descr)

    def already_downloaded(
        self,
        http_req_descr: http_dl.RequestDescription,
        local_file: Path,
    ) -> None:
        # This cannot check the content-type header (like download_finished() does), since
        # there likely is none in a '304 Not Modified' response.

        # Indicate to a future run that we just confirmed this file is still fresh.
        local_file.touch()

        # Poke Blender so it knows there's a thumbnail update. It shouldn't be necessary, but since it requested the
        # file for downloading, it may not have been aware it already existed. Better let it know.
        bpy.types.WindowManager.asset_library_status_ping_loaded_new_preview(str(local_file))

    def download_error(
        self,
        http_req_descr: http_dl.RequestDescription,
        local_file: Path,
        error: Exception,
    ) -> None:
        # TODO: create an empty file in the correct `.../_thumbs/failed` directory.
        self.download_finished(http_req_descr, local_file)

    def download_progress(
        self,
        http_req_descr: http_dl.RequestDescription,
        progress: http_dl.DownloadProgress,
    ) -> None:
        pass

    def download_finished(
        self,
        http_req_descr: http_dl.RequestDescription,
        local_file: Path,
    ) -> None:
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
            # around, so that at least the time-stamping works to prevent
            # hammering the server.

        # Indicate to a future run that we just confirmed this file is still fresh.
        local_file.touch()

        # Poke Blender so it knows there's a thumbnail update.
        bpy.types.WindowManager.asset_library_status_ping_loaded_new_preview(str(local_file))
