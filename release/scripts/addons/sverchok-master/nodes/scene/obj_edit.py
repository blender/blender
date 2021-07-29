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
from bpy.props import BoolProperty, StringProperty, EnumProperty

import sverchok
from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import updateNode


import json

class SvObjEditCallback(bpy.types.Operator):
    """ """
    bl_idname = "node.sverchok_objectedit_cb"
    bl_label = "Sverchok object in lite callback"
    bl_options = {'REGISTER', 'UNDO'}

    cmd = StringProperty()
    mode = StringProperty()

    def execute(self, context):
        getattr(context.node, self.cmd)(self, self.mode)
        return {'FINISHED'}


class SvObjEdit(bpy.types.Node, SverchCustomTreeNode):
    ''' Objects Set Edit/Object Mode'''
    bl_idname = 'SvObjEdit'
    bl_label = 'Obj Edit mode'
    bl_icon = 'OUTLINER_OB_EMPTY'

    obj_passed_in = StringProperty()

    def set_edit(self, ops, mode):
        try:
            obj_name = self.obj_passed_in or self.inputs[0].object_ref
            bpy.context.scene.objects.active = bpy.data.objects.get(obj_name)
            bpy.ops.object.mode_set(mode=mode)
        except:
            ops.report({'WARNING'}, 'No object selected / active')


    def sv_init(self, context):
        inputsocket = self.inputs.new
        inputsocket('SvObjectSocket', 'Objects')

    def draw_buttons(self, context, layout):

        if not (self.inputs and self.inputs[0]):
            return

        addon = context.user_preferences.addons.get(sverchok.__name__)
        prefs = addon.preferences
        callback = 'node.sverchok_objectedit_cb'

        col = layout.column(align=True)
        row = col.row(align=True)
        row.scale_y = 4.0 if prefs.over_sized_buttons else 1

        objects = bpy.data.objects
        if self.obj_passed_in or self.inputs[0].object_ref:
            obj = objects.get(self.obj_passed_in) or objects.get(self.inputs[0].object_ref)
            if obj:
                button_data, new_mode = {
                    'OBJECT': [dict(icon='EDITMODE_HLT', text='Edit'), 'EDIT'],
                    'EDIT': [dict(icon='OBJECT_DATAMODE', text='Object'), 'OBJECT']
                }.get(obj.mode)

                op1 = row.operator(callback, **button_data)
                op1.cmd = 'set_edit'
                op1.mode = new_mode


    def process(self):
        obj_socket = self.inputs[0]
        not_linked = not obj_socket.is_linked
        
        self.obj_passed_in = ''

        if not_linked:
            self.obj_passed_in = obj_socket.object_ref
        else:
            objlist = obj_socket.sv_get()
            if objlist and len(objlist) == 1:
                self.obj_passed_in = objlist[0].name


def register():
    bpy.utils.register_class(SvObjEditCallback)
    bpy.utils.register_class(SvObjEdit)


def unregister():
    bpy.utils.unregister_class(SvObjEditCallback)
    bpy.utils.unregister_class(SvObjEdit)
