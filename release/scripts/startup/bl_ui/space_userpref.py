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
import os
import addon_utils

from bpy.props import StringProperty, BoolProperty, EnumProperty


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


class USERPREF_HT_header(bpy.types.Header):
    bl_space_type = 'USER_PREFERENCES'

    def draw(self, context):
        layout = self.layout
        layout.template_header(menus=False)

        userpref = context.user_preferences

        layout.operator_context = 'EXEC_AREA'
        layout.operator("wm.save_homefile", text="Save As Default")

        layout.operator_context = 'INVOKE_DEFAULT'

        if userpref.active_section == 'INPUT':
            layout.operator("wm.keyconfig_export")
            layout.operator("wm.keyconfig_import")
        elif userpref.active_section == 'ADDONS':
            layout.operator("wm.addon_install")
            layout.menu("USERPREF_MT_addons_dev_guides")
        elif userpref.active_section == 'THEMES':
            layout.operator("ui.reset_default_theme")


class USERPREF_PT_tabs(bpy.types.Panel):
    bl_label = ""
    bl_space_type = 'USER_PREFERENCES'
    bl_region_type = 'WINDOW'
    bl_options = {'HIDE_HEADER'}

    def draw(self, context):
        layout = self.layout

        userpref = context.user_preferences

        layout.prop(userpref, "active_section", expand=True)


class USERPREF_MT_interaction_presets(bpy.types.Menu):
    bl_label = "Presets"
    preset_subdir = "interaction"
    preset_operator = "script.execute_preset"
    draw = bpy.types.Menu.draw_preset


class USERPREF_MT_appconfigs(bpy.types.Menu):
    bl_label = "AppPresets"
    preset_subdir = "keyconfig"
    preset_operator = "wm.appconfig_activate"

    def draw(self, context):
        props = self.layout.operator("wm.appconfig_default", text="Blender (default)")

        # now draw the presets
        bpy.types.Menu.draw_preset(self, context)


class USERPREF_MT_splash(bpy.types.Menu):
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


class USERPREF_PT_interface(bpy.types.Panel):
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


class USERPREF_PT_edit(bpy.types.Panel):
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
        #col.prop(edit, "use_grease_pencil_simplify_stroke", text="Simplify Stroke")
        col.prop(edit, "grease_pencil_eraser_radius", text="Eraser Radius")
        col.prop(edit, "use_grease_pencil_smooth_stroke", text="Smooth Stroke")
        col.separator()
        col.separator()
        col.separator()
        col.label(text="Playback:")
        col.prop(edit, "use_negative_frames")

        row.separator()
        row.separator()

        col = row.column()
        col.label(text="Keyframing:")
        col.prop(edit, "use_visual_keying")
        col.prop(edit, "use_keyframe_insert_needed", text="Only Insert Needed")

        col.separator()

        col.prop(edit, "use_auto_keying", text="Auto Keyframing:")

        sub = col.column()

        # sub.active = edit.use_keyframe_insert_auto # incorrect, timeline can enable
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


class USERPREF_PT_system(bpy.types.Panel):
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
        col.prop(system, "author", text="Author")
        col.prop(system, "use_scripts_auto_execute")
        col.prop(system, "use_tabs_as_spaces")

        col.separator()
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
        col.separator()

        col.label(text="Screencast:")
        col.prop(system, "screencast_fps")
        col.prop(system, "screencast_wait_time")
        col.separator()
        col.separator()
        col.separator()

        #column = split.column()
        #colsplit = column.split(percentage=0.85)

        # No translation in 2.5 yet
        #col.prop(system, "language")
        #col.label(text="Translate:")
        #col.prop(system, "use_translate_tooltips", text="Tooltips")
        #col.prop(system, "use_translate_buttons", text="Labels")
        #col.prop(system, "use_translate_toolbox", text="Toolbox")

        #col.separator()

        #col.prop(system, "use_textured_fonts")

        # 2. Column
        column = split.column()
        colsplit = column.split(percentage=0.85)

        col = colsplit.column()
        col.label(text="OpenGL:")
        col.prop(system, "gl_clip_alpha", slider=True)
        col.prop(system, "use_mipmaps")
        col.label(text="Anisotropic Filtering")
        col.prop(system, "anisotropic_filter", text="")
        col.prop(system, "use_vertex_buffer_objects")
        #Anti-aliasing is disabled as it breaks broder/lasso select
        #col.prop(system, "use_antialiasing")
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
        column.separator()
        column.separator()

        column.label(text="Color Picker Type:")
        column.row().prop(system, "color_picker_type", text="")

        column.separator()
        column.separator()
        column.separator()

        column.prop(system, "use_weight_color_range", text="Custom Weight Paint Range")
        sub = column.column()
        sub.active = system.use_weight_color_range
        sub.template_color_ramp(system, "weight_color_range", expand=True)


class USERPREF_PT_theme(bpy.types.Panel):
    bl_space_type = 'USER_PREFERENCES'
    bl_label = "Themes"
    bl_region_type = 'WINDOW'
    bl_options = {'HIDE_HEADER'}

    @staticmethod
    def _theme_generic(split, themedata):

        row = split.row()

        subsplit = row.split(percentage=0.95)

        padding1 = subsplit.split(percentage=0.15)
        padding1.column()

        subsplit = row.split(percentage=0.85)

        padding2 = subsplit.split(percentage=0.15)
        padding2.column()

        colsub_pair = padding1.column(), padding2.column()

        props_type = {}

        for i, prop in enumerate(themedata.rna_type.properties):
            attr = prop.identifier
            if attr == "rna_type":
                continue

            props_type.setdefault((prop.type, prop.subtype), []).append(prop.identifier)

        for props_type, props_ls in sorted(props_type.items()):
            for i, attr in enumerate(props_ls):
                colsub_pair[i % 2].row().prop(themedata, attr)

    @classmethod
    def poll(cls, context):
        userpref = context.user_preferences
        return (userpref.active_section == 'THEMES')

    def draw(self, context):
        layout = self.layout

        theme = context.user_preferences.themes[0]

        split_themes = layout.split(percentage=0.2)
        split_themes.prop(theme, "theme_area", expand=True)

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

            ui = theme.user_interface
            col.separator()
            col.separator()

            split = col.split(percentage=0.93)
            split.prop(ui, "icon_file")

            layout.separator()
            layout.separator()
        elif theme.theme_area == 'BONE_COLOR_SETS':
            col = split.column()

            for i, ui in enumerate(theme.bone_color_sets):
                col.label(text="Color Set %d:" % (i + 1))  # i starts from 0

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


class USERPREF_PT_file(bpy.types.Panel):
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

        split = layout.split(percentage=0.7)

        col = split.column()
        col.label(text="File Paths:")

        colsplit = col.split(percentage=0.95)
        col1 = colsplit.split(percentage=0.3)

        sub = col1.column()
        sub.label(text="Fonts:")
        sub.label(text="Textures:")
        sub.label(text="Texture Plugins:")
        sub.label(text="Sequence Plugins:")
        sub.label(text="Render Output:")
        sub.label(text="Scripts:")
        sub.label(text="Sounds:")
        sub.label(text="Temp:")
        sub.label(text="Image Editor:")
        sub.label(text="Animation Player:")

        sub = col1.column()
        sub.prop(paths, "font_directory", text="")
        sub.prop(paths, "texture_directory", text="")
        sub.prop(paths, "texture_plugin_directory", text="")
        sub.prop(paths, "sequence_plugin_directory", text="")
        sub.prop(paths, "render_output_directory", text="")
        sub.prop(paths, "script_directory", text="")
        sub.prop(paths, "sound_directory", text="")
        sub.prop(paths, "temporary_directory", text="")
        sub.prop(paths, "image_editor", text="")
        subsplit = sub.split(percentage=0.3)
        subsplit.prop(paths, "animation_player_preset", text="")
        subsplit.prop(paths, "animation_player", text="")

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

from bl_ui.space_userpref_keymap import InputKeyMapPanel


class USERPREF_PT_input(bpy.types.Panel, InputKeyMapPanel):
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
        ''' not implemented yet
        sub = col.column()
        sub.label(text="NDOF Device:")
        sub.prop(inputs, "ndof_pan_speed", text="Pan Speed")
        sub.prop(inputs, "ndof_rotate_speed", text="Orbit Speed")
        '''

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


class USERPREF_MT_addons_dev_guides(bpy.types.Menu):
    bl_label = "Develoment Guides"

    # menu to open webpages with addons development guides
    def draw(self, context):
        layout = self.layout
        layout.operator('wm.url_open', text='API Concepts', icon='URL').url = 'http://wiki.blender.org/index.php/Dev:2.5/Py/API/Intro'
        layout.operator('wm.url_open', text='Addon Guidelines', icon='URL').url = 'http://wiki.blender.org/index.php/Dev:2.5/Py/Scripts/Guidelines/Addons'
        layout.operator('wm.url_open', text='How to share your addon', icon='URL').url = 'http://wiki.blender.org/index.php/Dev:Py/Sharing'


class USERPREF_PT_addons(bpy.types.Panel):
    bl_space_type = 'USER_PREFERENCES'
    bl_label = "Addons"
    bl_region_type = 'WINDOW'
    bl_options = {'HIDE_HEADER'}

    _addons_fake_modules = {}

    @classmethod
    def poll(cls, context):
        userpref = context.user_preferences
        return (userpref.active_section == 'ADDONS')

    @staticmethod
    def module_get(mod_name):
        return USERPREF_PT_addons._addons_fake_modules[mod_name]

    def draw(self, context):
        layout = self.layout

        userpref = context.user_preferences
        used_ext = {ext.module for ext in userpref.addons}

        # collect the categories that can be filtered on
        addons = [(mod, addon_utils.module_bl_info(mod)) for mod in addon_utils.modules(USERPREF_PT_addons._addons_fake_modules)]

        split = layout.split(percentage=0.2)
        col = split.column()
        col.prop(context.window_manager, "addon_search", text="", icon='VIEWZOOM')
        col.label(text="Categories")
        col.prop(context.window_manager, "addon_filter", expand=True)

        col.label(text="Supported Level")
        col.prop(context.window_manager, "addon_support", expand=True)

        col = split.column()

        filter = context.window_manager.addon_filter
        search = context.window_manager.addon_search.lower()
        support = context.window_manager.addon_support

        for mod, info in addons:
            module_name = mod.__name__

            is_enabled = module_name in used_ext

            if info["support"] not in support:
                continue

            # check if add-on should be visible with current filters
            if (filter == "All") or \
                    (filter == info["category"]) or \
                    (filter == "Enabled" and is_enabled) or \
                    (filter == "Disabled" and not is_enabled):

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
                if info["support"] == 'OFFICIAL':
                    rowsub.label(icon='FILE_BLEND')
                elif info["support"] == 'COMMUNITY':
                    rowsub.label(icon='POSE_DATA')
                else:
                    rowsub.label(icon='QUESTION')

                if is_enabled:
                    row.operator("wm.addon_disable", icon='CHECKBOX_HLT', text="", emboss=False).module = module_name
                else:
                    row.operator("wm.addon_enable", icon='CHECKBOX_DEHLT', text="", emboss=False).module = module_name

                # Expanded UI (only if additional infos are available)
                if info["show_expanded"]:
                    if info["description"]:
                        split = colsub.row().split(percentage=0.15)
                        split.label(text='Description:')
                        split.label(text=info["description"])
                    if info["location"]:
                        split = colsub.row().split(percentage=0.15)
                        split.label(text='Location:')
                        split.label(text=info["location"])
                    if info["author"]:
                        split = colsub.row().split(percentage=0.15)
                        split.label(text='Author:')
                        split.label(text=info["author"])
                    if info["version"]:
                        split = colsub.row().split(percentage=0.15)
                        split.label(text='Version:')
                        split.label(text='.'.join(str(x) for x in info["version"]))
                    if info["warning"]:
                        split = colsub.row().split(percentage=0.15)
                        split.label(text="Warning:")
                        split.label(text='  ' + info["warning"], icon='ERROR')
                    if info["wiki_url"] or info["tracker_url"]:
                        split = colsub.row().split(percentage=0.15)
                        split.label(text="Internet:")
                        if info["wiki_url"]:
                            split.operator("wm.url_open", text="Link to the Wiki", icon='HELP').url = info["wiki_url"]
                        if info["tracker_url"]:
                            split.operator("wm.url_open", text="Report a Bug", icon='URL').url = info["tracker_url"]

                        if info["wiki_url"] and info["tracker_url"]:
                            split.separator()
                        else:
                            split.separator()
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


class WM_OT_addon_enable(bpy.types.Operator):
    "Enable an addon"
    bl_idname = "wm.addon_enable"
    bl_label = "Enable Add-On"

    module = StringProperty(name="Module", description="Module name of the addon to enable")

    def execute(self, context):
        mod = addon_utils.enable(self.module)

        if mod:
            # check if add-on is written for current blender version, or raise a warning
            info = addon_utils.module_bl_info(mod)

            if info.get("blender", (0, 0, 0)) > bpy.app.version:
                self.report("WARNING','This script was written for a newer version of Blender and might not function (correctly).\nThe script is enabled though.")
            return {'FINISHED'}
        else:
            return {'CANCELLED'}


class WM_OT_addon_disable(bpy.types.Operator):
    "Disable an addon"
    bl_idname = "wm.addon_disable"
    bl_label = "Disable Add-On"

    module = StringProperty(name="Module", description="Module name of the addon to disable")

    def execute(self, context):
        addon_utils.disable(self.module)
        return {'FINISHED'}


class WM_OT_addon_install(bpy.types.Operator):
    "Install an addon"
    bl_idname = "wm.addon_install"
    bl_label = "Install Add-On..."

    overwrite = BoolProperty(name="Overwrite", description="Remove existing addons with the same ID", default=True)
    target = EnumProperty(
            name="Target Path",
            items=(('DEFAULT', "Default", ""),
                   ('PREFS', "User Prefs", "")))

    filepath = StringProperty(name="File Path", description="File path to write file to")
    filter_folder = BoolProperty(name="Filter folders", description="", default=True, options={'HIDDEN'})
    filter_python = BoolProperty(name="Filter python", description="", default=True, options={'HIDDEN'})
    filter_glob = StringProperty(default="*.py;*.zip", options={'HIDDEN'})

    @staticmethod
    def _module_remove(path_addons, module):
        module = os.path.splitext(module)[0]
        for f in os.listdir(path_addons):
            f_base = os.path.splitext(f)[0]
            if f_base == module:
                f_full = os.path.join(path_addons, f)

                if os.path.isdir(f_full):
                    os.rmdir(f_full)
                else:
                    os.remove(f_full)

    def execute(self, context):
        import traceback
        import zipfile
        import shutil

        pyfile = self.filepath

        if self.target == 'DEFAULT':
            # dont use bpy.utils.script_paths("addons") because we may not be able to write to it.
            path_addons = bpy.utils.user_resource('SCRIPTS', "addons", create=True)
        else:
            path_addons = bpy.context.user_preferences.filepaths.script_directory
            if path_addons:
                path_addons = os.path.join(path_addons, "addons")

        if not path_addons:
            self.report({'ERROR'}, "Failed to get addons path")
            return {'CANCELLED'}

        # create dir is if missing.
        if not os.path.exists(path_addons):
            os.makedirs(path_addons)

        # Check if we are installing from a target path,
        # doing so causes 2+ addons of same name or when the same from/to
        # location is used, removal of the file!
        addon_path = ""
        pyfile_dir = os.path.dirname(pyfile)
        for addon_path in addon_utils.paths():
            if os.path.samefile(pyfile_dir, addon_path):
                self.report({'ERROR'}, "Source file is in the addon search path: %r" % addon_path)
                return {'CANCELLED'}
        del addon_path
        del pyfile_dir
        # done checking for exceptional case

        addon_files_old = set(os.listdir(path_addons))
        addons_old = {mod.__name__ for mod in addon_utils.modules(USERPREF_PT_addons._addons_fake_modules)}

        #check to see if the file is in compressed format (.zip)
        if zipfile.is_zipfile(pyfile):
            try:
                file_to_extract = zipfile.ZipFile(pyfile, 'r')
            except:
                traceback.print_exc()
                return {'CANCELLED'}

            if self.overwrite:
                for f in file_to_extract.namelist():
                    __class__._module_remove(path_addons, f)
            else:
                for f in file_to_extract.namelist():
                    path_dest = os.path.join(path_addons, os.path.basename(f))
                    if os.path.exists(path_dest):
                        self.report({'WARNING'}, "File already installed to %r\n" % path_dest)
                        return {'CANCELLED'}

            try:  # extract the file to "addons"
                file_to_extract.extractall(path_addons)

                # zip files can create this dir with metadata, don't need it
                macosx_dir = os.path.join(path_addons, '__MACOSX')
                if os.path.isdir(macosx_dir):
                    shutil.rmtree(macosx_dir)

            except:
                traceback.print_exc()
                return {'CANCELLED'}

        else:
            path_dest = os.path.join(path_addons, os.path.basename(pyfile))

            if self.overwrite:
                __class__._module_remove(path_addons, os.path.basename(pyfile))
            elif os.path.exists(path_dest):
                self.report({'WARNING'}, "File already installed to %r\n" % path_dest)
                return {'CANCELLED'}

            #if not compressed file just copy into the addon path
            try:
                shutil.copyfile(pyfile, path_dest)

            except:
                traceback.print_exc()
                return {'CANCELLED'}

        addons_new = {mod.__name__ for mod in addon_utils.modules(USERPREF_PT_addons._addons_fake_modules)} - addons_old
        addons_new.discard("modules")

        # disable any addons we may have enabled previously and removed.
        # this is unlikely but do just incase. bug [#23978]
        for new_addon in addons_new:
            addon_utils.disable(new_addon)

        # possible the zip contains multiple addons, we could disallow this
        # but for now just use the first
        for mod in addon_utils.modules(USERPREF_PT_addons._addons_fake_modules):
            if mod.__name__ in addons_new:
                info = addon_utils.module_bl_info(mod)

                # show the newly installed addon.
                context.window_manager.addon_filter = 'All'
                context.window_manager.addon_search = info["name"]
                break

        # incase a new module path was created to install this addon.
        bpy.utils.refresh_script_paths()

        # TODO, should not be a warning.
        # self.report({'WARNING'}, "File installed to '%s'\n" % path_dest)
        return {'FINISHED'}

    def invoke(self, context, event):
        wm = context.window_manager
        wm.fileselect_add(self)
        return {'RUNNING_MODAL'}


class WM_OT_addon_expand(bpy.types.Operator):
    "Display more information on this add-on"
    bl_idname = "wm.addon_expand"
    bl_label = ""

    module = StringProperty(name="Module", description="Module name of the addon to expand")

    def execute(self, context):
        module_name = self.module

        # unlikely to fail, module should have already been imported
        try:
            # mod = __import__(module_name)
            mod = USERPREF_PT_addons.module_get(module_name)
        except:
            import traceback
            traceback.print_exc()
            return {'CANCELLED'}

        info = addon_utils.module_bl_info(mod)
        info["show_expanded"] = not info["show_expanded"]
        return {'FINISHED'}

if __name__ == "__main__":  # only for live edit.
    bpy.utils.register_module(__name__)
