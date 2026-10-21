# SPDX-FileCopyrightText: 2025 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

from bpy.types import Operator
from bpy.props import FloatProperty

from ..utils.nodes import (
    NWBase,
    nw_check,
    nw_check_selected,
)


#### ------------------------------ OPERATORS ------------------------------ ####

class NODE_OT_change_factor(Operator, NWBase):
    bl_idname = "node.nw_factor"
    bl_label = "Change Factor"
    bl_description = "Change factor inputs of selected nodes"
    bl_options = {'REGISTER', 'UNDO'}

    @staticmethod
    def get_socket_min_and_max(node, socket):
        if group := getattr(node, "node_tree", None):
            interface_item = group.interface.items_tree[socket.identifier]
            return interface_item.min_value, interface_item.max_value
        else:
            prop = socket.bl_rna.properties["default_value"]
            return prop.soft_min, prop.soft_max

    @classmethod
    def poll(cls, context):
        return nw_check(cls, context) and nw_check_selected(cls, context)

    # option: Change factor.
    # If option is 1.0 or 0.0 - set to 1.0 or 0.0
    # Else - change factor by option value.
    option: FloatProperty()

    def execute(self, context):
        option = self.option
        any_socket_modified = False

        if option in {0.0, 1.0}:
            for node in context.selected_nodes:
                if socket := node.inputs.get("Factor"):
                    socket.hide = False
                    if socket.default_value != option:
                        socket.default_value = option
                        any_socket_modified = True
        else:
            for node in context.selected_nodes:
                if socket := node.inputs.get("Factor") and socket.type == 'VALUE':
                    socket.hide = False
                    min_fac, max_fac = self.get_socket_min_and_max(node, socket)
                    if (option < 0.0) and (socket.default_value > min_fac):
                        socket.default_value = max(min_fac, socket.default_value + option)
                        any_socket_modified = True
                    elif (option > 0.0) and (socket.default_value < max_fac):
                        socket.default_value = min(max_fac, socket.default_value + option)
                        any_socket_modified = True

        if not any_socket_modified:
            self.report({'WARNING'}, "No factor values were modified.")
            return {'CANCELLED'}

        return {'FINISHED'}
