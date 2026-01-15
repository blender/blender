# SPDX-FileCopyrightText: 2024 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

bl_info = {
    "name": "AI Assistant",
    "author": "Blender Foundation",
    "version": (1, 0, 0),
    "blender": (4, 0, 0),
    "location": "3D View > Sidebar > AI (Press N, then AI tab)",
    "description": "Chat with AI to control Blender",
    "support": "OFFICIAL",
    "category": "3D View",
}

if "bpy" in locals():
    import importlib
    importlib.reload(properties)
    importlib.reload(ai_backend)
    importlib.reload(operators)
    importlib.reload(gui)
else:
    from . import properties, ai_backend, operators, gui

import bpy


addon_keymaps = []


def open_ai_sidebar():
    """Open sidebar and switch to AI tab on startup."""
    for window in bpy.context.window_manager.windows:
        for area in window.screen.areas:
            if area.type == 'VIEW_3D':
                for region in area.regions:
                    if region.type == 'UI':
                        # Toggle sidebar open if closed
                        with bpy.context.temp_override(window=window, area=area, region=region):
                            if area.spaces[0].show_region_ui == False:
                                bpy.ops.wm.context_toggle(data_path="space_data.show_region_ui")
                        break
    return None  # Don't repeat


def register():
    properties.register()
    ai_backend.register()
    operators.register()
    gui.register()

    # Add keyboard shortcut: Ctrl+Shift+A to toggle sidebar
    wm = bpy.context.window_manager
    kc = wm.keyconfigs.addon
    if kc:
        km = kc.keymaps.new(name='3D View', space_type='VIEW_3D')
        kmi = km.keymap_items.new(
            'wm.context_toggle',
            'A', 'PRESS',
            ctrl=True, shift=True
        )
        kmi.properties.data_path = "space_data.show_region_ui"
        addon_keymaps.append((km, kmi))

    # Open sidebar on first load (delayed)
    bpy.app.timers.register(open_ai_sidebar, first_interval=0.5)


def unregister():
    # Remove keymaps
    for km, kmi in addon_keymaps:
        km.keymap_items.remove(kmi)
    addon_keymaps.clear()

    gui.unregister()
    operators.unregister()
    ai_backend.unregister()
    properties.unregister()


if __name__ == "__main__":
    register()
