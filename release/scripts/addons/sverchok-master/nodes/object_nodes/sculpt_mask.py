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
import numpy as np
from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import second_as_first_cycle as safc


class SvSculptMaskNode(bpy.types.Node, SverchCustomTreeNode):
    ''' Sculpt Mask '''
    bl_idname = 'SvSculptMaskNode'
    bl_label = 'Vertex Sculpt Masking'
    bl_icon = 'OUTLINER_OB_EMPTY'

    def sv_init(self, context):
        self.inputs.new('SvObjectSocket', "Object")
        self.inputs.new('StringsSocket',  "Sculpt Mask")

    def process(self):
        Objs, W = self.inputs
        MW = np.clip(W.sv_get()[0], 0, 1)
        bm = bmesh.new()
        for obj in Objs.sv_get():
            Om = obj.data
            bm.from_mesh(Om)
            if not bm.verts.layers.paint_mask:
                m = bm.verts.layers.paint_mask.new()
            else:
                m = bm.verts.layers.paint_mask[0]
            for i, i2 in zip(bm.verts, safc(bm.verts, MW)):
                i[m] = i2
            bm.to_mesh(Om)
            bm.clear()
            Om.update()


def register():
    bpy.utils.register_class(SvSculptMaskNode)


def unregister():
    bpy.utils.unregister_class(SvSculptMaskNode)
