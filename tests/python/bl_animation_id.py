# SPDX-FileCopyrightText: 2020-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import unittest
import sys

import bpy

"""
blender -b --factory-startup --python tests/python/bl_animation_id.py
"""


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
