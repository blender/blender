# SPDX-FileCopyrightText: 2025 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

from __future__ import annotations

from bpy.types import Operator
from bpy.props import EnumProperty


class SCREEN_OT_edge_swipe(Operator):
    """Handle edge swipe (for touch screens)"""
    bl_idname = "screen.edge_swipe"
    bl_label = "Edge Swipe"
    bl_options = {'REGISTER'}
    direction: EnumProperty(
        name='Direction',
        description="Edge swipe direction",
        items=(
            ('IN_LEFT', 'In Left', 'Inward edge swipe from the left edge'),
            ('IN_RIGHT', 'In Right', 'Inward edge swipe from the right edge'),
        ),
        default='IN_LEFT',
        options={'SKIP_SAVE'},
    )

    @classmethod
    def poll(cls, context):
        # Only run in the 3D viewport
        if context.area is None or context.area.type != 'VIEW_3D':
            return False

        # Must be in fullscreen with panels hidden
        if not context.screen.is_focus_mode:
            return False

        return True

    def execute(self, context):
        space = context.space_data

        # Show the properties/tools region
        if self.direction == 'IN_RIGHT':
            space.show_region_ui = True
        else:
            space.show_region_toolbar = True
        return {'FINISHED'}


classes = (
    SCREEN_OT_edge_swipe,
)

if __name__ == "__main__":  # Only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
