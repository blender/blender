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

# constructs for definition of helper functions in Python
from freestyle.types import (
    StrokeVertexIterator,
    )
import mathutils


def stroke_normal(it):
    """
    Compute the 2D normal at the stroke vertex pointed by the iterator
    'it'.  It is noted that Normal2DF0D computes normals based on
    underlying FEdges instead, which is inappropriate for strokes when
    they have already been modified by stroke geometry modifiers.
    """
    # first stroke segment
    it_next = StrokeVertexIterator(it)
    it_next.increment()
    if it.is_begin:
        e = it_next.object.point_2d - it.object.point_2d
        n = mathutils.Vector((e[1], -e[0]))
        n.normalize()
        return n
    # last stroke segment
    it_prev = StrokeVertexIterator(it)
    it_prev.decrement()
    if it_next.is_end:
        e = it.object.point_2d - it_prev.object.point_2d
        n = mathutils.Vector((e[1], -e[0]))
        n.normalize()
        return n
    # two subsequent stroke segments
    e1 = it_next.object.point_2d - it.object.point_2d
    e2 = it.object.point_2d - it_prev.object.point_2d
    n1 = mathutils.Vector((e1[1], -e1[0]))
    n2 = mathutils.Vector((e2[1], -e2[0]))
    n1.normalize()
    n2.normalize()
    n = n1 + n2
    n.normalize()
    return n
