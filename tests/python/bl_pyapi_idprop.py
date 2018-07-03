# Apache License, Version 2.0

# ./blender.bin --background -noaudio --python tests/python/bl_pyapi_idprop.py -- --verbose
import bpy
import unittest
import numpy as np
from array import array


class TestHelper:

    @property
    def id(self):
        return self._id

    def setUp(self):
        self._id = bpy.context.scene
        assert(len(self._id.keys()) == 0)

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


class TestBufferProtocol(TestHelper, unittest.TestCase):

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
            "readonly", "shape", "strides", "suboffsets"
        )
        for attr in props_to_compare:
            self.assertEqual(getattr(view1, attr), getattr(view2, attr))

        self.assertEqual(list(view1), list(view2))
        self.assertEqual(view1.tobytes(), view2.tobytes())


if __name__ == '__main__':
    import sys
    sys.argv = [__file__] + (sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else [])
    unittest.main()
