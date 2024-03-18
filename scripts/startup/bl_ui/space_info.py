# SPDX-FileCopyrightText: 2009-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

from bpy.types import Header, Menu

from bpy.app.translations import contexts as i18n_contexts


class INFO_HT_header(Header):
    bl_space_type = 'INFO'

    def draw(self, context):
        layout = self.layout
        layout.template_header()

        INFO_MT_editor_menus.draw_collapsible(context, layout)


class INFO_MT_editor_menus(Menu):
    bl_idname = "INFO_MT_editor_menus"
    bl_label = ""

    def draw(self, _context):
        layout = self.layout
        layout.menu("INFO_MT_view")
        layout.menu("INFO_MT_info")


class INFO_MT_view(Menu):
    bl_label = "View"

    def draw(self, _context):
        layout = self.layout

        layout.menu("INFO_MT_area")


class INFO_MT_info(Menu):
    bl_label = "Info"

    def draw(self, _context):
        layout = self.layout

        layout.operator("info.select_all", text="Select All").action = 'SELECT'
        layout.operator("info.select_all", text="Deselect All").action = 'DESELECT'
        layout.operator("info.select_all", text="Invert Selection").action = 'INVERT'
        layout.operator("info.select_all", text="Toggle Selection").action = 'TOGGLE'

        layout.separator()

        layout.operator("info.select_box")

        layout.separator()

        # Disabled because users will likely try this and find
        # it doesn't work all that well in practice.
        # Mainly because operators needs to run in the right context.

        # layout.operator("info.report_replay")
        # layout.separator()

        layout.operator("info.report_delete", text="Delete")
        layout.operator("info.report_copy", text="Copy")


class INFO_MT_area(Menu):
    bl_label = "Area"
    bl_translation_context = i18n_contexts.id_windowmanager

    def draw(self, context):
        layout = self.layout

        if context.space_data.type == 'VIEW_3D':
            layout.operator("screen.region_quadview")
            layout.separator()

        layout.operator("screen.area_split", icon='SPLIT_HORIZONTAL', text="Horizontal Split").direction = 'HORIZONTAL'
        layout.operator("screen.area_split", icon='SPLIT_VERTICAL', text="Vertical Split").direction = 'VERTICAL'

        layout.separator()

        layout.operator("screen.screen_full_area")
        layout.operator(
            "screen.screen_full_area",
            text="Toggle Fullscreen Area").use_hide_panels = True
        layout.operator("screen.area_dupli")

        layout.separator()

        layout.operator("screen.area_close")


class INFO_MT_context_menu(Menu):
    bl_label = "Info"

    def draw(self, _context):
        layout = self.layout

        layout.operator("info.report_copy", text="Copy")
        layout.operator("info.report_delete", text="Delete")


classes = (
    INFO_HT_header,
    INFO_MT_editor_menus,
    INFO_MT_area,
    INFO_MT_view,
    INFO_MT_info,
    INFO_MT_context_menu,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
