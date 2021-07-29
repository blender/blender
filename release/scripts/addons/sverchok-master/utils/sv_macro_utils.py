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

import re

import bpy
from bpy.props import StringProperty

import sverchok


# pylint: disable=w0141
# pylint: disable=w0123



def convert_string_to_settings(arguments):

    # expects       (varname=value,....)
    # for example   (selected_mode="int", fruits=20, alama=[0,0,0])
    def deform_args(**args):
        return args

    unsorted_dict = eval('deform_args{arguments}'.format(**vars()), locals(), locals())
    pattern = r'(\w+\s*)='
    results = re.findall(pattern, arguments)
    return [(varname, unsorted_dict[varname]) for varname in results]


class SvMacroInterpretter(bpy.types.Operator):
    """ Launch menu item as a macro """
    bl_idname = "node.sv_macro_interpretter"
    bl_label = "Sverchok check for new minor version"
    bl_options = {'REGISTER'}

    macro_bl_idname = StringProperty()
    settings = StringProperty()

    def create_node(self, context, node_type):
        space = context.space_data
        tree = space.edit_tree

        # select only the new node
        for n in tree.nodes:
            n.select = False

        node = tree.nodes.new(type=node_type)

        if self.settings:
            settings = convert_string_to_settings(self.settings)
            for name, value in settings:
                try:
                    setattr(node, name, value)
                except AttributeError as e:
                    self.report({'ERROR_INVALID_INPUT'}, "Node has no attribute " + name)
                    print(str(e))

        node.select = True
        tree.nodes.active = node
        node.location = space.cursor_location
        return node


    def execute(self, context):
        self.create_node(context, self.macro_bl_idname)    
        bpy.ops.node.translate_attach_remove_on_cancel('INVOKE_DEFAULT')
        return {'FINISHED'}


classes = (SvMacroInterpretter,)


def register():
    for class_name in classes:
        bpy.utils.register_class(class_name)


def unregister():
    for class_name in classes:
        bpy.utils.unregister_class(class_name)

