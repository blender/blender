# SPDX-FileCopyrightText: 2025 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy
from bpy.types import Operator
from bpy.props import EnumProperty
from bpy.app.translations import contexts as i18n_contexts

from ..utils.constants import (
    blend_types,
    operations,
    navs,
)
from ..utils.nodes import (
    NWBase,
    nw_check,
    nw_check_selected,
    nw_check_space_type,
)


#### ------------------------------ OPERATORS ------------------------------ ####

class NODE_OT_batch_change(Operator, NWBase):
    bl_idname = "node.nw_batch_change"
    bl_label = "Batch Change"
    bl_description = "Batch change blend type and math operation"
    bl_options = {'REGISTER', 'UNDO'}

    blend_type: EnumProperty(
        name="Blend Type",
        items=blend_types + navs,
        translation_context=i18n_contexts.color,
    )
    operation: EnumProperty(
        name="Operation",
        translation_context=i18n_contexts.id_nodetree,
        items=operations + navs,
    )

    @classmethod
    def poll(cls, context):
        return (nw_check(cls, context)
                and nw_check_space_type(cls, context, {'ShaderNodeTree', 'CompositorNodeTree',
                                        'TextureNodeTree', 'GeometryNodeTree'})
                and nw_check_selected(cls, context))

    def execute(self, context):
        blend_type = self.blend_type
        operation = self.operation
        for node in context.selected_nodes:
            if node.type == 'MIX_RGB' or (node.bl_idname == 'ShaderNodeMix' and node.data_type == 'RGBA'):
                if blend_type not in [nav[0] for nav in navs]:
                    node.blend_type = blend_type
                else:
                    if blend_type == 'NEXT':
                        index = [i for i, entry in enumerate(blend_types) if node.blend_type in entry][0]
                        # index = blend_types.index(node.blend_type)
                        if index == len(blend_types) - 1:
                            node.blend_type = blend_types[0][0]
                        else:
                            node.blend_type = blend_types[index + 1][0]

                    if blend_type == 'PREV':
                        index = [i for i, entry in enumerate(blend_types) if node.blend_type in entry][0]
                        if index == 0:
                            node.blend_type = blend_types[len(blend_types) - 1][0]
                        else:
                            node.blend_type = blend_types[index - 1][0]

            if node.type == 'MATH' or node.bl_idname == 'ShaderNodeMath':
                if operation not in [nav[0] for nav in navs]:
                    node.operation = operation
                else:
                    if operation == 'NEXT':
                        index = [i for i, entry in enumerate(operations) if node.operation in entry][0]
                        # index = operations.index(node.operation)
                        if index == len(operations) - 1:
                            node.operation = operations[0][0]
                        else:
                            node.operation = operations[index + 1][0]

                    if operation == 'PREV':
                        index = [i for i, entry in enumerate(operations) if node.operation in entry][0]
                        # index = operations.index(node.operation)
                        if index == 0:
                            node.operation = operations[len(operations) - 1][0]
                        else:
                            node.operation = operations[index - 1][0]

        return {'FINISHED'}
