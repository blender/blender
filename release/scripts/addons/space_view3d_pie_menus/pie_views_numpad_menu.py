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
    "name": "Hotkey: 'Q'",
    "description": "Viewport Numpad Menus",
    "author": "pitiwazou, meta-androcto",
    "version": (0, 1, 1),
    "blender": (2, 77, 0),
    "location": "Q key",
    "warning": "",
    "wiki_url": "",
    "category": "View Numpad Pie"
    }

import bpy
from bpy.types import (
        Menu,
        Operator,
        )


# Lock Camera Transforms
class LockTransforms(Operator):
    bl_idname = "object.locktransforms"
    bl_label = "Lock Object Transforms"
    bl_description = ("Enable or disable the editing of objects transforms in the 3D View\n"
                     "Needs an existing Active Object")
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context):
        return context.active_object is not None

    def execute(self, context):
        obj = context.active_object
        if obj.lock_rotation[0] is False:
            obj.lock_rotation[0] = True
            obj.lock_rotation[1] = True
            obj.lock_rotation[2] = True
            obj.lock_scale[0] = True
            obj.lock_scale[1] = True
            obj.lock_scale[2] = True

        elif context.object.lock_rotation[0] is True:
            obj.lock_rotation[0] = False
            obj.lock_rotation[1] = False
            obj.lock_rotation[2] = False
            obj.lock_scale[0] = False
            obj.lock_scale[1] = False
            obj.lock_scale[2] = False

        return {'FINISHED'}


# Pie View All Sel Glob Etc - Q
class PieViewallSelGlobEtc(Menu):
    bl_idname = "pie.vieallselglobetc"
    bl_label = "Pie View All Sel Glob..."

    def draw(self, context):
        layout = self.layout

        # 4 - LEFT
        layout.operator("view3d.view_all", text="View All").center = True
        # 6 - RIGHT
        layout.operator("view3d.view_selected", text="View Selected")
        # 2 - BOTTOM
        layout.operator("view3d.view_persportho", text="Persp/Ortho", icon='RESTRICT_VIEW_OFF')
        # 8 - TOP
        layout.operator("view3d.localview", text="Local/Global")
        # 7 - TOP - LEFT
        layout.operator("screen.region_quadview", text="Toggle Quad View", icon='SPLITSCREEN')
        # 1 - BOTTOM - LEFT
        layout.operator("wm.call_menu_pie", text="Previous Menu", icon='BACK').name = "pie.viewnumpad"
        # 9 - TOP - RIGHT
        layout.operator("screen.screen_full_area", text="Full Screen", icon='FULLSCREEN_ENTER')
        # 3 - BOTTOM - RIGHT


# Pie views numpad - Q
class PieViewNumpad(Menu):
    bl_idname = "pie.viewnumpad"
    bl_label = "Pie Views Ortho"

    def draw(self, context):
        layout = self.layout
        ob = context.active_object
        pie = layout.menu_pie()
        scene = context.scene
        rd = scene.render

        # 4 - LEFT
        pie.operator("view3d.viewnumpad", text="Left", icon='TRIA_LEFT').type = 'LEFT'
        # 6 - RIGHT
        pie.operator("view3d.viewnumpad", text="Right", icon='TRIA_RIGHT').type = 'RIGHT'
        # 2 - BOTTOM
        pie.operator("view3d.viewnumpad", text="Bottom", icon='TRIA_DOWN').type = 'BOTTOM'
        # 8 - TOP
        pie.operator("view3d.viewnumpad", text="Top", icon='TRIA_UP').type = 'TOP'
        # 7 - TOP - LEFT
        pie.operator("view3d.viewnumpad", text="Front").type = 'FRONT'
        # 9 - TOP - RIGHT
        pie.operator("view3d.viewnumpad", text="Back").type = 'BACK'
        # 1 - BOTTOM - LEFT
        box = pie.split().column()
        row = box.row(align=True)

        if context.space_data.lock_camera is False:
            row.operator("wm.context_toggle", text="Lock Cam to View",
                         icon='UNLOCKED').data_path = "space_data.lock_camera"
        elif context.space_data.lock_camera is True:
            row.operator("wm.context_toggle", text="Lock Cam to View",
                         icon='LOCKED').data_path = "space_data.lock_camera"

        row = box.row(align=True)
        row.operator("view3d.viewnumpad", text="View Cam", icon='VISIBLE_IPO_ON').type = 'CAMERA'
        row.operator("view3d.camera_to_view", text="Cam to view", icon='MAN_TRANS')

        icon_locked = 'LOCKED' if ob and ob.lock_rotation[0] is False else \
                      'UNLOCKED' if ob and ob.lock_rotation[0] is True else 'LOCKED'

        row = box.row(align=True)
        row.operator("object.locktransforms", text="Lock Transforms", icon=icon_locked)

        row = box.row(align=True)
        row.prop(rd, "use_border", text="Border")
        # 3 - BOTTOM - RIGHT
        pie.menu(PieViewallSelGlobEtc.bl_idname, text="View Menu", icon='BBOX')


classes = (
    PieViewNumpad,
    LockTransforms,
    PieViewallSelGlobEtc,
    )

addon_keymaps = []


def register():
    for cls in classes:
        bpy.utils.register_class(cls)

    wm = bpy.context.window_manager
    if wm.keyconfigs.addon:
        # Views numpad
        km = wm.keyconfigs.addon.keymaps.new(name='3D View Generic', space_type='VIEW_3D')
        kmi = km.keymap_items.new('wm.call_menu_pie', 'Q', 'PRESS')
        kmi.properties.name = "pie.viewnumpad"
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
