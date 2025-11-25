# SPDX-FileCopyrightText: 2025 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import sys
import unittest

from _bpy_internal.assets.remote_library_listing import asset_finder
from _bpy_internal.assets.remote_library_listing import blender_asset_library_openapi as api_models

import bpy


"""
blender -b --factory-startup --python tests/python/assets/remote_library_listing/listing_generator_test.py
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
        expected_custom = {
            'amazing': Prop(type=Types.BOOLEAN, value=True),
            'barcode': Prop(type=Types.STRING, value='155366'),
            'count': Prop(type=Types.INT, value=47),
            'location': Prop(type=Types.STRING, value='café'),
            'size': Prop(type=Types.FLOAT, value=32.7),
            'dimensions': Prop(type=Types.ARRAY,
                               value=[2.0, 2.0, 2.0],
                               itemtype=Types.FLOAT),
        }

        assert meta is not None
        self.assertEqual(expected_custom, meta.custom)

    def test_array_properties(self) -> None:
        asset_data = self.cube.asset_data

        asset_data["agents"] = ["007", "47", "327"]
        asset_data["locations"] = ["Hokkaido", "Santa Fortuna", "Sapienza"]
        asset_data["boundingbox"] = [-3.0, -4.0, -0.1, 1, 2, 3]

        meta = asset_finder._get_asset_meta(asset_data)

        Types = api_models.CustomPropertyTypeV1
        Prop = api_models.CustomPropertyV1
        expected_custom = {
            'agents': Prop(type=Types.ARRAY, value=["007", "47", "327"], itemtype=Types.STRING),
            'locations': Prop(type=Types.ARRAY, value=["Hokkaido", "Santa Fortuna", "Sapienza"], itemtype=Types.STRING),
            'boundingbox': Prop(type=Types.ARRAY, value=[-3.0, -4.0, -0.1, 1.0, 2.0, 3.0], itemtype=Types.FLOAT),
            'dimensions': Prop(type=Types.ARRAY,
                               value=[2.0, 2.0, 2.0],
                               itemtype=Types.FLOAT),
        }

        assert meta is not None
        self.assertEqual(expected_custom, meta.custom)

    def test_serialize_to_json(self) -> None:
        import cattrs.preconf.json

        meta = asset_finder._get_asset_meta(self.cube.asset_data)

        converter = cattrs.preconf.json.JsonConverter(omit_if_default=True)
        as_json = converter.dumps(meta, indent=2)
        self.assertIsNotNone(as_json)


def main():
    global args

    argv = [sys.argv[0]]
    if '--' in sys.argv:
        argv += sys.argv[sys.argv.index('--') + 1:]

    unittest.main(argv=argv)


if __name__ == "__main__":
    main()
