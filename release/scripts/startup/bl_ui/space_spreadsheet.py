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

        if len(space.context_path) == 0:
            self.draw_without_context_path(layout)
            return
        root_context = space.context_path[0]
        if root_context.type != 'OBJECT':
            self.draw_without_context_path(layout)
            return
        obj = root_context.object
        if obj is None:
            self.draw_without_context_path(layout)
            return

        layout.prop(space, "object_eval_state", text="")
        if space.object_eval_state != 'ORIGINAL':
            layout.prop(space, "geometry_component_type", text="")
        if space.geometry_component_type != 'INSTANCES':
            layout.prop(space, "attribute_domain", text="")

        context_path = space.context_path
        if space.object_eval_state == 'ORIGINAL':
            # Only show first context.
            context_path = context_path[:1]
        if space.display_context_path_collapsed:
            self.draw_collapsed_context_path(context, layout, context_path)
        else:
            self.draw_full_context_path(context, layout, context_path)

        pin_icon = 'PINNED' if space.is_pinned else 'UNPINNED'
        layout.operator("spreadsheet.toggle_pin", text="", icon=pin_icon, emboss=False)

        layout.separator_spacer()

        if isinstance(obj, bpy.types.Object) and obj.mode == 'EDIT':
            layout.prop(space, "show_only_selected", text="Selected Only")

    def draw_without_context_path(self, layout):
        layout.label(text="No active context")

    def draw_full_context_path(self, context, layout, context_path):
        space = context.space_data
        row = layout.row()
        for ctx in context_path[:-1]:
            subrow = row.row(align=True)
            self.draw_spreadsheet_context(subrow, ctx)
            self.draw_spreadsheet_context_path_icon(subrow, space)

        self.draw_spreadsheet_context(row, context_path[-1])

    def draw_collapsed_context_path(self, context, layout, context_path):
        space = context.space_data
        row = layout.row(align=True)
        self.draw_spreadsheet_context(row, context_path[0])
        if len(context_path) == 1:
            return
        self.draw_spreadsheet_context_path_icon(row, space)
        if len(context_path) > 2:
            self.draw_spreadsheet_context_path_icon(row, space, icon='DOT')
            self.draw_spreadsheet_context_path_icon(row, space)
        self.draw_spreadsheet_context(row, context_path[-1])

    def draw_spreadsheet_context(self, layout, ctx):
        if ctx.type == 'OBJECT':
            if ctx.object is None:
                layout.label(text="<no object>", icon='OBJECT_DATA')
            else:
                layout.label(text=ctx.object.name, icon='OBJECT_DATA')
        elif ctx.type == 'MODIFIER':
            layout.label(text=ctx.modifier_name, icon='MODIFIER')
        elif ctx.type == 'NODE':
            layout.label(text=ctx.node_name, icon='NODE')

    def draw_spreadsheet_context_path_icon(self, layout, space, icon='RIGHTARROW_THIN'):
        layout.prop(space, "display_context_path_collapsed", icon_only=True, emboss=False, icon=icon)

classes = (
    SPREADSHEET_HT_header,
)

if __name__ == "__main__":  # Only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
