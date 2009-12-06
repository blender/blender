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


class OUTLINER_HT_header(bpy.types.Header):
    bl_space_type = 'OUTLINER'

    def draw(self, context):
        layout = self.layout

        space = context.space_data
        scene = context.scene
        ks = context.scene.active_keying_set

        row = layout.row(align=True)
        row.template_header()

        if context.area.show_menus:
            sub = row.row(align=True)
            sub.menu("OUTLINER_MT_view")
            if space.display_mode == 'DATABLOCKS':
                sub.menu("OUTLINER_MT_edit_datablocks")

        layout.prop(space, "display_mode", text="")

        layout.separator()

        if space.display_mode == 'DATABLOCKS':
            row = layout.row(align=True)
            row.operator("outliner.keyingset_add_selected", icon='ICON_ZOOMIN', text="")
            row.operator("outliner.keyingset_remove_selected", icon='ICON_ZOOMOUT', text="")

            if ks:
                row = layout.row(align=False)
                row.prop_object(scene, "active_keying_set", scene, "keying_sets", text="")

                row = layout.row(align=True)
                row.operator("anim.keyframe_insert", text="", icon='ICON_KEY_HLT')
                row.operator("anim.keyframe_delete", text="", icon='ICON_KEY_DEHLT')
            else:
                row = layout.row(align=False)
                row.label(text="No Keying Set active")


class OUTLINER_MT_view(bpy.types.Menu):
    bl_label = "View"

    def draw(self, context):
        layout = self.layout

        space = context.space_data

        col = layout.column()
        if space.display_mode not in ('DATABLOCKS', 'USER_PREFERENCES', 'KEYMAPS'):
            col.prop(space, "show_restriction_columns")
            col.separator()
            col.operator("outliner.show_active")

        col.operator("outliner.show_one_level")
        col.operator("outliner.show_hierarchy")

        layout.separator()

        layout.operator("screen.area_dupli")
        layout.operator("screen.screen_full_area")


class OUTLINER_MT_edit_datablocks(bpy.types.Menu):
    bl_label = "Edit"

    def draw(self, context):
        layout = self.layout

        col = layout.column()

        col.operator("outliner.keyingset_add_selected")
        col.operator("outliner.keyingset_remove_selected")

        col.separator()

        col.operator("outliner.drivers_add_selected")
        col.operator("outliner.drivers_delete_selected")

bpy.types.register(OUTLINER_HT_header)
bpy.types.register(OUTLINER_MT_view)
bpy.types.register(OUTLINER_MT_edit_datablocks)
