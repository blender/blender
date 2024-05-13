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

    def draw(self, context) -> None:
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        # FIXME: this should be done in response to a message-bus callback, notifier, whatnot.
        adt = context.object.animation_data
        with _wm_selected_action_lock:
            if adt:
                context.window_manager.selected_action = adt.action
            else:
                context.window_manager.selected_action = None

        col = layout.column()
        # This has to go via an auxiliary property, as assigning an Animation
        # data-block should be possible even when `context.object.animation_data`
        # is `None`, and thus its `animation` property does not exist.
        col.template_ID(context.window_manager, 'selected_action')

        col = layout.column(align=False)
        anim = adt and adt.action
        if anim:
            binding_sub = col.column(align=True)

            # Binding selector.
            row = binding_sub.row(align=True)
            row.prop(adt, 'action_binding', text="Binding")
            row.operator('anim.binding_unassign_object', text="", icon='X')

            binding = anim.bindings.get(adt.action_binding, None)
            if binding:
                binding_sub.prop(binding, 'name_display', text="Name")

            internal_sub = binding_sub.box().column(align=True)
            internal_sub.active = False
            internal_sub.prop(adt, 'action_binding_handle', text="handle")
            if binding:
                internal_sub.prop(binding, 'name', text="Internal Name")

        if adt:
            col.prop(adt, 'action_binding_name', text="ADT Binding Name")
        else:
            col.label(text="ADT Binding Name: -")

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
    # the pointed-to Animation data-block.
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
