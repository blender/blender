# SPDX-FileCopyrightText: 2020-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import unittest
import sys

import bpy

"""
blender -b --factory-startup --python tests/python/bl_animation_id.py
"""


class AnimationIDAssignmentTest(unittest.TestCase):
    """Test assigning animations & check reference counts."""

    def test_animation_id_assignment(self):
        # Create new animation datablock.
        anim = bpy.data.animations.new('TestAnim')
        self.assertEqual(0, anim.users)

        # Assign the animation to the cube,
        cube = bpy.data.objects['Cube']
        cube_adt = cube.animation_data_create()
        cube_adt.animation = anim
        self.assertEqual(1, anim.users)

        # Assign the animation to the camera as well.
        camera = bpy.data.objects['Camera']
        camera_adt = camera.animation_data_create()
        camera_adt.animation = anim
        self.assertEqual(2, anim.users)

        # Unassigning should decrement the user count.
        cube_adt.animation = None
        self.assertEqual(1, anim.users)

        # Deleting the camera should also decrement the user count.
        bpy.data.objects.remove(camera)
        self.assertEqual(0, anim.users)

    def test_animation_binding_assignment(self):
        # Create new animation datablock.
        anim = bpy.data.animations.new('TestAnim')
        self.assertEqual(0, anim.users)

        # Assign the animation to the cube,
        cube = bpy.data.objects['Cube']
        cube_adt = cube.animation_data_create()
        cube_adt.animation = anim
        bind_cube = anim.bindings.new(for_id=cube)
        cube_adt.animation_binding_handle = bind_cube.handle
        self.assertEqual(cube_adt.animation_binding_handle, bind_cube.handle)

        # Assign the animation to the camera as well.
        camera = bpy.data.objects['Camera']
        bind_camera = anim.bindings.new(for_id=camera)
        camera_adt = camera.animation_data_create()
        camera_adt.animation = anim
        self.assertEqual(camera_adt.animation_binding_handle, bind_camera.handle)

        # Unassigning should keep the binding name.
        cube_adt.animation = None
        self.assertEqual(cube_adt.animation_binding_name, bind_cube.name)

        # It should not be possible to set the binding handle while the animation is unassigned.
        bind_extra = anim.bindings.new()
        cube_adt.animation_binding_handle = bind_extra.handle
        self.assertEqual(cube_adt.animation_binding_handle, bind_cube.handle)

        # Clearing out the binding handle should be fine though.
        cube_adt.animation_binding_handle = 0
        self.assertEqual(cube_adt.animation_binding_handle, 0)


class LimitationsTest(unittest.TestCase):
    """Test artificial limitations for the Animation data-block.

    Certain limitations are in place to keep development & testing focused.
    """

    def setUp(self):
        anims = bpy.data.animations
        while anims:
            anims.remove(anims[0])

    def test_initial_layers(self):
        """Test that upon creation an Animation has no layers/strips."""
        anim = bpy.data.animations.new('TestAnim')
        self.assertEqual([], anim.layers[:])

    def test_limited_layers_strips(self):
        """Test that there can only be one layer with one strip."""

        anim = bpy.data.animations.new('TestAnim')
        layer = anim.layers.new(name="Layer")
        self.assertEqual([], layer.strips[:])
        strip = layer.strips.new(type='KEYFRAME')

        # Adding a 2nd layer should be forbidden.
        with self.assertRaises(RuntimeError):
            anim.layers.new(name="Forbidden Layer")
        self.assertEqual([layer], anim.layers[:])

        # Adding a 2nd strip should be forbidden.
        with self.assertRaises(RuntimeError):
            layer.strips.new(type='KEYFRAME')
        self.assertEqual([strip], layer.strips[:])

    def test_limited_strip_api(self):
        """Test that strips have no frame start/end/offset properties."""

        anim = bpy.data.animations.new('TestAnim')
        layer = anim.layers.new(name="Layer")
        strip = layer.strips.new(type='KEYFRAME')

        self.assertFalse(hasattr(strip, 'frame_start'))
        self.assertFalse(hasattr(strip, 'frame_end'))
        self.assertFalse(hasattr(strip, 'frame_offset'))


class DataPathTest(unittest.TestCase):
    def setUp(self):
        anims = bpy.data.animations
        while anims:
            anims.remove(anims[0])

    def test_repr(self):
        anim = bpy.data.animations.new('TestAnim')

        layer = anim.layers.new(name="Layer")
        self.assertEqual("bpy.data.animations['TestAnim'].layers[\"Layer\"]", repr(layer))

        strip = layer.strips.new(type='KEYFRAME')
        self.assertEqual("bpy.data.animations['TestAnim'].layers[\"Layer\"].strips[0]", repr(strip))


def main():
    global args
    import argparse

    argv = [sys.argv[0]]
    if '--' in sys.argv:
        argv += sys.argv[sys.argv.index('--') + 1:]

    parser = argparse.ArgumentParser()
    args, remaining = parser.parse_known_args(argv)

    unittest.main(argv=remaining)


if __name__ == "__main__":
    main()
