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

from math import pi, sqrt
import bpy
import bmesh
from bpy.props import IntProperty, FloatProperty
from mathutils import Matrix, Vector

from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import updateNode, match_long_repeat
from sverchok.utils.sv_bmesh_utils import bmesh_from_pydata, pydata_from_bmesh

class SvSuzanneNode(bpy.types.Node, SverchCustomTreeNode):
    "Suzanne primitive"

    bl_idname = "SvSuzanneNode"
    bl_label = "Suzanne"
    bl_icon = "MONKEY"

    def sv_init(self, context):
        self.outputs.new('VerticesSocket', "Vertices")
        self.outputs.new('StringsSocket',  "Edges")
        self.outputs.new('StringsSocket',  "Faces")

    def process(self):
        # return if no outputs are connected
        if not any(s.is_linked for s in self.outputs):
            return

        out_verts = []
        out_edges = []
        out_faces = []

        bm = bmesh.new()
        bmesh.ops.create_monkey(bm)
        verts, edges, faces = pydata_from_bmesh(bm)
        bm.free()

        out_verts.append(verts)
        out_edges.append(edges)
        out_faces.append(faces)

        self.outputs['Vertices'].sv_set(out_verts)
        self.outputs['Edges'].sv_set(out_edges)
        self.outputs['Faces'].sv_set(out_faces)

def register():
    bpy.utils.register_class(SvSuzanneNode)

def unregister():
    bpy.utils.unregister_class(SvSuzanneNode)

