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


class TestGreasePencilDrawing(unittest.TestCase):
    def setUp(self):
        self.gp = bpy.data.grease_pencils_v3.new("test_grease_pencil")
        layer = self.gp.layers.new("test_layer01")
        frame = layer.frames.new(0)
        self.drawing = frame.drawing

        stroke_sizes = [3, 5, 7, 11]
        self.drawing.add_strokes(stroke_sizes)

    def tearDown(self):
        bpy.data.grease_pencils_v3.remove(self.gp)
        del self.gp

    def test_grease_pencil_drawing_add_strokes(self):
        self.assertEqual(len(self.drawing.strokes), 4)
        self.assertEqual(len(self.drawing.strokes[0].points), 3)
        self.assertEqual(len(self.drawing.strokes[1].points), 5)
        self.assertEqual(len(self.drawing.strokes[2].points), 7)
        self.assertEqual(len(self.drawing.strokes[3].points), 11)

    def test_grease_pencil_drawing_remove_all_strokes(self):
        self.drawing.remove_strokes()
        self.assertEqual(len(self.drawing.strokes), 0)

    def test_grease_pencil_drawing_remove_strokes(self):
        self.drawing.remove_strokes(indices=[0, 2])
        self.assertEqual(len(self.drawing.strokes), 2)
        self.assertEqual(len(self.drawing.strokes[0].points), 5)
        self.assertEqual(len(self.drawing.strokes[1].points), 11)

    def test_grease_pencil_drawing_resize_strokes(self):
        self.drawing.resize_strokes([20, 25, 15, 30])
        self.assertEqual(len(self.drawing.strokes), 4)
        self.assertEqual(len(self.drawing.strokes[0].points), 20)
        self.assertEqual(len(self.drawing.strokes[1].points), 25)
        self.assertEqual(len(self.drawing.strokes[2].points), 15)
        self.assertEqual(len(self.drawing.strokes[3].points), 30)

        self.drawing.resize_strokes([1, 2], indices=[0, 2])
        self.assertEqual(len(self.drawing.strokes), 4)
        self.assertEqual(len(self.drawing.strokes[0].points), 1)
        self.assertEqual(len(self.drawing.strokes[1].points), 25)
        self.assertEqual(len(self.drawing.strokes[2].points), 2)
        self.assertEqual(len(self.drawing.strokes[3].points), 30)

    def test_grease_pencil_drawing_stroke_add_points(self):
        self.drawing.strokes[0].add_points(5)
        self.assertEqual(len(self.drawing.strokes[0].points), 8)

        new_points = self.drawing.strokes[1].add_points(5)
        self.assertEqual(len(new_points), 5)

    def test_grease_pencil_drawing_stroke_remove_points(self):
        self.drawing.strokes[0].remove_points(1)
        self.assertEqual(len(self.drawing.strokes[0].points), 2)

        self.drawing.strokes[1].remove_points(10)
        self.assertEqual(len(self.drawing.strokes[1].points), 1)

    def test_grease_pencil_drawing_strokes_slice(self):
        self.assertEqual(len(self.drawing.strokes[:]), 4)
        self.assertEqual(len(self.drawing.strokes[0:]), 4)
        self.assertEqual(len(self.drawing.strokes[:4]), 4)
        self.assertEqual(len(self.drawing.strokes[-4:]), 4)

        self.assertEqual(len(self.drawing.strokes[:5]), 4)
        self.assertEqual(len(self.drawing.strokes[-5:]), 4)

        self.assertEqual(len(self.drawing.strokes[4:]), 0)
        self.assertEqual(len(self.drawing.strokes[:0]), 0)
        self.assertEqual(len(self.drawing.strokes[:-4]), 0)

        strokes = self.drawing.strokes[1:]
        self.assertEqual(len(strokes), 3)
        self.assertEqual(len(strokes[0].points), 5)
        self.assertEqual(len(strokes[1].points), 7)
        self.assertEqual(len(strokes[2].points), 11)

        strokes = self.drawing.strokes[:-1]
        self.assertEqual(len(strokes), 3)
        self.assertEqual(len(strokes[0].points), 3)
        self.assertEqual(len(strokes[1].points), 5)
        self.assertEqual(len(strokes[2].points), 7)

        strokes = self.drawing.strokes[:1]
        self.assertEqual(len(strokes), 1)
        self.assertEqual(len(strokes[0].points), 3)

        strokes = self.drawing.strokes[-1:]
        self.assertEqual(len(strokes), 1)
        self.assertEqual(len(strokes[0].points), 11)

        strokes = self.drawing.strokes[1:-1]
        self.assertEqual(len(strokes), 2)
        self.assertEqual(len(strokes[0].points), 5)
        self.assertEqual(len(strokes[1].points), 7)

        strokes = self.drawing.strokes[1:][1:]
        self.assertEqual(len(strokes), 2)
        self.assertEqual(len(strokes[0].points), 7)
        self.assertEqual(len(strokes[1].points), 11)

        strokes = self.drawing.strokes[:-1][1:]
        self.assertEqual(len(strokes), 2)
        self.assertEqual(len(strokes[0].points), 5)
        self.assertEqual(len(strokes[1].points), 7)

        strokes = self.drawing.strokes[1:-1][1:]
        self.assertEqual(len(strokes), 1)
        self.assertEqual(len(strokes[0].points), 7)

        strokes = self.drawing.strokes[1:-1][:-1]
        self.assertEqual(len(strokes), 1)
        self.assertEqual(len(strokes[0].points), 5)


if __name__ == '__main__':
    import sys
    sys.argv = [__file__] + (sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else [])
    unittest.main()
