# SPDX-License-Identifier: GPL-2.0-or-later
import bpy
from bpy.types import (
    Panel,
)


class ASSETSHELF_PT_display(Panel):
    bl_label = "Display Settings"
    bl_space_type = 'PREFERENCES'
    bl_region_type = 'HEADER'

    def draw(self, context):
        layout = self.layout

        shelf_settings = context.asset_shelf_settings

        layout.prop(shelf_settings, "show_names", text="Names")

    @classmethod
    def poll(cls, context):
        return context.asset_shelf_settings is not None


classes = (
    ASSETSHELF_PT_display,
)


if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class

    for cls in classes:
        register_class(cls)
