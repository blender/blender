# SPDX-License-Identifier: GPL-2.0-or-later
import bpy
from bpy.types import Header, Menu, Panel
from bpy.app.translations import pgettext_iface as iface_
from bl_ui.utils import CenterAlignMixIn


# -----------------------------------------------------------------------------
# Main Header

class PROJECTSETTINGS_HT_header(Header):
    bl_space_type = 'PROJECT_SETTINGS'

    @staticmethod
    def draw_buttons(layout, context):
        layout.operator_context = 'EXEC_AREA'
        is_dirty = True

        # Show '*' to let users know the settings have been modified.
        # TODO, wrong operator
        layout.operator(
            "wm.save_project_settings",
            text=iface_("Save Settings") + (" *" if is_dirty else ""),
            translate=False,
        )

    def draw(self, context):
        layout = self.layout
        layout.operator_context = 'EXEC_AREA'

        layout.template_header()

        PROJECTSETTINGS_MT_editor_menus.draw_collapsible(context, layout)

        layout.separator_spacer()

        self.draw_buttons(layout, context)


# -----------------------------------------------------------------------------
# Main Navigation Bar

class PROJECTSETTINGS_PT_navigation_bar(Panel):
    bl_label = "Project Settings Navigation"
    bl_space_type = 'PROJECT_SETTINGS'
    bl_region_type = 'NAVIGATION_BAR'
    bl_options = {'HIDE_HEADER'}

    def draw(self, context):
        layout = self.layout

        space_data = context.space_data

        col = layout.column()

        col.scale_x = 1.3
        col.scale_y = 1.3
        col.prop(space_data, "active_section", expand=True)


class PROJECTSETTINGS_MT_editor_menus(Menu):
    bl_idname = "PROJECTSETTINGS_MT_editor_menus"
    bl_label = ""

    def draw(self, _context):
        layout = self.layout
        layout.menu("PROJECTSETTINGS_MT_view")


class PROJECTSETTINGS_MT_view(Menu):
    bl_label = "View"

    def draw(self, _context):
        layout = self.layout

        layout.menu("INFO_MT_area")


class PROJECTSETTINGS_PT_save_project_settings(Panel):
    bl_label = "Save Project Settings"
    bl_space_type = 'PROJECT_SETTINGS'
    bl_region_type = 'EXECUTE'
    bl_options = {'HIDE_HEADER'}

    @classmethod
    def poll(cls, context):
        # Hide when header is visible
        for region in context.area.regions:
            if region.type == 'HEADER' and region.height <= 1:
                return True

        return False

    def draw(self, context):
        layout = self.layout.row()
        layout.operator_context = 'EXEC_AREA'

        PROJECTSETTINGS_HT_header.draw_buttons(layout, context)

class PROJECTSETTINGS_PT_setup(CenterAlignMixIn, Panel):
    bl_space_type = 'PROJECT_SETTINGS'
    bl_region_type = 'WINDOW'
    bl_context = "general"
    bl_label = "Setup"
    bl_options = {'HIDE_HEADER'}

    def draw_centered(self, context, layout):
        project = context.project

        if not project:
            layout.label(text="No active project.", icon='INFO')
            return

        layout.prop(project, "name")
        layout.prop(project, "root_path", text="Location")


classes = (
    PROJECTSETTINGS_HT_header,
    PROJECTSETTINGS_MT_editor_menus,
    PROJECTSETTINGS_MT_view,
    PROJECTSETTINGS_PT_navigation_bar,
    PROJECTSETTINGS_PT_save_project_settings,
    PROJECTSETTINGS_PT_setup,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
