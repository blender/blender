# SPDX-FileCopyrightText: 2024 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

from bpy.types import Menu


class ANIM_MT_keyframe_insert_pie(Menu):
    bl_label = "Keyframe Insert Pie"

    def draw(self, context):
        layout = self.layout
        pie = layout.menu_pie()

        prop = pie.operator("anim.keyframe_insert_by_name", text="Location")
        prop.type = "Location"

        prop = pie.operator("anim.keyframe_insert_by_name", text="Scale")
        prop.type = "Scaling"

        prop = pie.operator("anim.keyframe_insert_by_name", text="Available")
        prop.type = "Available"

        prop = pie.operator("anim.keyframe_insert_by_name", text="Rotation")
        prop.type = "Rotation"


classes = (
    ANIM_MT_keyframe_insert_pie,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
