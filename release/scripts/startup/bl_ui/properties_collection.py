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
from bpy.types import Panel, UIList


class CollectionButtonsPanel:
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "collection"


class COLLECTION_PT_context_collection(CollectionButtonsPanel, Panel):
    bl_label = ""
    bl_options = {'HIDE_HEADER'}

    def draw(self, context):
        layout = self.layout
        space = context.space_data

        collection = context.layer_collection
        name = collection.name
        if name == 'Master Collection':
            layout.label(text=name, icon='COLLAPSEMENU')
        else:
            layout.prop(collection, "name", text="", icon='COLLAPSEMENU')


class COLLECTION_UL_objects(UIList):
    def draw_item(self, context, layout, data, item, icon, active_data, active_propname, index):
        # assert(isinstance(item, bpy.types.Object)
        ob = item
        if self.layout_type in {'DEFAULT', 'COMPACT'}:
            layout.label(ob.name, icon_value=icon)

        elif self.layout_type == 'GRID':
            layout.alignment = 'CENTER'
            layout.label("", icon_value=icon)


class COLLECTION_PT_objects(CollectionButtonsPanel, Panel):
    bl_label = "Objects"

    def draw(self, context):
        layout = self.layout
        scene = context.scene
        collection = context.scene_collection

        row = layout.row()
        row.template_list("COLLECTION_UL_objects", "name", collection, "objects", collection.objects, "active_index", rows=2)

        col = row.column(align=True)
        col.operator("collections.objects_add", icon='ZOOMIN', text="")
        col.operator("collections.objects_remove", icon='ZOOMOUT', text="")

        row = layout.row(align=True)
        row.operator("collections.objects_select", text="Select")
        row.operator("collections.objects_deselect", text="Deselect")


if __name__ == "__main__":  # only for live edit.
    bpy.utils.register_module(__name__)
