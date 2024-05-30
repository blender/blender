# SPDX-FileCopyrightText: 2020-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import unittest
import sys

import bpy

"""
blender -b --factory-startup --python tests/python/bl_animation_action.py
"""


class ActionBindingAssignmentTest(unittest.TestCase):
    """Test assigning actions & check reference counts."""

    def setUp(self) -> None:
        bpy.ops.wm.read_homefile(use_factory_startup=True)

    def test_action_assignment(self):
        # Create new Action.
        anim = bpy.data.actions.new('TestAction')
        self.assertEqual(0, anim.users)

        # Assign the animation to the cube,
        cube = bpy.data.objects['Cube']
        cube_adt = cube.animation_data_create()
        cube_adt.action = anim
        self.assertEqual(1, anim.users)

        # Assign the animation to the camera as well.
        camera = bpy.data.objects['Camera']
        camera_adt = camera.animation_data_create()
        camera_adt.action = anim
        self.assertEqual(2, anim.users)

        # Unassigning should decrement the user count.
        cube_adt.action = None
        self.assertEqual(1, anim.users)

        # Deleting the camera should also decrement the user count.
        bpy.data.objects.remove(camera)
        self.assertEqual(0, anim.users)

    def test_binding_assignment(self):
        # Create new Action.
        anim = bpy.data.actions.new('TestAction')
        self.assertEqual(0, anim.users)

        # Assign the animation to the cube,
        cube = bpy.data.objects['Cube']
        cube_adt = cube.animation_data_create()
        cube_adt.action = anim
        bind_cube = anim.bindings.new(for_id=cube)
        cube_adt.action_binding_handle = bind_cube.handle
        self.assertEqual(cube_adt.action_binding_handle, bind_cube.handle)

        # Assign the animation to the camera as well.
        camera = bpy.data.objects['Camera']
        bind_camera = anim.bindings.new(for_id=camera)
        camera_adt = camera.animation_data_create()
        camera_adt.action = anim
        self.assertEqual(camera_adt.action_binding_handle, bind_camera.handle)

        # Unassigning should keep the binding name.
        cube_adt.action = None
        self.assertEqual(cube_adt.action_binding_name, bind_cube.name)

        # It should not be possible to set the binding handle while the animation is unassigned.
        bind_extra = anim.bindings.new()
        cube_adt.action_binding_handle = bind_extra.handle
        self.assertNotEqual(cube_adt.action_binding_handle, bind_extra.handle)


class LimitationsTest(unittest.TestCase):
    """Test artificial limitations for the Animation data-block.

    Certain limitations are in place to keep development & testing focused.
    """

    def setUp(self):
        anims = bpy.data.actions
        while anims:
            anims.remove(anims[0])

    def test_initial_layers(self):
        """Test that upon creation an Animation has no layers/strips."""
        anim = bpy.data.actions.new('TestAction')
        self.assertEqual([], anim.layers[:])

    def test_limited_layers_strips(self):
        """Test that there can only be one layer with one strip."""

        anim = bpy.data.actions.new('TestAction')
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

        anim = bpy.data.actions.new('TestAction')
        layer = anim.layers.new(name="Layer")
        strip = layer.strips.new(type='KEYFRAME')

        self.assertFalse(hasattr(strip, 'frame_start'))
        self.assertFalse(hasattr(strip, 'frame_end'))
        self.assertFalse(hasattr(strip, 'frame_offset'))


class DataPathTest(unittest.TestCase):
    def setUp(self):
        anims = bpy.data.actions
        while anims:
            anims.remove(anims[0])

    def test_repr(self):
        anim = bpy.data.actions.new('TestAction')

        layer = anim.layers.new(name="Layer")
        self.assertEqual("bpy.data.actions['TestAction'].layers[\"Layer\"]", repr(layer))

        strip = layer.strips.new(type='KEYFRAME')
        self.assertEqual("bpy.data.actions['TestAction'].layers[\"Layer\"].strips[0]", repr(strip))


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
