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
#  Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
# ##### END GPL LICENSE BLOCK #####

# <pep8 compliant>

import bpy


class DOPESHEET_HT_header(bpy.types.Header):
    bl_space_type = 'DOPESHEET_EDITOR'

    def draw(self, context):
        layout = self.layout

        st = context.space_data

        row = layout.row(align=True)
        row.template_header()

        if context.area.show_menus:
            sub = row.row(align=True)

            sub.menu("DOPESHEET_MT_view")
            sub.menu("DOPESHEET_MT_select")

            if st.mode == 'DOPESHEET' or (st.mode == 'ACTION' and st.action != None):
                sub.menu("DOPESHEET_MT_channel")
            elif st.mode == 'GPENCIL':
                # gpencil Channel menu
                pass

            if st.mode != 'GPENCIL':
                sub.menu("DOPESHEET_MT_key")

        layout.prop(st, "mode", text="")
        layout.prop(st.dopesheet, "display_summary", text="Summary")

        if st.mode == 'DOPESHEET':
            layout.template_dopesheet_filter(st.dopesheet)
        elif st.mode == 'ACTION':
            layout.template_ID(st, "action", new="action.new")

        if st.mode != 'GPENCIL':
            layout.prop(st, "autosnap", text="")

        row = layout.row(align=True)
        row.operator("action.copy", text="", icon='COPYDOWN')
        row.operator("action.paste", text="", icon='PASTEDOWN')


class DOPESHEET_MT_view(bpy.types.Menu):
    bl_label = "View"

    def draw(self, context):
        layout = self.layout

        st = context.space_data

        layout.column()

        layout.prop(st, "realtime_updates")
        layout.prop(st, "show_cframe_indicator")
        layout.prop(st, "show_sliders")
        layout.prop(st, "automerge_keyframes")

        if st.show_seconds:
            layout.operator("anim.time_toggle", text="Show Frames")
        else:
            layout.operator("anim.time_toggle", text="Show Seconds")

        layout.separator()
        layout.operator("anim.previewrange_set")
        layout.operator("anim.previewrange_clear")
        layout.operator("action.previewrange_set")

        layout.separator()
        layout.operator("action.frame_jump")
        layout.operator("action.view_all")

        layout.separator()
        layout.operator("screen.area_dupli")
        layout.operator("screen.screen_full_area")


class DOPESHEET_MT_select(bpy.types.Menu):
    bl_label = "Select"

    def draw(self, context):
        layout = self.layout

        layout.column()
        # This is a bit misleading as the operator's default text is "Select All" while it actually *toggles* All/None
        layout.operator("action.select_all_toggle")
        layout.operator("action.select_all_toggle", text="Invert Selection").invert = True

        layout.separator()
        layout.operator("action.select_border")
        layout.operator("action.select_border", text="Border Axis Range").axis_range = True

        layout.separator()
        layout.operator("action.select_column", text="Columns on Selected Keys").mode = 'KEYS'
        layout.operator("action.select_column", text="Column on Current Frame").mode = 'CFRA'

        layout.operator("action.select_column", text="Columns on Selected Markers").mode = 'MARKERS_COLUMN'
        layout.operator("action.select_column", text="Between Selected Markers").mode = 'MARKERS_BETWEEN'

        layout.separator()
        layout.operator("action.select_more")
        layout.operator("action.select_less")


class DOPESHEET_MT_channel(bpy.types.Menu):
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

        layout.separator()
        layout.operator("anim.channels_expand")
        layout.operator("anim.channels_collapse")


class DOPESHEET_MT_key(bpy.types.Menu):
    bl_label = "Key"

    def draw(self, context):
        layout = self.layout

        layout.column()
        layout.menu("DOPESHEET_MT_key_transform", text="Transform")

        layout.operator_menu_enum("action.snap", property="type", text="Snap")
        layout.operator_menu_enum("action.mirror", property="type", text="Mirror")

        layout.separator()
        layout.operator("action.keyframe_insert")

        layout.separator()
        layout.operator("action.duplicate")
        layout.operator("action.delete")

        layout.separator()
        layout.operator_menu_enum("action.keyframe_type", property="type", text="Keyframe Type")
        layout.operator_menu_enum("action.handle_type", property="type", text="Handle Type")
        layout.operator_menu_enum("action.interpolation_type", property="type", text="Interpolation Mode")
        layout.operator_menu_enum("action.extrapolation_type", property="type", text="Extrapolation Mode")

        layout.separator()
        layout.operator("action.clean")
        layout.operator("action.sample")

        layout.separator()
        layout.operator("action.copy")
        layout.operator("action.paste")


class DOPESHEET_MT_key_transform(bpy.types.Menu):
    bl_label = "Transform"

    def draw(self, context):
        layout = self.layout

        layout.column()
        layout.operator("transform.translate", text="Grab/Move")
        layout.operator("transform.transform", text="Extend").mode = 'TIME_EXTEND'
        layout.operator("transform.resize", text="Scale")


bpy.types.register(DOPESHEET_HT_header) # header/menu classes
bpy.types.register(DOPESHEET_MT_view)
bpy.types.register(DOPESHEET_MT_select)
bpy.types.register(DOPESHEET_MT_channel)
bpy.types.register(DOPESHEET_MT_key)
bpy.types.register(DOPESHEET_MT_key_transform)
