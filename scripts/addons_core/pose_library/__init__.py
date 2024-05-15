# SPDX-FileCopyrightText: 2021-2023 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
Pose Library based on the Asset Browser.
"""

bl_info = {
    "name": "Pose Library",
    "description": "Pose Library based on the asset system.",
    "author": "Sybren A. Stüvel, Julian Eisel",
    "version": (2, 0),
    "blender": (3, 0, 0),
    "location": "Asset Browser -> Animations, and 3D Viewport -> Animation panel",
    "doc_url": "{BLENDER_MANUAL_URL}/animation/armatures/posing/editing/pose_library.html",
    "support": "OFFICIAL",
    "category": "Animation",
}

from typing import List, Tuple

_need_reload = "operators" in locals()
from . import gui, keymaps, operators, conversion

if _need_reload:
    import importlib

    gui = importlib.reload(gui)
    keymaps = importlib.reload(keymaps)
    operators = importlib.reload(operators)
    conversion = importlib.reload(conversion)

import bpy

addon_keymaps: List[Tuple[bpy.types.KeyMap, bpy.types.KeyMapItem]] = []


def register() -> None:
    bpy.types.WindowManager.poselib_previous_action = bpy.props.PointerProperty(type=bpy.types.Action)

    operators.register()
    keymaps.register()
    gui.register()


def unregister() -> None:
    gui.unregister()
    keymaps.unregister()
    operators.unregister()

    try:
        del bpy.types.WindowManager.poselib_previous_action
    except AttributeError:
        pass
