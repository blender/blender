# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# Tests for `source/blender/python/intern/bpy_operator_function.cc`
# Python calling into the operator system (using a callable function like type).
#
# ./blender.bin --background --python tests/python/bl_operator_function_py_api.py -- --verbose

__all__ = (
    "main",
)

import unittest

import bpy


class TestOperatorFunctionAccess(unittest.TestCase):

    def test_access_operator(self):
        op = bpy.ops.object.select_all
        self.assertTrue(callable(op))


class TestOperatorCall(unittest.TestCase):

    def test_call_clear_recent_files(self):
        self.assertEqual(bpy.ops.wm.clear_recent_files(), {'FINISHED'})


class TestOperatorCallPositionalArgs(unittest.TestCase):

    def test_exec_default(self):
        self.assertEqual(bpy.ops.wm.clear_recent_files('EXEC_DEFAULT'), {'FINISHED'})

    def test_invoke_default(self):
        self.assertEqual(bpy.ops.wm.clear_recent_files('INVOKE_DEFAULT'), {'FINISHED'})

    def test_undo_flag(self):
        self.assertEqual(bpy.ops.wm.clear_recent_files(True), {'FINISHED'})

    def test_undo_flag_false(self):
        self.assertEqual(bpy.ops.wm.clear_recent_files(False), {'FINISHED'})

    def test_undo_flag_int(self):
        self.assertEqual(bpy.ops.wm.clear_recent_files(1), {'FINISHED'})

    def test_undo_flag_int_zero(self):
        self.assertEqual(bpy.ops.wm.clear_recent_files(0), {'FINISHED'})

    def test_context_and_undo(self):
        self.assertEqual(bpy.ops.wm.clear_recent_files('EXEC_DEFAULT', True), {'FINISHED'})

    def test_context_and_undo_int(self):
        self.assertEqual(bpy.ops.wm.clear_recent_files('EXEC_DEFAULT', 1), {'FINISHED'})

    def test_negative_int(self):
        self.assertEqual(bpy.ops.wm.clear_recent_files(-1), {'FINISHED'})


class TestOperatorCallPositionalArgsInvalid(unittest.TestCase):

    def test_args_invalid_type(self):
        for arg in (1.0, None, [], {}, (), set(), b'EXEC_DEFAULT'):
            with self.assertRaises(ValueError):
                bpy.ops.wm.clear_recent_files(arg)

    def test_args_invalid_order(self):
        for args in (
                (True, 'EXEC_DEFAULT'),
                (1, 'EXEC_DEFAULT'),
        ):
            with self.assertRaises(ValueError):
                bpy.ops.wm.clear_recent_files(*args)

    def test_args_invalid_duplicates(self):
        for args in (
                (True, True),
                (False, False),
                (1, 1),
                (1, True),
                ('EXEC_DEFAULT', 'EXEC_DEFAULT'),
                ('EXEC_DEFAULT', True, 'extra'),
                (True, True, 'EXEC_DEFAULT', 'EXEC_DEFAULT'),
        ):
            with self.assertRaises(ValueError):
                bpy.ops.wm.clear_recent_files(*args)


class TestOperatorCallKeywordArgs(unittest.TestCase):

    def test_enum_keyword(self):
        self.assertEqual(bpy.ops.object.select_all(action='TOGGLE'), {'FINISHED'})

    def test_keyword_with_context(self):
        self.assertEqual(
            bpy.ops.object.select_all('EXEC_DEFAULT', action='TOGGLE'), {'FINISHED'})

    def test_keyword_with_context_and_undo(self):
        self.assertEqual(
            bpy.ops.object.select_all('EXEC_DEFAULT', True, action='TOGGLE'), {'FINISHED'})

    def test_multiple_keywords(self):
        self.assertEqual(
            bpy.ops.mesh.primitive_cube_add(size=1.0, location=(0, 0, 0)), {'FINISHED'})


class TestOperatorCallKeywordArgsInvalid(unittest.TestCase):

    def test_invalid_enum_value(self):
        with self.assertRaises(TypeError):
            bpy.ops.object.select_all(action='INVALID')

    def test_wrong_type_for_enum(self):
        with self.assertRaises(TypeError):
            bpy.ops.object.select_all(action=123)

    def test_wrong_type_for_float(self):
        with self.assertRaises(TypeError):
            bpy.ops.mesh.primitive_cube_add(size='big')

    def test_wrong_type_for_bool(self):
        with self.assertRaises(TypeError):
            bpy.ops.mesh.primitive_cube_add(align='WORLD', calc_uvs='yes')

    def test_none_for_enum(self):
        with self.assertRaises(TypeError):
            bpy.ops.object.select_all(action=None)

    def test_none_for_float(self):
        with self.assertRaises(TypeError):
            bpy.ops.mesh.primitive_cube_add(size=None)

    def test_list_for_float(self):
        with self.assertRaises(TypeError):
            bpy.ops.mesh.primitive_cube_add(size=[1.0])

    def test_unknown_keyword(self):
        with self.assertRaises(TypeError):
            bpy.ops.object.select_all(nonexistent=True)

    def test_multiple_unknown_keywords(self):
        with self.assertRaises(TypeError):
            bpy.ops.object.select_all(foo=1, bar=2)

    def test_valid_and_unknown_keyword(self):
        with self.assertRaises(TypeError):
            bpy.ops.object.select_all(action='TOGGLE', nonexistent=True)

    def test_empty_string_for_enum(self):
        with self.assertRaises(TypeError):
            bpy.ops.object.select_all(action='')

    def test_non_string_keywords(self):
        for key in (1, None, (1, 2)):
            with self.assertRaises(TypeError):
                bpy.ops.object.select_all(**{key: 'TOGGLE'})


class TestOperatorCallInvalid(unittest.TestCase):

    def test_invalid_context_string(self):
        with self.assertRaises(TypeError):
            bpy.ops.wm.clear_recent_files('INVALID_CONTEXT')

    def test_invalid_keyword_argument(self):
        with self.assertRaises(TypeError):
            bpy.ops.wm.clear_recent_files(nonexistent_prop=True)

    def test_poll_failure(self):
        # Edit-mode operator cannot run without an active mesh in edit-mode.
        with self.assertRaises(RuntimeError):
            bpy.ops.mesh.subdivide()


def main():
    import sys
    sys.argv = [__file__] + (sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else [])
    unittest.main()


if __name__ == "__main__":
    main()
