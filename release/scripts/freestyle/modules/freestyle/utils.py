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
Helper functions used for Freestyle style module writing
"""

# module members
from _freestyle import (
    ContextFunctions,
    getCurrentScene,
    integrate,
    )

from mathutils import Vector
from functools import lru_cache
from math import cos, sin, pi


# -- real utility functions  -- #


def rgb_to_bw(r, g, b):
    """ Method to convert rgb to a bw intensity value. """
    return 0.35 * r + 0.45 * g + 0.2 * b


def bound(lower, x, higher):
    """ Returns x bounded by a maximum and minimum value. equivalent to:
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


# -- helper functions for chaining -- #


def get_chain_length(ve, orientation):
    """Returns the 2d length of a given ViewEdge """
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
    """Finds the matching vertexn, or returns None """
    return next((ve for ve in it if ve.id == id), None)


# -- helper functions for iterating -- #


def iter_current_previous(stroke):
    """
    iterates over the given iterator. yields a tuple of the form
    (it, prev, current)
    """
    prev = stroke[0]
    it = Interface0DIterator(stroke)
    for current in it:
        yield (it, prev, current)


def iter_t2d_along_stroke(stroke):
    """
    Yields the distance between two stroke vertices
    relative to the total stroke length.
    """
    total = stroke.length_2d
    distance = 0.0
    for it, prev, svert in iter_current_previous(stroke):
        distance += (prev.point - svert.point).length
        t = min(distance / total, 1.0) if total > 0.0 else 0.0
        yield (it, t)


def iter_distance_from_camera(stroke, range_min, range_max):
    """
    Yields the distance to the camera relative to the maximum
    possible distance for every stroke vertex, constrained by
    given minimum and maximum values.
    """
    normfac = range_max - range_min  # normalization factor
    it = Interface0DIterator(stroke)
    for svert in it:
        distance = svert.point_3d.length  # in the camera coordinate
        if distance < range_min:
            t = 0.0
        elif distance > range_max:
            t = 1.0
        else:
            t = (distance - range_min) / normfac
        yield (it, t)


def iter_distance_from_object(stroke, object, range_min, range_max):
    """
    yields the distance to the given object relative to the maximum
    possible distance for every stroke vertex, constrained by
    given minimum and maximum values.
    """
    scene = getCurrentScene()
    mv = scene.camera.matrix_world.copy().inverted()  # model-view matrix
    loc = mv * object.location  # loc in the camera coordinate
    normfac = range_max - range_min  # normalization factor
    it = Interface0DIterator(stroke)
    for svert in it:
        distance = (svert.point_3d - loc).length # in the camera coordinate
        if distance < range_min:
            t = 0.0
        elif distance > range_max:
            t = 1.0
        else:
            t = (distance - range_min) / normfac
        yield (it, t)


def iter_material_color(stroke, material_attribute):
    """
    yields the specified material attribute for every stroke vertex.
    the material is taken from the object behind the vertex.
    """
    func = CurveMaterialF0D()
    it = Interface0DIterator(stroke)
    for inter in it:
        material = func(it)
        if material_attribute == 'DIFF':
            color = material.diffuse[0:3]
        elif material_attribute == 'SPEC':
            color = material.specular[0:3]
        else:
            raise ValueError("unexpected material attribute: " + material_attribute)
        yield (it, color)


def iter_material_value(stroke, material_attribute):
    """
    yields a specific material attribute
    from the vertex' underlying material.
    """
    func = CurveMaterialF0D()
    it = Interface0DIterator(stroke)
    for svert in it:
        material = func(it)
        if material_attribute == 'DIFF':
            t = rgb_to_bw(*material.diffuse[0:3])
        elif material_attribute == 'DIFF_R':
            t = material.diffuse[0]
        elif material_attribute == 'DIFF_G':
            t = material.diffuse[1]
        elif material_attribute == 'DIFF_B':
            t = material.diffuse[2]
        elif material_attribute == 'SPEC':
            t = rgb_to_bw(*material.specular[0:3])
        elif material_attribute == 'SPEC_R':
            t = material.specular[0]
        elif material_attribute == 'SPEC_G':
            t = material.specular[1]
        elif material_attribute == 'SPEC_B':
            t = material.specular[2]
        elif material_attribute == 'SPEC_HARDNESS':
            t = material.shininess
        elif material_attribute == 'ALPHA':
            t = material.diffuse[3]
        else:
            raise ValueError("unexpected material attribute: " + material_attribute)
        yield (it, t)


def iter_distance_along_stroke(stroke):
    """
    yields the absolute distance between
    the current and preceding vertex.
    """
    distance = 0.0
    prev = stroke[0]
    it = Interface0DIterator(stroke)
    for svert in it:
        p = svert.point
        distance += (prev - p).length
        prev = p.copy()  # need a copy because the point can be altered
        yield it, distance


def iter_triplet(it):
    """
    Iterates over it, yielding a tuple containing
    the current vertex and its immediate neighbors
    """
    prev = next(it)
    current = next(it)
    for succ in it:
        yield prev, current, succ
        prev, current = current, succ


# -- mathmatical operations -- #


def stroke_curvature(it):
    """
    Compute the 2D curvature at the stroke vertex pointed by the iterator 'it'.
    K = 1 / R
    where R is the radius of the circle going through the current vertex and its neighbors
    """

    if it.is_end or it.is_begin:
        return 0.0

    next = it.incremented().point
    prev = it.decremented().point
    current = it.object.point


    ab = (current - prev)
    bc = (next - current)
    ac = (prev - next)

    a, b, c = ab.length, bc.length, ac.length

    try:
        area = 0.5 * ab.cross(ac)
        K = (4 * area) / (a * b * c)
        K = bound(0.0, K, 1.0)

    except ZeroDivisionError:
        K = 0.0

    return K


def stroke_normal(it):
    """
    Compute the 2D normal at the stroke vertex pointed by the iterator
    'it'.  It is noted that Normal2DF0D computes normals based on
    underlying FEdges instead, which is inappropriate for strokes when
    they have already been modified by stroke geometry modifiers.
    """
    # first stroke segment
    it_next = it.incremented()
    if it.is_begin:
        e = it_next.object.point_2d - it.object.point_2d
        n = Vector((e[1], -e[0]))
        return n.normalized()
    # last stroke segment
    it_prev = it.decremented()
    if it_next.is_end:
        e = it.object.point_2d - it_prev.object.point_2d
        n = Vector((e[1], -e[0]))
        return n.normalized()
    # two subsequent stroke segments
    e1 = it_next.object.point_2d - it.object.point_2d
    e2 = it.object.point_2d - it_prev.object.point_2d
    n1 = Vector((e1[1], -e1[0])).normalized()
    n2 = Vector((e2[1], -e2[0])).normalized()
    n = (n1 + n2)
    return n.normalized()
