# SPDX-FileCopyrightText: 2023 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later
import bpy
from bpy.types import Panel


class DataButtonsPanel:
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "data"

    @classmethod
    def poll(cls, context):
        return hasattr(context, "grease_pencil") and context.grease_pencil


class DATA_PT_context_grease_pencil(DataButtonsPanel, Panel):
    bl_label = ""
    bl_options = {'HIDE_HEADER'}

    def draw(self, context):
        layout = self.layout

        ob = context.object
        grease_pencil = context.grease_pencil
        space = context.space_data

        if ob:
            layout.template_ID(ob, "data")
        elif grease_pencil:
            layout.template_ID(space, "pin_id")


class DATA_PT_grease_pencil_layers(DataButtonsPanel, Panel):
    bl_label = "Layers"

    def draw(self, context):
        layout = self.layout

        row = layout.row()
        row.template_grease_pencil_layer_tree()

        col = row.column()
        col.operator("grease_pencil.layer_add", icon='ADD', text="")
        col.operator("grease_pencil.layer_remove", icon='REMOVE', text="")


classes = (
    DATA_PT_context_grease_pencil,
    DATA_PT_grease_pencil_layers,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
