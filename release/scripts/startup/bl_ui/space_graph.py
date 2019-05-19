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

from bpy.types import Header, Menu, Panel
from .space_dopesheet import (
    DopesheetFilterPopoverBase,
    dopesheet_filter,
)


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

        layout.prop(st, "auto_snap", text="")

        row = layout.row(align=True)
        row.prop(tool_settings, "use_proportional_fcurve", text="", icon_only=True)
        sub = row.row(align=True)
        sub.active = tool_settings.use_proportional_fcurve
        sub.prop(tool_settings, "proportional_edit_falloff", text="", icon_only=True)


class GRAPH_PT_filters(DopesheetFilterPopoverBase, Panel):
    bl_space_type = 'GRAPH_EDITOR'
    bl_region_type = 'HEADER'
    bl_label = "Filters"

    def draw(self, context):
        layout = self.layout

        DopesheetFilterPopoverBase.draw_generic_filters(context, layout)
        layout.separator()
        DopesheetFilterPopoverBase.draw_search_filters(context, layout)
        layout.separator()
        DopesheetFilterPopoverBase.draw_standard_filters(context, layout)


class GRAPH_MT_editor_menus(Menu):
    bl_idname = "GRAPH_MT_editor_menus"
    bl_label = ""

    def draw(self, _context):
        layout = self.layout
        layout.menu("GRAPH_MT_view")
        layout.menu("GRAPH_MT_select")
        layout.menu("GRAPH_MT_marker")
        layout.menu("GRAPH_MT_channel")
        layout.menu("GRAPH_MT_key")


class GRAPH_MT_view(Menu):
    bl_label = "View"

    def draw(self, context):
        layout = self.layout

        st = context.space_data

        layout.prop(st, "show_region_ui")
        layout.separator()

        layout.prop(st, "use_realtime_update")
        layout.prop(st, "show_frame_indicator")
        layout.prop(st, "show_cursor")
        layout.prop(st, "show_sliders")
        layout.prop(st, "show_group_colors")
        layout.prop(st, "show_marker_lines")
        layout.prop(st, "use_auto_merge_keyframes")

        layout.separator()
        layout.prop(st, "use_beauty_drawing")

        layout.separator()

        layout.prop(st, "show_handles")

        layout.prop(st, "use_only_selected_curves_handles")
        layout.prop(st, "use_only_selected_keyframe_handles")

        layout.prop(st, "show_seconds")
        layout.prop(st, "show_locked_time")

        layout.separator()
        layout.operator("anim.previewrange_set")
        layout.operator("anim.previewrange_clear")
        layout.operator("graph.previewrange_set")

        layout.separator()
        layout.operator("graph.view_all")
        layout.operator("graph.view_selected")
        layout.operator("graph.view_frame")

        # Add this to show key-binding (reverse action in dope-sheet).
        layout.separator()
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

        props = layout.operator("graph.select_box")
        props.axis_range = False
        props.include_handles = False
        props = layout.operator("graph.select_box", text="Border Axis Range")
        props.axis_range = True
        props.include_handles = False
        props = layout.operator("graph.select_box", text="Border (Include Handles)")
        props.axis_range = False
        props.include_handles = True

        layout.operator("graph.select_circle")

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
        layout.operator("graph.select_more")
        layout.operator("graph.select_less")

        layout.separator()
        layout.operator("graph.select_linked")


class GRAPH_MT_marker(Menu):
    bl_label = "Marker"

    def draw(self, context):
        layout = self.layout

        from .space_time import marker_menu_generic
        marker_menu_generic(layout, context)

        # TODO: pose markers for action edit mode only?


class GRAPH_MT_channel(Menu):
    bl_label = "Channel"

    def draw(self, context):
        layout = self.layout

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


class GRAPH_MT_key(Menu):
    bl_label = "Key"

    def draw(self, _context):
        layout = self.layout

        layout.menu("GRAPH_MT_key_transform", text="Transform")

        layout.operator_menu_enum("graph.snap", "type", text="Snap")
        layout.operator_menu_enum("graph.mirror", "type", text="Mirror")

        layout.separator()
        layout.operator_menu_enum("graph.keyframe_insert", "type")
        layout.operator_menu_enum("graph.fmodifier_add", "type")
        layout.operator("graph.sound_bake")

        layout.separator()
        layout.operator("graph.frame_jump")

        layout.separator()
        layout.operator("graph.copy")
        layout.operator("graph.paste")
        layout.operator("graph.paste", text="Paste Flipped").flipped = True
        layout.operator("graph.duplicate_move")
        layout.operator("graph.delete")

        layout.separator()
        layout.operator_menu_enum("graph.handle_type", "type", text="Handle Type")
        layout.operator_menu_enum("graph.interpolation_type", "type", text="Interpolation Mode")
        layout.operator_menu_enum("graph.easing_type", "type", text="Easing Type")

        layout.separator()
        layout.operator("graph.clean").channels = False
        layout.operator("graph.clean", text="Clean Channels").channels = True
        layout.operator("graph.smooth")
        layout.operator("graph.sample")
        layout.operator("graph.bake")

        layout.separator()
        layout.operator("graph.euler_filter", text="Discontinuity (Euler) Filter")


class GRAPH_MT_key_transform(Menu):
    bl_label = "Transform"

    def draw(self, _context):
        layout = self.layout

        layout.operator("transform.translate", text="Move")
        layout.operator("transform.transform", text="Extend").mode = 'TIME_EXTEND'
        layout.operator("transform.rotate", text="Rotate")
        layout.operator("transform.resize", text="Scale")


class GRAPH_MT_delete(Menu):
    bl_label = "Delete"

    def draw(self, _context):
        layout = self.layout

        layout.operator("graph.delete")

        layout.separator()

        layout.operator("graph.clean").channels = False
        layout.operator("graph.clean", text="Clean Channels").channels = True


class GRAPH_MT_context_menu(Menu):
    bl_label = "F-Curve Context Menu"

    def draw(self, _context):
        layout = self.layout

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

        pie.operator("graph.snap", text="Current Frame").type = 'CFRA'
        pie.operator("graph.snap", text="Cursor Value").type = 'VALUE'
        pie.operator("graph.snap", text="Nearest Frame").type = 'NEAREST_FRAME'
        pie.operator("graph.snap", text="Nearest Second").type = 'NEAREST_SECOND'
        pie.operator("graph.snap", text="Nearest Marker").type = 'NEAREST_MARKER'
        pie.operator("graph.snap", text="Flatten Handles").type = 'HORIZONTAL'


class GRAPH_MT_channel_context_menu(Menu):
    bl_label = "F-Curve Channel Context Menu"

    def draw(self, context):
        layout = self.layout
        st = context.space_data

        layout.separator()
        layout.operator("anim.channels_setting_enable", text="Mute Channels").type = 'MUTE'
        layout.operator("anim.channels_setting_disable", text="Unmute Channels").type = 'MUTE'
        layout.separator()
        layout.operator("anim.channels_setting_enable", text="Protect Channels").type = 'PROTECT'
        layout.operator("anim.channels_setting_disable", text="Unprotect Channels").type = 'PROTECT'

        layout.separator()
        layout.operator("anim.channels_group")
        layout.operator("anim.channels_ungroup")

        layout.separator()
        layout.operator("anim.channels_editable_toggle")
        layout.operator_menu_enum("graph.extrapolation_type", "type", text="Extrapolation Mode")

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

        layout.operator("anim.channels_delete")
        if st.mode == 'DRIVERS':
            layout.operator("graph.driver_delete_invalid")


classes = (
    GRAPH_HT_header,
    GRAPH_MT_editor_menus,
    GRAPH_MT_view,
    GRAPH_MT_select,
    GRAPH_MT_marker,
    GRAPH_MT_channel,
    GRAPH_MT_key,
    GRAPH_MT_key_transform,
    GRAPH_MT_delete,
    GRAPH_MT_context_menu,
    GRAPH_MT_channel_context_menu,
    GRAPH_MT_pivot_pie,
    GRAPH_MT_snap_pie,
    GRAPH_PT_filters,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
