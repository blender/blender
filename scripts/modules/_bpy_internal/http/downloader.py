# SPDX-FileCopyrightText: 2025 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

from __future__ import annotations

__all__ = (
    "ConditionalDownloader",
    "DownloaderOptions",
    "BackgroundDownloader",
    "DownloadReporter",
    "QueueingReporter",
    "MetadataProvider",
    "MetadataProviderFilesystem",
    "HTTPMetadata",
    "RequestDescription",
    "HTTPRequestDownloadError",
    "ContentLengthUnknownError",
    "ContentLengthError",
    "HTTPRequestUnknownContentEncoding",
    "DownloadCancelled",
    "BackgroundProcessNotRunningError",
    "http_session",
)

import collections
import contextlib
import dataclasses
import enum
import hashlib
import logging
import multiprocessing
import multiprocessing.connection
import multiprocessing.process
import sys
import time
import zlib  # For streaming gzip decompression.
from collections.abc import Callable
from pathlib import Path
from typing import Protocol, TypeAlias, Any, Generator

# To work around this error:
# mypy   : Variable "multiprocessing.Event" is not valid as a type
#          note: See https://mypy.readthedocs.io/en/stable/common_issues.html#variables-vs-type-aliases
# Pylance: Variable not allowed in type expression
from multiprocessing.synchronize import Event as EventClass

import cattrs
import cattrs.preconf.json
import requests
import requests.adapters
import urllib3.util.retry

logger = logging.getLogger(__name__)


class ConditionalDownloader:
    """File downloader supporting HTTP conditional requests.

    Request an URL and stream the body of the response to a file on disk.
    Metadata is saved in a caller-determined location on disk.

    When the file on disk already exists, conditional downloading is performed
    via the HTTP headers 'ETag'/'If-None-Match' and 'Last-Modified'/
    'If-Modified-Since'. When the HTTP server indicates the local file is up to
    date, via a `304 Not Modified` response, the file is not downloaded again.

    This class is fully synchronous, and will thus block the caller until the
    download is complete. It should not typically be used from Blender, as it
    can block the user interface. See `BackgroundDownloader` to download things
    in a background process.
    """

    metadata_provider: MetadataProvider
    """Directory where request metadata is stored."""

    http_session: requests.Session
    """Requests session, for control over retry behavior, TCP connection pooling, etc."""

    chunk_size: int = 8192
    """Download this many bytes before saving to disk and reporting progress."""

    periodic_check: Callable[[], bool]
    """Called repeatedly to see if a running download should continue or be canceled.

    During downloading, the ConditionalDownloader will repeatedly call this
    function to see if it should continue downloading (True) or should cancel
    (False).
    """

    timeout: int | tuple[int, int] | None
    """Timeout in seconds, tuple (connect timeout, read timeout).

    When only one number is given, it is used for both timeouts.
    """

    _reporter: DownloadReporter

    def __init__(
            self,
            metadata_provider: MetadataProvider,
    ) -> None:
        """Create a ConditionalDownloader.

        :param metadata_provider: Location on disk for request metadata,
            like the last-modified timestamp, etag, and content length.
        """
        self.metadata_provider = metadata_provider
        self.http_session = http_session()
        self.chunk_size = 8192  # Sensible default, can be adjusted after creation if necessary.
        self.periodic_check = lambda: True
        self.timeout = None
        self._reporter = _DummyReporter()

    def download_to_file(
        self, url: str, local_path: Path, *, http_method: str = "GET"
    ) -> None:
        """Download the URL to a file on disk.

        The download is streamed to 'local_path + "~"' first. When successful, it
        is renamed to the given path, overwriting any pre-existing file.

        Raises a HTTPRequestDownloadError for specific HTTP errors. Can also
        raise other exceptions, for example when file-system access fails. On any
        exception, the `download_error()` function will be called on the
        reporter.
        """

        http_req_descr = RequestDescription(http_method=http_method, url=url)

        try:
            self._download_to_file(http_req_descr, local_path)
        except Exception as ex:
            self._reporter.download_error(http_req_descr, local_path, ex)
            raise

    def _download_to_file(self, http_req_descr: RequestDescription, local_path: Path) -> None:
        """Same as download_to_file(), but without the exception handling."""

        http_meta = self._metadata_if_valid(http_req_descr, local_path)
        self._reporter.download_starts(http_req_descr)

        # Download to a temporary file first.
        temp_path = local_path.with_suffix(local_path.suffix + "~")
        temp_path.parent.mkdir(exist_ok=True, parents=True)

        try:
            result = self._request_and_stream(http_req_descr, temp_path, http_meta)
        except Exception:
            # Clean up the partially downloaded file.
            temp_path.unlink(missing_ok=True)
            raise

        http_meta, http_req_descr_with_headers = result
        if http_meta is None:
            # Local file is already fresh, no need to re-download.
            assert not temp_path.exists()
            self._reporter.already_downloaded(http_req_descr_with_headers, local_path)
            return

        # Move the downloaded file to the final filename.
        # TODO: AFAIK this is necessary on Windows, while on other platforms the
        # rename is atomic. See if we can get this atomic everywhere.
        local_path.unlink(missing_ok=True)
        temp_path.rename(local_path)

        self.metadata_provider.save(http_req_descr_with_headers, http_meta)

        self._reporter.download_finished(http_req_descr_with_headers, local_path)

    def _request_and_stream(
        self,
        http_req_descr: RequestDescription,
        local_path: Path,
        meta: HTTPMetadata | None,
    ) -> tuple[HTTPMetadata | None, RequestDescription]:
        """Download the remote URL to a local file.

        :return: tuple of:
            - the metadata of the downloaded data, or None if the passed-in
              metadata matches the URL (a "304 Not Modified" was returned).
            - RequestDescription object that includes the HTTP response headers.
        """

        # Don't bother doing anything when the download was cancelled already.
        if not self.periodic_check():
            raise DownloadCancelled(http_req_descr)

        req = requests.Request(http_req_descr.http_method, http_req_descr.url)
        prepped: requests.PreparedRequest = self.http_session.prepare_request(req)
        self._add_compression_request_headers(prepped)
        self._add_conditional_request_headers(prepped, meta)

        with self.http_session.send(prepped, stream=True, timeout=self.timeout) as stream:
            logger.debug(
                "HTTP %s %s (headers %s) -> %d",
                http_req_descr.http_method,
                http_req_descr.url,
                prepped.headers,
                stream.status_code,
            )

            stream.raise_for_status()

            # Create a copy of the `http_req_descr` to store the response headers.
            # The `RequestDescription` struct is frozen, so cannot be modified
            # in-place. And returning a modified copy is IMO a clearer interface
            # than modifying parameters and expecting the caller to know about
            # this.
            response_headers = {key.lower(): value for key, value in stream.headers.items()}
            http_req_descr_with_headers = dataclasses.replace(
                http_req_descr,
                response_headers=response_headers,
            )

            if stream.status_code == 304:  # 304 Not Modified
                # The remote file matches what we have locally. Don't bother streaming.
                return None, http_req_descr_with_headers

            meta = self._stream_to_file(stream, http_req_descr_with_headers, local_path)
            return meta, http_req_descr_with_headers

    def _stream_to_file(
        self,
        stream: requests.Response,
        http_req_descr: RequestDescription,
        local_path: Path,
    ) -> HTTPMetadata | None:
        """Stream the data obtained via the HTTP stream to a local file.

        :return: the metadata of the downloaded data, or None if the passed-in
            metadata matches the URL (a "304 Not Modified" was returned).
        """

        # Determine how many bytes are expected.
        content_length_str: str = stream.headers.get("Content-Length") or ""
        try:
            content_length = int(content_length_str, base=10)
        except ValueError:
            # TODO: add support for this case.
            raise ContentLengthUnknownError(http_req_descr) from None

        # The Content-Length header, obtained above, indicates the number of
        # bytes that we will be downloading. The Requests library automatically
        # decompresses this, and so if the normal (not `stream.raw`) streaming
        # approach would be used, we would count the wrong number of bytes.
        #
        # In order to get to the actual downloaded byte count, we need to bypass
        # Requests' automatic decompression, use the raw byte stream, and
        # decompress ourselves.
        content_encoding: str = stream.headers.get("Content-Encoding") or ""
        decoder: zlib._Decompress | None
        match content_encoding:
            case "gzip":
                wbits = 16 + zlib.MAX_WBITS
                decoder = zlib.decompressobj(wbits=wbits)
            case "":
                decoder = None
            case _:
                raise HTTPRequestUnknownContentEncoding(http_req_descr, content_encoding)

        # Avoid reporting any progress when the download was cancelled.
        if not self.periodic_check():
            raise DownloadCancelled(http_req_descr)

        self._reporter.download_progress(http_req_descr, content_length, 0)

        # Stream the response to a file.
        num_downloaded_bytes = 0
        with local_path.open("wb") as file:
            def write_and_report(chunk: bytes) -> None:
                """Write a chunk to file, and report on the download progress."""
                file.write(chunk)

                self._reporter.download_progress(
                    http_req_descr, content_length, num_downloaded_bytes
                )

                if num_downloaded_bytes > content_length:
                    raise ContentLengthError(http_req_descr, content_length, num_downloaded_bytes)

            # Download and process chunks until there are no more left.
            while chunk := stream.raw.read(self.chunk_size):
                if not self.periodic_check():
                    raise DownloadCancelled(http_req_descr)

                num_downloaded_bytes += len(chunk)
                if decoder:
                    chunk = decoder.decompress(chunk)
                write_and_report(chunk)

            if decoder:
                write_and_report(decoder.flush())
                assert decoder.eof

        if num_downloaded_bytes != content_length:
            raise ContentLengthError(http_req_descr, content_length, num_downloaded_bytes)

        meta = HTTPMetadata(
            request=http_req_descr,
            etag=stream.headers.get("ETag") or "",
            last_modified=stream.headers.get("Last-Modified") or "",
            content_length=num_downloaded_bytes,
        )

        return meta

    def _add_compression_request_headers(self, prepped: requests.PreparedRequest) -> None:
        # GZip is part of Python's stdlib.
        #
        # Deflate is hardly ever used.
        #
        # Zstd is bundled with Blender (and also will be in Python's stdlib in
        # 3.14+), but AFAICS doesn't have a way to decompress a stream so we'd
        # have to keep the entire file in memory. So, for now, limit to GZip
        # support.
        prepped.headers["Accept-Encoding"] = "gzip"

    def _add_conditional_request_headers(self, prepped: requests.PreparedRequest, meta: HTTPMetadata | None) -> None:
        if not meta:
            return

        if meta.last_modified:
            prepped.headers["If-Modified-Since"] = meta.last_modified
        if meta.etag:
            prepped.headers["If-None-Match"] = meta.etag

    def _metadata_if_valid(
        self, http_req_descr: RequestDescription, local_path: Path
    ) -> HTTPMetadata | None:
        meta = self.metadata_provider.load(http_req_descr)
        if meta is None:
            return None

        if meta.request != http_req_descr:
            # Somehow the metadata was loaded, but didn't match this request. Weird.
            self.metadata_provider.forget(http_req_descr)
            return None

        if not self.metadata_provider.is_valid(meta, http_req_descr, local_path):
            self.metadata_provider.forget(http_req_descr)
            return None

        return meta

    def add_reporter(self, reporter: DownloadReporter) -> None:
        """Add a reporter to receive download progress information.

        The reporter's functions are called from the same thread/process as the
        calls to this ConditionalDownloader.
        """
        if self.has_reporter():
            raise ValueError(
                "Only one reporter is supported, I already have {!s}".format(self._reporter)
            )
        self._reporter = reporter

    def has_reporter(self) -> bool:
        return not isinstance(self._reporter, _DummyReporter)


# On Linux, 'fork' is the default. However the Python docs state "Note
# that safely forking a multithreaded process is problematic.", and then
# mention:
#
# The default start method will change away from fork in Python 3.14.
# Code that requires fork should explicitly specify that via
# get_context() or set_start_method().
#
# So I (Sybren) figure it's better to test with the 'spawn' method,
# which is also the current default on Windows and macOS.
_mp_context = multiprocessing.get_context(method='spawn')


@dataclasses.dataclass
class DownloaderOptions:
    metadata_provider: MetadataProvider
    timeout: int | tuple[int, int]
    """Timeout in seconds, tuple (connect timeout, read timeout).

    When only one number is given, it is used for both timeouts.
    """
    http_headers: dict[str, str] = dataclasses.field(default_factory=dict)


class BackgroundDownloader:
    """Wrapper for a ConditionalDownloader + reporters.

    The downloader will run in a separate process, and the reporters will receive
    updates on the main process (or whatever process runs
    BackgroundDownloader.update()).
    """

    num_downloads_ok: int
    num_downloads_error: int
    _num_pending_downloads: int

    _logger: logging.Logger = logger.getChild("BackgroundDownloader")

    # Pipe connection between this class and the Downloader running in a subprocess.
    _connection: multiprocessing.connection.Connection

    # Here and below, 'RequestDescription' is quoted because Pylance (used by
    # VSCode) doesn't fully grasp the `from __future__ import annotations` yet.
    # Or at least so it seems - it shows these lines in error, while both mypy
    # is fine with it and at runtime it works.
    QueuedDownload: TypeAlias = tuple['RequestDescription', Path]
    """Tuple of URL to download, and path to download it to."""

    # Keep track of which callback to call on the completion of which HTTP request.
    # This assumes that RequestDescriptions are unique, and not queued up
    # multiple times simultaneously.
    DownloadDoneCallback: TypeAlias = Callable[['RequestDescription', Path], None]
    _on_downloaded_callbacks: dict[RequestDescription, DownloadDoneCallback]

    OnCallbackErrorCallback: TypeAlias = Callable[['RequestDescription', Path, Exception], None]
    _on_callback_error: OnCallbackErrorCallback

    _reporters: list[DownloadReporter]
    _options: DownloaderOptions
    _downloader_process: multiprocessing.process.BaseProcess | None

    _shutdown_event: EventClass
    _shutdown_complete_event: EventClass

    def __init__(self,
                 options: DownloaderOptions,
                 on_callback_error: OnCallbackErrorCallback,
                 ) -> None:
        """Create a BackgroundDownloader

        :param options: Options to pass to the underlying ConditionalDownloader
            that will run in the background process.
        :param on_callback_error: Callback function that is called whenever the
            "on_download_done" callback of a queued download raises an exception.
        """

        self.num_downloads_ok = 0
        self.num_downloads_error = 0
        self._num_pending_downloads = 0
        self._on_downloaded_callbacks = {}
        self._on_callback_error = on_callback_error

        self._queueing_reporter = QueueingReporter()
        self._options = options

        self._shutdown_event = _mp_context.Event()
        self._shutdown_complete_event = _mp_context.Event()

        self._reporters = [self]
        self._downloader_process = None

    def add_reporter(self, reporter: DownloadReporter) -> None:
        """Add a reporter to receive updates when .update() is called."""
        self._reporters.append(reporter)

    def queue_download(self, remote_url: str, local_path: Path,
                       on_download_done: DownloadDoneCallback | None = None,
                       *,
                       http_method: str = 'GET',
                       ) -> None:
        """Queue up a download of some URL to a location on disk."""

        if self._shutdown_event.is_set():
            raise RuntimeError("BackgroundDownloader is shutting down, cannot queue new downloads")

        if self._downloader_process is None:
            raise RuntimeError("BackgroundDownloader is not started yet, cannot queue downloads")

        self._num_pending_downloads += 1

        http_req_descr = RequestDescription(http_method=http_method, url=remote_url)
        if on_download_done:
            self._on_downloaded_callbacks[http_req_descr] = on_download_done

        self._connection.send(PipeMessage(
            msgtype=PipeMsgType.QUEUE_DOWNLOAD,
            payload=(http_req_descr, local_path),
        ))

    @property
    def all_downloads_done(self) -> bool:
        return self._num_pending_downloads == 0

    @property
    def num_pending_downloads(self) -> int:
        return self._num_pending_downloads

    def clear_download_counts(self) -> None:
        """Resets the number of ok/error downloads."""

        self.num_downloads_ok = 0
        self.num_downloads_error = 0

    def start(self) -> None:
        """Start the downloaded process.

        This MUST be called before calling .update().
        """
        if self._shutdown_event.is_set():
            raise ValueError("BackgroundDownloader was shut down, cannot start again")

        my_side, subprocess_side = _mp_context.Pipe(duplex=True)
        self._connection = my_side

        self._downloader_process = _mp_context.Process(
            name="BackgroundDownloader",
            target=_download_queued_items,
            args=(
                subprocess_side,
                self._options,
            ),
            daemon=True,
        )
        self._logger.info("starting downloader process")
        with _cleanup_main_file_attribute():
            self._downloader_process.start()

    @property
    def is_shutdown_requested(self) -> bool:
        return self._shutdown_event.is_set()

    @property
    def is_shutdown_complete(self) -> bool:
        return self._shutdown_complete_event.is_set()

    @property
    def is_subprocess_alive(self) -> bool:
        return bool(self._downloader_process and self._downloader_process.is_alive())

    def shutdown(self) -> None:
        """Cancel any pending downloads and shut down the background process.

        Blocks until the background process has stopped and all queued updates
        have been processed.

        NOTE: call this from the same process as used to call .update().
        """
        if self._downloader_process is None:
            self._logger.error("shutdown called while the downloader never started")
            return

        if self._shutdown_complete_event.is_set() and not self._downloader_process.is_alive():
            self._logger.debug("shutdown already completed")
            return

        self._logger.debug("shutting down")
        self._shutdown_event.set()

        # Send the CANCEL message to shut down the background process.
        try:
            self._connection.send(PipeMessage(PipeMsgType.CANCEL, None))
        except BrokenPipeError:
            # The other side is already shut down, which is fine.
            pass

        # Keep receiving incoming messages, to avoid the background process
        # getting stuck on a send() call.
        self._logger.debug("processing any pending updates")
        start_wait_time = time.monotonic()
        max_wait_duration = 5.0  # Seconds
        while self._downloader_process.is_alive():
            if time.monotonic() - start_wait_time > max_wait_duration:
                self._logger.error("timeout waiting for background process top stop")
                # Still keep going, as there may be updates that need to be handled,
                # and it's better to continue and set self._shutdown_complete_event
                # as well.
                break
            self._handle_incoming_messages()

        self._logger.debug("download process stopped")
        self._shutdown_complete_event.set()

    def update(self) -> None:
        """Call frequently to ensure the download progress is reported.

        The reports will be sent to all registered reporters, in the same
        process that calls this method.
        """
        if not self.is_subprocess_alive:
            raise BackgroundProcessNotRunningError()
        self._handle_incoming_messages()

    def _handle_incoming_messages(self) -> None:

        while True:
            # Instead of `while self._connection.poll():`, wrap in an exception
            # handler, as on Windows the `poll()` call can raise a
            # BrokenPipeError when the subprocess has ended. Maybe the `recv()`
            # call can raise that exception too, so just for safety I (Sybren)
            # put them in the same `try` block.
            try:
                if not self._connection.poll():
                    break
                msg: PipeMessage = self._connection.recv()
            except (EOFError, BrokenPipeError):
                # The remote end closed the pipe.
                break

            assert msg.msgtype == PipeMsgType.REPORT, \
                "The only messages that should be sent to the main process are reports"

            self._handle_report(msg.payload)

    def _handle_report(self, queued_call: QueueingReporter.FunctionCall) -> None:
        """Send a queued report to all registered reporters."""

        function_name, function_arguments = queued_call
        for reporter in self._reporters:
            function = getattr(reporter, function_name)
            function(*function_arguments)

    def download_starts(self, http_req_descr: RequestDescription) -> None:
        """CachingDownloadReporter interface function."""
        self._logger.debug("Download started %s", http_req_descr.url)

    def already_downloaded(
        self,
        http_req_descr: RequestDescription,
        local_file: Path,
    ) -> None:
        """CachingDownloadReporter interface function.

        Keeps track of internal bookkeeping.
        """
        self._logger.debug("Local file is fresh, no need to re-download %s: %s", http_req_descr.url, local_file)
        self._mark_download_done()
        self.num_downloads_ok += 1
        self._call_on_downloaded_callback(http_req_descr, local_file)

    def download_error(
        self,
        http_req_descr: RequestDescription,
        local_file: Path,
        error: Exception,
    ) -> None:
        """CachingDownloadReporter interface function.

        Keeps track of internal bookkeeping.
        """
        self._logger.error("Error downloading %s: (%r)", http_req_descr.url, error)
        self._mark_download_done()
        self.num_downloads_error += 1

    def download_progress(
        self,
        http_req_descr: RequestDescription,
        content_length_bytes: int,
        downloaded_bytes: int,
    ) -> None:
        """CachingDownloadReporter interface function.

        Keeps track of internal bookkeeping.
        """
        self._logger.debug(
            "Download progress %s: %d of %d: %.0f%%",
            http_req_descr.url,
            downloaded_bytes,
            content_length_bytes,
            downloaded_bytes / content_length_bytes * 100,
        )

    def download_finished(
        self,
        http_req_descr: RequestDescription,
        local_file: Path,
    ) -> None:
        """CachingDownloadReporter interface function.

        Keeps track of internal bookkeeping.
        """
        self._logger.debug("Download finished, stored at %s", local_file)
        self._mark_download_done()
        self.num_downloads_ok += 1
        self._call_on_downloaded_callback(http_req_descr, local_file)

    def _mark_download_done(self) -> None:
        """Reduce the number of pending downloads."""
        self._num_pending_downloads -= 1
        assert self._num_pending_downloads >= 0, "downloaded more files than were queued"

    def _call_on_downloaded_callback(self, http_req_descr: RequestDescription, local_file: Path) -> None:
        """Call the 'on-download-done' callback for this request."""

        if self._shutdown_event.is_set():
            # Do not call any callbacks any more, as the downloader is trying to shut down.
            return

        try:
            callback = self._on_downloaded_callbacks.pop(http_req_descr)
        except KeyError:
            # Not having a callback is fine.
            return

        self._logger.debug("download done, calling %s", callback.__name__)
        try:
            callback(http_req_descr, local_file)
        except Exception as ex:
            # Catch & log exceptions here, so that a callback causing trouble
            # doesn't break the downloader itself.
            self._logger.debug(
                "exception while calling {!r}({!r}, {!r})".format(
                    callback, http_req_descr, local_file))

            try:
                self._on_callback_error(http_req_descr, local_file, ex)
            except Exception:
                self._logger.exception(
                    "exception while handling an error in {!r}({!r}, {!r})".format(
                        callback, http_req_descr, local_file))


class PipeMsgType(enum.Enum):
    QUEUE_DOWNLOAD = 'queue'
    """Payload: BackgroundDownloader.QueuedDownload"""

    CANCEL = 'cancel'
    """Payload: None"""

    REPORT = 'report'
    """Payload: QueueingReporter.FunctionCall"""


@dataclasses.dataclass
class PipeMessage:
    msgtype: PipeMsgType
    payload: Any


def _download_queued_items(
        connection: multiprocessing.connection.Connection,
        options: DownloaderOptions,
) -> None:
    """Runs in a daemon process to download stuff.

    Managed by the BackgroundDownloader class above.
    """
    import queue
    import threading

    # logging.basicConfig(
    #     format="%(asctime)-15s %(processName)22s %(levelname)8s %(name)s %(message)s",
    #     level=logging.DEBUG,
    # )
    log = logger.getChild('background_process')
    log.info('Downloader background process starting')

    # Local queue for incoming messages.
    rx_queue: queue.Queue[PipeMessage] = queue.Queue()

    # Local queue of stuff to download.
    download_queue: collections.deque[BackgroundDownloader.QueuedDownload] = collections.deque()

    # Local queue of reports to send back to the main process.
    reporter = QueueingReporter()

    do_shutdown = threading.Event()

    def rx_thread_func() -> None:
        """Process incoming messages."""
        while not do_shutdown.is_set():
            # Always keep receiving messages while they're coming in,
            # to prevent the remote end hanging on their send() call.
            # Only once that's done should we check the do_shutdown event.
            while connection.poll():
                try:
                    received_msg: PipeMessage = connection.recv()
                except EOFError:
                    log.warning("Blender is no longer running, shutting down the downloader process")
                    do_shutdown.set()
                    return

                log.info("received message: %s", received_msg)
                rx_queue.put(received_msg)

    def tx_thread_func() -> None:
        """Send queued reports back to the main process."""
        while not do_shutdown.is_set():
            try:
                queued_call = reporter.pop()
            except IndexError:
                # Not having anything to do is fine.
                time.sleep(0.01)
                continue

            queued_msg = PipeMessage(
                msgtype=PipeMsgType.REPORT,
                payload=queued_call,
            )
            log.info("sending message %s", queued_msg)
            try:
                connection.send(queued_msg)
            except BrokenPipeError:
                log.warning("Blender is no longer running, shutting down the downloader process")
                do_shutdown.set()
                return

    rx_thread = threading.Thread(target=rx_thread_func)
    tx_thread = threading.Thread(target=tx_thread_func)

    rx_thread.start()
    tx_thread.start()

    def periodic_check() -> bool:
        """Handle received messages, and return whether we can keep running.

        Called periodically by this function, as well as by the downloader.
        """

        while not do_shutdown.is_set():
            try:
                received_msg: PipeMessage = rx_queue.get(block=False)
            except queue.Empty:
                # Not receiving anything is fine.
                return not do_shutdown.is_set()

            match received_msg.msgtype:
                case PipeMsgType.CANCEL:
                    do_shutdown.set()
                case PipeMsgType.QUEUE_DOWNLOAD:
                    download_queue.append(received_msg.payload)
                case PipeMsgType.REPORT:
                    # Reports are sent by us, not by the other side.
                    pass

        return not do_shutdown.is_set()

    # Construct a ConditionalDownloader. Unfortunately this is necessary, as
    # not all its properties can be pickled, and as a result, it cannot be
    # used to send across process boundaries via the multiprocessing module.
    downloader = ConditionalDownloader(
        metadata_provider=options.metadata_provider,
    )
    downloader.http_session.headers.update(options.http_headers)
    downloader.add_reporter(reporter)
    downloader.periodic_check = periodic_check
    downloader.timeout = options.timeout

    try:
        while periodic_check():
            # Pop an item off the front of the queue.
            try:
                queued_download = download_queue.popleft()
            except IndexError:
                time.sleep(0.1)
                continue

            http_req_descr, local_path = queued_download

            # Try and download it.
            try:
                downloader.download_to_file(
                    http_req_descr.url,
                    local_path,
                    http_method=http_req_descr.http_method,
                )
            except DownloadCancelled:
                # Can be logged at a lower level, because the caller did the
                # cancelling, and can log/report things more loudly if necessary.
                log.debug("download got cancelled: %s", http_req_descr)
            except HTTPRequestDownloadError as ex:
                # HTTP errors that were not an explicit cancellation. These are
                # communicated to the main process via the messaging system, so they
                # do not need much logging here.
                log.debug("could not download: %s: %s", http_req_descr, ex)
            except OSError as ex:
                # Things like "disk full", "permission denied", shouldn't need a
                # full stack trace. These are communicated to the main process via
                # the messaging system, so they do not need much logging here.
                log.debug("could not download: %s: %s", http_req_descr, ex)
            except Exception as ex:
                # Unexpected errors should really be logged here, as they may
                # indicate bugs (typos, dependencies not found, etc).
                log.exception("unexpected error downloading %s: %s", http_req_descr, ex)

    except KeyboardInterrupt:
        log.warning("Keyboard interrupt received, shutting down the downloader process")
        do_shutdown.set()

    try:
        rx_thread.join(timeout=1.0)
    except RuntimeError:
        log.exception("joining RX thread")

    try:
        tx_thread.join(timeout=1.0)
    except RuntimeError:
        log.exception("joining TX thread")

    log.debug("download process shutting down")


class CancelEvent(Protocol):
    """Protocol for event objects that indicate a download should be cancelled.

    multiprocessing.Event and processing.Event are compatible with this protocol.
    """

    def is_set(self) -> bool:
        return False

    def clear(self) -> None:
        return


class DownloadReporter(Protocol):
    """This protocol can be used to receive reporting from ConditionalDownloader."""

    def download_starts(self, http_req_descr: RequestDescription) -> None:
        """The download has started.

        After `download_starts()` is called, it is guaranteed that exactly one
        of these functions will be called with the same `RequestDescription`:
            - `already_downloaded()`
            - `download_error()`
            - `download_finished()`
        """

    def already_downloaded(
        self,
        http_req_descr: RequestDescription,
        local_file: Path,
    ) -> None:
        """The previous download to this file is still fresh."""

    def download_error(
        self,
        http_req_descr: RequestDescription,
        local_file: Path,
        error: Exception,
    ) -> None:
        """There was an error downloading the URL.

        This can be due to the actual download (network issues), but also local
        processing of the downloaded data (such as renaming the file from its
        temporary name to its final name).
        """

    def download_progress(
        self,
        http_req_descr: RequestDescription,
        content_length_bytes: int,
        downloaded_bytes: int,
    ) -> None: ...

    def download_finished(
        self,
        http_req_descr: RequestDescription,
        local_file: Path,
    ) -> None:
        """The URL was downloaded to the given file."""


class _DummyReporter(DownloadReporter):
    """Dummy CachingDownloadReporter.

    Does not do anything. This is mostly used to avoid None checks in the
    ConditionalDownloader.
    """

    def download_starts(self, http_req_descr: RequestDescription) -> None:
        pass

    def already_downloaded(
        self,
        http_req_descr: RequestDescription,
        local_file: Path,
    ) -> None:
        pass

    def download_error(
        self,
        http_req_descr: RequestDescription,
        local_file: Path,
        error: Exception,
    ) -> None:
        pass

    def download_progress(
        self,
        http_req_descr: RequestDescription,
        content_length_bytes: int,
        downloaded_bytes: int,
    ) -> None:
        pass

    def download_finished(
        self,
        http_req_descr: RequestDescription,
        local_file: Path,
    ) -> None:
        pass


class QueueingReporter(DownloadReporter):
    """Keeps track of which reporter functions are called.
    """

    FunctionCall: TypeAlias = tuple[str, tuple[Any, ...]]
    """Tuple of the function name and the positional arguments."""

    _queue: collections.deque[FunctionCall]
    """Queue of function calls."""

    _logger: logging.Logger

    def __init__(self) -> None:
        self._queue = collections.deque()
        self._logger = logger.getChild(self.__class__.__name__)

    def pop(self) -> FunctionCall:
        """Pops an item off the queue and returns it.

        Raises IndexError if the queue is empty.
        """
        return self._queue.popleft()

    def download_starts(self, http_req_descr: RequestDescription) -> None:
        self._queue_call('download_starts', http_req_descr)

    def already_downloaded(
        self,
        http_req_descr: RequestDescription,
        local_file: Path,
    ) -> None:
        self._queue_call('already_downloaded', http_req_descr, local_file)

    def download_error(
        self,
        http_req_descr: RequestDescription,
        local_file: Path,
        error: Exception,
    ) -> None:
        self._queue_call('download_error', http_req_descr, local_file, error)

    def download_progress(
        self,
        http_req_descr: RequestDescription,
        content_length_bytes: int,
        downloaded_bytes: int,
    ) -> None:
        self._queue_call('download_progress', http_req_descr, content_length_bytes, downloaded_bytes)

    def download_finished(
        self,
        http_req_descr: RequestDescription,
        local_file: Path,
    ) -> None:
        self._queue_call('download_finished', http_req_descr, local_file)

    def _queue_call(self, function_name: str, *function_args: Any) -> None:
        """Put a function call in the queue."""
        self._logger.debug("%s%s", function_name, function_args)
        self._queue.append((function_name, function_args))


class MetadataProvider(Protocol):
    """Protocol for the metadata necessary for conditional downloading.

    Tracks the ETag an Last-Modified header contents for downloaded files.
    """

    def save(self, http_req_descr: RequestDescription, meta: HTTPMetadata) -> None:
        pass

    def load(self, http_req_descr: RequestDescription) -> HTTPMetadata | None:
        """Return the metadata for the given request.

        Return None if there is no metadata known for this request. This does
        not check any already-downloaded file on disk and just returns the
        metadata as-is.
        """
        pass

    def is_valid(self, meta: HTTPMetadata, http_req_descr: RequestDescription, local_path: Path) -> bool:
        """Determine whether this metadata is still valid, given the other parameters."""
        return False

    def forget(self, http_req_descr: RequestDescription) -> None:
        pass


class MetadataProviderFilesystem(MetadataProvider):
    cache_location: Path
    _converter: cattrs.preconf.json.JsonConverter | None

    def __init__(self, cache_location: Path) -> None:
        self.cache_location = cache_location
        self._converter = None  # Created on first use.

    def __getstate__(self) -> dict[str, Any]:
        """Return the state of this object, for pickling.

        This is necessary to send an instance of this class to a subprocess.
        """
        # self._converter is not pickleable, and so it's just created again when
        # necessary.
        return {"cache_location": self.cache_location}

    def __setstate__(self, state: dict[str, Any]) -> None:
        self.cache_location = state["cache_location"]
        self._converter = None  # Created on first use.

    def _cache_key(self, http_req_descr: RequestDescription) -> str:
        method = http_req_descr.http_method
        url = http_req_descr.url
        return hashlib.sha256("{!s}:{!s}".format(method, url).encode()).hexdigest()

    def _metadata_path(self, http_req_descr: RequestDescription) -> Path:
        # TODO: maybe use part of the cache key to bucket into subdirectories?
        return self.cache_location / self._cache_key(http_req_descr)

    def load(self, http_req_descr: RequestDescription) -> HTTPMetadata | None:
        meta_path = self._metadata_path(http_req_descr)
        if not meta_path.exists():
            return None
        meta_json = meta_path.read_bytes()

        converter = self._ensure_converter()

        try:
            meta_data: HTTPMetadata = converter.loads(meta_json, HTTPMetadata)
        except cattrs.BaseValidationError:
            # File was an old format, got corrupted, or is otherwise unusable.
            # Just act as if it never existed in the first place.
            meta_path.unlink()
            return None

        return meta_data

    def is_valid(self, meta: HTTPMetadata, http_req_descr: RequestDescription, local_path: Path) -> bool:
        """Determine whether this metadata is still valid, given the other parameters."""
        if not local_path.exists():
            return False

        local_file_size = local_path.stat().st_size
        if local_file_size == 0:
            # This is an optimization for downloading bigger files. There is no
            # need to do a conditional download of a zero-bytes file. It is more
            # likely that something went wrong and a file got truncated.
            #
            # And even if the file is of the correct size, non-conditinally
            # doing the same request for the empty file will require less data
            # than including the headers necessary for a conditional download.
            return False

        if local_file_size != meta.content_length:
            return False

        return True

    def forget(self, http_req_descr: RequestDescription) -> None:
        meta_path = self._metadata_path(http_req_descr)
        meta_path.unlink(missing_ok=True)

    def save(self, http_req_descr: RequestDescription, meta: HTTPMetadata) -> None:
        meta.request = http_req_descr

        converter = self._ensure_converter()
        meta_json = converter.dumps(meta)
        meta_path = self._metadata_path(http_req_descr)

        meta_path.parent.mkdir(mode=0o700, parents=True, exist_ok=True)
        meta_path.write_bytes(meta_json.encode())

    def _ensure_converter(self) -> cattrs.preconf.json.JsonConverter:
        if self._converter is not None:
            return self._converter

        self._converter = cattrs.preconf.json.JsonConverter(omit_if_default=True)
        assert self._converter is not None
        return self._converter


@dataclasses.dataclass
class HTTPMetadata:
    """HTTP headers, stored so they can be used for conditional requests later."""

    # Saved without the response headers, as only the interesting ones are
    # stored in explicit fields below.
    request: RequestDescription

    etag: str = ""
    last_modified: str = ""
    content_length: int = 0


# Freeze instances of this class, so they can be used as map key.
@dataclasses.dataclass(frozen=True)
class RequestDescription:
    """Simple descriptor for HTTP requests.

    This is used to simplify function parameters, as well as a key for hashing
    to find the HTTPMetadata file that stores data of previous calls to this
    HTTP requests.

    When passed to callbacks after the request has been performed,
    `response_headers` will contain a copy of the HTTP response headers. The
    header names (i.e. the keys of the dictionary) will be converted to lower
    case.
    """

    http_method: str
    url: str

    response_headers: dict[str, str] = dataclasses.field(compare=False, default_factory=dict)
    """Response headers, dictionary keys are lower-cased.

    This field is ignored in hash() calls and equality checks, to keep instances
    of this class hashable (and thus usable as map key).
    """

    def __hash__(self) -> int:
        return hash((self.http_method, self.url))

    def __eq__(self, value: object) -> bool:
        if not isinstance(value, RequestDescription):
            return False
        return (self.http_method, self.url) == (value.http_method, value.url)

    def __str__(self) -> str:
        return "RequestDescription({!s} {!s})".format(self.http_method, self.url)

    def __repr__(self) -> str:
        return str(self)


class HTTPRequestDownloadError(RuntimeError):
    """Base class for HTTP download errors.

    Note that errors thrown by the Requests library, or thrown when writing
    downloaded data to disk, are NOT wrapped in this class, and are raised
    as-is.
    """

    http_req_desc: RequestDescription

    def __init__(self, http_req_desc: RequestDescription, *args: object) -> None:
        # NOTE: passing http_req_desc here is necessary for these exceptions to be pickleable.
        # See https://stackoverflow.com/a/28335286/875379 for an explanation.
        super().__init__(http_req_desc, *args)
        self.http_req_desc = http_req_desc

    def __repr__(self) -> str:
        return "{!s}({!s})".format(self.__class__.__name__, self.http_req_desc)

    def __str__(self) -> str:
        return repr(self)


class ContentLengthUnknownError(HTTPRequestDownloadError):
    """Raised when a HTTP response does not have a Content-Length header.

    Also raised when the header exists, but cannot be parsed as integer.
    """

    def __init__(self, http_req_desc: RequestDescription) -> None:
        # This __init__ method is necessary to be able to (un)pickle instances.
        super().__init__(http_req_desc)


class ContentLengthError(HTTPRequestDownloadError):
    """Raised when a HTTP response body is smaller or larger than its Content-Length header indicates."""

    def __init__(self, http_req_desc: RequestDescription, expected_size: int, actual_size: int) -> None:
        # This __init__ method is necessary to be able to (un)pickle instances.
        super().__init__(http_req_desc, expected_size, actual_size)
        self.expected_size = expected_size
        self.actual_size = actual_size

    def __repr__(self) -> str:
        return "{!s}(expected_size={:d}, actual_size={:d}, {!s})".format(
            self.__class__.__name__, self.expected_size, self.actual_size, self.http_req_desc)


class HTTPRequestUnknownContentEncoding(HTTPRequestDownloadError):
    """Raised when a HTTP response has an unsupported Content-Encoding header.."""

    def __init__(self, http_req_desc: RequestDescription, content_encoding: str) -> None:
        # This __init__ method is necessary to be able to (un)pickle instances.
        super().__init__(http_req_desc, content_encoding)
        self.content_encoding = content_encoding

    def __repr__(self) -> str:
        return "{!s}(content_encoding={!s}, {!s})".format(
            self.__class__.__name__, self.content_encoding, self.http_req_desc)


class DownloadCancelled(HTTPRequestDownloadError):
    """Raised when ConditionalDownloader.cancel_download() was called.

    This exception is raised in the thread/process that called
    ConditionalDownloader.download_to_file(), and NOT from the thread/process
    doing the cancellation.
    """

    def __init__(self, http_req_desc: RequestDescription) -> None:
        # This __init__ method is necessary to be able to (un)pickle instances.
        super().__init__(http_req_desc)


class BackgroundProcessNotRunningError(Exception):
    """The BackgroundDownloader process is not (yet) running.

    Raised when BackgroundDownloader.update() is called, but the background
    process is not yet running or has died unexpectedly.
    """


def http_session() -> requests.Session:
    """Construct a requests.Session for HTTP requests."""

    # TODO: expose these as function parameters?
    http_retries = urllib3.util.retry.Retry(
        total=8,  # Times,
        backoff_factor=0.05,
    )
    # TODO: add default timeouts as well?
    http_adapter = requests.adapters.HTTPAdapter(max_retries=http_retries)
    session = requests.session()
    session.mount("https://", http_adapter)
    session.mount("http://", http_adapter)

    return session


@contextlib.contextmanager
def _cleanup_main_file_attribute() -> Generator[None]:
    """Context manager to ensure __main__.__file__ is not set.

    `__main__.__file__` is set to "<blender string>" in `PyC_DefaultNameSpace()`
    in `source/blender/python/generic/py_capi_utils.cc`. This is problematic for
    Python's `multiprocessing` module, as it gets confused about what the entry
    point into the Python program was. The easiest way (short of modifying
    Blender itself) is to just temporarily erase the `__main__.__file__`
    attribute.

    See the `get_preparation_data(name)` function in Python's stdlib:
    https://github.com/python/cpython/blob/180b3eb697bf5bb0088f3f35ef2d3675f9fff04f/Lib/multiprocessing/spawn.py#L197

    This issue can be recognized by a failure to start a background process,
    with an error like:

        ``FileNotFoundError: [Errno 2] No such file or directory: '/path/to/blender/<blender string>'``

    """

    try:
        main_module = sys.modules['__main__']
    except KeyError:
        # No __main__ is fine.
        yield
        return

    # Be careful to only modify the property when we know it has a value
    # that will cause problems. Python dunder variables like this can
    # trigger all kinds of unknown magics, so they should be left alone
    # as much as possible.
    old_file = getattr(main_module, '__file__', None)
    if old_file != "<blender string>":
        yield
        return

    try:
        del main_module.__file__
        yield
    finally:
        main_module.__file__ = old_file
