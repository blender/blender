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

from mathutils import Vector


_MESH_ENTITIES = frozenset(["POLYFACE", "POLYMESH", "MESH", "POINT", "3DFACE", "SOLID", "TRACE"])


def mesh_entity(entity):
    return entity.dxftype in _MESH_ENTITIES


def mesh(typestr):
    return typestr in _MESH_ENTITIES

_POLYS = frozenset(["LWPOLYLINE", "POLYLINE"])

def closed_poly_no_bulge(entity):
    return entity.dxftype in _POLYS and not any([b != 0 for b in entity.bulge]) and entity.is_closed


_CURVE_ENTITIES = frozenset(("POLYLINE", "POLYGON", "LWPOLYLINE", "SPLINE",
                             "CIRCLE", "ARC", "ELLIPSE", "LINE", "HELIX"))

def curve_entity(entity):
    return entity.dxftype in _CURVE_ENTITIES


def curve(typestr):
    return typestr in _CURVE_ENTITIES


_NURBS_ENTITIES = frozenset(("BODY", "REGION", "PLANESURFACE", "SURFACE", "3DSOLID"))


def nurbs_entity(entity):
    return entity.dxftype in _NURBS_ENTITIES


def nurbs(typestr):
    return typestr in _NURBS_ENTITIES


_TEXT_ENTITIES = frozenset(("MTEXT", "TEXT"))


def text_entity(entity):
    return entity.dxftype in _TEXT_ENTITIES


def text(typestr):
    return typestr in _TEXT_ENTITIES


def insert_entity(entity):
    return entity.dxftype == "INSERT"


def insert(typestr):
    return typestr == "INSERT"


def light_entity(entity):
    return entity.dxftype == "LIGHT"


def light(typestr):
    return typestr == "LIGHT"


def attrib(entity):
    return entity.dxftype == "ATTDEF"


def attrib(typestr):
    return typestr == "ATTDEF"


_2D_ENTITIES = frozenset(("CIRCLE", "ARC", "SOLID", "TRACE", "TEXT", "ATTRIB", "ATTDEF", "SHAPE",
                          "INSERT", "LWPOLYLINE", "HATCH", "IMAGE", "ELLIPSE"))


def _2D_entity(entity):
    return entity.dxftype in _2D_ENTITIES or (entity.dxftype == "POLYGON" and entity.mode == "spline2d")


def varying_width(entity):
    if hasattr(entity, "width"):
        ew = entity.width
        if hasattr(ew, "__iter__"):
            if len(ew) == 0:
                return False
            else:
                return ew.count(ew[0]) != len(ew) or ew[0][0] != ew[0][1]
    return False


_SEPERATED_ENTITIES = frozenset(("POLYFACE", "POLYMESH", "LIGHT", "MTEXT", "TEXT", "INSERT", "BLOCK"))


def separated_entity(entity):
    """
    Indicates if the entity should be imported to one single Blender object or if it can be merged with other entities.
    This depends not only on the type of a dxf-entity but also whether the width values are varying or all the same.
    """
    return entity.dxftype in _SEPERATED_ENTITIES or varying_width(entity)


def separated(typestr):
    return typestr in _SEPERATED_ENTITIES


_NOT_COMBINED_ENTITIES = frozenset(tuple(_SEPERATED_ENTITIES) + ("ATTDEF",))


def combined_entity(entity):
    return not separated_entity(entity) and not entity.dxftype == "ATTDEF"


def combined(typestr):
    return typestr not in _NOT_COMBINED_ENTITIES


def extrusion(entity):
    if entity.extrusion is None:
        return False
    return Vector(entity.extrusion) != Vector((0, 0, 1)) \
                    or (hasattr(entity, "elevation") and entity.elevation != 0)