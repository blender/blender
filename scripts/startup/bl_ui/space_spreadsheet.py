# SPDX-FileCopyrightText: 2009-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy


class SPREADSHEET_HT_header(bpy.types.Header):
    bl_space_type = 'SPREADSHEET'

    def draw(self, context):
        layout = self.layout
        space = context.space_data

        layout.template_header()
        SPREADSHEET_MT_editor_menus.draw_collapsible(context, layout)
        layout.separator_spacer()

        row = layout.row(align=True)
        sub = row.row(align=True)
        sub.active = self._selection_filter_available(space)
        sub.prop(space, "show_only_selected", text="")
        row.prop(space, "use_filter", toggle=True, icon='FILTER', icon_only=True)

    @staticmethod
    def _selection_filter_available(space):
        path = space.viewer_path.path
        if not path:
            return False
        root_context = path[0]
        if root_context.type != 'ID':
            return False
        data_block = root_context.id
        if isinstance(data_block, bpy.types.Object):
            obj = data_block
            match obj.type:
                case 'MESH' | 'POINTCLOUD':
                    return obj.mode == 'EDIT'
                case 'CURVES':
                    return obj.mode in {'SCULPT_CURVES', 'EDIT'}
        return False


class SPREADSHEET_MT_editor_menus(bpy.types.Menu):
    bl_idname = "SPREADSHEET_MT_editor_menus"
    bl_label = ""

    def draw(self, context):
        del context
        layout = self.layout
        layout.menu("SPREADSHEET_MT_view")


class SPREADSHEET_MT_view(bpy.types.Menu):
    bl_label = "View"

    def draw(self, context):
        layout = self.layout
        sspreadsheet = context.space_data

        layout.prop(sspreadsheet, "show_region_toolbar")
        layout.prop(sspreadsheet, "show_region_ui")

        layout.separator()

        layout.prop(sspreadsheet, "show_internal_attributes", text="Internal Attributes")

        layout.separator()

        layout.menu("INFO_MT_area")


classes = (
    SPREADSHEET_HT_header,

    SPREADSHEET_MT_editor_menus,
    SPREADSHEET_MT_view,
)

if __name__ == "__main__":  # Only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
