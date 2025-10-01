# SPDX-FileCopyrightText: 2020-2023 Blender Authors
#
# SPDX-License-Identifier: Apache-2.0

# NOTE: See also `bl_pyapi_prop_array.py` for the `Vector` bpy.props similar tests,
# and `bl_pyapi_idprop.py` for some deeper testing of the consistency between
# the underlying IDProperty storage, and the property data exposed in Python.

# ./blender.bin --background --python tests/python/bl_pyapi_prop.py -- --verbose
import bpy
from bpy.props import (
    BoolProperty,
    IntProperty,
    FloatProperty,
    EnumProperty,
    StringProperty,
    PointerProperty,
    CollectionProperty,
)

import unittest
import functools

id_inst = bpy.context.scene
id_type = bpy.types.Scene


# -----------------------------------------------------------------------------
# Utility Types

class TestPropertyGroup(bpy.types.PropertyGroup):
    test_prop: IntProperty()


# -----------------------------------------------------------------------------
# Tests

class TestPropNumerical(unittest.TestCase):
    default_value = 0
    custom_value = 1
    min_value = -1
    max_value = 5

    def setUp(self):
        id_type.test_bool = BoolProperty(default=bool(self.default_value))
        id_type.test_int = IntProperty(
            default=int(self.default_value),
            min=int(self.min_value),
            max=int(self.max_value),
        )
        id_type.test_float = FloatProperty(
            default=float(self.default_value),
            min=float(self.min_value),
            max=float(self.max_value),
        )

        self.test_bool_storage = bool(self.custom_value)
        self.test_int_storage = int(self.custom_value)
        self.test_float_storage = float(self.custom_value)

        def bool_set_(s, v):
            self.test_bool_storage = v

        def int_set_(s, v):
            self.test_int_storage = v

        def float_set_(s, v):
            self.test_float_storage = v

        id_type.test_bool_getset = BoolProperty(
            default=bool(self.default_value),
            get=lambda s: self.test_bool_storage,
            set=bool_set_,
        )
        id_type.test_int_getset = IntProperty(
            default=int(self.default_value),
            min=int(self.min_value),
            max=int(self.max_value),
            get=lambda s: self.test_int_storage,
            set=int_set_,
        )
        id_type.test_float_getset = FloatProperty(
            default=float(self.default_value),
            min=float(self.min_value),
            max=float(self.max_value),
            get=lambda s: self.test_float_storage,
            set=float_set_,
        )

        id_type.test_bool_transform = BoolProperty(
            default=bool(self.default_value),
            get_transform=lambda s, c_v, isset: c_v,
            set_transform=lambda s, n_v, c_v, isset: n_v,
        )
        id_type.test_int_transform = IntProperty(
            default=int(self.default_value),
            min=int(self.min_value),
            max=int(self.max_value),
            get_transform=lambda s, c_v, isset: c_v,
            set_transform=lambda s, n_v, c_v, isset: n_v,
        )
        id_type.test_float_transform = FloatProperty(
            default=float(self.default_value),
            min=float(self.min_value),
            max=float(self.max_value),
            get_transform=lambda s, c_v, isset: c_v,
            set_transform=lambda s, n_v, c_v, isset: n_v,
        )

        id_type.test_bool_getset_transform = BoolProperty(
            default=bool(self.default_value),
            get=lambda s: self.test_bool_storage,
            set=bool_set_,
            get_transform=lambda s, c_v, isset: c_v,
            set_transform=lambda s, n_v, c_v, isset: n_v,
        )
        id_type.test_int_getset_transform = IntProperty(
            default=int(self.default_value),
            min=int(self.min_value),
            max=int(self.max_value),
            get=lambda s: self.test_int_storage,
            set=int_set_,
            get_transform=lambda s, c_v, isset: c_v,
            set_transform=lambda s, n_v, c_v, isset: n_v,
        )
        id_type.test_float_getset_transform = FloatProperty(
            default=float(self.default_value),
            min=float(self.min_value),
            max=float(self.max_value),
            get=lambda s: self.test_float_storage,
            set=float_set_,
            get_transform=lambda s, c_v, isset: c_v,
            set_transform=lambda s, n_v, c_v, isset: n_v,
        )

    def tearDown(self):
        del id_type.test_float
        del id_type.test_int
        del id_type.test_bool

        del id_type.test_float_getset
        del id_type.test_int_getset
        del id_type.test_bool_getset

        del id_type.test_float_transform
        del id_type.test_int_transform
        del id_type.test_bool_transform

        del id_type.test_float_getset_transform
        del id_type.test_int_getset_transform
        del id_type.test_bool_getset_transform

    def do_min_max_expect_success(self, prop_name, py_type):
        # This property is expected to properly clamp set values within required range.
        setattr(id_inst, prop_name, py_type(self.min_value - 1))
        self.assertEqual(getattr(id_inst, prop_name), py_type(self.min_value))
        setattr(id_inst, prop_name, py_type(self.max_value + 1))
        self.assertEqual(getattr(id_inst, prop_name), py_type(self.max_value))

    def do_min_max_expect_failure(self, prop_name, py_type):
        # This property is not expected to properly clamp set values within required range.
        # This happens when using custom setters.
        setattr(id_inst, prop_name, py_type(self.min_value - 1))
        self.assertNotEqual(getattr(id_inst, prop_name), py_type(self.min_value))
        setattr(id_inst, prop_name, py_type(self.max_value + 1))
        self.assertNotEqual(getattr(id_inst, prop_name), py_type(self.max_value))

    def do_test_access(self, prop_name, py_type, expected_value, do_min_max=None):
        v = getattr(id_inst, prop_name)
        self.assertIsInstance(v, py_type)
        self.assertEqual(v, expected_value)
        setattr(id_inst, prop_name, v)
        self.assertEqual(getattr(id_inst, prop_name), expected_value)
        setattr(id_inst, prop_name, py_type(self.custom_value))
        self.assertEqual(getattr(id_inst, prop_name), py_type(self.custom_value))
        if do_min_max:
            do_min_max(prop_name, py_type)

    def test_access_bool(self):
        self.do_test_access("test_bool", bool, bool(self.default_value))

    def test_access_int(self):
        self.do_test_access(
            "test_int",
            int,
            int(self.default_value),
            do_min_max=self.do_min_max_expect_success,
        )

    def test_access_float(self):
        self.do_test_access(
            "test_float",
            float,
            float(self.default_value),
            do_min_max=self.do_min_max_expect_success,
        )

    def test_access_bool_getset(self):
        self.do_test_access("test_bool_getset", bool, bool(self.custom_value))

    def test_access_int_getset(self):
        self.do_test_access(
            "test_int_getset",
            int,
            int(self.custom_value),
            do_min_max=self.do_min_max_expect_failure,
        )

    def test_access_float_getset(self):
        self.do_test_access(
            "test_float_getset",
            float,
            float(self.custom_value),
            do_min_max=self.do_min_max_expect_failure,
        )

    def test_access_bool_transform(self):
        self.do_test_access("test_bool_transform", bool, bool(self.default_value))

    def test_access_int_transform(self):
        self.do_test_access(
            "test_int_transform",
            int,
            int(self.default_value),
            do_min_max=self.do_min_max_expect_success,
        )

    def test_access_float_transform(self):
        self.do_test_access(
            "test_float_transform",
            float,
            float(self.default_value),
            do_min_max=self.do_min_max_expect_success,
        )

    def test_access_bool_getset_transform(self):
        self.do_test_access("test_bool_getset_transform", bool, bool(self.custom_value))

    def test_access_int_getset_transform(self):
        self.do_test_access(
            "test_int_getset_transform",
            int,
            int(self.custom_value),
            do_min_max=self.do_min_max_expect_failure,
        )

    def test_access_float_getset_transform(self):
        self.do_test_access(
            "test_float_getset_transform",
            float,
            float(self.custom_value),
            do_min_max=self.do_min_max_expect_failure,
        )

    # TODO: Add expected failure cases (e.g. handling of out-of range values).


class TestPropString(unittest.TestCase):
    default_value = ""
    custom_value = "Blender"

    def setUp(self):
        id_type.test_string = StringProperty(default=self.default_value)

        self.test_string_storage = self.custom_value

        def set_(s, v):
            self.test_string_storage = v
        id_type.test_string_getset = StringProperty(
            default=self.default_value,
            get=lambda s: self.test_string_storage,
            set=set_,
        )

    def tearDown(self):
        del id_type.test_string
        del id_type.test_string_getset

    def do_test_access(self, prop_name, py_type, expected_value):
        v = getattr(id_inst, prop_name)
        self.assertIsInstance(v, py_type)
        self.assertEqual(v, expected_value)
        setattr(id_inst, prop_name, v)

    def test_access_string(self):
        self.do_test_access("test_string", str, self.default_value)

    def test_access_string_getset(self):
        self.do_test_access("test_string_getset", str, self.custom_value)

    # TODO: Add expected failure cases (e.g. handling of too long values, invalid utf8 sequences, etc.).


class TestPropEnum(unittest.TestCase):
    # FIXME: Auto-generated enum values do not play well with partially specifying some values.
    # This won't work, generating (1, 1, 2, 16):
    #   enum_items = (("1", "1", "", 1), ("2", "2", ""), ("3", "3", ""), ("16", "16", "", 16),)
    enum_items = (("1", "1", ""), ("2", "2", ""), ("3", "3", ""), ("16", "16", "", 16),)
    enum_expected_values = {"1": 0, "2": 1, "3": 2, "16": 16}
    enum_expected_bitflag_values = {"1": 2**0, "2": 2**1, "3": 2**2, "16": 2**4}

    default_value = "1"
    custom_value = "2"
    default_bitflag_value = {"1", "3"}
    custom_bitflag_value = {"16", "3"}

    def setUp(self):
        id_type.test_enum = EnumProperty(items=self.enum_items, default=self.default_value)
        id_type.test_enum_bitflag = EnumProperty(
            items=self.enum_items,
            default=self.default_bitflag_value,
            options={"ENUM_FLAG"},
        )

        self.test_enum_storage = self.enum_expected_values[self.custom_value]
        self.test_enum_bitflag_storage = functools.reduce(
            lambda a, b: a | b,
            (self.enum_expected_bitflag_values[bf] for bf in self.custom_bitflag_value))

        def enum_set_(s, v):
            self.test_enum_storage = v

        def bitflag_set_(s, v):
            self.test_enum_bitflag_storage = v

        id_type.test_enum_getset = EnumProperty(
            items=self.enum_items,
            default=self.default_value,
            get=lambda s: self.test_enum_storage,
            set=enum_set_,
        )

        id_type.test_enum_bitflag_getset = EnumProperty(
            items=self.enum_items,
            default=self.default_bitflag_value,
            options={"ENUM_FLAG"},
            get=lambda s: self.test_enum_bitflag_storage,
            set=bitflag_set_,
        )

        id_type.test_enum_transform = EnumProperty(
            items=self.enum_items,
            default=self.default_value,
            get_transform=lambda s, c_v, isset: c_v,
            set_transform=lambda s, n_v, c_v, isset: n_v,
        )

        id_type.test_enum_bitflag_transform = EnumProperty(
            items=self.enum_items,
            default=self.default_bitflag_value,
            options={"ENUM_FLAG"},
            get_transform=lambda s, c_v, isset: c_v,
            set_transform=lambda s, n_v, c_v, isset: n_v,
        )

        id_type.test_enum_getset_transform = EnumProperty(
            items=self.enum_items,
            default=self.default_value,
            get=lambda s: self.test_enum_storage,
            set=enum_set_,
            get_transform=lambda s, c_v, isset: c_v,
            set_transform=lambda s, n_v, c_v, isset: n_v,
        )

        id_type.test_enum_bitflag_getset_transform = EnumProperty(
            items=self.enum_items,
            default=self.default_bitflag_value,
            options={"ENUM_FLAG"},
            get_transform=lambda s, c_v, isset: c_v,
            set_transform=lambda s, n_v, c_v, isset: n_v,
            get=lambda s: self.test_enum_bitflag_storage,
            set=bitflag_set_,
        )

    def tearDown(self):
        del id_type.test_enum
        del id_type.test_enum_bitflag
        del id_type.test_enum_getset
        del id_type.test_enum_bitflag_getset
        del id_type.test_enum_transform
        del id_type.test_enum_bitflag_transform
        del id_type.test_enum_getset_transform
        del id_type.test_enum_bitflag_getset_transform

    # Test expected generated values for enum items.
    def do_test_enum_values(self, prop_name, expected_item_values):
        enum_items = id_inst.bl_rna.properties[prop_name].enum_items
        self.assertEqual(len(expected_item_values), len(enum_items))
        for (expected_identifier, expected_value), item in zip(expected_item_values.items(), enum_items):
            self.assertEqual(expected_identifier, item.identifier)
            self.assertEqual(expected_value, item.value)

    def test_enum_item_values(self):
        self.do_test_enum_values("test_enum", self.enum_expected_values)

    def test_enum_bitflag_item_values(self):
        self.do_test_enum_values("test_enum_bitflag", self.enum_expected_bitflag_values)

    def test_enum_getset_item_values(self):
        self.do_test_enum_values("test_enum_getset", self.enum_expected_values)

    def test_enum_bitflag_getset_item_values(self):
        self.do_test_enum_values("test_enum_bitflag_getset", self.enum_expected_bitflag_values)

    def test_enum_transform_item_values(self):
        self.do_test_enum_values("test_enum_transform", self.enum_expected_values)

    def test_enum_bitflag_transform_item_values(self):
        self.do_test_enum_values("test_enum_bitflag_transform", self.enum_expected_bitflag_values)

    def test_enum_getset_transform_item_values(self):
        self.do_test_enum_values("test_enum_getset_transform", self.enum_expected_values)

    def test_enum_bitflag_getset_transform_item_values(self):
        self.do_test_enum_values("test_enum_bitflag_getset_transform", self.enum_expected_bitflag_values)

    # Test basic access to enum values.
    def do_test_access(self, prop_name, py_type, expected_value):
        v = getattr(id_inst, prop_name)
        self.assertIsInstance(v, py_type)
        self.assertEqual(v, expected_value)
        setattr(id_inst, prop_name, v)

    def test_access_enum(self):
        self.do_test_access("test_enum", str, self.default_value)

    def test_access_enum_bitflag(self):
        self.do_test_access("test_enum_bitflag", set, self.default_bitflag_value)

    def test_access_enum_getset(self):
        self.do_test_access("test_enum_getset", str, self.custom_value)

    def test_access_enum_bitflag_getset(self):
        self.do_test_access("test_enum_bitflag_getset", set, self.custom_bitflag_value)

    def test_access_enum_transform(self):
        self.do_test_access("test_enum_transform", str, self.default_value)

    def test_access_enum_bitflag_transform(self):
        self.do_test_access("test_enum_bitflag_transform", set, self.default_bitflag_value)

    def test_access_enum_getset_transform(self):
        self.do_test_access("test_enum_getset_transform", str, self.custom_value)

    def test_access_enum_bitflag_getset_transform(self):
        self.do_test_access("test_enum_bitflag_getset_transform", set, self.custom_bitflag_value)

    # TODO: Add expected failure cases (e.g. handling of invalid items identifiers).


class TestPropCollectionAndPointer(unittest.TestCase):

    def setUp(self):
        bpy.utils.register_class(TestPropertyGroup)

        id_type.test_pointer = PointerProperty(type=TestPropertyGroup)
        id_type.test_pointer_ID = PointerProperty(type=bpy.types.ID)
        id_type.test_pointer_ID_poll = PointerProperty(type=bpy.types.ID, poll=lambda s, v: v.id_type == 'OBJECT')
        id_type.test_collection = CollectionProperty(type=TestPropertyGroup)

    def tearDown(self):
        del id_type.test_pointer
        del id_type.test_pointer_ID
        del id_type.test_pointer_ID_poll
        del id_type.test_collection

        bpy.utils.unregister_class(TestPropertyGroup)

    def test_access_pointer(self):
        v = id_inst.test_pointer
        self.assertIsInstance(v, TestPropertyGroup)
        self.assertTrue(hasattr(v, "test_prop"))
        self.assertEqual(v.test_prop, 0)
        v.test_prop = 42
        self.assertEqual(id_inst.test_pointer.test_prop, 42)

    def test_access_pointer_ID(self):
        self.assertEqual(id_inst.test_pointer_ID, None)

        # Non-refcounting ID type
        win_man = bpy.data.window_managers[0]
        win_man_users = win_man.users
        id_inst.test_pointer_ID = win_man
        self.assertEqual(id_inst.test_pointer_ID, win_man)
        self.assertEqual(win_man.users, win_man_users)
        id_inst.test_pointer_ID = None
        self.assertEqual(id_inst.test_pointer_ID, None)
        self.assertEqual(win_man.users, win_man_users)

        # Refcounting ID type
        ma = bpy.data.materials[0]
        ma_users = ma.users
        id_inst.test_pointer_ID = ma
        self.assertEqual(id_inst.test_pointer_ID, ma)
        self.assertEqual(win_man.users, ma_users + 1)
        id_inst.test_pointer_ID = None
        self.assertEqual(id_inst.test_pointer_ID, None)
        self.assertEqual(ma.users, ma_users)

    def test_access_pointer_ID_poll(self):
        # Poll callback is only used for UI, in scripts it's still possible to assign an 'invalid' ID.
        self.assertEqual(id_inst.test_pointer_ID_poll, None)
        win_man = bpy.data.window_managers[0]
        id_inst.test_pointer_ID_poll = win_man
        self.assertEqual(id_inst.test_pointer_ID_poll, win_man)
        id_inst.test_pointer_ID_poll = None
        self.assertEqual(id_inst.test_pointer_ID_poll, None)

    def test_access_collection(self):
        self.assertEqual(len(id_inst.test_collection), 0)

        test_item = id_inst.test_collection.add()
        self.assertEqual(len(id_inst.test_collection), 1)
        self.assertIsInstance(test_item, TestPropertyGroup)
        self.assertTrue(hasattr(test_item, "test_prop"))
        self.assertEqual(test_item.test_prop, 0)
        self.assertEqual(id_inst.test_collection[0], test_item)
        test_item.test_prop = 42
        self.assertEqual(test_item.test_prop, 42)
        self.assertEqual(id_inst.test_collection[0].test_prop, 42)

        test_item_2 = id_inst.test_collection.add()
        test_item_3 = id_inst.test_collection.add()
        test_item_3.test_prop = 24
        self.assertEqual(len(id_inst.test_collection), 3)
        self.assertEqual(id_inst.test_collection[0], test_item)
        self.assertEqual(id_inst.test_collection[1], test_item_2)
        self.assertEqual(id_inst.test_collection[2], test_item_3)

        id_inst.test_collection.remove(1)
        self.assertEqual(len(id_inst.test_collection), 2)
        self.assertEqual(id_inst.test_collection[0], test_item)
        # Removing the second item re-allocates the third one, so no equality anymore.
        self.assertNotEqual(id_inst.test_collection[1], test_item_3)
        self.assertEqual(id_inst.test_collection[1].test_prop, 24)

    # TODO: Add expected failure cases (e.g. assigning propertygroup to a Pointer property, etc.).


if __name__ == '__main__':
    import sys
    sys.argv = [__file__] + (sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else [])
    unittest.main()
