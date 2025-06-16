# SPDX-FileCopyrightText: 2025 Blender Authors
#
# SPDX-License-Identifier: Apache-2.0

# ./blender.bin --background --python tests/python/bl_pyapi_bmesh.py -- --verbose
import bmesh
import unittest


class TestBMeshBasic(unittest.TestCase):

    def test_create_uvsphere(self):
        bm = bmesh.new()
        bmesh.ops.create_uvsphere(
            bm,
            u_segments=8,
            v_segments=5,
            radius=1.0,
        )

        self.assertEqual(len(bm.verts), 34)
        self.assertEqual(len(bm.edges), 72)
        self.assertEqual(len(bm.faces), 40)

        bm.free()


if __name__ == "__main__":
    import sys
    sys.argv = [__file__] + (sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else [])
    unittest.main()
