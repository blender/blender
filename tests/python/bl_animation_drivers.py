# SPDX-FileCopyrightText: 2020-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
blender -b --factory-startup --python tests/python/bl_animation_drivers.py -- --testdir /path/to/tests/data/animation
"""
__all__ = (
    "main",
)

import unittest
import bpy
import pathlib
import sys
from rna_prop_ui import rna_idprop_quote_path


class AbstractEmptyDriverTest:
    def setUp(self):
        super().setUp()
        bpy.ops.wm.read_homefile(use_factory_startup=True)
        self.obj = bpy.data.objects['Cube']

    def assertPropValue(self, prop_name, value):
        self.assertEqual(self.obj[prop_name], value)


def _make_context_driver(obj, prop_name, ctx_type, ctx_path, index=None, fallback=None, force_python=False):
    obj[prop_name] = 0
    fcu = obj.driver_add(rna_idprop_quote_path(prop_name), -1)
    drv = fcu.driver

    if force_python:
        # Expression that requires full python interpreter
        drv.type = 'SCRIPTED'
        drv.expression = '[var][0]'
    else:
        drv.type = 'SUM'

    var = drv.variables.new()
    var.name = "var"
    var.type = 'CONTEXT_PROP'
    tgt = var.targets[0]
    tgt.context_property = ctx_type
    tgt.data_path = rna_idprop_quote_path(ctx_path) + (f"[{index}]" if index is not None else "")

    if fallback is not None:
        tgt.use_fallback_value = True
        tgt.fallback_value = fallback

    return fcu


def _is_fallback_used(fcu):
    return fcu.driver.variables[0].targets[0].is_fallback_used


class ContextSceneDriverTest(AbstractEmptyDriverTest, unittest.TestCase):
    """ Ensure keying things by name or with a keying set adds the right keys. """

    def setUp(self):
        super().setUp()
        bpy.context.scene["test_property"] = 123

    def test_context_valid(self):
        fcu = _make_context_driver(
            self.obj, 'test_valid', 'ACTIVE_SCENE', 'test_property')
        bpy.context.view_layer.update()
        self.assertTrue(fcu.driver.is_valid)
        self.assertPropValue('test_valid', 123)

    def test_context_invalid(self):
        fcu = _make_context_driver(
            self.obj, 'test_invalid', 'ACTIVE_SCENE', 'test_property_bad')
        bpy.context.view_layer.update()
        self.assertFalse(fcu.driver.is_valid)

    def test_context_fallback(self):
        fcu = _make_context_driver(
            self.obj, 'test_fallback', 'ACTIVE_SCENE', 'test_property_bad', fallback=321)
        bpy.context.view_layer.update()
        self.assertTrue(fcu.driver.is_valid)
        self.assertTrue(_is_fallback_used(fcu))
        self.assertPropValue('test_fallback', 321)

    def test_context_fallback_valid(self):
        fcu = _make_context_driver(
            self.obj, 'test_fallback_valid', 'ACTIVE_SCENE', 'test_property', fallback=321)
        bpy.context.view_layer.update()
        self.assertTrue(fcu.driver.is_valid)
        self.assertFalse(_is_fallback_used(fcu))
        self.assertPropValue('test_fallback_valid', 123)

    def test_context_fallback_python(self):
        fcu = _make_context_driver(
            self.obj, 'test_fallback_py', 'ACTIVE_SCENE', 'test_property_bad', fallback=321, force_python=True)
        bpy.context.view_layer.update()
        self.assertTrue(fcu.driver.is_valid)
        self.assertTrue(_is_fallback_used(fcu))
        self.assertPropValue('test_fallback_py', 321)


class ContextSceneArrayDriverTest(AbstractEmptyDriverTest, unittest.TestCase):
    """ Ensure keying things by name or with a keying set adds the right keys. """

    def setUp(self):
        super().setUp()
        bpy.context.scene["test_property"] = [123, 456]

    def test_context_valid(self):
        fcu = _make_context_driver(
            self.obj, 'test_valid', 'ACTIVE_SCENE', 'test_property', index=0)
        bpy.context.view_layer.update()
        self.assertTrue(fcu.driver.is_valid)
        self.assertPropValue('test_valid', 123)

    def test_context_invalid(self):
        fcu = _make_context_driver(
            self.obj, 'test_invalid', 'ACTIVE_SCENE', 'test_property', index=2)
        bpy.context.view_layer.update()
        self.assertFalse(fcu.driver.is_valid)

    def test_context_fallback(self):
        fcu = _make_context_driver(
            self.obj, 'test_fallback', 'ACTIVE_SCENE', 'test_property', index=2, fallback=321)
        bpy.context.view_layer.update()
        self.assertTrue(fcu.driver.is_valid)
        self.assertTrue(_is_fallback_used(fcu))
        self.assertPropValue('test_fallback', 321)

    def test_context_fallback_valid(self):
        fcu = _make_context_driver(
            self.obj, 'test_fallback_valid', 'ACTIVE_SCENE', 'test_property', index=0, fallback=321)
        bpy.context.view_layer.update()
        self.assertTrue(fcu.driver.is_valid)
        self.assertFalse(_is_fallback_used(fcu))
        self.assertPropValue('test_fallback_valid', 123)

    def test_context_fallback_python(self):
        fcu = _make_context_driver(
            self.obj, 'test_fallback_py', 'ACTIVE_SCENE', 'test_property', index=2, fallback=321, force_python=True)
        bpy.context.view_layer.update()
        self.assertTrue(fcu.driver.is_valid)
        self.assertTrue(_is_fallback_used(fcu))
        self.assertPropValue('test_fallback_py', 321)


def _select_view_layer(index):
    bpy.context.window.view_layer = bpy.context.scene.view_layers[index]


class ContextViewLayerDriverTest(AbstractEmptyDriverTest, unittest.TestCase):
    """ Ensure keying things by name or with a keying set adds the right keys. """

    def setUp(self):
        super().setUp()
        bpy.ops.scene.view_layer_add(type='COPY')
        scene = bpy.context.scene
        scene.view_layers[0]['test_property'] = 123
        scene.view_layers[1]['test_property'] = 456
        _select_view_layer(0)

    def test_context_valid(self):
        fcu = _make_context_driver(
            self.obj, 'test_valid', 'ACTIVE_VIEW_LAYER', 'test_property')

        _select_view_layer(0)
        bpy.context.view_layer.update()
        self.assertTrue(fcu.driver.is_valid)
        self.assertPropValue('test_valid', 123)

        _select_view_layer(1)
        bpy.context.view_layer.update()
        self.assertTrue(fcu.driver.is_valid)
        self.assertPropValue('test_valid', 456)

    def test_context_fallback(self):
        del bpy.context.scene.view_layers[1]['test_property']

        fcu = _make_context_driver(
            self.obj, 'test_fallback', 'ACTIVE_VIEW_LAYER', 'test_property', fallback=321)

        _select_view_layer(0)
        bpy.context.view_layer.update()
        self.assertTrue(fcu.driver.is_valid)
        self.assertFalse(_is_fallback_used(fcu))
        self.assertPropValue('test_fallback', 123)

        _select_view_layer(1)
        bpy.context.view_layer.update()
        self.assertTrue(fcu.driver.is_valid)
        self.assertTrue(_is_fallback_used(fcu))
        self.assertPropValue('test_fallback', 321)


class SubDataDriverRemovalTest(AbstractEmptyDriverTest, unittest.TestCase):

    def test_remove_object_modifier(self):
        # Removing a modifier with a driver should also delete the driver
        modifier = self.obj.modifiers.new("test", 'ARRAY')
        # No animation data means no drivers.
        self.assertEqual(self.obj.animation_data, None)
        self.obj.driver_add('modifiers["test"].count')
        self.assertEqual(len(self.obj.animation_data.drivers), 1)
        self.obj.modifiers.remove(modifier)
        self.assertEqual(len(self.obj.modifiers), 0)
        self.assertEqual(len(self.obj.animation_data.drivers), 0,
                         "Removing the modifier should remove the driver on it")

    def test_remove_object_constraint(self):
        # Using limit distance constraint because that has a property that can have a driver.
        constraint = self.obj.constraints.new('LIMIT_DISTANCE')
        constraint.name = "test"
        # No animation data means no drivers.
        self.assertEqual(self.obj.animation_data, None)
        self.obj.driver_add('constraints["test"].distance')
        self.assertEqual(len(self.obj.animation_data.drivers), 1)
        self.obj.constraints.remove(constraint)
        self.assertEqual(len(self.obj.constraints), 0)
        self.assertEqual(len(self.obj.animation_data.drivers), 0,
                         "Removing the constraint should remove the driver on it")

    def test_remove_bone_constraint(self):
        arm = bpy.data.armatures.new('Armature')
        arm_ob = bpy.data.objects.new('ArmObject', arm)
        bpy.context.scene.collection.objects.link(arm_ob)
        bpy.context.view_layer.objects.active = arm_ob
        bpy.ops.object.mode_set(mode='EDIT')
        ebone = arm.edit_bones.new(name="test")
        ebone.tail = (1, 0, 0)
        bpy.ops.object.mode_set(mode='POSE')
        pose_bone = arm_ob.pose.bones["test"]
        constraint = pose_bone.constraints.new('LIMIT_DISTANCE')
        constraint.name = "test"
        self.assertEqual(len(pose_bone.constraints), 1)
        arm_ob.driver_add('pose.bones["test"].constraints["test"].distance')
        self.assertEqual(len(arm_ob.animation_data.drivers), 1)
        pose_bone.constraints.remove(constraint)
        self.assertEqual(len(pose_bone.constraints), 0)
        self.assertEqual(len(arm_ob.animation_data.drivers), 0,
                         "Removing the constraint should remove the driver on it")

    def test_remove_shapekey(self):
        self.obj.shape_key_add(name="base")
        test_key = self.obj.shape_key_add(name="test")
        # Due to the weirdness of shapekeys, this is an ID.
        shape_key_id = self.obj.data.shape_keys
        self.assertEqual(len(shape_key_id.key_blocks), 2)
        shape_key_id.driver_add('key_blocks["test"].value')
        self.assertEqual(len(shape_key_id.animation_data.drivers), 1)

        self.obj.shape_key_remove(test_key)
        self.assertEqual(len(shape_key_id.key_blocks), 1)
        self.assertEqual(len(shape_key_id.animation_data.drivers), 0,
                         "Removing the shape key should remove any driver on it")


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
