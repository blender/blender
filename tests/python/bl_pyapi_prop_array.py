# SPDX-License-Identifier: Apache-2.0

# ./blender.bin --background -noaudio --python tests/python/bl_pyapi_prop_array.py -- --verbose
import bpy
from bpy.props import (
    BoolVectorProperty,
    FloatVectorProperty,
    IntVectorProperty,
)
import unittest
import numpy as np

id_inst = bpy.context.scene
id_type = bpy.types.Scene


# -----------------------------------------------------------------------------
# Utility Functions

def seq_items_xform(data, xform_fn):
    """
    Recursively expand items using `xform_fn`.
    """
    if hasattr(data, "__len__"):
        return tuple(seq_items_xform(v, xform_fn) for v in data)
    return xform_fn(data)


def seq_items_as_tuple(data):
    """
    Return nested sequences as a nested tuple.
    Useful when comparing different kinds of nested sequences.
    """
    return seq_items_xform(data, lambda v: v)


def seq_items_as_dims(data):
    """
    Nested length calculation, extracting the length from each sequence.
    Where a 4x4 matrix returns ``(4, 4)`` for example.
    """
    return ((len(data),) + seq_items_as_dims(data[0])) if hasattr(data, "__len__") else ()


# -----------------------------------------------------------------------------
# Tests

class TestPropArray(unittest.TestCase):
    def setUp(self):
        id_type.test_array_f = FloatVectorProperty(size=10)
        id_type.test_array_i = IntVectorProperty(size=10)
        scene = bpy.context.scene
        self.array_f = scene.test_array_f
        self.array_i = scene.test_array_i

    def tearDown(self):
        del id_type.test_array_f
        del id_type.test_array_i

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


class TestPropArrayMultiDimensional(unittest.TestCase):

    def setUp(self):
        self._initial_dir = set(dir(id_type))

    def tearDown(self):
        for member in (set(dir(id_type)) - self._initial_dir):
            delattr(id_type, member)

    def test_defaults(self):
        # The data is in int format, converted into float & bool to avoid duplication.
        default_data = (
            # 1D.
            (1,),
            (1, 2),
            (1, 2, 3),
            (1, 2, 3, 4),
            # 2D.
            ((1,),),
            ((1,), (11,)),
            ((1, 2), (11, 22)),
            ((1, 2, 3), (11, 22, 33)),
            ((1, 2, 3, 4), (11, 22, 33, 44)),
            # 3D.
            (((1,),),),
            ((1,), (11,), (111,)),
            ((1, 2), (11, 22), (111, 222),),
            ((1, 2, 3), (11, 22, 33), (111, 222, 333)),
            ((1, 2, 3, 4), (11, 22, 33, 44), (111, 222, 333, 444)),
        )
        for data in default_data:
            for (vector_prop_fn, xform_fn) in (
                    (BoolVectorProperty, lambda v: bool(v % 2)),
                    (FloatVectorProperty, lambda v: float(v)),
                    (IntVectorProperty, lambda v: v),
            ):
                data_native = seq_items_xform(data, xform_fn)
                size = seq_items_as_dims(data)
                id_type.temp = vector_prop_fn(size=size, default=data_native)
                data_as_tuple = seq_items_as_tuple(id_inst.temp)
                self.assertEqual(data_as_tuple, data_native)
                del id_type.temp

    def test_matrix(self):
        data = ((1, 2, 3, 4), (11, 22, 33, 44), (111, 222, 333, 444), (1111, 2222, 3333, 4444),)
        data_native = seq_items_xform(data, lambda v: float(v))
        id_type.temp = FloatVectorProperty(size=(4, 4), subtype='MATRIX', default=data_native)
        data_as_tuple = seq_items_as_tuple(id_inst.temp)
        self.assertEqual(data_as_tuple, data_native)
        del id_type.temp

    def test_matrix_with_callbacks(self):
        # """
        # Internally matrices have rows/columns swapped,
        # This test ensures this is being done properly.
        # """
        data = ((1, 2, 3, 4), (11, 22, 33, 44), (111, 222, 333, 444), (1111, 2222, 3333, 4444),)
        data_native = seq_items_xform(data, lambda v: float(v))
        local_data = {"array": data}

        def get_fn(id_arg):
            return local_data["array"]

        def set_fn(id_arg, value):
            local_data["array"] = value

        id_type.temp = FloatVectorProperty(size=(4, 4), subtype='MATRIX', get=get_fn, set=set_fn)
        id_inst.temp = data_native
        data_as_tuple = seq_items_as_tuple(id_inst.temp)
        self.assertEqual(data_as_tuple, data_native)
        del id_type.temp


class TestPropArrayDynamicAssign(unittest.TestCase):
    """
    Pixels are dynamic in the sense the size can change however the assignment does not define the size.
    """

    dims = 12

    def setUp(self):
        self.image = bpy.data.images.new("", self.dims, self.dims)

    def tearDown(self):
        bpy.data.images.remove(self.image)
        self.image = None

    def test_assign_fixed_under_1px(self):
        image = self.image
        with self.assertRaises(ValueError):
            image.pixels = [1.0, 1.0, 1.0, 1.0]

    def test_assign_fixed_under_0px(self):
        image = self.image
        with self.assertRaises(ValueError):
            image.pixels = []

    def test_assign_fixed_over_by_1px(self):
        image = self.image
        with self.assertRaises(ValueError):
            image.pixels = ([1.0, 1.0, 1.0, 1.0] * (self.dims * self.dims)) + [1.0]

    def test_assign_fixed(self):
        # Valid assignment, ensure it works as intended.
        image = self.image
        values = [1.0, 0.0, 1.0, 0.0] * (self.dims * self.dims)
        image.pixels = values
        self.assertEqual(tuple(values), tuple(image.pixels))


class TestPropArrayDynamicArg(unittest.TestCase):
    """
    Index array, a dynamic array argument which defines its own length.
    """

    dims = 8

    def setUp(self):
        self.me = bpy.data.meshes.new("")
        self.me.vertices.add(self.dims)
        self.ob = bpy.data.objects.new("", self.me)

    def tearDown(self):
        bpy.data.objects.remove(self.ob)
        bpy.data.meshes.remove(self.me)
        self.me = None
        self.ob = None

    def test_param_dynamic(self):
        ob = self.ob
        vg = ob.vertex_groups.new(name="")

        # Add none.
        vg.add(index=(), weight=1.0, type='REPLACE')
        for i in range(self.dims):
            with self.assertRaises(RuntimeError):
                vg.weight(i)

        # Add all.
        vg.add(index=range(self.dims), weight=1.0, type='REPLACE')
        self.assertEqual(tuple([1.0] * self.dims), tuple([vg.weight(i) for i in range(self.dims)]))


if __name__ == '__main__':
    import sys
    sys.argv = [__file__] + (sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else [])
    unittest.main()
