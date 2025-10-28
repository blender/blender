# SPDX-FileCopyrightText: 2009-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

from bpy.types import Header, Menu, Panel
from bpy.app.translations import (
    pgettext_iface as iface_,
    contexts as i18n_contexts,
)
from bl_ui.space_dopesheet import (
    DopesheetFilterPopoverBase,
    dopesheet_filter,
)
from bl_ui.space_time import playback_controls


def drivers_editor_footer(layout, context):
    act_fcurve = context.active_editable_fcurve
    if not act_fcurve:
        return

    act_driver = act_fcurve.driver
    if not act_driver:
        return

    layout.separator_spacer()
    layout.label(
        text=iface_("Driver: {:s} ({:s})").format(
            act_fcurve.id_data.name,
            act_fcurve.data_path,
        ),
        translate=False,
    )

    if act_driver.variables:
        layout.separator(type='LINE')
        layout.label(text=iface_("Variables: {:d}").format(len(act_driver.variables)), translate=False)

    if act_driver.type == 'SCRIPTED' and act_driver.expression:
        layout.separator(type='LINE')
        layout.label(text=iface_("Expression: {:s}").format(act_driver.expression), translate=False)


class GRAPH_HT_header(Header):
    bl_space_type = 'GRAPH_EDITOR'

    def draw(self, context):
        layout = self.layout
        tool_settings = context.tool_settings

        st = context.space_data

        layout.template_header()

        # Now a exposed as a sub-space type
        # layout.prop(st, "mode", text="")

        GRAPH_MT_editor_menus.draw_collapsible(context, layout)

        row = layout.row(align=True)
        row.prop(st, "use_normalization", icon='NORMALIZE_FCURVES', text="Normalize", toggle=True)
        sub = row.row(align=True)
        sub.active = st.use_normalization
        sub.prop(st, "use_auto_normalization", icon='FILE_REFRESH', text="", toggle=True)

        layout.separator_spacer()

        dopesheet_filter(layout, context)

        row = layout.row(align=True)
        if st.has_ghost_curves:
            row.operator("graph.ghost_curves_clear", text="", icon='X')
        else:
            row.operator("graph.ghost_curves_create", text="", icon='FCURVE_SNAPSHOT')

        layout.popover(
            panel="GRAPH_PT_filters",
            text="",
            icon='FILTER',
        )

        layout.prop(st, "pivot_point", icon_only=True)

        row = layout.row(align=True)
        if context.space_data.mode == 'DRIVERS':
            row.prop(tool_settings, "use_snap_driver", text="")
            sub = row.row(align=True)
            sub.popover(
                panel="GRAPH_PT_driver_snapping",
                text="",
            )
        else:
            row.prop(tool_settings, "use_snap_anim", text="")
            sub = row.row(align=True)
            sub.popover(
                panel="GRAPH_PT_snapping",
                text="",
            )

        row = layout.row(align=True)
        row.prop(tool_settings, "use_proportional_fcurve", text="", icon_only=True)
        sub = row.row(align=True)
        sub.active = tool_settings.use_proportional_fcurve
        sub.prop_with_popover(
            tool_settings,
            "proportional_edit_falloff",
            text="",
            icon_only=True,
            panel="GRAPH_PT_proportional_edit",
        )


class GRAPH_HT_playback_controls(Header):
    bl_space_type = 'GRAPH_EDITOR'
    bl_region_type = 'FOOTER'

    def draw(self, context):
        layout = self.layout
        is_drivers_editor = context.space_data.mode == 'DRIVERS'

        if is_drivers_editor:
            drivers_editor_footer(layout, context)
        else:
            playback_controls(layout, context)


class GRAPH_PT_proportional_edit(Panel):
    bl_space_type = 'GRAPH_EDITOR'
    bl_region_type = 'HEADER'
    bl_label = "Proportional Editing"
    bl_ui_units_x = 8

    def draw(self, context):
        layout = self.layout
        tool_settings = context.tool_settings
        col = layout.column()
        col.active = tool_settings.use_proportional_fcurve

        col.prop(tool_settings, "proportional_edit_falloff", expand=True)
        col.prop(tool_settings, "proportional_size")


class GRAPH_PT_filters(DopesheetFilterPopoverBase, Panel):
    bl_space_type = 'GRAPH_EDITOR'
    bl_region_type = 'HEADER'
    bl_label = "Filters"

    def draw(self, context):
        layout = self.layout
        st = context.space_data

        DopesheetFilterPopoverBase.draw_generic_filters(context, layout)
        layout.separator()
        DopesheetFilterPopoverBase.draw_search_filters(context, layout)
        layout.separator()
        DopesheetFilterPopoverBase.draw_standard_filters(context, layout)

        if st.mode == 'DRIVERS':
            layout.separator()
            col = layout.column(align=True)
            col.label(text="Drivers:")
            col.prop(st.dopesheet, "show_driver_fallback_as_error")


class GRAPH_PT_snapping(Panel):
    bl_space_type = 'GRAPH_EDITOR'
    bl_region_type = 'HEADER'
    bl_label = "Snapping"

    def draw(self, context):
        layout = self.layout
        col = layout.column()
        col.label(text="Snap To")
        tool_settings = context.tool_settings
        col.prop(tool_settings, "snap_anim_element", expand=True)
        if tool_settings.snap_anim_element != 'MARKER':
            col.prop(tool_settings, "use_snap_time_absolute")


class GRAPH_PT_driver_snapping(Panel):
    bl_space_type = 'GRAPH_EDITOR'
    bl_region_type = 'HEADER'
    bl_label = "Snapping"

    def draw(self, context):
        layout = self.layout
        col = layout.column()
        tool_settings = context.tool_settings
        col.prop(tool_settings, "use_snap_driver_absolute")


class GRAPH_MT_editor_menus(Menu):
    bl_idname = "GRAPH_MT_editor_menus"
    bl_label = ""

    def draw(self, context):
        st = context.space_data
        layout = self.layout
        layout.menu("GRAPH_MT_view")
        layout.menu("GRAPH_MT_select")
        if st.mode != 'DRIVERS' and st.show_markers:
            layout.menu("GRAPH_MT_marker")
        layout.menu("GRAPH_MT_channel")
        layout.menu("GRAPH_MT_key")


class GRAPH_MT_view(Menu):
    bl_label = "View"

    def draw(self, context):
        layout = self.layout

        st = context.space_data

        layout.prop(st, "show_region_ui")
        layout.prop(st, "show_region_hud")
        layout.prop(st, "show_region_channels")
        if st.mode != 'DRIVERS':
            layout.prop(st, "show_region_footer", text="Playback Controls")
        layout.separator()

        layout.operator("graph.view_selected")
        layout.operator("graph.view_all")
        if context.scene.use_preview_range:
            layout.operator("anim.scene_range_frame", text="Frame Preview Range")
        else:
            layout.operator("anim.scene_range_frame", text="Frame Scene Range")
        layout.operator("graph.view_frame")
        layout.separator()

        layout.prop(st, "use_realtime_update")
        layout.prop(st, "show_sliders")
        layout.prop(st, "use_auto_merge_keyframes")
        layout.prop(st, "use_auto_lock_translation_axis")
        layout.separator()

        if st.mode != 'DRIVERS':
            layout.prop(st, "show_markers")
        layout.prop(st, "show_cursor")
        layout.prop(st, "show_seconds")
        layout.prop(st, "show_locked_time")
        layout.separator()

        layout.prop(st, "show_extrapolation")
        layout.prop(st, "show_handles")
        layout.prop(st, "use_only_selected_keyframe_handles")
        layout.separator()

        layout.operator("anim.previewrange_set")
        layout.operator("anim.previewrange_clear")
        layout.operator("graph.previewrange_set")
        layout.separator()

        # Add this to show key-binding (reverse action in dope-sheet).
        props = layout.operator("wm.context_set_enum", text="Toggle Dope Sheet")
        props.data_path = "area.type"
        props.value = 'DOPESHEET_EDITOR'
        layout.separator()

        layout.menu("INFO_MT_area")


class GRAPH_MT_select(Menu):
    bl_label = "Select"

    def draw(self, _context):
        layout = self.layout

        layout.operator("graph.select_all", text="All").action = 'SELECT'
        layout.operator("graph.select_all", text="None").action = 'DESELECT'
        layout.operator("graph.select_all", text="Invert").action = 'INVERT'

        layout.separator()

        layout.operator("graph.select_box", text="Box Select (Include Handles)")
        props = layout.operator("graph.select_box", text="Box Select (Axis Range)")
        props.axis_range = True
        props = layout.operator("graph.select_box")
        props.include_handles = False
        layout.operator("graph.select_circle")
        layout.operator_menu_enum("graph.select_lasso", "mode")

        layout.separator()
        layout.operator("graph.select_more", text="More")
        layout.operator("graph.select_less", text="Less")

        layout.separator()
        layout.operator("graph.select_linked")

        layout.separator()
        layout.operator("graph.select_column", text="Columns on Selected Keys").mode = 'KEYS'
        layout.operator("graph.select_column", text="Column on Current Frame").mode = 'CFRA'

        layout.operator("graph.select_column", text="Columns on Selected Markers").mode = 'MARKERS_COLUMN'
        layout.operator("graph.select_column", text="Between Selected Markers").mode = 'MARKERS_BETWEEN'

        layout.separator()
        props = layout.operator("graph.select_leftright", text="Before Current Frame")
        props.extend = False
        props.mode = 'LEFT'
        props = layout.operator("graph.select_leftright", text="After Current Frame")
        props.extend = False
        props.mode = 'RIGHT'

        layout.separator()
        props = layout.operator("graph.select_key_handles", text="Select Handles")
        props.left_handle_action = 'SELECT'
        props.right_handle_action = 'SELECT'
        props.key_action = 'KEEP'
        props = layout.operator("graph.select_key_handles", text="Select Key")
        props.left_handle_action = 'DESELECT'
        props.right_handle_action = 'DESELECT'
        props.key_action = 'SELECT'


class GRAPH_MT_marker(Menu):
    bl_label = "Marker"

    def draw(self, context):
        layout = self.layout

        from bl_ui.space_time import marker_menu_generic
        marker_menu_generic(layout, context)

        # TODO: pose markers for action edit mode only?


class GRAPH_MT_channel(Menu):
    bl_label = "Channel"

    def draw(self, context):
        layout = self.layout
        operator_context = layout.operator_context
        layout.operator_context = 'INVOKE_REGION_CHANNELS'

        layout.operator("anim.channels_delete")

        if context.space_data.mode == 'DRIVERS':
            layout.operator("graph.driver_delete_invalid")

        layout.separator()
        layout.operator("anim.channels_group")
        layout.operator("anim.channels_ungroup")

        layout.separator()
        layout.operator_menu_enum("anim.channels_setting_toggle", "type")
        layout.operator_menu_enum("anim.channels_setting_enable", "type")
        layout.operator_menu_enum("anim.channels_setting_disable", "type")

        layout.separator()
        layout.operator("anim.channels_editable_toggle")
        layout.operator_menu_enum("graph.extrapolation_type", "type", text="Extrapolation Mode")
        # To get it to display the hotkey.
        layout.operator_context = operator_context
        layout.operator_menu_enum("graph.fmodifier_add", "type").only_active = False
        layout.operator_context = 'INVOKE_REGION_CHANNELS'

        layout.separator()
        layout.operator("graph.hide", text="Hide Selected Curves").unselected = False
        layout.operator("graph.hide", text="Hide Unselected Curves").unselected = True
        layout.operator("graph.reveal")

        layout.separator()
        layout.operator("anim.channels_expand")
        layout.operator("anim.channels_collapse")

        layout.separator()
        layout.operator_menu_enum("anim.channels_move", "direction", text="Move...")

        layout.separator()
        layout.operator("anim.channels_fcurves_enable")

        layout.separator()
        layout.operator("graph.keys_to_samples")
        layout.operator("graph.samples_to_keys")
        layout.operator("graph.sound_to_samples")
        layout.operator("anim.channels_bake")

        layout.separator()
        layout.operator("graph.euler_filter", text="Discontinuity (Euler) Filter")

        layout.separator()
        layout.operator("anim.channels_view_selected")


class GRAPH_MT_key_density(Menu):
    bl_label = "Density"

    def draw(self, _context):
        from _bl_ui_utils.layout import operator_context
        layout = self.layout
        layout.operator("graph.decimate", text="Decimate (Ratio)").mode = 'RATIO'
        # Using the modal operation doesn't make sense for this variant
        # as we do not have a modal mode for it, so just execute it.
        with operator_context(layout, 'EXEC_REGION_WIN'):
            layout.operator("graph.decimate", text="Decimate (Allowed Change)").mode = 'ERROR'
        layout.operator("graph.bake_keys")

        layout.separator()
        layout.operator("graph.clean").channels = False


class GRAPH_MT_key_blending(Menu):
    bl_label = "Blend"
    bl_translation_context = i18n_contexts.operator_default

    def draw(self, _context):
        layout = self.layout
        layout.operator_context = 'INVOKE_DEFAULT'
        layout.operator("graph.breakdown", text="Breakdown")
        layout.operator("graph.blend_to_neighbor", text="Blend to Neighbor")
        layout.operator("graph.blend_to_default", text="Blend to Default Value")
        layout.operator("graph.ease", text="Ease")
        layout.operator("graph.blend_offset", text="Blend Offset")
        layout.operator("graph.blend_to_ease", text="Blend to Ease")
        layout.operator("graph.match_slope", text="Match Slope")
        layout.operator("graph.push_pull", text="Push Pull")
        layout.operator("graph.shear", text="Shear Keys")
        layout.operator("graph.scale_average", text="Scale Average")
        layout.operator("graph.scale_from_neighbor", text="Scale from Neighbor")
        layout.operator("graph.time_offset", text="Time Offset")


class GRAPH_MT_key_smoothing(Menu):
    bl_label = "Smooth"
    bl_translation_context = i18n_contexts.operator_default

    def draw(self, _context):
        layout = self.layout
        layout.operator_context = 'INVOKE_DEFAULT'
        layout.operator("graph.gaussian_smooth", text="Smooth (Gaussian)")
        layout.operator("graph.smooth", text="Smooth (Legacy)")
        layout.operator("graph.butterworth_smooth")


class GRAPH_MT_key(Menu):
    bl_label = "Key"

    def draw(self, _context):
        layout = self.layout

        layout.menu("GRAPH_MT_key_transform", text="Transform")
        layout.menu("GRAPH_MT_key_snap", text="Snap")
        layout.operator_menu_enum("graph.mirror", "type", text="Mirror")

        layout.separator()
        layout.operator("graph.frame_jump", text="Jump to Selected")

        layout.separator()
        layout.operator_menu_enum("graph.keyframe_insert", "type", text="Insert")
        layout.operator("graph.copy", text="Copy")
        layout.operator("graph.paste", text="Paste")
        layout.operator("graph.paste", text="Paste Flipped").flipped = True
        layout.operator("graph.duplicate_move")
        layout.operator("graph.delete", text="Delete")

        layout.separator()
        layout.operator_menu_enum("graph.handle_type", "type", text="Handle Type")
        layout.operator_menu_enum("graph.interpolation_type", "type", text="Interpolation Mode")
        layout.operator_menu_enum("graph.easing_type", "type", text="Easing Type")

        layout.separator()

        layout.menu("GRAPH_MT_key_density")
        layout.menu("GRAPH_MT_key_blending")
        layout.menu("GRAPH_MT_key_smoothing")


class GRAPH_MT_key_transform(Menu):
    bl_label = "Transform"

    def draw(self, _context):
        layout = self.layout

        layout.operator("transform.translate", text="Move")
        layout.operator("transform.transform", text="Extend").mode = 'TIME_EXTEND'
        layout.operator("transform.rotate", text="Rotate")
        layout.operator("transform.resize", text="Scale")


class GRAPH_MT_key_snap(Menu):
    bl_label = "Snap"

    def draw(self, _context):
        layout = self.layout

        layout.operator("graph.snap", text="Selection to Current Frame").type = 'CFRA'
        layout.operator("graph.snap", text="Selection to Cursor Value").type = 'VALUE'
        layout.operator("graph.snap", text="Selection to Nearest Frame").type = 'NEAREST_FRAME'
        layout.operator("graph.snap", text="Selection to Nearest Second").type = 'NEAREST_SECOND'
        layout.operator("graph.snap", text="Selection to Nearest Marker").type = 'NEAREST_MARKER'
        layout.operator("graph.snap", text="Flatten Handles").type = 'HORIZONTAL'
        layout.operator("graph.equalize_handles", text="Equalize Handles").side = 'BOTH'
        layout.separator()
        layout.operator("graph.frame_jump", text="Cursor to Selection")
        layout.operator("graph.snap_cursor_value", text="Cursor Value to Selection")


class GRAPH_MT_view_pie(Menu):
    bl_label = "View"

    def draw(self, context):
        layout = self.layout

        pie = layout.menu_pie()
        pie.operator("graph.view_all")
        pie.operator("graph.view_selected", icon='ZOOM_SELECTED')
        pie.operator("graph.view_frame")
        if context.scene.use_preview_range:
            pie.operator("anim.scene_range_frame", text="Frame Preview Range")
        else:
            pie.operator("anim.scene_range_frame", text="Frame Scene Range")


class GRAPH_MT_delete(Menu):
    bl_label = "Delete"

    def draw(self, _context):
        layout = self.layout

        layout.operator("graph.delete")

        layout.separator()

        layout.operator("graph.clean").channels = False
        layout.operator("graph.clean", text="Clean Channels").channels = True


class GRAPH_MT_context_menu(Menu):
    bl_label = "F-Curve"

    def draw(self, _context):
        layout = self.layout

        layout.operator_context = 'INVOKE_DEFAULT'

        layout.operator("graph.copy", text="Copy", icon='COPYDOWN')
        layout.operator("graph.paste", text="Paste", icon='PASTEDOWN')
        layout.operator("graph.paste", text="Paste Flipped", icon='PASTEFLIPDOWN').flipped = True

        layout.separator()

        layout.operator_menu_enum("graph.handle_type", "type", text="Handle Type")
        layout.operator_menu_enum("graph.interpolation_type", "type", text="Interpolation Mode")
        layout.operator_menu_enum("graph.easing_type", "type", text="Easing Type")

        layout.separator()

        layout.operator("graph.keyframe_insert").type = 'SEL'
        layout.operator("graph.duplicate_move")
        layout.operator_context = 'EXEC_REGION_WIN'
        layout.operator("graph.delete")

        layout.separator()

        layout.operator_menu_enum("graph.mirror", "type", text="Mirror")
        layout.operator_menu_enum("graph.snap", "type", text="Snap")


class GRAPH_MT_pivot_pie(Menu):
    bl_label = "Pivot Point"

    def draw(self, context):
        layout = self.layout
        pie = layout.menu_pie()

        pie.prop_enum(context.space_data, "pivot_point", value='BOUNDING_BOX_CENTER')
        pie.prop_enum(context.space_data, "pivot_point", value='CURSOR')
        pie.prop_enum(context.space_data, "pivot_point", value='INDIVIDUAL_ORIGINS')


class GRAPH_MT_snap_pie(Menu):
    bl_label = "Snap"

    def draw(self, _context):
        layout = self.layout
        pie = layout.menu_pie()

        pie.operator("graph.snap", text="Selection to Current Frame").type = 'CFRA'
        pie.operator("graph.snap", text="Selection to Cursor Value").type = 'VALUE'
        pie.operator("graph.snap", text="Selection to Nearest Frame").type = 'NEAREST_FRAME'
        pie.operator("graph.snap", text="Selection to Nearest Second").type = 'NEAREST_SECOND'
        pie.operator("graph.snap", text="Selection to Nearest Marker").type = 'NEAREST_MARKER'
        pie.operator("graph.snap", text="Flatten Handles").type = 'HORIZONTAL'
        pie.operator("graph.frame_jump", text="Cursor to Selection")
        pie.operator("graph.snap_cursor_value", text="Cursor Value to Selection")


classes = (
    GRAPH_HT_header,
    GRAPH_HT_playback_controls,
    GRAPH_PT_proportional_edit,
    GRAPH_MT_editor_menus,
    GRAPH_MT_view,
    GRAPH_MT_select,
    GRAPH_MT_marker,
    GRAPH_MT_channel,
    GRAPH_MT_key,
    GRAPH_MT_key_density,
    GRAPH_MT_key_transform,
    GRAPH_MT_key_snap,
    GRAPH_MT_key_smoothing,
    GRAPH_MT_key_blending,
    GRAPH_MT_delete,
    GRAPH_MT_context_menu,
    GRAPH_MT_pivot_pie,
    GRAPH_MT_snap_pie,
    GRAPH_MT_view_pie,
    GRAPH_PT_filters,
    GRAPH_PT_snapping,
    GRAPH_PT_driver_snapping,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
