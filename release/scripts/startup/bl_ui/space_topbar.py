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

# <pep8 compliant>
import bpy
from bpy.types import Header, Menu, Panel


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

        layout.operator("wm.splash", text="", icon='BLENDER', emboss=False)

        INFO_MT_editor_menus.draw_collapsible(context, layout)

        layout.separator()

        if not screen.show_fullscreen:
            layout.template_ID_tabs(
                window, "workspace",
                new="workspace.workspace_add_menu",
                unlink="workspace.workspace_delete",
            )
        else:
            layout.operator(
                "screen.back_to_previous",
                icon='SCREEN_BACK',
                text="Back to Previous",
            )

    def draw_right(self, context):
        layout = self.layout

        window = context.window
        scene = window.scene

        # Active workspace view-layer is retrieved through window, not through workspace.
        layout.template_ID(window, "scene", new="scene.new", unlink="scene.delete")

        row = layout.row(align=True)
        row.template_search(
            window, "view_layer",
            scene, "view_layers",
            new="scene.view_layer_add",
            unlink="scene.view_layer_remove")


class TOPBAR_HT_lower_bar(Header):
    bl_space_type = 'TOPBAR'
    bl_region_type = 'WINDOW'

    def draw(self, context):
        layout = self.layout
        region = context.region

        if region.alignment == 'LEFT':
            self.draw_left(context)
        elif region.alignment == 'RIGHT':
            self.draw_right(context)
        else:
            self.draw_center(context)

    def draw_left(self, context):
        layout = self.layout
        mode = context.mode

        # Active Tool
        # -----------
        from .space_toolsystem_common import ToolSelectPanelHelper
        ToolSelectPanelHelper.draw_active_tool_header(context, layout)

        # Object Mode Options
        # -------------------

        # Example of how toolsettings can be accessed as pop-overs.

        # TODO(campbell): editing options should be after active tool options
        # (obviously separated for from the users POV)
        draw_fn = getattr(_draw_left_context_mode, mode, None)
        if draw_fn is not None:
            draw_fn(context, layout)

        # Note: general mode options should be added to 'draw_right'.
        if mode == 'SCULPT':
            layout.popover_group(space_type='PROPERTIES', region_type='WINDOW', context=".paint_common", category="")
        elif mode == 'PAINT_VERTEX':
            layout.popover_group(space_type='PROPERTIES', region_type='WINDOW', context=".paint_common", category="")
        elif mode == 'PAINT_WEIGHT':
            layout.popover_group(space_type='PROPERTIES', region_type='WINDOW', context=".paint_common", category="")
        elif mode == 'PAINT_TEXTURE':
            layout.popover_group(space_type='PROPERTIES', region_type='WINDOW', context=".paint_common", category="")
        elif mode == 'EDIT_ARMATURE':
            pass
        elif mode == 'EDIT_CURVE':
            pass
        elif mode == 'EDIT_MESH':
            pass
        elif mode == 'POSE':
            pass
        elif mode == 'PARTICLE':
            layout.popover_group(space_type='PROPERTIES', region_type='WINDOW', context=".paint_common", category="")
        elif mode == 'GPENCIL_PAINT':
            layout.popover_group(space_type='PROPERTIES', region_type='WINDOW', context=".greasepencil_paint", category="")

    def draw_center(self, context):
        pass

    def draw_right(self, context):
        layout = self.layout

        # General options, note, these _could_ display at the RHS of the draw_left callback.
        # we just want them not to be confused with tool options.
        mode = context.mode

        if mode == 'SCULPT':
            layout.popover_group(space_type='PROPERTIES', region_type='WINDOW', context=".sculpt_mode", category="")
        elif mode == 'PAINT_VERTEX':
            layout.popover_group(space_type='PROPERTIES', region_type='WINDOW', context=".vertexpaint", category="")
        elif mode == 'PAINT_WEIGHT':
            layout.popover_group(space_type='PROPERTIES', region_type='WINDOW', context=".weightpaint", category="")
        elif mode == 'PAINT_TEXTURE':
            layout.popover_group(space_type='PROPERTIES', region_type='WINDOW', context=".imagepaint", category="")
        elif mode == 'EDIT_TEXT':
            layout.popover_group(space_type='PROPERTIES', region_type='WINDOW', context=".text_edit", category="")
        elif mode == 'EDIT_ARMATURE':
            layout.popover_group(space_type='PROPERTIES', region_type='WINDOW', context=".armature_edit", category="")
        elif mode == 'EDIT_METABALL':
            layout.popover_group(space_type='PROPERTIES', region_type='WINDOW', context=".mball_edit", category="")
        elif mode == 'EDIT_LATTICE':
            layout.popover_group(space_type='PROPERTIES', region_type='WINDOW', context=".lattice_edit", category="")
        elif mode == 'EDIT_CURVE':
            layout.popover_group(space_type='PROPERTIES', region_type='WINDOW', context=".curve_edit", category="")
        elif mode == 'EDIT_MESH':
            layout.popover_group(space_type='PROPERTIES', region_type='WINDOW', context=".mesh_edit", category="")
        elif mode == 'POSE':
            layout.popover_group(space_type='PROPERTIES', region_type='WINDOW', context=".posemode", category="")
        elif mode == 'PARTICLE':
            layout.popover_group(space_type='PROPERTIES', region_type='WINDOW', context=".particlemode", category="")
        elif mode == 'OBJECT':
            layout.popover_group(space_type='PROPERTIES', region_type='WINDOW', context=".objectmode", category="")
        elif mode == 'GPENCIL_PAINT':
            layout.prop(context.tool_settings, "gpencil_stroke_placement_view3d", text='')
            if context.tool_settings.gpencil_stroke_placement_view3d in ('ORIGIN', 'CURSOR'):
                layout.prop(context.tool_settings.gpencil_sculpt, "lockaxis", text='')
            layout.prop(context.tool_settings, "use_gpencil_draw_onback", text="", icon='ORTHO')
            layout.prop(context.tool_settings, "use_gpencil_additive_drawing", text="", icon='FREEZE')

        elif mode == 'GPENCIL_SCULPT':
            layout.prop(context.tool_settings.gpencil_sculpt, "lockaxis", text='')


class _draw_left_context_mode:
    @staticmethod
    def SCULPT(context, layout):
        brush = context.tool_settings.sculpt.brush
        if brush is None:
            return

        from .properties_paint_common import UnifiedPaintPanel

        UnifiedPaintPanel.prop_unified_size(layout, context, brush, "size", slider=True, text="Radius")
        UnifiedPaintPanel.prop_unified_strength(layout, context, brush, "strength", slider=True, text="Strength")
        layout.prop(brush, "direction", text="", expand=True)

    def PAINT_TEXTURE(context, layout):
        brush = context.tool_settings.vertex_paint.brush
        if brush is None:
            return

        from .properties_paint_common import UnifiedPaintPanel

        layout.prop(brush, "color", text="")
        UnifiedPaintPanel.prop_unified_size(layout, context, brush, "size", slider=True, text="Radius")
        UnifiedPaintPanel.prop_unified_strength(layout, context, brush, "strength", slider=True, text="Strength")

    def PAINT_VERTEX(context, layout):
        brush = context.tool_settings.vertex_paint.brush
        if brush is None:
            return

        from .properties_paint_common import UnifiedPaintPanel

        layout.prop(brush, "color", text="")
        UnifiedPaintPanel.prop_unified_size(layout, context, brush, "size", slider=True, text="Radius")
        UnifiedPaintPanel.prop_unified_strength(layout, context, brush, "strength", slider=True, text="Strength")

    def PAINT_WEIGHT(context, layout):
        brush = context.tool_settings.weight_paint.brush
        if brush is None:
            return

        from .properties_paint_common import UnifiedPaintPanel

        UnifiedPaintPanel.prop_unified_weight(layout, context, brush, "weight", slider=True, text="Weight")
        UnifiedPaintPanel.prop_unified_size(layout, context, brush, "size", slider=True, text="Radius")
        UnifiedPaintPanel.prop_unified_strength(layout, context, brush, "strength", slider=True, text="Strength")

    def PARTICLE(context, layout):
        # See: 'VIEW3D_PT_tools_brush', basically a duplicate
        settings = context.tool_settings.particle_edit
        brush = settings.brush
        tool = settings.tool
        if tool != 'NONE':
            layout.prop(brush, "size", slider=True)
            if tool == 'ADD':
                layout.prop(brush, "count")
            else:
                layout.prop(brush, "strength", slider=True)

            if tool == 'ADD':
                layout.prop(settings, "use_default_interpolate")
                layout.prop(brush, "steps", slider=True)
                layout.prop(settings, "default_key_count", slider=True)
            elif tool == 'LENGTH':
                layout.row().prop(brush, "length_mode", expand=True)
            elif tool == 'PUFF':
                layout.row().prop(brush, "puff_mode", expand=True)
                layout.prop(brush, "use_puff_volume")

class INFO_MT_editor_menus(Menu):
    bl_idname = "INFO_MT_editor_menus"
    bl_label = ""

    def draw(self, context):
        self.draw_menus(self.layout, context)

    @staticmethod
    def draw_menus(layout, context):
        layout.menu("INFO_MT_file")
        layout.menu("INFO_MT_edit")

        layout.menu("INFO_MT_render")

        layout.menu("INFO_MT_window")
        layout.menu("INFO_MT_help")


class INFO_MT_file(Menu):
    bl_label = "File"

    def draw(self, context):
        layout = self.layout

        layout.operator_context = 'INVOKE_AREA'
        layout.operator("wm.read_homefile", text="New", icon='NEW')
        layout.operator("wm.open_mainfile", text="Open...", icon='FILE_FOLDER')
        layout.menu("INFO_MT_file_open_recent")
        layout.operator("wm.revert_mainfile")
        layout.operator("wm.recover_last_session")
        layout.operator("wm.recover_auto_save", text="Recover Auto Save...")

        layout.separator()

        layout.operator_context = 'EXEC_AREA' if context.blend_data.is_saved else 'INVOKE_AREA'
        layout.operator("wm.save_mainfile", text="Save", icon='FILE_TICK')

        layout.operator_context = 'INVOKE_AREA'
        layout.operator("wm.save_as_mainfile", text="Save As...")
        layout.operator_context = 'INVOKE_AREA'
        layout.operator("wm.save_as_mainfile", text="Save Copy...").copy = True

        layout.separator()

        layout.operator_context = 'INVOKE_AREA'
        layout.operator("wm.save_homefile")
        layout.operator("wm.read_factory_settings")

        layout.separator()

        layout.operator_context = 'INVOKE_AREA'
        layout.operator("wm.link", text="Link...", icon='LINK_BLEND')
        layout.operator("wm.append", text="Append...", icon='APPEND_BLEND')
        layout.menu("INFO_MT_file_previews")

        layout.separator()

        layout.menu("INFO_MT_file_import", icon='IMPORT')
        layout.menu("INFO_MT_file_export", icon='EXPORT')

        layout.separator()

        layout.menu("INFO_MT_file_external_data")

        layout.separator()

        layout.operator_context = 'EXEC_AREA'
        if bpy.data.is_dirty and context.user_preferences.view.use_quit_dialog:
            layout.operator_context = 'INVOKE_SCREEN'  # quit dialog
        layout.operator("wm.quit_blender", text="Quit", icon='QUIT')


class INFO_MT_file_import(Menu):
    bl_idname = "INFO_MT_file_import"
    bl_label = "Import"

    def draw(self, context):
        if bpy.app.build_options.collada:
            self.layout.operator("wm.collada_import", text="Collada (Default) (.dae)")
        if bpy.app.build_options.alembic:
            self.layout.operator("wm.alembic_import", text="Alembic (.abc)")


class INFO_MT_file_export(Menu):
    bl_idname = "INFO_MT_file_export"
    bl_label = "Export"

    def draw(self, context):
        if bpy.app.build_options.collada:
            self.layout.operator("wm.collada_export", text="Collada (Default) (.dae)")
        if bpy.app.build_options.alembic:
            self.layout.operator("wm.alembic_export", text="Alembic (.abc)")


class INFO_MT_file_external_data(Menu):
    bl_label = "External Data"

    def draw(self, context):
        layout = self.layout

        icon = 'CHECKBOX_HLT' if bpy.data.use_autopack else 'CHECKBOX_DEHLT'
        layout.operator("file.autopack_toggle", icon=icon)

        layout.separator()

        pack_all = layout.row()
        pack_all.operator("file.pack_all")
        pack_all.active = not bpy.data.use_autopack

        unpack_all = layout.row()
        unpack_all.operator("file.unpack_all")
        unpack_all.active = not bpy.data.use_autopack

        layout.separator()

        layout.operator("file.make_paths_relative")
        layout.operator("file.make_paths_absolute")
        layout.operator("file.report_missing_files")
        layout.operator("file.find_missing_files")


class INFO_MT_file_previews(Menu):
    bl_label = "Data Previews"

    def draw(self, context):
        layout = self.layout

        layout.operator("wm.previews_ensure")
        layout.operator("wm.previews_batch_generate")

        layout.separator()

        layout.operator("wm.previews_clear")
        layout.operator("wm.previews_batch_clear")


class INFO_MT_game(Menu):
    bl_label = "Game"

    def draw(self, context):
        layout = self.layout

        gs = context.scene.game_settings

        layout.operator("view3d.game_start")

        layout.separator()

        layout.prop(gs, "show_debug_properties")
        layout.prop(gs, "show_framerate_profile")
        layout.prop(gs, "show_physics_visualization")
        layout.prop(gs, "use_deprecation_warnings")
        layout.prop(gs, "use_animation_record")
        layout.separator()
        layout.prop(gs, "use_auto_start")


class INFO_MT_render(Menu):
    bl_label = "Render"

    def draw(self, context):
        layout = self.layout

        rd = context.scene.render

        layout.operator("render.render", text="Render Image", icon='RENDER_STILL').use_viewport = True
        props = layout.operator("render.render", text="Render Animation", icon='RENDER_ANIMATION')
        props.animation = True
        props.use_viewport = True

        layout.separator()

        layout.operator("sound.mixdown", text="Render Audio...")

        layout.separator()

        layout.operator("render.view_show", text="View Render")
        layout.operator("render.play_rendered_anim", text="View Animation")
        layout.prop_menu_enum(rd, "display_mode", text="Display Mode")

        layout.separator()

        layout.prop(rd, "use_lock_interface", text="Lock Interface")


class INFO_MT_opengl_render(Menu):
    bl_label = "OpenGL Render Options"

    def draw(self, context):
        layout = self.layout

        rd = context.scene.render
        layout.prop(rd, "use_antialiasing")
        layout.prop(rd, "use_full_sample")

        layout.prop_menu_enum(rd, "antialiasing_samples")
        layout.prop_menu_enum(rd, "alpha_mode")


class INFO_MT_edit(Menu):
    bl_label = "Edit"

    def draw(self, context):
        layout = self.layout

        layout.operator("ed.undo")
        layout.operator("ed.redo")

        layout.separator()

        layout.operator("ed.undo_history", text="Undo History...")

        layout.separator()

        layout.operator("screen.repeat_last")
        layout.operator("screen.repeat_history", text="Repeat History...")

        layout.separator()

        layout.operator("screen.redo_last", text="Adjust Last Operation...")

        layout.separator()

        layout.operator("wm.search_menu", text="Operator Search...")

        layout.separator()

        # Should move elsewhere (impacts outliner & 3D view).
        tool_settings = context.tool_settings
        layout.prop(tool_settings, "lock_object_mode")

        layout.separator()

        layout.operator("screen.userpref_show", text="User Preferences...", icon='PREFERENCES')


class INFO_MT_window(Menu):
    bl_label = "Window"

    def draw(self, context):
        import sys

        layout = self.layout

        layout.operator("wm.window_new")
        layout.operator("wm.window_new_main")

        layout.separator()

        layout.operator("wm.window_fullscreen_toggle", icon='FULLSCREEN_ENTER')

        layout.separator()

        layout.operator("screen.workspace_cycle", text="Next Workspace").direction = 'NEXT'
        layout.operator("screen.workspace_cycle", text="Previous Workspace").direction = 'PREV'

        layout.separator()

        layout.operator("screen.screenshot")

        if sys.platform[:3] == "win":
            layout.separator()
            layout.operator("wm.console_toggle", icon='CONSOLE')

        if context.scene.render.use_multiview:
            layout.separator()
            layout.operator("wm.set_stereo_3d")


class INFO_MT_help(Menu):
    bl_label = "Help"

    def draw(self, context):
        layout = self.layout

        show_developer = context.user_preferences.view.show_developer_ui

        layout.operator(
            "wm.url_open", text="Manual", icon='HELP',
        ).url = "https://docs.blender.org/manual/en/dev/"

        layout.operator(
            "wm.url_open", text="Report a Bug", icon='URL',
        ).url = "https://developer.blender.org/maniphest/task/edit/form/1"

        layout.separator()

        layout.operator(
            "wm.url_open", text="User Communities", icon='URL',
        ).url = "https://www.blender.org/community/"
        layout.operator(
            "wm.url_open", text="Developer Community", icon='URL',
        ).url = "https://www.blender.org/get-involved/developers/"

        layout.separator()

        layout.operator(
            "wm.url_open", text="Blender Website", icon='URL',
        ).url = "https://www.blender.org"
        layout.operator(
            "wm.url_open", text="Blender Store", icon='URL',
        ).url = "https://store.blender.org"

        layout.operator(
            "wm.url_open", text="Release Notes", icon='URL',
        ).url = "https://www.blender.org/download/releases/%d-%d/" % bpy.app.version[:2]

        layout.separator()

        if show_developer:
            layout.operator(
                "wm.url_open", text="Python API Reference", icon='URL',
            ).url = bpy.types.WM_OT_doc_view._prefix

            layout.operator("wm.operator_cheat_sheet", icon='TEXT')

        layout.operator("wm.sysinfo")

        layout.separator()

        layout.operator("wm.splash", icon='BLENDER')


class TOPBAR_MT_file_specials(Menu):
    bl_label = "File Context Menu"

    def draw(self, context):
        layout = self.layout

        layout.operator_context = 'INVOKE_AREA'
        layout.operator("wm.read_homefile", text="New", icon='NEW')
        layout.operator("wm.open_mainfile", text="Open...", icon='FILE_FOLDER')

        layout.separator()

        layout.operator("wm.link", text="Link...", icon='LINK_BLEND')
        layout.operator("wm.append", text="Append...", icon='APPEND_BLEND')

        layout.separator()

        layout.menu("INFO_MT_file_import", icon='IMPORT')
        layout.menu("INFO_MT_file_export", icon='EXPORT')


class TOPBAR_MT_window_specials(Menu):
    bl_label = "Window Context Menu"

    def draw(self, context):
        layout = self.layout

        layout.operator_context = 'EXEC_AREA'

        layout.operator("wm.window_new")
        layout.operator("wm.window_new_main")

        layout.operator_context = 'INVOKE_AREA'

        layout.operator("screen.area_dupli")

        layout.operator("wm.window_fullscreen_toggle", icon='FULLSCREEN_ENTER')

        layout.separator()

        layout.operator("screen.area_split", text="Horizontal Split").direction = 'HORIZONTAL'
        layout.operator("screen.area_split", text="Vertical Split").direction = 'VERTICAL'

        layout.separator()

        layout.operator("screen.userpref_show", text="User Preferences...", icon='PREFERENCES')


classes = (
    TOPBAR_HT_upper_bar,
    TOPBAR_HT_lower_bar,
    TOPBAR_MT_file_specials,
    TOPBAR_MT_window_specials,
    INFO_MT_editor_menus,
    INFO_MT_file,
    INFO_MT_file_import,
    INFO_MT_file_export,
    INFO_MT_file_external_data,
    INFO_MT_file_previews,
    INFO_MT_edit,
    INFO_MT_game,
    INFO_MT_render,
    INFO_MT_window,
    INFO_MT_help,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
