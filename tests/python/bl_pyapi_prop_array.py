# SPDX-FileCopyrightText: 2020-2023 Blender Authors
#
# SPDX-License-Identifier: Apache-2.0

# NOTE: See also `bl_pyapi_prop.py` for the non-`Vector` bpy.props similar tests,
# and `bl_pyapi_idprop.py` for some deeper testing of the consistency between
# the underlying IDProperty storage, and the property data exposed in Python.

# ./blender.bin --background --python tests/python/bl_pyapi_prop_array.py -- --verbose

__all__ = (
    "main",
)

import bpy
from bpy.props import (
    BoolVectorProperty,
    FloatVectorProperty,
    IntVectorProperty,
)
import unittest
import numpy as np
import math

id_inst = bpy.context.scene
id_type = bpy.types.Scene


# -----------------------------------------------------------------------------
# Utility Functions

def seq_items_xform(data, xform_fn):
    """
    Recursively expand items using ``xform_fn``.
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


def matrix_with_repeating_digits(dims_x, dims_y):
    """
    Create an 2D matrix with easily identifiable unique elements:
    When: dims_x=4, dims_y=3 results in:
       ((1, 2, 3, 4), (11, 22, 33, 44), (111, 222, 333, 444))
    """
    prev = (0,) * dims_x
    return tuple([
        (prev := tuple(((10 ** yi) * xi) + prev[i] for i, xi in enumerate(range(1, dims_x + 1))))
        for yi in range(dims_y)
    ])


# -----------------------------------------------------------------------------
# Tests

class TestPropArrayIndex(unittest.TestCase):
    # Test index and slice access of 'vector' (aka array) properties.

    size_1d = 10
    valid_indices_1d = (
        (4, 9, -5, slice(7, 9)),
    )
    invalid_indices_1d = (
        (
            # Wrong slice indices are clamped to valid values, and therefore return smaller-than-expected arrays
            (..., (slice(7, 11),)),
            (IndexError, (-11, 10)),
            # Slices with step are not supported currently - although the 'inlined' [x:y:z] syntax does work?
            (TypeError, (slice(2, 9, 3),)),
        ),
    )

    size_2d = (4, 1)
    valid_indices_2d = (
        (1, 3, -2, slice(0, 3)),
        (0, -1, slice(0, 1)),
    )
    invalid_indices_2d = (
        (
            # Wrong slice indices are clamped to valid values, and therefore return smaller-than-expected arrays
            (..., (slice(0, 5),)),
            (IndexError, (-5, 4)),
            # Slices with step are not supported currently - although the 'inlined' [x:y:z] syntax does work?
            (TypeError, (slice(0, 4, 2),)),
        ),
        (
            # Wrong slice indices are clamped to valid values, and therefore return smaller-than-expected arrays
            (..., (slice(1, 2),)),
            (IndexError, (-2, 1)),
            # Slices with step are not supported currently - although the 'inlined' [x:y:z] syntax does work?
            (TypeError, (slice(0, 1, 2),)),
        ),
    )

    size_3d = (3, 2, 4)
    valid_indices_3d = (
        (1, 2, -2, slice(0, 3)),
        (0, -2, slice(0, 1)),
        (3, -4, slice(1, 3)),
    )
    invalid_indices_3d = (
        (
            # Wrong slice indices are clamped to valid values, and therefore return smaller-than-expected arrays
            (..., (slice(0, 5),)),
            (IndexError, (-4, 3)),
            # Slices with step are not supported currently - although the 'inlined' [x:y:z] syntax does work?
            (TypeError, (slice(0, 3, 2),)),
        ),
        (
            # Wrong slice indices are clamped to valid values, and therefore return smaller-than-expected arrays
            (..., (slice(1, 3),)),
            (IndexError, (-3, 2)),
            # Slices with step are not supported currently - although the 'inlined' [x:y:z] syntax does work?
            (TypeError, (slice(0, 1, 2),)),
        ),
        (
            # Wrong slice indices are clamped to valid values, and therefore return smaller-than-expected arrays
            (..., (slice(2, 7),)),
            (IndexError, (-5, 4)),
            # Slices with step are not supported currently - although the 'inlined' [x:y:z] syntax does work?
            (TypeError, (slice(1, 4, 2),)),
        ),
    )

    def setUp(self):
        id_type.test_array_b_1d = BoolVectorProperty(size=self.size_1d)
        id_type.test_array_b_2d = BoolVectorProperty(size=self.size_2d)
        id_type.test_array_b_3d = BoolVectorProperty(size=self.size_3d)
        id_type.test_array_i_1d = IntVectorProperty(size=self.size_1d)
        id_type.test_array_i_2d = IntVectorProperty(size=self.size_2d)
        id_type.test_array_i_3d = IntVectorProperty(size=self.size_3d)
        id_type.test_array_f_1d = FloatVectorProperty(size=self.size_1d)
        id_type.test_array_f_2d = FloatVectorProperty(size=self.size_2d)
        id_type.test_array_f_3d = FloatVectorProperty(size=self.size_3d)

        self.test_array_b_2d_storage = [[bool(v) for v in range(self.size_2d[1])] for i in range(self.size_2d[0])]

        def bool_set_(s, v):
            self.test_array_b_2d_storage = v

        self.test_array_i_2d_storage = [[int(v) for v in range(self.size_2d[1])] for i in range(self.size_2d[0])]

        def int_set_(s, v):
            self.test_array_i_2d_storage = v

        self.test_array_f_2d_storage = [[float(v) for v in range(self.size_2d[1])] for i in range(self.size_2d[0])]

        def float_set_(s, v):
            self.test_array_f_2d_storage = v

        id_type.test_array_b_2d_getset = BoolVectorProperty(
            size=self.size_2d,
            get=lambda s: self.test_array_b_2d_storage,
            set=bool_set_,
        )
        id_type.test_array_i_2d_getset = IntVectorProperty(
            size=self.size_2d,
            get=lambda s: self.test_array_i_2d_storage,
            set=int_set_,
        )
        id_type.test_array_f_2d_getset = FloatVectorProperty(
            size=self.size_2d,
            get=lambda s: self.test_array_f_2d_storage,
            set=float_set_,
        )

        id_type.test_array_b_3d_transform = BoolVectorProperty(
            size=self.size_3d,
            get_transform=lambda s, c_v, isset: seq_items_xform(c_v, lambda v: not v),
            set_transform=lambda s, n_v, c_v, isset: seq_items_xform(n_v, lambda v: not v),
        )
        id_type.test_array_i_3d_transform = IntVectorProperty(
            size=self.size_3d,
            get_transform=lambda s, c_v, isset: seq_items_xform(c_v, lambda v: v + 1),
            set_transform=lambda s, n_v, c_v, isset: seq_items_xform(n_v, lambda v: v - 1),
        )
        id_type.test_array_f_3d_transform = FloatVectorProperty(
            size=self.size_3d,
            get_transform=lambda s, c_v, isset: seq_items_xform(c_v, lambda v: v * 2.0),
            set_transform=lambda s, n_v, c_v, isset: seq_items_xform(n_v, lambda v: v / 2.0),
        )

        id_type.test_array_b_2d_getset_transform = BoolVectorProperty(
            size=self.size_2d,
            get=lambda s: self.test_array_b_2d_storage,
            set=bool_set_,
            get_transform=lambda s, c_v, isset: seq_items_xform(c_v, lambda v: not v),
            set_transform=lambda s, n_v, c_v, isset: seq_items_xform(n_v, lambda v: not v),
        )
        id_type.test_array_i_2d_getset_transform = IntVectorProperty(
            size=self.size_2d,
            get=lambda s: self.test_array_i_2d_storage,
            set=int_set_,
            get_transform=lambda s, c_v, isset: seq_items_xform(c_v, lambda v: v + 1),
            set_transform=lambda s, n_v, c_v, isset: seq_items_xform(n_v, lambda v: v - 1),
        )
        id_type.test_array_f_2d_getset_transform = FloatVectorProperty(
            size=self.size_2d,
            get=lambda s: self.test_array_f_2d_storage,
            set=float_set_,
            get_transform=lambda s, c_v, isset: seq_items_xform(c_v, lambda v: v * 2.0),
            set_transform=lambda s, n_v, c_v, isset: seq_items_xform(n_v, lambda v: v / 2.0),
        )

    def tearDown(self):
        del id_type.test_array_f_1d
        del id_type.test_array_f_2d
        del id_type.test_array_f_3d
        del id_type.test_array_i_1d
        del id_type.test_array_i_2d
        del id_type.test_array_i_3d
        del id_type.test_array_b_1d
        del id_type.test_array_b_2d
        del id_type.test_array_b_3d

        del id_type.test_array_f_2d_getset
        del id_type.test_array_i_2d_getset
        del id_type.test_array_b_2d_getset

        del id_type.test_array_f_3d_transform
        del id_type.test_array_i_3d_transform
        del id_type.test_array_b_3d_transform

        del id_type.test_array_f_2d_getset_transform
        del id_type.test_array_i_2d_getset_transform
        del id_type.test_array_b_2d_getset_transform

    @staticmethod
    def compute_slice_len(s):
        if not isinstance(s, slice):
            return ...
        return math.ceil((abs(s.stop) - (abs(s.start or 0))) / (abs(s.step or 1)))

    def do_test_indices_access_current_dimension(
            self, prop_array, prop_size, valid_indices, invalid_indices, current_dimension
    ):
        self.assertEqual(len(prop_array), prop_size[current_dimension])
        for idx in valid_indices[current_dimension]:
            expected_len = self.compute_slice_len(idx)
            data = prop_array[idx]
            if expected_len is not ...:
                self.assertEqual(len(data), expected_len)
            prop_array[idx] = data

        for error, indices in invalid_indices[current_dimension]:
            for idx in indices:
                if error is ...:
                    self.assertTrue(isinstance(idx, slice))
                    expected_len = self.compute_slice_len(idx)
                    data = prop_array[idx]
                    self.assertLess(len(data), expected_len)
                else:
                    with self.assertRaises(error):
                        data = prop_array[idx]

    def do_test_indices_access(self, prop_array, prop_size, valid_indices, invalid_indices):
        if not isinstance(prop_size, (tuple, list)):
            prop_size = (prop_size,)
        num_dimensions = len(prop_size)

        self.do_test_indices_access_current_dimension(
            prop_array, prop_size, valid_indices, invalid_indices, 0
        )
        if num_dimensions > 1:
            for sub_prop_array in prop_array:
                self.do_test_indices_access_current_dimension(
                    sub_prop_array, prop_size, valid_indices, invalid_indices, 1
                )
                if num_dimensions > 2:
                    for sub_sub_prop_array in sub_prop_array:
                        self.do_test_indices_access_current_dimension(
                            sub_sub_prop_array, prop_size, valid_indices, invalid_indices, 2
                        )

    def test_indices_access_b_1d(self):
        self.do_test_indices_access(
            id_inst.test_array_b_1d, self.size_1d, self.valid_indices_1d, self.invalid_indices_1d
        )

    def test_indices_access_b_2d(self):
        self.do_test_indices_access(
            id_inst.test_array_b_2d, self.size_2d, self.valid_indices_2d, self.invalid_indices_2d
        )

    def test_indices_access_b_3d(self):
        self.do_test_indices_access(
            id_inst.test_array_b_3d, self.size_3d, self.valid_indices_3d, self.invalid_indices_3d
        )

    def test_indices_access_i_1d(self):
        self.do_test_indices_access(
            id_inst.test_array_i_1d, self.size_1d, self.valid_indices_1d, self.invalid_indices_1d
        )

    def test_indices_access_i_2d(self):
        self.do_test_indices_access(
            id_inst.test_array_i_2d, self.size_2d, self.valid_indices_2d, self.invalid_indices_2d
        )

    def test_indices_access_i_3d(self):
        self.do_test_indices_access(
            id_inst.test_array_i_3d, self.size_3d, self.valid_indices_3d, self.invalid_indices_3d
        )

    def test_indices_access_f_1d(self):
        self.do_test_indices_access(
            id_inst.test_array_f_1d, self.size_1d, self.valid_indices_1d, self.invalid_indices_1d
        )

    def test_indices_access_f_2d(self):
        self.do_test_indices_access(
            id_inst.test_array_f_2d, self.size_2d, self.valid_indices_2d, self.invalid_indices_2d
        )

    def test_indices_access_f_3d(self):
        self.do_test_indices_access(
            id_inst.test_array_f_3d, self.size_3d, self.valid_indices_3d, self.invalid_indices_3d
        )

    def test_indices_access_b_2d_getset(self):
        self.do_test_indices_access(
            id_inst.test_array_b_2d_getset, self.size_2d, self.valid_indices_2d, self.invalid_indices_2d
        )

    def test_indices_access_i_2d_getset(self):
        self.do_test_indices_access(
            id_inst.test_array_i_2d_getset, self.size_2d, self.valid_indices_2d, self.invalid_indices_2d
        )

    def test_indices_access_f_2d_getset(self):
        self.do_test_indices_access(
            id_inst.test_array_f_2d_getset, self.size_2d, self.valid_indices_2d, self.invalid_indices_2d
        )

    def test_indices_access_b_3d_transform(self):
        self.do_test_indices_access(
            id_inst.test_array_b_3d_transform, self.size_3d, self.valid_indices_3d, self.invalid_indices_3d
        )

    def test_indices_access_i_3d_transform(self):
        self.do_test_indices_access(
            id_inst.test_array_i_3d_transform, self.size_3d, self.valid_indices_3d, self.invalid_indices_3d
        )

    def test_indices_access_f_3d_transform(self):
        self.do_test_indices_access(
            id_inst.test_array_f_3d_transform, self.size_3d, self.valid_indices_3d, self.invalid_indices_3d
        )

    def test_indices_access_b_2d_getset_transform(self):
        self.do_test_indices_access(
            id_inst.test_array_b_2d_getset_transform, self.size_2d, self.valid_indices_2d, self.invalid_indices_2d
        )

    def test_indices_access_i_2d_getset_transform(self):
        self.do_test_indices_access(
            id_inst.test_array_i_2d_getset_transform, self.size_2d, self.valid_indices_2d, self.invalid_indices_2d
        )

    def test_indices_access_f_2d_getset_transform(self):
        self.do_test_indices_access(
            id_inst.test_array_f_2d_getset_transform, self.size_2d, self.valid_indices_2d, self.invalid_indices_2d
        )


class TestPropArrayForeach(unittest.TestCase):
    # Test foreach_get/_set access of Int and Float vector properties (bool ones do not support this).

    size_1d = 10
    size_2d = (4, 1)
    size_3d = (3, 2, 4)

    def setUp(self):
        id_type.test_array_f_1d = FloatVectorProperty(size=self.size_1d)
        id_type.test_array_f_2d = FloatVectorProperty(size=self.size_2d)
        id_type.test_array_f_3d = FloatVectorProperty(size=self.size_3d)
        id_type.test_array_i_1d = IntVectorProperty(size=self.size_1d)
        id_type.test_array_i_2d = IntVectorProperty(size=self.size_2d)
        id_type.test_array_i_3d = IntVectorProperty(size=self.size_3d)

    def tearDown(self):
        del id_type.test_array_f_1d
        del id_type.test_array_f_2d
        del id_type.test_array_f_3d
        del id_type.test_array_i_1d
        del id_type.test_array_i_2d
        del id_type.test_array_i_3d

    @staticmethod
    def parse_test_args(prop_array_first_dim, prop_type, prop_size):
        match prop_type:
            case 'INT':
                expected_dtype = np.int32
                wrong_kind_dtype = np.float32
                wrong_size_dtype = np.int64
            case 'FLOAT':
                expected_dtype = np.float32
                wrong_kind_dtype = np.int32
                wrong_size_dtype = np.float64
            case _:
                raise AssertionError("Unexpected property type '%s'" % prop_type)

        expected_length = np.prod(prop_size)
        num_dims = len(prop_size)

        assert expected_length > 0
        too_short_length = expected_length - 1

        match num_dims:
            case 1:
                def get_flat_iterable_all_dimensions():
                    return prop_array_first_dim[:]
            case 2:
                def get_flat_iterable_all_dimensions():
                    return (flat_elem for array_1d in prop_array_first_dim[:] for flat_elem in array_1d[:])
            case 3:
                def get_flat_iterable_all_dimensions():
                    return (flat_elem
                            for array_2d in prop_array_first_dim[:]
                            for array_1d in array_2d[:]
                            for flat_elem in array_1d[:])
            case _:
                raise AssertionError("Number of dimensions must be 1, 2 or 3, but was %i" % num_dims)

        return (expected_dtype, wrong_kind_dtype, wrong_size_dtype, expected_length, too_short_length,
                get_flat_iterable_all_dimensions)

    def do_test_foreach_getset_current_dimension(
            self, prop_array, expected_dtype, wrong_kind_dtype, wrong_size_dtype,
            expected_length, too_short_length, get_flat_iterable_all_dimensions,
    ):
        with self.assertRaises(TypeError):
            prop_array.foreach_set(range(too_short_length))

        prop_array.foreach_set(range(5, 5 + expected_length))

        with self.assertRaises(TypeError):
            prop_array.foreach_set(np.arange(too_short_length, dtype=expected_dtype))

        with self.assertRaises(TypeError):
            prop_array.foreach_set(np.arange(expected_length, dtype=wrong_size_dtype))

        with self.assertRaises(TypeError):
            prop_array.foreach_get(np.arange(expected_length, dtype=wrong_kind_dtype))

        a = np.arange(expected_length, dtype=expected_dtype)
        prop_array.foreach_set(a)

        with self.assertRaises(TypeError):
            prop_array.foreach_set(a[:too_short_length])

        for v1, v2 in zip(a, get_flat_iterable_all_dimensions()):
            self.assertEqual(v1, v2)

        b = np.empty(expected_length, dtype=expected_dtype)
        prop_array.foreach_get(b)
        for v1, v2 in zip(a, b):
            self.assertEqual(v1, v2)

        b = [None] * expected_length
        prop_array.foreach_get(b)
        for v1, v2 in zip(a, b):
            self.assertEqual(v1, v2)

    def do_test_foreach_getset(self, prop_array, prop_type, prop_size):
        if not isinstance(prop_size, (tuple, list)):
            prop_size = (prop_size,)
        num_dimensions = len(prop_size)

        test_args = self.parse_test_args(prop_array, prop_type, prop_size)

        # Test that foreach_get/foreach_set work, and work the same regardless of the current dimension/sub-array being
        # accessed.
        self.do_test_foreach_getset_current_dimension(prop_array, *test_args)
        if num_dimensions > 1:
            for i in range(prop_size[0]):
                self.do_test_foreach_getset_current_dimension(prop_array[i], *test_args)
                if num_dimensions > 2:
                    for j in range(prop_size[1]):
                        self.do_test_foreach_getset_current_dimension(prop_array[i][j], *test_args)

    def test_foreach_getset_i_1d(self):
        self.do_test_foreach_getset(id_inst.test_array_i_1d, 'INT', self.size_1d)

    def test_foreach_getset_f_1d(self):
        self.do_test_foreach_getset(id_inst.test_array_f_1d, 'FLOAT', self.size_1d)

    def test_foreach_getset_i_2d(self):
        self.do_test_foreach_getset(id_inst.test_array_i_2d, 'INT', self.size_2d)

    def test_foreach_getset_f_2d(self):
        self.do_test_foreach_getset(id_inst.test_array_f_2d, 'FLOAT', self.size_2d)

    def test_foreach_getset_i_3d(self):
        self.do_test_foreach_getset(id_inst.test_array_i_3d, 'INT', self.size_3d)

    def test_foreach_getset_f_3d(self):
        self.do_test_foreach_getset(id_inst.test_array_f_3d, 'FLOAT', self.size_3d)


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

    def _test_matrix(self, dim_x, dim_y):
        data = matrix_with_repeating_digits(dim_x, dim_y)
        data_native = seq_items_xform(data, lambda v: float(v))
        id_type.temp = FloatVectorProperty(size=(dim_x, dim_y), subtype='MATRIX', default=data_native)
        data_as_tuple = seq_items_as_tuple(id_inst.temp)
        self.assertEqual(data_as_tuple, data_native)
        del id_type.temp

    def _test_matrix_with_callbacks(self, dim_x, dim_y):
        # """
        # Internally matrices have rows/columns swapped,
        # This test ensures this is being done properly.
        # """
        data = matrix_with_repeating_digits(dim_x, dim_y)
        data_native = seq_items_xform(data, lambda v: float(v))
        local_data = {"array": data}

        def get_fn(id_arg):
            return local_data["array"]

        def set_fn(id_arg, value):
            local_data["array"] = value

        def get_tx_fn(id_arg, curr_value, is_set):
            return seq_items_xform(curr_value, lambda v: v + 1.0)

        def set_tx_fn(id_arg, new_value, curr_value, is_set):
            return seq_items_xform(new_value, lambda v: v - 1.0)

        id_type.temp = FloatVectorProperty(size=(dim_x, dim_y), subtype='MATRIX', get=get_fn, set=set_fn)
        id_inst.temp = data_native
        data_as_tuple = seq_items_as_tuple(id_inst.temp)
        self.assertEqual(data_as_tuple, data_native)
        del id_type.temp

        id_type.temp = FloatVectorProperty(
            size=(dim_x, dim_y), subtype='MATRIX', get_transform=get_tx_fn, set_transform=set_tx_fn)
        id_inst.temp = data_native
        data_as_tuple = seq_items_as_tuple(id_inst.temp)
        self.assertEqual(data_as_tuple, data_native)
        del id_type.temp

        id_type.temp = FloatVectorProperty(
            size=(dim_x, dim_y),
            subtype='MATRIX',
            get=get_fn,
            set=set_fn,
            get_transform=get_tx_fn,
            set_transform=set_tx_fn)
        id_inst.temp = data_native
        data_as_tuple = seq_items_as_tuple(id_inst.temp)
        self.assertEqual(data_as_tuple, data_native)
        del id_type.temp

    def test_matrix_3x3(self):
        self._test_matrix(3, 3)

    def test_matrix_4x4(self):
        self._test_matrix(4, 4)

    def test_matrix_with_callbacks_3x3(self):
        self._test_matrix_with_callbacks(3, 3)

    def test_matrix_with_callbacks_4x4(self):
        self._test_matrix_with_callbacks(4, 4)


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


class TestPropArrayInvalidForeachGetSet(unittest.TestCase):
    """
    Test proper detection of invalid usages of foreach_get/foreach_set.
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

    def test_foreach_valid(self):
        me = self.me

        # Non-array (scalar) data access.
        valid_1b_list = [False] * len(me.vertices)
        me.vertices.foreach_get("select", valid_1b_list)
        self.assertEqual(tuple([True] * self.dims), tuple(valid_1b_list))

        valid_1b_list = [False] * len(me.vertices)
        me.vertices.foreach_set("select", valid_1b_list)
        for v in me.vertices:
            self.assertFalse(v.select)

        # Array (vector) data access.
        valid_3f_list = [1.0] * (len(me.vertices) * 3)
        me.vertices.foreach_get("co", valid_3f_list)
        self.assertEqual(tuple([0.0] * self.dims * 3), tuple(valid_3f_list))

        valid_3f_list = [1.0] * (len(me.vertices) * 3)
        me.vertices.foreach_set("co", valid_3f_list)
        for v in me.vertices:
            self.assertEqual(tuple(v.co), (1.0, 1.0, 1.0))

    def test_foreach_invalid_smaller_array(self):
        me = self.me

        # Non-array (scalar) data access.
        invalid_1b_list = [False] * (len(me.vertices) - 1)
        with self.assertRaises(RuntimeError):
            me.vertices.foreach_get("select", invalid_1b_list)

        invalid_1b_list = [False] * (len(me.vertices) - 1)
        with self.assertRaises(RuntimeError):
            me.vertices.foreach_set("select", invalid_1b_list)

        # Array (vector) data access.
        invalid_3f_list = [1.0] * (len(me.vertices) * 3 - 1)
        with self.assertRaises(RuntimeError):
            me.vertices.foreach_get("co", invalid_3f_list)

        invalid_3f_list = [1.0] * (len(me.vertices) * 3 - 1)
        with self.assertRaises(RuntimeError):
            me.vertices.foreach_set("co", invalid_3f_list)


def main():
    import sys
    sys.argv = [__file__] + (sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else [])
    unittest.main()


if __name__ == '__main__':
    main()
