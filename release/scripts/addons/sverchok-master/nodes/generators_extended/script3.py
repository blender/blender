# BEGIN GPL LICENSE BLOCK #####
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
# END GPL LICENSE BLOCK #####


import ast
import os
import traceback

import bpy
from bpy.props import (
    StringProperty,
    EnumProperty,
    BoolProperty,
    FloatVectorProperty,
    IntVectorProperty
)

from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import (
    updateNode,
)

from sverchok.utils.loadscript import SvScriptBase
import sverchok.utils.loadscript as loadscript

FAIL_COLOR = (0.8, 0.1, 0.1)
READY_COLOR = (0, 0.8, 0.95)



class SvScriptNodeGenericCallbackOp(bpy.types.Operator):
    ''' Used by ScriptNode Operators '''

    bl_idname = "node.sverchok_script3_callback"
    bl_label = "Sverchok scriptnode3 callback"
    bl_options = {'REGISTER', 'UNDO'}

    fn_name = StringProperty(default='')

    def execute(self, context):
        n = context.node
        module = getattr(sverchok.nodes.script, node.module, None)
        fn_name = self.fn_name

        f = getattr(module, fn_name, None)

        if not f:
            # fix message
            msg = "{0} has no function named '{1}'".format(n.name, fn_name)
            self.report({"WARNING"}, msg)
            return {'CANCELLED'}
        else:
            f(node, context)


        return {'FINISHED'}


class SvScriptNodeMK3(SvScriptBase, bpy.types.Node, SverchCustomTreeNode):

    ''' Script node loader'''
    bl_idname = 'SvScriptNodeMK3'
    bl_label = 'Script 3 Node'
    bl_icon = 'SCRIPTPLUGINS'


    is_loaded = BoolProperty()
    script_name = StringProperty()

    data_storage = StringProperty()

    def draw_label(self):
        func = self.func
        if func:
            return func.label
        else:
            return self.bl_label

    @property
    def func(self):
        module = loadscript._script_modules.get(self.script_name)
        if module:
            return module._func
        else:
            return None


    """
    needs to be adapated a bit then put in a mixin class and shared with monad
    """
    @property
    def input_template(self):
        if not self.data_storage:
            return []
        data = ast.literal_eval(self.data_storage)
        return data.get("input_template", {})

    @input_template.setter
    def input_template(self, value):
        if self.data_storage:
            data = ast.literal_eval(self.data_storage)
        else:
            data = {}
        data["input_template"] = value
        self.data_storage = str(data)
        self.inputs.clear()
        for socket_name, socket_bl_idname, _ in value:
            self.inputs.new(socket_bl_idname, socket_name)

    @property
    def output_template(self):
        if not self.data_storage:
            return []
        data = ast.literal_eval(self.data_storage)
        return data.get("output_template", [])

    @output_template.setter
    def output_template(self, value):
        if self.data_storage:
            data = ast.literal_eval(self.data_storage)
        else:
            data = {}
        data["output_template"] = value
        self.data_storage = str(data)
        self.outputs.clear()
        for socket_name, socket_bl_idname in value:
            self.outputs.new(socket_bl_idname, socket_name)


    def load(self):
        if not self.script_name:
            return

        loadscript.load_script(self.script_name)

        func = self.func
        if not func:
            raise ImportError("No script found")

        if hasattr(func, "cls"):
            cls = func.cls
            node = self.id_data.nodes.new(cls.bl_idname)
            node.location = self.location
            self.id_data.nodes.remove(self)
            return
        else:
            self.input_template = func._inputs_template
            self.output_template = func._outputs_template
            self.is_loaded = True

    def draw_buttons(self, context, layout):
        func = self.func
        if func and self.is_loaded:
            super().draw_buttons(context, layout)
        else:
            col = layout.column(align=True)
            row = col.row()
            row.prop_search(self, 'script_name', bpy.data, 'texts', text='', icon='TEXT')
            row.operator("node.sverchok_callback", text='', icon='PLUGIN').fn_name = 'load'




classes = [
    SvScriptNodeMK3,

]


def register():
    for class_name in classes:
        bpy.utils.register_class(class_name)


def unregister():
    for class_name in classes:
        bpy.utils.unregister_class(class_name)
