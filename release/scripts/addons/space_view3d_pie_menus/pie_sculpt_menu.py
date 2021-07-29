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
    "name": "Hotkey: 'W'",
    "description": "Sculpt Brush Menu",
    "author": "pitiwazou, meta-androcto",
    "version": (0, 1, 0),
    "blender": (2, 77, 0),
    "location": "W key",
    "warning": "",
    "wiki_url": "",
    "category": "Sculpt Pie"
    }

import bpy
from bpy.types import (
        Menu,
        Operator,
        )


# Sculpt Draw
class SculptSculptDraw(Operator):
    bl_idname = "sculpt.sculptraw"
    bl_label = "Sculpt SculptDraw"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        context.tool_settings.sculpt.brush = bpy.data.brushes['SculptDraw']
        return {'FINISHED'}


# Pie Sculp Pie Menus - W
class PieSculptPie(Menu):
    bl_idname = "pie.sculpt"
    bl_label = "Pie Sculpt"

    def draw(self, context):
        layout = self.layout
        pie = layout.menu_pie()
        # 4 - LEFT
        pie.operator("paint.brush_select",
                    text="Crease", icon='BRUSH_CREASE').sculpt_tool = 'CREASE'
        # 6 - RIGHT
        pie.operator("paint.brush_select",
                    text='Blob', icon='BRUSH_BLOB').sculpt_tool = 'BLOB'
        # 2 - BOTTOM
        pie.menu(PieSculpttwo.bl_idname,
                    text="More Brushes", icon='BRUSH_SMOOTH')
        # 8 - TOP
        pie.operator("sculpt.sculptraw",
                    text='SculptDraw', icon='BRUSH_SCULPT_DRAW')
        # 7 - TOP - LEFT
        pie.operator("paint.brush_select",
                    text="Clay", icon='BRUSH_CLAY').sculpt_tool = 'CLAY'
        # 9 - TOP - RIGHT
        pie.operator("paint.brush_select",
                    text='Claystrips', icon='BRUSH_CLAY_STRIPS').sculpt_tool = 'CLAY_STRIPS'
        # 1 - BOTTOM - LEFT
        pie.operator("paint.brush_select",
                    text='Inflate/Deflate', icon='BRUSH_INFLATE').sculpt_tool = 'INFLATE'
        # 3 - BOTTOM - RIGHT
        pie.menu(PieSculptthree.bl_idname,
                    text="Grab Brushes", icon='BRUSH_GRAB')


# Pie Sculpt 2
class PieSculpttwo(Menu):
    bl_idname = "pie.sculpttwo"
    bl_label = "Pie Sculpt 2"

    def draw(self, context):
        layout = self.layout

        layout.operator("paint.brush_select", text='Smooth',
                        icon='BRUSH_SMOOTH').sculpt_tool = 'SMOOTH'
        layout.operator("paint.brush_select", text='Flatten',
                        icon='BRUSH_FLATTEN').sculpt_tool = 'FLATTEN'
        layout.operator("paint.brush_select", text='Scrape/Peaks',
                        icon='BRUSH_SCRAPE').sculpt_tool = 'SCRAPE'
        layout.operator("paint.brush_select", text='Fill/Deepen',
                        icon='BRUSH_FILL').sculpt_tool = 'FILL'
        layout.operator("paint.brush_select", text='Pinch/Magnify',
                        icon='BRUSH_PINCH').sculpt_tool = 'PINCH'
        layout.operator("paint.brush_select", text='Layer',
                        icon='BRUSH_LAYER').sculpt_tool = 'LAYER'
        layout.operator("paint.brush_select", text='Mask',
                        icon='BRUSH_MASK').sculpt_tool = 'MASK'


# Pie Sculpt Three
class PieSculptthree(Menu):
    bl_idname = "pie.sculptthree"
    bl_label = "Pie Sculpt 3"

    def draw(self, context):
        layout = self.layout

        layout.operator("paint.brush_select",
                        text='Grab', icon='BRUSH_GRAB').sculpt_tool = 'GRAB'
        layout.operator("paint.brush_select",
                        text='Nudge', icon='BRUSH_NUDGE').sculpt_tool = 'NUDGE'
        layout.operator("paint.brush_select",
                        text='Thumb', icon='BRUSH_THUMB').sculpt_tool = 'THUMB'
        layout.operator("paint.brush_select",
                        text='Snakehook', icon='BRUSH_SNAKE_HOOK').sculpt_tool = 'SNAKE_HOOK'
        layout.operator("paint.brush_select",
                        text='Twist', icon='BRUSH_ROTATE').sculpt_tool = 'ROTATE'


classes = (
    PieSculptPie,
    PieSculpttwo,
    PieSculptthree,
    SculptSculptDraw,
    )

addon_keymaps = []


def register():
    for cls in classes:
        bpy.utils.register_class(cls)

    wm = bpy.context.window_manager
    if wm.keyconfigs.addon:
        # Sculpt Pie Menu
        km = wm.keyconfigs.addon.keymaps.new(name='Sculpt')
        kmi = km.keymap_items.new('wm.call_menu_pie', 'W', 'PRESS')
        kmi.properties.name = "pie.sculpt"
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
