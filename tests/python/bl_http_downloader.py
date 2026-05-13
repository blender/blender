# SPDX-FileCopyrightText: 2025 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
blender -b --factory-startup -P tests/python/bl_http_downloader.py  -- --verbose
"""


__all__ = (
    "main",
)

import unittest
from pathlib import Path

from _bpy_internal.http.downloader import RequestDescription


output_dir: Path


class BasicImportTest(unittest.TestCase):
    """Just do a basic import and instantiation of classes.

    This doesn't test the functionality, but does ensure that dependencies like
    third-party libraries are available.
    """

    def test_downloader(self) -> None:
        from _bpy_internal.http import downloader as http_dl

        metadata_provider = http_dl.MetadataProviderFilesystem(
            cache_location=output_dir / "http_metadata"
        )
        downloader = http_dl.ConditionalDownloader(metadata_provider=metadata_provider)
        self.assertIsNotNone(downloader)

    def test_background_downloader(self) -> None:
        from _bpy_internal.http import downloader as http_dl

        metadata_provider = http_dl.MetadataProviderFilesystem(
            cache_location=output_dir / "http_metadata"
        )
        options = http_dl.DownloaderOptions(
            metadata_provider=metadata_provider,
            timeout=1,
            http_headers={'X-Unit-Test': self.__class__.__name__}
        )

        def on_error(req_desc: http_dl.RequestDescription, local_path: Path, ex: Exception) -> None:
            self.fail(f"unexpected call to on_error({req_desc}, {local_path}, {ex})")

        downloader = http_dl.BackgroundDownloader(
            options=options,
            on_callback_error=on_error,
        )

        # Test some trivial properties that don't require anything running.
        self.assertTrue(downloader.all_downloads_done)
        self.assertEqual(0, downloader.num_pending_downloads)
        self.assertFalse(downloader.is_shutdown_requested)
        self.assertFalse(downloader.is_shutdown_complete)
        self.assertFalse(downloader.is_subprocess_alive)


class RequestDescriptionTest(unittest.TestCase):
    def test_comparison(self) -> None:
        from _bpy_internal.http import downloader as http_dl

        url = "http://example.com/download.me"
        http_descr = http_dl.RequestDescription("GET", url)

        # Equality.
        self.assertEqual(http_descr, http_descr, "Same instance")
        self.assertEqual(
            http_descr,
            http_dl.RequestDescription("GET", url),
            "Same HTTP method & URL, different instance",
        )
        self.assertEqual(
            http_descr,
            http_dl.RequestDescription("GET", url, response_headers={"a": "b"}),
            "Same HTTP method & URL, different response headers",
        )

        # Inequality.
        self.assertNotEqual(
            http_descr,
            http_dl.RequestDescription("DELETE", url),
            "Different HTTP method, same URL",
        )
        self.assertNotEqual(
            http_descr,
            http_dl.RequestDescription("GET", "http://example.com/another-file.md"),
            "Same HTTP method, different URL",
        )

        # Safety with other types.
        self.assertNotEqual(http_descr, None, "comparison should be None-safe")
        self.assertNotEqual(http_descr, "some other type", "comparison should be safe for other types")

    def test_hash(self) -> None:
        from _bpy_internal.http import downloader as http_dl

        url = "http://example.com/download.me"
        http_descr = http_dl.RequestDescription("GET", url)

        # Equality.
        self.assertEqual(hash(http_descr), hash(http_descr), "Same instance")
        self.assertEqual(
            hash(http_descr),
            hash(http_dl.RequestDescription("GET", url)),
            "Same HTTP method & URL, different instance",
        )
        self.assertEqual(
            hash(http_descr),
            hash(http_dl.RequestDescription("GET", url, response_headers={"a": "b"})),
            "Same HTTP method & URL, different response headers",
        )

        # Inequality.
        self.assertNotEqual(
            hash(http_descr),
            hash(http_dl.RequestDescription("DELETE", url)),
            "Different HTTP method, same URL",
        )
        self.assertNotEqual(
            hash(http_descr),
            hash(http_dl.RequestDescription("GET", "http://example.com/another-file.md")),
            "Same HTTP method, different URL",
        )

        # Safety with other types.
        self.assertNotEqual(hash(http_descr), hash(None))
        self.assertNotEqual(hash(RequestDescription("", "")), hash(None))


class MaxDownloadSizeTest(unittest.TestCase):
    def test_max_size(self) -> None:
        from _bpy_internal.http import downloader as http_dl
        from unittest.mock import MagicMock, patch

        metadata_provider = MagicMock(spec=http_dl.MetadataProvider)
        metadata_provider.load.return_value = None

        downloader = http_dl.ConditionalDownloader(metadata_provider=metadata_provider)
        downloader.max_disk_size_bytes = 100

        mock_response = MagicMock()
        mock_response.headers = {"Content-Length": "200", "Content-Type": "text/plain"}
        mock_response.status_code = 200
        # When used as context manager, return itself.
        mock_response.__enter__.return_value = mock_response

        # NOTE: when this test fails, it will say "TypeError: a bytes-like object is required, not 'MagicMock'". This
        # indicates that the HTTP downloader tried to actually read bytes from the mock response, which it doesn't
        # support.

        with patch.object(http_dl.requests.Session, 'send', return_value=mock_response):
            with self.assertRaises(http_dl.ContentLengthTooBigError):
                downloader.download_to_file("https://example.com/huge.json", Path("/tmp/huge.json"))


class BackgroundDownloaderProcessTest(unittest.TestCase):
    """Start & stop the background process for the BackgroundDownloader.

    This doesn't test any HTTP requests, but does start & stop the background
    process to check that this is at least possible.
    """

    def test_start_stop(self) -> None:
        from _bpy_internal.http import downloader as http_dl

        metadata_provider = http_dl.MetadataProviderFilesystem(
            cache_location=output_dir / "http_metadata"
        )
        options = http_dl.DownloaderOptions(
            metadata_provider=metadata_provider,
            timeout=1,
            http_headers={'X-Unit-Test': self.__class__.__name__}
        )

        def on_error(req_desc: http_dl.RequestDescription, local_path: Path, ex: Exception) -> None:
            self.fail(f"unexpected call to on_error({req_desc}, {local_path}, {ex})")

        downloader = http_dl.BackgroundDownloader(
            options=options,
            on_callback_error=on_error,
        )

        # Queueing a download before the downloader has started should be rejected.
        with self.assertRaises(RuntimeError):
            downloader.queue_download("https://example.com/", output_dir / "download.tmp")

        downloader.start()

        try:
            self.assertFalse(downloader.is_shutdown_requested)
            self.assertFalse(downloader.is_shutdown_complete)
            self.assertTrue(downloader.is_subprocess_alive)

            # For good measure, call the update function a few times to ensure that
            # any messages are sent. There shouldn't be any, but this should also
            # not be a problem.
            downloader.update()
            downloader.update()
            downloader.update()
        finally:
            # In case any of the pre-shutdown assertions fail, the downloader
            # should still be shut down.
            if downloader.is_subprocess_alive:
                downloader.shutdown()

        downloader.shutdown()

        self.assertTrue(downloader.is_shutdown_requested)
        self.assertTrue(downloader.is_shutdown_complete)
        self.assertFalse(downloader.is_subprocess_alive)


class ConditionalDownloaderTest(unittest.TestCase):

    def test_request_cancel(self) -> None:
        """Request and immediately cancel a download.

        This doesn't test actual HTTP traffic, but rather cancels the request
        at the first possible moment.
        """
        from _bpy_internal.http import downloader as http_dl

        # Create the downloader.
        metadata_provider = http_dl.MetadataProviderFilesystem(
            cache_location=output_dir / "http_metadata"
        )
        downloader = http_dl.ConditionalDownloader(metadata_provider=metadata_provider)

        download_url = "http://localhost:47/download.me"
        download_path = output_dir / "the_download"

        # Set up the periodic check callback function so it immediately cancels.
        expected_http_descr = http_dl.RequestDescription("GET", download_url)
        periodic_check_called = False

        def periodic_check(http_req_descr: http_dl.RequestDescription) -> bool:
            nonlocal periodic_check_called

            periodic_check_called = True
            self.assertEqual(expected_http_descr, http_req_descr)

            return False
        downloader.periodic_check = periodic_check

        # Set up a reporter.
        reporter = http_dl.QueueingReporter()
        downloader.add_reporter(reporter)

        # Request the download.
        with self.assertRaises(http_dl.DownloadCancelled) as cancel_ex:
            _ = downloader.download_to_file(download_url, download_path)

        # Check the result.
        self.assertFalse(download_path.exists())
        self.assertTrue(periodic_check_called)
        self.assertEqual(
            [
                ('download_starts', (expected_http_descr, )),
                ('download_error', (expected_http_descr, download_path, cancel_ex.exception)),
            ],
            list(reporter._queue)  # pyright: ignore[reportPrivateUsage]
        )


def main() -> None:
    global output_dir

    import sys
    import tempfile

    argv = [sys.argv[0]]
    if '--' in sys.argv:
        argv.extend(sys.argv[sys.argv.index('--') + 1:])

    with tempfile.TemporaryDirectory() as temp_dir:
        output_dir = Path(temp_dir)
        unittest.main(argv=argv)


if __name__ == "__main__":
    main()
