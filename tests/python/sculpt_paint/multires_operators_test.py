# SPDX-FileCopyrightText: 2025 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later */
"""
blender -b -X -P tests/python/sculpt_paint/multires_operators_test.py -- --testdir tests/files/sculpting/
"""

__all__ = (
    "main",
)

import pathlib
import sys
import unittest

import bpy
from mathutils import Vector

args = None


class ApplyBase(unittest.TestCase):
    def setUp(self):
        bpy.ops.wm.open_mainfile(filepath=str(args.testdir / "apply_base_monkey.blend"), load_ui=False)

    def test_subdividing_cube_results_in_same_mesh(self):
        bpy.ops.mesh.primitive_monkey_add()
        new_cube = bpy.context.object

        multires_mod = new_cube.modifiers.new("Multires", 'MULTIRES')
        bpy.ops.object.multires_subdivide(modifier="Multires")
        bpy.ops.object.multires_base_apply(modifier="Multires")
        bpy.ops.object.modifier_remove(modifier="Multires")

        expected_mesh = bpy.data.objects['Expected_Base_Mesh']
        result = expected_mesh.data.unit_test_compare(mesh=new_cube.data)
        self.assertEqual(result, 'Same')


class Unsubdivide(unittest.TestCase):
    def setUp(self):
        bpy.ops.wm.read_factory_settings(use_empty=True)

    @staticmethod
    def _sorted_positions(mesh, places=5):
        return sorted(tuple(round(c, places) for c in v.co) for v in mesh.vertices)

    @staticmethod
    def _mesh_volume(mesh):
        import bmesh
        bm = bmesh.new()
        bm.from_mesh(mesh)
        volume = bm.calc_volume()
        bm.free()
        return volume

    def test_subdivided_cube_round_trips(self):
        # Extracts grid data from the vertices.
        bpy.ops.mesh.primitive_cube_add()
        ob_cube = bpy.context.object

        bpy.ops.object.mode_set(mode='EDIT')
        bpy.ops.mesh.select_all(action='SELECT')
        bpy.ops.mesh.subdivide()
        bpy.ops.object.mode_set(mode='OBJECT')

        self.assertEqual(len(ob_cube.data.vertices), 26)
        self.assertAlmostEqual(self._mesh_volume(ob_cube.data), 8.0, places=5)
        subdivided_positions = self._sorted_positions(ob_cube.data)

        mod = ob_cube.modifiers.new(name="Multires", type='MULTIRES')
        result = bpy.ops.object.multires_unsubdivide(modifier=mod.name)
        self.assertEqual(result, {'FINISHED'})

        self.assertEqual(len(ob_cube.data.vertices), 8)
        self.assertAlmostEqual(self._mesh_volume(ob_cube.data), 8.0, places=5)

        mod.levels = 1
        mod.sculpt_levels = 1
        mod.render_levels = 1
        bpy.ops.object.modifier_apply(modifier=mod.name)

        self.assertEqual(len(ob_cube.data.vertices), 26)
        self.assertAlmostEqual(self._mesh_volume(ob_cube.data), 8.0, places=5)
        self.assertEqual(self._sorted_positions(ob_cube.data), subdivided_positions)

    def test_twice_subdivided_cube_round_trips(self):
        # The second un-subdivide call ensures it extracts grid data from the "grids".
        bpy.ops.mesh.primitive_cube_add()
        ob_cube = bpy.context.object

        bpy.ops.object.mode_set(mode='EDIT')
        bpy.ops.mesh.select_all(action='SELECT')
        bpy.ops.mesh.subdivide(number_cuts=3)
        bpy.ops.object.mode_set(mode='OBJECT')

        self.assertEqual(len(ob_cube.data.vertices), 98)
        self.assertAlmostEqual(self._mesh_volume(ob_cube.data), 8.0, places=5)
        subdivided_positions = self._sorted_positions(ob_cube.data)

        mod = ob_cube.modifiers.new(name="Multires", type='MULTIRES')
        result = bpy.ops.object.multires_unsubdivide(modifier=mod.name)
        self.assertEqual(result, {'FINISHED'})
        self.assertEqual(len(ob_cube.data.vertices), 26)
        self.assertAlmostEqual(self._mesh_volume(ob_cube.data), 8.0, places=5)

        result = bpy.ops.object.multires_unsubdivide(modifier=mod.name)
        self.assertEqual(result, {'FINISHED'})
        self.assertEqual(len(ob_cube.data.vertices), 8)
        self.assertAlmostEqual(self._mesh_volume(ob_cube.data), 8.0, places=5)

        mod.levels = mod.total_levels
        mod.sculpt_levels = mod.total_levels
        mod.render_levels = mod.total_levels
        bpy.ops.object.modifier_apply(modifier=mod.name)

        self.assertEqual(len(ob_cube.data.vertices), 98)
        self.assertAlmostEqual(self._mesh_volume(ob_cube.data), 8.0, places=5)
        self.assertEqual(self._sorted_positions(ob_cube.data), subdivided_positions)

    def test_unsubdivide_with_mirrored_coincident_faces(self):
        # Regression test for #158032: when `multires_unsubdivide` runs on topology containing edges shared by 4 faces.
        # The mirror plane creates this scenario, then ensure un-subdivide handles this gracefully.
        bpy.ops.mesh.primitive_cube_add()
        ob_cube = bpy.context.object

        mod = ob_cube.modifiers.new(name="Multires", type='MULTIRES')
        bpy.ops.object.multires_subdivide(modifier=mod.name)
        bpy.ops.object.multires_subdivide(modifier=mod.name)
        mod.levels = mod.total_levels
        mod.sculpt_levels = mod.total_levels
        mod.render_levels = mod.total_levels
        bpy.ops.object.modifier_apply(modifier=mod.name)
        self.assertEqual(len(ob_cube.data.vertices), 98)

        mod_mirror = ob_cube.modifiers.new(name="Mirror", type='MIRROR')
        bpy.ops.object.modifier_apply(modifier=mod_mirror.name)
        self.assertEqual(len(ob_cube.data.vertices), 180)

        mod = ob_cube.modifiers.new(name="Multires", type='MULTIRES')
        # First call takes the vertex-extraction path.
        result = bpy.ops.object.multires_unsubdivide(modifier=mod.name)
        self.assertEqual(result, {'FINISHED'})
        self.assertEqual(len(ob_cube.data.vertices), 44)
        self.assertEqual(mod.total_levels, 1)
        # Second call takes the grid-extraction path.
        # Some grids fail to walk along the 4-face-edge topology - the operator must
        # detect that and bail out of those grids instead of dereferencing a null edge.
        result = bpy.ops.object.multires_unsubdivide(modifier=mod.name)
        self.assertEqual(result, {'FINISHED'})
        self.assertEqual(len(ob_cube.data.vertices), 18)
        self.assertEqual(mod.total_levels, 2)

    def test_reshape_cube_to_sphere(self):
        # Test grid-data is properly extracted from MDISPS, keeping the shape.

        # Subdivide the base so the following `multires_unsubdivide` has a coarser cube to reduce to
        # (an 8-vert cube cannot be un-subdivided).
        bpy.ops.mesh.primitive_cube_add()
        ob_cube = bpy.context.object
        bpy.ops.object.mode_set(mode='EDIT')
        bpy.ops.mesh.select_all(action='SELECT')
        bpy.ops.mesh.subdivide()
        bpy.ops.object.mode_set(mode='OBJECT')

        # One multi-resolution level above the base, since `multires_reshape` needs
        # a top level to project onto.
        mod = ob_cube.modifiers.new(name="Multires", type='MULTIRES')
        bpy.ops.object.multires_subdivide(modifier=mod.name)

        # Build the reshape source by duplicating the cube and make it sphere-shaped it.
        # `multires_reshape` requires the source to have the same vert count as the multi-resolution top level,
        # which a primitive sphere cannot match exactly.
        bpy.ops.object.duplicate()
        ob_src = bpy.context.object
        mod_src_name = ob_src.modifiers[0].name
        ob_src.modifiers[mod_src_name].levels = ob_src.modifiers[mod_src_name].total_levels
        bpy.ops.object.modifier_apply(modifier=mod_src_name)
        self.assertEqual(len(ob_src.data.vertices), 98)
        for v in ob_src.data.vertices:
            v.co.normalize()

        # Verify the source volume - avoid confusing test failure if it ever changes.
        ob_src_volume = self._mesh_volume(ob_src.data)
        self.assertAlmostEqual(ob_src_volume, 3.8898128, places=4)

        # Reshape projects the cube's multi-resolution top level onto the sphere-shaped source.
        # Afterwards the cube's MDISPS encode the sphere displacement.
        bpy.ops.object.select_all(action='DESELECT')
        ob_src.select_set(True)
        ob_cube.select_set(True)
        bpy.context.view_layer.objects.active = ob_cube

        result = bpy.ops.object.multires_reshape(modifier=mod.name)
        self.assertEqual(result, {'FINISHED'})

        # Validate reshape on a duplicate (apply modifier, check sphere geometry) so the original
        # cube + modifier stay intact for the `multires_unsubdivide` round-trip below.
        bpy.ops.object.select_all(action='DESELECT')
        ob_cube.select_set(True)
        bpy.context.view_layer.objects.active = ob_cube
        bpy.ops.object.duplicate()
        ob_cube_copy = bpy.context.object
        mod_copy = ob_cube_copy.modifiers[0]
        mod_copy.levels = mod_copy.total_levels
        bpy.ops.object.modifier_apply(modifier=mod_copy.name)
        self.assertEqual(len(ob_cube_copy.data.vertices), 98)
        for v in ob_cube_copy.data.vertices:
            self.assertAlmostEqual(v.co.length, 1.0, places=4)
        self.assertAlmostEqual(self._mesh_volume(ob_cube_copy.data), ob_src_volume, places=4)
        bpy.data.objects.remove(ob_cube_copy, do_unlink=True)

        # Round-trip the MDISPS through `multires_unsubdivide` on a duplicate:
        # this tests grid-extraction with the non-trivial sphere-shaped MDISPS populated by reshape.
        # Applying at the new top level should reconstruct the same sphere.
        bpy.ops.object.select_all(action='DESELECT')
        ob_cube.select_set(True)
        bpy.context.view_layer.objects.active = ob_cube
        bpy.ops.object.duplicate()
        ob_unsubdiv = bpy.context.object
        mod_unsubdiv = ob_unsubdiv.modifiers[0]
        result = bpy.ops.object.multires_unsubdivide(modifier=mod_unsubdiv.name)
        self.assertEqual(result, {'FINISHED'})
        self.assertEqual(len(ob_unsubdiv.data.vertices), 8)

        mod_unsubdiv.levels = mod_unsubdiv.total_levels
        mod_unsubdiv.sculpt_levels = mod_unsubdiv.total_levels
        mod_unsubdiv.render_levels = mod_unsubdiv.total_levels
        bpy.ops.object.modifier_apply(modifier=mod_unsubdiv.name)
        self.assertEqual(len(ob_unsubdiv.data.vertices), 98)
        for v in ob_unsubdiv.data.vertices:
            self.assertAlmostEqual(v.co.length, 1.0, places=4)
        self.assertAlmostEqual(self._mesh_volume(ob_unsubdiv.data), ob_src_volume, places=4)


def main():
    global args
    import argparse

    argv = [sys.argv[0]]
    if '--' in sys.argv:
        argv += sys.argv[sys.argv.index('--') + 1:]

    parser = argparse.ArgumentParser()
    parser.add_argument('--testdir', required=True, type=pathlib.Path)

    args, remaining = parser.parse_known_args(argv)

    unittest.main(argv=remaining)


if __name__ == '__main__':
    main()
