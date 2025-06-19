# SPDX-FileCopyrightText: 2025 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy
from bpy.types import Operator
from bpy.props import (
    EnumProperty,
)


class GREASE_PENCIL_OT_relative_layer_mask_add(Operator):
    """Mask active layer with layer above or below"""

    bl_idname = "grease_pencil.relative_layer_mask_add"
    bl_label = "Mask with Layer Above/Below"
    bl_options = {'REGISTER', 'UNDO'}

    mode: EnumProperty(
        name="Mode",
        items=(
            ('ABOVE', "Above", ""),
            ('BELOW', "Below", "")
        ),
        description="Which relative layer (above or below) to use as a mask",
        default='ABOVE',
    )

    @classmethod
    def poll(cls, context):
        return (
            (obj := context.active_object) is not None and
            obj.is_editable and
            obj.type == 'GREASEPENCIL' and
            obj.data.layers.active is not None and
            obj.data.is_editable
        )

    def execute(self, context):
        obj = context.active_object
        active_layer = obj.data.layers.active

        if self.mode == 'ABOVE':
            masking_layer = active_layer.next_node
        elif self.mode == 'BELOW':
            masking_layer = active_layer.prev_node

        if masking_layer is None or type(masking_layer) != bpy.types.GreasePencilLayer:
            self.report({'ERROR'}, "No layer found")
            return {'CANCELLED'}

        if masking_layer.name in active_layer.mask_layers:
            self.report({'ERROR'}, "Layer is already added as a mask")
            return {'CANCELLED'}

        bpy.ops.grease_pencil.layer_mask_add(name=masking_layer.name)
        active_layer.use_masks = True
        return {'FINISHED'}


classes = (
    GREASE_PENCIL_OT_relative_layer_mask_add,
)
