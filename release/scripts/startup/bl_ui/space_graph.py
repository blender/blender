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
from bpy.types import Header, Menu


class GRAPH_HT_header(Header):
    bl_space_type = 'GRAPH_EDITOR'

    def draw(self, context):
        from bl_ui.space_dopesheet import dopesheet_filter

        layout = self.layout

        st = context.space_data

        row = layout.row(align=True)
        row.template_header()

        GRAPH_MT_editor_menus.draw_collapsible(context, layout)

        layout.prop(st, "mode", text="")

        dopesheet_filter(layout, context)

        layout.prop(st, "use_normalization", text="Normalize")
        row = layout.row()
        row.active = st.use_normalization
        row.prop(st, "use_auto_normalization", text="Auto")

        layout.prop(st, "auto_snap", text="")
        layout.prop(st, "pivot_point", icon_only=True)

        row = layout.row(align=True)
        row.operator("graph.copy", text="", icon='COPYDOWN')
        row.operator("graph.paste", text="", icon='PASTEDOWN')

        row = layout.row(align=True)
        if st.has_ghost_curves:
            row.operator("graph.ghost_curves_clear", text="", icon='GHOST_DISABLED')
        else:
            row.operator("graph.ghost_curves_create", text="", icon='GHOST_ENABLED')


class GRAPH_MT_editor_menus(Menu):
    bl_idname = "GRAPH_MT_editor_menus"
    bl_label = ""

    def draw(self, context):
        self.draw_menus(self.layout, context)

    @staticmethod
    def draw_menus(layout, context):
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

        layout.operator("graph.properties", icon='MENU_PANEL')
        layout.separator()

        layout.prop(st, "use_realtime_update")
        layout.prop(st, "show_frame_indicator")
        layout.prop(st, "show_cursor")
        layout.prop(st, "show_sliders")
        layout.prop(st, "show_group_colors")
        layout.prop(st, "use_auto_merge_keyframes")

        layout.separator()
        layout.prop(st, "use_beauty_drawing")

        layout.separator()

        layout.prop(st, "show_handles")

        layout.prop(st, "use_only_selected_curves_handles")
        layout.prop(st, "use_only_selected_keyframe_handles")

        layout.prop(st, "show_seconds")

        layout.separator()
        layout.operator("anim.previewrange_set")
        layout.operator("anim.previewrange_clear")
        layout.operator("graph.previewrange_set")

        layout.separator()
        layout.operator("graph.view_all")
        layout.operator("graph.view_selected")

        layout.separator()
        layout.operator("screen.area_dupli")
        layout.operator("screen.screen_full_area")


class GRAPH_MT_select(Menu):
    bl_label = "Select"

    def draw(self, context):
        layout = self.layout

        # This is a bit misleading as the operator's default text is "Select All" while it actually *toggles* All/None
        layout.operator("graph.select_all_toggle").invert = False
        layout.operator("graph.select_all_toggle", text="Invert Selection").invert = True

        layout.separator()
        layout.operator("graph.select_border")
        layout.operator("graph.select_border", text="Border Axis Range").axis_range = True
        layout.operator("graph.select_border", text="Border (Include Handles)").include_handles = True

        layout.separator()
        layout.operator("graph.select_column", text="Columns on Selected Keys").mode = 'KEYS'
        layout.operator("graph.select_column", text="Column on Current Frame").mode = 'CFRA'

        layout.operator("graph.select_column", text="Columns on Selected Markers").mode = 'MARKERS_COLUMN'
        layout.operator("graph.select_column", text="Between Selected Markers").mode = 'MARKERS_BETWEEN'

        layout.separator()
        layout.operator("graph.select_leftright", text="Before Current Frame").mode = 'LEFT'
        layout.operator("graph.select_leftright", text="After Current Frame").mode = 'RIGHT'

        layout.separator()
        layout.operator("graph.select_more")
        layout.operator("graph.select_less")

        layout.separator()
        layout.operator("graph.select_linked")


class GRAPH_MT_marker(Menu):
    bl_label = "Marker"

    def draw(self, context):
        layout = self.layout

        from bl_ui.space_time import marker_menu_generic
        marker_menu_generic(layout)

        # TODO: pose markers for action edit mode only?


class GRAPH_MT_channel(Menu):
    bl_label = "Channel"

    def draw(self, context):
        layout = self.layout

        layout.operator_context = 'INVOKE_REGION_CHANNELS'

        layout.operator("anim.channels_delete")

        layout.separator()
        layout.operator("anim.channels_group")
        layout.operator("anim.channels_ungroup")

        layout.separator()
        layout.operator_menu_enum("anim.channels_setting_toggle", "type")
        layout.operator_menu_enum("anim.channels_setting_enable", "type")
        layout.operator_menu_enum("anim.channels_setting_disable", "type")

        layout.separator()
        layout.operator("anim.channels_editable_toggle")
        layout.operator("anim.channels_visibility_set")
        layout.operator_menu_enum("graph.extrapolation_type", "type", text="Extrapolation Mode")

        layout.separator()
        layout.operator("anim.channels_expand")
        layout.operator("anim.channels_collapse")

        layout.separator()
        layout.operator_menu_enum("anim.channels_move", "direction", text="Move...")

        layout.separator()
        layout.operator("anim.channels_fcurves_enable")


class GRAPH_MT_key(Menu):
    bl_label = "Key"

    def draw(self, context):
        layout = self.layout

        layout.menu("GRAPH_MT_key_transform", text="Transform")

        layout.operator_menu_enum("graph.snap", "type", text="Snap")
        layout.operator_menu_enum("graph.mirror", "type", text="Mirror")

        layout.separator()
        layout.operator("graph.keyframe_insert")
        layout.operator("graph.fmodifier_add")
        layout.operator("graph.sound_bake")

        layout.separator()
        layout.operator("graph.frame_jump")

        layout.separator()
        layout.operator("graph.duplicate_move")
        layout.operator("graph.delete")

        layout.separator()
        layout.operator_menu_enum("graph.handle_type", "type", text="Handle Type")
        layout.operator_menu_enum("graph.interpolation_type", "type", text="Interpolation Mode")

        layout.separator()
        layout.operator("graph.clean")
        layout.operator("graph.smooth")
        layout.operator("graph.sample")
        layout.operator("graph.bake")

        layout.separator()
        layout.operator("graph.copy")
        layout.operator("graph.paste")

        layout.separator()
        layout.operator("graph.euler_filter", text="Discontinuity (Euler) Filter")


class GRAPH_MT_key_transform(Menu):
    bl_label = "Transform"

    def draw(self, context):
        layout = self.layout

        layout.operator("transform.translate", text="Grab/Move")
        layout.operator("transform.transform", text="Extend").mode = 'TIME_EXTEND'
        layout.operator("transform.rotate", text="Rotate")
        layout.operator("transform.resize", text="Scale")

if __name__ == "__main__":  # only for live edit.
    bpy.utils.register_module(__name__)
