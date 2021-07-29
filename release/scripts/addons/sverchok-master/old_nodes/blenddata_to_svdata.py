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


import bpy
from bpy.props import BoolProperty
from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import (updateNode)


class SvObjectToMeshNode(bpy.types.Node, SverchCustomTreeNode):
    bl_idname = 'SvObjectToMeshNode'
    bl_label = 'Object ID Out'
    bl_icon = 'OUTLINER_OB_EMPTY'

    modifiers = BoolProperty(name='Modifiers', default=False, update=updateNode)

    def sv_init(self, context):
        self.inputs.new('StringsSocket', "Objects")
        self.outputs.new('VerticesSocket', "Vertices", "Vertices")
        self.outputs.new('StringsSocket', "Edges", "Edges")
        self.outputs.new('StringsSocket', "Polygons", "Polygons")
        self.outputs.new('MatrixSocket', "Matrixes", "Matrixes")

    def draw_buttons(self, context, layout):
        row = layout.row()
        row.prop(self, "modifiers", text="Post modifiers")

    def process(self):
        objs = self.inputs[0].sv_get()
        if isinstance(objs[0], list):
            objs = objs[0]
        es,vs,ps,ms = [],[],[],[]
        scene, mod = bpy.context.scene, self.modifiers
        ot = objs[0].type in ['MESH', 'CURVE', 'FONT', 'SURFACE']
        for obj in objs:
            ms.append([m[:] for m in obj.matrix_world])
            if ot:
                obj_data = obj.to_mesh(scene, mod, 'PREVIEW')
                vs.append([v.co[:] for v in obj_data.vertices])
                es.append(obj_data.edge_keys)
                ps.append([p.vertices[:] for p in obj_data.polygons])
                bpy.data.meshes.remove(obj_data)
        for i,i2 in zip(self.outputs, [vs,es,ps,ms]):
            if i.is_linked:
                i.sv_set(i2)


def register():
    bpy.utils.register_class(SvObjectToMeshNode)


def unregister():
    bpy.utils.unregister_class(SvObjectToMeshNode)
