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
from bpy.props import FloatProperty, BoolProperty

from sverchok.node_tree import SverchCustomTreeNode


class FloatNode(bpy.types.Node, SverchCustomTreeNode):
    ''' Float '''
    bl_idname = 'FloatNode'
    bl_label = 'Float'
    bl_icon = 'OUTLINER_OB_EMPTY'

    # calling updateNode will trigger the update system
    # setting self.float_ will cause a recursive call to
    # update_value so after that we can return
    # this is also why updateNode shouldn't be called from
    # inside the update function, either directly
    # or indirectly by setting a property that will trigger another
    # update event
    
    def update_value(self, context):
        if self.float_ < self.minim:
            self.float_ = self.minim
            return  # recursion protection
        if self.float_ > self.maxim:
            self.float_ = self.maxim
            return  # recursion protection
        self.process_node(context)
        
    def update_max(self, context):
        if self.maxim < self.minim:
            self.maxim = self.minim + 1
            return
        if self.float_ > self.maxim:
            self.float_ = self.maxim
    
    def update_min(self, context):
        if self.minim > self.maxim:
            self.minim = self.maxim-1
            return 
        if self.float_ < self.minim:
            self.float_ = self.minim
    
    float_ = FloatProperty(name='Float', description='float number',
                           default=1.0,
                           options={'ANIMATABLE'}, update=update_value)
    maxim = FloatProperty(name='max', description='maximum',
                       default=1000,
                       update=update_max)
    minim = FloatProperty(name='min', description='minimum',
                       default=-1000,
                       update=update_min)
    to3d = BoolProperty(name='to3d', description='show in 3d panel',
                       default=True)

    def sv_init(self, context):
        self.inputs.new('StringsSocket', "Float").prop_name = 'float_'
        self.outputs.new('StringsSocket', "Float")

    def draw_buttons_ext(self, context, layout):
        row = layout.row(align=True)
        row.prop(self, 'minim')
        row.prop(self, 'maxim')
        row = layout.row(align=True)
        row.prop(self, 'to3d')

    def draw_label(self):
        if not self.inputs[0].links:
            return str(round(self.float_, 3))
        else:
            return self.bl_label
            
    def process(self):
        # inputs
        Float = min(max(float(self.inputs[0].sv_get()[0][0]), self.minim), self.maxim)
        # outputs
        self.outputs['Float'].sv_set([[Float]])

def register():
    bpy.utils.register_class(FloatNode)


def unregister():
    bpy.utils.unregister_class(FloatNode)



