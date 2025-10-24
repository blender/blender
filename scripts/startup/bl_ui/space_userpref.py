# SPDX-FileCopyrightText: 2009-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy
from bpy.types import (
    Header,
    Menu,
    Panel,
    UIList,
)
from bpy.app.translations import (
    contexts as i18n_contexts,
    pgettext_iface as iface_,
    pgettext_rpt as rpt_,
)
from bl_ui.utils import PresetPanel


# -----------------------------------------------------------------------------
# Main Header

class USERPREF_HT_header(Header):
    bl_space_type = 'PREFERENCES'

    @staticmethod
    def draw_buttons(layout, context):
        prefs = context.preferences

        layout.operator_context = 'EXEC_AREA'

        if prefs.use_preferences_save and (not bpy.app.use_userpref_skip_save_on_exit):
            pass
        else:
            # Show '*' to let users know the preferences have been modified.
            # It is shown to the left so that it is visible when the sidebar is narrow,
            # and for consistency with unsaved files in the title bar.
            layout.operator(
                "wm.save_userpref",
                text=("* " if prefs.is_dirty else "") + iface_("Save Preferences"),
                translate=False,
            )

    def draw(self, context):
        layout = self.layout
        layout.operator_context = 'EXEC_AREA'

        layout.template_header()

        USERPREF_MT_editor_menus.draw_collapsible(context, layout)

        layout.separator_spacer()

        self.draw_buttons(layout, context)


# -----------------------------------------------------------------------------
# Main Navigation Bar

class USERPREF_PT_navigation_bar(Panel):
    bl_label = "Preferences Navigation"
    bl_space_type = 'PREFERENCES'
    bl_region_type = 'UI'
    bl_category = "Navigation"
    bl_options = {'HIDE_HEADER'}

    def draw(self, context):
        layout = self.layout

        prefs = context.preferences

        col = layout.column()

        col.scale_x = 1.3
        col.scale_y = 1.3
        col.prop(prefs, "active_section", expand=True)


class USERPREF_MT_editor_menus(Menu):
    bl_idname = "USERPREF_MT_editor_menus"
    bl_label = ""

    def draw(self, _context):
        layout = self.layout
        layout.menu("USERPREF_MT_view")
        layout.menu("USERPREF_MT_save_load", text="Preferences")


class USERPREF_MT_view(Menu):
    bl_label = "View"

    def draw(self, context):
        layout = self.layout
        view = context.space_data

        layout.prop(view, "show_region_ui")
        layout.separator()

        layout.menu("INFO_MT_area")


class USERPREF_MT_save_load(Menu):
    bl_label = "Save & Load"

    def draw(self, context):
        layout = self.layout

        prefs = context.preferences

        row = layout.row()
        row.active = not bpy.app.use_userpref_skip_save_on_exit
        row.prop(prefs, "use_preferences_save", text="Auto-Save Preferences")

        layout.separator()

        layout.operator_context = 'EXEC_AREA'
        if prefs.use_preferences_save:
            layout.operator("wm.save_userpref", text="Save Preferences")
        sub_revert = layout.column(align=True)
        # NOTE: regarding `factory_startup`. To correctly show the active state of this menu item,
        # the user preferences themselves would need to have a `factory_startup` state.
        # Since showing an active menu item whenever factory-startup is used is not such a problem, leave this as-is.
        sub_revert.active = prefs.is_dirty or bpy.app.factory_startup
        sub_revert.operator("wm.read_userpref", text="Revert to Saved Preferences")

        layout.operator_context = 'INVOKE_AREA'

        app_template = prefs.app_template
        if app_template:
            display_name = bpy.path.display_name(iface_(app_template))
            layout.operator("wm.read_factory_userpref", text="Load Factory Blender Preferences")
            props = layout.operator(
                "wm.read_factory_userpref",
                text=iface_("Load Factory {:s} Preferences").format(display_name),
                translate=False,
            )
            props.use_factory_startup_app_template_only = True
            del display_name
        else:
            layout.operator("wm.read_factory_userpref", text="Load Factory Preferences")


class USERPREF_PT_save_preferences(Panel):
    bl_label = "Save Preferences"
    bl_space_type = 'PREFERENCES'
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

        layout.menu("USERPREF_MT_save_load", text="", icon='COLLAPSEMENU')

        USERPREF_HT_header.draw_buttons(layout, context)


# -----------------------------------------------------------------------------
# Min-In Helpers

# Panel mix-in.
class CenterAlignMixIn:
    """
    Base class for panels to center align contents with some horizontal margin.
    Deriving classes need to implement a ``draw_centered(context, layout)`` function.
    """

    def draw(self, context):
        layout = self.layout
        width = context.region.width
        ui_scale = context.preferences.system.ui_scale
        # No horizontal margin if region is rather small.
        is_wide = width > (350 * ui_scale)

        layout.use_property_split = True
        layout.use_property_decorate = False  # No animation.

        row = layout.row()
        if is_wide:
            row.label()  # Needed so col below is centered.

        col = row.column()
        col.ui_units_x = 50

        # Implemented by sub-classes.
        self.draw_centered(context, col)

        if is_wide:
            row.label()  # Needed so col above is centered.


# -----------------------------------------------------------------------------
# Interface Panels

class InterfacePanel:
    bl_space_type = 'PREFERENCES'
    bl_region_type = 'WINDOW'
    bl_context = "interface"


class USERPREF_PT_interface_display(InterfacePanel, CenterAlignMixIn, Panel):
    bl_label = "Display"

    def draw_centered(self, context, layout):
        prefs = context.preferences
        view = prefs.view

        col = layout.column()

        col.prop(view, "ui_scale", text="Resolution Scale")
        col.prop(view, "ui_line_width", text="Line Width")
        col.prop(view, "show_splash", text="Splash Screen")
        col.prop(view, "show_developer_ui")

        col.separator()

        col = layout.column(heading="Tooltips", align=True)
        col.prop(view, "show_tooltips", text="User Tooltips")
        sub = col.column()
        sub.active = view.show_tooltips
        sub.prop(view, "show_tooltips_python")

        col.separator()

        col = layout.column(heading="Search", align=True)
        col.prop(prefs, "use_recent_searches", text="Sort by Most Recent")


class USERPREF_PT_interface_text(InterfacePanel, CenterAlignMixIn, Panel):
    bl_label = "Text Rendering"
    bl_options = {'DEFAULT_CLOSED'}

    def draw_centered(self, context, layout):
        prefs = context.preferences
        view = prefs.view

        flow = layout.grid_flow(row_major=False, columns=0, even_columns=True, even_rows=False, align=False)

        flow.prop(view, "use_text_antialiasing", text="Anti-Aliasing")
        sub = flow.column()
        sub.active = view.use_text_antialiasing
        sub.prop(view, "use_text_render_subpixelaa", text="Subpixel Anti-Aliasing")
        sub.prop(view, "text_hinting", text="Hinting")

        flow.prop(view, "font_path_ui")
        flow.prop(view, "font_path_ui_mono")


class USERPREF_PT_interface_translation(InterfacePanel, CenterAlignMixIn, Panel):
    bl_label = "Language"
    bl_translation_context = i18n_contexts.id_windowmanager

    @classmethod
    def poll(cls, _context):
        return bpy.app.build_options.international

    def draw_centered(self, context, layout):
        prefs = context.preferences
        view = prefs.view

        layout.prop(view, "language")

        col = layout.column(heading="Translate", heading_ctxt=i18n_contexts.editor_preferences)
        col.active = (bpy.app.translations.locale != "en_US")
        col.prop(view, "use_translate_tooltips", text="Tooltips")
        col.prop(view, "use_translate_interface", text="Interface")
        col.prop(view, "use_translate_reports", text="Reports")
        col.prop(view, "use_translate_new_dataname", text="New Data")


class USERPREF_PT_interface_accessibility(InterfacePanel, CenterAlignMixIn, Panel):
    bl_label = "Accessibility"
    bl_options = {'DEFAULT_CLOSED'}

    def draw_centered(self, context, layout):
        prefs = context.preferences
        view = prefs.view

        flow = layout.grid_flow(row_major=False, columns=0, even_columns=True, even_rows=False, align=False)

        flow.prop(view, "use_reduce_motion")


class USERPREF_PT_interface_editors(InterfacePanel, CenterAlignMixIn, Panel):
    bl_label = "Editors"

    def draw_centered(self, context, layout):
        prefs = context.preferences
        view = prefs.view
        system = prefs.system

        col = layout.column()
        col.prop(system, "use_region_overlap")

        col = layout.column(heading="Show", align=True)
        col.prop(view, "show_area_handle")
        col.prop(view, "show_number_arrows", text="Numeric Input Arrows")
        col.prop(view, "show_navigate_ui")

        col = layout.column()
        col.prop(view, "border_width")
        col.prop(view, "color_picker_type")
        col.row().prop(view, "header_align")
        col.prop(view, "factor_display_type")


class USERPREF_PT_interface_temporary_windows(InterfacePanel, CenterAlignMixIn, Panel):
    bl_label = "Temporary Editors"
    bl_parent_id = "USERPREF_PT_interface_editors"
    bl_options = {'DEFAULT_CLOSED'}

    def draw_centered(self, context, layout):
        prefs = context.preferences
        view = prefs.view

        col = layout.column()
        col.prop(view, "render_display_type", text="Render In")
        col.prop(view, "filebrowser_display_type", text="File Browser")
        col.prop(view, "preferences_display_type", text="Preferences")


class USERPREF_PT_interface_statusbar(InterfacePanel, CenterAlignMixIn, Panel):
    bl_label = "Status Bar"
    bl_parent_id = "USERPREF_PT_interface_editors"
    bl_options = {'DEFAULT_CLOSED'}

    def draw_centered(self, context, layout):
        prefs = context.preferences
        view = prefs.view

        col = layout.column(heading="Show")
        col.prop(view, "show_statusbar_stats", text="Scene Statistics")
        col.prop(view, "show_statusbar_scene_duration", text="Scene Duration")
        col.prop(view, "show_statusbar_memory", text="System Memory")
        col.prop(view, "show_statusbar_vram", text="Video Memory")
        col.prop(view, "show_extensions_updates", text="Extensions Updates")
        col.prop(view, "show_statusbar_version", text="Blender Version")


class USERPREF_PT_interface_menus(InterfacePanel, CenterAlignMixIn, Panel):
    bl_label = "Menus"
    bl_options = {'DEFAULT_CLOSED'}

    def draw_centered(self, context, layout):
        prefs = context.preferences
        view = prefs.view
        col = layout.column()
        col.prop(view, "menu_close_leave")


class USERPREF_PT_interface_menus_mouse_over(InterfacePanel, CenterAlignMixIn, Panel):
    bl_label = "Open on Mouse Over"
    bl_parent_id = "USERPREF_PT_interface_menus"

    def draw_header(self, context):
        prefs = context.preferences
        view = prefs.view

        self.layout.prop(view, "use_mouse_over_open", text="")

    def draw_centered(self, context, layout):
        prefs = context.preferences
        view = prefs.view

        layout.active = view.use_mouse_over_open

        flow = layout.grid_flow(row_major=False, columns=0, even_columns=True, even_rows=False, align=False)

        flow.prop(view, "open_toplevel_delay", text="Top Level")
        flow.prop(view, "open_sublevel_delay", text="Sub Level")


class USERPREF_PT_interface_menus_pie(InterfacePanel, CenterAlignMixIn, Panel):
    bl_label = "Pie Menus"
    bl_parent_id = "USERPREF_PT_interface_menus"

    def draw_centered(self, context, layout):
        prefs = context.preferences
        view = prefs.view

        flow = layout.grid_flow(row_major=False, columns=0, even_columns=True, even_rows=False, align=False)

        flow.prop(view, "pie_animation_timeout")
        flow.prop(view, "pie_tap_timeout")
        flow.prop(view, "pie_initial_timeout")
        flow.prop(view, "pie_menu_radius")
        flow.prop(view, "pie_menu_threshold")
        flow.prop(view, "pie_menu_confirm")


# -----------------------------------------------------------------------------
# Editing Panels

class EditingPanel:
    bl_space_type = 'PREFERENCES'
    bl_region_type = 'WINDOW'
    bl_context = "editing"


class USERPREF_PT_edit_objects(EditingPanel, Panel):
    bl_label = "Objects"

    def draw(self, context):
        pass


class USERPREF_PT_edit_objects_new(EditingPanel, CenterAlignMixIn, Panel):
    bl_label = "New Objects"
    bl_parent_id = "USERPREF_PT_edit_objects"

    def draw_centered(self, context, layout):
        prefs = context.preferences
        edit = prefs.edit

        flow = layout.grid_flow(row_major=False, columns=0, even_columns=True, even_rows=False, align=False)

        flow.prop(edit, "material_link", text="Link Materials To")
        flow.prop(edit, "object_align", text="Align To")
        flow.prop(edit, "use_enter_edit_mode", text="Enter Edit Mode")
        flow.prop(edit, "collection_instance_empty_size", text="Instance Empty Size")


class USERPREF_PT_edit_objects_duplicate_data(EditingPanel, CenterAlignMixIn, Panel):
    bl_label = "Copy on Duplicate"
    bl_parent_id = "USERPREF_PT_edit_objects"
    bl_options = {'DEFAULT_CLOSED'}

    def draw_centered(self, context, layout):
        prefs = context.preferences
        edit = prefs.edit

        layout.use_property_split = False

        flow = layout.grid_flow(row_major=False, columns=0, even_columns=True, even_rows=False, align=True)

        datablock_types = (
            ("use_duplicate_action", "Action", 'ACTION', ""),
            ("use_duplicate_armature", "Armature", 'OUTLINER_DATA_ARMATURE', ""),
            ("use_duplicate_camera", "Camera", 'OUTLINER_DATA_CAMERA', ""),
            ("use_duplicate_curve", "Curve", 'OUTLINER_DATA_CURVE', ""),
            ("use_duplicate_curves", "Curves", 'OUTLINER_DATA_CURVES', ""),
            ("use_duplicate_grease_pencil", "Grease Pencil", 'OUTLINER_OB_GREASEPENCIL', ""),
            ("use_duplicate_lattice", "Lattice", 'OUTLINER_DATA_LATTICE', ""),
            (None, None, None, None),
            ("use_duplicate_light", "Light", 'OUTLINER_DATA_LIGHT', ""),
            ("use_duplicate_lightprobe", "Light Probe", 'OUTLINER_DATA_LIGHTPROBE', ""),
            ("use_duplicate_material", "Material", 'MATERIAL_DATA', ""),
            ("use_duplicate_mesh", "Mesh", 'OUTLINER_DATA_MESH', ""),
            ("use_duplicate_metaball", "Metaball", 'OUTLINER_DATA_META', ""),
            ("use_duplicate_node_tree", "Node Tree", 'NODETREE', ""),
            ("use_duplicate_particle", "Particle", 'PARTICLES', ""),
            (None, None, None, None),
            ("use_duplicate_pointcloud", "Point Cloud", 'OUTLINER_DATA_POINTCLOUD', ""),
            ("use_duplicate_speaker", "Speaker", 'OUTLINER_DATA_SPEAKER', ""),
            ("use_duplicate_surface", "Surface", 'OUTLINER_DATA_SURFACE', ""),
            ("use_duplicate_text", "Text", 'OUTLINER_DATA_FONT', ""),
            ("use_duplicate_volume", "Volume", 'OUTLINER_DATA_VOLUME', "i18n_contexts.id_id"),
        )

        col = flow.column()

        for prop, type_name, type_icon, type_ctx in datablock_types:
            if prop is None:
                col = flow.column()
                continue

            row = col.row()

            row_checkbox = row.row()
            row_checkbox.prop(edit, prop, text="", text_ctxt=type_ctx)

            row_label = row.row()
            row_label.label(text=type_name, icon=type_icon)


class USERPREF_PT_edit_cursor(EditingPanel, CenterAlignMixIn, Panel):
    bl_label = "3D Cursor"

    def draw_centered(self, context, layout):
        prefs = context.preferences
        edit = prefs.edit

        col = layout.column(heading="Cursor")
        col.prop(edit, "use_mouse_depth_cursor", text="Surface Project")
        col.prop(edit, "use_cursor_lock_adjust", text="Lock Adjust")


class USERPREF_PT_edit_gpencil(EditingPanel, CenterAlignMixIn, Panel):
    bl_label = "Grease Pencil"
    bl_options = {'DEFAULT_CLOSED'}

    def draw_centered(self, context, layout):
        prefs = context.preferences
        edit = prefs.edit

        col = layout.column(heading="Distance")
        col.prop(edit, "grease_pencil_manhattan_distance", text="Manhattan")
        col.prop(edit, "grease_pencil_euclidean_distance", text="Euclidean")


class USERPREF_PT_edit_annotations(EditingPanel, CenterAlignMixIn, Panel):
    bl_label = "Annotations"

    def draw_centered(self, context, layout):
        prefs = context.preferences
        edit = prefs.edit

        col = layout.column()
        col.prop(edit, "grease_pencil_default_color", text="Default Color")
        col.prop(edit, "grease_pencil_eraser_radius", text="Eraser Radius")


class USERPREF_PT_edit_weight_paint(EditingPanel, CenterAlignMixIn, Panel):
    bl_label = "Weight Paint"
    bl_options = {'DEFAULT_CLOSED'}

    def draw_centered(self, context, layout):
        prefs = context.preferences
        view = prefs.view

        layout.use_property_split = False

        layout.prop(view, "use_weight_color_range", text="Custom Gradient")

        col = layout.column()
        col.active = view.use_weight_color_range
        col.template_color_ramp(view, "weight_color_range", expand=True)


class USERPREF_PT_edit_text_editor(EditingPanel, CenterAlignMixIn, Panel):
    bl_label = "Text Editor"
    bl_options = {'DEFAULT_CLOSED'}

    def draw_centered(self, context, layout):
        prefs = context.preferences
        edit = prefs.edit

        layout.prop(edit, "use_text_edit_auto_close")


class USERPREF_PT_edit_node_editor(EditingPanel, CenterAlignMixIn, Panel):
    bl_label = "Node Editor"
    bl_options = {'DEFAULT_CLOSED'}

    def draw_centered(self, context, layout):
        prefs = context.preferences
        edit = prefs.edit

        col = layout.column(heading="Auto-Offset")
        row = col.row()
        row.prop(edit, "node_use_insert_offset", text="")
        subrow = row.row()
        subrow.prop(edit, "node_margin", text="")
        subrow.active = edit.node_use_insert_offset

        layout.prop(edit, "node_preview_resolution", text="Preview Resolution")


class USERPREF_PT_edit_sequence_editor(EditingPanel, CenterAlignMixIn, Panel):
    bl_label = "Video Sequencer"
    bl_options = {'DEFAULT_CLOSED'}

    def draw_centered(self, context, layout):
        prefs = context.preferences
        edit = prefs.edit

        layout.prop(edit, "connect_strips_by_default")


class USERPREF_PT_edit_misc(EditingPanel, CenterAlignMixIn, Panel):
    bl_label = "Miscellaneous"
    bl_options = {'DEFAULT_CLOSED'}

    def draw_centered(self, context, layout):
        prefs = context.preferences
        edit = prefs.edit

        col = layout.column()
        col.prop(edit, "sculpt_paint_overlay_color", text="Sculpt Overlay Color")


# -----------------------------------------------------------------------------
# Animation Panels

class AnimationPanel:
    bl_space_type = 'PREFERENCES'
    bl_region_type = 'WINDOW'
    bl_context = "animation"


class USERPREF_PT_animation_timeline(AnimationPanel, CenterAlignMixIn, Panel):
    bl_label = "Timeline"

    def draw_centered(self, context, layout):
        prefs = context.preferences
        view = prefs.view
        edit = prefs.edit

        col = layout.column()
        col.prop(edit, "use_negative_frames")

        col.prop(view, "view2d_grid_spacing_min", text="Minimum Grid Spacing")
        col.prop(view, "timecode_style")
        col.prop(view, "view_frame_type")
        if view.view_frame_type == 'SECONDS':
            col.prop(view, "view_frame_seconds")
        elif view.view_frame_type == 'KEYFRAMES':
            col.prop(view, "view_frame_keyframes")


class USERPREF_PT_animation_keyframes(AnimationPanel, CenterAlignMixIn, Panel):
    bl_label = "Keyframes"

    def draw_centered(self, context, layout):
        prefs = context.preferences
        edit = prefs.edit

        layout.prop(edit, "key_insert_channels", expand=True)

        row = layout.row(align=True, heading="Only Insert Needed")
        row.prop(edit, "use_keyframe_insert_needed", text="Manual", toggle=1)
        row.prop(edit, "use_auto_keyframe_insert_needed", text="Auto", toggle=1)

        col = layout.column(heading="Keyframing")
        col.prop(edit, "use_visual_keying")

        col = layout.column(heading="Auto-Keyframing")
        col.prop(edit, "use_auto_keying", text="Enable in New Scenes")
        col.prop(edit, "use_auto_keying_warning", text="Show Warning")
        col.prop(edit, "use_keyframe_insert_available", text="Only Insert Available")


class USERPREF_PT_animation_fcurves(AnimationPanel, CenterAlignMixIn, Panel):
    bl_label = "F-Curves"

    def draw_centered(self, context, layout):
        prefs = context.preferences
        edit = prefs.edit

        flow = layout.grid_flow(row_major=False, columns=0, even_columns=True, even_rows=False, align=False)

        flow.prop(edit, "fcurve_unselected_alpha", text="Unselected Opacity")
        flow.prop(edit, "fcurve_new_auto_smoothing", text="Default Smoothing Mode")
        flow.prop(edit, "keyframe_new_interpolation_type", text="Default Interpolation")
        flow.prop(edit, "keyframe_new_handle_type", text="Default Handles")
        flow.prop(edit, "use_insertkey_xyz_to_rgb", text="XYZ to RGB")
        flow.prop(edit, "use_anim_channel_group_colors")
        flow.prop(edit, "show_only_selected_curve_keyframes")
        flow.prop(edit, "use_fcurve_high_quality_drawing")


# -----------------------------------------------------------------------------
# System Panels

class SystemPanel:
    bl_space_type = 'PREFERENCES'
    bl_region_type = 'WINDOW'
    bl_context = "system"


class USERPREF_PT_system_sound(SystemPanel, CenterAlignMixIn, Panel):
    bl_label = "Sound"
    bl_options = {'DEFAULT_CLOSED'}

    def draw_centered(self, context, layout):
        prefs = context.preferences
        system = prefs.system

        layout.prop(system, "audio_device", expand=False)

        sub = layout.grid_flow(row_major=False, columns=0, even_columns=False, even_rows=False, align=False)
        sub.active = system.audio_device not in {'NONE', 'None'}
        sub.prop(system, "audio_channels", text="Channels")
        sub.prop(system, "audio_mixing_buffer", text="Mixing Buffer")
        sub.prop(system, "audio_sample_rate", text="Sample Rate")
        sub.prop(system, "audio_sample_format", text="Sample Format")


class USERPREF_PT_system_cycles_devices(SystemPanel, CenterAlignMixIn, Panel):
    bl_label = "Cycles Render Devices"

    def draw_centered(self, context, layout):
        prefs = context.preferences

        col = layout.column()
        col.use_property_split = False

        if bpy.app.build_options.cycles:
            addon = prefs.addons.get("cycles")
            if addon is None:
                layout.label(text="Enable Cycles Render Engine add-on to use Cycles", icon='INFO')
            else:
                addon.preferences.draw_impl(col, context)
            del addon
        else:
            layout.label(text="Cycles is disabled in this build", icon='INFO')


class USERPREF_PT_system_display_graphics(SystemPanel, CenterAlignMixIn, Panel):
    bl_label = "Display Graphics"

    @classmethod
    def poll(cls, _context):
        import platform
        return platform.system() != "Darwin"

    def draw_centered(self, context, layout):
        prefs = context.preferences
        system = prefs.system
        import gpu
        import sys

        col = layout.column()
        col.prop(system, "gpu_backend", text="Backend")
        if system.gpu_backend == 'VULKAN':
            col = layout.column()
            col.enabled = gpu.platform.backend_type_get() == 'VULKAN'
            col.prop(system, "gpu_preferred_device")

        if system.gpu_backend != gpu.platform.backend_type_get():
            layout.label(text="A restart of Blender is required", icon='INFO')

        if system.gpu_backend == 'VULKAN':
            col = layout.column()
            col.label(text="Current Vulkan backend limitations:", icon='INFO')
            col.label(text="\u2022 Low performance in VR", icon='BLANK1')
            if sys.platform == "win32" and gpu.platform.device_type_get() == 'QUALCOMM':
                col.label(text="\u2022 Windows on ARM requires driver 31.0.112.0 or higher", icon='BLANK1')


class USERPREF_PT_system_os_settings(SystemPanel, CenterAlignMixIn, Panel):
    bl_label = "Operating System Settings"

    @classmethod
    def poll(cls, _context):
        # macOS isn't supported.
        from sys import platform
        if platform == "darwin":
            return False
        return True

    @staticmethod
    def _draw_associate_supported_or_label(context, layout):
        from sys import platform
        if platform[:3] == "win":
            if context.preferences.system.is_microsoft_store_install:
                layout.label(text="Microsoft Store installation")
                layout.label(text="Use Windows 'Default Apps' to associate with blend files")
                return False
        else:
            # Linux.
            if not bpy.app.portable:
                layout.label(text="System Installation")
                layout.label(text="File association is handled by the package manager")
                return False

            import os
            if os.environ.get("SNAP"):
                layout.label(text="Snap Package Installation")
                layout.label(text="File association is handled by the package manager")
                return False

        return True

    def draw_centered(self, context, layout):
        if self._draw_associate_supported_or_label(context, layout):
            layout.label(text="Open blend files with this Blender version")
            split = layout.split(factor=0.5)
            split.alignment = 'LEFT'
            split.operator("preferences.associate_blend", text="Register")
            split.operator("preferences.unassociate_blend", text="Unregister")
            layout.prop(bpy.context.preferences.system, "register_all_users", text="For All Users")


class USERPREF_PT_system_network(SystemPanel, CenterAlignMixIn, Panel):
    bl_label = "Network"

    def draw_centered(self, context, layout):
        prefs = context.preferences
        system = prefs.system

        row = layout.row()
        row.prop(system, "use_online_access", text="Allow Online Access")

        # Show when the preference has been overridden and doesn't match the current preference.
        runtime_online_access = bpy.app.online_access
        if system.use_online_access != runtime_online_access:
            row = layout.split(factor=0.4)
            row.label(text="")
            if runtime_online_access:
                text = iface_("Enabled on startup, overriding the preference.")
            else:
                text = iface_("Disabled on startup, overriding the preference.")
            row.label(text=text, translate=False)

        layout.row().prop(system, "network_timeout", text="Time Out")
        layout.row().prop(system, "network_connection_limit", text="Connection Limit")


class USERPREF_PT_system_memory(SystemPanel, CenterAlignMixIn, Panel):
    bl_label = "Memory & Limits"

    def draw_centered(self, context, layout):
        import sys

        prefs = context.preferences
        system = prefs.system
        edit = prefs.edit

        col = layout.column()
        col.prop(edit, "undo_steps", text="Undo Steps")
        col.prop(edit, "undo_memory_limit", text="Undo Memory Limit")
        col.prop(edit, "use_global_undo")

        layout.separator()

        col = layout.column()
        col.prop(system, "scrollback", text="Console Scrollback Lines")

        layout.separator()

        col = layout.column()
        col.prop(system, "texture_time_out", text="Texture Time Out")
        col.prop(system, "texture_collection_rate", text="Garbage Collection Rate")

        layout.separator()

        col = layout.column()
        col.prop(system, "vbo_time_out", text="VBO Time Out")
        col.prop(system, "vbo_collection_rate", text="Garbage Collection Rate")

        if sys.platform != "darwin":
            layout.separator()
            col = layout.column(align=True)
            col.active = system.gpu_backend != 'VULKAN'
            col.row().prop(system, "shader_compilation_method", expand=True)
            label = iface_("Threads") if system.shader_compilation_method == 'THREAD' else iface_("Subprocesses")
            col.prop(system, "gpu_shader_workers", text=label, translate=False)


class USERPREF_PT_system_video_sequencer(SystemPanel, CenterAlignMixIn, Panel):
    bl_label = "Video Sequencer"

    def draw_centered(self, context, layout):
        prefs = context.preferences
        system = prefs.system

        layout.prop(system, "memory_cache_limit")

        layout.separator()

        layout.prop(system, "sequencer_proxy_setup")


# -----------------------------------------------------------------------------
# Viewport Panels

class ViewportPanel:
    bl_space_type = 'PREFERENCES'
    bl_region_type = 'WINDOW'
    bl_context = "viewport"


class USERPREF_PT_viewport_display(ViewportPanel, CenterAlignMixIn, Panel):
    bl_label = "Display"

    def draw_centered(self, context, layout):
        prefs = context.preferences
        view = prefs.view

        col = layout.column(heading="Text Info Overlay")
        col.prop(view, "show_object_info", text="Object Info")
        col.prop(view, "show_view_name", text="View Name")

        col = layout.column(heading="Playback Frame Rate (FPS)")
        row = col.row()
        row.prop(view, "show_playback_fps", text="")
        subrow = row.row()
        subrow.active = view.show_playback_fps
        subrow.prop(view, "playback_fps_samples", text="Samples")

        layout.separator()

        col = layout.column()
        col.prop(view, "gizmo_size")
        col.prop(view, "lookdev_sphere_size")

        col.separator()

        col.prop(view, "mini_axis_type", text="3D Viewport Axes")

        if view.mini_axis_type == 'MINIMAL':
            col.prop(view, "mini_axis_size", text="Size")
            col.prop(view, "mini_axis_brightness", text="Brightness")

        if view.mini_axis_type == 'GIZMO':
            col.prop(view, "gizmo_size_navigate_v3d", text="Size")

        layout.separator()
        col = layout.column(heading="Fresnel")
        col.prop(view, "use_fresnel_edit")


class USERPREF_PT_viewport_quality(ViewportPanel, CenterAlignMixIn, Panel):
    bl_label = "Quality"

    def draw_centered(self, context, layout):
        prefs = context.preferences
        system = prefs.system

        col = layout.column()
        col.prop(system, "viewport_aa")

        col = layout.column(heading="Smooth Wires")
        col.prop(system, "use_overlay_smooth_wire", text="Overlay")
        col.prop(system, "use_edit_mode_smooth_wire", text="Edit Mode")


class USERPREF_PT_viewport_textures(ViewportPanel, CenterAlignMixIn, Panel):
    bl_label = "Textures"

    def draw_centered(self, context, layout):
        prefs = context.preferences
        system = prefs.system

        col = layout.column()
        col.prop(system, "gl_texture_limit", text="Limit Size")
        col.prop(system, "anisotropic_filter")
        col.prop(system, "gl_clip_alpha", slider=True)
        col.prop(system, "image_draw_method", text="Image Display Method")


class USERPREF_PT_viewport_subdivision(ViewportPanel, CenterAlignMixIn, Panel):
    bl_label = "Subdivision"
    bl_options = {'DEFAULT_CLOSED'}

    def draw_centered(self, context, layout):
        prefs = context.preferences
        system = prefs.system

        layout.prop(system, "use_gpu_subdivision")


# -----------------------------------------------------------------------------
# Theme Panels

class ThemePanel:
    bl_space_type = 'PREFERENCES'
    bl_region_type = 'WINDOW'
    bl_context = "themes"


class USERPREF_MT_interface_theme_presets(Menu):
    # NOTE: this label is currently not used, see: !134844.
    bl_label = "Presets"

    preset_subdir = "interface_theme"
    preset_operator = "script.execute_preset"
    preset_type = 'XML'
    preset_xml_map = (
        ("preferences.themes[0]", "Theme"),
        ("preferences.ui_styles[0]", "ThemeStyle"),
    )
    # Prevent untrusted XML files "escaping" from these types.
    preset_xml_secure_types = {
        "Theme",
        "ThemeBoneColorSet",
        "ThemeClipEditor",
        "ThemeCollectionColor",
        "ThemeCommon",
        "ThemeCommonAnim",
        "ThemeCommonCurves",
        "ThemeConsole",
        "ThemeDopeSheet",
        "ThemeFileBrowser",
        "ThemeFontStyle",
        "ThemeGradientColors",
        "ThemeGraphEditor",
        "ThemeImageEditor",
        "ThemeInfo",
        "ThemeNLAEditor",
        "ThemeNodeEditor",
        "ThemeOutliner",
        "ThemePreferences",
        "ThemeProperties",
        "ThemeRegions",
        "ThemeRegionsAssetShelf",
        "ThemeRegionsChannels",
        "ThemeRegionsScrubbing",
        "ThemeRegionsSidebars",
        "ThemeSequenceEditor",
        "ThemeSpaceGeneric",
        "ThemeSpaceGradient",
        "ThemeSpaceListGeneric",
        "ThemeSpreadsheet",
        "ThemeStatusBar",
        "ThemeStripColor",
        "ThemeStyle",
        "ThemeTextEditor",
        "ThemeTopBar",
        "ThemeUserInterface",
        "ThemeView3D",
        "ThemeWidgetColors",
        "ThemeWidgetStateColors",
    }

    draw = Menu.draw_preset

    @staticmethod
    def reset_cb(_context, _filepath):
        bpy.ops.preferences.reset_default_theme()

    @staticmethod
    def post_cb(context, filepath):
        context.preferences.themes[0].filepath = filepath


class USERPREF_PT_theme(ThemePanel, Panel):
    bl_label = "Themes"
    bl_options = {'HIDE_HEADER'}

    def draw(self, context):
        import os

        layout = self.layout

        split = layout.split(factor=0.6)

        row = split.row(align=True)

        # Unlike most presets (which use the classes bl_label),
        # themes store the path, use this when set.
        if filepath := context.preferences.themes[0].filepath:
            preset_label = bpy.path.display_name(os.path.basename(filepath))
        else:
            # If the `filepath` is empty, assume the theme was reset and use the default theme name as a label.
            # This would typically be:
            # `preset_label = USERPREF_MT_interface_theme_presets.bl_label`
            # However the operator to reset the preferences doesn't clear the value,
            # so it's simplest to hard-code "Presets" here.
            preset_label = "Presets"

        row.menu("USERPREF_MT_interface_theme_presets", text=preset_label)
        del filepath, preset_label

        row.operator("wm.interface_theme_preset_add", text="", icon='ADD')
        row.operator("wm.interface_theme_preset_remove", text="", icon='REMOVE')
        row.operator("wm.interface_theme_preset_save", text="", icon='FILE_TICK')

        row = split.row(align=True)
        row.operator("preferences.theme_install", text="Install...", icon='IMPORT')
        row.operator("preferences.reset_default_theme", text="Reset", icon='LOOP_BACK')


class USERPREF_PT_theme_user_interface(ThemePanel, CenterAlignMixIn, Panel):
    bl_label = "User Interface"
    bl_options = {'DEFAULT_CLOSED'}

    def draw_header(self, _context):
        layout = self.layout

        layout.label(icon='WORKSPACE')

    def draw(self, context):
        pass


# Base class for dynamically defined widget color panels.
# This is not registered.
class PreferenceThemeWidgetColorPanel:
    bl_parent_id = "USERPREF_PT_theme_user_interface"

    def draw(self, context):
        theme = context.preferences.themes[0]
        ui = theme.user_interface
        widget_style = getattr(ui, self.wcol)
        layout = self.layout

        layout.use_property_split = True

        flow = layout.grid_flow(row_major=False, columns=0, even_columns=True, even_rows=False, align=False)

        col = flow.column(align=True)
        col.prop(widget_style, "text")
        col.prop(widget_style, "text_sel", text="Selected")
        col.prop(widget_style, "item", slider=True)

        col = flow.column(align=True)
        col.prop(widget_style, "inner", slider=True)
        col.prop(widget_style, "inner_sel", text="Selected", slider=True)

        col = flow.column(align=True)
        col.prop(widget_style, "outline")
        col.prop(widget_style, "outline_sel", text="Selected", slider=True)

        col.separator()

        col.prop(widget_style, "roundness")


# Base class for dynamically defined widget color panels.
# This is not registered.
class PreferenceThemeWidgetShadePanel:

    def draw(self, context):
        theme = context.preferences.themes[0]
        ui = theme.user_interface
        widget_style = getattr(ui, self.wcol)
        layout = self.layout

        layout.use_property_split = True

        col = layout.column(align=True)
        col.active = widget_style.show_shaded
        col.prop(widget_style, "shadetop", text="Shade Top")
        col.prop(widget_style, "shadedown", text="Down")

    def draw_header(self, context):
        theme = context.preferences.themes[0]
        ui = theme.user_interface
        widget_style = getattr(ui, self.wcol)

        self.layout.prop(widget_style, "show_shaded", text="")


class USERPREF_PT_theme_interface_panel(ThemePanel, CenterAlignMixIn, Panel):
    bl_label = "Panel"
    bl_options = {'DEFAULT_CLOSED'}
    bl_parent_id = "USERPREF_PT_theme_user_interface"

    def draw_centered(self, context, layout):
        theme = context.preferences.themes[0]
        ui = theme.user_interface

        flow = layout.grid_flow(row_major=False, columns=2, even_columns=True, even_rows=False, align=False)

        col = flow.column()
        col.prop(ui, "panel_header", text="Header")

        col = col.column(align=True)
        col.prop(ui, "panel_back", text="Background")
        col.prop(ui, "panel_sub_back", text="Sub-Panel")

        col = col.column()
        col.prop(ui, "panel_active", text="Active")

        col = flow.column(align=True)
        col.prop(ui, "panel_title", text="Title")
        col.prop(ui, "panel_text", text="Text")

        col = col.column()
        col.prop(ui, "panel_outline", text="Outline")
        col.prop(ui, "panel_roundness", text="Roundness")


class USERPREF_PT_theme_interface_state(ThemePanel, CenterAlignMixIn, Panel):
    bl_label = "State"
    bl_options = {'DEFAULT_CLOSED'}
    bl_parent_id = "USERPREF_PT_theme_user_interface"

    def draw_centered(self, context, layout):
        theme = context.preferences.themes[0]
        ui_state = theme.user_interface.wcol_state

        flow = layout.grid_flow(row_major=False, columns=0, even_columns=True, even_rows=False, align=False)

        col = flow.column(align=True)

        col.prop(ui_state, "error")
        col.prop(ui_state, "warning")
        col.prop(ui_state, "info")
        col.prop(ui_state, "success")

        col = flow.column(align=True)
        col.prop(ui_state, "inner_anim")
        col.prop(ui_state, "inner_anim_sel", text="Selected")

        col = flow.column(align=True)
        col.prop(ui_state, "inner_driven")
        col.prop(ui_state, "inner_driven_sel", text="Selected")

        col = flow.column(align=True)
        col.prop(ui_state, "inner_key")
        col.prop(ui_state, "inner_key_sel", text="Selected")

        col = flow.column(align=True)
        col.prop(ui_state, "inner_overridden")
        col.prop(ui_state, "inner_overridden_sel", text="Selected")

        col = flow.column(align=True)
        col.prop(ui_state, "inner_changed")
        col.prop(ui_state, "inner_changed_sel", text="Selected")

        col = flow.column(align=True)
        col.prop(ui_state, "blend")


class USERPREF_PT_theme_interface_styles(ThemePanel, CenterAlignMixIn, Panel):
    bl_label = "Styles"
    bl_options = {'DEFAULT_CLOSED'}
    bl_parent_id = "USERPREF_PT_theme_user_interface"

    def draw_centered(self, context, layout):
        theme = context.preferences.themes[0]
        ui = theme.user_interface

        flow = layout.grid_flow(row_major=False, columns=0, even_columns=True, even_rows=False, align=False)

        col = flow.column(align=True)
        col.prop(ui, "editor_border")
        col.prop(ui, "editor_outline")
        col.prop(ui, "editor_outline_active")

        col = flow.column()
        col.prop(ui, "widget_text_cursor")

        col = flow.column(align=True)
        col.prop(ui, "icon_alpha")
        col.prop(ui, "icon_saturation", text="Saturation")

        flow.separator()

        col = flow.column()
        col.prop(ui, "widget_emboss")

        col = flow.column(align=True)
        col.prop(ui, "menu_shadow_fac")
        col.prop(ui, "menu_shadow_width", text="Shadow Width")


class USERPREF_PT_theme_interface_transparent_checker(ThemePanel, CenterAlignMixIn, Panel):
    bl_label = "Transparent Checkerboard"
    bl_options = {'DEFAULT_CLOSED'}
    bl_parent_id = "USERPREF_PT_theme_user_interface"

    def draw_centered(self, context, layout):
        theme = context.preferences.themes[0]
        ui = theme.user_interface

        flow = layout.grid_flow(row_major=False, columns=0, even_columns=True, even_rows=False, align=False)

        col = flow.column(align=True)
        col.prop(ui, "transparent_checker_primary")
        col.prop(ui, "transparent_checker_secondary")

        col = flow.column()
        col.prop(ui, "transparent_checker_size")


class USERPREF_PT_theme_interface_gizmos(ThemePanel, CenterAlignMixIn, Panel):
    bl_label = "Axis & Gizmo Colors"
    bl_options = {'DEFAULT_CLOSED'}
    bl_parent_id = "USERPREF_PT_theme_user_interface"

    def draw_centered(self, context, layout):
        theme = context.preferences.themes[0]
        ui = theme.user_interface

        flow = layout.grid_flow(row_major=False, columns=0, even_columns=True, even_rows=True, align=False)

        col = flow.column(align=True)
        col.prop(ui, "axis_x", text="Axis X")
        col.prop(ui, "axis_y", text="Y")
        col.prop(ui, "axis_z", text="Z")
        col.prop(ui, "axis_w", text="W")

        col = flow.column()
        col.prop(ui, "gizmo_primary")
        col.prop(ui, "gizmo_secondary", text="Secondary")
        col.prop(ui, "gizmo_view_align", text="View Align")

        col = flow.column()
        col.prop(ui, "gizmo_a")
        col.prop(ui, "gizmo_b", text="B")


class USERPREF_PT_theme_interface_icons(ThemePanel, CenterAlignMixIn, Panel):
    bl_label = "Icon Colors"
    bl_options = {'DEFAULT_CLOSED'}
    bl_parent_id = "USERPREF_PT_theme_user_interface"

    def draw_centered(self, context, layout):
        theme = context.preferences.themes[0]
        ui = theme.user_interface

        flow = layout.grid_flow(row_major=False, columns=0, even_columns=True, even_rows=False, align=False)

        flow.prop(ui, "icon_scene")
        flow.prop(ui, "icon_collection")
        flow.prop(ui, "icon_object")
        flow.prop(ui, "icon_object_data")
        flow.prop(ui, "icon_modifier")
        flow.prop(ui, "icon_shading")
        flow.prop(ui, "icon_folder")
        flow.prop(ui, "icon_autokey")
        flow.prop(ui, "icon_border_intensity")


class USERPREF_PT_theme_text_style(ThemePanel, CenterAlignMixIn, Panel):
    bl_label = "Text Style"
    bl_options = {'DEFAULT_CLOSED'}

    @staticmethod
    def _ui_font_style(layout, font_style):
        layout.use_property_split = True
        flow = layout.grid_flow(row_major=False, columns=0, even_columns=True, even_rows=False, align=False)

        col = flow.column()
        col.prop(font_style, "points")
        col.prop(font_style, "character_weight", text="Weight", text_ctxt=i18n_contexts.id_text)

        col = flow.column(align=True)
        col.prop(font_style, "shadow_offset_x", text="Shadow Offset X")
        col.prop(font_style, "shadow_offset_y", text="Y")

        col = flow.column(align=True)
        col.prop(font_style, "shadow")
        col.prop(font_style, "shadow_alpha", text="Alpha")
        col.prop(font_style, "shadow_value", text="Brightness")

    def draw_header(self, _context):
        layout = self.layout

        layout.label(icon='FONTPREVIEW')

    def draw_centered(self, context, layout):
        style = context.preferences.ui_styles[0]

        layout.label(text="Panel Title")
        self._ui_font_style(layout, style.panel_title)

        layout.separator()

        layout.label(text="Widget")
        self._ui_font_style(layout, style.widget)

        layout.separator()

        layout.label(text="Tooltip")
        self._ui_font_style(layout, style.tooltip)


class USERPREF_PT_theme_bone_color_sets(ThemePanel, CenterAlignMixIn, Panel):
    bl_label = "Bone Color Sets"
    bl_options = {'DEFAULT_CLOSED'}

    def draw_header(self, _context):
        layout = self.layout

        layout.label(icon='COLOR')

    def draw_centered(self, context, layout):
        theme = context.preferences.themes[0]

        layout.use_property_split = True

        for i, ui in enumerate(theme.bone_color_sets, 1):
            layout.label(text=iface_("Color Set {:d}").format(i), translate=False)

            flow = layout.grid_flow(row_major=False, columns=0, even_columns=True, even_rows=False, align=True)

            flow.prop(ui, "normal")
            flow.prop(ui, "select", text="Selected")
            flow.prop(ui, "active")
            flow.prop(ui, "show_colored_constraints")


class USERPREF_PT_theme_collection_colors(ThemePanel, CenterAlignMixIn, Panel):
    bl_label = "Collection Colors"
    bl_options = {'DEFAULT_CLOSED'}

    def draw_header(self, _context):
        layout = self.layout

        layout.label(icon='GROUP')

    def draw_centered(self, context, layout):
        theme = context.preferences.themes[0]

        layout.use_property_split = True

        flow = layout.grid_flow(row_major=False, columns=2, even_columns=True, even_rows=False, align=False)
        for i, ui in enumerate(theme.collection_color, 1):
            flow.prop(ui, "color", text=iface_("Color {:d}").format(i), translate=False)


class USERPREF_PT_theme_strip_colors(ThemePanel, CenterAlignMixIn, Panel):
    bl_label = "Strip Color Tags"
    bl_options = {'DEFAULT_CLOSED'}

    def draw_header(self, _context):
        layout = self.layout

        layout.label(icon='SEQ_STRIP_DUPLICATE')

    def draw_centered(self, context, layout):
        theme = context.preferences.themes[0]

        layout.use_property_split = True

        flow = layout.grid_flow(row_major=False, columns=2, even_columns=True, even_rows=False, align=False)
        for i, ui in enumerate(theme.strip_color, 1):
            flow.prop(ui, "color", text=iface_("Color {:d}").format(i), translate=False)


# Base class for dynamically defined theme-space panels.
# This is not registered.
class PreferenceThemeSpacePanel:
    @staticmethod
    def _theme_generic(layout, themedata):

        layout.use_property_split = True

        flow = layout.grid_flow(row_major=False, columns=0, even_columns=True, even_rows=False, align=False)

        props_type = {}

        for prop in themedata.rna_type.properties:
            if prop.identifier == "rna_type":
                continue

            props_type.setdefault((prop.type, prop.subtype), []).append(prop)

        for props_type, props_ls in sorted(props_type.items()):
            if props_type[0] == 'POINTER':
                continue

            for prop in props_ls:
                flow.prop(themedata, prop.identifier)

    def draw_header(self, _context):
        icon = getattr(self, "icon", 'NONE')
        if icon != 'NONE':
            layout = self.layout
            layout.label(icon=icon)

    def draw(self, context):
        layout = self.layout
        theme = context.preferences.themes[0]

        datapath_list = self.datapath.split(".")
        data = theme
        for datapath_item in datapath_list:
            data = getattr(data, datapath_item)
        PreferenceThemeSpacePanel._theme_generic(layout, data)


class ThemeGenericClassGenerator:

    @staticmethod
    def generate_panel_classes_for_wcols():
        wcols = [
            ("Box", "wcol_box"),
            ("Curve", "wcol_curve"),
            ("List Item", "wcol_list_item"),
            ("Menu", "wcol_menu"),
            ("Menu Background", "wcol_menu_back"),
            ("Menu Item", "wcol_menu_item"),
            ("Number Field", "wcol_num"),
            ("Option", "wcol_option"),
            ("Pie Menu", "wcol_pie_menu"),
            ("Progress Bar", "wcol_progress"),
            ("Pulldown", "wcol_pulldown"),
            ("Radio Buttons", "wcol_radio"),
            ("Regular", "wcol_regular"),
            ("Scroll Bar", "wcol_scroll"),
            ("Tab", "wcol_tab"),
            ("Text", "wcol_text"),
            ("Toggle", "wcol_toggle"),
            ("Tool", "wcol_tool"),
            ("Toolbar Item", "wcol_toolbar_item"),
            ("Tooltip", "wcol_tooltip"),
            ("Value Slider", "wcol_numslider"),
        ]

        for (name, wcol) in wcols:
            panel_id = "USERPREF_PT_theme_interface_" + wcol
            yield type(panel_id, (PreferenceThemeWidgetColorPanel, ThemePanel, Panel), {
                "bl_label": name,
                "bl_options": {'DEFAULT_CLOSED'},
                "draw": PreferenceThemeWidgetColorPanel.draw,
                "wcol": wcol,
            })

            panel_shade_id = "USERPREF_PT_theme_interface_shade_" + wcol
            yield type(panel_shade_id, (PreferenceThemeWidgetShadePanel, ThemePanel, Panel), {
                "bl_label": "Shaded",
                "bl_options": {'DEFAULT_CLOSED'},
                "bl_parent_id": panel_id,
                "draw": PreferenceThemeWidgetShadePanel.draw,
                "wcol": wcol,
            })

    @staticmethod
    def generate_theme_area_child_panel_classes(parent_id, rna_type, theme_area, datapath):
        def generate_child_panel_classes_recurse(parent_id, rna_type, theme_area, datapath):
            props_type = {}

            for prop in rna_type.properties:
                if prop.identifier == "rna_type":
                    continue

                props_type.setdefault((prop.type, prop.subtype), []).append(prop)

            for props_type, props_ls in sorted(props_type.items()):
                if props_type[0] == 'POINTER':
                    for prop in props_ls:
                        new_datapath = datapath + "." + prop.identifier if datapath else prop.identifier
                        panel_id = parent_id + "_" + prop.identifier
                        yield type(panel_id, (PreferenceThemeSpacePanel, ThemePanel, Panel), {
                            "bl_label": rna_type.properties[prop.identifier].name,
                            "bl_parent_id": parent_id,
                            "bl_options": {'DEFAULT_CLOSED'},
                            "draw": PreferenceThemeSpacePanel.draw,
                            "theme_area": theme_area.identifier,
                            "datapath": new_datapath,
                        })

                        yield from generate_child_panel_classes_recurse(
                            panel_id,
                            prop.fixed_type,
                            theme_area,
                            new_datapath,
                        )

        yield from generate_child_panel_classes_recurse(parent_id, rna_type, theme_area, datapath)

    @staticmethod
    def generate_panel_classes_from_theme_areas():
        from bpy.types import Theme

        for theme_area in Theme.bl_rna.properties["theme_area"].enum_items_static:
            if theme_area.identifier in {'USER_INTERFACE', 'STYLE', 'BONE_COLOR_SETS'}:
                continue

            panel_id = "USERPREF_PT_theme_" + theme_area.identifier.lower()
            # Generate panel-class from theme_area
            yield type(panel_id, (PreferenceThemeSpacePanel, ThemePanel, Panel), {
                "bl_label": theme_area.name,
                "bl_options": {'DEFAULT_CLOSED'},
                "draw_header": PreferenceThemeSpacePanel.draw_header,
                "draw": PreferenceThemeSpacePanel.draw,
                "theme_area": theme_area.identifier,
                "icon": theme_area.icon,
                "datapath": theme_area.identifier.lower(),
            })

            yield from ThemeGenericClassGenerator.generate_theme_area_child_panel_classes(
                panel_id, Theme.bl_rna.properties[theme_area.identifier.lower()].fixed_type,
                theme_area, theme_area.identifier.lower())


# -----------------------------------------------------------------------------
# File Paths Panels

# Panel mix-in.
class FilePathsPanel:
    bl_space_type = 'PREFERENCES'
    bl_region_type = 'WINDOW'
    bl_context = "file_paths"


class USERPREF_PT_file_paths_data(FilePathsPanel, Panel):
    bl_label = "Data"

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        paths = context.preferences.filepaths

        col = self.layout.column()
        col.prop(paths, "font_directory", text="Fonts")
        col.prop(paths, "texture_directory", text="Textures")
        col.prop(paths, "sound_directory", text="Sounds")
        col.prop(paths, "temporary_directory", text="Temporary Files")


class USERPREF_PT_file_paths_script_directories(FilePathsPanel, Panel):
    bl_label = "Script Directories"

    def draw(self, context):
        layout = self.layout

        paths = context.preferences.filepaths

        if len(paths.script_directories) == 0:
            layout.operator("preferences.script_directory_add", text="Add", icon='ADD')
            return

        layout.use_property_split = False
        layout.use_property_decorate = False

        box = layout.box()
        split = box.split(factor=0.35)
        name_col = split.column()
        path_col = split.column()

        row = name_col.row(align=True)  # Padding
        row.separator()
        row.label(text="Name")

        row = path_col.row(align=True)  # Padding
        row.separator()
        row.label(text="Path", text_ctxt=i18n_contexts.editor_filebrowser)

        row.operator("preferences.script_directory_add", text="", icon='ADD', emboss=False)

        for i, script_directory in enumerate(paths.script_directories):
            row = name_col.row()
            row.alert = not script_directory.name
            row.prop(script_directory, "name", text="")

            row = path_col.row()
            subrow = row.row()
            subrow.alert = not script_directory.directory
            subrow.prop(script_directory, "directory", text="")
            row.operator("preferences.script_directory_remove", text="", icon='X', emboss=False).index = i


class USERPREF_PT_file_paths_render(FilePathsPanel, Panel):
    bl_label = "Render"
    bl_parent_id = "USERPREF_PT_file_paths_data"

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        paths = context.preferences.filepaths

        col = self.layout.column()
        col.prop(paths, "render_output_directory", text="Render Output")
        col.prop(paths, "render_cache_directory", text="Render Cache")


class USERPREF_PT_text_editor_presets(PresetPanel, Panel):
    bl_label = "Text Editor Presets"
    preset_subdir = "text_editor"
    preset_operator = "script.execute_preset"
    preset_add_operator = "text_editor.preset_add"


class USERPREF_PT_file_paths_applications(FilePathsPanel, Panel):
    bl_label = "Applications"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        paths = context.preferences.filepaths

        col = layout.column()
        col.prop(paths, "image_editor", text="Image Editor")
        col.prop(paths, "animation_player_preset", text="Animation Player")
        if paths.animation_player_preset == 'CUSTOM':
            col.prop(paths, "animation_player", text="Player")


class USERPREF_PT_text_editor(FilePathsPanel, Panel):
    bl_label = "Text Editor"
    bl_parent_id = "USERPREF_PT_file_paths_applications"

    def draw_header_preset(self, _context):
        USERPREF_PT_text_editor_presets.draw_panel_header(self.layout)

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        paths = context.preferences.filepaths

        col = layout.column()
        col.prop(paths, "text_editor", text="Program")
        col.prop(paths, "text_editor_args", text="Arguments")


class USERPREF_PT_file_paths_development(FilePathsPanel, Panel):
    bl_label = "Development"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        prefs = context.preferences
        return prefs.view.show_developer_ui

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        paths = context.preferences.filepaths
        layout.prop(paths, "i18n_branches_directory", text="I18n Branches")


class USERPREF_PT_saveload_autorun(FilePathsPanel, Panel):
    bl_label = "Auto Run Python Scripts"
    bl_parent_id = "USERPREF_PT_saveload_blend"

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
        row.label(text="Excluded Paths")
        row.operator("preferences.autoexec_path_add", text="", icon='ADD', emboss=False)
        for i, path_cmp in enumerate(prefs.autoexec_paths):
            row = box.row()
            row.prop(path_cmp, "path", text="")
            row.prop(path_cmp, "use_glob", text="", icon='FILTER')
            row.operator("preferences.autoexec_path_remove", text="", icon='X', emboss=False).index = i


class USERPREF_UL_extension_repos(UIList):
    def draw_item(self, _context, layout, _data, item, icon, _active_data, _active_propname, _index):
        repo = item
        icon = 'INTERNET' if repo.use_remote_url else 'DISK_DRIVE'
        layout.prop(repo, "name", text="", icon=icon, emboss=False)

        # Show an error icon if this repository has unusable settings.
        if repo.enabled:
            if (
                    (repo.use_custom_directory and repo.custom_directory == "") or
                    (repo.use_remote_url and repo.remote_url == "")
            ):
                layout.label(text="", icon='ERROR')

        layout.prop(repo, "enabled", text="", emboss=False, icon='CHECKBOX_HLT' if repo.enabled else 'CHECKBOX_DEHLT')

    def filter_items(self, _context, data, propname):
        # Repositories has no index, converting to a list.
        items = list(getattr(data, propname))

        flags = [self.bitflag_filter_item] * len(items)

        indices = [None] * len(items)
        for index, orig_index in enumerate(sorted(
            range(len(items)),
            key=lambda i: (
                # Order [Remote, User, System].
                0 if (repo := items[i]).use_remote_url else (1 if (repo.source != 'SYSTEM') else 2),
                repo.name.casefold(),
            )
        )):
            indices[orig_index] = index

        return flags, indices


# -----------------------------------------------------------------------------
# Save/Load Panels

class SaveLoadPanel:
    bl_space_type = 'PREFERENCES'
    bl_region_type = 'WINDOW'
    bl_context = "save_load"


class USERPREF_PT_saveload_blend(SaveLoadPanel, CenterAlignMixIn, Panel):
    bl_label = "Blend Files"

    def draw_centered(self, context, layout):
        prefs = context.preferences
        paths = prefs.filepaths
        view = prefs.view

        col = layout.column(heading="Save")
        col.prop(view, "use_save_prompt")

        col = layout.column()
        col.prop(paths, "save_version")
        col.prop(paths, "recent_files")

        layout.separator()

        col = layout.column(heading="Auto-Save")
        row = col.row()
        row.prop(paths, "use_auto_save_temporary_files", text="")
        subrow = row.row()
        subrow.active = paths.use_auto_save_temporary_files
        subrow.prop(paths, "auto_save_time", text="Timer (Minutes)")

        layout.separator()

        layout.prop(paths, "file_preview_type")

        layout.separator()

        layout.separator()

        col = layout.column(heading="Default To")
        col.prop(paths, "use_relative_paths")
        col.prop(paths, "use_file_compression")
        col.prop(paths, "use_load_ui")

        col = layout.column(heading="Text Files")
        col.prop(paths, "use_tabs_as_spaces")


class USERPREF_PT_saveload_file_browser(SaveLoadPanel, CenterAlignMixIn, Panel):
    bl_label = "File Browser"

    def draw_centered(self, context, layout):
        prefs = context.preferences
        paths = prefs.filepaths

        col = layout.column(heading="Show Locations")
        col.prop(paths, "show_recent_locations", text="Recent")
        col.prop(paths, "show_system_bookmarks", text="System")

        col = layout.column(heading="Defaults")
        col.prop(paths, "use_filter_files")
        col.prop(paths, "show_hidden_files_datablocks")


# -----------------------------------------------------------------------------
# Input Panels

class InputPanel:
    bl_space_type = 'PREFERENCES'
    bl_region_type = 'WINDOW'
    bl_context = "input"


class USERPREF_PT_input_keyboard(InputPanel, CenterAlignMixIn, Panel):
    bl_label = "Keyboard"

    def draw_centered(self, context, layout):
        prefs = context.preferences
        inputs = prefs.inputs

        layout.prop(inputs, "use_emulate_numpad")
        layout.prop(inputs, "use_numeric_input_advanced")


class USERPREF_PT_input_mouse(InputPanel, CenterAlignMixIn, Panel):
    bl_label = "Mouse"

    def draw_centered(self, context, layout):
        import sys
        prefs = context.preferences
        inputs = prefs.inputs

        flow = layout.grid_flow(row_major=False, columns=0, even_columns=True, even_rows=False, align=False)

        if sys.platform[:3] == "win":
            flow.prop(inputs, "use_mouse_emulate_3_button")
        else:
            col = flow.column(heading="Emulate 3 Button Mouse")
            row = col.row()
            row.prop(inputs, "use_mouse_emulate_3_button", text="")
            subrow = row.row()
            subrow.prop(inputs, "mouse_emulate_3_button_modifier", text="")
            subrow.active = inputs.use_mouse_emulate_3_button

        flow.prop(inputs, "use_mouse_continuous")
        flow.prop(inputs, "use_drag_immediately")
        flow.prop(inputs, "mouse_double_click_time", text="Double Click Speed")
        flow.prop(inputs, "drag_threshold_mouse")
        flow.prop(inputs, "drag_threshold_tablet")
        flow.prop(inputs, "drag_threshold")
        flow.prop(inputs, "move_threshold")


class USERPREF_PT_input_touchpad(InputPanel, CenterAlignMixIn, Panel):
    bl_label = "Touchpad"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        import sys
        if sys.platform[:3] == "win" or sys.platform == "darwin":
            return True

        # WAYLAND supports multi-touch, X11 and SDL don't.
        from _bpy import _ghost_backend
        if _ghost_backend() == 'WAYLAND':
            return True

        return False

    def draw_centered(self, context, layout):
        prefs = context.preferences
        inputs = prefs.inputs

        col = layout.column()
        col.prop(inputs, "use_multitouch_gestures")

        from _bpy import _wm_capabilities
        capabilities = _wm_capabilities()
        if not capabilities['TRACKPAD_PHYSICAL_DIRECTION']:
            row = col.row()
            row.active = inputs.use_multitouch_gestures
            row.prop(inputs, "touchpad_scroll_direction", text="Scroll Direction")


class USERPREF_PT_input_tablet(InputPanel, CenterAlignMixIn, Panel):
    bl_label = "Tablet"

    def draw_centered(self, context, layout):
        prefs = context.preferences
        inputs = prefs.inputs

        import sys
        if sys.platform[:3] == "win":
            layout.prop(inputs, "tablet_api")
            layout.separator()

        col = layout.column()
        col.prop(inputs, "pressure_threshold_max")
        col.prop(inputs, "pressure_softness")


class USERPREF_PT_input_ndof(InputPanel, CenterAlignMixIn, Panel):
    bl_label = "NDOF"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        return bpy.app.build_options.input_ndof

    def draw_centered(self, context, layout):
        prefs = context.preferences
        inputs = prefs.inputs

        USERPREF_PT_ndof_settings.draw_settings(layout, inputs)


# -----------------------------------------------------------------------------
# Navigation Panels

class NavigationPanel:
    bl_space_type = 'PREFERENCES'
    bl_region_type = 'WINDOW'
    bl_context = "navigation"


class USERPREF_PT_navigation_orbit(NavigationPanel, CenterAlignMixIn, Panel):
    bl_label = "Orbit & Pan"

    def draw_centered(self, context, layout):
        prefs = context.preferences
        inputs = prefs.inputs
        view = prefs.view

        col = layout.column()

        col.row().prop(inputs, "view_rotate_method", expand=True)
        if inputs.view_rotate_method == 'TURNTABLE':
            col.prop(inputs, "view_rotate_sensitivity_turntable")
        else:
            col.prop(inputs, "view_rotate_sensitivity_trackball")
        col.prop(inputs, "use_rotate_around_active")

        col.separator()

        col = layout.column(heading="Auto")
        col.prop(inputs, "use_auto_perspective", text="Perspective")
        col.prop(inputs, "use_mouse_depth_navigate", text="Depth")

        col = layout.column()
        col.prop(view, "smooth_view")
        col.prop(view, "rotation_angle")


class USERPREF_PT_navigation_zoom(NavigationPanel, CenterAlignMixIn, Panel):
    bl_label = "Zoom"

    def draw_centered(self, context, layout):
        prefs = context.preferences
        inputs = prefs.inputs

        col = layout.column()

        col.row().prop(inputs, "view_zoom_method", text="Zoom Method")
        if inputs.view_zoom_method in {'DOLLY', 'CONTINUE'}:
            col.row().prop(inputs, "view_zoom_axis")
            col.prop(inputs, "use_zoom_to_mouse")
            col = layout.column(heading="Invert Zoom Direction", align=True)
            col.prop(inputs, "invert_mouse_zoom", text="Mouse")
            col.prop(inputs, "invert_zoom_wheel", text="Wheel")
        else:
            col.prop(inputs, "use_zoom_to_mouse")
            col.prop(inputs, "invert_zoom_wheel", text="Invert Wheel Zoom Direction")


class USERPREF_PT_navigation_fly_walk(NavigationPanel, CenterAlignMixIn, Panel):
    bl_label = "Fly & Walk"

    def draw_centered(self, context, layout):
        prefs = context.preferences
        inputs = prefs.inputs

        layout.row().prop(inputs, "navigation_mode", expand=True)


class USERPREF_PT_navigation_fly_walk_navigation(NavigationPanel, CenterAlignMixIn, Panel):
    bl_label = "Walk"
    bl_parent_id = "USERPREF_PT_navigation_fly_walk"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        prefs = context.preferences
        return prefs.inputs.navigation_mode == 'WALK'

    def draw_centered(self, context, layout):
        prefs = context.preferences
        inputs = prefs.inputs
        walk = inputs.walk_navigation

        col = layout.column()
        col.prop(walk, "use_mouse_reverse")
        col.prop(walk, "mouse_speed")
        col.prop(walk, "teleport_time")

        col = layout.column(align=True)
        col.prop(walk, "walk_speed")
        col.prop(walk, "walk_speed_factor")


class USERPREF_PT_navigation_fly_walk_gravity(NavigationPanel, CenterAlignMixIn, Panel):
    bl_label = "Gravity"
    bl_parent_id = "USERPREF_PT_navigation_fly_walk"
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

    def draw_centered(self, context, layout):
        prefs = context.preferences
        inputs = prefs.inputs
        walk = inputs.walk_navigation

        layout.active = walk.use_gravity

        col = layout.column()
        col.prop(walk, "view_height")
        col.prop(walk, "jump_height")


# Special case, this is only exposed as a popover.
class USERPREF_PT_ndof_settings(Panel):
    bl_label = "3D Mouse Settings"
    bl_space_type = 'TOPBAR'  # dummy.
    bl_region_type = 'HEADER'
    bl_ui_units_x = 12

    @staticmethod
    def draw_settings(layout, props, show_3dview_settings=True):

        # Include this setting as it impacts 2D views as well (inverting translation).
        col = layout.column()
        col.row().prop(props, "ndof_navigation_mode", text="Navigation Mode")

        if show_3dview_settings:
            col.prop(props, "ndof_lock_horizon", text="Lock Horizon")

            layout.separator()

        if show_3dview_settings:
            col = layout.column(heading="Orbit Center")
            col.active = props.ndof_navigation_mode == 'OBJECT'
            col.prop(props, "ndof_orbit_center_auto")
            colsub = col.column()
            colsub.active = props.ndof_orbit_center_auto
            colsub.prop(props, "ndof_orbit_center_selected")
            del colsub
            col.separator()

            col = layout.column(heading="Show")
            col.prop(props, "ndof_show_guide_orbit_axis", text="Orbit Axis")
            colsub = col.column()
            colsub.active = props.ndof_navigation_mode == 'OBJECT'
            colsub.prop(props, "ndof_show_guide_orbit_center", text="Orbit Center")
            del colsub

        layout.separator()

        layout_header, layout_advanced = layout.panel("NDOF_advanced", default_closed=True)
        layout_header.label(text="Advanced")
        if layout_advanced:
            col = layout_advanced.column()
            col.prop(props, "ndof_translation_sensitivity")
            col.prop(props, "ndof_rotation_sensitivity")
            col.prop(props, "ndof_deadzone")

            col.separator()
            col.row().prop(props, "ndof_zoom_direction", expand=True)
            col.separator()

            row = col.row(heading=("Invert Pan" if show_3dview_settings else "Invert Pan Axis"))
            for text, attr in (
                    ("X", "ndof_panx_invert_axis"),
                    ("Y", "ndof_pany_invert_axis"),
                    ("Z", "ndof_panz_invert_axis"),
            ):
                row.prop(props, attr, text=text, toggle=True)

            if show_3dview_settings:
                row = col.row(heading="Invert Rotate")
                for text, attr in (
                        ("X", "ndof_rotx_invert_axis"),
                        ("Y", "ndof_roty_invert_axis"),
                        ("Z", "ndof_rotz_invert_axis"),
                ):
                    row.prop(props, attr, text=text, toggle=True)

            if show_3dview_settings:
                col.prop(props, "ndof_lock_camera_pan_zoom")

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False  # No animation.

        input_prefs = context.preferences.inputs
        is_view3d = context.space_data.type == 'VIEW_3D'
        self.draw_settings(layout, input_prefs, is_view3d)

# -----------------------------------------------------------------------------
# Key-Map Editor Panels


class KeymapPanel:
    bl_space_type = 'PREFERENCES'
    bl_region_type = 'WINDOW'
    bl_context = "keymap"


class USERPREF_MT_keyconfigs(Menu):
    bl_label = "KeyPresets"
    preset_subdir = "keyconfig"
    preset_operator = "preferences.keyconfig_activate"

    def draw(self, context):
        Menu.draw_preset(self, context)


class USERPREF_PT_keymap(KeymapPanel, Panel):
    bl_label = "Keymap"
    bl_options = {'HIDE_HEADER'}

    def draw(self, context):
        from rna_keymap_ui import draw_keymaps

        layout = self.layout

        # import time

        # start = time.time()

        # Keymap Settings
        draw_keymaps(context, layout)

        # print("runtime", time.time() - start)


# -----------------------------------------------------------------------------
# Extension Panels


class USERPREF_MT_extensions_active_repo(Menu):
    bl_label = "Active Repository"

    def draw(self, _context):
        # Add-ons may extend.
        pass


class USERPREF_MT_extensions_active_repo_remove(Menu):
    bl_label = "Remove Extension Repository"

    def draw(self, context):
        layout = self.layout

        extensions = context.preferences.extensions
        active_repo_index = extensions.active_repo

        try:
            active_repo = None if active_repo_index < 0 else extensions.repos[active_repo_index]
        except IndexError:
            active_repo = None

        is_system_repo = (active_repo.use_remote_url is False) and (active_repo.source == 'SYSTEM')

        props = layout.operator("preferences.extension_repo_remove", text="Remove Repository")
        props.index = active_repo_index

        if not is_system_repo:
            props = layout.operator("preferences.extension_repo_remove", text="Remove Repository & Files")
            props.index = active_repo_index
            props.remove_files = True


class USERPREF_PT_extensions_repos(Panel):
    bl_label = "Repositories"
    bl_options = {'HIDE_HEADER'}

    bl_space_type = 'TOPBAR'  # dummy.
    bl_region_type = 'HEADER'

    # Show wider than most panels so the URL & directory aren't overly clipped.
    bl_ui_units_x = 16

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = False
        layout.use_property_decorate = False

        extensions = context.preferences.extensions
        active_repo_index = extensions.active_repo

        row = layout.row()

        row.template_list(
            "USERPREF_UL_extension_repos", "user_extension_repos",
            extensions, "repos",
            extensions, "active_repo",
        )

        col = row.column(align=True)
        col.operator_menu_enum("preferences.extension_repo_add", "type", text="", icon='ADD')
        col.menu("USERPREF_MT_extensions_active_repo_remove", text="", icon='REMOVE')

        col.separator()

        col.menu_contents("USERPREF_MT_extensions_active_repo")

        try:
            active_repo = None if active_repo_index < 0 else extensions.repos[active_repo_index]
        except IndexError:
            active_repo = None

        if active_repo is None:
            return

        # NOTE: changing repositories from remote to local & vice versa could be supported but is obscure enough
        # that it can be hidden entirely. If there is a some justification to show this, it can be exposed.
        # For now it can be accessed from Python if someone is.
        # `layout.prop(active_repo, "use_remote_url", text="Use Remote URL")`

        use_remote_url = active_repo.use_remote_url
        if use_remote_url:
            row = layout.row()
            split = row.split(factor=0.936)
            if active_repo.remote_url == "":
                split.alert = True
            split.prop(active_repo, "remote_url", text="", icon='INTERNET', placeholder="Repository URL")
            split = row.split()

            if active_repo.use_access_token:
                access_token_icon = 'LOCKED' if active_repo.access_token else 'UNLOCKED'
                row = layout.row()
                split = row.split(factor=0.936)
                split.prop(active_repo, "access_token", icon=access_token_icon)
                split = row.split()

            layout.prop(active_repo, "use_sync_on_startup")

        layout_header, layout_panel = layout.panel("advanced", default_closed=True)
        layout_header.label(text="Advanced")

        if layout_panel:
            layout_panel.use_property_split = True
            use_custom_directory = active_repo.use_custom_directory

            col = layout_panel.column(align=False, heading="Custom Directory")
            row = col.row(align=True)
            sub = row.row(align=True)
            sub.prop(active_repo, "use_custom_directory", text="")
            sub = sub.row(align=True)
            sub.active = use_custom_directory
            if use_custom_directory:
                if active_repo.custom_directory == "":
                    sub.alert = True
                sub.prop(active_repo, "custom_directory", text="")
            else:
                # Show the read-only directory property.
                # Apart from being consistent with the custom directory UI,
                # prefer a read-only property over a label because this is not necessarily
                # valid UTF-8 which will raise a Python exception when passed in as text.
                sub.prop(active_repo, "directory", text="")

            if use_remote_url:
                row = layout_panel.row(align=True, heading="Authentication")
                row.prop(active_repo, "use_access_token")

                layout_panel.prop(active_repo, "use_cache")
            else:
                layout_panel.prop(active_repo, "source")

            layout_panel.separator()

            layout_panel.prop(active_repo, "module")


# -----------------------------------------------------------------------------
# Extensions Panels

class ExtensionsPanel:
    bl_space_type = 'PREFERENCES'
    bl_region_type = 'WINDOW'
    bl_context = "extensions"


class USERPREF_PT_extensions(ExtensionsPanel, Panel):
    bl_label = "Extensions"
    bl_options = {'HIDE_HEADER'}

    def draw(self, context):
        pass


# -----------------------------------------------------------------------------
# Add-on Panels

# Only a popover.
class USERPREF_PT_addons_filter(Panel):
    bl_label = "Add-ons Filter"

    bl_space_type = 'TOPBAR'  # dummy.
    bl_region_type = 'HEADER'
    bl_ui_units_x = 12

    def draw(self, context):
        USERPREF_PT_addons._draw_addon_header_for_extensions_popover(self.layout, context)


class AddOnPanel:
    bl_space_type = 'PREFERENCES'
    bl_region_type = 'WINDOW'
    bl_context = "addons"


class USERPREF_PT_addons(AddOnPanel, Panel):
    bl_label = "Add-ons"
    bl_options = {'HIDE_HEADER'}

    _support_icon_mapping = {
        'OFFICIAL': 'BLENDER',
        'COMMUNITY': 'COMMUNITY',
        'TESTING': 'EXPERIMENTAL',
    }

    @staticmethod
    def is_user_addon(mod, user_addon_paths):
        import os

        if not user_addon_paths:
            for path in (
                    bpy.utils.script_path_user(),
                    *bpy.utils.script_paths_pref(),
            ):
                if path is not None:
                    user_addon_paths.append(os.path.join(path, "addons"))

        for path in user_addon_paths:
            if bpy.path.is_subdir(mod.__file__, path):
                return True
        return False

    @staticmethod
    def draw_addon_preferences(layout, context, addon_preferences):
        if (draw := getattr(addon_preferences, "draw", None)) is None:
            return

        addon_preferences_class = type(addon_preferences)
        layout.label(text=" Preferences")
        box_prefs = layout.box()
        addon_preferences_class.layout = box_prefs
        try:
            draw(context)
        except Exception:
            import traceback
            traceback.print_exc()
            box_prefs.label(text="Error (see console)", icon='ERROR')
        del addon_preferences_class.layout

    @staticmethod
    def draw_error(layout, message):
        lines = message.split("\n")
        box = layout.box()
        sub = box.row()
        sub.label(text=lines[0])
        sub.label(icon='ERROR')
        for line in lines[1:]:
            box.label(text=line)

    @staticmethod
    def _draw_addon_header(layout, prefs, wm):
        split = layout.split(factor=0.6)

        row = split.row()
        row.prop(wm, "addon_support", expand=True)

        row = split.row(align=True)
        row.operator("preferences.addon_install", icon='IMPORT', text="Install...")
        row.operator("preferences.addon_refresh", icon='FILE_REFRESH', text="Refresh")

        row = layout.row()
        row.prop(prefs.view, "show_addons_enabled_only")
        row.prop(wm, "addon_filter", text="")
        row.prop(wm, "addon_search", text="", icon='VIEWZOOM')

    @staticmethod
    def _draw_addon_header_for_extensions_popover(layout, context):

        wm = context.window_manager
        prefs = context.preferences

        row = layout.row()
        row.prop(wm, "addon_support", expand=True)

        row = layout.row()
        row.prop(prefs.view, "show_addons_enabled_only")

        # Not filter, we could expose elsewhere.
        row = layout.row()
        row.operator("preferences.addon_install", icon='IMPORT', text="Install...")
        row.operator("preferences.addon_refresh", icon='FILE_REFRESH', text="Refresh")

    def draw(self, context):
        import os
        import addon_utils

        prefs = context.preferences

        if self.is_extended():
            # Rely on the draw function being extended by the extensions add-on (`bl_pkg`).
            return

        layout = self.layout
        wm = context.window_manager

        used_addon_module_name_map = {addon.module: addon for addon in prefs.addons}

        addon_user_dirs = tuple(
            p for p in (
                *[os.path.join(pref_p, "addons") for pref_p in bpy.utils.script_paths_pref()],
                bpy.utils.user_resource('SCRIPTS', path="addons"),
            )
            if p
        )

        self._draw_addon_header(layout, prefs, wm)

        layout_topmost = layout.column()

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

                sub_row = sub_col.row()
                sub_row.label(text="    " + addon_file)
                sub_row.operator("wm.path_open", text="", icon='FILE_FOLDER').filepath = os.path.dirname(addon_file)

                sub_row = sub_col.row()
                sub_row.label(text="    " + addon_path)
                sub_row.operator("wm.path_open", text="", icon='FILE_FOLDER').filepath = os.path.dirname(addon_path)

        if addon_utils.error_encoding:
            self.draw_error(
                col,
                "One or more addons do not have UTF-8 encoding\n"
                "(see console for details)",
            )

        show_enabled_only = prefs.view.show_addons_enabled_only
        filter = wm.addon_filter
        search = wm.addon_search.casefold()
        support = wm.addon_support

        module_names = set()

        # initialized on demand
        user_addon_paths = []

        for mod in addon_utils.modules(refresh=False):
            module_names.add(addon_module_name := mod.__name__)
            bl_info = addon_utils.module_bl_info(mod)

            is_enabled = addon_module_name in used_addon_module_name_map

            if bl_info["support"] not in support:
                continue

            # check if addon should be visible with current filters
            is_visible = (
                (filter == "All") or
                (filter == bl_info["category"]) or
                (filter == "User" and (mod.__file__.startswith(addon_user_dirs)))
            )
            if show_enabled_only:
                is_visible = is_visible and is_enabled

            if not is_visible:
                continue

            if search and not (
                    (search in bl_info["name"].casefold() or
                     search in iface_(bl_info["name"]).casefold()) or
                    (bl_info["author"] and (search in bl_info["author"].casefold())) or
                    ((filter == "All") and (
                        search in bl_info["category"].casefold() or
                        search in iface_(bl_info["category"]).casefold()
                    ))
            ):
                continue

            # Addon UI Code
            col_box = col.column()
            box = col_box.box()
            colsub = box.column()
            row = colsub.row(align=True)

            row.operator(
                "preferences.addon_expand",
                icon='DISCLOSURE_TRI_DOWN' if bl_info["show_expanded"] else 'DISCLOSURE_TRI_RIGHT',
                emboss=False,
            ).module = addon_module_name

            row.operator(
                "preferences.addon_disable" if is_enabled else "preferences.addon_enable",
                icon='CHECKBOX_HLT' if is_enabled else 'CHECKBOX_DEHLT', text="",
                emboss=False,
            ).module = addon_module_name

            sub = row.row()
            sub.active = is_enabled
            sub.label(text="{:s}: {:s}".format(iface_(bl_info["category"]), iface_(bl_info["name"])))

            if bl_info["warning"]:
                sub.label(icon='ERROR')

            # icon showing support level.
            sub.label(icon=self._support_icon_mapping.get(bl_info["support"], 'QUESTION'))

            # Expanded UI (only if additional bl_info is available)
            if bl_info["show_expanded"]:
                if value := bl_info["description"]:
                    split = colsub.row().split(factor=0.15)
                    split.label(text="Description:")
                    split.label(text=iface_(value))
                if value := bl_info["location"]:
                    split = colsub.row().split(factor=0.15)
                    split.label(text="Location:")
                    split.label(text=iface_(value))
                if mod:
                    split = colsub.row().split(factor=0.15)
                    split.label(text="File:")
                    split.label(text=mod.__file__, translate=False)
                if value := bl_info["author"]:
                    split = colsub.row().split(factor=0.15)
                    split.label(text="Author:")
                    split.label(text=value, translate=False)
                if value := bl_info["version"]:
                    split = colsub.row().split(factor=0.15)
                    split.label(text="Version:")
                    split.label(text=".".join(str(x) for x in value), translate=False)
                if value := bl_info["warning"]:
                    split = colsub.row().split(factor=0.15)
                    split.label(text="Warning:")
                    split.label(text="  " + iface_(value), icon='ERROR')
                del value

                user_addon = USERPREF_PT_addons.is_user_addon(mod, user_addon_paths)
                if bl_info["doc_url"] or bl_info.get("tracker_url"):
                    split = colsub.row().split(factor=0.15)
                    split.label(text="Internet:")
                    sub = split.row()
                    if bl_info["doc_url"]:
                        sub.operator(
                            "wm.url_open", text="Documentation", icon='HELP',
                        ).url = bl_info["doc_url"]
                    # Only add "Report a Bug" button if tracker_url is set.
                    # None of the core add-ons are expected to have tracker info (glTF is the exception).
                    if bl_info.get("tracker_url"):
                        sub.operator(
                            "wm.url_open", text="Report a Bug", icon='URL',
                        ).url = bl_info["tracker_url"]

                if user_addon:
                    split = colsub.row().split(factor=0.15)
                    split.label(text="User:")
                    split.operator(
                        "preferences.addon_remove", text="Remove", icon='CANCEL',
                    ).module = mod.__name__

                # Show addon user preferences
                if is_enabled:
                    if (addon_preferences := used_addon_module_name_map[addon_module_name].preferences) is not None:
                        self.draw_addon_preferences(col_box, context, addon_preferences)

        if filter in {"All", "Enabled"}:
            # Append missing scripts
            # First collect scripts that are used but have no script file.
            missing_modules = {
                addon_module_name for addon_module_name in used_addon_module_name_map
                if addon_module_name not in module_names
            }

            if missing_modules:
                layout_topmost.column().separator()
                layout_topmost.column().label(text="Missing script files")

                for addon_module_name in sorted(missing_modules):
                    is_enabled = addon_module_name in used_addon_module_name_map
                    # Addon UI Code
                    box = layout_topmost.column().box()
                    colsub = box.column()
                    row = colsub.row(align=True)

                    row.label(text="", icon='ERROR')

                    if is_enabled:
                        row.operator(
                            "preferences.addon_disable", icon='CHECKBOX_HLT', text="", emboss=False,
                        ).module = addon_module_name

                    row.label(text=addon_module_name, translate=False)


# -----------------------------------------------------------------------------
# Asset Panels

class AssetsPanel:
    bl_space_type = 'PREFERENCES'
    bl_region_type = 'WINDOW'
    bl_context = "assets"


class USERPREF_PT_assets(AssetsPanel, Panel):
    bl_label = "Assets"
    bl_options = {'HIDE_HEADER'}

    def draw(self, context):
        prefs = context.preferences

        # Check if the extensions "Welcome" panel should be displayed.
        # Even though it can be dismissed it's quite "in-your-face" so only show when it's needed.
        if (
            # The user didn't dismiss.
            (not prefs.extensions.use_online_access_handled) and
            # Running offline.
            (not bpy.app.online_access) and
            # There is one or more libraries that require remote access.
            any(library for library in prefs.filepaths.asset_libraries if
                # repo.enabled and
                library.use_remote_url)
        ):
            layout = self.layout
            layout_header, layout_panel = layout.panel("advanced", default_closed=False)
            layout_header.label(text="Internet Access Required", icon='INTERNET_OFFLINE')

            if layout_panel is None:
                return

            box = layout_panel.box()

            # Text wrapping isn't supported, manually wrap.
            for line in (
                    rpt_("Internet access is required to browse and download online assets."),
                    rpt_("You can adjust this later from \"System\" preferences."),
            ):
                box.label(text=line, translate=False)

            # TODO: Link to the manual?
            # row.operator(
            #     "wm.url_open",
            #     text="",
            #     icon='URL',
            #     emboss=False,
            # ).url = (
            #     "https://docs.blender.org/manual/"
            #     "{:s}/{:d}.{:d}/editors/preferences/extensions.html#installing-extensions"
            # ).format(
            #     bpy.utils.manual_language_code(),
            #     *bpy.app.version[:2],
            # )

            row = box.row()
            props = row.operator("wm.context_set_boolean", text="Continue Offline", icon='X')
            props.data_path = "preferences.extensions.use_online_access_handled"
            props.value = True

            # The only reason to prefer this over `screen.userpref_show`
            # is it will be disabled when `--offline-mode` is forced with a useful error for why.
            row.operator("extensions.userpref_allow_online", text="Allow Online Access", icon='CHECKMARK')


class USERPREF_PT_assets_asset_libraries(AssetsPanel, Panel):
    bl_label = "Asset Libraries"

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = False
        layout.use_property_decorate = False

        paths = context.preferences.filepaths
        active_library_index = paths.active_asset_library

        row = layout.row()

        row.template_list(
            "USERPREF_UL_asset_libraries", "user_asset_libraries",
            paths, "asset_libraries",
            paths, "active_asset_library",
        )

        col = row.column(align=True)
        col.operator_menu_enum("preferences.asset_library_add", "type", text="", icon='ADD')
        props = col.operator("preferences.asset_library_remove", text="", icon='REMOVE')
        props.index = active_library_index

        try:
            active_library = None if active_library_index < 0 else paths.asset_libraries[active_library_index]
        except IndexError:
            active_library = None

        if active_library is None:
            return

        layout.separator()

        if active_library.use_remote_url:
            layout.prop(active_library, "remote_url")
            layout.prop(active_library, "path", text="Download Location")
        else:
            layout.prop(active_library, "path")
            layout.prop(active_library, "import_method", text="Import Method")
            layout.prop(active_library, "use_relative_path")


class USERPREF_UL_asset_libraries(UIList):
    def draw_item(self, _context, layout, _data, item, _icon, _active_data, _active_propname, _index):
        asset_library = item

        icon = 'INTERNET' if asset_library.use_remote_url else 'DISK_DRIVE'
        layout.prop(asset_library, "name", text="", icon=icon, emboss=False)


# -----------------------------------------------------------------------------
# Studio Light Panels


class StudioLightPanel:
    bl_space_type = 'PREFERENCES'
    bl_region_type = 'WINDOW'
    bl_context = "lights"


class StudioLightPanelMixin:

    def _get_lights(self, prefs):
        return [light for light in prefs.studio_lights if light.is_user_defined and light.type == self.sl_type]

    def draw(self, context):
        layout = self.layout
        prefs = context.preferences
        lights = self._get_lights(prefs)

        self.draw_light_list(layout, lights)

    def draw_light_list(self, layout, lights):
        if lights:
            flow = layout.grid_flow(row_major=False, columns=4, even_columns=True, even_rows=True, align=False)
            for studio_light in lights:
                self.draw_studio_light(flow, studio_light)
        else:
            layout.label(text=self.get_error_message())

    def get_error_message(self):
        return rpt_("No custom {:s} configured").format(self.bl_label)

    def draw_studio_light(self, layout, studio_light):
        box = layout.box()
        row = box.row()

        row.template_icon(layout.icon(studio_light), scale=3.0)
        col = row.column()
        props = col.operator("preferences.studiolight_uninstall", text="", icon='REMOVE')
        props.index = studio_light.index

        if studio_light.type == 'STUDIO':
            props = col.operator("preferences.studiolight_copy_settings", text="", icon='IMPORT')
            props.index = studio_light.index

        box.label(text=studio_light.name)


class USERPREF_PT_studiolight_matcaps(StudioLightPanel, StudioLightPanelMixin, Panel):
    bl_label = "MatCaps"
    sl_type = 'MATCAP'

    def draw_header_preset(self, _context):
        layout = self.layout
        layout.operator("preferences.studiolight_install", icon='IMPORT', text="Install...").type = 'MATCAP'
        layout.separator()

    def get_error_message(self):
        return rpt_("No custom MatCaps configured")


class USERPREF_PT_studiolight_world(StudioLightPanel, StudioLightPanelMixin, Panel):
    bl_label = "HDRIs"
    sl_type = 'WORLD'

    def draw_header_preset(self, _context):
        layout = self.layout
        layout.operator("preferences.studiolight_install", icon='IMPORT', text="Install...").type = 'WORLD'
        layout.separator()

    def get_error_message(self):
        return rpt_("No custom HDRIs configured")


class USERPREF_PT_studiolight_lights(StudioLightPanel, StudioLightPanelMixin, Panel):
    bl_label = "Studio Lights"
    sl_type = 'STUDIO'

    def draw_header_preset(self, _context):
        layout = self.layout
        props = layout.operator("preferences.studiolight_install", icon='IMPORT', text="Install...")
        props.type = 'STUDIO'
        props.filter_glob = ".sl"
        layout.separator()

    def get_error_message(self):
        return rpt_("No custom Studio Lights configured")


class USERPREF_PT_studiolight_light_editor(StudioLightPanel, Panel):
    bl_label = "Editor"
    bl_parent_id = "USERPREF_PT_studiolight_lights"
    bl_options = {'DEFAULT_CLOSED'}

    @staticmethod
    def opengl_light_buttons(layout, light):
        col = layout.column()
        box = col.box()
        box.active = light.use

        box.prop(light, "use", text="Use Light")
        box.prop(light, "diffuse_color", text="Diffuse")
        box.prop(light, "specular_color", text="Specular")
        box.prop(light, "smooth")
        box.prop(light, "direction")

        col.separator()

    def draw(self, context):
        layout = self.layout

        prefs = context.preferences
        system = prefs.system

        row = layout.row()
        row.prop(system, "use_studio_light_edit", toggle=True)
        row.operator("preferences.studiolight_new", text="Save as Studio light", icon='FILE_TICK')

        layout.separator()

        layout.use_property_split = True

        flow = layout.grid_flow(row_major=True, columns=2, even_rows=True, even_columns=True)
        flow.active = system.use_studio_light_edit

        for light in system.solid_lights:
            self.opengl_light_buttons(flow, light)

        layout.prop(system, "light_ambient")


# -----------------------------------------------------------------------------
# Experimental Panels

# Also used for "Developer Tools" which are stored in `preferences.experimental` too.
def _draw_experimental_items(layout, preferences, items, url_prefix="https://projects.blender.org/"):
    experimental = preferences.experimental

    layout.use_property_split = False
    layout.use_property_decorate = False

    for prop_keywords, reference in items:
        split = layout.split(factor=0.66)
        col = split.split()
        col.prop(experimental, **prop_keywords)

        if reference:
            if type(reference) is tuple:
                url_ext = reference[0]
                text = reference[1]
            else:
                url_ext = reference
                text = reference

            col = split.split()
            col.operator("wm.url_open", text=text, icon='URL').url = url_prefix + url_ext


class USERPREF_PT_developer_tools(Panel):
    bl_space_type = 'PREFERENCES'
    bl_region_type = 'WINDOW'
    bl_context = "developer_tools"
    bl_label = "Debug"

    @classmethod
    def poll(cls, context):
        return context.preferences.view.show_developer_ui

    def draw(self, context):
        _draw_experimental_items(
            self.layout,
            context.preferences,
            (
                ({"property": "use_undo_legacy"}, ("blender/blender/issues/60695", "#60695")),
                ({"property": "override_auto_resync"}, ("blender/blender/issues/83811", "#83811")),
                ({"property": "use_all_linked_data_direct"}, None),
                ({"property": "use_recompute_usercount_on_save_debug"}, None),
                ({"property": "use_cycles_debug"}, None),
                ({"property": "show_asset_debug_info"}, None),
                ({"property": "use_asset_indexing"}, None),
                ({"property": "use_viewport_debug"}, None),
                ({"property": "use_eevee_debug"}, None),
                ({"property": "use_extensions_debug"}, ("/blender/blender/issues/119521", "#119521")),
                ({"property": "no_data_block_packing"}, ("/blender/blender/issues/132167", "#132167")),
            ),
        )


class ExperimentalPanel:
    bl_space_type = 'PREFERENCES'
    bl_region_type = 'WINDOW'
    bl_context = "experimental"

    @classmethod
    def poll(cls, _context):
        return bpy.app.version_cycle == "alpha"


"""
# Example panel, leave it here so we always have a template to follow even
# after the features are gone from the experimental panel.

class USERPREF_PT_experimental_virtual_reality(ExperimentalPanel, Panel):
    bl_label = "Virtual Reality"

    def draw(self, context):
        _draw_experimental_items(
            self.layout,
            context.preferences,
            (
                ({"property": "use_virtual_reality_scene_inspection"}, ("blender/blender/issues/71347", "#71347")),
                ({"property": "use_virtual_reality_immersive_drawing"}, ("blender/blender/issues/71348", "#71348")),
            ),
        )
"""


class USERPREF_PT_experimental_new_features(ExperimentalPanel, Panel):
    bl_label = "New Features"

    def draw(self, context):
        _draw_experimental_items(
            self.layout,
            context.preferences,
            (
                ({"property": "use_extended_asset_browser"},
                 ("blender/blender/projects/10", "Pipeline, Assets & IO Project Page")),
                ({"property": "use_shader_node_previews"}, ("blender/blender/issues/110353", "#110353")),
                ({"property": "use_geometry_nodes_lists"}, ("blender/blender/issues/140918", "#140918")),
            ),
        )


class USERPREF_PT_experimental_prototypes(ExperimentalPanel, Panel):
    bl_label = "Prototypes"

    def draw(self, context):
        _draw_experimental_items(
            self.layout,
            context.preferences,
            (
                ({"property": "use_new_curves_tools"}, ("blender/blender/issues/68981", "#68981")),
                ({"property": "use_sculpt_texture_paint"}, ("blender/blender/issues/96225", "#96225")),
                ({"property": "write_legacy_blend_file_format"}, ("/blender/blender/issues/129309", "#129309")),
            ),
        )


# Keep this as tweaks can be useful to restore.
"""
class USERPREF_PT_experimental_tweaks(ExperimentalPanel, Panel):
    bl_label = "Tweaks"

    def draw(self, context):
        _draw_experimental_items(
            self.layout,
            context.preferences,
            (
                ({"property": "use_select_nearest_on_first_click"}, ("blender/blender/issues/96752", "#96752")),
            ),
        )

"""

# -----------------------------------------------------------------------------
# Class Registration

# Order of registration defines order in UI,
# so dynamically generated classes are "injected" in the intended order.
classes = (
    USERPREF_PT_theme_user_interface,
    *ThemeGenericClassGenerator.generate_panel_classes_for_wcols(),
    USERPREF_HT_header,
    USERPREF_PT_navigation_bar,
    USERPREF_PT_save_preferences,
    USERPREF_MT_editor_menus,
    USERPREF_MT_view,
    USERPREF_MT_save_load,

    USERPREF_PT_interface_display,
    USERPREF_PT_interface_editors,
    USERPREF_PT_interface_temporary_windows,
    USERPREF_PT_interface_statusbar,
    USERPREF_PT_interface_translation,
    USERPREF_PT_interface_accessibility,
    USERPREF_PT_interface_text,
    USERPREF_PT_interface_menus,
    USERPREF_PT_interface_menus_mouse_over,
    USERPREF_PT_interface_menus_pie,

    USERPREF_PT_viewport_display,
    USERPREF_PT_viewport_quality,
    USERPREF_PT_viewport_textures,
    USERPREF_PT_viewport_subdivision,

    USERPREF_PT_edit_objects,
    USERPREF_PT_edit_objects_new,
    USERPREF_PT_edit_objects_duplicate_data,
    USERPREF_PT_edit_cursor,
    USERPREF_PT_edit_annotations,
    USERPREF_PT_edit_weight_paint,
    USERPREF_PT_edit_gpencil,
    USERPREF_PT_edit_text_editor,
    USERPREF_PT_edit_node_editor,
    USERPREF_PT_edit_sequence_editor,
    USERPREF_PT_edit_misc,

    USERPREF_PT_animation_timeline,
    USERPREF_PT_animation_keyframes,
    USERPREF_PT_animation_fcurves,

    USERPREF_PT_system_cycles_devices,
    USERPREF_PT_system_display_graphics,
    USERPREF_PT_system_os_settings,
    USERPREF_PT_system_network,
    USERPREF_PT_system_memory,
    USERPREF_PT_system_video_sequencer,
    USERPREF_PT_system_sound,

    USERPREF_MT_interface_theme_presets,
    USERPREF_PT_theme,
    USERPREF_PT_theme_interface_panel,
    USERPREF_PT_theme_interface_gizmos,
    USERPREF_PT_theme_interface_icons,
    USERPREF_PT_theme_interface_state,
    USERPREF_PT_theme_interface_styles,
    USERPREF_PT_theme_interface_transparent_checker,
    USERPREF_PT_theme_text_style,
    USERPREF_PT_theme_bone_color_sets,
    USERPREF_PT_theme_collection_colors,
    USERPREF_PT_theme_strip_colors,

    USERPREF_PT_file_paths_data,
    USERPREF_PT_file_paths_render,
    USERPREF_PT_file_paths_script_directories,
    USERPREF_PT_file_paths_applications,
    USERPREF_PT_text_editor,
    USERPREF_PT_text_editor_presets,
    USERPREF_PT_file_paths_development,

    USERPREF_PT_saveload_blend,
    USERPREF_PT_saveload_autorun,
    USERPREF_PT_saveload_file_browser,

    USERPREF_MT_keyconfigs,

    USERPREF_PT_input_keyboard,
    USERPREF_PT_input_mouse,
    USERPREF_PT_input_tablet,
    USERPREF_PT_input_touchpad,
    USERPREF_PT_input_ndof,
    USERPREF_PT_navigation_orbit,
    USERPREF_PT_navigation_zoom,
    USERPREF_PT_navigation_fly_walk,
    USERPREF_PT_navigation_fly_walk_navigation,
    USERPREF_PT_navigation_fly_walk_gravity,

    USERPREF_PT_keymap,

    USERPREF_PT_extensions,
    USERPREF_PT_addons,

    USERPREF_PT_assets,
    USERPREF_PT_assets_asset_libraries,

    USERPREF_MT_extensions_active_repo,
    USERPREF_MT_extensions_active_repo_remove,
    USERPREF_PT_extensions_repos,

    USERPREF_PT_studiolight_lights,
    USERPREF_PT_studiolight_light_editor,
    USERPREF_PT_studiolight_matcaps,
    USERPREF_PT_studiolight_world,

    # Popovers.
    USERPREF_PT_ndof_settings,
    USERPREF_PT_addons_filter,

    USERPREF_PT_experimental_new_features,
    USERPREF_PT_experimental_prototypes,
    # USERPREF_PT_experimental_tweaks,

    USERPREF_PT_developer_tools,

    # UI lists
    USERPREF_UL_asset_libraries,
    USERPREF_UL_extension_repos,

    # Add dynamically generated editor theme panels last,
    # so they show up last in the theme section.
    *ThemeGenericClassGenerator.generate_panel_classes_from_theme_areas(),
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
