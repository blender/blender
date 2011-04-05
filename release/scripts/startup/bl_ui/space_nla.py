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


class NLA_HT_header(bpy.types.Header):
    bl_space_type = 'NLA_EDITOR'

    def draw(self, context):
        from bl_ui.space_dopesheet import dopesheet_filter

        layout = self.layout

        st = context.space_data

        row = layout.row(align=True)
        row.template_header()

        if context.area.show_menus:
            sub = row.row(align=True)

            sub.menu("NLA_MT_view")
            sub.menu("NLA_MT_select")
            sub.menu("NLA_MT_marker")
            sub.menu("NLA_MT_edit")
            sub.menu("NLA_MT_add")

        dopesheet_filter(layout, context)

        layout.prop(st, "auto_snap", text="")


class NLA_MT_view(bpy.types.Menu):
    bl_label = "View"

    def draw(self, context):
        layout = self.layout

        st = context.space_data

        layout.column()

        layout.operator("nla.properties", icon='MENU_PANEL')

        layout.separator()

        layout.prop(st, "use_realtime_update")
        layout.prop(st, "show_frame_indicator")

        layout.operator("anim.time_toggle", text="Show Frames" if st.show_seconds else "Show Seconds")

        layout.prop(st, "show_strip_curves")

        layout.separator()
        layout.operator("anim.previewrange_set")
        layout.operator("anim.previewrange_clear")

        layout.separator()
        layout.operator("screen.area_dupli")
        layout.operator("screen.screen_full_area")


class NLA_MT_select(bpy.types.Menu):
    bl_label = "Select"

    def draw(self, context):
        layout = self.layout

        layout.column()
        # This is a bit misleading as the operator's default text is "Select All" while it actually *toggles* All/None
        layout.operator("nla.select_all_toggle")
        layout.operator("nla.select_all_toggle", text="Invert Selection").invert = True

        layout.separator()
        layout.operator("nla.select_border")
        layout.operator("nla.select_border", text="Border Axis Range").axis_range = True

        layout.separator()
        layout.operator("nla.select_leftright", text="Before Current Frame").mode = 'LEFT'
        layout.operator("nla.select_leftright", text="After Current Frame").mode = 'RIGHT'


class NLA_MT_marker(bpy.types.Menu):
    bl_label = "Marker"

    def draw(self, context):
        layout = self.layout

        #layout.operator_context = 'EXEC_REGION_WIN'

        layout.column()
        layout.operator("marker.add", "Add Marker")
        layout.operator("marker.duplicate", text="Duplicate Marker")
        layout.operator("marker.delete", text="Delete Marker")

        layout.separator()

        layout.operator("marker.rename", text="Rename Marker")
        layout.operator("marker.move", text="Grab/Move Marker")


class NLA_MT_edit(bpy.types.Menu):
    bl_label = "Edit"

    def draw(self, context):
        layout = self.layout

        scene = context.scene

        layout.column()
        layout.menu("NLA_MT_edit_transform", text="Transform")

        layout.operator_menu_enum("nla.snap", "type", text="Snap")

        layout.separator()
        layout.operator("nla.duplicate")
        layout.operator("nla.split")
        layout.operator("nla.delete")

        layout.separator()
        layout.operator("nla.mute_toggle")

        layout.separator()
        layout.operator("nla.apply_scale")
        layout.operator("nla.clear_scale")
        layout.operator("nla.action_sync_length").active = False

        layout.separator()
        layout.operator("nla.swap")
        layout.operator("nla.move_up")
        layout.operator("nla.move_down")

        # TODO: this really belongs more in a "channel" (or better, "track") menu
        layout.separator()
        layout.operator_menu_enum("anim.channels_move", "direction", text="Track Ordering...")

        layout.separator()
        # TODO: names of these tools for 'tweakmode' need changing?
        if scene.is_nla_tweakmode:
            layout.operator("nla.tweakmode_exit", text="Stop Tweaking Strip Actions")
        else:
            layout.operator("nla.tweakmode_enter", text="Start Tweaking Strip Actions")


class NLA_MT_add(bpy.types.Menu):
    bl_label = "Add"

    def draw(self, context):
        layout = self.layout

        layout.column()
        layout.operator("nla.actionclip_add")
        layout.operator("nla.transition_add")

        layout.separator()
        layout.operator("nla.meta_add")
        layout.operator("nla.meta_remove")

        layout.separator()
        layout.operator("nla.tracks_add")
        layout.operator("nla.tracks_add", text="Add Tracks Above Selected").above_selected = True


class NLA_MT_edit_transform(bpy.types.Menu):
    bl_label = "Transform"

    def draw(self, context):
        layout = self.layout

        layout.column()
        layout.operator("transform.translate", text="Grab/Move")
        layout.operator("transform.transform", text="Extend").mode = 'TIME_EXTEND'
        layout.operator("transform.transform", text="Scale").mode= 'TIME_SCALE'

if __name__ == "__main__":  # only for live edit.
    bpy.utils.register_module(__name__)
