# SPDX-FileCopyrightText: 2025 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy
from bpy.types import Operator
from bpy.props import StringProperty

from ..utils.nodes import (
    NWBase,
    nw_check,
    nw_check_space_type,
    get_nodes_links,
)


#### ------------------------------ OPERATORS ------------------------------ ####

class NODE_OT_add_attribute_node(Operator, NWBase):
    """Add an Attribute node with this name"""
    bl_idname = 'node.nw_add_attr_node'
    bl_label = 'Add Attribute'
    bl_options = {'REGISTER', 'UNDO'}

    attr_name: StringProperty()

    @classmethod
    def poll(cls, context):
        return nw_check(cls, context) and nw_check_space_type(cls, context, {'ShaderNodeTree'})

    def execute(self, context):
        bpy.ops.node.add_node('INVOKE_DEFAULT', use_transform=True, type="ShaderNodeAttribute")
        nodes, links = get_nodes_links(context)
        nodes.active.attribute_name = self.attr_name
        return {'FINISHED'}
