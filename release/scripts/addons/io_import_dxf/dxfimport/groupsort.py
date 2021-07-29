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

# <pep8 compliant>

import itertools
from . import is_
from mathutils import Vector


def map_dxf_to_blender_type(TYPE):
    """
    TYPE: DXF entity type (String)
    """
    if is_.mesh(TYPE):
        return "object_mesh"
    if is_.curve(TYPE):
        return "object_curve"
    if is_.nurbs(TYPE):
        return "object_surface"
    else:
        print("groupsort: not mergeable type ", TYPE)
        return "not_mergeable"


def by_blender_type(entities):
    """
    entities: list of DXF entities
    """
    keyf = lambda e: map_dxf_to_blender_type(e.dxftype)
    return itertools.groupby(sorted(entities, key=keyf), key=keyf)


def by_layer(entities):
    """
    entities: list of DXF entities
    """
    keyf = lambda e: e.layer
    return itertools.groupby(sorted(entities, key=keyf), key=keyf)

def by_closed_poly_no_bulge(entities):
    """
    entities: list of DXF entities
    """
    keyf = lambda e: is_.closed_poly_no_bulge(e)
    return itertools.groupby(sorted(entities, key=keyf), key=keyf)


def by_dxftype(entities):
    """
    entities: list of DXF entities
    """
    keyf = lambda e: e.dxftype
    return itertools.groupby(sorted(entities, key=keyf), key=keyf)


def by_attributes(entities):
    """
    entities: list of DXF entities
    attributes: thickness and width occuring in curve types; subdivision_levels occuring in MESH dxf types
    """
    def attributes(entity):
        width = [(0, 0)]
        subd = -1
        extrusion = entity.extrusion
        if hasattr(entity, "width"):
            if any((w != 0 for ww in entity.width for w in ww)):
                width = entity.width
        if hasattr(entity, "subdivision_levels"):
            subd = entity.subdivision_levels
        if entity.dxftype in {"LINE", "POINT"}:
            extrusion = (0.0, 0.0, 1.0)
        return entity.thickness, subd, width, extrusion

    return itertools.groupby(sorted(entities, key=attributes), key=attributes)

def by_insert_block_name(inserts):
    """
    entities: list of DXF inserts
    """
    keyf = lambda e: e.name
    return itertools.groupby(sorted(inserts, key=keyf), key=keyf)
