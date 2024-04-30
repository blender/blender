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
        with _wm_selected_animation_lock:
            if adt:
                context.window_manager.selected_animation = adt.animation
            else:
                context.window_manager.selected_animation = None

        col = layout.column()
        # This has to go via an auxiliary property, as assigning an Animation
        # data-block should be possible even when `context.object.animation_data`
        # is `None`, and thus its `animation` property does not exist.
        col.template_ID(context.window_manager, 'selected_animation')

        col = layout.column(align=False)
        anim = adt and adt.animation
        if anim:
            binding_sub = col.column(align=True)
            binding_sub.prop(adt, 'animation_binding_handle', text="Binding")
            binding = [o for o in anim.bindings if o.handle == adt.animation_binding_handle]
            if binding:
                binding_sub.prop(binding[0], 'name', text="Name")
                binding_sub.prop(binding[0], 'name_display', text="Display Name")
            else:
                col.label(text="AN Binding Name: -")

        if adt:
            col.prop(adt, 'animation_binding_name', text="ADT Binding Name")
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

_wm_selected_animation_lock = threading.Lock()


def _wm_selected_animation_update(wm, context):
    # Avoid responding to changes written by the panel above.
    lock_ok = _wm_selected_animation_lock.acquire(blocking=False)
    if not lock_ok:
        return
    try:
        if wm.selected_animation is None and context.object.animation_data is None:
            return

        adt = context.object.animation_data_create()
        if adt.animation == wm.selected_animation:
            # Avoid writing to the property when the new value hasn't changed.
            return
        adt.animation = wm.selected_animation
    finally:
        _wm_selected_animation_lock.release()


def register_props():
    # Put behind a `try` because it won't exist when Blender is built without
    # experimental features.
    try:
        from bpy.types import Animation
    except ImportError:
        return

    # Due to this hackyness, the WindowManager will increase the user count of
    # the pointed-to Animation data-block.
    WindowManager.selected_animation = PointerProperty(
        type=Animation,
        name="Animation",
        description="Animation assigned to the active Object",
        update=_wm_selected_animation_update,
    )


if __name__ == "__main__":  # only for live edit.
    register_, _ = bpy.utils.register_classes_factory(classes)
    register_()
    register_props()
