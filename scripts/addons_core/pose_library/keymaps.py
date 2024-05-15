# SPDX-FileCopyrightText: 2010-2023 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

from typing import List, Tuple

import bpy

addon_keymaps: List[Tuple[bpy.types.KeyMap, bpy.types.KeyMapItem]] = []


def register() -> None:
    wm = bpy.context.window_manager
    if wm.keyconfigs.addon is None:
        # This happens when Blender is running in the background.
        return

    km = wm.keyconfigs.addon.keymaps.new(name="File Browser Main", space_type="FILE_BROWSER")

    # DblClick to apply pose.
    kmi = km.keymap_items.new("poselib.apply_pose_asset", "LEFTMOUSE", "DOUBLE_CLICK")
    addon_keymaps.append((km, kmi))

    # Asset Shelf
    km = wm.keyconfigs.addon.keymaps.new(name="Asset Shelf")
    # Click to apply pose.
    kmi = km.keymap_items.new("poselib.apply_pose_asset", "LEFTMOUSE", "CLICK")
    addon_keymaps.append((km, kmi))
    # Drag to blend pose.
    kmi = km.keymap_items.new("poselib.blend_pose_asset", "LEFTMOUSE", "CLICK_DRAG")
    addon_keymaps.append((km, kmi))


def unregister() -> None:
    # Clear shortcuts from the keymap.
    for km, kmi in addon_keymaps:
        km.keymap_items.remove(kmi)
    addon_keymaps.clear()
