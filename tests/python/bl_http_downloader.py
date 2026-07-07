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


class MaxDownloadSizeTest(unittest.TestCase):
    def test_max_size(self) -> None:
        from _bpy_internal.http import downloader as http_dl
        from unittest.mock import MagicMock, patch

        metadata_provider = MagicMock(spec=http_dl.MetadataProvider)
        metadata_provider.load.return_value = None

        downloader = http_dl.ConditionalDownloader(metadata_provider=metadata_provider)
        downloader.max_size_bytes = 100

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
