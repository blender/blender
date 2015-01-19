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


class OUTLINER_HT_header(Header):
    bl_space_type = 'OUTLINER'

    def draw(self, context):
        layout = self.layout

        space = context.space_data
        scene = context.scene
        ks = context.scene.keying_sets.active

        row = layout.row(align=True)
        row.template_header()

        OUTLINER_MT_editor_menus.draw_collapsible(context, layout)

        layout.prop(space, "display_mode", text="")

        layout.prop(space, "filter_text", icon='VIEWZOOM', text="")

        layout.separator()

        if space.display_mode == 'DATABLOCKS':
            row = layout.row(align=True)
            row.operator("outliner.keyingset_add_selected", icon='ZOOMIN', text="")
            row.operator("outliner.keyingset_remove_selected", icon='ZOOMOUT', text="")

            if ks:
                row = layout.row()
                row.prop_search(scene.keying_sets, "active", scene, "keying_sets", text="")

                row = layout.row(align=True)
                row.operator("anim.keyframe_insert", text="", icon='KEY_HLT')
                row.operator("anim.keyframe_delete", text="", icon='KEY_DEHLT')
            else:
                row = layout.row()
                row.label(text="No Keying Set active")


class OUTLINER_MT_editor_menus(Menu):
    bl_idname = "OUTLINER_MT_editor_menus"
    bl_label = ""

    def draw(self, context):
        self.draw_menus(self.layout, context)

    @staticmethod
    def draw_menus(layout, context):
        space = context.space_data

        layout.menu("OUTLINER_MT_view")
        layout.menu("OUTLINER_MT_search")

        if space.display_mode == 'DATABLOCKS':
            layout.menu("OUTLINER_MT_edit_datablocks")


class OUTLINER_MT_view(Menu):
    bl_label = "View"

    def draw(self, context):
        layout = self.layout

        space = context.space_data

        if space.display_mode not in {'DATABLOCKS', 'USER_PREFERENCES', 'KEYMAPS'}:
            layout.prop(space, "use_sort_alpha")
            layout.prop(space, "show_restrict_columns")
            layout.separator()
            layout.operator("outliner.show_active")

        layout.operator("outliner.show_one_level")
        layout.operator("outliner.show_hierarchy")

        layout.separator()

        layout.operator("screen.area_dupli")
        layout.operator("screen.screen_full_area", text="Toggle Maximize Area")
        layout.operator("screen.screen_full_area").use_hide_panels = True


class OUTLINER_MT_search(Menu):
    bl_label = "Search"

    def draw(self, context):
        layout = self.layout

        space = context.space_data

        layout.prop(space, "use_filter_case_sensitive")
        layout.prop(space, "use_filter_complete")


class OUTLINER_MT_edit_datablocks(Menu):
    bl_label = "Edit"

    def draw(self, context):
        layout = self.layout

        layout.operator("outliner.keyingset_add_selected")
        layout.operator("outliner.keyingset_remove_selected")

        layout.separator()

        layout.operator("outliner.drivers_add_selected")
        layout.operator("outliner.drivers_delete_selected")

if __name__ == "__main__":  # only for live edit.
    bpy.utils.register_module(__name__)
