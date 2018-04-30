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
from bpy.types import Header, Menu, Panel


class OUTLINER_HT_header(Header):
    bl_space_type = 'OUTLINER'

    def draw(self, context):
        layout = self.layout

        space = context.space_data
        display_mode = space.display_mode
        scene = context.scene
        ks = context.scene.keying_sets.active

        row = layout.row(align=True)
        row.template_header()

        layout.prop(space, "display_mode", text="")

        row = layout.row(align=True)
        if display_mode in {'VIEW_LAYER'}:
            row.popover(space_type='OUTLINER',
                        region_type='HEADER',
                        panel_type="OUTLINER_PT_filter",
                        text="",
                        icon='FILTER')
        elif display_mode in {'LIBRARIES', 'ORPHAN_DATA'}:
            row.prop(space, "use_filter_id_type", text="", icon='FILTER')
            sub = row.row(align=True)
            sub.active = space.use_filter_id_type
            sub.prop(space, "filter_id_type", text="", icon_only=True)

        OUTLINER_MT_editor_menus.draw_collapsible(context, layout)

        if space.display_mode == 'DATA_API':
            layout.separator()

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
                row.label(text="No Keying Set Active")

        row = layout.row(align=True)
        row.prop(space, "use_filter_search", text="")
        if space.use_filter_search:
            row.prop(space, "filter_text", text="")
            row.prop(space, "use_filter_complete", text="")
            row.prop(space, "use_filter_case_sensitive", text="")


class OUTLINER_MT_editor_menus(Menu):
    bl_idname = "OUTLINER_MT_editor_menus"
    bl_label = ""

    def draw(self, context):
        self.draw_menus(self.layout, context)

    @staticmethod
    def draw_menus(layout, context):
        space = context.space_data

        layout.menu("OUTLINER_MT_view")

        if space.display_mode == 'DATA_API':
            layout.menu("OUTLINER_MT_edit_datablocks")

        elif space.display_mode == 'ORPHAN_DATA':
            layout.menu("OUTLINER_MT_edit_orphan_data")


class OUTLINER_MT_view(Menu):
    bl_label = "View"

    def draw(self, context):
        layout = self.layout

        space = context.space_data

        if space.display_mode != 'DATA_API':
            layout.prop(space, "use_sort_alpha")
            layout.prop(space, "show_restrict_columns")
            layout.separator()
            layout.operator("outliner.show_active")

        layout.operator("outliner.show_one_level", text="Show One Level")
        layout.operator("outliner.show_one_level", text="Hide One Level").open = False
        layout.operator("outliner.show_hierarchy")

        layout.separator()

        layout.operator("screen.area_dupli")
        layout.operator("screen.screen_full_area")
        layout.operator("screen.screen_full_area", text="Toggle Fullscreen Area").use_hide_panels = True


class OUTLINER_MT_edit_datablocks(Menu):
    bl_label = "Edit"

    def draw(self, context):
        layout = self.layout

        layout.operator("outliner.keyingset_add_selected")
        layout.operator("outliner.keyingset_remove_selected")

        layout.separator()

        layout.operator("outliner.drivers_add_selected")
        layout.operator("outliner.drivers_delete_selected")


class OUTLINER_MT_edit_orphan_data(Menu):
    bl_label = "Edit"

    def draw(self, context):
        layout = self.layout
        layout.operator("outliner.orphans_purge")


class OUTLINER_MT_collection_view_layer(Menu):
    bl_label = "View Layer"

    def draw(self, context):
        layout = self.layout

        space = context.space_data

        layout.operator("outliner.collection_exclude_set", text="Exclude")
        layout.operator("outliner.collection_include_set", text="Include")


class OUTLINER_MT_collection(Menu):
    bl_label = "Collection"

    def draw(self, context):
        layout = self.layout

        space = context.space_data

        layout.operator("outliner.collection_new", text="New").nested = True
        layout.operator("outliner.collection_duplicate", text="Duplicate")
        layout.operator("outliner.collection_delete", text="Delete").hierarchy = False
        layout.operator("outliner.collection_delete", text="Delete Hierarchy").hierarchy = True

        layout.separator()

        layout.operator("outliner.collection_objects_select", text="Select Objects")
        layout.operator("outliner.collection_objects_deselect", text="Deselect Objects")

        layout.separator()

        layout.operator("outliner.collection_instance", text="Instance to Scene")
        if space.display_mode != 'VIEW_LAYER':
            layout.operator("outliner.collection_link", text="Link to Scene")
        layout.operator("outliner.id_operation", text="Unlink").type='UNLINK'

        if space.display_mode == 'VIEW_LAYER':
            layout.separator()
            layout.menu("OUTLINER_MT_collection_view_layer")

        layout.separator()
        layout.operator_menu_enum("outliner.id_operation", 'type', text="ID Data")


class OUTLINER_MT_collection_new(Menu):
    bl_label = "Collection"

    def draw(self, context):
        layout = self.layout

        layout.operator("outliner.collection_new", text="New").nested = False


class OUTLINER_MT_object(Menu):
    bl_label = "Object"

    def draw(self, context):
        layout = self.layout

        space = context.space_data

        layout.operator("outliner.object_operation", text="Delete").type='DELETE'
        if space.display_mode == 'VIEW_LAYER' and not space.use_filter_collection:
            layout.operator("outliner.object_operation", text="Delete Hierarchy").type='DELETE_HIERARCHY'

        layout.separator()

        layout.operator("outliner.object_operation", text="Select").type='SELECT'
        layout.operator("outliner.object_operation", text="Select Hierarchy").type='SELECT_HIERARCHY'
        layout.operator("outliner.object_operation", text="Deselect").type='DESELECT'

        layout.separator()

        if not (space.display_mode == 'VIEW_LAYER' and not space.use_filter_collection):
            layout.operator("outliner.id_operation", text="Unlink").type='UNLINK'
            layout.separator()

        layout.operator_menu_enum("outliner.id_operation", 'type', text="ID Data")


class OUTLINER_PT_filter(Panel):
    bl_space_type = 'OUTLINER'
    bl_region_type = 'HEADER'
    bl_label = "Filter"

    def draw(self, context):
        layout = self.layout

        space = context.space_data
        display_mode = space.display_mode

        layout.prop(space, "use_filter_collection", text="Collections")

        layout.separator()

        col = layout.column()
        col.prop(space, "use_filter_object", text="Objects")
        active = space.use_filter_object

        sub = col.column(align=True)
        sub.active = active
        sub.prop(space, "filter_state", text="")
        sub.prop(space, "use_filter_object_content", text="Object Contents")
        sub.prop(space, "use_filter_children", text="Object Children")

        layout.separator()

        col = layout.column_flow(align=True)
        col.active = active

        if bpy.data.meshes:
            col.prop(space, "use_filter_object_mesh", text="Meshes")
        if bpy.data.armatures:
            col.prop(space, "use_filter_object_armature", text="Armatures")
        if bpy.data.lamps:
            col.prop(space, "use_filter_object_lamp", text="Lamps")
        if bpy.data.cameras:
            col.prop(space, "use_filter_object_camera", text="Cameras")

        col.prop(space, "use_filter_object_empty", text="Empties")

        if bpy.data.curves or \
           bpy.data.metaballs or \
           bpy.data.lightprobes or \
           bpy.data.lattices or \
           bpy.data.fonts or bpy.data.speakers:
            col.prop(space, "use_filter_object_others", text="Others")


classes = (
    OUTLINER_HT_header,
    OUTLINER_MT_editor_menus,
    OUTLINER_MT_view,
    OUTLINER_MT_edit_datablocks,
    OUTLINER_MT_edit_orphan_data,
    OUTLINER_MT_collection,
    OUTLINER_MT_collection_new,
    OUTLINER_MT_collection_view_layer,
    OUTLINER_MT_object,
    OUTLINER_PT_filter,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
