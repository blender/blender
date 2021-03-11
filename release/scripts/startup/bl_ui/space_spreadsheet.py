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

import bpy


class SPREADSHEET_HT_header(bpy.types.Header):
    bl_space_type = 'SPREADSHEET'

    def draw(self, context):
        layout = self.layout
        space = context.space_data

        layout.template_header()

        pinned_id = space.pinned_id
        used_id = pinned_id if pinned_id else context.active_object

        layout.prop(space, "geometry_component_type", text="")
        layout.prop(space, "attribute_domain", text="")

        if used_id:
            layout.label(text=used_id.name, icon="OBJECT_DATA")

        layout.operator("spreadsheet.toggle_pin", text="", icon='PINNED' if pinned_id else 'UNPINNED', emboss=False)

        layout.separator_spacer()

        if isinstance(used_id, bpy.types.Object) and used_id.mode == 'EDIT':
            layout.prop(space, "show_only_selected", text="Selected Only")


classes = (
    SPREADSHEET_HT_header,
)

if __name__ == "__main__":  # Only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
