# SPDX-FileCopyrightText: 2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""NOTE: this is temporary UI code to show animation layers.

It is not meant for any particular use, just to have *something* in the UI.
"""

import threading

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
    bl_label = "Baklava"

    @classmethod
    def poll(cls, context):
        return context.preferences.experimental.use_animation_baklava and context.object

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        # FIXME: this should be done in response to a message-bus callback, notifier, whatnot.
        adt = context.object.animation_data
        with _wm_selected_action_lock:
            selected_action = getattr(adt, "action", None)
            # Only set if it has to change, to avoid unnecessary notifies (that cause
            # a redraw, that cause this code to be called, etc.)
            if context.window_manager.selected_action != selected_action:
                context.window_manager.selected_action = selected_action

        col = layout.column()
        # This has to go via an auxiliary property, as assigning an Animation
        # data-block should be possible even when `context.object.animation_data`
        # is `None`, and thus its `animation` property does not exist.
        col.template_ID(context.window_manager, "selected_action")

        col = layout.column(align=False)
        anim = adt and adt.action
        if anim:
            slot_sub = col.column(align=True)

            # Slot selector.
            row = slot_sub.row(align=True)
            row.prop(adt, "action_slot", text="Slot")
            row.operator("anim.slot_unassign_object", text="", icon='X')

            slot = anim.slots.get(adt.action_slot, None)
            if slot:
                slot_sub.prop(slot, "name_display", text="Name")

            internal_sub = slot_sub.box().column(align=True)
            internal_sub.active = False
            internal_sub.prop(adt, "action_slot_handle", text="handle")
            if slot:
                internal_sub.prop(slot, "name", text="Internal Name")

        if adt:
            col.prop(adt, "action_slot_name", text="ADT Slot Name")
        else:
            col.label(text="ADT Slot Name: -")

        layout.separator()

        if not anim:
            layout.label(text="No layers")
            return

        for layer_idx, layer in reversed(list(enumerate(anim.layers))):
            layerbox = layout.box()
            col = layerbox.column(align=True)
            col.prop(layer, "name", text="Layer {:d}:".format(layer_idx + 1))
            col.prop(layer, "influence")
            col.prop(layer, "mix_mode")


classes = (
    VIEW3D_PT_animation_layers,
)

_wm_selected_action_lock = threading.Lock()


def _wm_selected_action_update(wm, context):
    # Avoid responding to changes written by the panel above.
    lock_ok = _wm_selected_action_lock.acquire(blocking=False)
    if not lock_ok:
        return
    try:
        if wm.selected_action is None and context.object.animation_data is None:
            return

        adt = context.object.animation_data_create()
        if adt.action == wm.selected_action:
            # Avoid writing to the property when the new value hasn't changed.
            return
        adt.action = wm.selected_action
    finally:
        _wm_selected_action_lock.release()


def register_props():
    # Due to this hackyness, the WindowManager will increase the user count of
    # the pointed-to Action.
    WindowManager.selected_action = PointerProperty(
        type=bpy.types.Action,
        name="Action",
        description="Action assigned to the active Object",
        update=_wm_selected_action_update,
    )


if __name__ == "__main__":  # only for live edit.
    register_, _ = bpy.utils.register_classes_factory(classes)
    register_()
    register_props()
