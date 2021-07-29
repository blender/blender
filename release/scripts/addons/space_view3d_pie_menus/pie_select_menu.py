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

bl_info = {
    "name": "Hotkey: 'A'",
    "description": "Object/Edit mode Selection Menu",
    "author": "pitiwazou, meta-androcto",
    "version": (0, 1, 1),
    "blender": (2, 77, 0),
    "location": "3D View",
    "warning": "",
    "wiki_url": "",
    "category": "Select Pie"
    }

import bpy
from bpy.types import Menu


# Pie Selection Object Mode - A
class PieSelectionsMore(Menu):
    bl_idname = "pie.selectionsmore"
    bl_label = "Pie Selections Object Mode"

    def draw(self, context):
        layout = self.layout
        pie = layout.menu_pie()
        box = pie.split().column()
        box.operator("object.select_by_type", text="Select By Type", icon='SNAP_VOLUME')
        box.operator("object.select_grouped", text="Select Grouped", icon='ROTATE')
        box.operator("object.select_linked", text="Select Linked", icon='CONSTRAINT_BONE')
        box.menu("VIEW3D_MT_select_object_more_less", text="More/Less")


# Pie Selection Object Mode - A
class PieSelectionsOM(Menu):
    bl_idname = "pie.selectionsom"
    bl_label = "Pie Selections Object Mode"

    def draw(self, context):
        layout = self.layout
        pie = layout.menu_pie()
        # 4 - LEFT
        pie.operator("object.select_by_layer", text="Select By Layer", icon='LAYER_ACTIVE')
        # 6 - RIGHT
        pie.operator("object.select_random", text="Select Random", icon='GROUP_VERTEX')
        # 2 - BOTTOM
        pie.operator("object.select_all", text="Invert Selection",
                    icon='ZOOM_PREVIOUS').action = 'INVERT'
        # 8 - TOP
        pie.operator("object.select_all", text="Select All Toggle",
                    icon='RENDER_REGION').action = 'TOGGLE'
        # 7 - TOP - LEFT
        pie.operator("view3d.select_circle", text="Circle Select", icon='BORDER_LASSO')
        # 9 - TOP - RIGHT
        pie.operator("view3d.select_border", text="Border Select", icon='BORDER_RECT')
        # 1 - BOTTOM - LEFT
        pie.operator("object.select_camera", text="Select Camera", icon='CAMERA_DATA')
        # 3 - BOTTOM - RIGHT
        pie.menu("pie.selectionsmore", text="Select Menu", icon='RESTRICT_SELECT_OFF')


# Pie Selection Edit Mode
class PieSelectionsEM(Menu):
    bl_idname = "pie.selectionsem"
    bl_label = "Pie Selections Edit Mode"

    def draw(self, context):
        layout = self.layout
        pie = layout.menu_pie()
        # 4 - LEFT
        pie.operator("view3d.select_border", text="Border Select",
                    icon='BORDER_RECT')
        # 6 - RIGHT
        pie.menu("object.selectloopselection", text="Select Loop Menu", icon='LOOPSEL')
        # 2 - BOTTOM
        pie.operator("mesh.select_all", text="Select None",
                    icon='RESTRICT_SELECT_ON').action = 'DESELECT'
        # 8 - TOP
        pie.operator("mesh.select_all", text="Select All",
                    icon='RESTRICT_SELECT_OFF').action = 'SELECT'
        # 7 - TOP - LEFT
        pie.operator("mesh.select_all", text="Select All Toggle",
                    icon='ARROW_LEFTRIGHT').action = 'TOGGLE'
        # 9 - TOP - RIGHT
        pie.operator("mesh.select_all", text="Invert Selection",
                    icon='FULLSCREEN_EXIT').action = 'INVERT'
        # 1 - BOTTOM - LEFT
        pie.operator("view3d.select_circle", text="Circle Select",
                    icon='BORDER_LASSO')
        # 3 - BOTTOM - RIGHT
        pie.menu("object.selectallbyselection", text="Multi Select Menu", icon='SNAP_EDGE')


# Select All By Selection
class SelectAllBySelection(Menu):
    bl_idname = "object.selectallbyselection"
    bl_label = "Verts Edges Faces"
    bl_options = {'REGISTER', 'UNDO'}

    def draw(self, context):
        layout = self.layout
        layout.operator_context = 'INVOKE_REGION_WIN'

        prop = layout.operator("wm.context_set_value", text="Vertex Select",
                               icon='VERTEXSEL')
        prop.value = "(True, False, False)"
        prop.data_path = "tool_settings.mesh_select_mode"

        prop = layout.operator("wm.context_set_value", text="Edge Select",
                               icon='EDGESEL')
        prop.value = "(False, True, False)"
        prop.data_path = "tool_settings.mesh_select_mode"

        prop = layout.operator("wm.context_set_value", text="Face Select",
                               icon='FACESEL')
        prop.value = "(False, False, True)"
        prop.data_path = "tool_settings.mesh_select_mode"

        prop = layout.operator("wm.context_set_value",
                               text="Vertex & Edge & Face Select",
                               icon='SNAP_VOLUME')
        prop.value = "(True, True, True)"
        prop.data_path = "tool_settings.mesh_select_mode"


class SelectLoopSelection(Menu):
    bl_idname = "object.selectloopselection"
    bl_label = "Verts Edges Faces"
    bl_options = {'REGISTER', 'UNDO'}

    def draw(self, context):
        layout = self.layout
        layout.operator_context = 'INVOKE_REGION_WIN'

        layout.operator("mesh.loop_multi_select", text="Select Loop", icon='LOOPSEL').ring = False
        layout.operator("mesh.loop_multi_select", text="Select Ring", icon='EDGESEL').ring = True
        layout.operator("mesh.loop_to_region", text="Select Loop Inner Region", icon='FACESEL')


classes = (
    PieSelectionsOM,
    PieSelectionsEM,
    SelectAllBySelection,
    PieSelectionsMore,
    SelectLoopSelection,
    )

addon_keymaps = []


def register():
    for cls in classes:
        bpy.utils.register_class(cls)

    wm = bpy.context.window_manager
    if wm.keyconfigs.addon:
        # Selection Object Mode
        km = wm.keyconfigs.addon.keymaps.new(name='Object Mode')
        kmi = km.keymap_items.new('wm.call_menu_pie', 'A', 'PRESS')
        kmi.properties.name = "pie.selectionsom"
        addon_keymaps.append((km, kmi))

        # Selection Edit Mode
        km = wm.keyconfigs.addon.keymaps.new(name='Mesh')
        kmi = km.keymap_items.new('wm.call_menu_pie', 'A', 'PRESS')
        kmi.properties.name = "pie.selectionsem"
        addon_keymaps.append((km, kmi))


def unregister():
    for cls in classes:
        bpy.utils.unregister_class(cls)

    wm = bpy.context.window_manager
    kc = wm.keyconfigs.addon
    if kc:
        for km, kmi in addon_keymaps:
            km.keymap_items.remove(kmi)
    addon_keymaps.clear()


if __name__ == "__main__":
    register()
