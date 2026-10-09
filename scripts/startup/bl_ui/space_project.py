# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy
from bpy.types import Header, Menu, Panel

from bpy.app.translations import (
    pgettext_iface as iface_,
    contexts as i18n_contexts,
)

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

        project = bpy.data.project
        if project:
            row = layout.row()
            row.label(text=project.name, translate=False)
            row.active = False


class PROJECT_MT_editor_menus(Menu):
    bl_idname = "PROJECT_MT_editor_menus"
    bl_label = ""

    def draw(self, context):
        layout = self.layout
        layout.menu("PROJECT_MT_view")
        layout.menu("PROJECT_MT_save_load", text="Project", text_ctxt=i18n_contexts.editor_preferences)


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
        layout.operator("project.config_save", text="Save Project")


class PROJECT_MT_variable_add(Menu):
    bl_label = "Add Variable"

    def draw(self, context):
        layout = self.layout

        layout.operator("project.variable_add", text="String", icon='NONE').variable_type = 'STRING'
        layout.operator("project.variable_add", text="Filepath", icon='NONE').variable_type = 'FILEPATH'
        layout.separator()
        layout.operator("project.variable_add", text="Integer", icon='NONE').variable_type = 'INTEGER'
        layout.operator("project.variable_add", text="Float", icon='NONE').variable_type = 'FLOAT'


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
                "project.config_save",
                text=("* " if bpy.data.project.is_dirty else "") + iface_("Save Project"),
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
    bl_translation_context = i18n_contexts.editor_preferences
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


class PROJECT_PT_splash_screen(Panel, CenterAlignMixIn):
    bl_label = "Splash Screen"
    bl_space_type = 'PROJECT'
    bl_region_type = 'WINDOW'
    bl_category = MAIN_SECTION_NAME
    bl_parent_id = "PROJECT_PT_main"
    bl_options = {'DEFAULT_CLOSED'}

    def draw_centered(self, context, layout):
        split = layout.split(factor=layout.property_split_factor)
        row = split.row()
        row.alignment = 'RIGHT'
        row.label(text="Artwork")
        col = split.column()
        col.template_project_splash()


class PROJECT_PT_color_management(Panel, CenterAlignMixIn):
    bl_label = "Color Management"
    bl_space_type = 'PROJECT'
    bl_region_type = 'WINDOW'
    bl_category = MAIN_SECTION_NAME

    @classmethod
    def poll(cls, context):
        return bpy.data.project is not None

    def draw_centered(self, context, layout):
        project = bpy.data.project
        colorspace = bpy.data.colorspace

        config_source = colorspace.ocio_config_source
        overridden = config_source in {'BLENDER_OCIO', 'OCIO'}

        if colorspace.ocio_config_source == 'PROJECT':
            needs_reload = colorspace.ocio_config_path != project.ocio_config_path
        else:
            needs_reload = not overridden and (project.ocio_config_path or colorspace.is_failed_opencolorio_config)

        col = layout.column()
        col.active = not overridden
        col.prop(project, "ocio_config")
        if project.ocio_config == 'PATH':
            row = col.row()
            row.alert = colorspace.is_failed_opencolorio_config
            row.prop(project, "ocio_config_path", text="Path")

        if overridden:
            row = layout.split(factor=0.4)
            row.label()
            row.label(
                text="Overridden by the {:s} environment variable".format(config_source),
                icon='STATUS_INFO',
            )

        if needs_reload:
            split = layout.split(factor=layout.property_split_factor)
            split.label()
            row = split.split()
            row.label()
            if not bpy.data.is_dirty:
                # Skip prompt on unmodified blend file
                row.operator_context = 'EXEC_DEFAULT'
            row.operator("wm.revert_mainfile", text="Reload to Apply Changes", icon='FILE_REFRESH')


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

        row = col.row()
        row.alignment = 'CENTER'
        row.label(
            text="No active project.",
            icon='STATUS_INFO',
        )
        col.separator()

        row = col.row()
        row.label_multiline(
            text="Create a new project, or open an existing project or a file inside of one.",
            alignment='CENTER')

        col.separator()
        row = col.row()
        row.alignment = 'CENTER'
        row.operator("project.new", text="New Project...", icon='ADD')
        row.operator("project.open", icon='FILE_FOLDER')


class PROJECT_UL_variables(bpy.types.UIList):
    def draw_item(self, context, layout, data, item, icon, active_data, active_propname):
        col = layout.column()
        col.prop(item, "name", text="", expand=False, emboss=False)

        col = layout.column()
        col.alignment = 'RIGHT'
        col.active = False
        match item.type:
            case 'INTEGER':
                col.label(text=str(item.value))
            case 'FLOAT':
                col.label(text="{:.3f}".format(item.value))
            case 'STRING':
                col.label(text=str(item.value))


class PROJECT_PT_variables(Panel):
    bl_label = "Variables"
    bl_space_type = 'PROJECT'
    bl_region_type = 'WINDOW'
    bl_category = "Variables"

    def draw(self, context):
        project = bpy.data.project

        layout = self.layout

        row = layout.row()
        row.template_list(
            listtype_name="PROJECT_UL_variables",
            list_id="Variables",
            dataptr=project,
            propname="variables",
            item_dyntip_propname="description",
            active_dataptr=project,
            active_propname="active_variable_index",
        )

        col = row.column(align=True)
        col.menu("PROJECT_MT_variable_add", text="", icon='ADD')
        col.operator("project.variable_remove", text="", icon='REMOVE')
        col.separator()
        col.operator("project.variable_move", text="", icon='TRIA_UP').direction = 'UP'
        col.operator("project.variable_move", text="", icon='TRIA_DOWN').direction = 'DOWN'

        if project.active_variable_index >= 0 and project.active_variable_index < len(project.variables):
            var = project.variables[project.active_variable_index]

            col = layout.column()
            col.use_property_split = True
            col.alignment = 'LEFT'
            col.separator()

            col.prop(var, "type")
            if var.type == 'STRING':
                col.prop(var, "subtype")
            col.prop(var, "name")
            col.prop(var, "value")
            col.prop(var, "description")


# -------------------------------------------------------------
# Register

classes = (
    PROJECT_HT_header,
    PROJECT_MT_editor_menus,
    PROJECT_MT_view,
    PROJECT_MT_save_load,
    PROJECT_MT_variable_add,
    PROJECT_PT_navigation_bar,
    PROJECT_PT_save_project,
    PROJECT_PT_main_unset,
    PROJECT_PT_main,
    PROJECT_PT_splash_screen,
    PROJECT_PT_color_management,
    PROJECT_PT_variables,
    PROJECT_UL_variables,
)
