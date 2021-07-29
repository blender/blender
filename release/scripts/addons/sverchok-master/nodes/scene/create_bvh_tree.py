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
import bmesh
from bpy.props import EnumProperty
from mathutils.bvhtree import BVHTree
from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import (updateNode, enum_item as e)


class SvBVHtreeNode(bpy.types.Node, SverchCustomTreeNode):
    ''' BVH Tree '''
    bl_idname = 'SvBVHtreeNode'
    bl_label = 'BVH Tree In'
    bl_icon = 'OUTLINER_OB_EMPTY'

    def mode_change(self, context):
        inputs = self.inputs
        while len(inputs) > 1:
            inputs.remove(inputs[-1])
        if self.Mod == "FromObject":
            inputs[0].name= "Objects"
            inputs[0].hide= 0
        elif self.Mod == "FromBMesh":
            inputs[0].name= "bmesh_list"
            inputs[0].hide= 0
        else:
            inputs[0].hide= 1
            inputs.new('VerticesSocket', 'Verts')
            inputs.new('StringsSocket', 'Polys')

    Modes = ['FromObject','FromBMesh','FromSVdata']
    Mod = EnumProperty(name="getmodes", default=Modes[0], items=e(Modes), update=mode_change)

    def sv_init(self, context):
        self.inputs.new('StringsSocket', 'Objects')
        self.outputs.new('StringsSocket', 'BVHtree_list')

    def draw_buttons(self, context, layout):
        layout.prop(self, "Mod", "Get")

    def process(self):
        bvh = []
        if self.Mod == "FromObject":
            for i in self.inputs[0].sv_get():
                bvh.append(BVHTree.FromObject(i, bpy.context.scene, deform=True, render=False, cage=False, epsilon=0.0))
        elif self.Mod == "FromBMesh":
            for i in self.inputs[0].sv_get():
                bvh.append(BVHTree.FromBMesh(i))
        else:
            for i,i2 in zip(self.inputs[1].sv_get(),self.inputs[2].sv_get()):
                bvh.append(BVHTree.FromPolygons(i, i2, all_triangles=False, epsilon=0.0))
        self.outputs[0].sv_set(bvh)

    def update_socket(self, context):
        self.update()


def register():
    bpy.utils.register_class(SvBVHtreeNode)


def unregister():
    bpy.utils.unregister_class(SvBVHtreeNode)
