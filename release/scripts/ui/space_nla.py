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


class NLA_HT_header(bpy.types.Header):
    bl_space_type = 'NLA_EDITOR'

    def draw(self, context):
        layout = self.layout

        st = context.space_data

        row = layout.row(align=True)
        row.template_header()

        if context.area.show_menus:
            sub = row.row(align=True)

            sub.menu("NLA_MT_view")
            sub.menu("NLA_MT_select")
            sub.menu("NLA_MT_edit")
            sub.menu("NLA_MT_add")

        layout.template_dopesheet_filter(st.dopesheet)

        layout.prop(st, "autosnap", text="")


class NLA_MT_view(bpy.types.Menu):
    bl_label = "View"

    def draw(self, context):
        layout = self.layout

        st = context.space_data

        layout.column()

        layout.operator("nla.properties", icon="ICON_MENU_PANEL")

        layout.separator()
        layout.prop(st, "show_cframe_indicator")

        if st.show_seconds:
            layout.operator("anim.time_toggle", text="Show Frames")
        else:
            layout.operator("anim.time_toggle", text="Show Seconds")

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


class NLA_MT_edit(bpy.types.Menu):
    bl_label = "Edit"

    def draw(self, context):
        layout = self.layout

        layout.column()
        layout.menu("NLA_MT_edit_transform", text="Transform")

        layout.operator_menu_enum("nla.snap", property="type", text="Snap")

        layout.separator()
        layout.operator("nla.duplicate")
        layout.operator("nla.split")
        layout.operator("nla.delete")

        layout.separator()
        layout.operator("nla.mute_toggle")

        layout.separator()
        layout.operator("nla.apply_scale")
        layout.operator("nla.clear_scale")

        layout.separator()
        layout.operator("nla.move_up")
        layout.operator("nla.move_down")

        """
        XXX not sure if we need this check and so scene.flag wrapped?
    // TODO: names of these tools for 'tweakmode' need changing?
    if (scene->flag & SCE_NLA_EDIT_ON)
        uiItemO(layout, "Stop Tweaking Strip Actions", 0, "NLA_OT_tweakmode_exit");
    else
        uiItemO(layout, "Start Tweaking Strip Actions", 0, "NLA_OT_tweakmode_enter");
        """
        layout.separator()
        layout.operator("nla.tweakmode_exit")
        layout.operator("nla.tweakmode_enter")


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
        layout.operator("tfm.translate", text="Grab/Move")
        layout.operator("tfm.transform", text="Extend").mode = 'TIME_EXTEND'
        layout.operator("tfm.resize", text="Scale")


bpy.types.register(NLA_HT_header) # header/menu classes
bpy.types.register(NLA_MT_view)
bpy.types.register(NLA_MT_select)
bpy.types.register(NLA_MT_edit)
bpy.types.register(NLA_MT_add)
bpy.types.register(NLA_MT_edit_transform)
