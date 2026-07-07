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
        self.gp = bpy.data.grease_pencils.new("test_grease_pencil")

    def tearDown(self):
        bpy.data.grease_pencils.remove(self.gp)
        del self.gp

    def test_grease_pencil_new(self):
        self.assertEqual(self.gp.name, "test_grease_pencil")
        self.assertEqual(len(self.gp.layers), 0)


class TestGreasePencilLayers(unittest.TestCase):
    tint_factors = [0.3, 0.6, 0.9]

    def setUp(self):
        self.gp = bpy.data.grease_pencils.new("test_grease_pencil")
        self.gp.layers.new("test_layer01")
        self.gp.layers.new("test_layer02")
        self.gp.layers.new("test_layer03")

        for i, layer in enumerate(self.gp.layers):
            layer.tint_factor = self.tint_factors[i]

    def tearDown(self):
        bpy.data.grease_pencils.remove(self.gp)
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
        self.gp.layers.move(self.gp.layers[-1], "DOWN")
        self.assertEqual(self.gp.layers[0].name, "test_layer01")
        self.assertEqual(self.gp.layers[1].name, "test_layer03")
        self.assertEqual(self.gp.layers[2].name, "test_layer02")

    def test_grease_pencil_layers_move_up(self):
        # Move the bottom most layer up
        self.gp.layers.move(self.gp.layers[0], "UP")
        self.assertEqual(self.gp.layers[0].name, "test_layer02")
        self.assertEqual(self.gp.layers[1].name, "test_layer01")
        self.assertEqual(self.gp.layers[2].name, "test_layer03")

    def test_grease_pencil_layers_attribute_reorder(self):
        layer = self.gp.layers[0]
        self.gp.layers.move_top(layer)
        # Check layer attribute
        self.assertEqual(round(self.gp.layers[0].tint_factor, 1), self.tint_factors[1])
        self.assertEqual(round(self.gp.layers[1].tint_factor, 1), self.tint_factors[2])
        self.assertEqual(round(self.gp.layers[2].tint_factor, 1), self.tint_factors[0])


class TestGreasePencilFrame(unittest.TestCase):
    def setUp(self):
        self.gp = bpy.data.grease_pencils.new("test_grease_pencil")
        self.layer = self.gp.layers.new("test_layer01")

    def tearDown(self):
        bpy.data.grease_pencils.remove(self.gp)
        del self.gp

    def test_grease_pencil_frame_add(self):
        frame1 = self.layer.frames.new(0)
        frame2 = self.layer.frames.new(-100)
        frame3 = self.layer.frames.new(10)
        with self.assertRaises(RuntimeError):
            self.layer.frames.new(10)

        self.assertEqual(len(self.layer.frames), 3)
        self.assertEqual(frame1.frame_number, 0)
        self.assertEqual(frame2.frame_number, -100)
        self.assertEqual(frame3.frame_number, 10)

    def test_grease_pencil_frame_remove(self):
        self.layer.frames.new(0)
        self.layer.frames.new(-10)
        frame3 = self.layer.frames.new(20)

        self.assertEqual(len(self.layer.frames), 3)

        self.layer.frames.remove(0)
        self.layer.frames.remove(-10)

        with self.assertRaises(RuntimeError):
            self.layer.frames.remove(19)

        self.assertEqual(len(self.layer.frames), 1)
        self.assertEqual(frame3.frame_number, 20)

    def test_grease_pencil_frame_copy(self):
        self.layer.frames.new(0)
        self.layer.frames.new(-10)
        self.layer.frames.new(20)

        self.assertEqual(len(self.layer.frames), 3)

        frame = self.layer.frames.copy(0, 1)

        self.assertEqual(len(self.layer.frames), 4)
        self.assertEqual(frame.frame_number, 1)

        with self.assertRaises(RuntimeError):
            self.layer.frames.copy(0, 1)

        with self.assertRaises(RuntimeError):
            self.layer.frames.copy(10, 20)

    def test_grease_pencil_frame_move(self):
        self.layer.frames.new(0)
        self.layer.frames.new(-10)
        self.layer.frames.new(20)

        self.assertEqual(len(self.layer.frames), 3)

        frame = self.layer.frames.move(0, 1)

        self.assertEqual(frame.frame_number, 1)

        with self.assertRaises(RuntimeError):
            self.layer.frames.move(0, 1)

        with self.assertRaises(RuntimeError):
            self.layer.frames.move(-10, 20)

    def test_grease_pencil_frame_set(self):
        frame1 = self.layer.frames.new(0)
        frame1.drawing.add_strokes([3, 5, 7, 11])

        self.assertEqual(len(frame1.drawing.strokes), 4)

        frame1.drawing = None
        self.assertEqual(len(frame1.drawing.strokes), 0)

        frame2 = self.layer.frames.new(10)
        frame2.drawing.add_strokes([13, 17])

        frame1.drawing = frame2.drawing
        self.assertEqual(len(frame1.drawing.strokes), 2)
        self.assertEqual(len(frame1.drawing.strokes[0].points), 13)
        self.assertEqual(len(frame1.drawing.strokes[1].points), 17)

        layer2 = self.gp.layers.new("test_layer02")
        frame2 = layer2.frames.new(0)
        frame2.drawing.add_strokes([19, 23])

        frame1.drawing = frame2.drawing
        self.assertEqual(len(frame1.drawing.strokes), 2)
        self.assertEqual(len(frame1.drawing.strokes[0].points), 19)
        self.assertEqual(len(frame1.drawing.strokes[1].points), 23)


class TestGreasePencilDrawing(unittest.TestCase):
    def setUp(self):
        self.gp = bpy.data.grease_pencils.new("test_grease_pencil")
        layer = self.gp.layers.new("test_layer01")
        frame = layer.frames.new(0)
        self.drawing = frame.drawing

        stroke_sizes = [3, 5, 7, 11]
        self.drawing.add_strokes(stroke_sizes)

    def tearDown(self):
        bpy.data.grease_pencils.remove(self.gp)
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


class TestGreasePencilRootNodes(unittest.TestCase):
    def setUp(self):
        self.gp = bpy.data.grease_pencils.new("test_gp")

        self.layer_a = self.gp.layers.new("LayerA")
        self.group_g = self.gp.layer_groups.new("GroupG")

        self.layer_b = self.gp.layers.new("LayerB")
        self.layer_c = self.gp.layers.new("LayerC")

        self.gp.layers.move_to_layer_group(self.layer_b, self.group_g)
        self.gp.layers.move_to_layer_group(self.layer_c, self.group_g)

        self.layer_d = self.gp.layers.new("LayerD")

    def tearDown(self):
        bpy.data.grease_pencils.remove(self.gp)

    def test_root_nodes_len_and_order(self):
        nodes = self.gp.root_nodes

        self.assertEqual(len(nodes), 3)
        self.assertEqual(nodes[0].name, "LayerA")
        self.assertEqual(nodes[1].name, "GroupG")
        self.assertEqual(nodes[2].name, "LayerD")

    def test_root_nodes_types(self):
        nodes = self.gp.root_nodes

        self.assertIsInstance(nodes[0], bpy.types.GreasePencilLayer)
        self.assertIsInstance(nodes[1], bpy.types.GreasePencilLayerGroup)
        self.assertIsInstance(nodes[2], bpy.types.GreasePencilLayer)


class TestGreasePencilLayerGroupChildren(unittest.TestCase):
    def setUp(self):
        self.gp = bpy.data.grease_pencils.new("test_gp")

        self.group = self.gp.layer_groups.new("Group")

        self.layer1 = self.gp.layers.new("Layer1")
        self.layer2 = self.gp.layers.new("Layer2")

        self.gp.layers.move_to_layer_group(self.layer1, self.group)
        self.gp.layers.move_to_layer_group(self.layer2, self.group)

        self.subgroup = self.gp.layer_groups.new("SubGroup")
        self.gp.layer_groups.move_to_layer_group(self.subgroup, self.group)

        self.sublayer = self.gp.layers.new("SubLayer")
        self.gp.layers.move_to_layer_group(self.sublayer, self.subgroup)

    def tearDown(self):
        bpy.data.grease_pencils.remove(self.gp)

    def test_children_basic(self):
        children = self.group.children

        self.assertEqual(len(children), 3)
        self.assertEqual(children[0].name, "Layer1")
        self.assertEqual(children[1].name, "Layer2")
        self.assertEqual(children[2].name, "SubGroup")

    def test_children_not_recursive(self):
        children = self.group.children
        names = {child.name for child in children}

        self.assertNotIn("SubLayer", names)

    def test_children_iteration(self):
        names = []
        for node in self.group.children:
            names.append(node.name)

        self.assertEqual(names, ["Layer1", "Layer2", "SubGroup"])


if __name__ == "__main__":
    import sys

    sys.argv = [__file__] + (
        sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    )
    unittest.main()
