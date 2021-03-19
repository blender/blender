# ##### BEGIN GPL LICENSE BLOCK #####
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software Foundation,
#  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ##### END GPL LICENSE BLOCK #####

from __future__ import annotations

import bpy

class SPREADSHEET_OT_toggle_pin(bpy.types.Operator):
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

        if space.pinned_id:
            space.pinned_id = None
        else:
            space.pinned_id = context.active_object

        return {'FINISHED'}


classes = (
    SPREADSHEET_OT_toggle_pin,
)

if __name__ == "__main__":  # Only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
