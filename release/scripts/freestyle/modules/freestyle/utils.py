# ##### BEGIN GPL LICENSE BLOCK #####
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software Foundation,
#  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ##### END GPL LICENSE BLOCK #####

"""
This module contains helper functions used for Freestyle style module
writing.
"""

__all__ = (
    "ContextFunctions",
    "bound",
    "bounding_box",
    "find_matching_vertex",
    "getCurrentScene",
    "get_chain_length",
    "get_test_stroke",
    "integrate",
    "iter_distance_along_stroke",
    "iter_distance_from_camera",
    "iter_distance_from_object",
    "iter_material_value",
    "iter_t2d_along_stroke",
    "pairwise",
    "phase_to_direction",
    "rgb_to_bw",
    "stroke_curvature",
    "stroke_normal",
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
    StrokeVertexIterator,
    )

from mathutils import Vector
from functools import lru_cache, namedtuple
from math import cos, sin, pi
from itertools import tee


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


def bounding_box(stroke):
    """
    Returns the maximum and minimum coordinates (the bounding box) of the stroke's vertices
    """
    x, y = zip(*(svert.point for svert in stroke))
    return (Vector((min(x), min(y))), Vector((max(x), max(y))))

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

# A named tuple primitive used for storing data that has an upper and
# lower bound (e.g., thickness, range and certain values)
BoundedProperty = namedtuple("BoundedProperty", ["min", "max", "delta"])

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
    "Yields a specific material attribute from the vertex' underlying material."
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
        # line seperate
        elif attribute == 'LINE_R':
            value = material.line[0]
        elif attribute == 'LINE_G':
            value = material.line[1]
        elif attribute == 'LINE_B':
            value = material.line[2]
        elif attribute == 'LINE_A':
            value = material.line[3]
        # diffuse seperate
        elif attribute == 'DIFF_R':
            value = material.diffuse[0]
        elif attribute == 'DIFF_G':
            value = material.diffuse[1]
        elif attribute == 'DIFF_B':
            value = material.diffuse[2]
        elif attribute == 'ALPHA':
            value = material.diffuse[3]
        # specular seperate
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
    "Yields the absolute distance along the stroke up to the current vertex."
    distance = 0.0
    # the positions need to be copied, because they are changed in the calling function
    points = tuple(svert.point.copy() for svert in stroke)
    yield distance
    for prev, curr in pairwise(points):
        distance += (prev - curr).length
        yield distance

# -- mathmatical operations -- #


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
    n = len(stroke) - 1

    for i, svert in enumerate(stroke):
        if i == 0:
            e = stroke[i + 1].point - svert.point
            yield Vector((e[1], -e[0])).normalized()
        elif i == n:
            e = svert.point - stroke[i - 1].point
            yield Vector((e[1], -e[0])).normalized()
        else:
            e1 = stroke[i + 1].point - svert.point
            e2 = svert.point - stroke[i - 1].point
            n1 = Vector((e1[1], -e1[0])).normalized()
            n2 = Vector((e2[1], -e2[0])).normalized()
            yield (n1 + n2).normalized()

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
