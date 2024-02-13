# SPDX-FileCopyrightText: 2017-2022 Blender Authors
#
# SPDX-License-Identifier: Apache-2.0

# ./blender.bin --background --python tests/python/bl_pyapi_idprop.py -- --verbose
import bpy
import idprop
import unittest
from array import array

# Run if `numpy` is installed.
try:
    import numpy as np
except ImportError:
    np = None


class TestHelper:

    @property
    def id(self):
        return self._id

    def setUp(self):
        self._id = bpy.context.scene
        self._id.pop("cycles", None)
        assert len(self._id.keys()) == 0

    def tearDown(self):
        for key in list(self._id.keys()):
            del self._id[key]

    def assertAlmostEqualSeq(self, list1, list2):
        self.assertEqual(len(list1), len(list2))
        for v1, v2 in zip(list1, list2):
            self.assertAlmostEqual(v1, v2, places=5)


class TestIdPropertyCreation(TestHelper, unittest.TestCase):

    def test_name_empty(self):
        self.id[""] = 4
        self.assertEqual(self.id[""], 4)

    def test_name_too_long(self):
        with self.assertRaises(KeyError):
            self.id["name" * 30] = 4

    def test_int(self):
        self.id["a"] = 2
        self.assertEqual(self.id["a"], 2)
        self.assertTrue(isinstance(self.id["a"], int))

        with self.assertRaises(OverflowError):
            self.id["a"] = 2 ** 31  # integer <= 2 ** 31-1

    def test_double(self):
        self.id["a"] = 2.5
        self.assertEqual(self.id["a"], 2.5)
        self.assertTrue(isinstance(self.id["a"], float))

    def test_unicode(self):
        self.id["a"] = "Hello World"
        self.assertEqual(self.id["a"], "Hello World")
        self.assertTrue(isinstance(self.id["a"], str))

    def test_bytes(self):
        self.id["a"] = b"Hello World"
        self.assertEqual(self.id["a"], b"Hello World")
        self.assertTrue(isinstance(self.id["a"], bytes))

    def test_enum(self):
        self.id["a"] = 5
        self.assertEqual(self.id["a"], 5)
        self.assertTrue(isinstance(self.id["a"], int))

    def test_sequence_double_list(self):
        mylist = [1.2, 3.4, 5.6]
        self.id["a"] = mylist
        self.assertEqual(self.id["a"].to_list(), mylist)
        self.assertEqual(self.id["a"].typecode, "d")

    def test_sequence_int_list(self):
        mylist = [1, 2, 3]
        self.id["a"] = mylist
        self.assertEqual(self.id["a"].to_list(), mylist)
        self.assertEqual(self.id["a"].typecode, "i")

    def test_sequence_float_array(self):
        mylist = [1.2, 3.4, 5.6]
        self.id["a"] = array("f", mylist)
        self.assertAlmostEqualSeq(self.id["a"].to_list(), mylist)
        self.assertEqual(self.id["a"].typecode, "f")

    def test_sequence_double_array(self):
        mylist = [1.2, 3.4, 5.6]
        self.id["a"] = array("d", mylist)
        self.assertAlmostEqualSeq(self.id["a"].to_list(), mylist)
        self.assertEqual(self.id["a"].typecode, "d")

    def test_sequence_int_array(self):
        mylist = [1, 2, 3]
        self.id["a"] = array("i", mylist)
        self.assertAlmostEqualSeq(self.id["a"].to_list(), mylist)
        self.assertEqual(self.id["a"].typecode, "i")

    def test_sequence_other_array(self):
        mylist = [1, 2, 3]
        self.id["a"] = array("Q", mylist)
        self.assertEqual(self.id["a"].to_list(), mylist)

    def test_sequence_mixed_numerical_type(self):
        self.id["a"] = [1, 2, 3.4, 5]
        self.assertAlmostEqualSeq(self.id["a"].to_list(), [1.0, 2.0, 3.4, 5.0])
        self.assertEqual(self.id["a"].typecode, "d")

    def test_sequence_str_list(self):
        # I'm a bit surprised that this works
        mylist = ["abc", "qwe"]
        self.id["a"] = mylist
        self.assertEqual(self.id["a"], mylist)

    def test_sequence_mixed_type(self):
        with self.assertRaises(TypeError):
            mylist = ["abc", 3, "qwe", 3.4]
            self.id["a"] = mylist

    def test_mapping_simple(self):
        mydict = {"1": 10, "2": "20", "3": 30.5}
        self.id["a"] = mydict
        self.assertEqual(self.id["a"]["1"], mydict["1"])
        self.assertEqual(self.id["a"]["2"], mydict["2"])
        self.assertEqual(self.id["a"]["3"], mydict["3"])

    def test_mapping_complex(self):
        mydict = {
            "1": [1, 2, 3],
            "2": {"1": "abc", "2": array("i", [4, 5, 6])},
            "3": {"1": {"1": 10}, "2": b"qwe"},
        }
        self.id["a"] = mydict
        self.assertEqual(self.id["a"]["1"].to_list(), [1, 2, 3])
        self.assertEqual(self.id["a"]["2"]["1"], "abc")
        self.assertEqual(self.id["a"]["2"]["2"].to_list(), [4, 5, 6])
        self.assertEqual(self.id["a"]["3"]["1"]["1"], 10)
        self.assertEqual(self.id["a"]["3"]["2"], b"qwe")

        with self.assertRaises(KeyError):
            a = self.id["a"]["2"]["a"]

    def test_invalid_type(self):
        with self.assertRaises(TypeError):
            self.id["a"] = self


class TestIdPropertyGroupView(TestHelper, unittest.TestCase):

    def test_type(self):
        self.assertEqual(type(self.id.keys()), idprop.types.IDPropertyGroupViewKeys)
        self.assertEqual(type(self.id.values()), idprop.types.IDPropertyGroupViewValues)
        self.assertEqual(type(self.id.items()), idprop.types.IDPropertyGroupViewItems)

        self.assertEqual(type(iter(self.id.keys())), idprop.types.IDPropertyGroupIterKeys)
        self.assertEqual(type(iter(self.id.values())), idprop.types.IDPropertyGroupIterValues)
        self.assertEqual(type(iter(self.id.items())), idprop.types.IDPropertyGroupIterItems)

    def test_basic(self):
        text = ["A", "B", "C"]
        for i, ch in enumerate(text):
            self.id[ch] = i
        self.assertEqual(len(self.id.keys()), len(text))
        self.assertEqual(list(self.id.keys()), text)
        self.assertEqual(list(reversed(self.id.keys())), list(reversed(text)))

        self.assertEqual(len(self.id.values()), len(text))
        self.assertEqual(list(self.id.values()), list(range(len(text))))
        self.assertEqual(list(reversed(self.id.values())), list(reversed(range(len(text)))))

        self.assertEqual(len(self.id.items()), len(text))
        self.assertEqual(list(self.id.items()), [(k, v) for v, k in enumerate(text)])
        self.assertEqual(list(reversed(self.id.items())), list(reversed([(k, v) for v, k in enumerate(text)])))

        # Check direct iteration is working as expected.
        self.id["group"] = {ch: i for i, ch in enumerate(text)}
        group = self.id["group"]

        self.assertEqual(len(group), len(text))
        self.assertEqual(list(iter(group)), text)

    def test_contains(self):
        # Check `idprop.types.IDPropertyGroupView{Keys/Values/Items}.__contains__`
        text = ["A", "B", "C"]
        for i, ch in enumerate(text):
            self.id[ch] = i

        self.assertIn("A", self.id)
        self.assertNotIn("D", self.id)

        self.assertIn("A", self.id.keys())
        self.assertNotIn("D", self.id.keys())

        self.assertIn(2, self.id.values())
        self.assertNotIn(3, self.id.values())

        self.assertIn(("A", 0), self.id.items())
        self.assertNotIn(("D", 3), self.id.items())


class TestBufferProtocol(TestHelper, unittest.TestCase):

    def test_copy(self):
        self.id["a"] = array("i", [1, 2, 3, 4, 5])
        self.id["b"] = self.id["a"]
        self.assertEqual(self.id["a"].to_list(), self.id["b"].to_list())

    def test_memview_attributes(self):
        mylist = [1, 2, 3]
        self.id["a"] = mylist

        view1 = memoryview(self.id["a"])
        view2 = memoryview(array("i", mylist))

        self.assertEqualMemviews(view1, view2)

    def assertEqualMemviews(self, view1, view2):
        props_to_compare = (
            "contiguous", "format", "itemsize", "nbytes", "ndim",
            "readonly", "shape", "strides", "suboffsets",
        )
        for attr in props_to_compare:
            self.assertEqual(getattr(view1, attr), getattr(view2, attr))

        self.assertEqual(list(view1), list(view2))
        self.assertEqual(view1.tobytes(), view2.tobytes())


if np is not None:
    class TestBufferProtocol_Numpy(TestHelper, unittest.TestCase):
        def test_int(self):
            self.id["a"] = array("i", [1, 2, 3, 4, 5])
            a = np.frombuffer(self.id["a"], self.id["a"].typecode)
            self.assertEqual(len(a), 5)
            a[2] = 10
            self.assertEqual(self.id["a"].to_list(), [1, 2, 10, 4, 5])

        def test_float(self):
            self.id["a"] = array("f", [1.0, 2.0, 3.0, 4.0])
            a = np.frombuffer(self.id["a"], self.id["a"].typecode)
            self.assertEqual(len(a), 4)
            a[-1] = 10
            self.assertEqual(self.id["a"].to_list(), [1.0, 2.0, 3.0, 10.0])

        def test_double(self):
            self.id["a"] = array("d", [1.0, 2.0, 3.0, 4.0])
            a = np.frombuffer(self.id["a"], self.id["a"].typecode)
            a[1] = 10
            self.assertEqual(self.id["a"].to_list(), [1.0, 10.0, 3.0, 4.0])

        def test_full_update(self):
            self.id["a"] = array("i", [1, 2, 3, 4, 5, 6])
            a = np.frombuffer(self.id["a"], self.id["a"].typecode)
            a[:] = [10, 20, 30, 40, 50, 60]
            self.assertEqual(self.id["a"].to_list(), [10, 20, 30, 40, 50, 60])

        def test_partial_update(self):
            self.id["a"] = array("i", [1, 2, 3, 4, 5, 6, 7, 8])
            a = np.frombuffer(self.id["a"], self.id["a"].typecode)
            a[1:5] = [10, 20, 30, 40]
            self.assertEqual(self.id["a"].to_list(), [1, 10, 20, 30, 40, 6, 7, 8])


class TestRNAData(TestHelper, unittest.TestCase):

    def test_custom_properties_access(self):
        # Ensure the RNA path resolving behaves as expected & is compatible with ID-property keys.
        keys_to_test = (
            "test",
            "\\"
            '"',
            '""',
            '"""',
            '[',
            ']',
            '[]',
            '["]',
            '[""]',
            '["""]',
            '[""""]',
            # Empty properties are also valid.
            "",
        )
        for key_id in keys_to_test:
            self.id[key_id] = 1
            self.assertEqual(self.id[key_id], self.id.path_resolve('["%s"]' % bpy.utils.escape_identifier(key_id)))
            del self.id[key_id]

    def test_custom_properties_none(self):
        bpy.data.objects.new("test", None)
        test_object = bpy.data.objects["test"]

        # Access default RNA data values.
        test_object.id_properties_clear()
        test_object["test_prop"] = 0.5
        ui_data_test_prop = test_object.id_properties_ui("test_prop")

        rna_data = ui_data_test_prop.as_dict()
        self.assertTrue("min" in rna_data)
        self.assertLess(rna_data["min"], -10000.0)
        self.assertEqual(rna_data["subtype"], "NONE")
        self.assertGreater(rna_data["soft_max"], 10000.0)

        # Change RNA data values.
        ui_data_test_prop.update(subtype="TEMPERATURE", min=0, soft_min=0.1)
        rna_data = ui_data_test_prop.as_dict()
        self.assertEqual(rna_data["min"], 0)
        self.assertEqual(rna_data["soft_min"], 0.1)
        self.assertEqual(rna_data["subtype"], "TEMPERATURE")

        # Copy RNA data values from one property to another.
        test_object["test_prop_2"] = 11.7
        ui_data_test_prop_2 = test_object.id_properties_ui("test_prop_2")
        ui_data_test_prop_2.update_from(ui_data_test_prop)
        rna_data = ui_data_test_prop_2.as_dict()
        self.assertEqual(rna_data["min"], 0)
        self.assertEqual(rna_data["soft_min"], 0.1)
        self.assertEqual(rna_data["subtype"], "TEMPERATURE")
        self.assertGreater(rna_data["soft_max"], 10000.0)

        # Copy RNA data values to another object's property.
        bpy.data.objects.new("test_2", None)
        test_object_2 = bpy.data.objects["test_2"]
        test_object_2["test_prop_3"] = 20.1
        ui_data_test_prop_3 = test_object_2.id_properties_ui("test_prop_3")
        ui_data_test_prop_3.update_from(ui_data_test_prop_2)
        rna_data = ui_data_test_prop_3.as_dict()
        self.assertEqual(rna_data["min"], 0)
        self.assertEqual(rna_data["soft_min"], 0.1)
        self.assertEqual(rna_data["subtype"], "TEMPERATURE")
        self.assertGreater(rna_data["soft_max"], 10000.0)

        # Test RNA data for string property.
        test_object.id_properties_clear()
        test_object["test_string_prop"] = "Hello there!"
        ui_data_test_prop_string = test_object.id_properties_ui("test_string_prop")
        ui_data_test_prop_string.update(default="Goodbye where?")
        rna_data = ui_data_test_prop_string.as_dict()
        self.assertEqual(rna_data["default"], "Goodbye where?")

        # Test RNA data for array property.
        test_object.id_properties_clear()
        test_object["test_array_prop"] = [1, 2, 3]
        ui_data_test_prop_array = test_object.id_properties_ui("test_array_prop")
        ui_data_test_prop_array.update(default=[1, 2])
        rna_data = ui_data_test_prop_array.as_dict()
        self.assertEqual(rna_data["default"], [1, 2])

        # Test RNA data for enum property.
        test_object.id_properties_clear()
        test_object["test_enum_prop"] = 2
        ui_data_test_prop_enum = test_object.id_properties_ui("test_enum_prop")
        ui_data_test_prop_enum_items = [
            ("TOMATOES", "Tomatoes", "Solanum lycopersicum"),
            ("CUCUMBERS", "Cucumbers", "Cucumis sativus"),
            ("RADISHES", "Radishes", "Raphanus raphanistrum"),
        ]
        ui_data_test_prop_enum.update(items=ui_data_test_prop_enum_items)
        ui_data_test_prop_enum_items_full = [
            ("TOMATOES", "Tomatoes", "Solanum lycopersicum", 0, 0),
            ("CUCUMBERS", "Cucumbers", "Cucumis sativus", 0, 1),
            ("RADISHES", "Radishes", "Raphanus raphanistrum", 0, 2),
        ]
        rna_data = ui_data_test_prop_enum.as_dict()
        self.assertEqual(rna_data["items"], ui_data_test_prop_enum_items_full)


if __name__ == '__main__':
    import sys
    sys.argv = [__file__] + (sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else [])
    unittest.main()
