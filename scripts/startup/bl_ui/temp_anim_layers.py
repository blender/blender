# SPDX-FileCopyrightText: 2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""NOTE: this is temporary UI code to show animation layers.

It is not meant for any particular use, just to have *something* in the UI.
"""

import bpy
from bpy.types import (
    Panel,
    WindowManager,
)
from bpy.props import PointerProperty


class VIEW3D_PT_animation_layers(Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_category = "Animation"
    bl_label = "Animation Debug"

    @classmethod
    def poll(cls, context):
        return context.preferences.experimental.use_animation_baklava and context.object

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        col = layout.column(align=False)

        adt = context.object.animation_data
        anim = adt and adt.action
        if anim:
            slot_sub = col.column(align=True)
            slot_sub.template_search(
                adt, "action_slot",
                adt, "action_slots",
                new="",
                unlink="anim.slot_unassign_object",
            )

            internal_sub = slot_sub.box().column(align=True)
            internal_sub.active = False  # Just to dim.
            internal_sub.prop(adt, "action_slot_handle", text="handle")
            if adt.action_slot:
                internal_sub.prop(adt.action_slot, "name", text="Internal Name")

        if adt:
            col.prop(adt, "action_slot_name", text="ADT Slot Name")
        else:
            col.label(text="ADT Slot Name: -")


classes = (
    VIEW3D_PT_animation_layers,
)


if __name__ == "__main__":  # only for live edit.
    register_, _ = bpy.utils.register_classes_factory(classes)
    register_()
