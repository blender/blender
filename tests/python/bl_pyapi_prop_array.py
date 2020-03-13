# Apache License, Version 2.0

# ./blender.bin --background -noaudio --python tests/python/bl_pyapi_prop_array.py -- --verbose
import bpy
import unittest
import numpy as np


class TestPropArray(unittest.TestCase):
    def setUp(self):
        bpy.types.Scene.test_array_f = bpy.props.FloatVectorProperty(size=10)
        bpy.types.Scene.test_array_i = bpy.props.IntVectorProperty(size=10)
        scene = bpy.context.scene
        self.array_f = scene.test_array_f
        self.array_i = scene.test_array_i

    def test_foreach_getset_i(self):
        with self.assertRaises(TypeError):
            self.array_i.foreach_set(range(5))

        self.array_i.foreach_set(range(5, 15))

        with self.assertRaises(TypeError):
            self.array_i.foreach_set(np.arange(5, dtype=np.int32))

        with self.assertRaises(TypeError):
            self.array_i.foreach_set(np.arange(10, dtype=np.int64))

        with self.assertRaises(TypeError):
            self.array_i.foreach_get(np.arange(10, dtype=np.float32))

        a = np.arange(10, dtype=np.int32)
        self.array_i.foreach_set(a)

        with self.assertRaises(TypeError):
            self.array_i.foreach_set(a[:5])

        for v1, v2 in zip(a, self.array_i[:]):
            self.assertEqual(v1, v2)

        b = np.empty(10, dtype=np.int32)
        self.array_i.foreach_get(b)
        for v1, v2 in zip(a, b):
            self.assertEqual(v1, v2)

        b = [None] * 10
        self.array_f.foreach_get(b)
        for v1, v2 in zip(a, b):
            self.assertEqual(v1, v2)

    def test_foreach_getset_f(self):
        with self.assertRaises(TypeError):
            self.array_i.foreach_set(range(5))

        self.array_f.foreach_set(range(5, 15))

        with self.assertRaises(TypeError):
            self.array_f.foreach_set(np.arange(5, dtype=np.float32))

        with self.assertRaises(TypeError):
            self.array_f.foreach_set(np.arange(10, dtype=np.int32))

        with self.assertRaises(TypeError):
            self.array_f.foreach_get(np.arange(10, dtype=np.float64))

        a = np.arange(10, dtype=np.float32)
        self.array_f.foreach_set(a)
        for v1, v2 in zip(a, self.array_f[:]):
            self.assertEqual(v1, v2)

        b = np.empty(10, dtype=np.float32)
        self.array_f.foreach_get(b)
        for v1, v2 in zip(a, b):
            self.assertEqual(v1, v2)

        b = [None] * 10
        self.array_f.foreach_get(b)
        for v1, v2 in zip(a, b):
            self.assertEqual(v1, v2)


if __name__ == '__main__':
    import sys
    sys.argv = [__file__] + (sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else [])
    unittest.main()
