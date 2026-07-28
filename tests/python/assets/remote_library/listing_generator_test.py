# SPDX-FileCopyrightText: 2025 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import sys
import unittest
from pathlib import PurePath, PurePosixPath, PureWindowsPath

from _bpy_internal.assets.remote_library import json_parsing, hashing
from _bpy_internal.assets.remote_library import blender_asset_library_openapi as api_models
from _bpy_internal.assets.remote_library import cli_listing_generator_asset_finder as asset_finder

import bpy


"""
blender -b --factory-startup --python tests/python/assets/remote_library/listing_generator_test.py
"""


class CustomPropertiesTest(unittest.TestCase):
    cube: bpy.types.Object

    def setUp(self) -> None:
        bpy.ops.wm.read_homefile(use_factory_startup=True)

        self.cube = bpy.data.objects['Cube']
        self.cube.asset_mark()
        self.maxDiff = 100000

    def test_empty_metadata(self) -> None:
        del self.cube.asset_data["dimensions"]
        meta = asset_finder._get_asset_meta(self.cube.asset_data)
        self.assertIsNone(meta)

    def test_plain_properties(self) -> None:
        asset_data = self.cube.asset_data

        asset_data["barcode"] = "155366"  # Integer-like, should be stored as string.
        asset_data["location"] = "café"  # Non-ASCII string.
        asset_data["size"] = 32.7  # FLOAT
        asset_data["count"] = 47  # INT
        asset_data["amazing"] = True  # BOOL

        meta = asset_finder._get_asset_meta(asset_data)

        Types = api_models.CustomPropertyTypeV1
        Prop = api_models.CustomPropertyV1
        # autopep8: off
        expected_props = [
            Prop(name='dimensions', type=Types.IDP_ARRAY, value=[2.0, 2.0, 2.0], itemtype=Types.IDP_FLOAT),
            Prop(name='barcode', type=Types.IDP_STRING, value='155366'),
            Prop(name='location', type=Types.IDP_STRING, value='café'),
            Prop(name='size', type=Types.IDP_FLOAT, value=32.7),
            Prop(name='count', type=Types.IDP_INT, value=47),
            Prop(name='amazing', type=Types.IDP_BOOL, value=True),
        ]
        # autopep8: on

        assert meta is not None
        self.assertEqual(expected_props, meta.properties)

    def test_array_properties(self) -> None:
        asset_data = self.cube.asset_data

        asset_data["agents"] = ["007", "47", "327"]
        asset_data["locations"] = ["Hokkaido", "Santa Fortuna", "Sapienza"]
        asset_data["boundingbox"] = [-3.0, -4.0, -0.1, 1, 2, 3]

        meta = asset_finder._get_asset_meta(asset_data)

        Types = api_models.CustomPropertyTypeV1
        Prop = api_models.CustomPropertyV1
        # autopep8: off
        expected_prop = [
            Prop(name='dimensions', type=Types.IDP_ARRAY, value=[2.0, 2.0, 2.0], itemtype=Types.IDP_FLOAT),
            Prop(name='agents', type=Types.IDP_ARRAY, value=["007", "47", "327"], itemtype=Types.IDP_STRING),
            Prop(name='locations', type=Types.IDP_ARRAY, value=["Hokkaido", "Santa Fortuna", "Sapienza"], itemtype=Types.IDP_STRING),
            Prop(name='boundingbox', type=Types.IDP_ARRAY, value=[-3.0, -4.0, -0.1, 1.0, 2.0, 3.0], itemtype=Types.IDP_FLOAT),
        ]
        # autopep8: on

        assert meta is not None
        self.assertEqual(expected_prop, meta.properties)

    def test_serialize_to_json(self) -> None:
        meta = asset_finder._get_asset_meta(self.cube.asset_data)

        # The asset metadata should be convertable to JSON.
        parser = json_parsing.ValidatingParser()
        as_json = parser.dumps(meta)
        self.assertIsNotNone(as_json)

        # The JSON should also be deserializable as well, and produce the same data.
        roundtripped = parser.parse_and_validate(api_models.AssetMetadataV1, as_json)
        self.assertEqual(meta, roundtripped)


class HashingTest(unittest.TestCase):
    def test_url_function(self) -> None:
        # No hash.
        url_with_hash = api_models.URLWithHash(
            url="http://localhost:8080/_v1/asset-index.json",
            hash=""
        )
        self.assertEqual("http://localhost:8080/_v1/asset-index.json", hashing.url(url_with_hash))

        # Hash without type, and to-be-quoted characters.
        url_with_hash.hash = "this is a weird häsh"
        self.assertEqual(
            "http://localhost:8080/_v1/asset-index.json?hash=this%20is%20a%20weird%20h%C3%A4sh",
            hashing.url(url_with_hash))

        # Hash with a type prefix, should be stripped.
        url_with_hash.hash = "sha1:2cafc9d388fb8c2d0b6ca9780d6b75963587916d"
        self.assertEqual(
            "http://localhost:8080/_v1/asset-index.json?hash=2cafc9d388fb8c2d0b6ca9780d6b75963587916d",
            hashing.url(url_with_hash))

        # Existing query string, should be correctly appended to.
        url_with_hash.url = "http://localhost:8080/_v1/asset-index.json?auth=none"
        self.assertEqual(
            "http://localhost:8080/_v1/asset-index.json?auth=none&hash=2cafc9d388fb8c2d0b6ca9780d6b75963587916d",
            hashing.url(url_with_hash))

        # Using a tuple instead of an URLWithHash object.
        self.assertEqual(
            "http://localhost:8080/_v1/asset-index.json?auth=none&hash=2cafc9d388fb8c2d0b6ca9780d6b75963587916d",
            hashing.url((
                "http://localhost:8080/_v1/asset-index.json?auth=none",
                "sha1:2cafc9d388fb8c2d0b6ca9780d6b75963587916d"
            )))


# This test is using pure paths to make it independent of the platform it runs on. However, since the code under test
# works with concrete paths, some type errors have to be silenced in this test.
class BlenderVersionFromFilenameTest(unittest.TestCase):
    maxDiff = None

    def assert_versions_per_filename(
        self,
        paths_in: list[PurePath],
        expect: list[tuple[PurePath, asset_finder.BlenderVersion, asset_finder.BlenderVersion]]
    ) -> None:
        """Helper to test the asset_finder.filenames_group_by_version_metadata() function."""

        expect = [
            asset_finder.FileVersionInfo(
                path=path,  # type: ignore
                blender_version_min=version_min,
                blender_version_until=version_until,
            )
            for path, version_min, version_until in expect
        ]
        actual = asset_finder.filenames_group_by_version_metadata(paths_in)
        self.assertEqual(expect, actual)

    def test_filenames_group_by_version_metadata(self) -> None:
        # One asset with three versions.
        # - Mis-matched case of `@b` shouldn't matter.
        # - Not given in version-order.
        self.assert_versions_per_filename([
            PurePosixPath("/path/to/library/subdir/filéname.blend"),
            PurePosixPath("/path/to/library/subdir/filéname@B6_0.blend"),
            PurePosixPath("/path/to/library/subdir/filéname@b5_3.blend"),
        ], [
            (PurePosixPath("/path/to/library/subdir/filéname.blend"), (), (5, 3)),
            (PurePosixPath("/path/to/library/subdir/filéname@b5_3.blend"), (5, 3), (6, 0)),
            (PurePosixPath("/path/to/library/subdir/filéname@B6_0.blend"), (6, 0), ()),
        ])

        # Major versions > 10:
        self.assert_versions_per_filename([
            PurePosixPath("/path/to/big/major.blend"),
            PurePosixPath("/path/to/big/major@b20_0.blend"),
            PurePosixPath("/path/to/big/major@b15_3.blend"),
        ], [
            (PurePosixPath("/path/to/big/major.blend"), (), (15, 3)),
            (PurePosixPath("/path/to/big/major@b15_3.blend"), (15, 3), (20, 0)),
            (PurePosixPath("/path/to/big/major@b20_0.blend"), (20, 0), ()),
        ])

        # Minor versions > 10:
        self.assert_versions_per_filename([
            PurePosixPath("/path/to/big/minor.blend"),
            PurePosixPath("/path/to/big/minor@b5_14.blend"),
            PurePosixPath("/path/to/big/minor@b5_3.blend"),
        ], [
            (PurePosixPath("/path/to/big/minor.blend"), (), (5, 3)),
            (PurePosixPath("/path/to/big/minor@b5_3.blend"), (5, 3), (5, 14)),
            (PurePosixPath("/path/to/big/minor@b5_14.blend"), (5, 14), ()),
        ])

        # Malformed marker.
        self.assert_versions_per_filename([
            PurePosixPath("/path/to/malformed.blend"),
            PurePosixPath("/path/to/malformed@b553.blend"),
        ], [
            (PurePosixPath("/path/to/malformed.blend"), (), ()),
            (PurePosixPath("/path/to/malformed@b553.blend"), (), ()),
        ])

        # No marker.
        self.assert_versions_per_filename([
            PurePosixPath("/path/to/alone.blend"),
        ], [
            (PurePosixPath("/path/to/alone.blend"), (), ()),
        ])

        # Only marker.
        self.assert_versions_per_filename([
            PurePosixPath("/path/to/@b5_5.blend"),
        ], [
            (PurePosixPath("/path/to/@b5_5.blend"), (), ()),
        ])

        # Double markers. These should be seen as two separate base names, as only
        # the last marker is stripped to form the base name.
        self.assert_versions_per_filename([
            PurePosixPath("/path/to/double@b4_5.blend"),
            PurePosixPath("/path/to/double@b5_3@b6_0.blend"),
        ], [
            (PurePosixPath("/path/to/double@b4_5.blend"), (4, 5), ()),
            (PurePosixPath("/path/to/double@b5_3@b6_0.blend"), (6, 0), ()),
        ])

        # No markerless first version.
        self.assert_versions_per_filename([
            PurePosixPath("/path/to/only_marked@b5_4.blend"),
            PurePosixPath("/path/to/only_marked@b6_0.blend"),
        ], [
            (PurePosixPath("/path/to/only_marked@b5_4.blend"), (5, 4), (6, 0)),
            (PurePosixPath("/path/to/only_marked@b6_0.blend"), (6, 0), ()),
        ])

    def test_filenames_group_by_version_metadata_duplicate(self) -> None:
        # Two files with the same min version:
        self.assert_versions_per_filename([
            PurePosixPath("/path/to/asset.blend"),
            PurePosixPath("/path/to/asset@b5_3.blend"),
            PurePosixPath("/path/to/asset@B5_3.blend"),
            PurePosixPath("/path/to/asset@b6_0.blend"),
        ], [
            (PurePosixPath("/path/to/asset.blend"), (), (5, 3)),
            (PurePosixPath("/path/to/asset@B5_3.blend"), (5, 3), (6, 0)),
            (PurePosixPath("/path/to/asset@b5_3.blend"), (5, 3), (6, 0)),
            (PurePosixPath("/path/to/asset@b6_0.blend"), (6, 0), ()),
        ])

    def test_filenames_group_by_version_metadata__posix(self) -> None:
        # POSIX paths are case sensitive:
        self.assert_versions_per_filename([
            PurePosixPath('/path/to/case/sensitive.blend'),
            PurePosixPath('/path/to/CASE/sensitive@b5_3.blend'),
            PurePosixPath('/path/to/case/SENSITIVE@B6_0.blend'),
        ], [
            (PurePosixPath('/path/to/case/sensitive.blend'), (), ()),
            (PurePosixPath('/path/to/CASE/sensitive@b5_3.blend'), (5, 3), ()),
            (PurePosixPath('/path/to/case/SENSITIVE@B6_0.blend'), (6, 0), ()),
        ])

    def test_filenames_group_by_version_metadata__windows(self) -> None:
        # One asset with three versions.
        # - Mis-matched case of `@b` shouldn't matter.
        # - Not given in version-order.
        self.assert_versions_per_filename([
            PureWindowsPath(r'C:\path\to\library\subdir\filéname.blend'),
            PureWindowsPath(r'C:\path\to\library\subdir\filéname@B6_0.blend'),
            PureWindowsPath(r'C:\path\to\library\subdir\filéname@b5_3.blend'),
        ], [
            (PureWindowsPath(r"C:\path\to\library\subdir\filéname.blend"), (), (5, 3)),
            (PureWindowsPath(r"C:\path\to\library\subdir\filéname@b5_3.blend"), (5, 3), (6, 0)),
            (PureWindowsPath(r"C:\path\to\library\subdir\filéname@B6_0.blend"), (6, 0), ()),
        ])

        # Same path on different drive:
        self.assert_versions_per_filename([
            PureWindowsPath(r'C:\path\to\drive\filéname.blend'),
            PureWindowsPath(r'D:\path\to\drive\filéname@B6_0.blend'),
            PureWindowsPath(r'C:\path\to\drive\filéname@b5_3.blend'),
        ], [
            (PureWindowsPath(r'C:\path\to\drive\filéname.blend'), (), (5, 3)),
            # No version, because next file is different cluster
            (PureWindowsPath(r'C:\path\to\drive\filéname@b5_3.blend'), (5, 3), ()),
            (PureWindowsPath(r'D:\path\to\drive\filéname@B6_0.blend'), (6, 0), ()),
        ])

        # Windows paths are not case sensitive:
        self.assert_versions_per_filename([
            PureWindowsPath(r'C:\path\to\case\insensitive.blend'),
            PureWindowsPath(r'C:\path\to\CASE\insensitive@b5_3.blend'),
            PureWindowsPath(r'C:\path\to\case\INSENSITIVE@B6_0.blend'),
        ], [
            (PureWindowsPath(r'C:\path\to\case\insensitive.blend'), (), (5, 3)),
            (PureWindowsPath(r'C:\path\to\CASE\insensitive@b5_3.blend'), (5, 3), (6, 0)),
            (PureWindowsPath(r'C:\path\to\case\INSENSITIVE@B6_0.blend'), (6, 0), ()),
        ])

    def test_asset_versions_min_until(self) -> None:
        # Expected case: written by minimum blend version, with 'until' version.
        actual = asset_finder._asset_versions_min_until(
            asset_finder.FileVersionInfo(
                path=PurePosixPath("/path/to/library/subdir/filéname@b5_3.blend"),
                blender_version_min=(5, 3),
                blender_version_until=(6, 0),
            ),
            file_written_version=(5, 3),
        )
        expected = api_models.AssetBlenderVersionsV1(min="5.3", until="6.0")
        self.assertEqual(expected, actual)

        # Expected case: written by minimum blend version, without 'until' version.
        actual = asset_finder._asset_versions_min_until(
            asset_finder.FileVersionInfo(
                path=PurePosixPath("/path/to/library/subdir/filéname@b5_3.blend"),
                blender_version_min=(5, 3),
                blender_version_until=(),
            ),
            file_written_version=(5, 3),
        )
        expected = api_models.AssetBlenderVersionsV1(min="5.3")
        self.assertEqual(expected, actual)

        # Expected case: written by minimum blend version, no versions in the filename.
        actual = asset_finder._asset_versions_min_until(
            asset_finder.FileVersionInfo(
                path=PurePosixPath("/path/to/library/subdir/filéname.blend"),
                blender_version_min=(),
                blender_version_until=(),
            ),
            file_written_version=(5, 3),
        )
        expected = api_models.AssetBlenderVersionsV1(min="5.3")
        self.assertEqual(expected, actual)

        # Weird case case: written by version newer than the minimal one.
        actual = asset_finder._asset_versions_min_until(
            asset_finder.FileVersionInfo(
                path=PurePosixPath("/path/to/library/subdir/filéname@b5_3.blend"),
                blender_version_min=(5, 3),
                blender_version_until=(6, 0),
            ),
            file_written_version=(5, 4),
        )

        expected = api_models.AssetBlenderVersionsV1(min="5.4", until="6.0")
        self.assertEqual(expected, actual)

        # Weird case case: written by version newer than the 'until' one.
        actual = asset_finder._asset_versions_min_until(
            asset_finder.FileVersionInfo(
                path=PurePosixPath("/path/to/library/subdir/filéname@b5_3.blend"),
                blender_version_min=(5, 3),
                blender_version_until=(6, 0),
            ),
            file_written_version=(6, 2),
        )

        expected = api_models.AssetBlenderVersionsV1(min="6.2", until="6.0")
        self.assertEqual(expected, actual)


def main():
    global args

    argv = [sys.argv[0]]
    if '--' in sys.argv:
        argv += sys.argv[sys.argv.index('--') + 1:]

    unittest.main(argv=argv)


if __name__ == "__main__":
    main()
