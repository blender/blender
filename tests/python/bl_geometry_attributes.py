# SPDX-FileCopyrightText: 2024 Blender Authors
#
# SPDX-License-Identifier: Apache-2.0

# ./blender.bin --background --python tests/python/bl_geometry_attributes.py -- --verbose
import bpy
import unittest


class TestCurves(unittest.TestCase):
    def setUp(self):
        self.curves = bpy.data.hair_curves.new("test")
        # 50 points, 4 curves
        self.curves.add_curves([5, 10, 15, 20])

    def tearDown(self):
        bpy.data.hair_curves.remove(self.curves)
        del self.curves

    def test_add_attribute(self):
        a = self.curves.attributes.new("a", 'FLOAT', 'POINT')
        self.assertTrue(a.name == "a")
        self.assertTrue(a.data_type == 'FLOAT')
        self.assertTrue(a.domain == 'POINT')
        self.assertTrue(a.storage_type == 'ARRAY')
        self.assertFalse(a.is_internal)
        self.assertTrue(len(a.data) == 50)

    def test_is_required(self):
        a = self.curves.attributes.new("a", 'FLOAT', 'POINT')
        self.assertFalse(a.is_required)
        self.assertTrue(self.curves.attributes["position"].is_required)

    def test_pointer_stability_on_add(self):
        attrs = [self.curves.attributes.new("a" + str(i), 'FLOAT', 'POINT') for i in range(100)]
        for i in range(100):
            self.assertTrue(attrs[i].name == "a" + str(i))
            self.assertTrue(attrs[i].data_type == 'FLOAT')
            self.assertTrue(attrs[i].domain == 'POINT')

        # Remove some attributes
        for i in range(50):
            self.curves.attributes.remove(attrs[i])
            del attrs[i]

        self.assertTrue(len(self.curves.attributes) == 51)
        self.assertTrue(self.curves.attributes["a51"].name == "a51")

    def test_add_same_name(self):
        a = self.curves.attributes.new("a", 'FLOAT', 'POINT')
        b = self.curves.attributes.new("a", 'BOOLEAN', 'CURVE')
        self.assertFalse(a.name == b.name)

    def test_add_wrong_domain(self):
        with self.assertRaises(RuntimeError):
            self.curves.attributes.new("a", 'FLOAT', 'CORNER')

    def rename_attribute(self, name, new_name):
        with self.assertRaises(RuntimeError):
            self.curves.attributes["position"].name = "asjhfksjhdfkjsh"
        a = self.curves.attributes.new("a", 'FLOAT', 'POINT')
        a.name = "better_name"
        self.assertTrue(a.name == "better_name")
        self.assertTrue(self.curves.attributes["better_name"].name == "better_name")

    def test_long_name(self):
        self.curves.attributes.new("a" * 100, 'FLOAT', 'POINT')
        self.assertTrue(self.curves.attributes["a" * 100].name == "a" * 100)


if __name__ == '__main__':
    import sys
    sys.argv = [__file__] + (sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else [])
    unittest.main()
