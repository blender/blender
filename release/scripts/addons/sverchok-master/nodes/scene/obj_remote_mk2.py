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
from mathutils import Matrix

from sverchok.node_tree import SverchCustomTreeNode, MatrixSocket
from sverchok.sockets import SvObjectSocket
from sverchok.data_structure import dataCorrect, updateNode, match_long_repeat


class SvObjRemoteNodeMK2(bpy.types.Node, SverchCustomTreeNode):

    bl_idname = 'SvObjRemoteNodeMK2'
    bl_label = 'Object Remote (Control) mk2'
    bl_icon = 'OUTLINER_OB_EMPTY'

    def sv_init(self, context):
        self.inputs.new('MatrixSocket', 'matrices')
        self.inputs.new('SvObjectSocket', 'objects')
        self.outputs.new('SvObjectSocket', 'objects')

    def draw_buttons(self, context, layout):
        pass

    def process(self):
        if not self.inputs[1] and not self.inputs[1].is_linked:
            return

        matrices = dataCorrect(self.inputs[0].sv_get())
        objects = self.inputs[1].sv_get()
        matrices, objects = match_long_repeat([matrices, objects])
        for obj, mat in zip(objects, matrices):
            setattr(obj, 'matrix_world', Matrix(mat))

        if self.outputs[0].is_linked:
            self.outputs[0].sv_set(objects)


def register():
    bpy.utils.register_class(SvObjRemoteNodeMK2)


def unregister():
    bpy.utils.unregister_class(SvObjRemoteNodeMK2)
