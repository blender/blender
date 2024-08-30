# SPDX-FileCopyrightText: 2018-2021 The glTF-Blender-IO authors
#
# SPDX-License-Identifier: Apache-2.0

from enum import IntEnum


class ComponentType(IntEnum):
    Byte = 5120
    UnsignedByte = 5121
    Short = 5122
    UnsignedShort = 5123
    UnsignedInt = 5125
    Float = 5126

    @classmethod
    def to_type_code(cls, component_type):
        return {
            ComponentType.Byte: 'b',
            ComponentType.UnsignedByte: 'B',
            ComponentType.Short: 'h',
            ComponentType.UnsignedShort: 'H',
            ComponentType.UnsignedInt: 'I',
            ComponentType.Float: 'f'
        }[component_type]

    @classmethod
    def to_numpy_dtype(cls, component_type):
        import numpy as np
        return {
            ComponentType.Byte: np.int8,
            ComponentType.UnsignedByte: np.uint8,
            ComponentType.Short: np.int16,
            ComponentType.UnsignedShort: np.uint16,
            ComponentType.UnsignedInt: np.uint32,
            ComponentType.Float: np.float32,
        }[component_type]

    @classmethod
    def from_legacy_define(cls, type_define):
        return {
            GLTF_COMPONENT_TYPE_BYTE: ComponentType.Byte,
            GLTF_COMPONENT_TYPE_UNSIGNED_BYTE: ComponentType.UnsignedByte,
            GLTF_COMPONENT_TYPE_SHORT: ComponentType.Short,
            GLTF_COMPONENT_TYPE_UNSIGNED_SHORT: ComponentType.UnsignedShort,
            GLTF_COMPONENT_TYPE_UNSIGNED_INT: ComponentType.UnsignedInt,
            GLTF_COMPONENT_TYPE_FLOAT: ComponentType.Float
        }[type_define]

    @classmethod
    def get_size(cls, component_type):
        return {
            ComponentType.Byte: 1,
            ComponentType.UnsignedByte: 1,
            ComponentType.Short: 2,
            ComponentType.UnsignedShort: 2,
            ComponentType.UnsignedInt: 4,
            ComponentType.Float: 4
        }[component_type]


class DataType:
    Scalar = "SCALAR"
    Vec2 = "VEC2"
    Vec3 = "VEC3"
    Vec4 = "VEC4"
    Mat2 = "MAT2"
    Mat3 = "MAT3"
    Mat4 = "MAT4"

    def __new__(cls, *args, **kwargs):
        raise RuntimeError("{} should not be instantiated".format(cls.__name__))

    @classmethod
    def num_elements(cls, data_type):
        return {
            DataType.Scalar: 1,
            DataType.Vec2: 2,
            DataType.Vec3: 3,
            DataType.Vec4: 4,
            DataType.Mat2: 4,
            DataType.Mat3: 9,
            DataType.Mat4: 16
        }[data_type]

    @classmethod
    def vec_type_from_num(cls, num_elems):
        if not (0 < num_elems < 5):
            raise ValueError("No vector type with {} elements".format(num_elems))
        return {
            1: DataType.Scalar,
            2: DataType.Vec2,
            3: DataType.Vec3,
            4: DataType.Vec4
        }[num_elems]

    @classmethod
    def mat_type_from_num(cls, num_elems):
        if not (4 <= num_elems <= 16):
            raise ValueError("No matrix type with {} elements".format(num_elems))
        return {
            4: DataType.Mat2,
            9: DataType.Mat3,
            16: DataType.Mat4
        }[num_elems]


class TextureFilter(IntEnum):
    Nearest = 9728
    Linear = 9729
    NearestMipmapNearest = 9984
    LinearMipmapNearest = 9985
    NearestMipmapLinear = 9986
    LinearMipmapLinear = 9987


class TextureWrap(IntEnum):
    ClampToEdge = 33071
    MirroredRepeat = 33648
    Repeat = 10497


class BufferViewTarget(IntEnum):
    ARRAY_BUFFER = 34962
    ELEMENT_ARRAY_BUFFER = 34963

#################
# LEGACY DEFINES


GLTF_VERSION = "2.0"

#
# Component Types
#
GLTF_COMPONENT_TYPE_BYTE = "BYTE"
GLTF_COMPONENT_TYPE_UNSIGNED_BYTE = "UNSIGNED_BYTE"
GLTF_COMPONENT_TYPE_SHORT = "SHORT"
GLTF_COMPONENT_TYPE_UNSIGNED_SHORT = "UNSIGNED_SHORT"
GLTF_COMPONENT_TYPE_UNSIGNED_INT = "UNSIGNED_INT"
GLTF_COMPONENT_TYPE_FLOAT = "FLOAT"


#
# Data types
#
GLTF_DATA_TYPE_SCALAR = "SCALAR"
GLTF_DATA_TYPE_VEC2 = "VEC2"
GLTF_DATA_TYPE_VEC3 = "VEC3"
GLTF_DATA_TYPE_VEC4 = "VEC4"
GLTF_DATA_TYPE_MAT2 = "MAT2"
GLTF_DATA_TYPE_MAT3 = "MAT3"
GLTF_DATA_TYPE_MAT4 = "MAT4"

GLTF_IOR = 1.5
BLENDER_COAT_ROUGHNESS = 0.03

# Rounding digit used for normal/tangent rounding
ROUNDING_DIGIT = 4
