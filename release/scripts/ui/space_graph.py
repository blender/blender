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


class GRAPH_HT_header(bpy.types.Header):
    bl_space_type = 'GRAPH_EDITOR'

    def draw(self, context):
        layout = self.layout

        st = context.space_data

        row = layout.row(align=True)
        row.template_header()

        if context.area.show_menus:
            sub = row.row(align=True)

            sub.menu("GRAPH_MT_view")
            sub.menu("GRAPH_MT_select")
            sub.menu("GRAPH_MT_channel")
            sub.menu("GRAPH_MT_key")

        layout.prop(st, "mode", text="")

        layout.template_dopesheet_filter(st.dopesheet)

        layout.prop(st, "autosnap", text="")
        layout.prop(st, "pivot_point", text="", icon_only=True)

        row = layout.row(align=True)
        row.operator("graph.copy", text="", icon='COPYDOWN')
        row.operator("graph.paste", text="", icon='PASTEDOWN')

        row = layout.row(align=True)
        if st.has_ghost_curves:
            row.operator("graph.ghost_curves_clear", text="", icon='GHOST_DISABLED')
        else:
            row.operator("graph.ghost_curves_create", text="", icon='GHOST_ENABLED')


class GRAPH_MT_view(bpy.types.Menu):
    bl_label = "View"

    def draw(self, context):
        layout = self.layout

        st = context.space_data

        layout.column()

        layout.operator("graph.properties", icon='MENU_PANEL')
        layout.separator()

        layout.prop(st, "realtime_updates")
        layout.prop(st, "show_cframe_indicator")
        layout.prop(st, "show_cursor")
        layout.prop(st, "show_sliders")
        layout.prop(st, "automerge_keyframes")

        layout.separator()
        if st.show_handles:
            layout.operator("graph.handles_view_toggle", icon='CHECKBOX_HLT', text="Show All Handles")
        else:
            layout.operator("graph.handles_view_toggle", icon='CHECKBOX_DEHLT', text="Show All Handles")
        layout.prop(st, "only_selected_curves_handles")
        layout.prop(st, "only_selected_keyframe_handles")
        layout.operator("anim.time_toggle")

        layout.separator()
        layout.operator("anim.previewrange_set")
        layout.operator("anim.previewrange_clear")
        layout.operator("graph.previewrange_set")

        layout.separator()
        layout.operator("graph.frame_jump")
        layout.operator("graph.view_all")

        layout.separator()
        layout.operator("screen.area_dupli")
        layout.operator("screen.screen_full_area")


class GRAPH_MT_select(bpy.types.Menu):
    bl_label = "Select"

    def draw(self, context):
        layout = self.layout

        layout.column()
        # This is a bit misleading as the operator's default text is "Select All" while it actually *toggles* All/None
        layout.operator("graph.select_all_toggle")
        layout.operator("graph.select_all_toggle", text="Invert Selection").invert = True

        layout.separator()
        layout.operator("graph.select_border")
        layout.operator("graph.select_border", text="Border Axis Range").axis_range = True

        layout.separator()
        layout.operator("graph.select_column", text="Columns on Selected Keys").mode = 'KEYS'
        layout.operator("graph.select_column", text="Column on Current Frame").mode = 'CFRA'

        layout.operator("graph.select_column", text="Columns on Selected Markers").mode = 'MARKERS_COLUMN'
        layout.operator("graph.select_column", text="Between Selected Markers").mode = 'MARKERS_BETWEEN'

        layout.separator()
        layout.operator("graph.select_more")
        layout.operator("graph.select_less")


class GRAPH_MT_channel(bpy.types.Menu):
    bl_label = "Channel"

    def draw(self, context):
        layout = self.layout

        layout.operator_context = 'INVOKE_REGION_CHANNELS'

        layout.column()
        layout.operator("anim.channels_setting_toggle")
        layout.operator("anim.channels_setting_enable")
        layout.operator("anim.channels_setting_disable")

        layout.separator()
        layout.operator("anim.channels_editable_toggle")
        layout.operator("anim.channels_visibility_set")

        layout.separator()
        layout.operator("anim.channels_expand")
        layout.operator("anim.channels_collapse")

        layout.separator()
        layout.operator("graph.euler_filter", text="Discontinuity (Euler) Filter")


class GRAPH_MT_key(bpy.types.Menu):
    bl_label = "Key"

    def draw(self, context):
        layout = self.layout

        layout.column()
        layout.menu("GRAPH_MT_key_transform", text="Transform")

        layout.operator_menu_enum("graph.snap", property="type", text="Snap")
        layout.operator_menu_enum("graph.mirror", property="type", text="Mirror")

        layout.separator()
        layout.operator("graph.keyframe_insert")
        layout.operator("graph.fmodifier_add")

        layout.separator()
        layout.operator("graph.duplicate")
        layout.operator("graph.delete")

        layout.separator()
        layout.operator_menu_enum("graph.handle_type", property="type", text="Handle Type")
        layout.operator_menu_enum("graph.interpolation_type", property="type", text="Interpolation Mode")
        layout.operator_menu_enum("graph.extrapolation_type", property="type", text="Extrapolation Mode")

        layout.separator()
        layout.operator("graph.clean")
        layout.operator("graph.sample")
        layout.operator("graph.bake")

        layout.separator()
        layout.operator("graph.copy")
        layout.operator("graph.paste")


class GRAPH_MT_key_transform(bpy.types.Menu):
    bl_label = "Transform"

    def draw(self, context):
        layout = self.layout

        layout.column()
        layout.operator("transform.translate", text="Grab/Move")
        layout.operator("transform.transform", text="Extend").mode = 'TIME_EXTEND'
        layout.operator("transform.rotate", text="Rotate")
        layout.operator("transform.resize", text="Scale")


classes = [
    GRAPH_HT_header, # header/menu classes
    GRAPH_MT_view,
    GRAPH_MT_select,
    GRAPH_MT_channel,
    GRAPH_MT_key,
    GRAPH_MT_key_transform]


def register():
    register = bpy.types.register
    for cls in classes:
        register(cls)


def unregister():
    unregister = bpy.types.unregister
    for cls in classes:
        unregister(cls)

if __name__ == "__main__":
    register()
