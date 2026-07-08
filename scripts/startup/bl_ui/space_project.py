# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy
from bpy.types import Header, Menu, Panel

from bpy.app.translations import pgettext_iface

from .space_userpref import CenterAlignMixIn


MAIN_SECTION_NAME = "General"


# -------------------------------------------------------------
# Header

class PROJECT_HT_header(Header):
    bl_space_type = 'PROJECT'

    def draw(self, context):
        layout = self.layout

        layout.template_header()
        PROJECT_MT_editor_menus.draw_collapsible(context, layout)
        layout.separator_spacer()


class PROJECT_MT_editor_menus(Menu):
    bl_idname = "PROJECT_MT_editor_menus"
    bl_label = ""

    def draw(self, context):
        layout = self.layout
        layout.menu("PROJECT_MT_view")
        layout.menu("PROJECT_MT_save_load", text="Project")


class PROJECT_MT_view(Menu):
    bl_label = "View"

    def draw(self, context):
        layout = self.layout
        project_space = context.space_data

        layout.prop(project_space, "show_region_ui")

        layout.separator()

        layout.menu("INFO_MT_area")


class PROJECT_MT_save_load(Menu):
    bl_label = "Save & Load"

    def draw(self, context):
        layout = self.layout

        prefs = context.preferences

        layout.prop(prefs, "use_project_auto_save", text="Auto-Save Project")
        layout.operator("project.save_project", text="Save Project")


# -------------------------------------------------------------
# Execution Area
#
# Shown when header is hidden.

class PROJECT_PT_save_project(Panel):
    bl_label = "Save Project"
    bl_space_type = 'PROJECT'
    bl_region_type = 'EXECUTE'
    bl_options = {'HIDE_HEADER'}

    def draw(self, context):
        layout = self.layout.row()
        layout.operator_context = 'EXEC_AREA'

        layout.menu("PROJECT_MT_save_load", text="", icon='COLLAPSEMENU')

        # Save button.
        if not context.preferences.use_project_auto_save and bpy.data.project is not None:
            # Show '*' to let users know the project has been modified.
            # It is shown to the left so that it is visible when the sidebar is narrow,
            # and for consistency with unsaved files in the title bar.
            layout.operator(
                "project.save_project",
                text=("* " if bpy.data.project.is_dirty else "") + pgettext_iface("Save Project"),
                icon='FILE_TICK',
                translate=False,
            )


# -------------------------------------------------------------
# Navigation Bar

class PROJECT_PT_navigation_bar(Panel):
    bl_label = "Project Navigation"
    bl_space_type = 'PROJECT'
    bl_region_type = 'UI'
    bl_category = "Navigation"
    bl_options = {'HIDE_HEADER'}

    @classmethod
    def poll(cls, context):
        return True

    def draw(self, context):
        layout = self.layout

        space_data = context.space_data

        col = layout.column()

        if bpy.data.project is None:
            # If there's no project, we need to make sure the UI for creating a
            # new project is visible. That UI is in the main section, so we
            # ensure it's the active section.
            space_data.active_section = MAIN_SECTION_NAME
            col.enabled = False

        col.scale_x = 1.3
        col.scale_y = 1.3
        col.prop(space_data, "active_section", expand=True)


# -------------------------------------------------------------
# Main Area

class PROJECT_PT_main(Panel, CenterAlignMixIn):
    bl_label = "Project"
    bl_space_type = 'PROJECT'
    bl_region_type = 'WINDOW'
    bl_category = MAIN_SECTION_NAME

    @classmethod
    def poll(cls, context):
        return bpy.data.project is not None

    def draw_centered(self, context, layout):
        project = bpy.data.project

        col = layout.column()
        col.prop(project, "name")
        col.prop(project, "root_path")


class PROJECT_PT_main_unset(Panel, CenterAlignMixIn):
    bl_label = "No Project"
    bl_space_type = 'PROJECT'
    bl_region_type = 'WINDOW'
    bl_options = {'HIDE_HEADER'}
    bl_category = MAIN_SECTION_NAME

    @classmethod
    def poll(cls, context):
        return not PROJECT_PT_main.poll(context)

    def draw_centered(self, context, layout):
        col = layout.column()
        col.separator(factor=2.0)

        if context.blend_data.filepath == "":
            row = col.row()
            row.alignment = 'CENTER'
            row.label(
                text="No active project.",
                icon='STATUS_INFO',
            )
            col.separator()

            row = col.row()
            row.alignment = 'CENTER'
            row.label(text="Save the current file, and make sure to place it in a folder that will be part of the project")

            row = col.row()
            row.alignment = 'CENTER'
            row.label(text="Alternatively, open a file inside of a project directory to see its settings.")

            col.separator()
            row = col.row()
            row.alignment = 'CENTER'
            row.operator("wm.save_as_mainfile", text="Save File...", icon='FILE_TICK')
            row.operator("project.open_blend_in_project", icon='FILE_FOLDER')
        else:
            row = col.row()
            row.alignment = 'CENTER'
            row.label(
                text="No active project.",
                icon='STATUS_INFO',
            )
            col.separator()

            row = col.row()
            row.alignment = 'CENTER'
            row.label(text="Set up a new project by choosing any parent directory of the current file.")
            row = col.row()
            row.alignment = 'CENTER'
            row.label(text="Alternatively, open a file inside of a project directory to see its settings.")

            col.separator()
            row = col.row()
            row.alignment = 'CENTER'
            row.operator("project.new_project", text="New Project...", icon='ADD')
            row.operator("project.open_blend_in_project", icon='FILE_FOLDER')


# -------------------------------------------------------------
# Register

classes = (
    PROJECT_HT_header,
    PROJECT_MT_editor_menus,
    PROJECT_MT_view,
    PROJECT_MT_save_load,
    PROJECT_PT_navigation_bar,
    PROJECT_PT_save_project,
    PROJECT_PT_main_unset,
    PROJECT_PT_main,
)
