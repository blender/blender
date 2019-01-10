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
from bpy.types import (
    Header,
    Menu,
    Panel,
)
from bpy.app.translations import pgettext_iface as iface_
from bpy.app.translations import contexts as i18n_contexts


class USERPREF_HT_header(Header):
    bl_space_type = 'PREFERENCES'

    def draw(self, context):
        layout = self.layout
        layout.operator_context = 'EXEC_AREA'

        layout.template_header()


class USERPREF_PT_navigation(Panel):
    bl_label = "Preferences Navigation"
    bl_space_type = 'PREFERENCES'
    bl_region_type = 'NAVIGATION_BAR'
    bl_options = {'HIDE_HEADER'}

    def draw(self, context):
        layout = self.layout

        prefs = context.preferences

        col = layout.column()

        col.scale_x = 1.3
        col.scale_y = 1.3
        col.prop(prefs, "active_section", expand=True)


class USERPREF_PT_save_preferences(Panel):
    bl_label = "Save Preferences"
    bl_space_type = 'PREFERENCES'
    bl_region_type = 'EXECUTE'
    bl_options = {'HIDE_HEADER'}

    def draw(self, context):
        layout = self.layout
        layout.operator_context = 'EXEC_AREA'

        prefs = context.preferences

        layout.scale_x = 1.3
        layout.scale_y = 1.3

        layout.operator("wm.save_userpref")


class PreferencePanel(Panel):
    """
    Base class for panels to center align contents with some horizontal margin.
    Deriving classes need to implement a ``draw_props(context, layout)`` function.
    """

    bl_space_type = 'PREFERENCES'
    bl_region_type = 'WINDOW'

    def draw(self, context):
        layout = self.layout
        width = context.region.width
        pixel_size = context.preferences.system.pixel_size

        layout.use_property_split = True
        layout.use_property_decorate = False  # No animation.

        row = layout.row()
        if width > (350 * pixel_size):  # No horizontal margin if region is rather small.
            row.label()  # Needed so col below is centered.

        col = row.column()
        col.ui_units_x = 50

        # draw_props implemented by deriving classes.
        self.draw_props(context, col)

        if width > (350 * pixel_size):  # No horizontal margin if region is rather small.
            row.label()  # Needed so col above is centered.


class USERPREF_PT_interface_display(PreferencePanel):
    bl_label = "Display"

    @classmethod
    def poll(cls, context):
        prefs = context.preferences
        return (prefs.active_section == 'INTERFACE')

    def draw_props(self, context, layout):
        prefs = context.preferences
        view = prefs.view

        flow = layout.grid_flow(row_major=False, columns=0, even_columns=True, even_rows=False, align=False)

        flow.prop(view, "ui_scale", text="Resolution Scale")
        flow.prop(view, "ui_line_width", text="Line Width")


class USERPREF_PT_interface_display_info(PreferencePanel):
    bl_label = "Information"
    bl_parent_id = "USERPREF_PT_interface_display"

    def draw_props(self, context, layout):
        prefs = context.preferences
        view = prefs.view

        flow = layout.grid_flow(row_major=False, columns=0, even_columns=True, even_rows=False, align=False)

        flow.prop(view, "show_tooltips")
        flow.prop(view, "show_object_info", text="Object Info")
        flow.prop(view, "show_large_cursors")
        flow.prop(view, "show_view_name", text="View Name")
        flow.prop(view, "show_playback_fps", text="Playback FPS")


class USERPREF_PT_interface_text(PreferencePanel):
    bl_label = "Text"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        prefs = context.preferences
        return (prefs.active_section == 'INTERFACE')

    def draw_props(self, context, layout):
        prefs = context.preferences
        view = prefs.view

        flow = layout.grid_flow(row_major=False, columns=0, even_columns=True, even_rows=False, align=False)

        flow.prop(view, "use_text_antialiasing", text="Anti-aliasing")
        sub = flow.column()
        sub.active = view.use_text_antialiasing
        sub.prop(view, "text_hinting", text="Hinting")

        flow.prop(view, "font_path_ui")
        flow.prop(view, "font_path_ui_mono")


class USERPREF_PT_interface_text_translate(PreferencePanel):
    bl_label = "Translate UI"
    bl_parent_id = "USERPREF_PT_interface_text"

    @classmethod
    def poll(cls, context):
        prefs = context.preferences
        if bpy.app.build_options.international:
            return (prefs.active_section == 'INTERFACE')

    def draw_header(self, context):
        prefs = context.preferences
        view = prefs.view

        self.layout.prop(view, "use_international_fonts", text="")

    def draw_props(self, context, layout):
        prefs = context.preferences
        view = prefs.view

        layout.active = view.use_international_fonts

        flow = layout.grid_flow(row_major=False, columns=0, even_columns=True, even_rows=False, align=False)

        flow.prop(view, "language")

        flow.prop(view, "use_translate_tooltips", text="Translate Tooltips")
        flow.prop(view, "use_translate_interface", text="Translate Interface")
        flow.prop(view, "use_translate_new_dataname", text="Translate New Data")


class USERPREF_PT_interface_develop(PreferencePanel):
    bl_label = "Develop"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        prefs = context.preferences
        return (prefs.active_section == 'INTERFACE')

    def draw_props(self, context, layout):
        prefs = context.preferences
        view = prefs.view

        flow = layout.grid_flow(row_major=False, columns=0, even_columns=True, even_rows=False, align=False)

        flow.prop(view, "show_tooltips_python")
        flow.prop(view, "show_developer_ui")


class USERPREF_PT_interface_viewports(PreferencePanel):
    bl_label = "Viewports"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        prefs = context.preferences
        return (prefs.active_section == 'INTERFACE')

    def draw(self, context):
        pass


class USERPREF_PT_interface_viewports_3d(PreferencePanel):
    bl_label = "3D Viewports"
    bl_parent_id = "USERPREF_PT_interface_viewports"

    def draw_props(self, context, layout):
        prefs = context.preferences
        view = prefs.view

        flow = layout.grid_flow(row_major=False, columns=0, even_columns=True, even_rows=False, align=False)

        col = flow.column()

        col.prop(view, "smooth_view")
        col.prop(view, "rotation_angle")
        col.separator()

        col = flow.column()

        col.prop(view, "object_origin_size")
        col.prop(view, "gizmo_size", text="Gizmo Size")
        col.separator()

        col = flow.column()

        col.prop(view, "mini_axis_type", text="3D Viewport Axis")

        sub = col.column()
        sub.active = view.mini_axis_type == 'MINIMAL'
        sub.prop(view, "mini_axis_size", text="Size")
        sub.prop(view, "mini_axis_brightness", text="Brightness")


class USERPREF_PT_interface_viewports_3d_weight_paint(PreferencePanel):
    bl_label = "Custom Weight Paint Range"
    bl_options = {'DEFAULT_CLOSED'}
    bl_parent_id = "USERPREF_PT_interface_viewports_3d"

    def draw_header(self, context):
        prefs = context.preferences
        view = prefs.view

        self.layout.prop(view, "use_weight_color_range", text="")

    def draw_props(self, context, layout):
        prefs = context.preferences
        view = prefs.view

        layout.active = view.use_weight_color_range
        layout.template_color_ramp(view, "weight_color_range", expand=True)


class USERPREF_PT_interface_viewports_2d(PreferencePanel):
    bl_label = "2D Viewports"
    bl_parent_id = "USERPREF_PT_interface_viewports"

    def draw_props(self, context, layout):
        prefs = context.preferences
        view = prefs.view

        flow = layout.grid_flow(row_major=False, columns=0, even_columns=True, even_rows=False, align=False)

        flow.prop(view, "view2d_grid_spacing_min", text="Minimum Grid Spacing")
        flow.prop(view, "timecode_style")
        flow.prop(view, "view_frame_type")
        if view.view_frame_type == 'SECONDS':
            flow.prop(view, "view_frame_seconds")
        elif view.view_frame_type == 'KEYFRAMES':
            flow.prop(view, "view_frame_keyframes")


class USERPREF_PT_interface_menus(PreferencePanel):
    bl_label = "Menus"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        prefs = context.preferences
        return (prefs.active_section == 'INTERFACE')

    def draw_props(self, context, layout):
        prefs = context.preferences
        view = prefs.view
        system = prefs.system

        flow = layout.grid_flow(row_major=False, columns=0, even_columns=True, even_rows=False, align=False)

        flow.prop(view, "color_picker_type")
        flow.row().prop(view, "header_align_default", expand=True)
        flow.prop(system, "use_region_overlap")
        flow.prop(view, "show_splash")
        flow.prop(view, "use_quit_dialog")


class USERPREF_PT_interface_menus_mouse_over(PreferencePanel):
    bl_label = "Open on Mouse Over"
    bl_parent_id = "USERPREF_PT_interface_menus"

    def draw_header(self, context):
        prefs = context.preferences
        view = prefs.view

        self.layout.prop(view, "use_mouse_over_open", text="")

    def draw_props(self, context, layout):
        prefs = context.preferences
        view = prefs.view

        layout.active = view.use_mouse_over_open

        flow = layout.grid_flow(row_major=False, columns=0, even_columns=True, even_rows=False, align=False)

        flow.prop(view, "open_toplevel_delay", text="Top Level")
        flow.prop(view, "open_sublevel_delay", text="Sub Level")


class USERPREF_PT_interface_menus_pie(PreferencePanel):
    bl_label = "Pie Menus"
    bl_parent_id = "USERPREF_PT_interface_menus"
    bl_options = {'DEFAULT_CLOSED'}

    def draw_props(self, context, layout):
        prefs = context.preferences
        view = prefs.view

        flow = layout.grid_flow(row_major=False, columns=0, even_columns=True, even_rows=False, align=False)

        flow.prop(view, "pie_animation_timeout")
        flow.prop(view, "pie_tap_timeout")
        flow.prop(view, "pie_initial_timeout")
        flow.prop(view, "pie_menu_radius")
        flow.prop(view, "pie_menu_threshold")
        flow.prop(view, "pie_menu_confirm")


class USERPREF_PT_interface_templates(PreferencePanel):
    bl_label = "Application Templates"
    bl_options = {'DEFAULT_CLOSED'}
    bl_parent_id = "USERPREF_PT_interface_develop"

    @classmethod
    def poll(cls, context):
        prefs = context.preferences
        return (prefs.active_section == 'INTERFACE')

    def draw_props(self, context, layout):
        prefs = context.preferences
        view = prefs.view

        layout.label(text="Options intended for developing application templates only")
        layout.prop(view, "show_layout_ui")


class USERPREF_PT_edit_objects(PreferencePanel):
    bl_label = "Objects"

    @classmethod
    def poll(cls, context):
        prefs = context.preferences
        return (prefs.active_section == 'EDITING')

    def draw_props(self, context, layout):
        prefs = context.preferences
        edit = prefs.edit

        flow = layout.grid_flow(row_major=False, columns=0, even_columns=True, even_rows=False, align=False)

        flow.prop(edit, "material_link", text="Link Materials to")
        flow.prop(edit, "object_align", text="Align New Objects to")
        flow.prop(edit, "use_enter_edit_mode", text="Enter Edit Mode for New Objects")


class USERPREF_PT_edit_animation(PreferencePanel):
    bl_label = "Animation"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        prefs = context.preferences
        return (prefs.active_section == 'EDITING')

    def draw_props(self, context, layout):
        prefs = context.preferences
        edit = prefs.edit

        flow = layout.grid_flow(row_major=False, columns=0, even_columns=True, even_rows=False, align=False)

        flow.prop(edit, "use_negative_frames")
        flow.prop(edit, "use_visual_keying")
        flow.prop(edit, "use_keyframe_insert_needed", text="Only Insert Needed")


class USERPREF_PT_edit_animation_autokey(PreferencePanel):
    bl_label = "Auto-Keyframing"
    bl_parent_id = "USERPREF_PT_edit_animation"

    def draw_header(self, context):
        prefs = context.preferences
        edit = prefs.edit

        self.layout.prop(edit, "use_auto_keying", text="")

    def draw_props(self, context, layout):
        prefs = context.preferences
        edit = prefs.edit

        flow = layout.grid_flow(row_major=False, columns=0, even_columns=True, even_rows=False, align=False)

        flow.prop(edit, "use_auto_keying_warning")
        flow.prop(edit, "use_keyframe_insert_available", text="Only Insert Available")


class USERPREF_PT_edit_animation_fcurves(PreferencePanel):
    bl_label = "F-Curves"
    bl_parent_id = "USERPREF_PT_edit_animation"

    def draw_props(self, context, layout):
        prefs = context.preferences
        edit = prefs.edit

        flow = layout.grid_flow(row_major=False, columns=0, even_columns=True, even_rows=False, align=False)

        flow.prop(edit, "fcurve_unselected_alpha", text="F-Curve Visibility")
        flow.prop(edit, "keyframe_new_interpolation_type", text="Default Interpolation")
        flow.prop(edit, "keyframe_new_handle_type", text="Default Handles")
        flow.prop(edit, "use_insertkey_xyz_to_rgb", text="XYZ to RGB")


class USERPREF_PT_edit_duplicate_data(PreferencePanel):
    bl_label = "Duplicate Data"
    bl_options = {'DEFAULT_CLOSED'}
    bl_parent_id = "USERPREF_PT_edit_objects"

    @classmethod
    def poll(cls, context):
        prefs = context.preferences
        return (prefs.active_section == 'EDITING')

    def draw_props(self, context, layout):
        prefs = context.preferences
        edit = prefs.edit

        flow = layout.grid_flow(row_major=False, columns=0, even_columns=True, even_rows=False, align=True)

        col = flow.column()
        col.prop(edit, "use_duplicate_action", text="Action")
        col.prop(edit, "use_duplicate_armature", text="Armature")
        col.prop(edit, "use_duplicate_curve", text="Curve")
        # col.prop(edit, "use_duplicate_fcurve", text="F-Curve")
        col.prop(edit, "use_duplicate_light", text="Light")
        col = flow.column()
        col.prop(edit, "use_duplicate_material", text="Material")
        col.prop(edit, "use_duplicate_mesh", text="Mesh")
        col.prop(edit, "use_duplicate_metaball", text="Metaball")
        col.prop(edit, "use_duplicate_particle", text="Particle")
        col = flow.column()
        col.prop(edit, "use_duplicate_surface", text="Surface")
        col.prop(edit, "use_duplicate_text", text="Text")
        col.prop(edit, "use_duplicate_texture", text="Texture")


class USERPREF_PT_edit_gpencil(PreferencePanel):
    bl_label = "Grease Pencil"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        prefs = context.preferences
        return (prefs.active_section == 'EDITING')

    def draw_props(self, context, layout):
        prefs = context.preferences
        edit = prefs.edit

        flow = layout.grid_flow(row_major=False, columns=0, even_columns=True, even_rows=False, align=False)

        flow.prop(edit, "grease_pencil_manhattan_distance", text="Manhattan Distance")
        flow.prop(edit, "grease_pencil_euclidean_distance", text="Euclidean Distance")


class USERPREF_PT_edit_annotations(PreferencePanel):
    bl_label = "Annotations"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        prefs = context.preferences
        return (prefs.active_section == 'EDITING')

    def draw_props(self, context, layout):
        prefs = context.preferences
        edit = prefs.edit

        flow = layout.grid_flow(row_major=False, columns=0, even_columns=True, even_rows=False, align=False)

        flow.prop(edit, "grease_pencil_default_color", text="Default Color")
        flow.prop(edit, "grease_pencil_eraser_radius", text="Eraser Radius")
        flow.prop(edit, "use_grease_pencil_simplify_stroke", text="Simplify Stroke")


class USERPREF_PT_edit_misc(PreferencePanel):
    bl_label = "Miscellaneous"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        prefs = context.preferences
        return (prefs.active_section == 'EDITING')

    def draw_props(self, context, layout):
        prefs = context.preferences
        edit = prefs.edit

        flow = layout.grid_flow(row_major=False, columns=0, even_columns=True, even_rows=False, align=False)

        flow.prop(edit, "sculpt_paint_overlay_color", text="Sculpt Overlay Color")
        flow.prop(edit, "node_margin", text="Node Auto-offset Margin")


class USERPREF_PT_edit_cursor(PreferencePanel):
    bl_label = "3D Cursor"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        prefs = context.preferences
        return (prefs.active_section == 'EDITING')

    def draw_props(self, context, layout):
        prefs = context.preferences
        edit = prefs.edit

        flow = layout.grid_flow(row_major=False, columns=0, even_columns=True, even_rows=False, align=False)

        flow.prop(edit, "use_mouse_depth_cursor")
        flow.prop(edit, "use_cursor_lock_adjust")


class USERPREF_PT_system_sound(PreferencePanel):
    bl_label = "Sound"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        prefs = context.preferences
        return (prefs.active_section == 'SYSTEM_GENERAL')

    def draw_props(self, context, layout):
        prefs = context.preferences
        system = prefs.system

        flow = layout.grid_flow(row_major=False, columns=0, even_columns=True, even_rows=False, align=False)

        flow.prop(system, "audio_device", expand=False)
        sub = flow.grid_flow(row_major=False, columns=0, even_columns=False, even_rows=False, align=False)
        sub.active = system.audio_device not in {'NONE', 'Null'}
        sub.prop(system, "audio_channels", text="Channels")
        sub.prop(system, "audio_mixing_buffer", text="Mixing Buffer")
        sub.prop(system, "audio_sample_rate", text="Sample Rate")
        sub.prop(system, "audio_sample_format", text="Sample Format")


class USERPREF_PT_system_compute_device(PreferencePanel):
    bl_label = "Cycles Compute Device"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        prefs = context.preferences
        return (prefs.active_section == 'SYSTEM_GENERAL')

    def draw_props(self, context, layout):
        prefs = context.preferences
        system = prefs.system

        col = layout.column()

        if bpy.app.build_options.cycles:
            addon = prefs.addons.get("cycles")
            if addon is not None:
                addon.preferences.draw_impl(col, context)
            del addon

        # NOTE: Disabled for until GPU side of OpenSubdiv is brought back.
        # if hasattr(system, "opensubdiv_compute_type"):
        #     col.label(text="OpenSubdiv compute:")
        #     col.row().prop(system, "opensubdiv_compute_type", text="")


class USERPREF_PT_system_opengl(PreferencePanel):
    bl_label = "OpenGL"

    @classmethod
    def poll(cls, context):
        prefs = context.preferences
        return (prefs.active_section == 'SYSTEM_GENERAL')

    def draw_props(self, context, layout):
        import sys
        prefs = context.preferences
        system = prefs.system

        flow = layout.grid_flow(row_major=False, columns=0, even_columns=True, even_rows=False, align=False)

        flow.prop(system, "gpu_viewport_quality")
        flow.prop(system, "gl_clip_alpha", slider=True)
        flow.prop(system, "multi_sample", text="Multisampling")
        flow.prop(system, "gpencil_multi_sample", text="Grease Pencil Multisampling")

        if sys.platform == "linux" and system.multi_sample != 'NONE':
            layout.label(text="Might fail for Mesh editing selection!")


class USERPREF_PT_system_opengl_textures(PreferencePanel):
    bl_label = "Textures"
    bl_parent_id = "USERPREF_PT_system_opengl"

    @classmethod
    def poll(cls, context):
        prefs = context.preferences
        return (prefs.active_section == 'SYSTEM_GENERAL')

    def draw_props(self, context, layout):
        prefs = context.preferences
        system = prefs.system

        flow = layout.grid_flow(row_major=False, columns=0, even_columns=True, even_rows=False, align=False)

        flow.prop(system, "gl_texture_limit", text="Limit Size")
        flow.prop(system, "anisotropic_filter")
        flow.prop(system, "texture_time_out", text="Time Out")
        flow.prop(system, "texture_collection_rate", text="Garbage Collection Rate")
        flow.prop(system, "image_draw_method", text="Image Display Method")

        flow.prop(system, "use_16bit_textures")
        flow.prop(system, "use_gpu_mipmap")


class USERPREF_PT_system_opengl_selection(PreferencePanel):
    bl_label = "Selection"
    bl_parent_id = "USERPREF_PT_system_opengl"

    def draw_props(self, context, layout):
        prefs = context.preferences
        system = prefs.system

        flow = layout.grid_flow(row_major=False, columns=0, even_columns=True, even_rows=False, align=False)

        flow.prop(system, "select_method", text="Selection Method")
        flow.prop(system, "use_select_pick_depth")


class USERPREF_PT_system_memory(PreferencePanel):
    bl_label = "Memory/Limits"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        prefs = context.preferences
        return (prefs.active_section == 'SYSTEM_GENERAL')

    def draw_props(self, context, layout):
        prefs = context.preferences
        system = prefs.system
        edit = prefs.edit

        flow = layout.grid_flow(row_major=False, columns=0, even_columns=True, even_rows=False, align=False)

        flow.prop(edit, "undo_steps", text="Undo Steps")
        flow.prop(edit, "undo_memory_limit", text="Undo Memory Limit")
        flow.prop(edit, "use_global_undo")

        layout.separator()

        flow = layout.grid_flow(row_major=False, columns=0, even_columns=True, even_rows=False, align=False)

        flow.prop(system, "memory_cache_limit", text="Sequencer Cache Limit")
        flow.prop(system, "scrollback", text="Console Scrollback Lines")


class USERPREF_MT_interface_theme_presets(Menu):
    bl_label = "Presets"
    preset_subdir = "interface_theme"
    preset_operator = "script.execute_preset"
    preset_type = 'XML'
    preset_xml_map = (
        ("preferences.themes[0]", "Theme"),
        ("preferences.ui_styles[0]", "ThemeStyle"),
    )
    draw = Menu.draw_preset

    def reset_cb(context):
        bpy.ops.ui.reset_default_theme()


class USERPREF_PT_theme(Panel):
    bl_space_type = 'PREFERENCES'
    bl_label = "Themes"
    bl_region_type = 'WINDOW'
    bl_options = {'HIDE_HEADER'}

    @classmethod
    def poll(cls, context):
        prefs = context.preferences
        return (prefs.active_section == 'THEMES')

    def draw(self, context):
        layout = self.layout

        theme = context.preferences.themes[0]

        row = layout.row()

        row.operator("wm.theme_install", text="Install...", icon='IMPORT')
        row.operator("ui.reset_default_theme", text="Reset", icon='LOOP_BACK')

        subrow = row.row(align=True)
        subrow.menu("USERPREF_MT_interface_theme_presets", text=USERPREF_MT_interface_theme_presets.bl_label)
        subrow.operator("wm.interface_theme_preset_add", text="", icon='ADD')
        subrow.operator("wm.interface_theme_preset_add", text="", icon='REMOVE').remove_active = True


class USERPREF_PT_theme_user_interface(PreferencePanel):
    bl_space_type = 'PREFERENCES'
    bl_region_type = 'WINDOW'
    bl_label = "User Interface"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        prefs = context.preferences
        return (prefs.active_section == 'THEMES')

    def draw_header(self, context):
        layout = self.layout

        layout.label(icon='WORKSPACE')

    def draw(self, context):
        pass


# Base class for dynamically defined widget color panels.
class PreferenceThemeWidgetColorPanel(Panel):
    bl_space_type = 'PREFERENCES'
    bl_region_type = 'WINDOW'
    bl_parent_id = "USERPREF_PT_theme_user_interface"

    @staticmethod
    def draw(self, context):
        theme = context.preferences.themes[0]
        ui = theme.user_interface
        widget_style = getattr(ui, self.wcol)
        layout = self.layout

        layout.use_property_split = True

        flow = layout.grid_flow(row_major=False, columns=0, even_columns=True, even_rows=False, align=False)

        col = flow.column()
        col.prop(widget_style, "outline")
        col.prop(widget_style, "item", slider=True)
        col.prop(widget_style, "inner", slider=True)
        col.prop(widget_style, "inner_sel", slider=True)

        col = flow.column()
        col.prop(widget_style, "text")
        col.prop(widget_style, "text_sel")
        col.prop(widget_style, "roundness")

        col = flow.column()
        col.prop(widget_style, "show_shaded")

        colsub = col.column()
        colsub.active = widget_style.show_shaded
        colsub.prop(widget_style, "shadetop")
        colsub.prop(widget_style, "shadedown")

    @classmethod
    def poll(cls, context):
        prefs = context.preferences
        return (prefs.active_section == 'THEMES')


class USERPREF_PT_theme_interface_state(PreferencePanel):
    bl_label = "State"
    bl_options = {'DEFAULT_CLOSED'}
    bl_parent_id = "USERPREF_PT_theme_user_interface"

    def draw_props(self, context, layout):
        theme = context.preferences.themes[0]
        ui = theme.user_interface
        ui_state = theme.user_interface.wcol_state

        flow = layout.grid_flow(row_major=False, columns=0, even_columns=True, even_rows=False, align=False)

        col = flow.column(align=True)
        col.prop(ui_state, "inner_anim")
        col.prop(ui_state, "inner_anim_sel")

        col = flow.column(align=True)
        col.prop(ui_state, "inner_driven")
        col.prop(ui_state, "inner_driven_sel")

        col = flow.column(align=True)
        col.prop(ui_state, "inner_key")
        col.prop(ui_state, "inner_key_sel")

        col = flow.column(align=True)
        col.prop(ui_state, "inner_overridden")
        col.prop(ui_state, "inner_overridden_sel")

        col = flow.column(align=True)
        col.prop(ui_state, "inner_changed")
        col.prop(ui_state, "inner_changed_sel")

        col = flow.column(align=True)
        col.prop(ui_state, "blend")


class USERPREF_PT_theme_interface_styles(PreferencePanel):
    bl_label = "Styles"
    bl_options = {'DEFAULT_CLOSED'}
    bl_parent_id = "USERPREF_PT_theme_user_interface"

    def draw_props(self, context, layout):
        theme = context.preferences.themes[0]
        ui = theme.user_interface

        flow = layout.grid_flow(row_major=False, columns=0, even_columns=True, even_rows=False, align=False)

        flow.prop(ui, "menu_shadow_fac")
        flow.prop(ui, "icon_alpha")
        flow.prop(ui, "icon_saturation")
        flow.prop(ui, "editor_outline")
        flow.prop(ui, "menu_shadow_width")
        flow.prop(ui, "widget_emboss")


class USERPREF_PT_theme_interface_gizmos(PreferencePanel):
    bl_label = "Axis & Gizmo Colors"
    bl_options = {'DEFAULT_CLOSED'}
    bl_parent_id = "USERPREF_PT_theme_user_interface"

    def draw_props(self, context, layout):
        theme = context.preferences.themes[0]
        ui = theme.user_interface

        flow = layout.grid_flow(row_major=False, columns=0, even_columns=True, even_rows=True, align=False)

        col = flow.column(align=True)
        col.prop(ui, "axis_x", text="Axis X")
        col.prop(ui, "axis_y", text="Y")
        col.prop(ui, "axis_z", text="Z")

        col = flow.column()
        col.prop(ui, "gizmo_primary")
        col.prop(ui, "gizmo_secondary")

        col = flow.column()
        col.prop(ui, "gizmo_a")
        col.prop(ui, "gizmo_b")


class USERPREF_PT_theme_interface_icons(PreferencePanel):
    bl_label = "Icon Colors"
    bl_options = {'DEFAULT_CLOSED'}
    bl_parent_id = "USERPREF_PT_theme_user_interface"

    def draw_props(self, context, layout):
        theme = context.preferences.themes[0]
        ui = theme.user_interface

        flow = layout.grid_flow(row_major=False, columns=0, even_columns=True, even_rows=False, align=False)

        flow.prop(ui, "icon_collection")
        flow.prop(ui, "icon_object")
        flow.prop(ui, "icon_object_data")
        flow.prop(ui, "icon_modifier")
        flow.prop(ui, "icon_shading")


class USERPREF_PT_theme_text_style(PreferencePanel):
    bl_label = "Text Style"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        prefs = context.preferences
        return (prefs.active_section == 'THEMES')

    @staticmethod
    def _ui_font_style(layout, font_style):
        layout.use_property_split = True
        flow = layout.grid_flow(row_major=False, columns=0, even_columns=True, even_rows=False, align=True)

        col = flow.column()
        col.row().prop(font_style, "font_kerning_style", expand=True)
        col.prop(font_style, "points")

        col = flow.column(align=True)
        col.prop(font_style, "shadow_offset_x", text="Shadow Offset X")
        col.prop(font_style, "shadow_offset_y", text="Y")

        col = flow.column()
        col.prop(font_style, "shadow")
        col.prop(font_style, "shadow_alpha")
        col.prop(font_style, "shadow_value")

    def draw_header(self, context):
        layout = self.layout

        layout.label(icon='FONTPREVIEW')

    def draw_props(self, context, layout):
        style = context.preferences.ui_styles[0]

        layout.label(text="Panel Title")
        self._ui_font_style(layout, style.panel_title)

        layout.separator()

        layout.label(text="Widget")
        self._ui_font_style(layout, style.widget)

        layout.separator()

        layout.label(text="Widget Label")
        self._ui_font_style(layout, style.widget_label)


class USERPREF_PT_theme_bone_color_sets(PreferencePanel):
    bl_label = "Bone Color Sets"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        prefs = context.preferences
        return (prefs.active_section == 'THEMES')

    def draw_header(self, context):
        layout = self.layout

        layout.label(icon='COLOR')

    def draw_props(self, context, layout):
        theme = context.preferences.themes[0]

        layout.use_property_split = True

        for i, ui in enumerate(theme.bone_color_sets, 1):
            layout.label(text=iface_(f"Color Set {i:d}"), translate=False)

            flow = layout.grid_flow(row_major=False, columns=0, even_columns=True, even_rows=False, align=False)

            flow.prop(ui, "normal")
            flow.prop(ui, "select")
            flow.prop(ui, "active")
            flow.prop(ui, "show_colored_constraints")


# Base class for dynamically defined theme-space panels.
class PreferenceThemeSpacePanel(Panel):
    bl_space_type = 'PREFERENCES'
    bl_region_type = 'WINDOW'

    # not essential, hard-coded UI delimiters for the theme layout
    ui_delimiters = {
        'VIEW_3D': {
            "text_grease_pencil",
            "text_keyframe",
            "speaker",
            "freestyle_face_mark",
            "split_normal",
            "bone_solid",
            "paint_curve_pivot",
        },
        'GRAPH_EDITOR': {
            "handle_vertex_select",
        },
        'IMAGE_EDITOR': {
            "paint_curve_pivot",
        },
        'NODE_EDITOR': {
            "layout_node",
        },
        'CLIP_EDITOR': {
            "handle_vertex_select",
        }
    }

    # TODO theme_area should be deprecated
    @staticmethod
    def _theme_generic(layout, themedata, theme_area):

        layout.use_property_split = True

        flow = layout.grid_flow(row_major=False, columns=0, even_columns=True, even_rows=False, align=False)

        props_type = {}

        for i, prop in enumerate(themedata.rna_type.properties):
            if prop.identifier == "rna_type":
                continue

            props_type.setdefault((prop.type, prop.subtype), []).append(prop)

        th_delimiters = PreferenceThemeSpacePanel.ui_delimiters.get(theme_area)
        for props_type, props_ls in sorted(props_type.items()):
            if props_type[0] == 'POINTER':
                continue

            if th_delimiters is None:
                # simple, no delimiters
                for i, prop in enumerate(props_ls):
                    flow.prop(themedata, prop.identifier)
            else:

                for prop in props_ls:
                    flow.prop(themedata, prop.identifier)

    @staticmethod
    def draw_header(self, context):
        if hasattr(self, "icon") and self.icon != 'NONE':
            layout = self.layout
            layout.label(icon=self.icon)

    @staticmethod
    def draw(self, context):
        layout = self.layout
        theme = context.preferences.themes[0]

        datapath_list = self.datapath.split(".")
        data = theme
        for datapath_item in datapath_list:
            data = getattr(data, datapath_item)
        PreferenceThemeSpacePanel._theme_generic(layout, data, self.theme_area)

    @classmethod
    def poll(cls, context):
        prefs = context.preferences
        return (prefs.active_section == 'THEMES')


class ThemeGenericClassGenerator():
    generated_classes = []

    @staticmethod
    def generate_panel_classes_for_wcols():
        wcols = [
            ("Regular", "wcol_regular"),
            ("Tool", "wcol_tool"),
            ("Toolbar Item", "wcol_toolbar_item"),
            ("Radio Buttons", "wcol_radio"),
            ("Text", "wcol_text"),
            ("Option", "wcol_option"),
            ("Toggle", "wcol_toggle"),
            ("Number Field", "wcol_num"),
            ("Value Slider", "wcol_numslider"),
            ("Box", "wcol_box"),
            ("Menu", "wcol_menu"),
            ("Pie Menu", "wcol_pie_menu"),
            ("Pulldown", "wcol_pulldown"),
            ("Menu Back", "wcol_menu_back"),
            ("Tooltip", "wcol_tooltip"),
            ("Menu Item", "wcol_menu_item"),
            ("Scroll Bar", "wcol_scroll"),
            ("Progress Bar", "wcol_progress"),
            ("List Item", "wcol_list_item"),
            ("Tab", "wcol_tab"),
        ]

        for (name, wcol) in wcols:
            panel_id = "USERPREF_PT_theme_interface_" + wcol
            paneltype = type(panel_id, (PreferenceThemeWidgetColorPanel,), {
                "bl_label": name,
                "bl_options": {'DEFAULT_CLOSED'},
                "draw": PreferenceThemeWidgetColorPanel.draw,
                "wcol": wcol,
            })

            ThemeGenericClassGenerator.generated_classes.append(paneltype)

    @staticmethod
    def generate_theme_area_child_panel_classes(parent_id, rna_type, theme_area, datapath):
        def generate_child_panel_classes_recurse(parent_id, rna_type, theme_area, datapath):
            props_type = {}

            for i, prop in enumerate(rna_type.properties):
                if prop.identifier == "rna_type":
                    continue

                props_type.setdefault((prop.type, prop.subtype), []).append(prop)

            for props_type, props_ls in sorted(props_type.items()):
                if props_type[0] == 'POINTER':
                    for i, prop in enumerate(props_ls):
                        new_datapath = datapath + "." + prop.identifier if datapath else prop.identifier
                        panel_id = parent_id + "_" + prop.identifier
                        paneltype = type(panel_id, (PreferenceThemeSpacePanel,), {
                            "bl_label": rna_type.properties[prop.identifier].name,
                            "bl_parent_id": parent_id,
                            "bl_options": {'DEFAULT_CLOSED'},
                            "draw": PreferenceThemeSpacePanel.draw,
                            "theme_area": theme_area.identifier,
                            "datapath": new_datapath,
                        })

                        ThemeGenericClassGenerator.generated_classes.append(paneltype)
                        generate_child_panel_classes_recurse(panel_id, prop.fixed_type, theme_area, new_datapath)

        generate_child_panel_classes_recurse(parent_id, rna_type, theme_area, datapath)

    @staticmethod
    def generate_panel_classes_from_theme_areas():
        from bpy.types import Theme

        for theme_area in Theme.bl_rna.properties['theme_area'].enum_items_static:
            if theme_area.identifier in {'USER_INTERFACE', 'STYLE', 'BONE_COLOR_SETS'}:
                continue

            panel_id = "USERPREF_PT_theme_" + theme_area.identifier.lower()
            # Generate panel-class from theme_area
            paneltype = type(panel_id, (PreferenceThemeSpacePanel,), {
                "bl_label": theme_area.name,
                "bl_options": {'DEFAULT_CLOSED'},
                "draw_header": PreferenceThemeSpacePanel.draw_header,
                "draw": PreferenceThemeSpacePanel.draw,
                "theme_area": theme_area.identifier,
                "icon": theme_area.icon,
                "datapath": theme_area.identifier.lower(),
            })

            ThemeGenericClassGenerator.generated_classes.append(paneltype)
            ThemeGenericClassGenerator.generate_theme_area_child_panel_classes(
                panel_id, Theme.bl_rna.properties[theme_area.identifier.lower()].fixed_type,
                theme_area, theme_area.identifier.lower())


class USERPREF_PT_file_paths(Panel):
    bl_space_type = 'PREFERENCES'
    bl_region_type = 'WINDOW'
    bl_label = "File Paths"

    @classmethod
    def poll(cls, context):
        prefs = context.preferences
        return (prefs.active_section == 'SYSTEM_FILES')

    def draw(self, context):
        layout = self.layout
        prefs = context.preferences
        paths = prefs.filepaths
        system = prefs.system

        layout.use_property_split = True
        layout.use_property_decorate = False  # No animation.

        flow = layout.grid_flow(row_major=False, columns=0, even_columns=True, even_rows=False, align=False)

        flow.prop(paths, "render_output_directory", text="Render Output")
        flow.prop(paths, "render_cache_directory", text="Render Cache")
        flow.prop(paths, "font_directory", text="Fonts")
        flow.prop(paths, "texture_directory", text="Textures")
        flow.prop(paths, "script_directory", text="Scripts")
        flow.prop(paths, "sound_directory", text="Sounds")
        flow.prop(paths, "temporary_directory", text="Temp")
        flow.prop(paths, "i18n_branches_directory", text="I18n Branches")
        flow.prop(paths, "image_editor", text="Image Editor")
        flow.prop(paths, "animation_player_preset", text="Playback Preset")

        if paths.animation_player_preset == 'CUSTOM':
            flow.prop(paths, "animation_player", text="Animation Player")


class USERPREF_PT_file_autorun(Panel):
    bl_space_type = 'PREFERENCES'
    bl_region_type = 'WINDOW'
    bl_label = "Auto Run Python Scripts"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        prefs = context.preferences
        return (prefs.active_section == 'SYSTEM_FILES')

    def draw_header(self, context):
        prefs = context.preferences
        paths = prefs.filepaths

        self.layout.prop(paths, "use_scripts_auto_execute", text="")

    def draw(self, context):
        layout = self.layout
        prefs = context.preferences
        paths = prefs.filepaths

        layout.use_property_split = True
        layout.use_property_decorate = False  # No animation.

        layout.active = paths.use_scripts_auto_execute

        box = layout.box()
        row = box.row()
        row.label(text="Excluded Paths:")
        row.operator("wm.userpref_autoexec_path_add", text="", icon='ADD', emboss=False)
        for i, path_cmp in enumerate(prefs.autoexec_paths):
            row = box.row()
            row.prop(path_cmp, "path", text="")
            row.prop(path_cmp, "use_glob", text="", icon='FILTER')
            row.operator("wm.userpref_autoexec_path_remove", text="", icon='X', emboss=False).index = i


class USERPREF_PT_file_saveload(PreferencePanel):
    bl_label = "Save & Load"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        prefs = context.preferences
        return (prefs.active_section == 'SYSTEM_FILES')

    def draw_props(self, context, layout):
        prefs = context.preferences
        paths = prefs.filepaths
        system = prefs.system

        flow = layout.grid_flow(row_major=False, columns=0, even_columns=True, even_rows=False, align=False)

        flow.prop(paths, "use_relative_paths")
        flow.prop(paths, "use_file_compression")
        flow.prop(paths, "use_load_ui")
        flow.prop(paths, "use_filter_files")
        flow.prop(paths, "show_hidden_files_datablocks")
        flow.prop(paths, "hide_recent_locations")
        flow.prop(paths, "hide_system_bookmarks")
        flow.prop(paths, "show_thumbnails")
        flow.prop(paths, "use_save_preview_images")
        flow.prop(paths, "use_tabs_as_spaces")

        layout.separator()

        flow = layout.grid_flow(row_major=False, columns=0, even_columns=True, even_rows=False, align=False)

        flow.prop(paths, "save_version")
        flow.prop(paths, "recent_files")
        flow.prop(paths, "author", text="Author")


class USERPREF_PT_file_saveload_autosave(PreferencePanel):
    bl_label = "Auto Save"
    bl_parent_id = "USERPREF_PT_file_saveload"

    def draw_props(self, context, layout):
        prefs = context.preferences
        paths = prefs.filepaths
        system = prefs.system

        flow = layout.grid_flow(row_major=False, columns=0, even_columns=True, even_rows=False, align=False)

        flow.prop(paths, "use_keep_session")
        flow.prop(paths, "use_auto_save_temporary_files")
        sub = flow.column()
        sub.active = paths.use_auto_save_temporary_files
        sub.prop(paths, "auto_save_time", text="Timer (mins)")


class USERPREF_MT_ndof_settings(Menu):
    # accessed from the window key-bindings in C (only)
    bl_label = "3D Mouse Settings"

    def draw(self, context):
        layout = self.layout

        input_prefs = context.preferences.inputs

        is_view3d = context.space_data.type == 'VIEW_3D'

        layout.prop(input_prefs, "ndof_sensitivity")
        layout.prop(input_prefs, "ndof_orbit_sensitivity")
        layout.prop(input_prefs, "ndof_deadzone")

        if is_view3d:
            layout.separator()
            layout.prop(input_prefs, "ndof_show_guide")

            layout.separator()
            layout.label(text="Orbit Style")
            layout.row().prop(input_prefs, "ndof_view_navigate_method", text="")
            layout.row().prop(input_prefs, "ndof_view_rotate_method", text="")
            layout.separator()
            layout.label(text="Orbit Options")
            layout.prop(input_prefs, "ndof_rotx_invert_axis")
            layout.prop(input_prefs, "ndof_roty_invert_axis")
            layout.prop(input_prefs, "ndof_rotz_invert_axis")

        # view2d use pan/zoom
        layout.separator()
        layout.label(text="Pan Options")
        layout.prop(input_prefs, "ndof_panx_invert_axis")
        layout.prop(input_prefs, "ndof_pany_invert_axis")
        layout.prop(input_prefs, "ndof_panz_invert_axis")
        layout.prop(input_prefs, "ndof_pan_yz_swap_axis")

        layout.label(text="Zoom Options")
        layout.prop(input_prefs, "ndof_zoom_invert")

        if is_view3d:
            layout.separator()
            layout.label(text="Fly/Walk Options")
            layout.prop(input_prefs, "ndof_fly_helicopter", icon='NDOF_FLY')
            layout.prop(input_prefs, "ndof_lock_horizon", icon='NDOF_DOM')


class USERPREF_PT_input_devices(PreferencePanel):
    bl_label = "Devices"

    @classmethod
    def poll(cls, context):
        prefs = context.preferences
        return (prefs.active_section == 'INPUT')

    def draw(self, context):
        pass


class USERPREF_PT_input_devices_keyboard(PreferencePanel):
    bl_label = "Keyboard"
    bl_parent_id = "USERPREF_PT_input_devices"

    def draw_props(self, context, layout):
        prefs = context.preferences
        inputs = prefs.inputs

        flow = layout.grid_flow(row_major=False, columns=0, even_columns=True, even_rows=False, align=False)

        flow.prop(inputs, "use_emulate_numpad")
        flow.prop(inputs, "use_numeric_input_advanced")


class USERPREF_PT_input_devices_mouse(PreferencePanel):
    bl_label = "Mouse"
    bl_parent_id = "USERPREF_PT_input_devices"

    def draw_props(self, context, layout):
        prefs = context.preferences
        inputs = prefs.inputs

        flow = layout.grid_flow(row_major=False, columns=0, even_columns=True, even_rows=False, align=False)

        flow.prop(inputs, "drag_threshold")
        flow.prop(inputs, "tweak_threshold")
        flow.prop(inputs, "mouse_double_click_time", text="Double Click Speed")
        flow.prop(inputs, "use_mouse_emulate_3_button")
        flow.prop(inputs, "use_mouse_continuous")
        flow.prop(inputs, "use_drag_immediately")


class USERPREF_PT_input_view(PreferencePanel):
    bl_label = "View Navigation"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        prefs = context.preferences
        return (prefs.active_section == 'INPUT')

    def draw(self, context):
        pass


class USERPREF_PT_input_view_orbit(PreferencePanel):
    bl_label = "Orbit & Pan"
    bl_parent_id = "USERPREF_PT_input_view"

    def draw_props(self, context, layout):
        import sys
        prefs = context.preferences
        inputs = prefs.inputs

        flow = layout.grid_flow(row_major=False, columns=0, even_columns=True, even_rows=False, align=False)

        flow.row().prop(inputs, "view_rotate_method", expand=True)
        flow.prop(inputs, "use_rotate_around_active")
        flow.prop(inputs, "use_auto_perspective")
        flow.prop(inputs, "use_mouse_depth_navigate")

        if sys.platform == "darwin":
            flow.prop(inputs, "use_trackpad_natural", text="Natural Trackpad Direction")


class USERPREF_PT_input_view_zoom(PreferencePanel):
    bl_label = "Zoom"
    bl_parent_id = "USERPREF_PT_input_view"

    def draw_props(self, context, layout):
        prefs = context.preferences
        inputs = prefs.inputs

        flow = layout.grid_flow(row_major=False, columns=0, even_columns=True, even_rows=False, align=False)

        flow.row().prop(inputs, "view_zoom_method", text="Zoom Method", expand=True)
        if inputs.view_zoom_method in {'DOLLY', 'CONTINUE'}:
            flow.row().prop(inputs, "view_zoom_axis", expand=True)
            flow.prop(inputs, "invert_mouse_zoom", text="Invert Mouse Zoom Direction")

        flow.prop(inputs, "invert_zoom_wheel", text="Invert Wheel Zoom Direction")
        # sub.prop(view, "wheel_scroll_lines", text="Scroll Lines")
        flow.prop(inputs, "use_zoom_to_mouse")


class USERPREF_PT_input_view_fly_walk(PreferencePanel):
    bl_label = "Fly & Walk"
    bl_parent_id = "USERPREF_PT_input_view"

    def draw_props(self, context, layout):
        prefs = context.preferences
        inputs = prefs.inputs

        flow = layout.grid_flow(row_major=False, columns=0, even_columns=True, even_rows=False, align=False)

        flow.row().prop(inputs, "navigation_mode", expand=True)
        flow.prop(inputs, "use_camera_lock_parent")


class USERPREF_PT_input_view_fly_walk_navigation(PreferencePanel):
    bl_label = "Walk"
    bl_parent_id = "USERPREF_PT_input_view_fly_walk"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        prefs = context.preferences
        return prefs.inputs.navigation_mode == 'WALK'

    def draw_props(self, context, layout):
        prefs = context.preferences
        inputs = prefs.inputs
        walk = inputs.walk_navigation

        flow = layout.grid_flow(row_major=False, columns=0, even_columns=True, even_rows=False, align=False)

        flow.prop(walk, "use_mouse_reverse")
        flow.prop(walk, "mouse_speed")
        flow.prop(walk, "teleport_time")

        sub = flow.column(align=True)
        sub.prop(walk, "walk_speed")
        sub.prop(walk, "walk_speed_factor")


class USERPREF_PT_input_view_fly_walk_gravity(PreferencePanel):
    bl_label = "Gravity"
    bl_parent_id = "USERPREF_PT_input_view_fly_walk"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        prefs = context.preferences
        return prefs.inputs.navigation_mode == 'WALK'

    def draw_header(self, context):
        prefs = context.preferences
        inputs = prefs.inputs
        walk = inputs.walk_navigation

        self.layout.prop(walk, "use_gravity", text="")

    def draw_props(self, context, layout):
        prefs = context.preferences
        inputs = prefs.inputs
        walk = inputs.walk_navigation

        layout.active = walk.use_gravity

        flow = layout.grid_flow(row_major=False, columns=0, even_columns=True, even_rows=False, align=False)

        flow.prop(walk, "view_height")
        flow.prop(walk, "jump_height")


class USERPREF_PT_input_devices_tablet(PreferencePanel):
    bl_label = "Tablet"
    bl_parent_id = "USERPREF_PT_input_devices"

    def draw_props(self, context, layout):
        prefs = context.preferences
        inputs = prefs.inputs

        flow = layout.grid_flow(row_major=False, columns=0, even_columns=True, even_rows=False, align=False)

        flow.prop(inputs, "pressure_threshold_max")
        flow.prop(inputs, "pressure_softness")


class USERPREF_PT_input_devices_ndof(PreferencePanel):
    bl_label = "NDOF"
    bl_options = {'DEFAULT_CLOSED'}
    bl_parent_id = "USERPREF_PT_input_devices"

    @classmethod
    def poll(cls, context):
        prefs = context.preferences
        inputs = prefs.inputs
        if inputs.use_ndof:
            return (prefs.active_section == 'INPUT')

    def draw_props(self, context, layout):
        prefs = context.preferences
        inputs = prefs.inputs

        flow = layout.grid_flow(row_major=False, columns=0, even_columns=True, even_rows=False, align=False)

        flow.prop(inputs, "ndof_sensitivity", text="Pan Sensitivity")
        flow.prop(inputs, "ndof_orbit_sensitivity", text="Orbit Sensitivity")
        flow.prop(inputs, "ndof_deadzone", text="Deadzone")

        layout.separator()

        flow.row().prop(inputs, "ndof_view_navigate_method", expand=True)
        flow.row().prop(inputs, "ndof_view_rotate_method", expand=True)


class USERPREF_MT_keyconfigs(Menu):
    bl_label = "KeyPresets"
    preset_subdir = "keyconfig"
    preset_operator = "wm.keyconfig_activate"

    def draw(self, context):
        Menu.draw_preset(self, context)


class USERPREF_PT_keymap(Panel):
    bl_space_type = 'PREFERENCES'
    bl_label = "Keymap"
    bl_region_type = 'WINDOW'
    bl_options = {'HIDE_HEADER'}

    @classmethod
    def poll(cls, context):
        prefs = context.preferences
        return (prefs.active_section == 'KEYMAP')

    def draw(self, context):
        from rna_keymap_ui import draw_keymaps

        layout = self.layout

        # import time

        # start = time.time()

        prefs = context.preferences
        keymappref = prefs.keymap

        col = layout.column()

        # Keymap Settings
        draw_keymaps(context, col)

        # print("runtime", time.time() - start)


class USERPREF_MT_addons_online_resources(Menu):
    bl_label = "Online Resources"

    # menu to open web-pages with addons development guides
    def draw(self, context):
        layout = self.layout

        layout.operator(
            "wm.url_open", text="Add-ons Catalog", icon='URL',
        ).url = "http://wiki.blender.org/index.php/Extensions:2.6/Py/Scripts"

        layout.separator()

        layout.operator(
            "wm.url_open", text="How to share your add-on", icon='URL',
        ).url = "http://wiki.blender.org/index.php/Dev:Py/Sharing"
        layout.operator(
            "wm.url_open", text="Add-on Guidelines", icon='URL',
        ).url = "http://wiki.blender.org/index.php/Dev:2.5/Py/Scripts/Guidelines/Addons"
        layout.operator(
            "wm.url_open", text="API Concepts", icon='URL',
        ).url = bpy.types.WM_OT_doc_view._prefix + "/info_quickstart.html"
        layout.operator(
            "wm.url_open", text="Add-on Tutorial", icon='URL',
        ).url = bpy.types.WM_OT_doc_view._prefix + "/info_tutorial_addon.html"


class USERPREF_PT_addons(Panel):
    bl_space_type = 'PREFERENCES'
    bl_label = "Add-ons"
    bl_region_type = 'WINDOW'
    bl_options = {'HIDE_HEADER'}

    _support_icon_mapping = {
        'OFFICIAL': 'FILE_BLEND',
        'COMMUNITY': 'COMMUNITY',
        'TESTING': 'EXPERIMENTAL',
    }

    @classmethod
    def poll(cls, context):
        prefs = context.preferences
        return (prefs.active_section == 'ADDONS')

    @staticmethod
    def is_user_addon(mod, user_addon_paths):
        import os

        if not user_addon_paths:
            for path in (
                    bpy.utils.script_path_user(),
                    bpy.utils.script_path_pref(),
            ):
                if path is not None:
                    user_addon_paths.append(os.path.join(path, "addons"))

        for path in user_addon_paths:
            if bpy.path.is_subdir(mod.__file__, path):
                return True
        return False

    @staticmethod
    def draw_error(layout, message):
        lines = message.split("\n")
        box = layout.box()
        sub = box.row()
        sub.label(text=lines[0])
        sub.label(icon='ERROR')
        for l in lines[1:]:
            box.label(text=l)

    def draw(self, context):
        import os
        import addon_utils

        layout = self.layout

        prefs = context.preferences
        used_ext = {ext.module for ext in prefs.addons}

        addon_user_dirs = tuple(
            p for p in (
                os.path.join(prefs.filepaths.script_directory, "addons"),
                bpy.utils.user_resource('SCRIPTS', "addons"),
            )
            if p
        )

        # Development option for 2.8x, don't show users bundled addons
        # unless they have been updated for 2.8x.
        # Developers can turn them on with '--debug'
        show_official_27x_addons = bpy.app.debug

        # collect the categories that can be filtered on
        addons = [
            (mod, addon_utils.module_bl_info(mod))
            for mod in addon_utils.modules(refresh=False)
        ]

        row = layout.row()
        row.operator("wm.addon_install", icon='IMPORT', text="Install...")
        row.operator("wm.addon_refresh", icon='FILE_REFRESH', text="Refresh")
        row.menu("USERPREF_MT_addons_online_resources", text="Online Resources")

        layout.separator()

        row = layout.row()
        row.prop(context.window_manager, "addon_support", expand=True)
        row.prop(context.window_manager, "addon_filter", text="")
        row.prop(context.window_manager, "addon_search", text="", icon='VIEWZOOM')

        col = layout.column()

        # set in addon_utils.modules_refresh()
        if addon_utils.error_duplicates:
            box = col.box()
            row = box.row()
            row.label(text="Multiple add-ons with the same name found!")
            row.label(icon='ERROR')
            box.label(text="Delete one of each pair to resolve:")
            for (addon_name, addon_file, addon_path) in addon_utils.error_duplicates:
                box.separator()
                sub_col = box.column(align=True)
                sub_col.label(text=addon_name + ":")
                sub_col.label(text="    " + addon_file)
                sub_col.label(text="    " + addon_path)

        if addon_utils.error_encoding:
            self.draw_error(
                col,
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
            if (
                    (filter == "All") or
                    (filter == info["category"]) or
                    (filter == "Enabled" and is_enabled) or
                    (filter == "Disabled" and not is_enabled) or
                    (filter == "User" and (mod.__file__.startswith(addon_user_dirs)))
            ):
                if search and search not in info["name"].lower():
                    if info["author"]:
                        if search not in info["author"].lower():
                            continue
                    else:
                        continue

                # Skip 2.7x add-ons included with Blender, unless in debug mode.
                is_addon_27x = info.get("blender", (0,)) < (2, 80)
                if (
                        is_addon_27x and
                        (not show_official_27x_addons) and
                        (not mod.__file__.startswith(addon_user_dirs))
                ):
                    continue

                # Addon UI Code
                col_box = col.column()
                box = col_box.box()
                colsub = box.column()
                row = colsub.row(align=True)

                row.operator(
                    "wm.addon_expand",
                    icon='DISCLOSURE_TRI_DOWN' if info["show_expanded"] else 'DISCLOSURE_TRI_RIGHT',
                    emboss=False,
                ).module = module_name

                row.operator(
                    "wm.addon_disable" if is_enabled else "wm.addon_enable",
                    icon='CHECKBOX_HLT' if is_enabled else 'CHECKBOX_DEHLT', text="",
                    emboss=False,
                ).module = module_name

                sub = row.row()
                sub.active = is_enabled
                sub.label(text="%s: %s" % (info["category"], info["name"]))

                # WARNING: 2.8x exception, may be removed
                # use disabled state for old add-ons, chances are they are broken.
                if is_addon_27x:
                    sub.label(text="upgrade to 2.8x required")
                    sub.label(icon='ERROR')
                # Remove code above after 2.8x migration is complete.
                elif info["warning"]:
                    sub.label(icon='ERROR')

                # icon showing support level.
                sub.label(icon=self._support_icon_mapping.get(info["support"], 'QUESTION'))

                # Expanded UI (only if additional info is available)
                if info["show_expanded"]:
                    if info["description"]:
                        split = colsub.row().split(factor=0.15)
                        split.label(text="Description:")
                        split.label(text=info["description"])
                    if info["location"]:
                        split = colsub.row().split(factor=0.15)
                        split.label(text="Location:")
                        split.label(text=info["location"])
                    if mod:
                        split = colsub.row().split(factor=0.15)
                        split.label(text="File:")
                        split.label(text=mod.__file__, translate=False)
                    if info["author"]:
                        split = colsub.row().split(factor=0.15)
                        split.label(text="Author:")
                        split.label(text=info["author"], translate=False)
                    if info["version"]:
                        split = colsub.row().split(factor=0.15)
                        split.label(text="Version:")
                        split.label(text=".".join(str(x) for x in info["version"]), translate=False)
                    if info["warning"]:
                        split = colsub.row().split(factor=0.15)
                        split.label(text="Warning:")
                        split.label(text="  " + info["warning"], icon='ERROR')

                    user_addon = USERPREF_PT_addons.is_user_addon(mod, user_addon_paths)
                    tot_row = bool(info["wiki_url"]) + bool(user_addon)

                    if tot_row:
                        split = colsub.row().split(factor=0.15)
                        split.label(text="Internet:")
                        if info["wiki_url"]:
                            split.operator(
                                "wm.url_open", text="Documentation", icon='HELP',
                            ).url = info["wiki_url"]
                        # Only add "Report a Bug" button if tracker_url is set
                        # or the add-on is bundled (use official tracker then).
                        if info.get("tracker_url") or not user_addon:
                            split.operator(
                                "wm.url_open", text="Report a Bug", icon='URL',
                            ).url = info.get(
                                "tracker_url",
                                "https://developer.blender.org/maniphest/task/edit/form/2",
                            )
                        if user_addon:
                            split.operator(
                                "wm.addon_remove", text="Remove", icon='CANCEL',
                            ).module = mod.__name__

                        for _ in range(4 - tot_row):
                            split.separator()

                    # Show addon user preferences
                    if is_enabled:
                        addon_preferences = prefs.addons[module_name].preferences
                        if addon_preferences is not None:
                            draw = getattr(addon_preferences, "draw", None)
                            if draw is not None:
                                addon_preferences_class = type(addon_preferences)
                                box_prefs = col_box.box()
                                box_prefs.label(text="Preferences:")
                                addon_preferences_class.layout = box_prefs
                                try:
                                    draw(context)
                                except:
                                    import traceback
                                    traceback.print_exc()
                                    box_prefs.label(text="Error (see console)", icon='ERROR')
                                del addon_preferences_class.layout

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
                row = colsub.row(align=True)

                row.label(text="", icon='ERROR')

                if is_enabled:
                    row.operator(
                        "wm.addon_disable", icon='CHECKBOX_HLT', text="", emboss=False,
                    ).module = module_name

                row.label(text=module_name, translate=False)


class USERPREF_PT_studiolight_add(PreferencePanel):
    bl_space_type = 'PREFERENCES'
    bl_label = "Add Lights"
    bl_region_type = 'WINDOW'
    bl_options = {'HIDE_HEADER'}

    @classmethod
    def poll(cls, context):
        prefs = context.preferences
        return (prefs.active_section == 'LIGHTS')

    def draw(self, context):
        layout = self.layout
        prefs = context.preferences

        row = layout.row()
        row.operator("wm.studiolight_install", icon='IMPORT', text="Add MatCap...").type = 'MATCAP'
        row.operator("wm.studiolight_install", icon='IMPORT', text="Add LookDev HDRI...").type = 'WORLD'
        op = row.operator("wm.studiolight_install", icon='IMPORT', text="Add Studio Light...")
        op.type = 'STUDIO'
        op.filter_glob = ".sl"


class StudioLightPanelMixin():
    bl_space_type = 'PREFERENCES'
    bl_region_type = 'WINDOW'

    @classmethod
    def poll(cls, context):
        prefs = context.preferences
        return (prefs.active_section == 'LIGHTS')

    def _get_lights(self, prefs):
        return [light for light in prefs.studio_lights if light.is_user_defined and light.type == self.sl_type]

    def draw(self, context):
        layout = self.layout
        prefs = context.preferences
        lights = self._get_lights(prefs)

        self.draw_light_list(layout, lights)

    def draw_light_list(self, layout, lights):
        if lights:
            flow = layout.grid_flow(row_major=False, columns=0, even_columns=False, even_rows=False, align=False)
            for studio_light in lights:
                self.draw_studio_light(flow, studio_light)
        else:
            layout.label(text="No custom {} configured".format(self.bl_label))

    def draw_studio_light(self, layout, studio_light):
        box = layout.box()
        row = box.row()

        row.template_icon(layout.icon(studio_light), scale=6.0)
        col = row.column()
        op = col.operator("wm.studiolight_uninstall", text="", icon='REMOVE')
        op.index = studio_light.index

        if studio_light.type == 'STUDIO':
            op = col.operator("wm.studiolight_copy_settings", text="", icon='IMPORT')
            op.index = studio_light.index

        box.label(text=studio_light.name)


class USERPREF_PT_studiolight_matcaps(Panel, StudioLightPanelMixin):
    bl_label = "MatCaps"
    sl_type = 'MATCAP'


class USERPREF_PT_studiolight_world(Panel, StudioLightPanelMixin):
    bl_label = "LookDev HDRIs"
    sl_type = 'WORLD'


class USERPREF_PT_studiolight_lights(Panel, StudioLightPanelMixin):
    bl_label = "Studio Lights"
    sl_type = 'STUDIO'


class USERPREF_PT_studiolight_light_editor(Panel):
    bl_label = "Studio Light Editor"
    bl_parent_id = "USERPREF_PT_studiolight_lights"
    bl_space_type = 'PREFERENCES'
    bl_region_type = 'WINDOW'
    bl_options = {'DEFAULT_CLOSED'}

    def opengl_light_buttons(self, layout, light):

        col = layout.column()
        col.active = light.use

        col.prop(light, "use", text="Use Light")
        col.prop(light, "diffuse_color", text="Diffuse")
        col.prop(light, "specular_color", text="Specular")
        col.prop(light, "smooth")
        col.prop(light, "direction")

    def draw(self, context):
        layout = self.layout

        prefs = context.preferences
        system = prefs.system

        row = layout.row()
        row.prop(system, "edit_studio_light", toggle=True)
        row.operator("wm.studiolight_new", text="Save as Studio light", icon='FILE_TICK')

        layout.separator()

        layout.use_property_split = True
        column = layout.split()
        column.active = system.edit_studio_light

        light = system.solid_lights[0]
        colsplit = column.split(factor=0.85)
        self.opengl_light_buttons(colsplit, light)

        light = system.solid_lights[1]
        colsplit = column.split(factor=0.85)
        self.opengl_light_buttons(colsplit, light)

        light = system.solid_lights[2]
        colsplit = column.split(factor=0.85)
        self.opengl_light_buttons(colsplit, light)

        light = system.solid_lights[3]
        self.opengl_light_buttons(column, light)

        layout.separator()

        layout.prop(system, "light_ambient")


ThemeGenericClassGenerator.generate_panel_classes_for_wcols()

# Order of registration defines order in UI, so dynamically generated classes are 'injected' in the intended order.
classes = (USERPREF_PT_theme_user_interface,) + tuple(ThemeGenericClassGenerator.generated_classes)

classes += (
    USERPREF_HT_header,
    USERPREF_PT_navigation,
    USERPREF_PT_save_preferences,

    USERPREF_PT_interface_display,
    USERPREF_PT_interface_display_info,
    USERPREF_PT_interface_text,
    USERPREF_PT_interface_text_translate,
    USERPREF_PT_interface_viewports,
    USERPREF_PT_interface_viewports_3d,
    USERPREF_PT_interface_viewports_3d_weight_paint,
    USERPREF_PT_interface_viewports_2d,
    USERPREF_PT_interface_menus,
    USERPREF_PT_interface_menus_mouse_over,
    USERPREF_PT_interface_menus_pie,
    USERPREF_PT_interface_develop,
    USERPREF_PT_interface_templates,

    USERPREF_PT_edit_objects,
    USERPREF_PT_edit_animation,
    USERPREF_PT_edit_animation_autokey,
    USERPREF_PT_edit_animation_fcurves,
    USERPREF_PT_edit_duplicate_data,
    USERPREF_PT_edit_gpencil,
    USERPREF_PT_edit_annotations,
    USERPREF_PT_edit_cursor,
    USERPREF_PT_edit_misc,

    USERPREF_PT_system_opengl,
    USERPREF_PT_system_opengl_textures,
    USERPREF_PT_system_opengl_selection,
    USERPREF_PT_system_sound,
    USERPREF_PT_system_compute_device,
    USERPREF_PT_system_memory,

    USERPREF_MT_interface_theme_presets,
    USERPREF_PT_theme,
    USERPREF_PT_theme_interface_state,
    USERPREF_PT_theme_interface_styles,
    USERPREF_PT_theme_interface_gizmos,
    USERPREF_PT_theme_interface_icons,
    USERPREF_PT_theme_text_style,
    USERPREF_PT_theme_bone_color_sets,

    USERPREF_PT_file_paths,
    USERPREF_PT_file_autorun,
    USERPREF_PT_file_saveload,
    USERPREF_PT_file_saveload_autosave,

    USERPREF_MT_ndof_settings,
    USERPREF_MT_keyconfigs,

    USERPREF_PT_input_devices,
    USERPREF_PT_input_devices_keyboard,
    USERPREF_PT_input_devices_mouse,
    USERPREF_PT_input_devices_tablet,
    USERPREF_PT_input_devices_ndof,
    USERPREF_PT_input_view,
    USERPREF_PT_input_view_orbit,
    USERPREF_PT_input_view_zoom,
    USERPREF_PT_input_view_fly_walk,
    USERPREF_PT_input_view_fly_walk_navigation,
    USERPREF_PT_input_view_fly_walk_gravity,

    USERPREF_PT_keymap,
    USERPREF_MT_addons_online_resources,
    USERPREF_PT_addons,

    USERPREF_PT_studiolight_add,
    USERPREF_PT_studiolight_lights,
    USERPREF_PT_studiolight_light_editor,
    USERPREF_PT_studiolight_matcaps,
    USERPREF_PT_studiolight_world,
)

# Add dynamically generated editor theme panels last, so they show up last in the theme section.
ThemeGenericClassGenerator.generated_classes.clear()
ThemeGenericClassGenerator.generate_panel_classes_from_theme_areas()
classes += tuple(ThemeGenericClassGenerator.generated_classes)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
