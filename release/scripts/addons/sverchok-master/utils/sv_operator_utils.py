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
from bpy.props import StringProperty

class SvGenericCallbackOldOp(bpy.types.Operator):
    """ 
    This operator is generic and will call .fn_name on the instance of the caller node
    """
    bl_idname = "node.sverchok_generic_callback_old"
    bl_label = "Sverchok text input"
    bl_options = {'REGISTER', 'UNDO'}

    fn_name = StringProperty(name='function name')

    # this information is not communicated unless you trigger it from a node
    # in the case the operator button appears on a 3dview panel, it will need to pass these too.
    tree_name = StringProperty(default='')
    node_name = StringProperty(default='')

    def get_node(self, context):
        """ context.node is usually provided, else tree_name/node_name must be passed """
        if self.tree_name and self.node_name:
            return bpy.data.node_groups[self.tree_name].nodes[self.node_name]

        return context.node


    def execute(self, context):
        n = self.get_node(context)

        f = getattr(n, self.fn_name, None)
        if not f:
            msg = "{0} has no function named '{1}'".format(n.name, self.fn_name)
            self.report({"WARNING"}, msg)
            return {'CANCELLED'}
        f()

        return {'FINISHED'}


def register():
    bpy.utils.register_class(SvGenericCallbackOldOp)


def unregister():
    bpy.utils.unregister_class(SvGenericCallbackOldOp)

