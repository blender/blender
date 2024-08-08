# SPDX-FileCopyrightText: 2024 Blender Authors
#
# SPDX-License-Identifier: Apache-2.0

# ./blender.bin --background --python tests/python/bl_pyapi_grease_pencil.py -- --verbose
import bpy
import unittest


# -----------------------------------------------------------------------------
# Tests

class TestGreasePencil(unittest.TestCase):
    def setUp(self):
        self.gp = bpy.data.grease_pencils_v3.new("test_grease_pencil")

    def tearDown(self):
        bpy.data.grease_pencils_v3.remove(self.gp)
        del self.gp

    def test_grease_pencil_new(self):
        self.assertEqual(self.gp.name, "test_grease_pencil")
        self.assertEqual(len(self.gp.layers), 0)


class TestGreasePencilLayers(unittest.TestCase):
    def setUp(self):
        self.gp = bpy.data.grease_pencils_v3.new("test_grease_pencil")
        self.gp.layers.new("test_layer01")
        self.gp.layers.new("test_layer02")
        self.gp.layers.new("test_layer03")

    def tearDown(self):
        bpy.data.grease_pencils_v3.remove(self.gp)
        del self.gp

    def test_grease_pencil_layers_new(self):
        self.assertEqual(len(self.gp.layers), 3)
        # Test empty name
        self.gp.layers.new("")
        self.assertEqual(self.gp.layers[-1].name, "Layer")
        self.gp.layers.new("")
        self.assertEqual(self.gp.layers[-1].name, "Layer.001")

    def test_grease_pencil_layers_rename(self):
        self.gp.layers[0].name = "test"
        self.assertEqual(self.gp.layers[0].name, "test")
        self.gp.layers[0].name = ""
        self.assertEqual(self.gp.layers[0].name, "Layer")
        self.gp.layers[0].name = "test_layer02"
        self.assertEqual(self.gp.layers[0].name, "test_layer02.001")

    def test_grease_pencil_layers_remove(self):
        self.gp.layers.remove(self.gp.layers[-1])
        self.assertEqual(len(self.gp.layers), 2)
        self.assertEqual(self.gp.layers[-1].name, "test_layer02")

    def test_grease_pencil_layers_move_down(self):
        # Move the top most layer down
        self.gp.layers.move(self.gp.layers[-1], 'DOWN')
        self.assertEqual(self.gp.layers[0].name, "test_layer01")
        self.assertEqual(self.gp.layers[1].name, "test_layer03")
        self.assertEqual(self.gp.layers[2].name, "test_layer02")

    def test_grease_pencil_layers_move_up(self):
        # Move the bottom most layer up
        self.gp.layers.move(self.gp.layers[0], 'UP')
        self.assertEqual(self.gp.layers[0].name, "test_layer02")
        self.assertEqual(self.gp.layers[1].name, "test_layer01")
        self.assertEqual(self.gp.layers[2].name, "test_layer03")


if __name__ == '__main__':
    import sys
    sys.argv = [__file__] + (sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else [])
    unittest.main()
