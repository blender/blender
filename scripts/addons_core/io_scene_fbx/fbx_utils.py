# SPDX-FileCopyrightText: 2013 Campbell Barton
# SPDX-FileCopyrightText: 2014 Bastien Montagne
#
# SPDX-License-Identifier: GPL-2.0-or-later

import math
import time

from collections import namedtuple
from collections.abc import Iterable
from itertools import zip_longest, chain
from dataclasses import dataclass, field
from typing import Callable
import numpy as np

import bpy
import bpy_extras
from bpy.types import Object, Bone, PoseBone, DepsgraphObjectInstance
from mathutils import Vector, Matrix

from . import encode_bin, data_types


# "Constants"
FBX_VERSION = 7400
# 1004 adds use of "OtherFlags"->"TCDefinition" to control the FBX_KTIME opt-in in FBX version 7700.
FBX_HEADER_VERSION = 1003
FBX_SCENEINFO_VERSION = 100
FBX_TEMPLATES_VERSION = 100

FBX_MODELS_VERSION = 232

FBX_GEOMETRY_VERSION = 124
# Revert back normals to 101 (simple 3D values) for now, 102 (4D + weights) seems not well supported by most apps
# currently, apart from some AD products.
FBX_GEOMETRY_NORMAL_VERSION = 101
FBX_GEOMETRY_BINORMAL_VERSION = 101
FBX_GEOMETRY_TANGENT_VERSION = 101
FBX_GEOMETRY_SMOOTHING_VERSION = 102
FBX_GEOMETRY_CREASE_VERSION = 101
FBX_GEOMETRY_VCOLOR_VERSION = 101
FBX_GEOMETRY_UV_VERSION = 101
FBX_GEOMETRY_MATERIAL_VERSION = 101
FBX_GEOMETRY_LAYER_VERSION = 100
FBX_GEOMETRY_SHAPE_VERSION = 100
FBX_DEFORMER_SHAPE_VERSION = 100
FBX_DEFORMER_SHAPECHANNEL_VERSION = 100
FBX_POSE_BIND_VERSION = 100
FBX_DEFORMER_SKIN_VERSION = 101
FBX_DEFORMER_CLUSTER_VERSION = 100
FBX_MATERIAL_VERSION = 102
FBX_TEXTURE_VERSION = 202
FBX_ANIM_KEY_VERSION = 4008

FBX_NAME_CLASS_SEP = b"\x00\x01"
FBX_ANIM_PROPSGROUP_NAME = "d"

FBX_KTIME_V7 = 46186158000  # This is the number of "ktimes" in one second (yep, precision over the nanosecond...)
# FBX 2019.5 (FBX version 7700) changed the number of "ktimes" per second, however, the new value is opt-in until FBX
# version 8000 where it will probably become opt-out.
FBX_KTIME_V8 = 141120000
# To explicitly use the V7 value in FBX versions 7700-7XXX: fbx_root->"FBXHeaderExtension"->"OtherFlags"->"TCDefinition"
# is set to 127.
# To opt in to the V8 value in FBX version 7700-7XXX: "TCDefinition" is set to 0.
FBX_TIMECODE_DEFINITION_TO_KTIME_PER_SECOND = {
    0: FBX_KTIME_V8,
    127: FBX_KTIME_V7,
}
# The "ktimes" per second for Blender exported FBX is constant because the exported `FBX_VERSION` is constant.
FBX_KTIME = FBX_KTIME_V8 if FBX_VERSION >= 8000 else FBX_KTIME_V7


MAT_CONVERT_LIGHT = Matrix.Rotation(math.pi / 2.0, 4, 'X')  # Blender is -Z, FBX is -Y.
MAT_CONVERT_CAMERA = Matrix.Rotation(math.pi / 2.0, 4, 'Y')  # Blender is -Z, FBX is +X.
# XXX I can't get this working :(
# MAT_CONVERT_BONE = Matrix.Rotation(math.pi / 2.0, 4, 'Z')  # Blender is +Y, FBX is -X.
MAT_CONVERT_BONE = Matrix()


BLENDER_OTHER_OBJECT_TYPES = {'CURVE', 'SURFACE', 'FONT', 'META'}
BLENDER_OBJECT_TYPES_MESHLIKE = {'MESH'} | BLENDER_OTHER_OBJECT_TYPES

SHAPE_KEY_SLIDER_HARD_MIN = bpy.types.ShapeKey.bl_rna.properties["slider_min"].hard_min
SHAPE_KEY_SLIDER_HARD_MAX = bpy.types.ShapeKey.bl_rna.properties["slider_max"].hard_max


# Lamps.
FBX_LIGHT_TYPES = {
    'POINT': 0,  # Point.
    'SUN': 1,    # Directional.
    'SPOT': 2,   # Spot.
    'HEMI': 1,   # Directional.
    'AREA': 3,   # Area.
}
FBX_LIGHT_DECAY_TYPES = {
    'CONSTANT': 0,                   # None.
    'INVERSE_LINEAR': 1,             # Linear.
    'INVERSE_SQUARE': 2,             # Quadratic.
    'INVERSE_COEFFICIENTS': 2,       # Quadratic...
    'CUSTOM_CURVE': 2,               # Quadratic.
    'LINEAR_QUADRATIC_WEIGHTED': 2,  # Quadratic.
}


RIGHT_HAND_AXES = {
    # Up, Forward -> FBX values (tuples of (axis, sign), Up, Front, Coord).
    ('X', '-Y'): ((0, 1), (1, 1), (2, 1)),
    ('X', 'Y'): ((0, 1), (1, -1), (2, -1)),
    ('X', '-Z'): ((0, 1), (2, 1), (1, -1)),
    ('X', 'Z'): ((0, 1), (2, -1), (1, 1)),
    ('-X', '-Y'): ((0, -1), (1, 1), (2, -1)),
    ('-X', 'Y'): ((0, -1), (1, -1), (2, 1)),
    ('-X', '-Z'): ((0, -1), (2, 1), (1, 1)),
    ('-X', 'Z'): ((0, -1), (2, -1), (1, -1)),
    ('Y', '-X'): ((1, 1), (0, 1), (2, -1)),
    ('Y', 'X'): ((1, 1), (0, -1), (2, 1)),
    ('Y', '-Z'): ((1, 1), (2, 1), (0, 1)),
    ('Y', 'Z'): ((1, 1), (2, -1), (0, -1)),
    ('-Y', '-X'): ((1, -1), (0, 1), (2, 1)),
    ('-Y', 'X'): ((1, -1), (0, -1), (2, -1)),
    ('-Y', '-Z'): ((1, -1), (2, 1), (0, -1)),
    ('-Y', 'Z'): ((1, -1), (2, -1), (0, 1)),
    ('Z', '-X'): ((2, 1), (0, 1), (1, 1)),
    ('Z', 'X'): ((2, 1), (0, -1), (1, -1)),
    ('Z', '-Y'): ((2, 1), (1, 1), (0, -1)),
    ('Z', 'Y'): ((2, 1), (1, -1), (0, 1)),  # Blender system!
    ('-Z', '-X'): ((2, -1), (0, 1), (1, -1)),
    ('-Z', 'X'): ((2, -1), (0, -1), (1, 1)),
    ('-Z', '-Y'): ((2, -1), (1, 1), (0, 1)),
    ('-Z', 'Y'): ((2, -1), (1, -1), (0, -1)),
}


# NOTE: Not fully in enum value order, since when exporting the first entry matching the framerate value is used
# (e.g. better have NTSC fullframe than NTSC drop frame for 29.97 framerate).
FBX_FRAMERATES = (
    # (-1.0, 0),  # Default framerate.
    (-1.0, 14),  # Custom framerate.
    (120.0, 1),
    (100.0, 2),
    (60.0, 3),
    (50.0, 4),
    (48.0, 5),
    (30.0, 6),  # BW NTSC, full frame.
    (30.0, 7),  # Drop frame.
    (30.0 / 1.001, 9),  # Color NTSC, full frame.
    (30.0 / 1.001, 8),  # Color NTSC, drop frame.
    (25.0, 10),
    (24.0, 11),
    # (1.0, 12),  # 1000 milli/s (use for date time?).
    (24.0 / 1.001, 13),
    (96.0, 15),
    (72.0, 16),
    (60.0 / 1.001, 17),
    (120.0 / 1.001, 18),
)


# ##### Misc utilities #####

# Enable performance reports (measuring time used to perform various steps of importing or exporting).
DO_PERFMON = False

if DO_PERFMON:
    class PerfMon():
        def __init__(self):
            self.level = -1
            self.ref_time = []

        def level_up(self, message=""):
            self.level += 1
            self.ref_time.append(None)
            if message:
                print("\t" * self.level, message, sep="")

        def level_down(self, message=""):
            if not self.ref_time:
                if message:
                    print(message)
                return
            ref_time = self.ref_time[self.level]
            print("\t" * self.level,
                  "\tDone (%f sec)\n" % ((time.process_time() - ref_time) if ref_time is not None else 0.0),
                  sep="")
            if message:
                print("\t" * self.level, message, sep="")
            del self.ref_time[self.level]
            self.level -= 1

        def step(self, message=""):
            ref_time = self.ref_time[self.level]
            curr_time = time.process_time()
            if ref_time is not None:
                print("\t" * self.level, "\tDone (%f sec)\n" % (curr_time - ref_time), sep="")
            self.ref_time[self.level] = curr_time
            print("\t" * self.level, message, sep="")
else:
    class PerfMon():
        def __init__(self):
            pass

        def level_up(self, message=""):
            pass

        def level_down(self, message=""):
            pass

        def step(self, message=""):
            pass


# Scale/unit mess. FBX can store the 'reference' unit of a file in its UnitScaleFactor property
# (1.0 meaning centimeter, afaik). We use that to reflect user's default unit as set in Blender with scale_length.
# However, we always get values in BU (i.e. meters), so we have to reverse-apply that scale in global matrix...
# Note that when no default unit is available, we assume 'meters' (and hence scale by 100).
def units_blender_to_fbx_factor(scene):
    return 100.0 if (scene.unit_settings.system == 'NONE') else (100.0 * scene.unit_settings.scale_length)


# Note: this could be in a utility (math.units e.g.)...

UNITS = {
    "meter": 1.0,  # Ref unit!
    "kilometer": 0.001,
    "millimeter": 1000.0,
    "foot": 1.0 / 0.3048,
    "inch": 1.0 / 0.0254,
    "turn": 1.0,  # Ref unit!
    "degree": 360.0,
    "radian": math.pi * 2.0,
    "second": 1.0,  # Ref unit!
    "ktime": FBX_KTIME,  # For export use only because the imported "ktimes" per second may vary.
}


def units_convertor(u_from, u_to):
    """Return a convertor between specified units."""
    conv = UNITS[u_to] / UNITS[u_from]
    return lambda v: v * conv


def units_convertor_iter(u_from, u_to):
    """Return an iterable convertor between specified units."""
    conv = units_convertor(u_from, u_to)

    def convertor(it):
        for v in it:
            yield(conv(v))

    return convertor


def matrix4_to_array(mat):
    """Concatenate matrix's columns into a single, flat tuple"""
    # blender matrix is row major, fbx is col major so transpose on write
    return tuple(f for v in mat.transposed() for f in v)


def array_to_matrix4(arr):
    """Convert a single 16-len tuple into a valid 4D Blender matrix"""
    # Blender matrix is row major, fbx is col major so transpose on read
    return Matrix(tuple(zip(*[iter(arr)] * 4))).transposed()


def parray_as_ndarray(arr):
    """Convert an array.array into an np.ndarray that shares the same memory"""
    return np.frombuffer(arr, dtype=arr.typecode)


def similar_values(v1, v2, e=1e-6):
    """Return True if v1 and v2 are nearly the same."""
    if v1 == v2:
        return True
    return ((abs(v1 - v2) / max(abs(v1), abs(v2))) <= e)


def similar_values_iter(v1, v2, e=1e-6):
    """Return True if iterables v1 and v2 are nearly the same."""
    if v1 == v2:
        return True
    for v1, v2 in zip(v1, v2):
        if (v1 != v2) and ((abs(v1 - v2) / max(abs(v1), abs(v2))) > e):
            return False
    return True


def shape_difference_exclude_similar(sv_cos, ref_cos, e=1e-6):
    """Return a tuple of:
        the difference between the vertex cos in sv_cos and ref_cos, excluding any that are nearly the same,
        and the indices of the vertices that are not nearly the same"""
    assert(sv_cos.size == ref_cos.size)

    # Create views of 1 co per row of the arrays, only making copies if needed.
    sv_cos = sv_cos.reshape(-1, 3)
    ref_cos = ref_cos.reshape(-1, 3)

    # Quick check for equality
    if np.array_equal(sv_cos, ref_cos):
        # There's no difference between the two arrays.
        empty_cos = np.empty((0, 3), dtype=sv_cos.dtype)
        empty_indices = np.empty(0, dtype=np.int32)
        return empty_cos, empty_indices

    # Note that unlike math.isclose(a,b), np.isclose(a,b) is not symmetrical and the second argument 'b', is
    # considered to be the reference value.
    # Note that atol=0 will mean that if only one co component being compared is zero, they won't be considered close.
    similar_mask = np.isclose(sv_cos, ref_cos, atol=0, rtol=e)

    # A co is only similar if every component in it is similar.
    co_similar_mask = np.all(similar_mask, axis=1)

    # Get the indices of cos that are not similar.
    not_similar_verts_idx = np.flatnonzero(~co_similar_mask)

    # Subtracting first over the entire arrays and then indexing seems faster than indexing both arrays first and then
    # subtracting, until less than about 3% of the cos are being indexed.
    difference_cos = (sv_cos - ref_cos)[not_similar_verts_idx]
    return difference_cos, not_similar_verts_idx


def _mat4_vec3_array_multiply(mat4, vec3_array, dtype=None, return_4d=False):
    """Multiply a 4d matrix by each 3d vector in an array and return as an array of either 3d or 4d vectors.

    A view of the input array is returned if return_4d=False, the dtype matches the input array and either the matrix is
    None or, ignoring the last row, is a 3x3 identity matrix with no translation:
    ┌1, 0, 0, 0┐
    │0, 1, 0, 0│
    └0, 0, 1, 0┘

    When dtype=None, it defaults to the dtype of the input array."""
    return_dtype = dtype if dtype is not None else vec3_array.dtype
    vec3_array = vec3_array.reshape(-1, 3)

    # Multiplying a 4d mathutils.Matrix by a 3d mathutils.Vector implicitly extends the Vector to 4d during the
    # calculation by appending 1.0 to the Vector and then the 4d result is truncated back to 3d.
    # Numpy does not do an implicit extension to 4d, so it would have to be done explicitly by extending the entire
    # vec3_array to 4d.
    # However, since the w component of the vectors is always 1.0, the last column can be excluded from the
    # multiplication and then added to every multiplied vector afterwards, which avoids having to make a 4d copy of
    # vec3_array beforehand.
    # For a single column vector:
    # ┌a, b, c, d┐   ┌x┐   ┌ax+by+cz+d┐
    # │e, f, g, h│ @ │y│ = │ex+fy+gz+h│
    # │i, j, k, l│   │z│   │ix+jy+kz+l│
    # └m, n, o, p┘   └1┘   └mx+ny+oz+p┘
    # ┌a, b, c┐   ┌x┐   ┌d┐   ┌ax+by+cz┐   ┌d┐   ┌ax+by+cz+d┐
    # │e, f, g│ @ │y│ + │h│ = │ex+fy+gz│ + │h│ = │ex+fy+gz+h│
    # │i, j, k│   └z┘   │l│   │ix+jy+kz│   │l│   │ix+jy+kz+l│
    # └m, n, o┘         └p┘   └mx+ny+oz┘   └p┘   └mx+ny+oz+p┘

    # column_vector_multiplication in mathutils_Vector.c uses double precision math for Matrix @ Vector by casting the
    # matrix's values to double precision and then casts back to single precision when returning the result, so at least
    # double precision math is always be used to match standard Blender behaviour.
    math_precision = np.result_type(np.double, vec3_array)

    to_multiply = None
    to_add = None
    w_to_set = 1.0
    if mat4 is not None:
        mat_np = np.array(mat4, dtype=math_precision)
        # Identity matrix is compared against to check if any matrix multiplication is required.
        identity = np.identity(4, dtype=math_precision)
        if not return_4d:
            # If returning 3d, the entire last row of the matrix can be ignored because it only affects the w component.
            mat_np = mat_np[:3]
            identity = identity[:3]

        # Split mat_np into the columns to multiply and the column to add afterwards.
        # First 3 columns
        multiply_columns = mat_np[:, :3]
        multiply_identity = identity[:, :3]
        # Last column only
        add_column = mat_np.T[3]

        # Analyze the split parts of the matrix to figure out if there is anything to multiply and anything to add.
        if not np.array_equal(multiply_columns, multiply_identity):
            to_multiply = multiply_columns

        if return_4d and to_multiply is None:
            # When there's nothing to multiply, the w component of add_column can be set directly into the array because
            # mx+ny+oz+p becomes 0x+0y+0z+p where p is add_column[3].
            w_to_set = add_column[3]
            # Replace add_column with a view of only the translation.
            add_column = add_column[:3]

        if add_column.any():
            to_add = add_column

    if to_multiply is None:
        # If there's anything to add, ensure it's added using the precision being used for math.
        array_dtype = math_precision if to_add is not None else return_dtype
        if return_4d:
            multiplied_vectors = np.empty((len(vec3_array), 4), dtype=array_dtype)
            multiplied_vectors[:, :3] = vec3_array
            multiplied_vectors[:, 3] = w_to_set
        else:
            # If there's anything to add, ensure a copy is made so that the input vec3_array isn't modified.
            multiplied_vectors = vec3_array.astype(array_dtype, copy=to_add is not None)
    else:
        # Matrix multiplication has the signature (n,k) @ (k,m) -> (n,m).
        # Where v is the number of vectors in vec3_array and d is the number of vector dimensions to return:
        # to_multiply has shape (d,3), vec3_array has shape (v,3) and the result should have shape (v,d).
        # Either vec3_array or to_multiply must be transposed:
        # Can transpose vec3_array and then transpose the result:
        # (v,3).T -> (3,v); (d,3) @ (3,v) -> (d,v); (d,v).T -> (v,d)
        # Or transpose to_multiply and swap the order of multiplication:
        # (d,3).T -> (3,d); (v,3) @ (3,d) -> (v,d)
        # There's no, or negligible, performance difference between the two options, however, the result of the latter
        # will be C contiguous in memory, making it faster to convert to flattened bytes with .tobytes().
        multiplied_vectors = vec3_array @ to_multiply.T

    if to_add is not None:
        for axis, to_add_to_axis in zip(multiplied_vectors.T, to_add):
            if to_add_to_axis != 0:
                axis += to_add_to_axis

    # Cast to the desired return type before returning.
    return multiplied_vectors.astype(return_dtype, copy=False)


def vcos_transformed(raw_cos, m=None, dtype=None):
    return _mat4_vec3_array_multiply(m, raw_cos, dtype)


def nors_transformed(raw_nors, m=None, dtype=None):
    # Great, now normals are also expected 4D!
    # XXX Back to 3D normals for now!
    # return _mat4_vec3_array_multiply(m, raw_nors, dtype, return_4d=True)
    return _mat4_vec3_array_multiply(m, raw_nors, dtype)


def astype_view_signedness(arr, new_dtype):
    """Unsafely views arr as new_dtype if the itemsize and byteorder of arr matches but the signedness does not.

    Safely views arr as new_dtype if both arr and new_dtype have the same itemsize, byteorder and signedness, but could
    have a different character code, e.g. 'i' and 'l'. np.ndarray.astype with copy=False does not normally create this
    view, but Blender can be picky about the character code used, so this function will create the view.

    Otherwise, calls np.ndarray.astype with copy=False.

    The benefit of copy=False is that if the array can be safely viewed as the new type, then a view is made, instead of
    a copy with the new type.

    Unsigned types can't be viewed safely as signed or vice-versa, meaning that a copy would always be made by
    .astype(..., copy=False).

    This is intended for viewing uintc data (a common Blender C type with variable itemsize, though usually 4 bytes, so
    uint32) as int32 (a common FBX type), when the itemsizes match."""
    arr_dtype = arr.dtype

    if not isinstance(new_dtype, np.dtype):
        # new_dtype could be a type instance or a string, but it needs to be a dtype to compare its itemsize, byteorder
        # and kind.
        new_dtype = np.dtype(new_dtype)

    # For simplicity, only dtypes of the same itemsize and byteorder, but opposite signedness, are handled. Everything
    # else is left to .astype.
    arr_kind = arr_dtype.kind
    new_kind = new_dtype.kind
    # Signed and unsigned int are opposite in terms of signedness. Other types don't have signedness.
    integer_kinds = {'i', 'u'}
    if (
        arr_kind in integer_kinds and new_kind in integer_kinds
        and arr_dtype.itemsize == new_dtype.itemsize
        and arr_dtype.byteorder == new_dtype.byteorder
    ):
        # arr and new_dtype have signedness and matching itemsize and byteorder, so return a view of the new type.
        return arr.view(new_dtype)
    else:
        return arr.astype(new_dtype, copy=False)


def fast_first_axis_flat(ar):
    """Get a flat view (or a copy if a view is not possible) of the input array whereby each element is a single element
    of a dtype that is fast to sort, sorts according to individual bytes and contains the data for an entire row (and
    any further dimensions) of the input array.

    Since the dtype of the view could sort in a different order to the dtype of the input array, this isn't typically
    useful for actual sorting, but it is useful for sorting-based uniqueness, such as np.unique."""
    # If there are no rows, each element will be viewed as the new dtype.
    elements_per_row = math.prod(ar.shape[1:])
    row_itemsize = ar.itemsize * elements_per_row

    # Get a dtype with itemsize that equals row_itemsize.
    # Integer types sort the fastest, but are only available for specific itemsizes.
    uint_dtypes_by_itemsize = {1: np.uint8, 2: np.uint16, 4: np.uint32, 8: np.uint64}
    # Signed/unsigned makes no noticeable speed difference, but using unsigned will result in ordering according to
    # individual bytes like the other, non-integer types.
    if row_itemsize in uint_dtypes_by_itemsize:
        entire_row_dtype = uint_dtypes_by_itemsize[row_itemsize]
    else:
        # When using kind='stable' sorting, numpy only uses radix sort with integer types, but it's still
        # significantly faster to sort by a single item per row instead of multiple row elements or multiple structured
        # type fields.
        # Construct a flexible size dtype with matching itemsize.
        # Should always be 4 because each character in a unicode string is UCS4.
        str_itemsize = np.dtype((np.str_, 1)).itemsize
        if row_itemsize % str_itemsize == 0:
            # Unicode strings seem to be slightly faster to sort than bytes.
            entire_row_dtype = np.dtype((np.str_, row_itemsize // str_itemsize))
        else:
            # Bytes seem to be slightly faster to sort than raw bytes (np.void).
            entire_row_dtype = np.dtype((np.bytes_, row_itemsize))

    # View each element along the first axis as a single element.
    # View (or copy if a view is not possible) as flat
    ar = ar.reshape(-1)
    # To view as a dtype of different size, the last axis (entire array in NumPy 1.22 and earlier) must be C-contiguous.
    if row_itemsize != ar.itemsize and not ar.flags.c_contiguous:
        ar = np.ascontiguousarray(ar)
    return ar.view(entire_row_dtype)


def fast_first_axis_unique(ar, return_unique=True, return_index=False, return_inverse=False, return_counts=False):
    """np.unique with axis=0 but optimised for when the input array has multiple elements per row, and the returned
    unique array doesn't need to be sorted.

    Arrays with more than one element per row are more costly to sort in np.unique due to being compared one
    row-element at a time, like comparing tuples.

    By viewing each entire row as a single non-structured element, much faster sorting can be achieved. Since the values
    are viewed as a different type to their original, this means that the returned array of unique values may not be
    sorted according to their original type.

    The array of unique values can be excluded from the returned tuple by specifying return_unique=False.

    Float type caveats:
    All elements of -0.0 in the input array will be replaced with 0.0 to ensure that both values are collapsed into one.
    NaN values can have lots of different byte representations (e.g. signalling/quiet and custom payloads). Only the
    duplicates of each unique byte representation will be collapsed into one."""
    # At least something should always be returned.
    assert(return_unique or return_index or return_inverse or return_counts)
    # Only signed integer, unsigned integer and floating-point kinds of data are allowed. Other kinds of data have not
    # been tested.
    assert(ar.dtype.kind in "iuf")

    # Floating-point types have different byte representations for -0.0 and 0.0. Collapse them together by replacing all
    # -0.0 in the input array with 0.0.
    if ar.dtype.kind == 'f':
        ar[ar == -0.0] = 0.0

    # It's a bit annoying that the unique array is always calculated even when it might not be needed, but it is
    # generally insignificant compared to the cost of sorting.
    result = np.unique(fast_first_axis_flat(ar), return_index=return_index,
                       return_inverse=return_inverse, return_counts=return_counts)

    if return_unique:
        unique = result[0] if isinstance(result, tuple) else result
        # View in the original dtype.
        unique = unique.view(ar.dtype)
        # Return the same number of elements per row and any extra dimensions per row as the input array.
        unique.shape = (-1, *ar.shape[1:])
        if isinstance(result, tuple):
            return (unique,) + result[1:]
        else:
            return unique
    else:
        # Remove the first element, the unique array.
        result = result[1:]
        if len(result) == 1:
            # Unpack single element tuples.
            return result[0]
        else:
            return result


def ensure_object_not_in_edit_mode(context, obj):
    """Objects in Edit mode usually cannot be exported because much of the API used when exporting is not available for
    Objects in Edit mode.

    Exiting the currently active Object (and any other Objects opened in multi-editing) from Edit mode is simple and
    should be done with `bpy.ops.mesh.mode_set(mode='OBJECT')` instead of using this function.

    This function is for the rare case where an Object is in Edit mode, but the current context mode is not Edit mode.
    This can occur from a state where the current context mode is Edit mode, but then the active Object of the current
    View Layer is changed to a different Object that is not in Edit mode. This changes the current context mode, but
    leaves the other Object(s) in Edit mode.
    """
    if obj.mode != 'EDIT':
        return True

    # Get the active View Layer.
    view_layer = context.view_layer

    # A View Layer belongs to a scene.
    scene = view_layer.id_data

    # Get the current active Object of this View Layer, so we can restore it once done.
    orig_active = view_layer.objects.active

    # Check if obj is in the View Layer. If obj is not in the View Layer, it cannot be set as the active Object.
    # We don't use `obj.name in view_layer.objects` because an Object from a Library could have the same name.
    is_in_view_layer = any(o == obj for o in view_layer.objects)

    do_unlink_from_scene_collection = False
    try:
        if not is_in_view_layer:
            # There might not be any enabled collections in the View Layer, so link obj into the Scene Collection
            # instead, which is always available to all View Layers of that Scene.
            scene.collection.objects.link(obj)
            do_unlink_from_scene_collection = True
        view_layer.objects.active = obj

        # Now we're finally ready to attempt to change obj's mode.
        if bpy.ops.object.mode_set.poll():
            bpy.ops.object.mode_set(mode='OBJECT')
        if obj.mode == 'EDIT':
            # The Object could not be set out of EDIT mode and therefore cannot be exported.
            return False
    finally:
        # Always restore the original active Object and unlink obj from the Scene Collection if it had to be linked.
        view_layer.objects.active = orig_active
        if do_unlink_from_scene_collection:
            scene.collection.objects.unlink(obj)

    return True


def expand_shape_key_range(shape_key, value_to_fit):
    """Attempt to expand the slider_min/slider_max of a shape key to fit `value_to_fit` within the slider range,
    expanding slightly beyond `value_to_fit` if possible, so that the new slider_min/slider_max is not the same as
    `value_to_fit`. Blender has a hard minimum and maximum for slider values, so it may not be possible to fit the value
    within the slider range.

    If `value_to_fit` is already within the slider range, no changes are made.

    First tries setting slider_min/slider_max to double `value_to_fit`, otherwise, expands the range in the direction of
    `value_to_fit` by double the distance to `value_to_fit`.

    The new slider_min/slider_max is rounded down/up to the nearest whole number for a more visually pleasing result.

    Returns whether it was possible to expand the slider range to fit `value_to_fit`."""
    if value_to_fit < (slider_min := shape_key.slider_min):
        if value_to_fit < 0.0:
            # For the most common case, set slider_min to double value_to_fit.
            target_slider_min = value_to_fit * 2.0
        else:
            # Doubling value_to_fit would make it larger, so instead decrease slider_min by double the distance between
            # slider_min and value_to_fit.
            target_slider_min = slider_min - (slider_min - value_to_fit) * 2.0
        # Set slider_min to the first whole number less than or equal to target_slider_min.
        shape_key.slider_min = math.floor(target_slider_min)

        return value_to_fit >= SHAPE_KEY_SLIDER_HARD_MIN
    elif value_to_fit > (slider_max := shape_key.slider_max):
        if value_to_fit > 0.0:
            # For the most common case, set slider_max to double value_to_fit.
            target_slider_max = value_to_fit * 2.0
        else:
            # Doubling value_to_fit would make it smaller, so instead increase slider_max by double the distance between
            # slider_max and value_to_fit.
            target_slider_max = slider_max + (value_to_fit - slider_max) * 2.0
        # Set slider_max to the first whole number greater than or equal to target_slider_max.
        shape_key.slider_max = math.ceil(target_slider_max)

        return value_to_fit <= SHAPE_KEY_SLIDER_HARD_MAX
    else:
        # Value is already within the range.
        return True


# ##### Attribute utils. #####
AttributeDataTypeInfo = namedtuple("AttributeDataTypeInfo", ["dtype", "foreach_attribute", "item_size"])
_attribute_data_type_info_lookup = {
    'FLOAT': AttributeDataTypeInfo(np.single, "value", 1),
    'INT': AttributeDataTypeInfo(np.intc, "value", 1),
    'FLOAT_VECTOR': AttributeDataTypeInfo(np.single, "vector", 3),
    'FLOAT_COLOR': AttributeDataTypeInfo(np.single, "color", 4),  # color_srgb is an alternative
    'BYTE_COLOR': AttributeDataTypeInfo(np.single, "color", 4),  # color_srgb is an alternative
    'STRING': AttributeDataTypeInfo(None, "value", 1),  # Not usable with foreach_get/set
    'BOOLEAN': AttributeDataTypeInfo(bool, "value", 1),
    'FLOAT2': AttributeDataTypeInfo(np.single, "vector", 2),
    'INT8': AttributeDataTypeInfo(np.intc, "value", 1),
    'INT32_2D': AttributeDataTypeInfo(np.intc, "value", 2),
}


def attribute_get(attributes, name, data_type, domain):
    """Get an attribute by its name, data_type and domain.

    Returns None if no attribute with this name, data_type and domain exists."""
    attr = attributes.get(name)
    if not attr:
        return None
    if attr.data_type == data_type and attr.domain == domain:
        return attr
    # It shouldn't normally happen, but it's possible there are multiple attributes with the same name, but different
    # data_types or domains.
    for attr in attributes:
        if attr.name == name and attr.data_type == data_type and attr.domain == domain:
            return attr
    return None


def attribute_foreach_set(attribute, array_or_list, foreach_attribute=None):
    """Set every value of an attribute with foreach_set."""
    if foreach_attribute is None:
        foreach_attribute = _attribute_data_type_info_lookup[attribute.data_type].foreach_attribute
    attribute.data.foreach_set(foreach_attribute, array_or_list)


def attribute_to_ndarray(attribute, foreach_attribute=None):
    """Create a NumPy ndarray from an attribute."""
    data = attribute.data
    data_type_info = _attribute_data_type_info_lookup[attribute.data_type]
    ndarray = np.empty(len(data) * data_type_info.item_size, dtype=data_type_info.dtype)
    if foreach_attribute is None:
        foreach_attribute = data_type_info.foreach_attribute
    data.foreach_get(foreach_attribute, ndarray)
    return ndarray


@dataclass
class AttributeDescription:
    """Helper class to reduce duplicate code for handling built-in Blender attributes."""
    name: str
    # Valid identifiers can be found in bpy.types.Attribute.bl_rna.properties["data_type"].enum_items
    data_type: str
    # Valid identifiers can be found in bpy.types.Attribute.bl_rna.properties["domain"].enum_items
    domain: str
    # Some attributes are required to exist if certain conditions are met. If a required attribute does not exist when
    # attempting to get it, an AssertionError is raised.
    is_required_check: Callable[[bpy.types.AttributeGroup], bool] = None
    # NumPy dtype that matches the internal C data of this attribute.
    dtype: np.dtype = field(init=False)
    # The default attribute name to use with foreach_get and foreach_set.
    foreach_attribute: str = field(init=False)
    # The number of elements per value of the attribute when flattened into a 1-dimensional list/array.
    item_size: int = field(init=False)

    def __post_init__(self):
        data_type_info = _attribute_data_type_info_lookup[self.data_type]
        self.dtype = data_type_info.dtype
        self.foreach_attribute = data_type_info.foreach_attribute
        self.item_size = data_type_info.item_size

    def is_required(self, attributes):
        """Check if the attribute is required to exist in the provided attributes."""
        is_required_check = self.is_required_check
        return is_required_check and is_required_check(attributes)

    def get(self, attributes):
        """Get the attribute.

        If the attribute is required, but does not exist, an AssertionError is raised, otherwise None is returned."""
        attr = attribute_get(attributes, self.name, self.data_type, self.domain)
        if not attr and self.is_required(attributes):
            raise AssertionError("Required attribute '%s' with type '%s' and domain '%s' not found in %r"
                                 % (self.name, self.data_type, self.domain, attributes))
        return attr

    def ensure(self, attributes):
        """Get the attribute, creating it if it does not exist.

        Raises a RuntimeError if the attribute could not be created, which should only happen when attempting to create
        an attribute with a reserved name, but with the wrong data_type or domain. See usage of
        BuiltinCustomDataLayerProvider in Blender source for most reserved names.

        There is no guarantee that the returned attribute has the desired name because the name could already be in use
        by another attribute with a different data_type and/or domain."""
        attr = self.get(attributes)
        if attr:
            return attr

        attr = attributes.new(self.name, self.data_type, self.domain)
        if not attr:
            raise RuntimeError("Could not create attribute '%s' with type '%s' and domain '%s' in %r"
                               % (self.name, self.data_type, self.domain, attributes))
        return attr

    def foreach_set(self, attributes, array_or_list, foreach_attribute=None):
        """Get the attribute, creating it if it does not exist, and then set every value in the attribute."""
        attribute_foreach_set(self.ensure(attributes), array_or_list, foreach_attribute)

    def get_ndarray(self, attributes, foreach_attribute=None):
        """Get the attribute and if it exists, return a NumPy ndarray containing its data, otherwise return None."""
        attr = self.get(attributes)
        return attribute_to_ndarray(attr, foreach_attribute) if attr else None

    def to_ndarray(self, attributes, foreach_attribute=None):
        """Get the attribute and if it exists, return a NumPy ndarray containing its data, otherwise return a
        zero-length ndarray."""
        ndarray = self.get_ndarray(attributes, foreach_attribute)
        return ndarray if ndarray is not None else np.empty(0, dtype=self.dtype)


# Built-in Blender attributes
# Only attributes used by the importer/exporter are included here.
# See usage of BuiltinCustomDataLayerProvider in Blender source to find most built-in attributes.
MESH_ATTRIBUTE_MATERIAL_INDEX = AttributeDescription("material_index", 'INT', 'FACE')
MESH_ATTRIBUTE_POSITION = AttributeDescription("position", 'FLOAT_VECTOR', 'POINT',
                                               is_required_check=lambda attributes: bool(attributes.id_data.vertices))
MESH_ATTRIBUTE_SHARP_EDGE = AttributeDescription("sharp_edge", 'BOOLEAN', 'EDGE')
MESH_ATTRIBUTE_EDGE_VERTS = AttributeDescription(".edge_verts", 'INT32_2D', 'EDGE',
                                                 is_required_check=lambda attributes: bool(attributes.id_data.edges))
MESH_ATTRIBUTE_CORNER_VERT = AttributeDescription(".corner_vert", 'INT', 'CORNER',
                                                  is_required_check=lambda attributes: bool(attributes.id_data.loops))
MESH_ATTRIBUTE_CORNER_EDGE = AttributeDescription(".corner_edge", 'INT', 'CORNER',
                                                  is_required_check=lambda attributes: bool(attributes.id_data.loops))
MESH_ATTRIBUTE_SHARP_FACE = AttributeDescription("sharp_face", 'BOOLEAN', 'FACE')


# ##### UIDs code. #####

# ID class (mere int).
class UUID(int):
    pass


# UIDs storage.
_keys_to_uuids = {}
_uuids_to_keys = {}


def _key_to_uuid(uuids, key):
    # TODO: Check this is robust enough for our needs!
    # Note: We assume we have already checked the related key wasn't yet in _keys_to_uids!
    #       As int64 is signed in FBX, we keep uids below 2**63...
    if isinstance(key, int) and 0 <= key < 2**63:
        # We can use value directly as id!
        uuid = key
    else:
        uuid = hash(key)
        if uuid < 0:
            uuid = -uuid
        if uuid >= 2**63:
            uuid //= 2
    # Try to make our uid shorter!
    if uuid > int(1e9):
        t_uuid = uuid % int(1e9)
        if t_uuid not in uuids:
            uuid = t_uuid
    # Make sure our uuid *is* unique.
    if uuid in uuids:
        inc = 1 if uuid < 2**62 else -1
        while uuid in uuids:
            uuid += inc
            if 0 > uuid >= 2**63:
                # Note that this is more that unlikely, but does not harm anyway...
                raise ValueError("Unable to generate an UUID for key {}".format(key))
    return UUID(uuid)


def get_fbx_uuid_from_key(key):
    """
    Return an UUID for given key, which is assumed to be hashable.
    """
    uuid = _keys_to_uuids.get(key, None)
    if uuid is None:
        uuid = _key_to_uuid(_uuids_to_keys, key)
        _keys_to_uuids[key] = uuid
        _uuids_to_keys[uuid] = key
    return uuid


# XXX Not sure we'll actually need this one?
def get_key_from_fbx_uuid(uuid):
    """
    Return the key which generated this uid.
    """
    assert(uuid.__class__ == UUID)
    return _uuids_to_keys.get(uuid, None)


# Blender-specific key generators
def get_bid_name(bid):
    library = getattr(bid, "library", None)
    if library is not None:
        return "%s_L_%s" % (bid.name, library.name)
    else:
        return bid.name


def get_blenderID_key(bid):
    if isinstance(bid, Iterable):
        return "|".join("B" + e.rna_type.name + "#" + get_bid_name(e) for e in bid)
    else:
        return "B" + bid.rna_type.name + "#" + get_bid_name(bid)


def get_blenderID_name(bid):
    if isinstance(bid, Iterable):
        return "|".join(get_bid_name(e) for e in bid)
    else:
        return get_bid_name(bid)


def get_blender_empty_key(obj):
    """Return bone's keys (Model and NodeAttribute)."""
    return "|".join((get_blenderID_key(obj), "Empty"))


def get_blender_mesh_shape_key(me):
    """Return main shape deformer's key."""
    return "|".join((get_blenderID_key(me), "Shape"))


def get_blender_mesh_shape_channel_key(me, shape):
    """Return shape channel and geometry shape keys."""
    return ("|".join((get_blenderID_key(me), "Shape", get_blenderID_key(shape))),
            "|".join((get_blenderID_key(me), "Geometry", get_blenderID_key(shape))))


def get_blender_bone_key(armature, bone):
    """Return bone's keys (Model and NodeAttribute)."""
    return "|".join((get_blenderID_key((armature, bone)), "Data"))


def get_blender_bindpose_key(obj, mesh):
    """Return object's bindpose key."""
    return "|".join((get_blenderID_key(obj), get_blenderID_key(mesh), "BindPose"))


def get_blender_armature_skin_key(armature, mesh):
    """Return armature's skin key."""
    return "|".join((get_blenderID_key(armature), get_blenderID_key(mesh), "DeformerSkin"))


def get_blender_bone_cluster_key(armature, mesh, bone):
    """Return bone's cluster key."""
    return "|".join((get_blenderID_key(armature), get_blenderID_key(mesh),
                     get_blenderID_key(bone), "SubDeformerCluster"))


def get_blender_anim_id_base(scene, ref_id):
    if ref_id is not None:
        return get_blenderID_key(scene) + "|" + get_blenderID_key(ref_id)
    else:
        return get_blenderID_key(scene)


def get_blender_anim_stack_key(scene, ref_id):
    """Return single anim stack key."""
    return get_blender_anim_id_base(scene, ref_id) + "|AnimStack"


def get_blender_anim_layer_key(scene, ref_id):
    """Return ID's anim layer key."""
    return get_blender_anim_id_base(scene, ref_id) + "|AnimLayer"


def get_blender_anim_curve_node_key(scene, ref_id, obj_key, fbx_prop_name):
    """Return (stack/layer, ID, fbxprop) curve node key."""
    return "|".join((get_blender_anim_id_base(scene, ref_id), obj_key, fbx_prop_name, "AnimCurveNode"))


def get_blender_anim_curve_key(scene, ref_id, obj_key, fbx_prop_name, fbx_prop_item_name):
    """Return (stack/layer, ID, fbxprop, item) curve key."""
    return "|".join((get_blender_anim_id_base(scene, ref_id), obj_key, fbx_prop_name,
                     fbx_prop_item_name, "AnimCurve"))


def get_blender_nodetexture_key(ma, socket_names):
    return "|".join((get_blenderID_key(ma), *socket_names))


# ##### Element generators. #####

# Note: elem may be None, in this case the element is not added to any parent.
def elem_empty(elem, name):
    sub_elem = encode_bin.FBXElem(name)
    if elem is not None:
        elem.elems.append(sub_elem)
    return sub_elem


def _elem_data_single(elem, name, value, func_name):
    sub_elem = elem_empty(elem, name)
    getattr(sub_elem, func_name)(value)
    return sub_elem


def _elem_data_vec(elem, name, value, func_name):
    sub_elem = elem_empty(elem, name)
    func = getattr(sub_elem, func_name)
    for v in value:
        func(v)
    return sub_elem


def elem_data_single_bool(elem, name, value):
    return _elem_data_single(elem, name, value, "add_bool")


def elem_data_single_char(elem, name, value):
    return _elem_data_single(elem, name, value, "add_char")


def elem_data_single_int8(elem, name, value):
    return _elem_data_single(elem, name, value, "add_int8")


def elem_data_single_int16(elem, name, value):
    return _elem_data_single(elem, name, value, "add_int16")


def elem_data_single_int32(elem, name, value):
    return _elem_data_single(elem, name, value, "add_int32")


def elem_data_single_int64(elem, name, value):
    return _elem_data_single(elem, name, value, "add_int64")


def elem_data_single_float32(elem, name, value):
    return _elem_data_single(elem, name, value, "add_float32")


def elem_data_single_float64(elem, name, value):
    return _elem_data_single(elem, name, value, "add_float64")


def elem_data_single_bytes(elem, name, value):
    return _elem_data_single(elem, name, value, "add_bytes")


def elem_data_single_string(elem, name, value):
    return _elem_data_single(elem, name, value, "add_string")


def elem_data_single_string_unicode(elem, name, value):
    return _elem_data_single(elem, name, value, "add_string_unicode")


def elem_data_single_bool_array(elem, name, value):
    return _elem_data_single(elem, name, value, "add_bool_array")


def elem_data_single_int32_array(elem, name, value):
    return _elem_data_single(elem, name, value, "add_int32_array")


def elem_data_single_int64_array(elem, name, value):
    return _elem_data_single(elem, name, value, "add_int64_array")


def elem_data_single_float32_array(elem, name, value):
    return _elem_data_single(elem, name, value, "add_float32_array")


def elem_data_single_float64_array(elem, name, value):
    return _elem_data_single(elem, name, value, "add_float64_array")


def elem_data_single_byte_array(elem, name, value):
    return _elem_data_single(elem, name, value, "add_byte_array")


def elem_data_vec_float64(elem, name, value):
    return _elem_data_vec(elem, name, value, "add_float64")


# ##### Generators for standard FBXProperties70 properties. #####

def elem_properties(elem):
    return elem_empty(elem, b"Properties70")


# Properties definitions, format: (b"type_1", b"label(???)", "name_set_value_1", "name_set_value_2", ...)
# XXX Looks like there can be various variations of formats here... Will have to be checked ultimately!
#     Also, those "custom" types like 'FieldOfView' or 'Lcl Translation' are pure nonsense,
#     these are just Vector3D ultimately... *sigh* (again).
FBX_PROPERTIES_DEFINITIONS = {
    # Generic types.
    "p_bool": (b"bool", b"", "add_int32"),  # Yes, int32 for a bool (and they do have a core bool type)!!!
    "p_integer": (b"int", b"Integer", "add_int32"),
    "p_ulonglong": (b"ULongLong", b"", "add_int64"),
    "p_double": (b"double", b"Number", "add_float64"),  # Non-animatable?
    "p_number": (b"Number", b"", "add_float64"),  # Animatable-only?
    "p_enum": (b"enum", b"", "add_int32"),
    "p_vector_3d": (b"Vector3D", b"Vector", "add_float64", "add_float64", "add_float64"),  # Non-animatable?
    "p_vector": (b"Vector", b"", "add_float64", "add_float64", "add_float64"),  # Animatable-only?
    "p_color_rgb": (b"ColorRGB", b"Color", "add_float64", "add_float64", "add_float64"),  # Non-animatable?
    "p_color": (b"Color", b"", "add_float64", "add_float64", "add_float64"),  # Animatable-only?
    "p_string": (b"KString", b"", "add_string_unicode"),
    "p_string_url": (b"KString", b"Url", "add_string_unicode"),
    "p_timestamp": (b"KTime", b"Time", "add_int64"),
    "p_datetime": (b"DateTime", b"", "add_string_unicode"),
    # Special types.
    "p_object": (b"object", b""),  # XXX Check this! No value for this prop??? Would really like to know how it works!
    "p_compound": (b"Compound", b""),
    # Specific types (sic).
    # ## Objects (Models).
    "p_lcl_translation": (b"Lcl Translation", b"", "add_float64", "add_float64", "add_float64"),
    "p_lcl_rotation": (b"Lcl Rotation", b"", "add_float64", "add_float64", "add_float64"),
    "p_lcl_scaling": (b"Lcl Scaling", b"", "add_float64", "add_float64", "add_float64"),
    "p_visibility": (b"Visibility", b"", "add_float64"),
    "p_visibility_inheritance": (b"Visibility Inheritance", b"", "add_int32"),
    # ## Cameras!!!
    "p_roll": (b"Roll", b"", "add_float64"),
    "p_opticalcenterx": (b"OpticalCenterX", b"", "add_float64"),
    "p_opticalcentery": (b"OpticalCenterY", b"", "add_float64"),
    "p_fov": (b"FieldOfView", b"", "add_float64"),
    "p_fov_x": (b"FieldOfViewX", b"", "add_float64"),
    "p_fov_y": (b"FieldOfViewY", b"", "add_float64"),
}


def _elem_props_set(elem, ptype, name, value, flags):
    p = elem_data_single_string(elem, b"P", name)
    for t in ptype[:2]:
        p.add_string(t)
    p.add_string(flags)
    if len(ptype) == 3:
        getattr(p, ptype[2])(value)
    elif len(ptype) > 3:
        # We assume value is iterable, else it's a bug!
        for callback, val in zip(ptype[2:], value):
            getattr(p, callback)(val)


def _elem_props_flags(animatable, animated, custom):
    # XXX: There are way more flags, see
    #      http://help.autodesk.com/view/FBX/2015/ENU/?guid=__cpp_ref_class_fbx_property_flags_html
    #      Unfortunately, as usual, no doc at all about their 'translation' in actual FBX file format.
    #      Curse you-know-who.
    if animatable:
        if animated:
            if custom:
                return b"A+U"
            return b"A+"
        if custom:
            # Seems that customprops always need those 'flags', see T69554. Go figure...
            return b"A+U"
        return b"A"
    if custom:
        # Seems that customprops always need those 'flags', see T69554. Go figure...
        return b"A+U"
    return b""


def elem_props_set(elem, ptype, name, value=None, animatable=False, animated=False, custom=False):
    ptype = FBX_PROPERTIES_DEFINITIONS[ptype]
    _elem_props_set(elem, ptype, name, value, _elem_props_flags(animatable, animated, custom))


def elem_props_compound(elem, cmpd_name, custom=False):
    def _setter(ptype, name, value, animatable=False, animated=False, custom=False):
        name = cmpd_name + b"|" + name
        elem_props_set(elem, ptype, name, value, animatable=animatable, animated=animated, custom=custom)

    elem_props_set(elem, "p_compound", cmpd_name, custom=custom)
    return _setter


def elem_props_template_init(templates, template_type):
    """
    Init a writing template of given type, for *one* element's properties.
    """
    ret = {}
    tmpl = templates.get(template_type)
    if tmpl is not None:
        written = tmpl.written[0]
        props = tmpl.properties
        ret = {name: [val, ptype, anim, written] for name, (val, ptype, anim) in props.items()}
    return ret


def elem_props_template_set(template, elem, ptype_name, name, value, animatable=False, animated=False):
    """
    Only add a prop if the same value is not already defined in given template.
    Note it is important to not give iterators as value, here!
    """
    ptype = FBX_PROPERTIES_DEFINITIONS[ptype_name]
    if len(ptype) > 3:
        value = tuple(value)
    tmpl_val, tmpl_ptype, tmpl_animatable, tmpl_written = template.get(name, (None, None, False, False))
    # Note animatable flag from template takes precedence over given one, if applicable.
    # However, animated properties are always written, since they cannot match their template!
    if tmpl_ptype is not None and not animated:
        if (tmpl_written and
            ((len(ptype) == 3 and (tmpl_val, tmpl_ptype) == (value, ptype_name)) or
             (len(ptype) > 3 and (tuple(tmpl_val), tmpl_ptype) == (value, ptype_name)))):
            return  # Already in template and same value.
        _elem_props_set(elem, ptype, name, value, _elem_props_flags(tmpl_animatable, animated, False))
        template[name][3] = True
    else:
        _elem_props_set(elem, ptype, name, value, _elem_props_flags(animatable, animated, False))


def elem_props_template_finalize(template, elem):
    """
    Finalize one element's template/props.
    Issue is, some templates might be "needed" by different types (e.g. NodeAttribute is for lights, cameras, etc.),
    but values for only *one* subtype can be written as template. So we have to be sure we write those for the other
    subtypes in each and every elements, if they are not overridden by that element.
    Yes, hairy, FBX that is to say. When they could easily support several subtypes per template... :(
    """
    for name, (value, ptype_name, animatable, written) in template.items():
        if written:
            continue
        ptype = FBX_PROPERTIES_DEFINITIONS[ptype_name]
        _elem_props_set(elem, ptype, name, value, _elem_props_flags(animatable, False, False))


# ##### Templates #####
# TODO: check all those "default" values, they should match Blender's default as much as possible, I guess?

FBXTemplate = namedtuple("FBXTemplate", ("type_name", "prop_type_name", "properties", "nbr_users", "written"))


def fbx_templates_generate(root, fbx_templates):
    # We may have to gather different templates in the same node (e.g. NodeAttribute template gathers properties
    # for Lights, Cameras, LibNodes, etc.).
    ref_templates = {(tmpl.type_name, tmpl.prop_type_name): tmpl for tmpl in fbx_templates.values()}

    templates = {}
    for type_name, prop_type_name, properties, nbr_users, _written in fbx_templates.values():
        tmpl = templates.setdefault(type_name, [{}, 0])
        tmpl[0][prop_type_name] = (properties, nbr_users)
        tmpl[1] += nbr_users

    for type_name, (subprops, nbr_users) in templates.items():
        template = elem_data_single_string(root, b"ObjectType", type_name)
        elem_data_single_int32(template, b"Count", nbr_users)

        if len(subprops) == 1:
            prop_type_name, (properties, _nbr_sub_type_users) = next(iter(subprops.items()))
            subprops = (prop_type_name, properties)
            ref_templates[(type_name, prop_type_name)].written[0] = True
        else:
            # Ack! Even though this could/should work, looks like it is not supported. So we have to chose one. :|
            max_users = max_props = -1
            written_prop_type_name = None
            for prop_type_name, (properties, nbr_sub_type_users) in subprops.items():
                if nbr_sub_type_users > max_users or (nbr_sub_type_users == max_users and len(properties) > max_props):
                    max_users = nbr_sub_type_users
                    max_props = len(properties)
                    written_prop_type_name = prop_type_name
            subprops = (written_prop_type_name, properties)
            ref_templates[(type_name, written_prop_type_name)].written[0] = True

        prop_type_name, properties = subprops
        if prop_type_name and properties:
            elem = elem_data_single_string(template, b"PropertyTemplate", prop_type_name)
            props = elem_properties(elem)
            for name, (value, ptype, animatable) in properties.items():
                try:
                    elem_props_set(props, ptype, name, value, animatable=animatable)
                except Exception as e:
                    print("Failed to write template prop (%r)" % e)
                    print(props, ptype, name, value, animatable)


# ##### FBX animation helpers. #####


class AnimationCurveNodeWrapper:
    """
    This class provides a same common interface for all (FBX-wise) AnimationCurveNode and AnimationCurve elements,
    and easy API to handle those.
    """
    __slots__ = (
        'elem_keys', 'default_values', 'fbx_group', 'fbx_gname', 'fbx_props',
        'force_keying', 'force_startend_keying',
        '_frame_times_array', '_frame_values_array', '_frame_write_mask_array',
    )

    kinds = {
        'LCL_TRANSLATION': ("Lcl Translation", "T", ("X", "Y", "Z")),
        'LCL_ROTATION': ("Lcl Rotation", "R", ("X", "Y", "Z")),
        'LCL_SCALING': ("Lcl Scaling", "S", ("X", "Y", "Z")),
        'SHAPE_KEY': ("DeformPercent", "DeformPercent", ("DeformPercent",)),
        'CAMERA_FOCAL': ("FocalLength", "FocalLength", ("FocalLength",)),
        'CAMERA_FOCUS_DISTANCE': ("FocusDistance", "FocusDistance", ("FocusDistance",)),
    }

    def __init__(self, elem_key, kind, force_keying, force_startend_keying, default_values=...):
        self.elem_keys = [elem_key]
        assert(kind in self.kinds)
        self.fbx_group = [self.kinds[kind][0]]
        self.fbx_gname = [self.kinds[kind][1]]
        self.fbx_props = [self.kinds[kind][2]]
        self.force_keying = force_keying
        self.force_startend_keying = force_startend_keying
        self._frame_times_array = None
        self._frame_values_array = None
        self._frame_write_mask_array = None
        if default_values is not ...:
            assert(len(default_values) == len(self.fbx_props[0]))
            self.default_values = default_values
        else:
            self.default_values = (0.0) * len(self.fbx_props[0])

    def __bool__(self):
        # We are 'True' if we do have some validated keyframes...
        return self._frame_write_mask_array is not None and bool(np.any(self._frame_write_mask_array))

    def add_group(self, elem_key, fbx_group, fbx_gname, fbx_props):
        """
        Add another whole group stuff (curvenode, animated item/prop + curvnode/curve identifiers).
        E.g. Shapes animations is written twice, houra!
        """
        assert(len(fbx_props) == len(self.fbx_props[0]))
        self.elem_keys.append(elem_key)
        self.fbx_group.append(fbx_group)
        self.fbx_gname.append(fbx_gname)
        self.fbx_props.append(fbx_props)

    def set_keyframes(self, keyframe_times, keyframe_values):
        """
        Set all keyframe times and values of the group.
        Values can be a 2D array where each row is the values for a separate curve.
        """
        # View 1D keyframe_values as 2D with a single row, so that the same code can be used for both 1D and
        # 2D inputs.
        if len(keyframe_values.shape) == 1:
            keyframe_values = keyframe_values[np.newaxis]
        # There must be a time for each column of values.
        assert(len(keyframe_times) == keyframe_values.shape[1])
        # There must be as many rows of values as there are properties.
        assert(len(self.fbx_props[0]) == len(keyframe_values))
        write_mask = np.full_like(keyframe_values, True, dtype=bool)  # write everything by default
        self._frame_times_array = keyframe_times
        self._frame_values_array = keyframe_values
        self._frame_write_mask_array = write_mask

    def simplify(self, fac, step, force_keep=False):
        """
        Simplifies sampled curves by only enabling samples when:
            * their values relatively differ from the previous sample ones.
        """
        if self._frame_times_array is None:
            # Keyframes have not been added yet.
            return

        if fac == 0.0:
            return

        # So that, with default factor and step values (1), we get:
        min_reldiff_fac = fac * 1.0e-3  # min relative value evolution: 0.1% of current 'order of magnitude'.
        min_absdiff_fac = 0.1  # A tenth of reldiff...

        # Initialise to no values enabled for writing.
        self._frame_write_mask_array[:] = False

        # Values are enabled for writing if they differ enough from either of their adjacent values or if they differ
        # enough from the closest previous value that is enabled due to either of these conditions.
        for sampled_values, enabled_mask in zip(self._frame_values_array, self._frame_write_mask_array):
            # Create overlapping views of the 'previous' (all but the last) and 'current' (all but the first)
            # `sampled_values` and `enabled_mask`.
            # Calculate absolute values from `sampled_values` so that the 'previous' and 'current' absolute arrays can
            # be views into the same array instead of separately calculated arrays.
            abs_sampled_values = np.abs(sampled_values)
            # 'previous' views.
            p_val_view = sampled_values[:-1]
            p_abs_val_view = abs_sampled_values[:-1]
            p_enabled_mask_view = enabled_mask[:-1]
            # 'current' views.
            c_val_view = sampled_values[1:]
            c_abs_val_view = abs_sampled_values[1:]
            c_enabled_mask_view = enabled_mask[1:]

            # If enough difference from previous sampled value, enable the current value *and* the previous one!
            # The difference check is symmetrical, so this will compare each value to both of its adjacent values.
            # Unless it is forcefully enabled later, this is the only way that the first value can be enabled.
            # This is a contracted form of relative + absolute-near-zero difference:
            # def is_different(a, b):
            #     abs_diff = abs(a - b)
            #     if abs_diff < min_reldiff_fac * min_absdiff_fac:
            #         return False
            #     return (abs_diff / ((abs(a) + abs(b)) / 2)) > min_reldiff_fac
            # Note that we ignore the '/ 2' part here, since it's not much significant for us.
            # Contracted form using only builtin Python functions:
            #     return abs(a - b) > (min_reldiff_fac * max(abs(a) + abs(b), min_absdiff_fac))
            abs_diff = np.abs(c_val_view - p_val_view)
            different_if_greater_than = min_reldiff_fac * np.maximum(c_abs_val_view + p_abs_val_view, min_absdiff_fac)
            enough_diff_p_val_mask = abs_diff > different_if_greater_than
            # Enable both the current values *and* the previous values where `enough_diff_p_val_mask` is True. Some
            # values may get set to True twice because the views overlap, but this is not a problem.
            p_enabled_mask_view[enough_diff_p_val_mask] = True
            c_enabled_mask_view[enough_diff_p_val_mask] = True

            # Else, if enough difference from previous enabled value, enable the current value only!
            # For each 'current' value, get the index of the nearest previous enabled value in `sampled_values` (or
            # itself if the value is enabled).
            # Start with an array that is the index of the 'current' value in `sampled_values`. The 'current' values are
            # all but the first value, so the indices will be from 1 to `len(sampled_values)` exclusive.
            # Let len(sampled_values) == 9:
            #   [1, 2, 3, 4, 5, 6, 7, 8]
            p_enabled_idx_in_sampled_values = np.arange(1, len(sampled_values))
            # Replace the indices of all disabled values with 0 in preparation of filling them in with the index of the
            # nearest previous enabled value. We choose to replace with 0 so that if there is no nearest previous
            # enabled value, we instead default to `sampled_values[0]`.
            c_val_disabled_mask = ~c_enabled_mask_view
            # Let `c_val_disabled_mask` be:
            #   [F, F, T, F, F, T, T, T]
            # Set indices to 0 where `c_val_disabled_mask` is True:
            #   [1, 2, 3, 4, 5, 6, 7, 8]
            #          v        v  v  v
            #   [1, 2, 0, 4, 5, 0, 0, 0]
            p_enabled_idx_in_sampled_values[c_val_disabled_mask] = 0
            # Accumulative maximum travels across the array from left to right, filling in the zeroed indices with the
            # maximum value so far, which will be the closest previous enabled index because the non-zero indices are
            # strictly increasing.
            #   [1, 2, 0, 4, 5, 0, 0, 0]
            #          v        v  v  v
            #   [1, 2, 2, 4, 5, 5, 5, 5]
            p_enabled_idx_in_sampled_values = np.maximum.accumulate(p_enabled_idx_in_sampled_values)
            # Only disabled values need to be checked against their nearest previous enabled values.
            # We can additionally ignore all values which equal their immediately previous value because those values
            # will never be enabled if they were not enabled by the earlier difference check against immediately
            # previous values.
            p_enabled_diff_to_check_mask = np.logical_and(c_val_disabled_mask, p_val_view != c_val_view)
            # Convert from a mask to indices because we need the indices later and because the array of indices will
            # usually be smaller than the mask array making it faster to index other arrays with.
            p_enabled_diff_to_check_idx = np.flatnonzero(p_enabled_diff_to_check_mask)
            # `p_enabled_idx_in_sampled_values` from earlier:
            #   [1, 2, 2, 4, 5, 5, 5, 5]
            # `p_enabled_diff_to_check_mask` assuming no values equal their immediately previous value:
            #   [F, F, T, F, F, T, T, T]
            # `p_enabled_diff_to_check_idx`:
            #   [      2,       5, 6, 7]
            # `p_enabled_idx_in_sampled_values_to_check`:
            #   [      2,       5, 5, 5]
            p_enabled_idx_in_sampled_values_to_check = p_enabled_idx_in_sampled_values[p_enabled_diff_to_check_idx]
            # Get the 'current' disabled values that need to be checked.
            c_val_to_check = c_val_view[p_enabled_diff_to_check_idx]
            c_abs_val_to_check = c_abs_val_view[p_enabled_diff_to_check_idx]
            # Get the nearest previous enabled value for each value to be checked.
            nearest_p_enabled_val = sampled_values[p_enabled_idx_in_sampled_values_to_check]
            abs_nearest_p_enabled_val = np.abs(nearest_p_enabled_val)
            # Check the relative + absolute-near-zero difference again, but against the nearest previous enabled value
            # this time.
            abs_diff = np.abs(c_val_to_check - nearest_p_enabled_val)
            different_if_greater_than = (min_reldiff_fac
                                         * np.maximum(c_abs_val_to_check + abs_nearest_p_enabled_val, min_absdiff_fac))
            enough_diff_p_enabled_val_mask = abs_diff > different_if_greater_than
            # If there are any that are different enough from the previous enabled value, then we have to check them all
            # iteratively because enabling a new value can change the nearest previous enabled value of some elements,
            # which changes their relative + absolute-near-zero difference:
            # `p_enabled_diff_to_check_idx`:
            #   [2, 5, 6, 7]
            # `p_enabled_idx_in_sampled_values_to_check`:
            #   [2, 5, 5, 5]
            # Let `enough_diff_p_enabled_val_mask` be:
            #   [F, F, T, T]
            # The first index that is newly enabled is 6:
            #   [2, 5,>6<,5]
            # But 6 > 5, so the next value's nearest previous enabled index is also affected:
            #   [2, 5, 6,>6<]
            # We had calculated a newly enabled index of 7 too, but that was calculated against the old nearest previous
            # enabled index of 5, which has now been updated to 6, so whether 7 is enabled or not needs to be
            # recalculated:
            #   [F, F, T, ?]
            if np.any(enough_diff_p_enabled_val_mask):
                # Accessing .data, the memoryview of the array, iteratively or by individual index is faster than doing
                # the same with the array itself.
                zipped = zip(p_enabled_diff_to_check_idx.data,
                             c_val_to_check.data,
                             c_abs_val_to_check.data,
                             p_enabled_idx_in_sampled_values_to_check.data,
                             enough_diff_p_enabled_val_mask.data)
                # While iterating, we could set updated values into `enough_diff_p_enabled_val_mask` as we go and then
                # update `enabled_mask` in bulk after the iteration, but if we're going to update an array while
                # iterating, we may as well update `enabled_mask` directly instead and skip the bulk update.
                # Additionally, the number of `True` writes to `enabled_mask` is usually much less than the number of
                # updates that would be required to `enough_diff_p_enabled_val_mask`.
                c_enabled_mask_view_mv = c_enabled_mask_view.data

                # While iterating, keep track of the most recent newly enabled index, so we can tell when we need to
                # recalculate whether the current value needs to be enabled.
                new_p_enabled_idx = -1
                # Keep track of its value too for performance.
                new_p_enabled_val = -1
                new_abs_p_enabled_val = -1
                for cur_idx, c_val, c_abs_val, old_p_enabled_idx, enough_diff in zipped:
                    if new_p_enabled_idx > old_p_enabled_idx:
                        # The nearest previous enabled value is newly enabled and was not included when
                        # `enough_diff_p_enabled_val_mask` was calculated, so whether the current value is different
                        # enough needs to be recalculated using the newly enabled value.
                        # Check if the relative + absolute-near-zero difference is enough to enable this value.
                        enough_diff = (abs(c_val - new_p_enabled_val)
                                       > (min_reldiff_fac * max(c_abs_val + new_abs_p_enabled_val, min_absdiff_fac)))
                    if enough_diff:
                        # The current value needs to be enabled.
                        c_enabled_mask_view_mv[cur_idx] = True
                        # Update the index and values for this newly enabled value.
                        new_p_enabled_idx = cur_idx
                        new_p_enabled_val = c_val
                        new_abs_p_enabled_val = c_abs_val

        # If we write nothing (action doing nothing) and are in 'force_keep' mode, we key everything! :P
        # See T41766.
        # Also, it seems some importers (e.g. UE4) do not handle correctly armatures where some bones
        # are not animated, but are children of animated ones, so added an option to systematically force writing
        # one key in this case.
        # See T41719, T41605, T41254...
        if self.force_keying or (force_keep and not self):
            are_keyed = [True] * len(self._frame_write_mask_array)
        else:
            are_keyed = np.any(self._frame_write_mask_array, axis=1)

        # If we did key something, ensure first and last sampled values are keyed as well.
        if self.force_startend_keying:
            for is_keyed, frame_write_mask in zip(are_keyed, self._frame_write_mask_array):
                if is_keyed:
                    frame_write_mask[:1] = True
                    frame_write_mask[-1:] = True

    def get_final_data(self, scene, ref_id, force_keep=False):
        """
        Yield final anim data for this 'curvenode' (for all curvenodes defined).
        force_keep is to force to keep a curve even if it only has one valid keyframe.
        """
        curves = [
            (self._frame_times_array[write_mask], values[write_mask])
            for values, write_mask in zip(self._frame_values_array, self._frame_write_mask_array)
        ]

        force_keep = force_keep or self.force_keying
        for elem_key, fbx_group, fbx_gname, fbx_props in \
                zip(self.elem_keys, self.fbx_group, self.fbx_gname, self.fbx_props):
            group_key = get_blender_anim_curve_node_key(scene, ref_id, elem_key, fbx_group)
            group = {}
            for c, def_val, fbx_item in zip(curves, self.default_values, fbx_props):
                fbx_item = FBX_ANIM_PROPSGROUP_NAME + "|" + fbx_item
                curve_key = get_blender_anim_curve_key(scene, ref_id, elem_key, fbx_group, fbx_item)
                # (curve key, default value, keyframes, write flag).
                times = c[0]
                write_flag = len(times) > (0 if force_keep else 1)
                group[fbx_item] = (curve_key, def_val, c, write_flag)
            yield elem_key, group_key, group, fbx_group, fbx_gname


# ##### FBX objects generators. #####

# FBX Model-like data (i.e. Blender objects, depsgraph instances and bones) are wrapped in ObjectWrapper.
# This allows us to have a (nearly) same code FBX-wise for all those types.
# The wrapper tries to stay as small as possible, by mostly using callbacks (property(get...))
# to actual Blender data it contains.
# Note it caches its instances, so that you may call several times ObjectWrapper(your_object)
# with a minimal cost (just re-computing the key).

class MetaObjectWrapper(type):
    def __call__(cls, bdata, armature=None):
        if bdata is None:
            return None
        dup_mat = None
        if isinstance(bdata, Object):
            key = get_blenderID_key(bdata)
        elif isinstance(bdata, DepsgraphObjectInstance):
            if bdata.is_instance:
                key = "|".join((get_blenderID_key((bdata.parent.original, bdata.instance_object.original)),
                                cls._get_dup_num_id(bdata)))
                dup_mat = bdata.matrix_world.copy()
            else:
                key = get_blenderID_key(bdata.object.original)
        else:  # isinstance(bdata, (Bone, PoseBone)):
            if isinstance(bdata, PoseBone):
                bdata = armature.data.bones[bdata.name]
            key = get_blenderID_key((armature, bdata))

        cache = getattr(cls, "_cache", None)
        if cache is None:
            cache = cls._cache = {}
        instance = cache.get(key)
        if instance is not None:
            # Duplis hack: since dupli instances are not persistent in Blender (we have to re-create them to get updated
            # info like matrix...), we *always* need to reset that matrix when calling ObjectWrapper() (all
            # other data is supposed valid during whole cache live span, so we can skip resetting it).
            instance._dupli_matrix = dup_mat
            return instance

        instance = cls.__new__(cls, bdata, armature)
        instance.__init__(bdata, armature)
        instance.key = key
        instance._dupli_matrix = dup_mat
        cache[key] = instance
        return instance


class ObjectWrapper(metaclass=MetaObjectWrapper):
    """
    This class provides a same common interface for all (FBX-wise) object-like elements:
    * Blender Object
    * Blender Bone and PoseBone
    * Blender DepsgraphObjectInstance (for dulis).
    Note since a same Blender object might be 'mapped' to several FBX models (esp. with duplis),
    we need to use a key to identify each.
    """
    __slots__ = (
        'name', 'key', 'bdata', 'parented_to_armature', 'override_materials',
        '_tag', '_ref', '_dupli_matrix'
    )

    @classmethod
    def cache_clear(cls):
        if hasattr(cls, "_cache"):
            del cls._cache

    @staticmethod
    def _get_dup_num_id(bdata):
        INVALID_IDS = {2147483647, 0}
        pids = tuple(bdata.persistent_id)
        idx_valid = 0
        prev_i = ...
        for idx, i in enumerate(pids[::-1]):
            if i not in INVALID_IDS or (idx == len(pids) and i == 0 and prev_i != 0):
                idx_valid = len(pids) - idx
                break
            prev_i = i
        return ".".join(str(i) for i in pids[:idx_valid])

    def __init__(self, bdata, armature=None):
        """
        bdata might be an Object (deprecated), DepsgraphObjectInstance, Bone or PoseBone.
        If Bone or PoseBone, armature Object must be provided.
        """
        # Note: DepsgraphObjectInstance are purely runtime data, they become invalid as soon as we step to the next item!
        #       Hence we have to immediately copy *all* needed data...
        if isinstance(bdata, Object):  # DEPRECATED
            self._tag = 'OB'
            self.name = get_blenderID_name(bdata)
            self.bdata = bdata
            self._ref = None
        elif isinstance(bdata, DepsgraphObjectInstance):
            if bdata.is_instance:
                # Note that dupli instance matrix is set by meta-class initialization.
                self._tag = 'DP'
                self.name = "|".join((get_blenderID_name((bdata.parent.original, bdata.instance_object.original)),
                                      "Dupli", self._get_dup_num_id(bdata)))
                self.bdata = bdata.instance_object.original
                self._ref = bdata.parent.original
            else:
                self._tag = 'OB'
                self.name = get_blenderID_name(bdata)
                self.bdata = bdata.object.original
                self._ref = None
        else:  # isinstance(bdata, (Bone, PoseBone)):
            if isinstance(bdata, PoseBone):
                bdata = armature.data.bones[bdata.name]
            self._tag = 'BO'
            self.name = get_blenderID_name(bdata)
            self.bdata = bdata
            self._ref = armature
        self.parented_to_armature = False
        self.override_materials = None

    def __eq__(self, other):
        return isinstance(other, self.__class__) and self.key == other.key

    def __hash__(self):
        return hash(self.key)

    def __repr__(self):
        return self.key

    # #### Common to all _tag values.
    def get_fbx_uuid(self):
        return get_fbx_uuid_from_key(self.key)
    fbx_uuid = property(get_fbx_uuid)

    # XXX Not sure how much that’s useful now... :/
    def get_hide(self):
        return self.bdata.hide_viewport if self._tag in {'OB', 'DP'} else self.bdata.hide
    hide = property(get_hide)

    def get_parent(self):
        if self._tag == 'OB':
            if (self.bdata.parent and self.bdata.parent.type == 'ARMATURE' and
                    self.bdata.parent_type == 'BONE' and self.bdata.parent_bone):
                # Try to parent to a bone.
                bo_par = self.bdata.parent.pose.bones.get(self.bdata.parent_bone, None)
                if (bo_par):
                    return ObjectWrapper(bo_par, self.bdata.parent)
                else:  # Fallback to mere object parenting.
                    return ObjectWrapper(self.bdata.parent)
            else:
                # Mere object parenting.
                return ObjectWrapper(self.bdata.parent)
        elif self._tag == 'DP':
            return ObjectWrapper(self._ref)
        else:  # self._tag == 'BO'
            return ObjectWrapper(self.bdata.parent, self._ref) or ObjectWrapper(self._ref)
    parent = property(get_parent)

    def get_bdata_pose_bone(self):
        if self._tag == 'BO':
            return self._ref.pose.bones[self.bdata.name]
        return None
    bdata_pose_bone = property(get_bdata_pose_bone)

    def get_matrix_local(self):
        if self._tag == 'OB':
            return self.bdata.matrix_local.copy()
        elif self._tag == 'DP':
            return self._ref.matrix_world.inverted_safe() @ self._dupli_matrix
        else:  # 'BO', current pose
            # PoseBone.matrix is in armature space, bring in back in real local one!
            par = self.bdata.parent
            par_mat_inv = self._ref.pose.bones[par.name].matrix.inverted_safe() if par else Matrix()
            return par_mat_inv @ self._ref.pose.bones[self.bdata.name].matrix
    matrix_local = property(get_matrix_local)

    def get_matrix_global(self):
        if self._tag == 'OB':
            return self.bdata.matrix_world.copy()
        elif self._tag == 'DP':
            return self._dupli_matrix
        else:  # 'BO', current pose
            return self._ref.matrix_world @ self._ref.pose.bones[self.bdata.name].matrix
    matrix_global = property(get_matrix_global)

    def get_matrix_rest_local(self):
        if self._tag == 'BO':
            # Bone.matrix_local is in armature space, bring in back in real local one!
            par = self.bdata.parent
            par_mat_inv = par.matrix_local.inverted_safe() if par else Matrix()
            return par_mat_inv @ self.bdata.matrix_local
        else:
            return self.matrix_local.copy()
    matrix_rest_local = property(get_matrix_rest_local)

    def get_matrix_rest_global(self):
        if self._tag == 'BO':
            return self._ref.matrix_world @ self.bdata.matrix_local
        else:
            return self.matrix_global.copy()
    matrix_rest_global = property(get_matrix_rest_global)

    # #### Transform and helpers
    def has_valid_parent(self, objects):
        par = self.parent
        if par in objects:
            if self._tag == 'OB':
                par_type = self.bdata.parent_type
                if par_type in {'OBJECT', 'BONE'}:
                    return True
                else:
                    print("Sorry, “{}” parenting type is not supported".format(par_type))
                    return False
            return True
        return False

    def use_bake_space_transform(self, scene_data):
        # NOTE: Only applies to object types supporting this!!! Currently, only meshes and the like...
        # TODO: Check whether this can work for bones too...
        return (scene_data.settings.bake_space_transform and self._tag in {'OB', 'DP'} and
                self.bdata.type in BLENDER_OBJECT_TYPES_MESHLIKE | {'EMPTY'})

    def fbx_object_matrix(self, scene_data, rest=False, local_space=False, global_space=False):
        """
        Generate object transform matrix (*always* in matching *FBX* space!).
        If local_space is True, returned matrix is *always* in local space.
        Else if global_space is True, returned matrix is always in world space.
        If both local_space and global_space are False, returned matrix is in parent space if parent is valid,
        else in world space.
        Note local_space has precedence over global_space.
        If rest is True and object is a Bone, returns matching rest pose transform instead of current pose one.
        Applies specific rotation to bones, lamps and cameras (conversion Blender -> FBX).
        """
        # Objects which are not bones and do not have any parent are *always* in global space
        # (unless local_space is True!).
        is_global = (not local_space and
                     (global_space or not (self._tag in {'DP', 'BO'} or self.has_valid_parent(scene_data.objects))))

        # Objects (meshes!) parented to armature are not parented to anything in FBX, hence we need them
        # in global space, which is their 'virtual' local space...
        is_global = is_global or self.parented_to_armature

        # Since we have to apply corrections to some types of object, we always need local Blender space here...
        matrix = self.matrix_rest_local if rest else self.matrix_local
        parent = self.parent

        # Bones, lamps and cameras need to be rotated (in local space!).
        if self._tag == 'BO':
            # If we have a bone parent we need to undo the parent correction.
            if not is_global and scene_data.settings.bone_correction_matrix_inv and parent and parent.is_bone:
                matrix = scene_data.settings.bone_correction_matrix_inv @ matrix
            # Apply the bone correction.
            if scene_data.settings.bone_correction_matrix:
                matrix = matrix @ scene_data.settings.bone_correction_matrix
        elif self.bdata.type == 'LIGHT':
            matrix = matrix @ MAT_CONVERT_LIGHT
        elif self.bdata.type == 'CAMERA':
            matrix = matrix @ MAT_CONVERT_CAMERA

        if self._tag in {'DP', 'OB'} and parent:
            if parent._tag == 'BO':
                # In bone parent case, we get transformation in **bone tip** space (sigh).
                # Have to bring it back into bone root, which is FBX expected value.
                matrix = Matrix.Translation((0, (parent.bdata.tail - parent.bdata.head).length, 0)) @ matrix

        # Our matrix is in local space, time to bring it in its final desired space.
        if parent:
            if is_global:
                # Move matrix to global Blender space.
                matrix = (parent.matrix_rest_global if rest else parent.matrix_global) @ matrix
            elif parent.use_bake_space_transform(scene_data):
                # Blender's and FBX's local space of parent may differ if we use bake_space_transform...
                # Apply parent's *Blender* local space...
                matrix = (parent.matrix_rest_local if rest else parent.matrix_local) @ matrix
                # ...and move it back into parent's *FBX* local space.
                par_mat = parent.fbx_object_matrix(scene_data, rest=rest, local_space=True)
                matrix = par_mat.inverted_safe() @ matrix

        if self.use_bake_space_transform(scene_data):
            # If we bake the transforms we need to post-multiply inverse global transform.
            # This means that the global transform will not apply to children of this transform.
            matrix = matrix @ scene_data.settings.global_matrix_inv
        if is_global:
            # In any case, pre-multiply the global matrix to get it in FBX global space!
            matrix = scene_data.settings.global_matrix @ matrix

        return matrix

    def fbx_object_tx(self, scene_data, rest=False, rot_euler_compat=None):
        """
        Generate object transform data (always in local space when possible).
        """
        matrix = self.fbx_object_matrix(scene_data, rest=rest)
        loc, rot, scale = matrix.decompose()
        matrix_rot = rot.to_matrix()
        # quat -> euler, we always use 'XYZ' order, use ref rotation if given.
        if rot_euler_compat is not None:
            rot = rot.to_euler('XYZ', rot_euler_compat)
        else:
            rot = rot.to_euler('XYZ')
        return loc, rot, scale, matrix, matrix_rot

    # #### _tag dependent...
    def get_is_object(self):
        return self._tag == 'OB'
    is_object = property(get_is_object)

    def get_is_dupli(self):
        return self._tag == 'DP'
    is_dupli = property(get_is_dupli)

    def get_is_bone(self):
        return self._tag == 'BO'
    is_bone = property(get_is_bone)

    def get_type(self):
        if self._tag in {'OB', 'DP'}:
            return self.bdata.type
        return ...
    type = property(get_type)

    def get_armature(self):
        if self._tag == 'BO':
            return ObjectWrapper(self._ref)
        return None
    armature = property(get_armature)

    def get_bones(self):
        if self._tag == 'OB' and self.bdata.type == 'ARMATURE':
            return (ObjectWrapper(bo, self.bdata) for bo in self.bdata.data.bones)
        return ()
    bones = property(get_bones)

    def get_materials(self):
        override_materials = self.override_materials
        if override_materials is not None:
            return override_materials
        if self._tag in {'OB', 'DP'}:
            return tuple(slot.material for slot in self.bdata.material_slots)
        return ()
    materials = property(get_materials)

    def is_deformed_by_armature(self, arm_obj):
        if not (self.is_object and self.type == 'MESH'):
            return False
        if self.parent == arm_obj and self.bdata.parent_type == 'ARMATURE':
            return True
        for mod in self.bdata.modifiers:
            if mod.type == 'ARMATURE' and mod.object == arm_obj.bdata:
                return True

    # #### Duplis...
    def dupli_list_gen(self, depsgraph):
        if self._tag == 'OB' and self.bdata.is_instancer:
            return (ObjectWrapper(dup) for dup in depsgraph.object_instances
                    if dup.parent and ObjectWrapper(dup.parent.original) == self)
        return ()


def fbx_name_class(name, cls):
    return FBX_NAME_CLASS_SEP.join((name, cls))


# ##### Top-level FBX data container. #####

# Helper sub-container gathering all exporter settings related to media (texture files).
FBXExportSettingsMedia = namedtuple("FBXExportSettingsMedia", (
    "path_mode", "base_src", "base_dst", "subdir",
    "embed_textures", "copy_set", "embedded_set",
))

# Helper container gathering all exporter settings.
FBXExportSettings = namedtuple("FBXExportSettings", (
    "report", "to_axes", "global_matrix", "global_scale", "apply_unit_scale", "unit_scale",
    "bake_space_transform", "global_matrix_inv", "global_matrix_inv_transposed",
    "context_objects", "object_types", "use_mesh_modifiers", "use_mesh_modifiers_render",
    "mesh_smooth_type", "use_subsurf", "use_mesh_edges", "use_tspace", "use_triangles",
    "armature_nodetype", "use_armature_deform_only", "add_leaf_bones",
    "bone_correction_matrix", "bone_correction_matrix_inv",
    "bake_anim", "bake_anim_use_all_bones", "bake_anim_use_nla_strips", "bake_anim_use_all_actions",
    "bake_anim_step", "bake_anim_simplify_factor", "bake_anim_force_startend_keying",
    "use_metadata", "media_settings", "use_custom_props", "colors_type", "prioritize_active_color"
))

# Helper container gathering some data we need multiple times:
#     * templates.
#     * settings, scene.
#     * objects.
#     * object data.
#     * skinning data (binding armature/mesh).
#     * animations.
FBXExportData = namedtuple("FBXExportData", (
    "templates", "templates_users", "connections",
    "settings", "scene", "depsgraph", "objects", "animations", "animated", "frame_start", "frame_end",
    "data_empties", "data_lights", "data_cameras", "data_meshes", "mesh_material_indices",
    "data_bones", "data_leaf_bones", "data_deformers_skin", "data_deformers_shape",
    "data_world", "data_materials", "data_textures", "data_videos",
))

# Helper container gathering all importer settings.
FBXImportSettings = namedtuple("FBXImportSettings", (
    "report", "to_axes", "global_matrix", "global_scale",
    "bake_space_transform", "global_matrix_inv", "global_matrix_inv_transposed",
    "use_custom_normals", "use_image_search",
    "use_alpha_decals", "decal_offset",
    "use_anim", "anim_offset",
    "use_subsurf",
    "use_custom_props", "use_custom_props_enum_as_string",
    "nodal_material_wrap_map", "image_cache",
    "ignore_leaf_bones", "force_connect_children", "automatic_bone_orientation", "bone_correction_matrix",
    "use_prepost_rot", "colors_type",
))
