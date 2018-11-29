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
    bl_space_type = 'USER_PREFERENCES'

    def draw(self, context):
        layout = self.layout

        layout.template_header()

        userpref = context.user_preferences

        if userpref.active_section == 'INPUT':
            layout.operator("wm.keyconfig_import", icon='IMPORT')
            layout.operator("wm.keyconfig_export", icon='EXPORT')
        elif userpref.active_section == 'ADDONS':
            layout.operator("wm.addon_install", icon='FILEBROWSER')
            layout.operator("wm.addon_refresh", icon='FILE_REFRESH')
            layout.menu("USERPREF_MT_addons_online_resources")
        elif userpref.active_section == 'LIGHTS':
            layout.operator('wm.studiolight_install', text="Add MatCap").type = 'MATCAP'
            layout.operator('wm.studiolight_install', text="Add LookDev HDRI").type = 'WORLD'
            op = layout.operator('wm.studiolight_install', text="Add Studio Light")
            op.type = 'STUDIO'
            op.filter_glob = ".sl"
        elif userpref.active_section == 'THEMES':
            layout.operator("wm.theme_install", icon='FILEBROWSER')
            layout.operator("ui.reset_default_theme", icon='LOOP_BACK')

        layout.separator_spacer()

        layout.operator_context = 'EXEC_AREA'
        layout.operator("wm.save_userpref")


class USERPREF_PT_navigation(Panel):
    bl_label = ""
    bl_space_type = 'USER_PREFERENCES'
    bl_region_type = 'NAVIGATION_BAR'
    bl_options = {'HIDE_HEADER'}

    def draw(self, context):
        layout = self.layout

        userpref = context.user_preferences

        col = layout.column()

        col.scale_x = 1.3
        col.scale_y = 1.3
        col.prop(userpref, "active_section", expand=True)


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

        split = layout.split()
        row = split.row()
        col = row.column()

        col.label(text="Display:")
        col.prop(view, "ui_scale", text="Scale")
        col.prop(view, "ui_line_width", text="Line Width")
        col.prop(view, "show_tooltips")
        col.prop(view, "show_object_info", text="Object Info")
        col.prop(view, "show_large_cursors")
        col.prop(view, "show_view_name", text="View Name")
        col.prop(view, "show_playback_fps", text="Playback FPS")
        col.prop(view, "object_origin_size")

        col.separator()

        # col.prop(view, "show_gizmo_navigate")

        sub = col.column(align=True)

        sub.label(text="3D Viewport Axis:")
        sub.row().prop(view, "mini_axis_type", text="")

        sub = col.column(align=True)
        sub.active = view.mini_axis_type == 'MINIMAL'
        sub.prop(view, "mini_axis_size", text="Size")
        sub.prop(view, "mini_axis_brightness", text="Brightness")

        col.separator()

        # Toolbox doesn't exist yet
        # col.label(text="Toolbox:")
        #col.prop(view, "show_column_layout")
        #col.label(text="Open Toolbox Delay:")
        #col.prop(view, "open_left_mouse_delay", text="Hold LMB")
        #col.prop(view, "open_right_mouse_delay", text="Hold RMB")
        col.prop(view, "show_gizmo", text="Gizmos")
        sub = col.column()
        sub.active = view.show_gizmo
        sub.prop(view, "gizmo_size", text="Size")

        col.separator()

        col.label(text="Development:")
        col.prop(view, "show_tooltips_python")
        col.prop(view, "show_developer_ui")

        row = split.row()
        row.separator()
        col = row.column()

        col.label(text="View Gizmos:")
        col.prop(view, "use_mouse_depth_cursor")
        col.prop(view, "use_cursor_lock_adjust")
        col.prop(view, "use_mouse_depth_navigate")
        col.prop(view, "use_zoom_to_mouse")
        col.prop(view, "use_rotate_around_active")
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
        col.prop(view, "view_frame_type")
        if view.view_frame_type == 'SECONDS':
            col.prop(view, "view_frame_seconds")
        elif view.view_frame_type == 'KEYFRAMES':
            col.prop(view, "view_frame_keyframes")

        row = split.row()
        row.separator()
        col = row.column()

        col.label(text="Menus:")
        col.prop(view, "use_mouse_over_open")
        sub = col.column()
        sub.active = view.use_mouse_over_open

        sub.prop(view, "open_toplevel_delay", text="Top Level")
        sub.prop(view, "open_sublevel_delay", text="Sub Level")

        col.separator()
        col.label(text="Pie Menus:")
        sub = col.column(align=True)
        sub.prop(view, "pie_animation_timeout")
        sub.prop(view, "pie_initial_timeout")
        sub.prop(view, "pie_menu_radius")
        sub.prop(view, "pie_menu_threshold")
        sub.prop(view, "pie_menu_confirm")
        col.separator()

        col.prop(view, "show_splash")

        col.label(text="Warnings:")
        col.prop(view, "use_quit_dialog")

        col.separator()

        col.label(text="App Template:")
        col.label(text="Options intended for use with app-templates only.")
        col.prop(view, "show_layout_ui")


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

        split = layout.split()
        row = split.row()
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

        row = split.row()
        row.separator()
        col = row.column()

        col.label(text="Grease Pencil/Annotations:")
        col.separator()
        col.prop(edit, "grease_pencil_manhattan_distance", text="Manhattan Distance")
        col.prop(edit, "grease_pencil_euclidean_distance", text="Euclidean Distance")
        col.separator()

        col.label(text="Annotations:")
        sub = col.row()
        sub.prop(edit, "grease_pencil_default_color", text="Default Color")
        col.prop(edit, "grease_pencil_eraser_radius", text="Eraser Radius")
        col.separator()
        col.prop(edit, "use_grease_pencil_simplify_stroke", text="Simplify Stroke")
        col.separator()

        col.separator()
        col.separator()
        col.separator()
        col.label(text="Playback:")
        col.prop(edit, "use_negative_frames")
        col.separator()
        col.label(text="Node Editor:")
        col.prop(edit, "node_margin")
        col.label(text="Animation Editors:")
        col.prop(edit, "fcurve_unselected_alpha", text="F-Curve Visibility")

        row = split.row()
        row.separator()
        col = row.column()

        col.label(text="Keyframing:")
        col.prop(edit, "use_visual_keying")
        col.prop(edit, "use_keyframe_insert_needed", text="Only Insert Needed")

        col.separator()

        col.prop(edit, "use_auto_keying", text="Auto Keyframing:")
        col.prop(edit, "use_auto_keying_warning")

        sub = col.column()

        # ~ sub.active = edit.use_keyframe_insert_auto # incorrect, time-line can enable
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
        col.prop(edit, "use_numeric_input_advanced")

        row = split.row()
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
        col.prop(edit, "use_duplicate_light", text="Light")
        col.prop(edit, "use_duplicate_material", text="Material")
        col.prop(edit, "use_duplicate_texture", text="Texture")
        #col.prop(edit, "use_duplicate_fcurve", text="F-Curve")
        col.prop(edit, "use_duplicate_action", text="Action")
        col.prop(edit, "use_duplicate_particle", text="Particle")


class USERPREF_PT_system_general(Panel):
    bl_space_type = 'USER_PREFERENCES'
    bl_label = "System General"
    bl_region_type = 'WINDOW'
    bl_options = {'HIDE_HEADER'}

    @classmethod
    def poll(cls, context):
        userpref = context.user_preferences
        return (userpref.active_section == 'SYSTEM_GENERAL')

    def draw(self, context):
        import sys
        layout = self.layout

        userpref = context.user_preferences
        system = userpref.system

        split = layout.split()

        # 1. Column
        column = split.column()
        colsplit = column.split(factor=0.85)

        col = colsplit.column()
        col.label(text="General:")

        col.prop(system, "scrollback", text="Console Scrollback")

        col.separator()

        col.label(text="Sound:")
        col.row().prop(system, "audio_device", expand=False)
        sub = col.column()
        sub.active = system.audio_device not in {'NONE', 'Null'}
        #sub.prop(system, "use_preview_images")
        sub.prop(system, "audio_channels", text="Channels")
        sub.prop(system, "audio_mixing_buffer", text="Mixing Buffer")
        sub.prop(system, "audio_sample_rate", text="Sample Rate")
        sub.prop(system, "audio_sample_format", text="Sample Format")

        col.separator()

        if bpy.app.build_options.cycles:
            addon = userpref.addons.get("cycles")
            if addon is not None:
                addon.preferences.draw_impl(col, context)
            del addon

        # NOTE: Disabled for until GPU side of OpenSubdiv is brought back.
        # if hasattr(system, "opensubdiv_compute_type"):
        #     col.label(text="OpenSubdiv compute:")
        #     col.row().prop(system, "opensubdiv_compute_type", text="")

        # 2. Column
        column = split.column()
        colsplit = column.split(factor=0.85)

        col = colsplit.column()
        col.label(text="OpenGL:")
        col.prop(system, "gl_clip_alpha", slider=True)
        col.prop(system, "use_gpu_mipmap")
        col.prop(system, "use_16bit_textures")

        col.separator()
        col.label(text="Selection:")
        col.prop(system, "select_method", text="")
        col.prop(system, "use_select_pick_depth")

        col.separator()

        col.label(text="Anisotropic Filtering:")
        col.prop(system, "anisotropic_filter", text="")

        col.separator()

        col.prop(system, "multi_sample", text="")
        if sys.platform == "linux" and system.multi_sample != 'NONE':
            col.label(text="Might fail for Mesh editing selection!")
            col.separator()
        col.prop(system, "use_region_overlap")

        col.separator()
        col.prop(system, "gpu_viewport_quality")

        col.separator()
        col.label(text="Grease Pencil Options:")
        col.prop(system, "gpencil_multi_sample", text="")

        col.separator()
        col.label(text="Text Draw Options:")
        col.prop(system, "use_text_antialiasing", text="Anti-aliasing")
        sub = col.column()
        sub.active = system.use_text_antialiasing
        sub.prop(system, "text_hinting", text="Hinting")

        # 3. Column
        column = split.column()

        column.label(text="Textures:")
        column.prop(system, "gl_texture_limit", text="Limit Size")
        column.prop(system, "texture_time_out", text="Time Out")
        column.prop(system, "texture_collection_rate", text="Collection Rate")

        column.separator()

        column.label(text="Images Draw Method:")
        column.prop(system, "image_draw_method", text="")

        column.separator()

        column.label(text="Sequencer/Clip Editor:")
        # currently disabled in the code
        # column.prop(system, "prefetch_frames")
        column.prop(system, "memory_cache_limit")

        column.separator()

        column.label(text="Color Picker Type:")
        column.row().prop(system, "color_picker_type", text="")

        column.separator()

        column.prop(system, "use_weight_color_range", text="Custom Weight Paint Range")
        sub = column.column()
        sub.active = system.use_weight_color_range
        sub.template_color_ramp(system, "weight_color_range", expand=True)

        column.separator()
        column.prop(system, "font_path_ui")
        column.prop(system, "font_path_ui_mono")

        if bpy.app.build_options.international:
            column.prop(system, "use_international_fonts")
            sub_col = column.column()
            sub_col.active = system.use_international_fonts
            sub_col.prop(system, "language")
            row = sub_col.row()
            row.label(text="Translate:", text_ctxt=i18n_contexts.id_windowmanager)
            row = sub_col.row(align=True)
            row.prop(system, "use_translate_tooltips", text="Tooltips", toggle=True)
            row.prop(system, "use_translate_interface", text="Interface", toggle=True)
            row.prop(system, "use_translate_new_dataname", text="New Data", toggle=True)


class USERPREF_MT_interface_theme_presets(Menu):
    bl_label = "Presets"
    preset_subdir = "interface_theme"
    preset_operator = "script.execute_preset"
    preset_type = 'XML'
    preset_xml_map = (
        ("user_preferences.themes[0]", "Theme"),
        ("user_preferences.ui_styles[0]", "ThemeStyle"),
    )
    draw = Menu.draw_preset

    def reset_cb(context):
        bpy.ops.ui.reset_default_theme()


class USERPREF_PT_theme(Panel):
    bl_space_type = 'USER_PREFERENCES'
    bl_label = "Themes"
    bl_region_type = 'WINDOW'
    bl_options = {'HIDE_HEADER'}

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

    @staticmethod
    def _theme_generic(split, themedata, theme_area):

        col = split.column()

        def theme_generic_recurse(data):
            col.label(text=data.rna_type.name)
            row = col.row()
            subsplit = row.split(factor=0.95)

            padding1 = subsplit.split(factor=0.15)
            padding1.column()

            subsplit = row.split(factor=0.85)

            padding2 = subsplit.split(factor=0.15)
            padding2.column()

            colsub_pair = padding1.column(), padding2.column()

            props_type = {}

            for i, prop in enumerate(data.rna_type.properties):
                if prop.identifier == "rna_type":
                    continue

                props_type.setdefault((prop.type, prop.subtype), []).append(prop)

            th_delimiters = USERPREF_PT_theme.ui_delimiters.get(theme_area)
            for props_type, props_ls in sorted(props_type.items()):
                if props_type[0] == 'POINTER':
                    for i, prop in enumerate(props_ls):
                        theme_generic_recurse(getattr(data, prop.identifier))
                else:
                    if th_delimiters is None:
                        # simple, no delimiters
                        for i, prop in enumerate(props_ls):
                            colsub_pair[i % 2].row().prop(data, prop.identifier)
                    else:
                        # add hard coded delimiters
                        i = 0
                        for prop in props_ls:
                            colsub = colsub_pair[i]
                            colsub.row().prop(data, prop.identifier)
                            i = (i + 1) % 2
                            if prop.identifier in th_delimiters:
                                if i:
                                    colsub = colsub_pair[1]
                                    colsub.row().label(text="")
                                colsub_pair[0].row().label(text="")
                                colsub_pair[1].row().label(text="")
                                i = 0

        theme_generic_recurse(themedata)

    @staticmethod
    def _theme_widget_style(layout, widget_style):

        row = layout.row()

        subsplit = row.split(factor=0.95)

        padding = subsplit.split(factor=0.15)
        colsub = padding.column()
        colsub = padding.column()
        colsub.row().prop(widget_style, "outline")
        colsub.row().prop(widget_style, "item", slider=True)
        colsub.row().prop(widget_style, "inner", slider=True)
        colsub.row().prop(widget_style, "inner_sel", slider=True)
        colsub.row().prop(widget_style, "roundness")

        subsplit = row.split(factor=0.85)

        padding = subsplit.split(factor=0.15)
        colsub = padding.column()
        colsub = padding.column()
        colsub.row().prop(widget_style, "text")
        colsub.row().prop(widget_style, "text_sel")
        colsub.prop(widget_style, "show_shaded")
        subsub = colsub.column(align=True)
        subsub.active = widget_style.show_shaded
        subsub.prop(widget_style, "shadetop")
        subsub.prop(widget_style, "shadedown")

        layout.separator()

    @staticmethod
    def _ui_font_style(layout, font_style):

        split = layout.split()

        col = split.column()
        col.label(text="Kerning Style:")
        col.row().prop(font_style, "font_kerning_style", expand=True)
        col.prop(font_style, "points")

        col = split.column()
        col.label(text="Shadow Offset:")
        col.prop(font_style, "shadow_offset_x", text="X")
        col.prop(font_style, "shadow_offset_y", text="Y")

        col = split.column()
        col.prop(font_style, "shadow")
        col.prop(font_style, "shadow_alpha")
        col.prop(font_style, "shadow_value")

        layout.separator()

    @classmethod
    def poll(cls, context):
        userpref = context.user_preferences
        return (userpref.active_section == 'THEMES')

    def draw(self, context):
        layout = self.layout

        theme = context.user_preferences.themes[0]

        split_themes = layout.split(factor=0.2)

        sub = split_themes.column()

        sub.label(text="Presets:")
        subrow = sub.row(align=True)

        subrow.menu("USERPREF_MT_interface_theme_presets", text=USERPREF_MT_interface_theme_presets.bl_label)
        subrow.operator("wm.interface_theme_preset_add", text="", icon='ADD')
        subrow.operator("wm.interface_theme_preset_add", text="", icon='REMOVE').remove_active = True
        sub.separator()

        sub.prop(theme, "theme_area", expand=True)

        split = layout.split(factor=0.4)

        layout.separator()
        layout.separator()

        split = split_themes.split()

        if theme.theme_area == 'USER_INTERFACE':
            col = split.column()
            ui = theme.user_interface

            col.label(text="Regular:")
            self._theme_widget_style(col, ui.wcol_regular)

            col.label(text="Tool:")
            self._theme_widget_style(col, ui.wcol_tool)

            col.label(text="Toolbar Item:")
            self._theme_widget_style(col, ui.wcol_toolbar_item)

            col.label(text="Radio Buttons:")
            self._theme_widget_style(col, ui.wcol_radio)

            col.label(text="Text:")
            self._theme_widget_style(col, ui.wcol_text)

            col.label(text="Option:")
            self._theme_widget_style(col, ui.wcol_option)

            col.label(text="Toggle:")
            self._theme_widget_style(col, ui.wcol_toggle)

            col.label(text="Number Field:")
            self._theme_widget_style(col, ui.wcol_num)

            col.label(text="Value Slider:")
            self._theme_widget_style(col, ui.wcol_numslider)

            col.label(text="Box:")
            self._theme_widget_style(col, ui.wcol_box)

            col.label(text="Menu:")
            self._theme_widget_style(col, ui.wcol_menu)

            col.label(text="Pie Menu:")
            self._theme_widget_style(col, ui.wcol_pie_menu)

            col.label(text="Pulldown:")
            self._theme_widget_style(col, ui.wcol_pulldown)

            col.label(text="Menu Back:")
            self._theme_widget_style(col, ui.wcol_menu_back)

            col.label(text="Tooltip:")
            self._theme_widget_style(col, ui.wcol_tooltip)

            col.label(text="Menu Item:")
            self._theme_widget_style(col, ui.wcol_menu_item)

            col.label(text="Scroll Bar:")
            self._theme_widget_style(col, ui.wcol_scroll)

            col.label(text="Progress Bar:")
            self._theme_widget_style(col, ui.wcol_progress)

            col.label(text="List Item:")
            self._theme_widget_style(col, ui.wcol_list_item)

            col.label(text="Tab:")
            self._theme_widget_style(col, ui.wcol_tab)

            ui_state = theme.user_interface.wcol_state
            col.label(text="State:")

            row = col.row()

            subsplit = row.split(factor=0.95)

            padding = subsplit.split(factor=0.15)
            colsub = padding.column()
            colsub = padding.column()
            colsub.row().prop(ui_state, "inner_anim")
            colsub.row().prop(ui_state, "inner_anim_sel")
            colsub.row().prop(ui_state, "inner_driven")
            colsub.row().prop(ui_state, "inner_driven_sel")
            colsub.row().prop(ui_state, "blend")

            subsplit = row.split(factor=0.85)

            padding = subsplit.split(factor=0.15)
            colsub = padding.column()
            colsub = padding.column()
            colsub.row().prop(ui_state, "inner_key")
            colsub.row().prop(ui_state, "inner_key_sel")
            colsub.row().prop(ui_state, "inner_overridden")
            colsub.row().prop(ui_state, "inner_overridden_sel")

            col.separator()
            col.separator()

            col.label(text="Styles:")

            row = col.row()

            subsplit = row.split(factor=0.95)

            padding = subsplit.split(factor=0.15)
            colsub = padding.column()
            colsub = padding.column()
            colsub.row().prop(ui, "menu_shadow_fac")
            colsub.row().prop(ui, "icon_alpha")
            colsub.row().prop(ui, "icon_saturation")
            colsub.row().prop(ui, "editor_outline")

            subsplit = row.split(factor=0.85)

            padding = subsplit.split(factor=0.15)
            colsub = padding.column()
            colsub = padding.column()
            colsub.row().prop(ui, "menu_shadow_width")
            colsub.row().prop(ui, "widget_emboss")

            col.separator()
            col.separator()

            col.label(text="Axis & Gizmo Colors:")

            row = col.row()

            subsplit = row.split(factor=0.95)

            padding = subsplit.split(factor=0.15)
            colsub = padding.column()
            colsub = padding.column()
            colsub.row().prop(ui, "axis_x")
            colsub.row().prop(ui, "axis_y")
            colsub.row().prop(ui, "axis_z")

            subsplit = row.split(factor=0.85)

            padding = subsplit.split(factor=0.15)
            colsub = padding.column()
            colsub = padding.column()
            colsub.row().prop(ui, "gizmo_primary")
            colsub.row().prop(ui, "gizmo_secondary")
            colsub.row().prop(ui, "gizmo_a")
            colsub.row().prop(ui, "gizmo_b")

            col.separator()
            col.separator()

            col.label(text="Icon Colors:")

            row = col.row()

            subsplit = row.split(factor=0.95)

            padding = subsplit.split(factor=0.15)
            colsub = padding.column()
            colsub = padding.column()
            colsub.row().prop(ui, "icon_collection")
            colsub.row().prop(ui, "icon_object")
            colsub.row().prop(ui, "icon_object_data")

            subsplit = row.split(factor=0.85)

            padding = subsplit.split(factor=0.15)
            colsub = padding.column()
            colsub = padding.column()
            colsub.row().prop(ui, "icon_modifier")
            colsub.row().prop(ui, "icon_shading")

            col.separator()
            col.separator()
        elif theme.theme_area == 'BONE_COLOR_SETS':
            col = split.column()

            for i, ui in enumerate(theme.bone_color_sets, 1):
                col.label(text=iface_(f"Color Set {i:d}"), translate=False)

                row = col.row()

                subsplit = row.split(factor=0.95)

                padding = subsplit.split(factor=0.15)
                colsub = padding.column()
                colsub = padding.column()
                colsub.row().prop(ui, "normal")
                colsub.row().prop(ui, "select")
                colsub.row().prop(ui, "active")

                subsplit = row.split(factor=0.85)

                padding = subsplit.split(factor=0.15)
                colsub = padding.column()
                colsub = padding.column()
                colsub.row().prop(ui, "show_colored_constraints")
        elif theme.theme_area == 'STYLE':
            col = split.column()

            style = context.user_preferences.ui_styles[0]

            col.label(text="Panel Title:")
            self._ui_font_style(col, style.panel_title)

            col.separator()

            col.label(text="Widget:")
            self._ui_font_style(col, style.widget)

            col.separator()

            col.label(text="Widget Label:")
            self._ui_font_style(col, style.widget_label)
        else:
            self._theme_generic(split, getattr(theme, theme.theme_area.lower()), theme.theme_area)


class USERPREF_PT_file(Panel):
    bl_space_type = 'USER_PREFERENCES'
    bl_label = "Files"
    bl_region_type = 'WINDOW'
    bl_options = {'HIDE_HEADER'}

    @classmethod
    def poll(cls, context):
        userpref = context.user_preferences
        return (userpref.active_section == 'SYSTEM_FILES')

    def draw(self, context):
        layout = self.layout

        userpref = context.user_preferences
        paths = userpref.filepaths
        system = userpref.system

        split = layout.split(factor=0.7)

        col = split.column()
        col.label(text="File Paths:")

        colsplit = col.split(factor=0.95)
        col1 = colsplit.split(factor=0.3)

        sub = col1.column()
        sub.label(text="Fonts:")
        sub.label(text="Textures:")
        sub.label(text="Render Output:")
        sub.label(text="Scripts:")
        sub.label(text="Sounds:")
        sub.label(text="Temp:")
        sub.label(text="Render Cache:")
        sub.label(text="I18n Branches:")
        sub.label(text="Image Editor:")
        sub.label(text="Animation Player:")

        sub = col1.column()
        sub.prop(paths, "font_directory", text="")
        sub.prop(paths, "texture_directory", text="")
        sub.prop(paths, "render_output_directory", text="")
        sub.prop(paths, "script_directory", text="")
        sub.prop(paths, "sound_directory", text="")
        sub.prop(paths, "temporary_directory", text="")
        sub.prop(paths, "render_cache_directory", text="")
        sub.prop(paths, "i18n_branches_directory", text="")
        sub.prop(paths, "image_editor", text="")
        subsplit = sub.split(factor=0.3)
        subsplit.prop(paths, "animation_player_preset", text="")
        subsplit.prop(paths, "animation_player", text="")

        col.separator()
        col.separator()

        colsplit = col.split(factor=0.95)
        sub = colsplit.column()

        row = sub.split(factor=0.3)
        row.label(text="Auto Execution:")
        row.prop(system, "use_scripts_auto_execute")

        if system.use_scripts_auto_execute:
            box = sub.box()
            row = box.row()
            row.label(text="Excluded Paths:")
            row.operator("wm.userpref_autoexec_path_add", text="", icon='ADD', emboss=False)
            for i, path_cmp in enumerate(userpref.autoexec_paths):
                row = box.row()
                row.prop(path_cmp, "path", text="")
                row.prop(path_cmp, "use_glob", text="", icon='FILTER')
                row.operator("wm.userpref_autoexec_path_remove", text="", icon='X', emboss=False).index = i

        col = split.column()
        col.label(text="Save & Load:")
        col.prop(paths, "use_relative_paths")
        col.prop(paths, "use_file_compression")
        col.prop(paths, "use_load_ui")
        col.prop(paths, "use_filter_files")
        col.prop(paths, "show_hidden_files_datablocks")
        col.prop(paths, "hide_recent_locations")
        col.prop(paths, "hide_system_bookmarks")
        col.prop(paths, "show_thumbnails")

        col.separator()

        col.prop(paths, "save_version")
        col.prop(paths, "recent_files")
        col.prop(paths, "use_save_preview_images")

        col.separator()

        col.label(text="Auto Save:")
        col.prop(paths, "use_keep_session")
        col.prop(paths, "use_auto_save_temporary_files")
        sub = col.column()
        sub.active = paths.use_auto_save_temporary_files
        sub.prop(paths, "auto_save_time", text="Timer (mins)")

        col.separator()

        col.label(text="Text Editor:")
        col.prop(system, "use_tabs_as_spaces")

        colsplit = col.split(factor=0.95)
        col1 = colsplit.split(factor=0.3)

        sub = col1.column()
        sub.label(text="Author:")
        sub = col1.column()
        sub.prop(system, "author", text="")


class USERPREF_MT_ndof_settings(Menu):
    # accessed from the window key-bindings in C (only)
    bl_label = "3D Mouse Settings"

    def draw(self, context):
        layout = self.layout

        input_prefs = context.user_preferences.inputs

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


class USERPREF_MT_keyconfigs(Menu):
    bl_label = "KeyPresets"
    preset_subdir = "keyconfig"
    preset_operator = "wm.keyconfig_activate"

    def draw(self, context):
        Menu.draw_preset(self, context)


class USERPREF_PT_input(Panel):
    bl_space_type = 'USER_PREFERENCES'
    bl_label = "Input"
    bl_region_type = 'WINDOW'
    bl_options = {'HIDE_HEADER'}

    @classmethod
    def poll(cls, context):
        userpref = context.user_preferences
        return (userpref.active_section == 'INPUT')

    @staticmethod
    def draw_input_prefs(inputs, layout):
        import sys

        # General settings
        sub = layout.column()

        sub.label(text="Mouse:")
        sub.prop(inputs, "use_mouse_emulate_3_button")
        sub.prop(inputs, "use_mouse_continuous")
        sub.prop(inputs, "drag_threshold")
        sub.prop(inputs, "tweak_threshold")

        sub = layout.column()
        sub.label(text="Double Click:")
        sub.prop(inputs, "mouse_double_click_time", text="Speed")

        sub.separator()

        sub.prop(inputs, "use_emulate_numpad")

        sub.separator()

        sub.label(text="Orbit Style:")
        sub.row().prop(inputs, "view_rotate_method", expand=True)

        sub.separator()

        sub.label(text="Zoom Style:")
        sub.row().prop(inputs, "view_zoom_method", text="")
        if inputs.view_zoom_method in {'DOLLY', 'CONTINUE'}:
            sub.row().prop(inputs, "view_zoom_axis", expand=True)
            sub.prop(inputs, "invert_mouse_zoom", text="Invert Mouse Zoom Direction")

        #sub.prop(inputs, "use_mouse_mmb_paste")

        # layout.separator()

        sub = layout.column()
        sub.prop(inputs, "invert_zoom_wheel", text="Invert Wheel Zoom Direction")
        #sub.prop(view, "wheel_scroll_lines", text="Scroll Lines")

        if sys.platform == "darwin":
            sub = layout.column()
            sub.prop(inputs, "use_trackpad_natural", text="Natural Trackpad Direction")

        layout.separator()
        sub = layout.column()
        sub.label(text="View Navigation:")
        sub.row().prop(inputs, "navigation_mode", expand=True)

        sub.label(text="Walk Navigation:")

        walk = inputs.walk_navigation

        sub.prop(walk, "use_mouse_reverse")
        sub.prop(walk, "mouse_speed")
        sub.prop(walk, "teleport_time")

        sub = layout.column(align=True)
        sub.prop(walk, "walk_speed")
        sub.prop(walk, "walk_speed_factor")

        sub.separator()
        sub.prop(walk, "use_gravity")
        sub = layout.column(align=True)
        sub.active = walk.use_gravity
        sub.prop(walk, "view_height")
        sub.prop(walk, "jump_height")

        sub.separator()
        sub = layout.column()
        sub.label(text="Tablet Pressure:")
        sub.prop(inputs, "pressure_threshold_max")
        sub.prop(inputs, "pressure_softness")

        if inputs.use_ndof:
            layout.separator()
            layout.label(text="NDOF Device:")
            sub = layout.column(align=True)
            sub.prop(inputs, "ndof_sensitivity", text="Pan Sensitivity")
            sub.prop(inputs, "ndof_orbit_sensitivity", text="Orbit Sensitivity")
            sub.prop(inputs, "ndof_deadzone", text="Deadzone")

            sub.separator()
            layout.label(text="Navigation Style:")
            sub = layout.column(align=True)
            sub.row().prop(inputs, "ndof_view_navigate_method", expand=True)

            sub.separator()
            layout.label(text="Rotation Style:")
            sub = layout.column(align=True)
            sub.row().prop(inputs, "ndof_view_rotate_method", expand=True)

    def draw(self, context):
        from rna_keymap_ui import draw_keymaps

        layout = self.layout

        #import time

        #start = time.time()

        userpref = context.user_preferences

        inputs = userpref.inputs

        split = layout.split(factor=0.25)

        row = split.row()
        col = row.column()

        # Input settings
        self.draw_input_prefs(inputs, col)

        row.separator()

        # Keymap Settings
        col = split.column()
        draw_keymaps(context, col)

        #print("runtime", time.time() - start)


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
    bl_space_type = 'USER_PREFERENCES'
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
        userpref = context.user_preferences
        return (userpref.active_section == 'ADDONS')

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

        userpref = context.user_preferences
        used_ext = {ext.module for ext in userpref.addons}

        addon_user_dirs = tuple(
            p for p in (
                os.path.join(userpref.filepaths.script_directory, "addons"),
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

        split = layout.split(factor=0.2)
        col = split.column()
        col.prop(context.window_manager, "addon_search", text="", icon='VIEWZOOM')

        col.label(text="Supported Level")
        col.prop(context.window_manager, "addon_support", expand=True)

        col.label(text="Categories")
        col.prop(context.window_manager, "addon_filter", expand=True)

        col = split.column()

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
                        addon_preferences = userpref.addons[module_name].preferences
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


class StudioLightPanelMixin():
    bl_space_type = 'USER_PREFERENCES'
    bl_region_type = 'WINDOW'

    @classmethod
    def poll(cls, context):
        userpref = context.user_preferences
        return (userpref.active_section == 'LIGHTS')

    def _get_lights(self, userpref):
        return [light for light in userpref.studio_lights if light.is_user_defined and light.type == self.sl_type]

    def draw(self, context):
        layout = self.layout
        userpref = context.user_preferences
        lights = self._get_lights(userpref)

        self.draw_light_list(layout, lights)

    def draw_light_list(self, layout, lights):
        if lights:
            flow = layout.column_flow(columns=4)
            for studio_light in lights:
                self.draw_studio_light(flow, studio_light)
        else:
            layout.label(text="No custom {} configured".format(self.bl_label))

    def draw_studio_light(self, layout, studio_light):
        box = layout.box()
        row = box.row()

        row.template_icon(layout.icon(studio_light), scale=6.0)
        op = row.operator('wm.studiolight_uninstall', text="", icon='REMOVE')
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

    @classmethod
    def poll(cls, context):
        userpref = context.user_preferences
        return (userpref.active_section == 'LIGHTS')

    def opengl_light_buttons(self, layout, light):

        col = layout.column()
        col.active = light.use

        col.prop(light, "use", text="Use Light")
        col.prop(light, "diffuse_color", text="Diffuse")
        col.prop(light, "specular_color", text="Specular")
        col.prop(light, "smooth")
        col.prop(light, "direction")

    def draw(self, context):
        userpref = context.user_preferences
        lights = self._get_lights(userpref)
        layout = self.layout

        self.draw_light_list(layout, lights)


class USERPREF_PT_studiolight_lights_editor(Panel):
    bl_label = "Studio Lights Editor"
    bl_parent_id = "USERPREF_PT_studiolight_lights"
    bl_space_type = 'USER_PREFERENCES'
    bl_region_type = 'WINDOW'

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

        userpref = context.user_preferences
        system = userpref.system

        row = layout.row()
        row.prop(system, "edit_solid_light", toggle=True)
        row.operator('wm.studiolight_new', text="Save as Studio light", icon="FILE_TICK")

        layout.separator()

        layout.use_property_split = True
        column = layout.split()
        column.active = system.edit_solid_light

        light = system.solid_lights[0]
        colsplit = column.split(factor=0.85)
        self.opengl_light_buttons(colsplit, light)

        light = system.solid_lights[1]
        colsplit = column.split(factor=0.85)
        self.opengl_light_buttons(colsplit, light)

        light = system.solid_lights[2]
        self.opengl_light_buttons(column, light)

        layout.separator()

        layout.prop(system, "light_ambient")


classes = (
    USERPREF_HT_header,
    USERPREF_PT_navigation,
    USERPREF_PT_interface,
    USERPREF_PT_edit,
    USERPREF_PT_system_general,
    USERPREF_MT_interface_theme_presets,
    USERPREF_PT_theme,
    USERPREF_PT_file,
    USERPREF_MT_ndof_settings,
    USERPREF_MT_keyconfigs,
    USERPREF_PT_input,
    USERPREF_MT_addons_online_resources,
    USERPREF_PT_addons,
    USERPREF_PT_studiolight_lights,
    USERPREF_PT_studiolight_lights_editor,
    USERPREF_PT_studiolight_matcaps,
    USERPREF_PT_studiolight_world,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
