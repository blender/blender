# SPDX-FileCopyrightText: 2025 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

from bpy.types import Operator
from bpy.props import BoolProperty

from ..utils.nodes import (
    NWBase,
    nw_check,
    nw_check_selected,
)


#### ------------------------------ OPERATORS ------------------------------ ####

class NODE_OT_labels_clear(Operator, NWBase):
    bl_idname = "node.nw_clear_label"
    bl_label = "Clear Label"
    bl_options = {'REGISTER', 'UNDO'}
    bl_description = "Clear labels on selected nodes"

    option: BoolProperty()

    @classmethod
    def poll(cls, context):
        return nw_check(cls, context) and nw_check_selected(cls, context)

    def execute(self, context):
        tree = context.space_data.edit_tree
        nodes = tree.nodes
        for node in [n for n in nodes if n.select]:
            node.label = ''

        return {'FINISHED'}

    def invoke(self, context, event):
        if self.option:
            return self.execute(context)
        else:
            return context.window_manager.invoke_confirm(self, event)
