# SPDX-FileCopyrightText: 2018-2021 The glTF-Blender-IO authors
#
# SPDX-License-Identifier: Apache-2.0

import typing
import math
from mathutils import Matrix, Vector, Quaternion, Euler

from .data_path import get_target_property_name


def list_to_mathutils(values: typing.List[float], data_path: str) -> typing.Union[Vector, Quaternion, Euler]:
    """Transform a list to blender py object."""
    target = get_target_property_name(data_path)

    if target == 'delta_location':
        return Vector(values)  # TODO Should be Vector(values) - Vector(something)?
    elif target == 'delta_rotation_euler':
        return Euler(values).to_quaternion()  # TODO Should be Euler(values).to_quaternion() @ something?
    elif target == 'location':
        return Vector(values)
    elif target == 'rotation_axis_angle':
        angle = values[0]
        axis = values[1:]
        return Quaternion(axis, math.radians(angle))
    elif target == 'rotation_euler':
        return Euler(values).to_quaternion()
    elif target == 'rotation_quaternion':
        return Quaternion(values)
    elif target == 'scale':
        return Vector(values)
    elif target == 'value':
        return Vector(values)

    return values


def mathutils_to_gltf(x: typing.Union[Vector, Quaternion]) -> typing.List[float]:
    """Transform a py object to glTF list."""
    if isinstance(x, Vector):
        return list(x)
    if isinstance(x, Quaternion):
        # Blender has w-first quaternion notation
        return [x[1], x[2], x[3], x[0]]
    else:
        return list(x)


def to_yup() -> Matrix:
    """Transform to Yup."""
    return Matrix(
        ((1.0, 0.0, 0.0, 0.0),
         (0.0, 0.0, 1.0, 0.0),
         (0.0, -1.0, 0.0, 0.0),
         (0.0, 0.0, 0.0, 1.0))
    )


to_zup = to_yup


def swizzle_yup(v: typing.Union[Vector, Quaternion], data_path: str) -> typing.Union[Vector, Quaternion]:
    """Manage Yup."""
    target = get_target_property_name(data_path)
    swizzle_func = {
        "delta_location": swizzle_yup_location,
        "delta_rotation_euler": swizzle_yup_rotation,
        "location": swizzle_yup_location,
        "rotation_axis_angle": swizzle_yup_rotation,
        "rotation_euler": swizzle_yup_rotation,
        "rotation_quaternion": swizzle_yup_rotation,
        "scale": swizzle_yup_scale,
        "value": swizzle_yup_value
    }.get(target)

    if swizzle_func is None:
        raise RuntimeError("Cannot transform values at {}".format(data_path))

    return swizzle_func(v)


def swizzle_yup_location(loc: Vector) -> Vector:
    """Manage Yup location."""
    return Vector((loc[0], loc[2], -loc[1]))


def swizzle_yup_rotation(rot: Quaternion) -> Quaternion:
    """Manage Yup rotation."""
    return Quaternion((rot[0], rot[1], rot[3], -rot[2]))


def swizzle_yup_scale(scale: Vector) -> Vector:
    """Manage Yup scale."""
    return Vector((scale[0], scale[2], scale[1]))


def swizzle_yup_value(value: typing.Any) -> typing.Any:
    """Manage Yup value."""
    return value


def transform(v: typing.Union[Vector, Quaternion], data_path: str, transform: Matrix = Matrix.Identity(
        4), need_rotation_correction: bool = False) -> typing .Union[Vector, Quaternion]:
    """Manage transformations."""
    target = get_target_property_name(data_path)
    transform_func = {
        "delta_location": transform_location,
        "delta_rotation_euler": transform_rotation,
        "location": transform_location,
        "rotation_axis_angle": transform_rotation,
        "rotation_euler": transform_rotation,
        "rotation_quaternion": transform_rotation,
        "scale": transform_scale,
        "value": transform_value
    }.get(target)

    if transform_func is None:
        raise RuntimeError("Cannot transform values at {}".format(data_path))

    return transform_func(v, transform, need_rotation_correction)


def transform_location(location: Vector, transform: Matrix = Matrix.Identity(4),
                       need_rotation_correction: bool = False) -> Vector:
    """Transform location."""
    correction = Quaternion((2**0.5 / 2, -2**0.5 / 2, 0.0, 0.0))
    m = Matrix.Translation(location)
    if need_rotation_correction:
        m @= correction.to_matrix().to_4x4()
    m = transform @ m
    return m.to_translation()


def transform_rotation(rotation: Quaternion, transform: Matrix = Matrix.Identity(4),
                       need_rotation_correction: bool = False) -> Quaternion:
    """Transform rotation."""
    rotation.normalize()
    correction = Quaternion((2**0.5 / 2, -2**0.5 / 2, 0.0, 0.0))
    m = rotation.to_matrix().to_4x4()
    if need_rotation_correction:
        m @= correction.to_matrix().to_4x4()
    m = transform @ m
    return m.to_quaternion()


def transform_scale(scale: Vector, transform: Matrix = Matrix.Identity(4),
                    need_rotation_correction: bool = False) -> Vector:
    """Transform scale."""
    m = Matrix.Identity(4)
    m[0][0] = scale.x
    m[1][1] = scale.y
    m[2][2] = scale.z
    m = transform @ m

    return m.to_scale()


def transform_value(value: Vector, _: Matrix = Matrix.Identity(4), need_rotation_correction: bool = False) -> Vector:
    """Transform value."""
    return value


def round_if_near(value: float, target: float) -> float:
    """If value is very close to target, round to target."""
    return value if abs(value - target) > 2.0e-6 else target


def scale_rot_swap_matrix(rot):
    """Returns a matrix m st. Scale[s] Rot[rot] = Rot[rot] Scale[m s].
    If rot.to_matrix() is a signed permutation matrix, works for any s.
    Otherwise works only if s is a uniform scaling.
    """
    m = nearby_signed_perm_matrix(rot)  # snap to signed perm matrix
    m.transpose()  # invert permutation
    for i in range(3):
        for j in range(3):
            m[i][j] = abs(m[i][j])  # discard sign
    return m


def nearby_signed_perm_matrix(rot):
    """Returns a signed permutation matrix close to rot.to_matrix().
    (A signed permutation matrix is like a permutation matrix, except
    the non-zero entries can be ±1.)
    """
    m = rot.to_matrix()
    x, y, z = m[0], m[1], m[2]

    # Set the largest entry in the first row to ±1
    a, b, c = abs(x[0]), abs(x[1]), abs(x[2])
    i = 0 if a >= b and a >= c else 1 if b >= c else 2
    x[i] = 1 if x[i] > 0 else -1
    x[(i + 1) % 3] = 0
    x[(i + 2) % 3] = 0

    # Same for second row: only two columns to consider now.
    a, b = abs(y[(i + 1) % 3]), abs(y[(i + 2) % 3])
    j = (i + 1) % 3 if a >= b else (i + 2) % 3
    y[j] = 1 if y[j] > 0 else -1
    y[(j + 1) % 3] = 0
    y[(j + 2) % 3] = 0

    # Same for third row: only one column left
    k = (0 + 1 + 2) - i - j
    z[k] = 1 if z[k] > 0 else -1
    z[(k + 1) % 3] = 0
    z[(k + 2) % 3] = 0

    return m
