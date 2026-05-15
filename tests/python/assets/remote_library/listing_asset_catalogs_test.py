# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import sys
import unittest
from pathlib import Path

from _bpy_internal.assets.remote_library import listing_asset_catalogs
from _bpy_internal.assets.remote_library import blender_asset_library_openapi as api_models

"""
blender -b --factory-startup -P tests/python/assets/remote_library/listing_asset_catalogs_test.py -- --outdir=/tmp
"""

# CLI argument, will be set to its actual value in main() below.
arg_outdir: Path


class ListingDownloaderTest(unittest.TestCase):
    cats_path: Path

    @classmethod
    def setUpClass(cls) -> None:
        arg_outdir.mkdir(parents=True, exist_ok=True)
        cls.cats_path = arg_outdir / "test_catalogs.cats.txt"

    def tearDown(self) -> None:
        self.cats_path.unlink(missing_ok=True)

    def test_write_sanitized_cats(self) -> None:
        catalogs = [
            api_models.CatalogV1(
                path="Cats/Laksa",
                uuids=["cdf49402-6814-5c20-a026-9f3211d8a615"],
                simple_name="cats-laksa",
            ),
            api_models.CatalogV1(
                path="Cats/Quercus\nwith\nnewlines",
                uuids=["2143dfac-9e81-5a31-99f2-befa8ff26ac2"],
                simple_name="cats-quercus",
            ),
            api_models.CatalogV1(
                path="Cats/Raymond\n: Pawducer",
                uuids=["db847984-cb61-5197-87b5-39134a2e758c"],
                simple_name="cats-raymond",
            ),
            api_models.CatalogV1(
                path="Cats/Muesli: Huntress",
                uuids=["d7ce44c8-d1df-5055-bae4-8a6ed03b8329"],
                simple_name="cats-muesli",
            ),
        ]
        asset_library_meta = api_models.AssetLibraryMeta(
            api_versions={},
            contact=api_models.Contact(name="Unit the Tester"),
            name="Cats of\nAmsterdam",
        )

        listing_asset_catalogs.write(catalogs, self.cats_path, asset_library_meta)

        # Check the file is as expected.
        file_lines = self.cats_path.read_text().splitlines()

        # The line numbers are determined by listing_asset_catalogs._ASSET_CATS_HEADER.
        header_num_lines = len(listing_asset_catalogs._ASSET_CATS_HEADER.splitlines())
        self.assertIn("Cats of Amsterdam", file_lines[header_num_lines - 3])

        # The catalogs should be ordered by path, and sanitized.
        written_cats = file_lines[header_num_lines + 1:]
        expect_cats = [
            "cdf49402-6814-5c20-a026-9f3211d8a615:Cats/Laksa:cats-laksa",
            "d7ce44c8-d1df-5055-bae4-8a6ed03b8329:Cats/Muesli Huntress:cats-muesli",
            "2143dfac-9e81-5a31-99f2-befa8ff26ac2:Cats/Quercus with newlines:cats-quercus",
            "db847984-cb61-5197-87b5-39134a2e758c:Cats/Raymond Pawducer:cats-raymond",
        ]
        self.assertEqual(expect_cats, written_cats)


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
