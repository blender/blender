# SPDX-FileCopyrightText: 2025 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import dataclasses
import datetime
import os
import shutil
import sys
import unittest
from pathlib import Path

"""
blender -b --factory-startup --python tests/python/assets/remote_library_listing/index_downloader_test.py -- --output-dir /tmp/tests
"""

from _bpy_internal.assets.remote_library_listing import index_downloader
from _bpy_internal.assets.remote_library_listing import blender_asset_library_openapi as api_models


@dataclasses.dataclass
class CLIArgs:
    output_dir: Path


# CLI arguments, will be reassigned with actual arguments from the CLI in the main() function.
args = CLIArgs(Path('.'))


class ThumbnailTimestampTest(unittest.TestCase):

    def setUp(self) -> None:
        self.loc = index_downloader.RemoteAssetListingLocator("https://localhost:8000/", args.output_dir)

        asset = api_models.AssetV1(
            name="Suzanne",
            id_type=api_models.AssetIDTypeV1.object,
            archive_url='http://localhost:8000/monkeys/suzanne.blend',
            archive_size_in_bytes=327,
            archive_hash='010203040506',
            thumbnail_url="monkeys/suzanne/thumbnails/suzanne.webp",
        )

        thumb_path = self.loc.thumbnail_download_path(asset)
        assert thumb_path is not None
        self.thumb_path = thumb_path

        self.failure_path = self.loc.thumbnail_failed_path(thumb_path)

    def tearDown(self) -> None:
        self.thumb_path.unlink(missing_ok=True)
        self.failure_path.unlink(missing_ok=True)
        shutil.rmtree(self.loc._thumbs_root, ignore_errors=True)

    def should_download(self) -> bool:
        """Run the function under test."""
        return index_downloader._compare_thumbnail_filepath_timestamps(self.thumb_path, self.failure_path)

    def set_timestamp(self, path: Path, *, age_in_minutes: int) -> None:
        """Set the path's timestamp, creating the file if necessary."""
        # Ensure the file exists.
        path.parent.mkdir(exist_ok=True, parents=True)
        path.touch(exist_ok=True)

        # Update the last-accessed and last-modified timestamps.
        # They can only be updated in unison.
        now = datetime.datetime.now(datetime.timezone.utc)
        then = now - datetime.timedelta(minutes=age_in_minutes)
        stamp = then.timestamp()
        os.utime(path, (stamp, stamp))

    def test_both_missing(self) -> None:
        self.thumb_path.unlink(missing_ok=True)
        self.failure_path.unlink(missing_ok=True)
        self.assertTrue(self.should_download())

    def test_exists_but_fresh(self) -> None:
        self.set_timestamp(self.thumb_path, age_in_minutes=index_downloader.THUMBNAIL_FRESH_DURATION_MINS // 2)
        self.failure_path.unlink(missing_ok=True)
        self.assertFalse(self.should_download(), self.thumb_path)

    def test_exists_but_old(self) -> None:
        self.set_timestamp(self.thumb_path, age_in_minutes=index_downloader.THUMBNAIL_FRESH_DURATION_MINS * 2)
        self.failure_path.unlink(missing_ok=True)
        self.assertTrue(self.should_download(), self.thumb_path)

    def test_failed_but_thumb_less_old(self) -> None:
        self.set_timestamp(self.thumb_path, age_in_minutes=index_downloader.THUMBNAIL_FRESH_DURATION_MINS * 2)
        self.set_timestamp(self.failure_path, age_in_minutes=index_downloader.THUMBNAIL_FRESH_DURATION_MINS * 3)
        self.assertTrue(self.should_download(), self.thumb_path)

    def test_failed_recently_without_thumb(self) -> None:
        self.thumb_path.unlink(missing_ok=True)
        self.set_timestamp(self.failure_path, age_in_minutes=1)
        self.assertFalse(self.should_download(), self.thumb_path)

    def test_failed_long_ago_without_thumb(self) -> None:
        self.thumb_path.unlink(missing_ok=True)
        self.set_timestamp(self.failure_path, age_in_minutes=index_downloader.THUMBNAIL_FAILED_REDOWNLOAD_MINS * 2)
        self.assertTrue(self.should_download(), self.thumb_path)

    def test_failed_recently_and_thumb_older(self) -> None:
        self.set_timestamp(self.thumb_path, age_in_minutes=index_downloader.THUMBNAIL_FRESH_DURATION_MINS * 2)
        self.set_timestamp(self.failure_path, age_in_minutes=1)
        self.assertFalse(self.should_download(), self.thumb_path)

    def test_failed_long_ago_and_thumb_older(self) -> None:
        self.set_timestamp(self.thumb_path, age_in_minutes=index_downloader.THUMBNAIL_FAILED_REDOWNLOAD_MINS * 3)
        self.set_timestamp(self.failure_path, age_in_minutes=index_downloader.THUMBNAIL_FAILED_REDOWNLOAD_MINS * 2)
        self.assertTrue(self.should_download(), self.thumb_path)


def main():
    global args
    import argparse

    argv = [sys.argv[0]]
    if '--' in sys.argv:
        argv += sys.argv[sys.argv.index('--') + 1:]

    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--output-dir",
        dest="output_dir",
        default=".",
        help="Where to output temp saved files",
        required=False,
    )

    parsed_args, remaining = parser.parse_known_args(argv)

    args = CLIArgs(
        output_dir=Path(parsed_args.output_dir)
    )

    unittest.main(argv=remaining)


if __name__ == "__main__":
    main()
