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

from ast import literal_eval

import bpy
from bpy.props import BoolProperty, StringProperty, IntProperty

from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import updateNode, node_id, changable_sockets


class SvCacheNode(bpy.types.Node, SverchCustomTreeNode):
    '''Cache data Node'''
    bl_idname = 'SvCacheNode'
    bl_label = 'Cache'
    bl_icon = 'OUTLINER_OB_EMPTY'


    n_id = StringProperty()
    
    cache_amount = IntProperty(default=1, min=0)
    cache_offset = IntProperty(default=1, min=0)
    node_dict = {}
    
    def sv_init(self, context):
        self.inputs.new("StringsSocket", "Data")
        self.outputs.new("StringsSocket", "Data")

    def draw_buttons(self, context, layout):
        layout.prop(self, "cache_offset")

    def update(self):
        changable_sockets(self, "Data", ["Data"])
        
    def process(self):
        n_id = node_id(self)
        data = self.node_dict.get(n_id)
        if not data:
            self.node_dict[n_id] = {}
            data = self.node_dict.get(n_id)
            
        frame_current = bpy.context.scene.frame_current
        out_frame = frame_current - self.cache_offset
        data[frame_current] = self.inputs[0].sv_get()
        out_data = data.get(out_frame, [])
        self.outputs[0].sv_set(out_data)

def register():
    bpy.utils.register_class(SvCacheNode)

def unregister():
    bpy.utils.unregister_class(SvCacheNode)

