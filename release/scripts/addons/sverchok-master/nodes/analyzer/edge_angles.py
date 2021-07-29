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

import math
from copy import copy

import bpy
from bpy.props import BoolProperty, EnumProperty

from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import updateNode, match_long_repeat
from sverchok.utils.sv_bmesh_utils import bmesh_from_pydata, pydata_from_bmesh

def untangle_edges(orig_edges, bmesh_edges, angles):
    result = []
    edges = bmesh_edges[:]
    for orig_edge in orig_edges:
        i,e = [(i,e) for i,e in enumerate(edges) if set(v.index for v in e.verts) == set(orig_edge)][0]
        result.append(angles[i])
    return result

class SvEdgeAnglesNode(bpy.types.Node, SverchCustomTreeNode):
    '''Calculate angles between faces at edges'''
    bl_idname = 'SvEdgeAnglesNode'
    bl_label = 'Angles at the edges'
    bl_icon = 'OUTLINER_OB_EMPTY'

    signed = BoolProperty(name="Signed",
        description="Return negative angle for concave edges",
        default=False,
        update=updateNode)

    complement = BoolProperty(name="Complement",
        description="Return Pi (or 180) for angle between complanar faces, instead of 0",
        default=False,
        update=updateNode)

    angle_modes = [
            ("radians", "Radian", "Return angles in radians", 1),
            ("degrees", "Degree", "Return angles in degrees", 2)
        ]

    angles_mode = EnumProperty(items=angle_modes, default="radians", update=updateNode)

    degenerated_modes = [
            ("zero", "Zero", "Return zero angle", 1),
            ("pi",   "Pi",   "Return Pi as angle", 2),
            ("pi2",  "Pi/2", "Return Pi/2 as angle", 3),
            ("none", "None", "Return None as angle", 4),
            ("default", "Default", "Use value returned by bmesh", 6)
        ]

    degenerated_mode = EnumProperty(name="Wire/Boundary value",
        items=degenerated_modes,
        description="What to return as angle for wire or boundary edges",
        default="default",
        update=updateNode)

    def draw_buttons(self, context, layout):
        layout.prop(self, "signed")
        layout.prop(self, "complement")
        layout.prop(self, "angles_mode", expand=True)

    def draw_buttons_ext(self, context, layout):
        layout.prop(self, "degenerated_mode")

    def get_degenerated_angle(self, angle):
        if self.degenerated_mode == "zero":
            return 0.0
        elif self.degenerated_mode == "pi":
            return math.pi
        elif self.degenerated_mode == "pi2":
            return math.pi/2.0
        elif self.degenerated_mode == "default":
            return angle
        return None
    
    def sv_init(self, context):
        self.inputs.new('VerticesSocket', "Vertices")
        self.inputs.new('StringsSocket', "Edges")
        self.inputs.new('StringsSocket', "Polygons")

        self.outputs.new('StringsSocket', "Angles")

    def is_degenerated(self, edge):
        return (edge.is_wire or edge.is_boundary)

    def process(self):

        if not self.outputs['Angles'].is_linked:
            return

        vertices_s = self.inputs['Vertices'].sv_get(default=[[]])
        edges_s = self.inputs['Edges'].sv_get(default=[[]])
        faces_s = self.inputs['Polygons'].sv_get(default=[[]])

        result_angles = []

        meshes = match_long_repeat([vertices_s, edges_s, faces_s])
        for vertices, edges, faces in zip(*meshes):
            new_angles = []
            bm = bmesh_from_pydata(vertices, edges, faces)
            bm.normal_update()
            for edge in bm.edges:

                if self.signed:
                    angle = edge.calc_face_angle_signed()
                else:
                    angle = edge.calc_face_angle()

                if self.complement:
                    angle = math.copysign(math.pi, angle) - angle

                if self.is_degenerated(edge):
                    angle = self.get_degenerated_angle(angle)

                if self.angles_mode == "degrees" and angle is not None:
                    angle = math.degrees(angle)
                new_angles.append(angle)

            if edges:
                new_angles = untangle_edges(edges, bm.edges, new_angles)

            result_angles.append(new_angles)

        if self.outputs['Angles'].is_linked:
            self.outputs['Angles'].sv_set(result_angles)

def register():
    bpy.utils.register_class(SvEdgeAnglesNode)

def unregister():
    bpy.utils.unregister_class(SvEdgeAnglesNode)

