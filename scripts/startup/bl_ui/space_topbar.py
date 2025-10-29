# SPDX-FileCopyrightText: 2009-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy
from bpy.types import Header, Menu, Panel

from bpy.app.translations import (
    pgettext_iface as iface_,
    contexts as i18n_contexts,
)


class TOPBAR_HT_upper_bar(Header):
    bl_space_type = 'TOPBAR'

    def draw(self, context):
        region = context.region

        if region.alignment == 'RIGHT':
            self.draw_right(context)
        else:
            self.draw_left(context)

    def draw_left(self, context):
        layout = self.layout

        window = context.window
        screen = context.screen

        TOPBAR_MT_editor_menus.draw_collapsible(context, layout)

        layout.separator(type='LINE')

        if not screen.show_fullscreen:
            layout.template_ID_tabs(window, "workspace", new="workspace.add", menu="TOPBAR_MT_workspace_menu")
        else:
            layout.operator("screen.back_to_previous", icon='SCREEN_BACK', text="Back to Previous")

    def draw_right(self, context):
        layout = self.layout

        window = context.window
        screen = context.screen
        scene = window.scene

        # If statusbar is hidden, still show messages at the top
        if not screen.show_statusbar:
            layout.template_reports_banner()
            layout.template_running_jobs()

        # Active workspace view-layer is retrieved through window, not through workspace.
        layout.template_ID(window, "scene", new="scene.new", unlink="scene.delete")

        row = layout.row(align=True)
        row.template_search(
            window, "view_layer",
            scene, "view_layers",
            new="scene.view_layer_add",
            unlink="scene.view_layer_remove",
        )


class TOPBAR_PT_tool_settings_extra(Panel):
    """
    Popover panel for adding extra options that don't fit in the tool settings header
    """
    bl_idname = "TOPBAR_PT_tool_settings_extra"
    bl_region_type = 'HEADER'
    bl_space_type = 'TOPBAR'
    bl_label = "Extra Options"
    bl_description = "Extra options"

    def draw(self, context):
        from bl_ui.space_toolsystem_common import ToolSelectPanelHelper
        layout = self.layout

        # Get the active tool
        space_type, mode = ToolSelectPanelHelper._tool_key_from_context(context)
        cls = ToolSelectPanelHelper._tool_class_from_space_type(space_type)
        item, tool, _ = cls._tool_get_active(context, space_type, mode, with_icon=True)
        if item is None:
            return

        # Draw the extra settings
        item.draw_settings(context, layout, tool, extra=True)


class TOPBAR_PT_tool_fallback(Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'HEADER'
    bl_label = "Layers"
    bl_ui_units_x = 8

    def draw(self, context):
        from bl_ui.space_toolsystem_common import ToolSelectPanelHelper
        layout = self.layout

        tool_settings = context.tool_settings
        ToolSelectPanelHelper.draw_fallback_tool_items(layout, context)
        if tool_settings.workspace_tool_type == 'FALLBACK':
            tool = context.tool
            ToolSelectPanelHelper.draw_active_tool_fallback(context, layout, tool)


class TOPBAR_MT_editor_menus(Menu):
    bl_idname = "TOPBAR_MT_editor_menus"
    bl_label = ""

    def draw(self, context):
        layout = self.layout

        # Allow calling this menu directly (this might not be a header area).
        if getattr(context.area, "show_menus", False):
            layout.menu("TOPBAR_MT_blender", text="", icon='BLENDER')
        else:
            layout.menu("TOPBAR_MT_blender", text="Blender")

        layout.menu("TOPBAR_MT_file")
        layout.menu("TOPBAR_MT_edit")

        layout.menu("TOPBAR_MT_render")

        layout.menu("TOPBAR_MT_window")
        layout.menu("TOPBAR_MT_help")


class TOPBAR_MT_blender(Menu):
    bl_label = "Blender"

    def draw(self, _context):
        layout = self.layout

        layout.operator("wm.splash")
        layout.operator("wm.splash_about")

        layout.separator()

        layout.operator("preferences.app_template_install", text="Install Application Template...")

        layout.separator()

        layout.menu("TOPBAR_MT_blender_system")


class TOPBAR_MT_file_cleanup(Menu):
    bl_label = "Clean Up"

    def draw(self, _context):
        layout = self.layout
        layout.separator()

        layout.operator("outliner.orphans_purge", text="Purge Unused Data...")
        layout.operator("outliner.orphans_manage", text="Manage Unused Data...")


class TOPBAR_MT_file(Menu):
    bl_label = "File"

    def draw(self, context):
        layout = self.layout

        layout.operator_context = 'INVOKE_AREA'
        layout.menu("TOPBAR_MT_file_new", text="New", text_ctxt=i18n_contexts.id_windowmanager, icon='FILE_NEW')
        layout.operator("wm.open_mainfile", text="Open...", icon='FILE_FOLDER')
        layout.menu("TOPBAR_MT_file_open_recent")
        layout.operator("wm.revert_mainfile")
        layout.menu("TOPBAR_MT_file_recover")

        layout.separator()

        layout.operator_context = 'EXEC_AREA' if context.blend_data.is_saved else 'INVOKE_AREA'
        layout.operator("wm.save_mainfile", text="Save", icon='FILE_TICK')

        layout.operator_context = 'INVOKE_AREA'
        layout.operator("wm.save_as_mainfile", text="Save As...")
        layout.operator_context = 'INVOKE_AREA'
        layout.operator("wm.save_as_mainfile", text="Save Copy...").copy = True

        sub = layout.row()
        sub.enabled = context.blend_data.is_saved
        sub.operator_context = 'EXEC_AREA'
        sub.operator("wm.save_mainfile", text="Save Incremental").incremental = True

        layout.separator()

        layout.operator_context = 'INVOKE_AREA'
        layout.operator("wm.link", text="Link...", icon='LINK_BLEND')
        layout.operator("wm.append", text="Append...", icon='APPEND_BLEND')
        layout.menu("TOPBAR_MT_file_previews")

        layout.separator()

        layout.menu("TOPBAR_MT_file_import", icon='IMPORT')
        layout.menu("TOPBAR_MT_file_export", icon='EXPORT')
        row = layout.row()
        row.operator("wm.collection_export_all")
        row.enabled = context.view_layer.has_export_collections

        layout.separator()

        layout.menu("TOPBAR_MT_file_external_data")
        layout.menu("TOPBAR_MT_file_cleanup")

        layout.separator()

        layout.menu("TOPBAR_MT_file_defaults")

        layout.separator()

        layout.operator("wm.quit_blender", text="Quit", icon='QUIT')


class TOPBAR_MT_file_new(Menu):
    bl_label = "New File"

    @staticmethod
    def app_template_paths():
        import os

        template_paths = bpy.utils.app_template_paths()

        # Expand template paths.

        # Use a set to avoid duplicate user/system templates.
        # This is a corner case, but users managed to do it! #76849.
        app_templates = set()
        for path in template_paths:
            for d in os.listdir(path):
                if d.startswith(("__", ".")):
                    continue
                template = os.path.join(path, d)
                if os.path.isdir(template):
                    app_templates.add(d)

        return sorted(app_templates)

    @staticmethod
    def draw_ex(layout, _context, *, use_splash=False, use_more=False):
        layout.operator_context = 'INVOKE_DEFAULT'

        # Limit number of templates in splash screen, spill over into more menu.
        paths = TOPBAR_MT_file_new.app_template_paths()
        splash_limit = 6

        if use_splash:
            show_more = len(paths) > (splash_limit - 1)
            if show_more:
                paths = paths[:splash_limit - 2]
        elif use_more:
            paths = paths[splash_limit - 2:]
            show_more = False
        else:
            show_more = False

        # Draw application templates.
        if not use_more:
            props = layout.operator("wm.read_homefile", text="General", icon='FILE_NEW')
            props.app_template = ""

        for d in paths:
            icon = 'FILE_NEW'
            # Set icon per template.
            if d == "2D_Animation":
                icon = 'GREASEPENCIL_LAYER_GROUP'
            elif d == "Sculpting":
                icon = 'SCULPTMODE_HLT'
            elif d == "Storyboarding":
                icon = 'GREASEPENCIL'
            elif d == "VFX":
                icon = 'TRACKER'
            elif d == "Video_Editing":
                icon = 'SEQUENCE'
            props = layout.operator("wm.read_homefile", text=bpy.path.display_name(iface_(d)), icon=icon)
            props.app_template = d

        layout.operator_context = 'EXEC_DEFAULT'

        if show_more:
            layout.menu("TOPBAR_MT_templates_more", text="More...")

    def draw(self, context):
        TOPBAR_MT_file_new.draw_ex(self.layout, context)


class TOPBAR_MT_file_recover(Menu):
    bl_label = "Recover"

    def draw(self, _context):
        layout = self.layout

        layout.operator("wm.recover_last_session", text="Last Session")
        layout.operator("wm.recover_auto_save", text="Auto Save...")


class TOPBAR_MT_file_defaults(Menu):
    bl_label = "Defaults"

    def draw(self, context):
        layout = self.layout
        prefs = context.preferences

        layout.operator_context = 'INVOKE_AREA'

        if any(bpy.utils.app_template_paths()):
            app_template = prefs.app_template
        else:
            app_template = None

        if app_template:
            layout.label(
                text=iface_(bpy.path.display_name(app_template, has_ext=False), i18n_contexts.id_workspace),
                translate=False,
            )

        layout.operator("wm.save_homefile")
        if app_template:
            display_name = bpy.path.display_name(iface_(app_template))
            props = layout.operator("wm.read_factory_settings", text="Load Factory Blender Settings")
            props.app_template = app_template
            props = layout.operator(
                "wm.read_factory_settings",
                text=iface_("Load Factory {:s} Settings", i18n_contexts.operator_default).format(display_name),
                translate=False,
            )
            props.app_template = app_template
            props.use_factory_startup_app_template_only = True
            del display_name
        else:
            layout.operator("wm.read_factory_settings")


# Include technical operators here which would otherwise have no way for users to access.
class TOPBAR_MT_blender_system(Menu):
    bl_label = "System"

    def draw(self, _context):
        layout = self.layout

        layout.operator("script.reload")

        layout.separator()

        layout.operator("wm.memory_statistics")
        layout.operator("wm.debug_menu")
        layout.operator_menu_enum("wm.redraw_timer", "type")

        layout.separator()

        layout.operator("screen.spacedata_cleanup")
        layout.operator("wm.operator_presets_cleanup")


class TOPBAR_MT_templates_more(Menu):
    bl_label = "Templates"

    def draw(self, context):
        bpy.types.TOPBAR_MT_file_new.draw_ex(self.layout, context, use_more=True)


class TOPBAR_MT_file_import(Menu):
    bl_idname = "TOPBAR_MT_file_import"
    bl_label = "Import"
    bl_owner_use_filter = False

    def draw(self, _context):
        if bpy.app.build_options.alembic:
            self.layout.operator("wm.alembic_import", text="Alembic (.abc)")
        if bpy.app.build_options.usd:
            self.layout.operator(
                "wm.usd_import", text="Universal Scene Description (.usd*)")

        if bpy.app.build_options.io_gpencil:
            self.layout.operator("wm.grease_pencil_import_svg", text="SVG as Grease Pencil")

        if bpy.app.build_options.io_wavefront_obj:
            self.layout.operator("wm.obj_import", text="Wavefront (.obj)")
        if bpy.app.build_options.io_ply:
            self.layout.operator("wm.ply_import", text="Stanford PLY (.ply)")
        if bpy.app.build_options.io_stl:
            self.layout.operator("wm.stl_import", text="STL (.stl)")

        if bpy.app.build_options.io_fbx:
            self.layout.operator("wm.fbx_import", text="FBX (.fbx)")


class TOPBAR_MT_file_export(Menu):
    bl_idname = "TOPBAR_MT_file_export"
    bl_label = "Export"
    bl_owner_use_filter = False

    def draw(self, _context):
        if bpy.app.build_options.alembic:
            self.layout.operator("wm.alembic_export", text="Alembic (.abc)")
        if bpy.app.build_options.usd:
            self.layout.operator(
                "wm.usd_export", text="Universal Scene Description (.usd*)")

        if bpy.app.build_options.io_gpencil:
            # PUGIXML library dependency.
            if bpy.app.build_options.pugixml:
                self.layout.operator("wm.grease_pencil_export_svg", text="Grease Pencil as SVG")
            # HARU library dependency.
            if bpy.app.build_options.haru:
                self.layout.operator("wm.grease_pencil_export_pdf", text="Grease Pencil as PDF")

        if bpy.app.build_options.io_wavefront_obj:
            self.layout.operator("wm.obj_export", text="Wavefront (.obj)")
        if bpy.app.build_options.io_ply:
            self.layout.operator("wm.ply_export", text="Stanford PLY (.ply)")
        if bpy.app.build_options.io_stl:
            self.layout.operator("wm.stl_export", text="STL (.stl)")


class TOPBAR_MT_file_external_data(Menu):
    bl_label = "External Data"

    def draw(self, _context):
        layout = self.layout

        icon = 'CHECKBOX_HLT' if bpy.data.use_autopack else 'CHECKBOX_DEHLT'
        layout.operator("file.autopack_toggle", icon=icon)

        pack_all = layout.row()
        pack_all.operator("file.pack_all")
        pack_all.active = not bpy.data.use_autopack

        unpack_all = layout.row()
        unpack_all.operator("file.unpack_all")
        unpack_all.active = not bpy.data.use_autopack

        layout.separator()

        layout.operator("file.pack_libraries")
        layout.operator("file.unpack_libraries")

        layout.separator()

        layout.operator("file.make_paths_relative")
        layout.operator("file.make_paths_absolute")

        layout.separator()

        layout.operator("file.report_missing_files")
        layout.operator("file.find_missing_files", text="Find Missing Files...")


class TOPBAR_MT_file_previews(Menu):
    bl_label = "Data Previews"

    def draw(self, _context):
        layout = self.layout

        layout.operator("wm.previews_ensure")
        layout.operator("wm.previews_batch_generate", text="Batch-Generate Previews...")

        layout.separator()

        layout.operator("wm.previews_clear", text="Clear Data-Block Previews...")
        layout.operator("wm.previews_batch_clear", text="Batch-Clear Previews...")


class TOPBAR_MT_render(Menu):
    bl_label = "Render"

    def draw(self, context):
        layout = self.layout

        rd = context.scene.render
        scene = context.scene
        seq_scene = context.sequencer_scene
        strips = getattr(context, "strips", ())

        can_render_seq = seq_scene and seq_scene.render.use_sequencer and strips

        layout.operator("render.render", text="Render Image", icon='RENDER_STILL').use_viewport = True
        props = layout.operator("render.render", text="Render Animation", icon='RENDER_ANIMATION')
        props.animation = True
        props.use_viewport = True

        layout.separator()

        if can_render_seq and (seq_scene != scene):
            props = layout.operator("render.render", text="Render Sequencer Image", icon='RENDER_STILL')
            props.use_viewport = True
            props.use_sequencer_scene = True

            props = layout.operator("render.render", text="Render Sequencer Animation", icon='RENDER_ANIMATION')
            props.animation = True
            props.use_viewport = True
            props.use_sequencer_scene = True

            layout.separator()

        layout.operator("sound.mixdown", text="Render Audio...")

        layout.separator()

        layout.operator("render.view_show", text="View Render")
        layout.operator("render.play_rendered_anim", text="View Animation")

        layout.separator()

        layout.prop(rd, "use_lock_interface", text="Lock Interface")


class TOPBAR_MT_edit(Menu):
    bl_label = "Edit"

    def draw(self, context):
        layout = self.layout

        show_developer = context.preferences.view.show_developer_ui

        layout.operator("ed.undo", icon='LOOP_BACK')
        layout.operator("ed.redo", icon='LOOP_FORWARDS')
        layout.menu("TOPBAR_MT_undo_history")

        layout.separator()

        layout.operator("screen.redo_last", text="Adjust Last Operation...")
        layout.operator("screen.repeat_last")
        layout.operator("screen.repeat_history", text="Repeat History...")

        layout.separator()

        layout.operator("wm.search_menu", text="Menu Search...", icon='VIEWZOOM')
        if show_developer:
            layout.operator("wm.search_operator", text="Operator Search...")

        layout.separator()

        # Mainly to expose shortcut since this depends on the context.
        props = layout.operator("wm.call_panel", text="Rename Active Item...")
        props.name = "TOPBAR_PT_name"
        props.keep_open = False

        layout.operator("wm.batch_rename", text="Batch Rename...")

        layout.separator()

        # Should move elsewhere (impacts outliner & 3D view).
        tool_settings = context.tool_settings
        layout.prop(tool_settings, "lock_object_mode")

        layout.separator()

        layout.operator("screen.userpref_show", text="Preferences...", icon='PREFERENCES')


class TOPBAR_MT_window(Menu):
    bl_label = "Window"

    def draw(self, context):
        import sys
        from _bl_ui_utils.layout import operator_context

        layout = self.layout

        layout.operator("wm.window_new")
        layout.operator("wm.window_new_main")

        layout.separator()

        layout.operator("wm.window_fullscreen_toggle", icon='FULLSCREEN_ENTER')

        layout.separator()

        layout.operator("screen.workspace_cycle", text="Next Workspace").direction = 'NEXT'
        layout.operator("screen.workspace_cycle", text="Previous Workspace").direction = 'PREV'

        layout.separator()

        layout.prop(context.screen, "show_statusbar")

        layout.separator()

        layout.operator("screen.screenshot")

        # Showing the status in the area doesn't work well in this case.
        # - From the top-bar, the text replaces the file-menu (not so bad but strange).
        # - From menu-search it replaces the area that the user may want to screen-shot.
        # Setting the context to screen causes the status to show in the global status-bar.
        with operator_context(layout, 'INVOKE_SCREEN'):
            layout.operator("screen.screenshot_area")

        if sys.platform[:3] == "win":
            layout.separator()
            layout.operator("wm.console_toggle", icon='CONSOLE')

        if context.scene.render.use_multiview:
            layout.separator()
            layout.operator("wm.set_stereo_3d")


class TOPBAR_MT_help(Menu):
    bl_label = "Help"

    def draw(self, context):
        layout = self.layout

        show_developer = context.preferences.view.show_developer_ui

        layout.operator("wm.url_open_preset", text="Manual", icon='URL').type = 'MANUAL'
        layout.operator("wm.url_open", text="Support").url = "https://www.blender.org/support"
        layout.operator("wm.url_open", text="User Communities").url = "https://www.blender.org/community/"
        layout.operator("wm.url_open", text="Get Involved").url = "https://www.blender.org/get-involved/"
        layout.operator("wm.url_open_preset", text="Release Notes").type = 'RELEASE_NOTES'

        layout.separator()

        if show_developer:
            layout.operator(
                "wm.url_open",
                text="Developer Documentation",
                icon='URL',
            ).url = "https://developer.blender.org/docs/"
            layout.operator("wm.url_open", text="Developer Community").url = "https://devtalk.blender.org"
            layout.operator("wm.url_open_preset", text="Python API Reference").type = 'API'
            layout.operator("wm.operator_cheat_sheet", icon='TEXT')

        layout.separator()

        layout.operator("wm.url_open_preset", text="Report a Bug", icon='URL').type = 'BUG'
        layout.operator("wm.sysinfo")


class TOPBAR_MT_file_context_menu(Menu):
    bl_label = "File"

    def draw(self, _context):
        layout = self.layout

        layout.operator_context = 'INVOKE_AREA'
        layout.menu("TOPBAR_MT_file_new", text="New", text_ctxt=i18n_contexts.id_windowmanager, icon='FILE_NEW')
        layout.operator("wm.open_mainfile", text="Open...", icon='FILE_FOLDER')
        layout.menu("TOPBAR_MT_file_open_recent")

        layout.separator()

        layout.operator("wm.link", text="Link...", icon='LINK_BLEND')
        layout.operator("wm.append", text="Append...", icon='APPEND_BLEND')

        layout.separator()

        layout.menu("TOPBAR_MT_file_import", icon='IMPORT')
        layout.menu("TOPBAR_MT_file_export", icon='EXPORT')

        layout.separator()

        layout.operator("screen.userpref_show", text="Preferences...", icon='PREFERENCES')


class TOPBAR_MT_workspace_menu(Menu):
    bl_label = "Workspace"

    def draw(self, _context):
        layout = self.layout

        layout.operator("workspace.duplicate", text="Duplicate", icon='DUPLICATE')
        if len(bpy.data.workspaces) <= 1:
            return

        layout.operator("workspace.delete", text="Delete", icon='REMOVE')

        layout.separator()

        layout.operator("workspace.reorder_to_front", text="Reorder to Front", icon='TRIA_LEFT_BAR')
        layout.operator("workspace.reorder_to_back", text="Reorder to Back", icon='TRIA_RIGHT_BAR')

        layout.separator()

        # For key binding discoverability.
        props = layout.operator("screen.workspace_cycle", text="Previous Workspace")
        props.direction = 'PREV'
        props = layout.operator("screen.workspace_cycle", text="Next Workspace")
        props.direction = 'NEXT'

        layout.separator()

        layout.operator("workspace.delete_all_others")


# Grease Pencil Object - Primitive curve
class TOPBAR_PT_gpencil_primitive(Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'HEADER'
    bl_label = "Primitives"

    def draw(self, context):
        settings = context.tool_settings.gpencil_sculpt

        layout = self.layout
        # Curve
        layout.template_curve_mapping(settings, "thickness_primitive_curve", brush=True)


# Only a popover
class TOPBAR_PT_name(Panel):
    bl_space_type = 'TOPBAR'  # dummy
    bl_region_type = 'HEADER'
    bl_label = "Rename Active Item"
    bl_ui_units_x = 14

    def draw(self, context):
        layout = self.layout

        # Edit first editable button in popup
        def row_with_icon(layout, icon):
            row = layout.row()
            row.activate_init = True
            row.label(icon=icon)
            return row

        mode = context.mode
        space = context.space_data
        space_type = None if (space is None) else space.type
        found = False
        if space_type == 'SEQUENCE_EDITOR':
            layout.label(text="Sequence Strip Name")
            item = context.active_strip
            if item:
                row = row_with_icon(layout, 'SEQUENCE')
                row.prop(item, "name", text="")
                found = True
        elif space_type == 'NODE_EDITOR':
            layout.label(text="Node Label")
            item = context.active_node
            if item:
                row = row_with_icon(layout, 'NODE')
                row.prop(item, "label", text="")
                found = True
        elif space_type == 'NLA_EDITOR':
            layout.label(text="NLA Strip Name")
            item = next(
                (strip for strip in context.selected_nla_strips if strip.active), None)
            if item:
                row = row_with_icon(layout, 'NLA')
                row.prop(item, "name", text="")
                found = True
        else:
            if mode == 'POSE' or (mode == 'WEIGHT_PAINT' and context.pose_object):
                layout.label(text="Bone Name")
                item = context.active_pose_bone
                if item:
                    row = row_with_icon(layout, 'BONE_DATA')
                    row.prop(item, "name", text="")
                    found = True
            elif mode == 'EDIT_ARMATURE':
                layout.label(text="Bone Name")
                item = context.active_bone
                if item:
                    row = row_with_icon(layout, 'BONE_DATA')
                    row.prop(item, "name", text="")
                    found = True
            else:
                layout.label(text="Object Name")
                item = context.object
                if item:
                    row = row_with_icon(layout, 'OBJECT_DATA')
                    row.prop(item, "name", text="")
                    found = True

        if not found:
            row = row_with_icon(layout, 'ERROR')
            row.label(text="No active item")


class TOPBAR_PT_name_marker(Panel):
    bl_space_type = 'TOPBAR'  # dummy
    bl_region_type = 'HEADER'
    bl_label = "Rename Marker"
    bl_ui_units_x = 14

    @staticmethod
    def is_using_pose_markers(context):
        sd = context.space_data
        return (
            sd.type == 'DOPESHEET_EDITOR' and sd.mode in {'ACTION', 'SHAPEKEY'} and
            sd.show_pose_markers and context.active_action
        )

    @staticmethod
    def is_using_sequencer(context):
        sd = context.space_data
        return sd.type == 'SEQUENCE_EDITOR'

    @staticmethod
    def get_selected_marker(context):
        if TOPBAR_PT_name_marker.is_using_pose_markers(context):
            markers = context.active_action.pose_markers
        elif TOPBAR_PT_name_marker.is_using_sequencer(context):
            markers = context.sequencer_scene.timeline_markers
        else:
            markers = context.scene.timeline_markers

        for marker in markers:
            if marker.select:
                return marker
        return None

    @staticmethod
    def row_with_icon(layout, icon):
        row = layout.row()
        row.activate_init = True
        row.label(icon=icon)
        return row

    def draw(self, context):
        layout = self.layout

        layout.label(text="Marker Name")

        scene = context.scene
        if scene.tool_settings.lock_markers:
            row = self.row_with_icon(layout, 'ERROR')
            label = "Markers are locked"
            row.label(text=label)
            return

        marker = self.get_selected_marker(context)
        if marker is None:
            row = self.row_with_icon(layout, 'ERROR')
            row.label(text="No active marker")
            return

        icon = 'TIME'
        if marker.camera is not None:
            icon = 'CAMERA_DATA'
        elif self.is_using_pose_markers(context):
            icon = 'ARMATURE_DATA'
        row = self.row_with_icon(layout, icon)
        row.prop(marker, "name", text="")


class TOPBAR_PT_grease_pencil_layers(Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'HEADER'
    bl_label = "Layers"
    bl_ui_units_x = 14

    @classmethod
    def poll(cls, context):
        object = context.object
        if object is None:
            return False
        if object.type != 'GREASEPENCIL':
            return False

        return True

    def draw(self, context):
        from .properties_data_grease_pencil import DATA_PT_grease_pencil_layers

        layout = self.layout
        grease_pencil = context.object.data

        DATA_PT_grease_pencil_layers.draw_settings(layout, grease_pencil)


classes = (
    TOPBAR_HT_upper_bar,
    TOPBAR_MT_file_context_menu,
    TOPBAR_MT_workspace_menu,
    TOPBAR_MT_editor_menus,
    TOPBAR_MT_blender,
    TOPBAR_MT_blender_system,
    TOPBAR_MT_file,
    TOPBAR_MT_file_new,
    TOPBAR_MT_file_recover,
    TOPBAR_MT_file_defaults,
    TOPBAR_MT_templates_more,
    TOPBAR_MT_file_import,
    TOPBAR_MT_file_export,
    TOPBAR_MT_file_external_data,
    TOPBAR_MT_file_cleanup,
    TOPBAR_MT_file_previews,
    TOPBAR_MT_edit,
    TOPBAR_MT_render,
    TOPBAR_MT_window,
    TOPBAR_MT_help,
    TOPBAR_PT_tool_fallback,
    TOPBAR_PT_tool_settings_extra,
    TOPBAR_PT_gpencil_primitive,
    TOPBAR_PT_name,
    TOPBAR_PT_name_marker,
    TOPBAR_PT_grease_pencil_layers,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
