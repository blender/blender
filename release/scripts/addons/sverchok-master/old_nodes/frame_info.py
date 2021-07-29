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

from sverchok.node_tree import SverchCustomTreeNode


class SvFrameInfoNode(bpy.types.Node, SverchCustomTreeNode):
    ''' Frame Info '''
    bl_idname = 'SvFrameInfoNode'
    bl_label = 'Frame info'
    bl_icon = 'OUTLINER_OB_EMPTY'

    def sv_init(self, context):
        self.outputs.new('StringsSocket', "Current Frame", "Current Frame")
        self.outputs.new('StringsSocket', "Start Frame", "Start Frame")
        self.outputs.new('StringsSocket', "End Frame", "End Frame")

    def process(self):
        # outputs
        scene = bpy.context.scene
        if self.outputs['Current Frame'].is_linked:
            self.outputs['Current Frame'].sv_set([[scene.frame_current]])
        if self.outputs['Start Frame'].is_linked:
            self.outputs['Start Frame'].sv_set([[scene.frame_start]])
        if self.outputs['End Frame'].is_linked:
            self.outputs['End Frame'].sv_set([[scene.frame_end]])


def register():
    bpy.utils.register_class(SvFrameInfoNode)


def unregister():
    bpy.utils.unregister_class(SvFrameInfoNode)
