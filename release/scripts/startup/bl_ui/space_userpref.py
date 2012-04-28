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
import os
import addon_utils


def ui_items_general(col, context):
    """ General UI Theme Settings (User Interface)
    """

    row = col.row()

    subsplit = row.split(percentage=0.95)

    padding = subsplit.split(percentage=0.15)
    colsub = padding.column()
    colsub = padding.column()
    colsub.row().prop(context, "outline")
    colsub.row().prop(context, "item", slider=True)
    colsub.row().prop(context, "inner", slider=True)
    colsub.row().prop(context, "inner_sel", slider=True)

    subsplit = row.split(percentage=0.85)

    padding = subsplit.split(percentage=0.15)
    colsub = padding.column()
    colsub = padding.column()
    colsub.row().prop(context, "text")
    colsub.row().prop(context, "text_sel")
    colsub.prop(context, "show_shaded")
    subsub = colsub.column(align=True)
    subsub.active = context.show_shaded
    subsub.prop(context, "shadetop")
    subsub.prop(context, "shadedown")

    col.separator()


def opengl_lamp_buttons(column, lamp):
    split = column.split(percentage=0.1)

    split.prop(lamp, "use", text="", icon='OUTLINER_OB_LAMP' if lamp.use else 'LAMP_DATA')

    col = split.column()
    col.active = lamp.use
    row = col.row()
    row.label(text="Diffuse:")
    row.prop(lamp, "diffuse_color", text="")
    row = col.row()
    row.label(text="Specular:")
    row.prop(lamp, "specular_color", text="")

    col = split.column()
    col.active = lamp.use
    col.prop(lamp, "direction", text="")


class USERPREF_HT_header(Header):
    bl_space_type = 'USER_PREFERENCES'

    def draw(self, context):
        layout = self.layout
        layout.template_header(menus=False)

        userpref = context.user_preferences

        layout.operator_context = 'EXEC_AREA'
        layout.operator("wm.save_homefile", text="Save As Default")

        layout.operator_context = 'INVOKE_DEFAULT'

        if userpref.active_section == 'INPUT':
            layout.operator("wm.keyconfig_import")
            layout.operator("wm.keyconfig_export")
        elif userpref.active_section == 'ADDONS':
            layout.operator("wm.addon_install")
            layout.menu("USERPREF_MT_addons_dev_guides")
        elif userpref.active_section == 'THEMES':
            layout.operator("ui.reset_default_theme")


class USERPREF_PT_tabs(Panel):
    bl_label = ""
    bl_space_type = 'USER_PREFERENCES'
    bl_region_type = 'WINDOW'
    bl_options = {'HIDE_HEADER'}

    def draw(self, context):
        layout = self.layout

        userpref = context.user_preferences

        layout.prop(userpref, "active_section", expand=True)


class USERPREF_MT_interaction_presets(Menu):
    bl_label = "Presets"
    preset_subdir = "interaction"
    preset_operator = "script.execute_preset"
    draw = Menu.draw_preset


class USERPREF_MT_appconfigs(Menu):
    bl_label = "AppPresets"
    preset_subdir = "keyconfig"
    preset_operator = "wm.appconfig_activate"

    def draw(self, context):
        self.layout.operator("wm.appconfig_default", text="Blender (default)")

        # now draw the presets
        Menu.draw_preset(self, context)


class USERPREF_MT_splash(Menu):
    bl_label = "Splash"

    def draw(self, context):
        layout = self.layout
        split = layout.split()
        row = split.row()
        row.label("")
        row = split.row()
        row.label("Interaction:")
        # XXX, no redraws
        # text = bpy.path.display_name(context.window_manager.keyconfigs.active.name)
        # if not text:
        #     text = "Blender (default)"
        row.menu("USERPREF_MT_appconfigs", text="Preset")


class USERPREF_PT_interface(Panel):
    bl_space_type = 'USER_PREFERENCES'
    bl_label = "Interface"
    bl_region_type = 'WINDOW'
    bl_options = {'HIDE_HEADER'}

    @classmethod
    def poll(cls, context):
        userpref = context.user_preferences
        return (userpref.active_section == 'INTERFACE')

    def draw(self, context):
        layout = self.layout

        userpref = context.user_preferences
        view = userpref.view

        row = layout.row()

        col = row.column()
        col.label(text="Display:")
        col.prop(view, "show_tooltips")
        col.prop(view, "show_tooltips_python")
        col.prop(view, "show_object_info", text="Object Info")
        col.prop(view, "show_large_cursors")
        col.prop(view, "show_view_name", text="View Name")
        col.prop(view, "show_playback_fps", text="Playback FPS")
        col.prop(view, "use_global_scene")
        col.prop(view, "object_origin_size")

        col.separator()
        col.separator()
        col.separator()

        col.prop(view, "show_mini_axis", text="Display Mini Axis")
        sub = col.column()
        sub.active = view.show_mini_axis
        sub.prop(view, "mini_axis_size", text="Size")
        sub.prop(view, "mini_axis_brightness", text="Brightness")

        col.separator()
        row.separator()
        row.separator()

        col = row.column()
        col.label(text="View Manipulation:")
        col.prop(view, "use_mouse_auto_depth")
        col.prop(view, "use_zoom_to_mouse")
        col.prop(view, "use_rotate_around_active")
        col.prop(view, "use_global_pivot")
        col.prop(view, "use_camera_lock_parent")

        col.separator()

        col.prop(view, "use_auto_perspective")
        col.prop(view, "smooth_view")
        col.prop(view, "rotation_angle")

        col.separator()
        col.separator()

        col.label(text="2D Viewports:")
        col.prop(view, "view2d_grid_spacing_min", text="Minimum Grid Spacing")
        col.prop(view, "timecode_style")

        row.separator()
        row.separator()

        col = row.column()
        #Toolbox doesn't exist yet
        #col.label(text="Toolbox:")
        #col.prop(view, "show_column_layout")
        #col.label(text="Open Toolbox Delay:")
        #col.prop(view, "open_left_mouse_delay", text="Hold LMB")
        #col.prop(view, "open_right_mouse_delay", text="Hold RMB")
        col.prop(view, "show_manipulator")
        sub = col.column()
        sub.active = view.show_manipulator
        sub.prop(view, "manipulator_size", text="Size")
        sub.prop(view, "manipulator_handle_size", text="Handle Size")
        sub.prop(view, "manipulator_hotspot", text="Hotspot")

        col.separator()
        col.separator()
        col.separator()

        col.label(text="Menus:")
        col.prop(view, "use_mouse_over_open")
        col.label(text="Menu Open Delay:")
        col.prop(view, "open_toplevel_delay", text="Top Level")
        col.prop(view, "open_sublevel_delay", text="Sub Level")

        col.separator()

        col.prop(view, "show_splash")

        if os.name == 'nt':
            col.prop(view, "quit_dialog")


class USERPREF_PT_edit(Panel):
    bl_space_type = 'USER_PREFERENCES'
    bl_label = "Edit"
    bl_region_type = 'WINDOW'
    bl_options = {'HIDE_HEADER'}

    @classmethod
    def poll(cls, context):
        userpref = context.user_preferences
        return (userpref.active_section == 'EDITING')

    def draw(self, context):
        layout = self.layout

        userpref = context.user_preferences
        edit = userpref.edit

        row = layout.row()

        col = row.column()
        col.label(text="Link Materials To:")
        col.prop(edit, "material_link", text="")

        col.separator()
        col.separator()
        col.separator()

        col.label(text="New Objects:")
        col.prop(edit, "use_enter_edit_mode")
        col.label(text="Align To:")
        col.prop(edit, "object_align", text="")

        col.separator()
        col.separator()
        col.separator()

        col.label(text="Undo:")
        col.prop(edit, "use_global_undo")
        col.prop(edit, "undo_steps", text="Steps")
        col.prop(edit, "undo_memory_limit", text="Memory Limit")

        row.separator()
        row.separator()

        col = row.column()
        col.label(text="Grease Pencil:")
        col.prop(edit, "grease_pencil_manhattan_distance", text="Manhattan Distance")
        col.prop(edit, "grease_pencil_euclidean_distance", text="Euclidean Distance")
        #~ col.prop(edit, "use_grease_pencil_simplify_stroke", text="Simplify Stroke")
        col.prop(edit, "grease_pencil_eraser_radius", text="Eraser Radius")
        col.prop(edit, "use_grease_pencil_smooth_stroke", text="Smooth Stroke")
        col.separator()
        col.separator()
        col.separator()
        col.label(text="Playback:")
        col.prop(edit, "use_negative_frames")
        col.separator()
        col.separator()
        col.separator()
        col.label(text="Animation Editors:")
        col.prop(edit, "fcurve_unselected_alpha", text="F-Curve Visibility")

        row.separator()
        row.separator()

        col = row.column()
        col.label(text="Keyframing:")
        col.prop(edit, "use_visual_keying")
        col.prop(edit, "use_keyframe_insert_needed", text="Only Insert Needed")

        col.separator()

        col.prop(edit, "use_auto_keying", text="Auto Keyframing:")

        sub = col.column()

        #~ sub.active = edit.use_keyframe_insert_auto # incorrect, time-line can enable
        sub.prop(edit, "use_keyframe_insert_available", text="Only Insert Available")

        col.separator()

        col.label(text="New F-Curve Defaults:")
        col.prop(edit, "keyframe_new_interpolation_type", text="Interpolation")
        col.prop(edit, "keyframe_new_handle_type", text="Handles")
        col.prop(edit, "use_insertkey_xyz_to_rgb", text="XYZ to RGB")

        col.separator()
        col.separator()
        col.separator()

        col.label(text="Transform:")
        col.prop(edit, "use_drag_immediately")

        row.separator()
        row.separator()

        col = row.column()
        col.prop(edit, "sculpt_paint_overlay_color", text="Sculpt Overlay Color")

        col.separator()
        col.separator()
        col.separator()

        col.label(text="Duplicate Data:")
        col.prop(edit, "use_duplicate_mesh", text="Mesh")
        col.prop(edit, "use_duplicate_surface", text="Surface")
        col.prop(edit, "use_duplicate_curve", text="Curve")
        col.prop(edit, "use_duplicate_text", text="Text")
        col.prop(edit, "use_duplicate_metaball", text="Metaball")
        col.prop(edit, "use_duplicate_armature", text="Armature")
        col.prop(edit, "use_duplicate_lamp", text="Lamp")
        col.prop(edit, "use_duplicate_material", text="Material")
        col.prop(edit, "use_duplicate_texture", text="Texture")
        #col.prop(edit, "use_duplicate_fcurve", text="F-Curve")
        col.prop(edit, "use_duplicate_action", text="Action")
        col.prop(edit, "use_duplicate_particle", text="Particle")


class USERPREF_PT_system(Panel):
    bl_space_type = 'USER_PREFERENCES'
    bl_label = "System"
    bl_region_type = 'WINDOW'
    bl_options = {'HIDE_HEADER'}

    @classmethod
    def poll(cls, context):
        userpref = context.user_preferences
        return (userpref.active_section == 'SYSTEM')

    def draw(self, context):
        layout = self.layout

        userpref = context.user_preferences
        system = userpref.system

        split = layout.split()

        # 1. Column
        column = split.column()
        colsplit = column.split(percentage=0.85)

        col = colsplit.column()
        col.label(text="General:")
        col.prop(system, "dpi")
        col.prop(system, "frame_server_port")
        col.prop(system, "scrollback", text="Console Scrollback")

        col.separator()
        col.separator()

        col.label(text="Sound:")
        col.row().prop(system, "audio_device", expand=True)
        sub = col.column()
        sub.active = system.audio_device != 'NONE'
        #sub.prop(system, "use_preview_images")
        sub.prop(system, "audio_channels", text="Channels")
        sub.prop(system, "audio_mixing_buffer", text="Mixing Buffer")
        sub.prop(system, "audio_sample_rate", text="Sample Rate")
        sub.prop(system, "audio_sample_format", text="Sample Format")

        col.separator()
        col.separator()

        col.label(text="Screencast:")
        col.prop(system, "screencast_fps")
        col.prop(system, "screencast_wait_time")

        col.separator()
        col.separator()

        if hasattr(system, 'compute_device'):
            col.label(text="Compute Device:")
            col.row().prop(system, "compute_device_type", expand=True)
            sub = col.row()
            sub.active = system.compute_device_type != 'CPU'
            sub.prop(system, "compute_device", text="")

        # 2. Column
        column = split.column()
        colsplit = column.split(percentage=0.85)

        col = colsplit.column()
        col.label(text="OpenGL:")
        col.prop(system, "gl_clip_alpha", slider=True)
        col.prop(system, "use_mipmaps")
        col.prop(system, "use_16bit_textures")
        col.label(text="Anisotropic Filtering")
        col.prop(system, "anisotropic_filter", text="")
        col.prop(system, "use_vertex_buffer_objects")
        # Anti-aliasing is disabled as it breaks border/lasso select
        #~ col.prop(system, "use_antialiasing")
        col.label(text="Window Draw Method:")
        col.prop(system, "window_draw_method", text="")
        col.label(text="Text Draw Options:")
        col.prop(system, "use_text_antialiasing")
        col.label(text="Textures:")
        col.prop(system, "gl_texture_limit", text="Limit Size")
        col.prop(system, "texture_time_out", text="Time Out")
        col.prop(system, "texture_collection_rate", text="Collection Rate")

        col.separator()
        col.separator()
        col.separator()

        col.label(text="Sequencer:")
        col.prop(system, "prefetch_frames")
        col.prop(system, "memory_cache_limit")

        # 3. Column
        column = split.column()

        column.label(text="Solid OpenGL lights:")

        split = column.split(percentage=0.1)
        split.label()
        split.label(text="Colors:")
        split.label(text="Direction:")

        lamp = system.solid_lights[0]
        opengl_lamp_buttons(column, lamp)

        lamp = system.solid_lights[1]
        opengl_lamp_buttons(column, lamp)

        lamp = system.solid_lights[2]
        opengl_lamp_buttons(column, lamp)

        column.separator()

        column.label(text="Color Picker Type:")
        column.row().prop(system, "color_picker_type", text="")

        column.separator()

        column.prop(system, "use_weight_color_range", text="Custom Weight Paint Range")
        sub = column.column()
        sub.active = system.use_weight_color_range
        sub.template_color_ramp(system, "weight_color_range", expand=True)

        column.separator()

        column.prop(system, "use_international_fonts")
        if system.use_international_fonts:
            column.prop(system, "language")
            row = column.row()
            row.label(text="Translate:")
            row.prop(system, "use_translate_interface", text="Interface")
            row.prop(system, "use_translate_tooltips", text="Tooltips")


class USERPREF_MT_interface_theme_presets(Menu):
    bl_label = "Presets"
    preset_subdir = "interface_theme"
    preset_operator = "script.execute_preset"
    preset_type = 'XML'
    preset_xml_map = (("user_preferences.themes[0]", "Theme"), )
    draw = Menu.draw_preset


class USERPREF_PT_theme(Panel):
    bl_space_type = 'USER_PREFERENCES'
    bl_label = "Themes"
    bl_region_type = 'WINDOW'
    bl_options = {'HIDE_HEADER'}

    @staticmethod
    def _theme_generic(split, themedata):

        col = split.column()

        def theme_generic_recurse(data):
            col.label(data.rna_type.name)
            row = col.row()
            subsplit = row.split(percentage=0.95)

            padding1 = subsplit.split(percentage=0.15)
            padding1.column()

            subsplit = row.split(percentage=0.85)

            padding2 = subsplit.split(percentage=0.15)
            padding2.column()

            colsub_pair = padding1.column(), padding2.column()

            props_type = {}

            for i, prop in enumerate(data.rna_type.properties):
                if prop.identifier == "rna_type":
                    continue

                props_type.setdefault((prop.type, prop.subtype), []).append(prop)

            for props_type, props_ls in sorted(props_type.items()):
                if props_type[0] == 'POINTER':
                    for i, prop in enumerate(props_ls):
                        theme_generic_recurse(getattr(data, prop.identifier))
                else:
                    for i, prop in enumerate(props_ls):
                        colsub_pair[i % 2].row().prop(data, prop.identifier)

        theme_generic_recurse(themedata)

    @classmethod
    def poll(cls, context):
        userpref = context.user_preferences
        return (userpref.active_section == 'THEMES')

    def draw(self, context):
        layout = self.layout

        theme = context.user_preferences.themes[0]

        split_themes = layout.split(percentage=0.2)

        sub = split_themes.column()

        sub.label(text="Presets:")
        subrow = sub.row(align=True)

        subrow.menu("USERPREF_MT_interface_theme_presets", text=USERPREF_MT_interface_theme_presets.bl_label)
        subrow.operator("wm.interface_theme_preset_add", text="", icon='ZOOMIN')
        subrow.operator("wm.interface_theme_preset_add", text="", icon='ZOOMOUT').remove_active = True
        sub.separator()

        sub.prop(theme, "theme_area", expand=True)

        split = layout.split(percentage=0.4)

        layout.separator()
        layout.separator()

        split = split_themes.split()

        if theme.theme_area == 'USER_INTERFACE':
            col = split.column()

            ui = theme.user_interface.wcol_regular
            col.label(text="Regular:")
            ui_items_general(col, ui)

            ui = theme.user_interface.wcol_tool
            col.label(text="Tool:")
            ui_items_general(col, ui)

            ui = theme.user_interface.wcol_radio
            col.label(text="Radio Buttons:")
            ui_items_general(col, ui)

            ui = theme.user_interface.wcol_text
            col.label(text="Text:")
            ui_items_general(col, ui)

            ui = theme.user_interface.wcol_option
            col.label(text="Option:")
            ui_items_general(col, ui)

            ui = theme.user_interface.wcol_toggle
            col.label(text="Toggle:")
            ui_items_general(col, ui)

            ui = theme.user_interface.wcol_num
            col.label(text="Number Field:")
            ui_items_general(col, ui)

            ui = theme.user_interface.wcol_numslider
            col.label(text="Value Slider:")
            ui_items_general(col, ui)

            ui = theme.user_interface.wcol_box
            col.label(text="Box:")
            ui_items_general(col, ui)

            ui = theme.user_interface.wcol_menu
            col.label(text="Menu:")
            ui_items_general(col, ui)

            ui = theme.user_interface.wcol_pulldown
            col.label(text="Pulldown:")
            ui_items_general(col, ui)

            ui = theme.user_interface.wcol_menu_back
            col.label(text="Menu Back:")
            ui_items_general(col, ui)

            ui = theme.user_interface.wcol_tooltip
            col.label(text="Tooltip:")
            ui_items_general(col, ui)

            ui = theme.user_interface.wcol_tooltip
            col.label(text="Tooltip:")
            ui_items_general(col, ui)

            ui = theme.user_interface.wcol_menu_item
            col.label(text="Menu Item:")
            ui_items_general(col, ui)

            ui = theme.user_interface.wcol_scroll
            col.label(text="Scroll Bar:")
            ui_items_general(col, ui)

            ui = theme.user_interface.wcol_progress
            col.label(text="Progress Bar:")
            ui_items_general(col, ui)

            ui = theme.user_interface.wcol_list_item
            col.label(text="List Item:")
            ui_items_general(col, ui)

            ui = theme.user_interface.wcol_state
            col.label(text="State:")

            row = col.row()

            subsplit = row.split(percentage=0.95)

            padding = subsplit.split(percentage=0.15)
            colsub = padding.column()
            colsub = padding.column()
            colsub.row().prop(ui, "inner_anim")
            colsub.row().prop(ui, "inner_anim_sel")
            colsub.row().prop(ui, "inner_driven")
            colsub.row().prop(ui, "inner_driven_sel")

            subsplit = row.split(percentage=0.85)

            padding = subsplit.split(percentage=0.15)
            colsub = padding.column()
            colsub = padding.column()
            colsub.row().prop(ui, "inner_key")
            colsub.row().prop(ui, "inner_key_sel")
            colsub.row().prop(ui, "blend")

            col.separator()
            col.separator()

            ui = theme.user_interface
            col.label("Icons:")

            row = col.row()

            subsplit = row.split(percentage=0.95)

            padding = subsplit.split(percentage=0.15)
            colsub = padding.column()
            colsub = padding.column()
            colsub.row().prop(ui, "icon_file")

            subsplit = row.split(percentage=0.85)

            padding = subsplit.split(percentage=0.15)
            colsub = padding.column()
            colsub = padding.column()
            colsub.row().prop(ui, "icon_alpha")

            col.separator()
            col.separator()

            ui = theme.user_interface.panel
            col.label("Panels:")

            row = col.row()

            subsplit = row.split(percentage=0.95)

            padding = subsplit.split(percentage=0.15)
            colsub = padding.column()
            colsub = padding.column()
            rowsub = colsub.row()
            rowsub.prop(ui, "show_header")
            rowsub.label()

            subsplit = row.split(percentage=0.85)

            padding = subsplit.split(percentage=0.15)
            colsub = padding.column()
            colsub = padding.column()
            colsub.row().prop(ui, "header")

            layout.separator()
            layout.separator()
        elif theme.theme_area == 'BONE_COLOR_SETS':
            col = split.column()

            for i, ui in enumerate(theme.bone_color_sets):
                col.label(text="Color Set" + " %d:" % (i + 1))  # i starts from 0

                row = col.row()

                subsplit = row.split(percentage=0.95)

                padding = subsplit.split(percentage=0.15)
                colsub = padding.column()
                colsub = padding.column()
                colsub.row().prop(ui, "normal")
                colsub.row().prop(ui, "select")
                colsub.row().prop(ui, "active")

                subsplit = row.split(percentage=0.85)

                padding = subsplit.split(percentage=0.15)
                colsub = padding.column()
                colsub = padding.column()
                colsub.row().prop(ui, "show_colored_constraints")
        else:
            self._theme_generic(split, getattr(theme, theme.theme_area.lower()))


class USERPREF_PT_file(Panel):
    bl_space_type = 'USER_PREFERENCES'
    bl_label = "Files"
    bl_region_type = 'WINDOW'
    bl_options = {'HIDE_HEADER'}

    @classmethod
    def poll(cls, context):
        userpref = context.user_preferences
        return (userpref.active_section == 'FILES')

    def draw(self, context):
        layout = self.layout

        userpref = context.user_preferences
        paths = userpref.filepaths
        system = userpref.system

        split = layout.split(percentage=0.7)

        col = split.column()
        col.label(text="File Paths:")

        colsplit = col.split(percentage=0.95)
        col1 = colsplit.split(percentage=0.3)

        sub = col1.column()
        sub.label(text="Fonts:")
        sub.label(text="Textures:")
        sub.label(text="Render Output:")
        sub.label(text="Scripts:")
        sub.label(text="Sounds:")
        sub.label(text="Temp:")
        sub.label(text="Image Editor:")
        sub.label(text="Animation Player:")

        sub = col1.column()
        sub.prop(paths, "font_directory", text="")
        sub.prop(paths, "texture_directory", text="")
        sub.prop(paths, "render_output_directory", text="")
        sub.prop(paths, "script_directory", text="")
        sub.prop(paths, "sound_directory", text="")
        sub.prop(paths, "temporary_directory", text="")
        sub.prop(paths, "image_editor", text="")
        subsplit = sub.split(percentage=0.3)
        subsplit.prop(paths, "animation_player_preset", text="")
        subsplit.prop(paths, "animation_player", text="")

        col.separator()
        col.separator()

        colsplit = col.split(percentage=0.95)
        sub = colsplit.column()
        sub.label(text="Author:")
        sub.prop(system, "author", text="")

        col = split.column()
        col.label(text="Save & Load:")
        col.prop(paths, "use_relative_paths")
        col.prop(paths, "use_file_compression")
        col.prop(paths, "use_load_ui")
        col.prop(paths, "use_filter_files")
        col.prop(paths, "show_hidden_files_datablocks")
        col.prop(paths, "hide_recent_locations")
        col.prop(paths, "show_thumbnails")

        col.separator()
        col.separator()

        col.prop(paths, "save_version")
        col.prop(paths, "recent_files")
        col.prop(paths, "use_save_preview_images")
        col.label(text="Auto Save:")
        col.prop(paths, "use_auto_save_temporary_files")
        sub = col.column()
        sub.active = paths.use_auto_save_temporary_files
        sub.prop(paths, "auto_save_time", text="Timer (mins)")

        col.separator()

        col.label(text="Scripts:")
        col.prop(system, "use_scripts_auto_execute")
        col.prop(system, "use_tabs_as_spaces")


from bl_ui.space_userpref_keymap import InputKeyMapPanel


class USERPREF_MT_ndof_settings(Menu):
    # accessed from the window key-bindings in C (only)
    bl_label = "3D Mouse Settings"

    def draw(self, context):
        layout = self.layout
        input_prefs = context.user_preferences.inputs

        layout.separator()
        layout.prop(input_prefs, "ndof_sensitivity")

        if context.space_data.type == 'VIEW_3D':
            layout.separator()
            layout.prop(input_prefs, "ndof_show_guide")

            layout.separator()
            layout.label(text="Orbit options")
            if input_prefs.view_rotate_method == 'TRACKBALL':
                layout.prop(input_prefs, "ndof_roll_invert_axis")
            layout.prop(input_prefs, "ndof_tilt_invert_axis")
            layout.prop(input_prefs, "ndof_rotate_invert_axis")
            layout.prop(input_prefs, "ndof_zoom_invert")

            layout.separator()
            layout.label(text="Pan options")
            layout.prop(input_prefs, "ndof_panx_invert_axis")
            layout.prop(input_prefs, "ndof_pany_invert_axis")
            layout.prop(input_prefs, "ndof_panz_invert_axis")

            layout.label(text="Zoom options")
            layout.prop(input_prefs, "ndof_zoom_updown")

            layout.separator()
            layout.label(text="Fly options")
            layout.prop(input_prefs, "ndof_fly_helicopter", icon='NDOF_FLY')
            layout.prop(input_prefs, "ndof_lock_horizon", icon='NDOF_DOM')


class USERPREF_PT_input(Panel, InputKeyMapPanel):
    bl_space_type = 'USER_PREFERENCES'
    bl_label = "Input"

    @classmethod
    def poll(cls, context):
        userpref = context.user_preferences
        return (userpref.active_section == 'INPUT')

    def draw_input_prefs(self, inputs, layout):
        # General settings
        row = layout.row()
        col = row.column()

        sub = col.column()
        sub.label(text="Presets:")
        subrow = sub.row(align=True)

        subrow.menu("USERPREF_MT_interaction_presets", text=bpy.types.USERPREF_MT_interaction_presets.bl_label)
        subrow.operator("wm.interaction_preset_add", text="", icon='ZOOMIN')
        subrow.operator("wm.interaction_preset_add", text="", icon='ZOOMOUT').remove_active = True
        sub.separator()

        sub.label(text="Mouse:")
        sub1 = sub.column()
        sub1.active = (inputs.select_mouse == 'RIGHT')
        sub1.prop(inputs, "use_mouse_emulate_3_button")
        sub.prop(inputs, "use_mouse_continuous")
        sub.prop(inputs, "drag_threshold")
        sub.prop(inputs, "tweak_threshold")

        sub.label(text="Select With:")
        sub.row().prop(inputs, "select_mouse", expand=True)

        sub = col.column()
        sub.label(text="Double Click:")
        sub.prop(inputs, "mouse_double_click_time", text="Speed")

        sub.separator()

        sub.prop(inputs, "use_emulate_numpad")

        sub.separator()

        sub.label(text="Orbit Style:")
        sub.row().prop(inputs, "view_rotate_method", expand=True)

        sub.label(text="Zoom Style:")
        sub.row().prop(inputs, "view_zoom_method", text="")
        if inputs.view_zoom_method in {'DOLLY', 'CONTINUE'}:
            sub.row().prop(inputs, "view_zoom_axis", expand=True)
            sub.prop(inputs, "invert_mouse_zoom")

        #sub.prop(inputs, "use_mouse_mmb_paste")

        #col.separator()

        sub = col.column()
        sub.label(text="Mouse Wheel:")
        sub.prop(inputs, "invert_zoom_wheel", text="Invert Wheel Zoom Direction")
        #sub.prop(view, "wheel_scroll_lines", text="Scroll Lines")

        col.separator()
        sub = col.column()
        sub.label(text="NDOF Device:")
        sub.prop(inputs, "ndof_sensitivity", text="NDOF Sensitivity")

        row.separator()

    def draw(self, context):
        layout = self.layout

        #import time

        #start = time.time()

        userpref = context.user_preferences

        inputs = userpref.inputs

        split = layout.split(percentage=0.25)

        # Input settings
        self.draw_input_prefs(inputs, split)

        # Keymap Settings
        self.draw_keymaps(context, split)

        #print("runtime", time.time() - start)


class USERPREF_MT_addons_dev_guides(Menu):
    bl_label = "Development Guides"

    # menu to open web-pages with addons development guides
    def draw(self, context):
        layout = self.layout
        layout.operator("wm.url_open", text="API Concepts", icon='URL').url = "http://wiki.blender.org/index.php/Dev:2.5/Py/API/Intro"
        layout.operator("wm.url_open", text="Addon Guidelines", icon='URL').url = "http://wiki.blender.org/index.php/Dev:2.5/Py/Scripts/Guidelines/Addons"
        layout.operator("wm.url_open", text="How to share your addon", icon='URL').url = "http://wiki.blender.org/index.php/Dev:Py/Sharing"


class USERPREF_PT_addons(Panel):
    bl_space_type = 'USER_PREFERENCES'
    bl_label = "Addons"
    bl_region_type = 'WINDOW'
    bl_options = {'HIDE_HEADER'}

    _support_icon_mapping = {
        'OFFICIAL': 'FILE_BLEND',
        'COMMUNITY': 'POSE_DATA',
        'TESTING': 'MOD_EXPLODE',
        }

    @classmethod
    def poll(cls, context):
        userpref = context.user_preferences
        return (userpref.active_section == 'ADDONS')

    @staticmethod
    def is_user_addon(mod, user_addon_paths):
        if not user_addon_paths:
            user_script_path = bpy.utils.user_script_path()
            if user_script_path is not None:
                user_addon_paths.append(os.path.join(user_script_path, "addons"))
            user_addon_paths.append(os.path.join(bpy.utils.resource_path('USER'), "scripts", "addons"))

        for path in user_addon_paths:
            if bpy.path.is_subdir(mod.__file__, path):
                return True
        return False

    @staticmethod
    def draw_error(layout, message):
        lines = message.split("\n")
        box = layout.box()
        rowsub = box.row()
        rowsub.label(lines[0])
        rowsub.label(icon='ERROR')
        for l in lines[1:]:
            box.label(l)

    def draw(self, context):
        layout = self.layout

        userpref = context.user_preferences
        used_ext = {ext.module for ext in userpref.addons}

        # collect the categories that can be filtered on
        addons = [(mod, addon_utils.module_bl_info(mod)) for mod in addon_utils.modules(addon_utils.addons_fake_modules)]

        split = layout.split(percentage=0.2)
        col = split.column()
        col.prop(context.window_manager, "addon_search", text="", icon='VIEWZOOM')

        col.label(text="Supported Level")
        col.prop(context.window_manager, "addon_support", expand=True)

        col.label(text="Categories")
        col.prop(context.window_manager, "addon_filter", expand=True)

        col = split.column()

        # set in addon_utils.modules(...)
        if addon_utils.error_duplicates:
            self.draw_error(col,
                            "Multiple addons using the same name found!\n"
                            "likely a problem with the script search path.\n"
                            "(see console for details)",
                            )

        if addon_utils.error_encoding:
            self.draw_error(col,
                            "One or more addons do not have UTF-8 encoding\n"
                            "(see console for details)",
                            )

        filter = context.window_manager.addon_filter
        search = context.window_manager.addon_search.lower()
        support = context.window_manager.addon_support

        # initialized on demand
        user_addon_paths = []

        for mod, info in addons:
            module_name = mod.__name__

            is_enabled = module_name in used_ext

            if info["support"] not in support:
                continue

            # check if addon should be visible with current filters
            if ((filter == "All") or
                (filter == info["category"]) or
                (filter == "Enabled" and is_enabled) or
                (filter == "Disabled" and not is_enabled)):

                if search and search not in info["name"].lower():
                    if info["author"]:
                        if search not in info["author"].lower():
                            continue
                    else:
                        continue

                # Addon UI Code
                box = col.column().box()
                colsub = box.column()
                row = colsub.row()

                row.operator("wm.addon_expand", icon='TRIA_DOWN' if info["show_expanded"] else 'TRIA_RIGHT', emboss=False).module = module_name

                rowsub = row.row()
                rowsub.active = is_enabled
                rowsub.label(text='%s: %s' % (info['category'], info["name"]))
                if info["warning"]:
                    rowsub.label(icon='ERROR')

                # icon showing support level.
                rowsub.label(icon=self._support_icon_mapping.get(info["support"], 'QUESTION'))

                if is_enabled:
                    row.operator("wm.addon_disable", icon='CHECKBOX_HLT', text="", emboss=False).module = module_name
                else:
                    row.operator("wm.addon_enable", icon='CHECKBOX_DEHLT', text="", emboss=False).module = module_name

                # Expanded UI (only if additional info is available)
                if info["show_expanded"]:
                    if info["description"]:
                        split = colsub.row().split(percentage=0.15)
                        split.label(text="Description:")
                        split.label(text=info["description"])
                    if info["location"]:
                        split = colsub.row().split(percentage=0.15)
                        split.label(text="Location:")
                        split.label(text=info["location"])
                    if mod:
                        split = colsub.row().split(percentage=0.15)
                        split.label(text="File:")
                        split.label(text=mod.__file__)
                    if info["author"]:
                        split = colsub.row().split(percentage=0.15)
                        split.label(text="Author:")
                        split.label(text=info["author"])
                    if info["version"]:
                        split = colsub.row().split(percentage=0.15)
                        split.label(text="Version:")
                        split.label(text='.'.join(str(x) for x in info["version"]))
                    if info["warning"]:
                        split = colsub.row().split(percentage=0.15)
                        split.label(text="Warning:")
                        split.label(text='  ' + info["warning"], icon='ERROR')

                    user_addon = USERPREF_PT_addons.is_user_addon(mod, user_addon_paths)
                    tot_row = bool(info["wiki_url"]) + bool(info["tracker_url"]) + bool(user_addon)

                    if tot_row:
                        split = colsub.row().split(percentage=0.15)
                        split.label(text="Internet:")
                        if info["wiki_url"]:
                            split.operator("wm.url_open", text="Link to the Wiki", icon='HELP').url = info["wiki_url"]
                        if info["tracker_url"]:
                            split.operator("wm.url_open", text="Report a Bug", icon='URL').url = info["tracker_url"]
                        if user_addon:
                            split.operator("wm.addon_remove", text="Remove", icon='CANCEL').module = mod.__name__

                        for i in range(4 - tot_row):
                            split.separator()

        # Append missing scripts
        # First collect scripts that are used but have no script file.
        module_names = {mod.__name__ for mod, info in addons}
        missing_modules = {ext for ext in used_ext if ext not in module_names}

        if missing_modules and filter in {"All", "Enabled"}:
            col.column().separator()
            col.column().label(text="Missing script files")

            module_names = {mod.__name__ for mod, info in addons}
            for module_name in sorted(missing_modules):
                is_enabled = module_name in used_ext
                # Addon UI Code
                box = col.column().box()
                colsub = box.column()
                row = colsub.row()

                row.label(text=module_name, icon='ERROR')

                if is_enabled:
                    row.operator("wm.addon_disable", icon='CHECKBOX_HLT', text="", emboss=False).module = module_name

if __name__ == "__main__":  # only for live edit.
    bpy.utils.register_module(__name__)
