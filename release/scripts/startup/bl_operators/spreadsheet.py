# SPDX-License-Identifier: GPL-2.0-or-later

from __future__ import annotations

from bpy.types import Operator


class SPREADSHEET_OT_toggle_pin(Operator):
    '''Turn on or off pinning'''
    bl_idname = "spreadsheet.toggle_pin"
    bl_label = "Toggle Pin"
    bl_options = {'REGISTER'}

    @classmethod
    def poll(cls, context):
        space = context.space_data
        return space and space.type == 'SPREADSHEET'

    def execute(self, context):
        space = context.space_data

        if space.is_pinned:
            self.unpin(context)
        else:
            self.pin(context)
        return {'FINISHED'}

    def pin(self, context):
        space = context.space_data
        space.is_pinned = True

    def unpin(self, context):
        space = context.space_data
        space.is_pinned = False


classes = (
    SPREADSHEET_OT_toggle_pin,
)

if __name__ == "__main__":  # Only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
