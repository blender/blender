# SPDX-FileCopyrightText: 2021-2023 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

bl_info = {
    "name": "VR Scene Inspection",
    # This is now displayed as the maintainer, so show the foundation.
    # "author": "Julian Eisel (Severin), Sebastian Koenig, Peter Kim (muxed-reality)", # Original Authors
    "author": "Blender Foundation",
    "version": (0, 11, 2),
    "blender": (3, 2, 0),
    "location": "3D View > Sidebar > VR",
    "description": ("View the viewport with virtual reality glasses "
                    "(head-mounted displays)"),
    "support": "OFFICIAL",
    "doc_url": "{BLENDER_MANUAL_URL}/addons/3d_view/vr_scene_inspection.html",
    "category": "3D View",
}


if "bpy" in locals():
    import importlib
    importlib.reload(action_map)
    importlib.reload(gui)
    importlib.reload(operators)
    importlib.reload(properties)
    importlib.reload(preferences)
else:
    from . import action_map, gui, operators, properties, preferences

import bpy


def register():
    if not bpy.app.build_options.xr_openxr:
        bpy.utils.register_class(gui.VIEW3D_PT_vr_info)
        return

    action_map.register()
    gui.register()
    operators.register()
    properties.register()
    preferences.register()


def unregister():
    if not bpy.app.build_options.xr_openxr:
        bpy.utils.unregister_class(gui.VIEW3D_PT_vr_info)
        return

    action_map.unregister()
    gui.unregister()
    operators.unregister()
    properties.unregister()
    preferences.unregister()
