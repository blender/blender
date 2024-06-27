# SPDX-FileCopyrightText: 2019-2022 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy
import math
import collections
import typing

from abc import ABC
from itertools import tee, chain, islice, repeat, permutations
from mathutils import Vector, Matrix, Color
from rna_prop_ui import rna_idprop_value_to_python


T = typing.TypeVar('T')
IdType = typing.TypeVar('IdType', bound=bpy.types.ID)

AnyVector = Vector | typing.Sequence[float]

##############################################
# Math
##############################################

axis_vectors = {
    'x': (1, 0, 0),
    'y': (0, 1, 0),
    'z': (0, 0, 1),
    '-x': (-1, 0, 0),
    '-y': (0, -1, 0),
    '-z': (0, 0, -1),
}


# Matrices that reshuffle axis order and/or invert them
shuffle_matrix = {
    sx+x+sy+y+sz+z: Matrix((
        axis_vectors[sx+x], axis_vectors[sy+y], axis_vectors[sz+z]
        )).transposed().freeze()
    for x, y, z in permutations(['x', 'y', 'z'])
    for sx in ('', '-')
    for sy in ('', '-')
    for sz in ('', '-')
}


def angle_on_plane(plane: Vector, vec1: Vector, vec2: Vector):
    """ Return the angle between two vectors projected onto a plane.
    """
    plane.normalize()
    vec1 = vec1 - (plane * (vec1.dot(plane)))
    vec2 = vec2 - (plane * (vec2.dot(plane)))
    vec1.normalize()
    vec2.normalize()

    # Determine the angle
    angle = math.acos(max(-1.0, min(1.0, vec1.dot(vec2))))

    if angle < 0.00001:  # close enough to zero that sign doesn't matter
        return angle

    # Determine the sign of the angle
    vec3 = vec2.cross(vec1)
    vec3.normalize()
    sign = vec3.dot(plane)
    if sign >= 0:
        sign = 1
    else:
        sign = -1

    return angle * sign


# Convert between a matrix and axis+roll representations.
# Re-export the C implementation internally used by bones.
matrix_from_axis_roll = bpy.types.Bone.MatrixFromAxisRoll
axis_roll_from_matrix = bpy.types.Bone.AxisRollFromMatrix


def matrix_from_axis_pair(y_axis: AnyVector, other_axis: AnyVector, axis_name: str):
    assert axis_name in 'xz'

    y_axis = Vector(y_axis).normalized()

    if axis_name == 'x':
        z_axis = Vector(other_axis).cross(y_axis).normalized()
        x_axis = y_axis.cross(z_axis)
    else:
        x_axis = y_axis.cross(other_axis).normalized()
        z_axis = x_axis.cross(y_axis)

    return Matrix((x_axis, y_axis, z_axis)).transposed()


##############################################
# Color correction functions
##############################################

# noinspection SpellCheckingInspection
def linsrgb_to_srgb(linsrgb: float):
    """Convert physically linear RGB values into sRGB ones. The transform is
    uniform in the components, so *linsrgb* can be of any shape.

    *linsrgb* values should range between 0 and 1, inclusively.

    """
    # From Wikipedia, but easy analogue to the above.
    gamma = 1.055 * linsrgb**(1./2.4) - 0.055
    scale = linsrgb * 12.92
    # return np.where (linsrgb > 0.0031308, gamma, scale)
    if linsrgb > 0.0031308:
        return gamma
    return scale


def gamma_correct(color: Color):
    corrected_color = Color()
    for i, component in enumerate(color):               # noqa
        corrected_color[i] = linsrgb_to_srgb(color[i])  # noqa
    return corrected_color


##############################################
# Iterators
##############################################

# noinspection SpellCheckingInspection
def padnone(iterable, pad=None):
    return chain(iterable, repeat(pad))


# noinspection SpellCheckingInspection
def pairwise_nozip(iterable):
    """s -> (s0,s1), (s1,s2), (s2,s3), ..."""
    a, b = tee(iterable)
    next(b, None)
    return a, b


def pairwise(iterable):
    """s -> (s0,s1), (s1,s2), (s2,s3), ..."""
    a, b = tee(iterable)
    next(b, None)
    return zip(a, b)


def map_list(func, *inputs):
    """[func(a0,b0...), func(a1,b1...), ...]"""
    return list(map(func, *inputs))


def skip(n, iterable):
    """Returns an iterator skipping first n elements of an iterable."""
    iterator = iter(iterable)
    if n == 1:
        next(iterator, None)
    else:
        next(islice(iterator, n, n), None)
    return iterator


def map_apply(func, *inputs):
    """Apply the function to inputs like map for side effects, discarding results."""
    collections.deque(map(func, *inputs), maxlen=0)


def find_index(sequence, item, default=None):
    for i, elem in enumerate(sequence):
        if elem == item:
            return i

    return default


def flatten_children(iterable: typing.Iterable):
    """Enumerate the iterator items as well as their children in the tree order."""
    for item in iterable:
        yield item
        yield from flatten_children(item.children)


def flatten_parents(item):
    """Enumerate the item and all its parents."""
    while item:
        yield item
        item = item.parent


##############################################
# Lazy references
##############################################

Lazy: typing.TypeAlias = T | typing.Callable[[], T]
OptionalLazy: typing.TypeAlias = typing.Optional[T | typing.Callable[[], T]]


def force_lazy(value: OptionalLazy[T]) -> T:
    """If the argument is callable, invokes it without arguments.
    Otherwise, returns the argument as is."""
    if callable(value):
        return value()
    else:
        return value


class LazyRef(typing.Generic[T]):
    """Hashable lazy reference. When called, evaluates (foo, 'a', 'b'...) as foo('a','b')
    if foo is callable. Otherwise, the remaining arguments are used as attribute names or
    keys, like foo.a.b or foo.a[b] etc."""

    def __init__(self, first, *args):
        self.first = first
        self.args = tuple(args)
        self.first_hashable = first.__hash__ is not None

    def __repr__(self):
        return 'LazyRef{}'.format((self.first, *self.args))

    def __eq__(self, other):
        return (
            isinstance(other, LazyRef) and
            (self.first == other.first if self.first_hashable else self.first is other.first) and
            self.args == other.args
        )

    def __hash__(self):
        return (hash(self.first) if self.first_hashable
                else hash(id(self.first))) ^ hash(self.args)

    def __call__(self) -> T:
        first = self.first
        if callable(first):
            return first(*self.args)

        for item in self.args:
            if isinstance(first, (dict, list)):
                first = first[item]
            else:
                first = getattr(first, item)

        return first


##############################################
# Misc
##############################################

def copy_attributes(a, b):
    keys = dir(a)
    for key in keys:
        if not (key.startswith("_") or
                key.startswith("error_") or
                key in ("group", "is_valid", "is_valid", "bl_rna")):
            try:
                setattr(b, key, getattr(a, key))
            except AttributeError:
                pass


def property_to_python(value) -> typing.Any:
    value = rna_idprop_value_to_python(value)

    if isinstance(value, dict):
        return {k: property_to_python(v) for k, v in value.items()}
    elif isinstance(value, list):
        return map_list(property_to_python, value)
    else:
        return value


def clone_parameters(target):
    return property_to_python(dict(target))


def assign_parameters(target, val_dict=None, **params):
    if val_dict is not None:
        for key in list(target.keys()):
            del target[key]

        data = {**val_dict, **params}
    else:
        data = params

    for key, value in data.items():
        try:
            target[key] = value
        except Exception as e:
            raise Exception(f"Couldn't set {key} to {value}: {e}")


def select_object(context: bpy.types.Context, obj: bpy.types.Object, deselect_all=False):
    view_layer = context.view_layer

    if deselect_all:
        for layer_obj in view_layer.objects:
            layer_obj.select_set(False)  # deselect all objects

    obj.select_set(True)
    view_layer.objects.active = obj


def choose_next_uid(collection: typing.Iterable, prop_name: str, *, min_value=0):
    return 1 + max(
        (getattr(obj, prop_name, min_value - 1) for obj in collection),
        default=min_value-1,
    )


##############################################
# Text
##############################################

def wrap_list_to_lines(prefix: str, delimiters: tuple[str, str] | str,
                       items: typing.Iterable[str], *,
                       limit=90, indent=4) -> list[str]:
    """
    Generate a string representation of a list of items, wrapping lines if necessary.

    Args:
        prefix:       Text of the first line before the list.
        delimiters:   Start and end of list delimiters.
        items:        List items, already converted to strings.
        limit:        Maximum line length.
        indent:       Wrapped line indent relative to prefix.
    """
    start, end = delimiters
    items = list(items)
    simple_line = prefix + start + ', '.join(items) + end

    if not items or len(simple_line) <= limit:
        return [simple_line]

    prefix_indent = prefix[0: len(prefix) - len(prefix.lstrip())]
    inner_indent = prefix_indent + ' ' * indent

    result = []
    line = prefix + start

    for item in items:
        item_repr = item + ','

        if not result or len(line) + len(item_repr) + 1 > limit:
            result.append(line)
            line = inner_indent + item_repr
        else:
            line += ' ' + item_repr

    result.append(line[:-1] + end)
    return result


##############################################
# Typing
##############################################

class TypedObject(bpy.types.Object, typing.Generic[IdType]):
    data: IdType


ArmatureObject = TypedObject[bpy.types.Armature]
MeshObject = TypedObject[bpy.types.Mesh]


def verify_armature_obj(obj: bpy.types.Object) -> ArmatureObject:
    assert obj and obj.type == 'ARMATURE'
    return obj  # noqa


def verify_mesh_obj(obj: bpy.types.Object) -> MeshObject:
    assert obj and obj.type == 'MESH'
    return obj  # noqa


class IdPropSequence(typing.Mapping[str, T], typing.Sequence[T], ABC):
    def __getitem__(self, item: str | int) -> T:
        pass

    def __setitem__(self, key: str | int, value: T):
        pass

    def __iter__(self) -> typing.Iterator[T]:
        pass

    def add(self) -> T:
        pass

    def clear(self):
        pass

    def move(self, from_idx: int, to_idx: int):
        pass

    def remove(self, item: int):
        pass
