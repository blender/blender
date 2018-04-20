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

        TOPBAR_MT_editor_menus.draw_collapsible(context, layout)

        layout.separator()

        if not screen.show_fullscreen:
            layout.template_ID_tabs(window, "workspace", new="workspace.workspace_add_menu", unlink="workspace.workspace_delete")

        layout.separator()

        layout.template_running_jobs()

        layout.template_reports_banner()

        row = layout.row(align=True)

        if bpy.app.autoexec_fail is True and bpy.app.autoexec_fail_quiet is False:
            row.label("Auto-run disabled", icon='ERROR')
            if bpy.data.is_saved:
                props = row.operator("wm.revert_mainfile", icon='SCREEN_BACK', text="Reload Trusted")
                props.use_scripts = True

            row.operator("script.autoexec_warn_clear", text="Ignore")

            # include last so text doesn't push buttons out of the header
            row.label(bpy.app.autoexec_fail_message)
            return

    def draw_right(self, context):
        layout = self.layout

        window = context.window

        layout.template_ID(window, "scene", new="scene.new", unlink="scene.delete")


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
            # WITH_REDO_REGION_REMOVAL:
            # layout.template_operator_redo_props()
            pass

    def draw_left(self, context):
        layout = self.layout
        layer = context.view_layer

        act_mode_item = bpy.types.Object.bl_rna.properties['mode'].enum_items[layer.objects.active.mode]
        layout.operator_menu_enum("object.mode_set", "mode", text=act_mode_item.name, icon=act_mode_item.icon)


    def draw_right(self, context):
        layout = self.layout

        window = context.window
        workspace = context.workspace
        scene = context.scene
        screen = context.screen

        if screen.show_fullscreen:
            layout.operator("screen.back_to_previous", icon='SCREEN_BACK', text="Back to Previous")
        else:
            layout.template_search_preview(window, "screen", workspace, "screens", new="screen.new", unlink="screen.delete", rows=2, cols=6)
        # Active workspace view-layer is retrieved through window, not through workspace.
        layout.template_search(window, "view_layer", scene, "view_layers")


class TOPBAR_MT_editor_menus(Menu):
    bl_idname = "TOPBAR_MT_editor_menus"
    bl_label = ""

    def draw(self, context):
        self.draw_menus(self.layout, context)

    @staticmethod
    def draw_menus(layout, context):
        layout.menu("TOPBAR_MT_file")

        layout.menu("TOPBAR_MT_render")

        layout.menu("TOPBAR_MT_window")
        layout.menu("TOPBAR_MT_help")


class TOPBAR_MT_file(Menu):
    bl_label = "File"

    def draw(self, context):
        layout = self.layout

        layout.operator_context = 'INVOKE_AREA'
        layout.operator("wm.read_homefile", text="New", icon='NEW')
        layout.operator("wm.open_mainfile", text="Open...", icon='FILE_FOLDER')
        layout.menu("TOPBAR_MT_file_open_recent", icon='OPEN_RECENT')
        layout.operator("wm.revert_mainfile", icon='FILE_REFRESH')
        layout.operator("wm.recover_last_session", icon='RECOVER_LAST')
        layout.operator("wm.recover_auto_save", text="Recover Auto Save...", icon='RECOVER_AUTO')

        layout.separator()

        layout.operator_context = 'EXEC_AREA' if context.blend_data.is_saved else 'INVOKE_AREA'
        layout.operator("wm.save_mainfile", text="Save", icon='FILE_TICK')

        layout.operator_context = 'INVOKE_AREA'
        layout.operator("wm.save_as_mainfile", text="Save As...", icon='SAVE_AS')
        layout.operator_context = 'INVOKE_AREA'
        layout.operator("wm.save_as_mainfile", text="Save Copy...", icon='SAVE_COPY').copy = True

        layout.separator()

        layout.operator("screen.userpref_show", text="User Preferences...", icon='PREFERENCES')

        layout.operator_context = 'INVOKE_AREA'
        layout.operator("wm.save_homefile", icon='SAVE_PREFS')
        layout.operator("wm.read_factory_settings", icon='LOAD_FACTORY')

        layout.separator()

        layout.operator_context = 'INVOKE_AREA'
        layout.operator("wm.link", text="Link", icon='LINK_BLEND')
        layout.operator("wm.append", text="Append", icon='APPEND_BLEND')
        layout.menu("TOPBAR_MT_file_previews")

        layout.separator()

        layout.menu("TOPBAR_MT_file_import", icon='IMPORT')
        layout.menu("TOPBAR_MT_file_export", icon='EXPORT')

        layout.separator()

        layout.menu("TOPBAR_MT_file_external_data", icon='EXTERNAL_DATA')

        layout.separator()

        layout.operator_context = 'EXEC_AREA'
        if bpy.data.is_dirty and context.user_preferences.view.use_quit_dialog:
            layout.operator_context = 'INVOKE_SCREEN'  # quit dialog
        layout.operator("wm.quit_blender", text="Quit", icon='QUIT')


class TOPBAR_MT_file_import(Menu):
    bl_idname = "TOPBAR_MT_file_import"
    bl_label = "Import"

    def draw(self, context):
        if bpy.app.build_options.collada:
            self.layout.operator("wm.collada_import", text="Collada (Default) (.dae)")
        if bpy.app.build_options.alembic:
            self.layout.operator("wm.alembic_import", text="Alembic (.abc)")


class TOPBAR_MT_file_export(Menu):
    bl_idname = "TOPBAR_MT_file_export"
    bl_label = "Export"

    def draw(self, context):
        if bpy.app.build_options.collada:
            self.layout.operator("wm.collada_export", text="Collada (Default) (.dae)")
        if bpy.app.build_options.alembic:
            self.layout.operator("wm.alembic_export", text="Alembic (.abc)")


class TOPBAR_MT_file_external_data(Menu):
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


class TOPBAR_MT_file_previews(Menu):
    bl_label = "Data Previews"

    def draw(self, context):
        layout = self.layout

        layout.operator("wm.previews_ensure")
        layout.operator("wm.previews_batch_generate")

        layout.separator()

        layout.operator("wm.previews_clear")
        layout.operator("wm.previews_batch_clear")


class TOPBAR_MT_game(Menu):
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


class TOPBAR_MT_render(Menu):
    bl_label = "Render"

    def draw(self, context):
        layout = self.layout

        layout.operator("render.render", text="Render Image", icon='RENDER_STILL').use_viewport = True
        props = layout.operator("render.render", text="Render Animation", icon='RENDER_ANIMATION')
        props.animation = True
        props.use_viewport = True

        layout.separator()

        layout.operator("render.opengl", text="OpenGL Render Image")
        layout.operator("render.opengl", text="OpenGL Render Animation").animation = True
        layout.menu("TOPBAR_MT_opengl_render")

        layout.separator()

        layout.operator("render.view_show")
        layout.operator("render.play_rendered_anim", icon='PLAY')


class TOPBAR_MT_opengl_render(Menu):
    bl_label = "OpenGL Render Options"

    def draw(self, context):
        layout = self.layout

        rd = context.scene.render
        layout.prop(rd, "use_antialiasing")
        layout.prop(rd, "use_full_sample")

        layout.prop_menu_enum(rd, "antialiasing_samples")
        layout.prop_menu_enum(rd, "alpha_mode")


class TOPBAR_MT_window(Menu):
    bl_label = "Window"

    def draw(self, context):
        import sys

        layout = self.layout

        layout.operator("wm.window_new")
        layout.operator("wm.window_fullscreen_toggle", icon='FULLSCREEN_ENTER')

        layout.separator()

        layout.operator("screen.screenshot")
        layout.operator("screen.screencast")

        if sys.platform[:3] == "win":
            layout.separator()
            layout.operator("wm.console_toggle", icon='CONSOLE')

        if context.scene.render.use_multiview:
            layout.separator()
            layout.operator("wm.set_stereo_3d", icon='CAMERA_STEREO')


class TOPBAR_MT_help(Menu):
    bl_label = "Help"

    def draw(self, context):
        layout = self.layout

        layout.operator(
                "wm.url_open", text="Manual", icon='HELP',
                ).url = "https://docs.blender.org/manual/en/dev/"
        layout.operator(
                "wm.url_open", text="Release Log", icon='URL',
                ).url = "http://wiki.blender.org/index.php/Dev:Ref/Release_Notes/%d.%d" % bpy.app.version[:2]
        layout.separator()

        layout.operator(
                "wm.url_open", text="Blender Website", icon='URL',
                ).url = "https://www.blender.org"
        layout.operator(
                "wm.url_open", text="Blender Store", icon='URL',
                ).url = "https://store.blender.org"
        layout.operator(
                "wm.url_open", text="Developer Community", icon='URL',
                ).url = "https://www.blender.org/get-involved/"
        layout.operator(
                "wm.url_open", text="User Community", icon='URL',
                ).url = "https://www.blender.org/support/user-community"
        layout.separator()
        layout.operator(
                "wm.url_open", text="Report a Bug", icon='URL',
                ).url = "https://developer.blender.org/maniphest/task/edit/form/1"
        layout.separator()

        layout.operator(
                "wm.url_open", text="Python API Reference", icon='URL',
                ).url = bpy.types.WM_OT_doc_view._prefix

        layout.operator("wm.operator_cheat_sheet", icon='TEXT')
        layout.operator("wm.sysinfo", icon='TEXT')
        layout.separator()

        layout.operator("wm.splash", icon='BLENDER')


classes = (
    TOPBAR_HT_upper_bar,
    TOPBAR_HT_lower_bar,
    TOPBAR_MT_editor_menus,
    TOPBAR_MT_file,
    TOPBAR_MT_file_import,
    TOPBAR_MT_file_export,
    TOPBAR_MT_file_external_data,
    TOPBAR_MT_file_previews,
    TOPBAR_MT_game,
    TOPBAR_MT_render,
    TOPBAR_MT_opengl_render,
    TOPBAR_MT_window,
    TOPBAR_MT_help,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
