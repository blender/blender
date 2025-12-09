# SPDX-FileCopyrightText: 2020-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
blender -b --factory-startup --python tests/python/bl_animation_shapekey.py -- --testdir /tests/files/animation
"""
__all__ = (
    "main",
)

import unittest
import bpy
import pathlib
import sys


class ObjectShapekeyTest(unittest.TestCase):

    def setUp(self) -> None:
        bpy.ops.wm.read_homefile(use_factory_startup=True)
        self.cube = bpy.data.objects["Cube"]

    def test_evaluation_result(self):
        self.cube.data.vertices[0].co = (1, 1, 1)

        self.cube.shape_key_add(name="Base")
        key_1 = self.cube.shape_key_add(name="one")
        key_2 = self.cube.shape_key_add(name="two")
        key_1.data[0].co = (10, 10, 10)
        key_2.data[0].co = (2, 2, 2)

        key_1.value = 0.0
        key_2.value = 0.0
        depsgraph = bpy.context.evaluated_depsgraph_get()
        eval_cube = self.cube.evaluated_get(depsgraph)
        self.assertEqual(eval_cube.data.vertices[0].co[0], 1)
        self.assertEqual(eval_cube.data.vertices[0].co[1], 1)
        self.assertEqual(eval_cube.data.vertices[0].co[2], 1)

        key_1.value = 1.0
        depsgraph = bpy.context.evaluated_depsgraph_get()
        eval_cube = self.cube.evaluated_get(depsgraph)
        self.assertEqual(eval_cube.data.vertices[0].co[0], 10)
        self.assertEqual(eval_cube.data.vertices[0].co[1], 10)
        self.assertEqual(eval_cube.data.vertices[0].co[2], 10)

        key_2.value = 1.0
        depsgraph = bpy.context.evaluated_depsgraph_get()
        eval_cube = self.cube.evaluated_get(depsgraph)
        # Shapekeys are additive by default.
        self.assertEqual(eval_cube.data.vertices[0].co[0], 11)
        self.assertEqual(eval_cube.data.vertices[0].co[1], 11)
        self.assertEqual(eval_cube.data.vertices[0].co[2], 11)


class CurveShapekeyTest(unittest.TestCase):

    def setUp(self) -> None:
        bpy.ops.wm.read_homefile(use_factory_startup=True)
        self.curve_data = bpy.data.curves.new("curve_data", 'CURVE')
        self.curve_ob = bpy.data.objects.new("curve_ob", self.curve_data)
        self.bezier_spline = self.curve_data.splines.new('BEZIER')
        # Splines always have one point created by default.
        # If this is not the case, the shapekey indices in the test cases will break.
        self.assertEqual(len(self.bezier_spline.bezier_points), 1)

        # A single curve can contain both bezier and nurbs splines. Both can be
        # targeted by a single shapekey.
        self.nurbs_spline = self.curve_data.splines.new('NURBS')
        self.assertEqual(len(self.nurbs_spline.points), 1)

        bpy.context.collection.objects.link(self.curve_ob)
        bpy.context.view_layer.objects.active = self.curve_ob
        self.curve_ob.select_set(True)
        self.bezier_spline.bezier_points[0].co = (1, 1, 1)
        self.nurbs_spline.points[0].co = (2, 2, 2, 1)

    def test_evaluation_result(self):
        self.curve_ob.shape_key_add(name="Base")
        key_1 = self.curve_ob.shape_key_add(name="one")
        key_2 = self.curve_ob.shape_key_add(name="two")

        key_1.data[0].co = (10, 10, 10)
        key_2.data[0].co = (4, 4, 4)

        key_1.data[1].co = (5, 5, 5)
        key_2.data[1].co = (4, 4, 4)

        key_1.value = 0.0
        key_2.value = 0.0

        depsgraph = bpy.context.evaluated_depsgraph_get()
        eval_curve = self.curve_ob.evaluated_get(depsgraph)
        bpy.ops.object.mode_set(mode='EDIT')
        self.assertEqual(eval_curve.data.splines[0].bezier_points[0].co[0], 1)
        self.assertEqual(eval_curve.data.splines[0].bezier_points[0].co[1], 1)
        self.assertEqual(eval_curve.data.splines[0].bezier_points[0].co[2], 1)
        self.assertEqual(eval_curve.data.splines[1].points[0].co[0], 2)
        self.assertEqual(eval_curve.data.splines[1].points[0].co[1], 2)
        self.assertEqual(eval_curve.data.splines[1].points[0].co[2], 2)
        self.assertEqual(eval_curve.data.splines[1].points[0].co[3], 1)
        bpy.ops.object.mode_set(mode='OBJECT')

        # This test is incomplete and blocked by a bug in the python API.
        # For curves, it fails to return the evaluated coordinates. See #150973.


class LatticeShapekeyTest(unittest.TestCase):

    def setUp(self) -> None:
        bpy.ops.wm.read_homefile(use_factory_startup=True)
        self.lattice = bpy.data.lattices.new("lattice_data")
        self.lattice_ob = bpy.data.objects.new("lattice_ob", self.lattice)
        bpy.context.collection.objects.link(self.lattice_ob)

    def test_evaluation_result(self):
        self.lattice_ob.shape_key_add(name="Base")
        key_1 = self.lattice_ob.shape_key_add(name="one")
        key_2 = self.lattice_ob.shape_key_add(name="two")

        key_1.data[0].co = (10, 10, 10)
        key_2.data[0].co = (2, 2, 2)

        key_1.value = 0.0
        key_2.value = 0.0
        depsgraph = bpy.context.evaluated_depsgraph_get()
        eval_lattice = self.lattice_ob.evaluated_get(depsgraph)
        self.assertEqual(eval_lattice.data.points[0].co[0], -0.5)
        self.assertEqual(eval_lattice.data.points[0].co[1], -0.5)
        self.assertEqual(eval_lattice.data.points[0].co[2], -0.5)

        # Incomplete test, same issue as CurveShapekeyTest.


def main():
    global args
    import argparse

    if '--' in sys.argv:
        argv = [sys.argv[0]] + sys.argv[sys.argv.index('--') + 1:]
    else:
        argv = sys.argv

    parser = argparse.ArgumentParser()
    parser.add_argument('--testdir', required=True, type=pathlib.Path)
    args, remaining = parser.parse_known_args(argv)

    unittest.main(argv=remaining)


if __name__ == "__main__":
    main()
