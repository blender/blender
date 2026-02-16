# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import sys
import unittest
from pathlib import Path, PurePath, PurePosixPath, PureWindowsPath

from _bpy_internal.assets.remote_library_listing import json_parsing, listing_downloader
from _bpy_internal.assets.remote_library_listing import blender_asset_library_openapi as api_models

import bpy

"""
blender -b --factory-startup -P tests/python/assets/remote_library_listing/listing_downloader_test.py -- --outdir=/tmp
"""

# CLI argument, will be set to its actual value in main() below.
arg_outdir: Path


class ListingDownloaderTest(unittest.TestCase):
    json_path: Path

    @classmethod
    def setUpClass(cls) -> None:
        arg_outdir.mkdir(parents=True, exist_ok=True)
        cls.json_path = arg_outdir / "asset_page.json"

    def tearDown(self) -> None:
        self.json_path.unlink(missing_ok=True)

    def test_sanitize_asset_page__bad_paths(self) -> None:
        # These paths should be handled identically, independent of the current platform.
        self._test_sanitize_asset_page_with_specific_bad_path(r"C:\temp\kubus.blend", "temp/kubus.blend")
        self.assertTrue(self.json_path.exists(), "JSON file should have been rewritten")
        self._test_sanitize_asset_page_with_specific_bad_path(r"C:/temp/kubus.blend", "temp/kubus.blend")
        self.assertTrue(self.json_path.exists(), "JSON file should have been rewritten")
        self._test_sanitize_asset_page_with_specific_bad_path("/temp/kubus.blend", "temp/kubus.blend")
        self.assertTrue(self.json_path.exists(), "JSON file should have been rewritten")
        self._test_sanitize_asset_page_with_specific_bad_path("//temp/kubus.blend", "temp/kubus.blend")
        self.assertTrue(self.json_path.exists(), "JSON file should have been rewritten")
        self._test_sanitize_asset_page_with_specific_bad_path(r"\\NAS\share\temp\kubus.blend", "temp/kubus.blend")
        self.assertTrue(self.json_path.exists(), "JSON file should have been rewritten")
        self._test_sanitize_asset_page_with_specific_bad_path(
            r"//NAS/share/temp/kubus.blend", "NAS/share/temp/kubus.blend")
        self.assertTrue(self.json_path.exists(), "JSON file should have been rewritten")

        # Relative path that attempts to break out of the asset library.
        self._test_sanitize_asset_page_with_specific_bad_path(
            "sneaky/path/../../../temp/kubus.blend", "temp/kubus.blend")
        self.assertTrue(self.json_path.exists(), "JSON file should have been rewritten")

    def test_sanitize_asset_page__all_ok(self) -> None:
        # Construct an incorrect asset page, with bad counts and an absolute path to a file.
        blend_path = "temp/kubus.blend"

        asset_page = api_models.AssetLibraryIndexPageV1(
            # Set correct asset & file counts.
            asset_count=1,
            file_count=1,

            assets=[
                api_models.AssetV1(
                    name="Kubus",
                    id_type="OBJECT",
                    files=[blend_path],
                    thumbnail=api_models.URLWithHash(url="thumbs/kubus.webp", hash="12345"),
                    meta=None,
                ),
            ],
            files=[
                api_models.FileV1(
                    # Absolute path, should get corrected.
                    path=blend_path,
                    size_in_bytes=328051946337,
                    hash="CAT:51756572637573",
                    blender_version="5.2",
                    url=None,
                ),
            ],
        )

        Downloader = listing_downloader.RemoteAssetListingDownloader
        dl = Downloader(
            remote_url="http://localhost/",
            local_path="/tmp/does-not-matter-we-do-not-write",
            on_update_callback=lambda downloader: None,
            on_done_callback=lambda downloader: None,
            on_metafiles_done_callback=None,
            on_page_done_callback=None,
        )
        self.json_path.unlink(missing_ok=True)
        dl._sanitize_asset_page(asset_page, self.json_path)

        # Check that the counts have been kept the same.
        self.assertEqual(1, asset_page.asset_count)
        self.assertEqual(1, asset_page.file_count)

        # Check that the file paths are also kept the same.
        self.assertEqual(blend_path, asset_page.files[0].path, "In-memory file entry should have been sanitized")
        self.assertEqual(
            [blend_path],
            asset_page.assets[0].files,
            "In-memory file reference of asset entry should have been sanitized")

        # Check that the JSON file does not exist, because rewriting was not necessary.
        self.assertFalse(self.json_path.exists(), "JSON file should NOT have been rewritten")

    def _test_sanitize_asset_page_with_specific_bad_path(self, bad_path: str, sanitized_path: str) -> None:
        # Construct an incorrect asset page, with bad counts and an absolute path to a file.
        asset_page = api_models.AssetLibraryIndexPageV1(
            # Set incorrect asset & file counts. These should get corrected.
            asset_count=47,
            file_count=327,

            assets=[
                api_models.AssetV1(
                    name="Kubus",
                    id_type="OBJECT",
                    files=[bad_path],
                    thumbnail=api_models.URLWithHash(url="thumbs/kubus.webp", hash="12345"),
                    meta=None,
                ),
            ],
            files=[
                api_models.FileV1(
                    # Absolute path, should get corrected.
                    path=bad_path,
                    size_in_bytes=328051946337,
                    hash="CAT:51756572637573",
                    blender_version="5.2",
                    url=None,
                ),
            ],
        )

        Downloader = listing_downloader.RemoteAssetListingDownloader
        dl = Downloader(
            remote_url="http://localhost/",
            local_path="/tmp/does-not-matter-we-do-not-write",
            on_update_callback=lambda downloader: None,
            on_done_callback=lambda downloader: None,
            on_metafiles_done_callback=None,
            on_page_done_callback=None,
        )
        self.json_path.unlink(missing_ok=True)
        dl._sanitize_asset_page(asset_page, self.json_path)

        # Check that the counts have been sanitized.
        self.assertEqual(1, asset_page.asset_count)
        self.assertEqual(1, asset_page.file_count)

        # Check that the absolute file path has been sanitized.
        # Both the Windows and the POSIX version of bad_path should sanitize to this path.
        self.assertEqual(sanitized_path, asset_page.files[0].path, "In-memory file entry should have been sanitized")
        self.assertEqual([sanitized_path], asset_page.assets[0].files,
                         "In-memory file reference of asset entry should have been sanitized")

        # Check that the JSON file has been updated, so that when Blender later reads it, it's been sanitized.
        parser = json_parsing.ValidatingParser()
        json_payload = self.json_path.read_bytes()
        from_file = parser.parse_and_validate(api_models.AssetLibraryIndexPageV1, json_payload)

        # Check that the counts have been sanitized.
        self.assertEqual(1, from_file.asset_count)
        self.assertEqual(1, from_file.file_count)

        # Check that the absolute file path has been sanitized.
        # Both the Windows and the POSIX version of bad_path should sanitize to this path.
        self.assertEqual(sanitized_path, from_file.files[0].path, "File entry should have been sanitized")
        self.assertEqual([sanitized_path], from_file.assets[0].files,
                         "File reference of asset entry should have been sanitized")


class PathGuessingTest(unittest.TestCase):
    def test_str_to_path_multiplatform(self) -> None:
        str_to_path = listing_downloader._str_to_path_multiplatform
        self.assertEqual(PureWindowsPath(r'C:\Program Files\Blender'), str_to_path(r'C:\Program Files/Blender'))
        self.assertEqual(PureWindowsPath(r'C:\Program Files\Blender'), str_to_path(r'C:/Program Files/Blender'))
        self.assertEqual(PureWindowsPath(r'\\NAS\share\flamenco\file.blend'),
                         str_to_path(r'\\NAS\share\flamenco\file.blend'))
        self.assertEqual(PurePosixPath('/Program Files/Blender'), str_to_path('/Program Files/Blender'))
        self.assertEqual(PurePath('file.blend'), str_to_path('file.blend'))


class SanitizePathFromURLTest(unittest.TestCase):
    def test_sanitize_path_from_url(self) -> None:
        _sanitize_path_from_url = listing_downloader._sanitize_path_from_url
        self.assertEqual(
            PurePosixPath('normal/path/as/expected.blend'),
            _sanitize_path_from_url('/normal/path/as/expected.blend'),
        )
        self.assertEqual(
            PurePosixPath('normal/path/as/expected.blend'),
            _sanitize_path_from_url(PurePosixPath('/normal/path/as/expected.blend')),
        )
        self.assertEqual(
            PurePosixPath('.'),
            _sanitize_path_from_url(PurePosixPath('')),
        )
        self.assertEqual(
            PurePosixPath('path/filename.blend'),
            _sanitize_path_from_url(PurePosixPath('/path/sub/../filename.blend')),
        )
        self.assertEqual(
            PurePosixPath('path/filename.blend'),
            _sanitize_path_from_url('/path/sub%2F%2E%2e/filename.blend'),
        )
        self.assertEqual(
            PurePosixPath('path/filename.blend'),
            _sanitize_path_from_url('path/filename.blend'),
        )
        self.assertEqual(
            PurePosixPath('longer/filename.blend'),
            _sanitize_path_from_url(PurePosixPath('/longer/faster/path/../../filename.blend')),
        )
        self.assertEqual(
            PurePosixPath('filename.blend'),
            _sanitize_path_from_url(PurePosixPath('/faster/path/../../filename.blend')),
        )
        self.assertEqual(
            PurePosixPath('filename.blend'),
            _sanitize_path_from_url('/faster/path/../../filename.blend'),
        )
        self.assertEqual(
            PurePosixPath('etc/passwd'),
            _sanitize_path_from_url(PurePosixPath('/../../../../../etc/passwd')),
        )


def main():
    global arg_outdir
    import argparse

    argv = [sys.argv[0]]
    if '--' in sys.argv:
        argv += sys.argv[sys.argv.index('--') + 1:]

    parser = argparse.ArgumentParser()
    parser.add_argument('--outdir', required=True, type=Path)
    args, remaining = parser.parse_known_args(argv)

    arg_outdir = args.outdir

    unittest.main(argv=remaining)


if __name__ == "__main__":
    main()
