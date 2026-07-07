# SPDX-FileCopyrightText: 2025 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy
from bpy.types import Operator
from bpy.props import StringProperty

from ..utils.nodes import (
    NWBase,
    nw_check,
    nw_check_selected,
    get_nodes_links,
)


#### ------------------------------ OPERATORS ------------------------------ ####

class NODE_OT_labels_modify(Operator, NWBase):
    """Modify labels of all selected nodes"""
    bl_idname = "node.nw_modify_labels"
    bl_label = "Modify Labels"
    bl_options = {'REGISTER', 'UNDO'}

    prepend: StringProperty(
        name="Add to Beginning"
    )
    append: StringProperty(
        name="Add to End"
    )
    replace_from: StringProperty(
        name="Text to Replace"
    )
    replace_to: StringProperty(
        name="Replace with"
    )

    @classmethod
    def poll(cls, context):
        return nw_check(cls, context) and nw_check_selected(cls, context)

    def execute(self, context) -> set[str]:
        nodes, links = get_nodes_links(context)
        for node in [n for n in nodes if n.select]:
            node.label = self.prepend + node.label.replace(self.replace_from, self.replace_to) + self.append

        return {'FINISHED'}

    def invoke(self, context, event):
        self.prepend = ""
        self.append = ""
        self.remove = ""
        return context.window_manager.invoke_props_dialog(self)
