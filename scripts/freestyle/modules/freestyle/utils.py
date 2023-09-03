# SPDX-FileCopyrightText: 2009-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
This module contains helper functions used for Freestyle style module
writing.
"""

__all__ = (
    "angle_x_normal",
    "bound",
    "bounding_box",
    "BoundingBox",
    "ContextFunctions",
    "curvature_from_stroke_vertex",
    "find_matching_vertex",
    "get_chain_length",
    "get_object_name",
    "get_strokes",
    "get_test_stroke",
    "getCurrentScene",
    "integrate",
    "is_poly_clockwise",
    "iter_distance_along_stroke",
    "iter_distance_from_camera",
    "iter_distance_from_object",
    "iter_material_value",
    "iter_t2d_along_stroke",
    "material_from_fedge",
    "normal_at_I0D",
    "pairwise",
    "phase_to_direction",
    "rgb_to_bw",
    "simplify",
    "stroke_curvature",
    "stroke_normal",
    "StrokeCollector",
    "tripplewise",
)

# module members
from _freestyle import (
    ContextFunctions,
    getCurrentScene,
    integrate,
)

# constructs for helper functions in Python
from freestyle.types import (
    Interface0DIterator,
    Stroke,
    StrokeShader,
    StrokeVertexIterator,
)

from mathutils import Vector
from functools import lru_cache, namedtuple
from math import cos, sin, pi, atan2
from itertools import tee, compress


# -- types -- #

# A named tuple primitive used for storing data that has an upper and
# lower bound (e.g., thickness, range and certain other values)
class BoundedProperty(namedtuple("BoundedProperty", ["min", "max", "delta"])):
    def __new__(cls, minimum, maximum, delta=None):
        if delta is None:
            delta = abs(maximum - minimum)
        return super().__new__(cls, minimum, maximum, delta)

    def interpolate(self, val):
        result = (self.max - val) / self.delta
        return 1.0 - bound(0, result, 1)


# -- real utility functions  -- #

def rgb_to_bw(r, g, b):
    """Method to convert rgb to a bw intensity value."""
    return 0.35 * r + 0.45 * g + 0.2 * b


def bound(lower, x, higher):
    """Returns x bounded by a maximum and minimum value. Equivalent to:
    return min(max(x, lower), higher)
    """
    # this is about 50% quicker than min(max(x, lower), higher)
    return (lower if x <= lower else higher if x >= higher else x)


def get_strokes():
    """Get all strokes that are currently available"""
    return tuple(map(Operators().get_stroke_from_index, range(Operators().get_strokes_size())))


def is_poly_clockwise(stroke):
    """True if the stroke is orientated in a clockwise way, False otherwise"""
    v = sum((v2.point.x - v1.point.x) * (v1.point.y + v2.point.y) for v1, v2 in pairwise(stroke))
    v1, v2 = stroke[0], stroke[-1]
    if (v1.point - v2.point).length > 1e-3:
        v += (v2.point.x - v1.point.x) * (v1.point.y + v2.point.y)
    return v > 0


def get_object_name(stroke):
    """Returns the name of the object that this stroke is drawn on."""
    fedge = stroke[0].fedge
    if fedge is None:
        return None
    return fedge.viewedge.viewshape.name


def material_from_fedge(fe):
    "get the diffuse RGBA color from an FEdge"
    if fe is None:
        return None
    if fe.is_smooth:
        material = fe.material
    else:
        right, left = fe.material_right, fe.material_left
        material = right if (right.priority > left.priority) else left
    return material


def bounding_box(stroke):
    """
    Returns the maximum and minimum coordinates (the bounding box) of the stroke's vertices
    """
    x, y = zip(*(svert.point for svert in stroke))
    return (Vector((min(x), min(y))), Vector((max(x), max(y))))


def normal_at_I0D(it: Interface0DIterator) -> Vector:
    """Normal at an Interface0D object. In contrast to Normal2DF0D this
       function uses the actual data instead of underlying Fedge objects.
    """
    if it.at_last and it.is_begin:
        # corner-case
        return Vector((0, 0))
    elif it.at_last:
        it.decrement()
        a, b = it.object, next(it)
    elif it.is_begin:
        a, b = it.object, next(it)
        # give iterator back in original state
        it.decrement()
    elif it.is_end:
        # just fail hard: this shouldn not happen
        raise StopIteration()
    else:
        # this case sometimes has a small difference with Normal2DF0D (1e-3 -ish)
        it.decrement()
        a = it.object
        _curr, b = next(it), next(it)
        # give iterator back in original state
        it.decrement()
    return (b.point - a.point).orthogonal().normalized()


def angle_x_normal(it: Interface0DIterator):
    """unsigned angle between a Point's normal and the X axis, in radians"""
    normal = normal_at_I0D(it)
    return abs(atan2(normal[1], normal[0]))


def curvature_from_stroke_vertex(svert):
    """The 3D curvature of an stroke vertex' underlying geometry
       The result is None or in the range [-inf, inf]"""
    c1 = svert.first_svertex.curvatures
    c2 = svert.second_svertex.curvatures
    if c1 is None and c2 is None:
        Kr = None
    elif c1 is None:
        Kr = c2[4]
    elif c2 is None:
        Kr = c1[4]
    else:
        Kr = c1[4] + svert.t2d * (c2[4] - c1[4])
    return Kr


# -- General helper functions -- #

@lru_cache(maxsize=32)
def phase_to_direction(length):
    """
    Returns a list of tuples each containing:
    - the phase
    - a Vector with the values of the cosine and sine of 2pi * phase  (the direction)
    """
    results = list()
    for i in range(length):
        phase = i / (length - 1)
        results.append((phase, Vector((cos(2 * pi * phase), sin(2 * pi * phase)))))
    return results


# -- simplification of a set of points; based on simplify.js by Vladimir Agafonkin --
#    https://mourner.github.io/simplify-js/

def getSquareSegmentDistance(p, p1, p2):
    """
    Square distance between point and a segment
    """
    x, y = p1

    dx, dy = (p2 - p1)

    if dx or dy:
        t = ((p.x - x) * dx + (p.y - y) * dy) / (dx * dx + dy * dy)

        if t > 1:
            x, y = p2
        elif t > 0:
            x += dx * t
            y += dy * t

    dx, dy = p.x - x, p.y - y
    return dx * dx + dy * dy


def simplifyDouglasPeucker(points, tolerance):
    length = len(points)
    markers = [0] * length

    first = 0
    last = length - 1

    first_stack = []
    last_stack = []

    markers[first] = 1
    markers[last] = 1

    while last:
        max_sqdist = 0

        for i in range(first, last):
            sqdist = getSquareSegmentDistance(points[i], points[first], points[last])

            if sqdist > max_sqdist:
                index = i
                max_sqdist = sqdist

        if max_sqdist > tolerance:
            markers[index] = 1

            first_stack.append(first)
            last_stack.append(index)

            first_stack.append(index)
            last_stack.append(last)

        first = first_stack.pop() if first_stack else None
        last = last_stack.pop() if last_stack else None

    return tuple(compress(points, markers))


def simplify(points, tolerance):
    """Simplifies a set of points"""
    return simplifyDouglasPeucker(points, tolerance * tolerance)


class BoundingBox:
    """Object representing a bounding box consisting out of 2 2D vectors"""

    __slots__ = (
        "minimum",
        "maximum",
        "size",
        "corners",
    )

    def __init__(self, minimum: Vector, maximum: Vector):
        self.minimum = minimum
        self.maximum = maximum
        if len(minimum) != len(maximum):
            raise TypeError("Expected two vectors of size 2, got", minimum, maximum)
        self.size = len(minimum)
        self.corners = (minimum, maximum)

    def __repr__(self):
        return "BoundingBox(%r, %r)" % (self.minimum, self.maximum)

    @classmethod
    def from_sequence(cls, sequence):
        """BoundingBox from sequence of 2D or 3D Vector objects"""
        x, y = zip(*sequence)
        mini = Vector((min(x), min(y)))
        maxi = Vector((max(x), max(y)))
        return cls(mini, maxi)

    def inside(self, other):
        """True if self inside other, False otherwise"""
        if self.size != other.size:
            raise TypeError("Expected two BoundingBox of the same size, got", self, other)
        return (self.minimum.x >= other.minimum.x and self.minimum.y >= other.minimum.y and
                self.maximum.x <= other.maximum.x and self.maximum.y <= other.maximum.y)


class StrokeCollector(StrokeShader):
    """Collects and Stores stroke objects"""

    def __init__(self):
        StrokeShader.__init__(self)
        self.strokes = []

    def shade(self, stroke):
        self.strokes.append(stroke)


# -- helper functions for chaining -- #

def get_chain_length(ve, orientation):
    """Returns the 2d length of a given ViewEdge."""
    from freestyle.chainingiterators import pyChainSilhouetteGenericIterator
    length = 0.0
    # setup iterator
    _it = pyChainSilhouetteGenericIterator(False, False)
    _it.begin = ve
    _it.current_edge = ve
    _it.orientation = orientation
    _it.init()

    # run iterator till end of chain
    while not (_it.is_end):
        length += _it.object.length_2d
        if (_it.is_begin):
            # _it has looped back to the beginning;
            # break to prevent infinite loop
            break
        _it.increment()

    # reset iterator
    _it.begin = ve
    _it.current_edge = ve
    _it.orientation = orientation

    # run iterator till begin of chain
    if not _it.is_begin:
        _it.decrement()
        while not (_it.is_end or _it.is_begin):
            length += _it.object.length_2d
            _it.decrement()

    return length


def find_matching_vertex(id, it):
    """Finds the matching vertex, or returns None."""
    return next((ve for ve in it if ve.id == id), None)


# -- helper functions for iterating -- #

def pairwise(iterable, types={Stroke, StrokeVertexIterator}):
    """Yields a tuple containing the previous and current object """
    # use .incremented() for types that support it
    if type(iterable) in types:
        it = iter(iterable)
        return zip(it, it.incremented())
    else:
        a, b = tee(iterable)
        next(b, None)
        return zip(a, b)


def tripplewise(iterable):
    """Yields a tuple containing the current object and its immediate neighbors """
    a, b, c = tee(iterable)
    next(b, None)
    next(c, None)
    return zip(a, b, c)


def iter_t2d_along_stroke(stroke):
    """Yields the progress along the stroke."""
    total = stroke.length_2d
    distance = 0.0
    # yield for the comparison from the first vertex to itself
    yield 0.0
    for prev, svert in pairwise(stroke):
        distance += (prev.point - svert.point).length
        yield min(distance / total, 1.0) if total != 0.0 else 0.0


def iter_distance_from_camera(stroke, range_min, range_max, normfac):
    """
    Yields the distance to the camera relative to the maximum
    possible distance for every stroke vertex, constrained by
    given minimum and maximum values.
    """
    for svert in stroke:
        # length in the camera coordinate
        distance = svert.point_3d.length
        if range_min < distance < range_max:
            yield (svert, (distance - range_min) / normfac)
        else:
            yield (svert, 0.0) if range_min > distance else (svert, 1.0)


def iter_distance_from_object(stroke, location, range_min, range_max, normfac):
    """
    yields the distance to the given object relative to the maximum
    possible distance for every stroke vertex, constrained by
    given minimum and maximum values.
    """
    for svert in stroke:
        distance = (svert.point_3d - location).length  # in the camera coordinate
        if range_min < distance < range_max:
            yield (svert, (distance - range_min) / normfac)
        else:
            yield (svert, 0.0) if distance < range_min else (svert, 1.0)


def iter_material_value(stroke, func, attribute):
    """Yields a specific material attribute from the vertex' underlying material."""
    it = Interface0DIterator(stroke)
    for svert in it:
        material = func(it)
        # main
        if attribute == 'LINE':
            value = rgb_to_bw(*material.line[0:3])
        elif attribute == 'DIFF':
            value = rgb_to_bw(*material.diffuse[0:3])
        elif attribute == 'SPEC':
            value = rgb_to_bw(*material.specular[0:3])
        # line separate
        elif attribute == 'LINE_R':
            value = material.line[0]
        elif attribute == 'LINE_G':
            value = material.line[1]
        elif attribute == 'LINE_B':
            value = material.line[2]
        elif attribute == 'LINE_A':
            value = material.line[3]
        # diffuse separate
        elif attribute == 'DIFF_R':
            value = material.diffuse[0]
        elif attribute == 'DIFF_G':
            value = material.diffuse[1]
        elif attribute == 'DIFF_B':
            value = material.diffuse[2]
        elif attribute == 'ALPHA':
            value = material.diffuse[3]
        # specular separate
        elif attribute == 'SPEC_R':
            value = material.specular[0]
        elif attribute == 'SPEC_G':
            value = material.specular[1]
        elif attribute == 'SPEC_B':
            value = material.specular[2]
        elif attribute == 'SPEC_HARDNESS':
            value = material.shininess
        else:
            raise ValueError("unexpected material attribute: " + attribute)
        yield (svert, value)


def iter_distance_along_stroke(stroke):
    """Yields the absolute distance along the stroke up to the current vertex."""
    distance = 0.0
    # the positions need to be copied, because they are changed in the calling function
    points = tuple(svert.point.copy() for svert in stroke)
    yield distance
    for prev, curr in pairwise(points):
        distance += (prev - curr).length
        yield distance


# -- mathematical operations -- #

def stroke_curvature(it):
    """
    Compute the 2D curvature at the stroke vertex pointed by the iterator 'it'.
    K = 1 / R
    where R is the radius of the circle going through the current vertex and its neighbors
    """
    for _ in it:
        if (it.is_begin or it.is_end):
            yield 0.0
            continue
        else:
            it.decrement()
            prev, current, succ = it.object.point.copy(), next(it).point.copy(), next(it).point.copy()
            # return the iterator in an unchanged state
            it.decrement()

        ab = (current - prev)
        bc = (succ - current)
        ac = (prev - succ)

        a, b, c = ab.length, bc.length, ac.length

        try:
            area = 0.5 * ab.cross(ac)
            K = (4 * area) / (a * b * c)
        except ZeroDivisionError:
            K = 0.0

        yield abs(K)


def stroke_normal(stroke):
    """
    Compute the 2D normal at the stroke vertex pointed by the iterator
    'it'.  It is noted that Normal2DF0D computes normals based on
    underlying FEdges instead, which is inappropriate for strokes when
    they have already been modified by stroke geometry modifiers.

    The returned normals are dynamic: they update when the
    vertex position (and therefore the vertex normal) changes.
    for use in geometry modifiers it is advised to
    cast this generator function to a tuple or list
    """
    it = iter(stroke)
    yield from (normal_at_I0D(it) for _ in it)


def get_test_stroke():
    """Returns a static stroke object for testing """
    from freestyle.types import Stroke, Interface0DIterator, StrokeVertexIterator, SVertex, Id, StrokeVertex
    # points for our fake stroke
    points = (Vector((1.0, 5.0, 3.0)), Vector((1.0, 2.0, 9.0)),
              Vector((6.0, 2.0, 3.0)), Vector((7.0, 2.0, 3.0)),
              Vector((2.0, 6.0, 3.0)), Vector((2.0, 8.0, 3.0)))
    ids = (Id(0, 0), Id(1, 1), Id(2, 2), Id(3, 3), Id(4, 4), Id(5, 5))

    stroke = Stroke()
    it = iter(stroke)

    for svert in map(SVertex, points, ids):
        stroke.insert_vertex(StrokeVertex(svert), it)
        it = iter(stroke)

    stroke.update_length()
    return stroke
