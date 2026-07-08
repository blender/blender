# SPDX-FileCopyrightText: 2009-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy
from bpy.types import (
    Panel,
    Operator,
)


class StripModButtonsPanel:
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "strip_modifier"

    @classmethod
    def poll(cls, context):
        return context.active_strip is not None


class STRIP_PT_modifiers(StripModButtonsPanel, Panel):
    bl_label = "Modifiers"
    bl_options = {'HIDE_HEADER'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        layout.operator("wm.call_menu", text="Add Modifier", icon='ADD').name = "SEQUENCER_MT_modifier_add"

        layout.template_strip_modifiers()


class AddStripModifierMenu(Operator):
    bl_idname = "sequencer.add_strip_modifier_menu"
    bl_label = "Add Modifier"

    @classmethod
    def poll(cls, context):
        # NOTE: This operator only exists to add a poll to the add modifier shortcut in the property editor.
        space = context.space_data
        return space and space.type == 'PROPERTIES' and space.context == 'STRIP_MODIFIER'

    def invoke(self, _context, _event):
        return bpy.ops.wm.call_menu(name="SEQUENCER_MT_modifier_add")


classes = (
    STRIP_PT_modifiers,
    AddStripModifierMenu,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
