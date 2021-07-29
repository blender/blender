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
    "name": "Hotkey: 'Ctrl Space'",
    "description": "Extended Manipulator Menu",
    "author": "pitiwazou, meta-androcto",
    "version": (0, 1, 1),
    "blender": (2, 77, 0),
    "location": "3D View",
    "warning": "",
    "wiki_url": "",
    "category": "Manipulator Pie"
    }

import bpy
from bpy.types import (
        Menu,
        Operator,
        )


class ManipTranslate(Operator):
    bl_idname = "manip.translate"
    bl_label = "Manip Translate"
    bl_options = {'REGISTER', 'UNDO'}
    bl_description = " Show Translate"

    def execute(self, context):
        if context.space_data.show_manipulator is False:
            context.space_data.show_manipulator = True
            context.space_data.transform_manipulators = {'TRANSLATE'}
        if context.space_data.transform_manipulators != {'TRANSLATE'}:
            context.space_data.transform_manipulators = {'TRANSLATE'}
        return {'FINISHED'}


class ManipRotate(Operator):
    bl_idname = "manip.rotate"
    bl_label = "Manip Rotate"
    bl_options = {'REGISTER', 'UNDO'}
    bl_description = " Show Rotate"

    def execute(self, context):
        if context.space_data.show_manipulator is False:
            context.space_data.show_manipulator = True
            context.space_data.transform_manipulators = {'ROTATE'}
        if context.space_data.transform_manipulators != {'ROTATE'}:
            context.space_data.transform_manipulators = {'ROTATE'}
        return {'FINISHED'}


class ManipScale(Operator):
    bl_idname = "manip.scale"
    bl_label = "Manip Scale"
    bl_options = {'REGISTER', 'UNDO'}
    bl_description = " Show Scale"

    def execute(self, context):
        if context.space_data.show_manipulator is False:
            context.space_data.show_manipulator = True
            context.space_data.transform_manipulators = {'SCALE'}
        if context.space_data.transform_manipulators != {'SCALE'}:
            context.space_data.transform_manipulators = {'SCALE'}
        return {'FINISHED'}


class TranslateRotate(Operator):
    bl_idname = "translate.rotate"
    bl_label = "Translate Rotate"
    bl_options = {'REGISTER', 'UNDO'}
    bl_description = " Show Translate/Rotate"

    def execute(self, context):
        if context.space_data.show_manipulator is False:
            context.space_data.show_manipulator = True
            context.space_data.transform_manipulators = {'TRANSLATE', 'ROTATE'}
        if context.space_data.transform_manipulators != {'TRANSLATE', 'ROTATE'}:
            context.space_data.transform_manipulators = {'TRANSLATE', 'ROTATE'}
        return {'FINISHED'}


class TranslateScale(Operator):
    bl_idname = "translate.scale"
    bl_label = "Translate Scale"
    bl_options = {'REGISTER', 'UNDO'}
    bl_description = " Show Translate/Scale"

    def execute(self, context):
        if context.space_data.show_manipulator is False:
            context.space_data.show_manipulator = True
            context.space_data.transform_manipulators = {'TRANSLATE', 'SCALE'}
        if context.space_data.transform_manipulators != {'TRANSLATE', 'SCALE'}:
            context.space_data.transform_manipulators = {'TRANSLATE', 'SCALE'}
        return {'FINISHED'}


class RotateScale(Operator):
    bl_idname = "rotate.scale"
    bl_label = "Rotate Scale"
    bl_options = {'REGISTER', 'UNDO'}
    bl_description = " Show Rotate/Scale"

    def execute(self, context):
        if context.space_data.show_manipulator is False:
            context.space_data.show_manipulator = True
            context.space_data.transform_manipulators = {'ROTATE', 'SCALE'}
        if context.space_data.transform_manipulators != {'ROTATE', 'SCALE'}:
            context.space_data.transform_manipulators = {'ROTATE', 'SCALE'}
        return {'FINISHED'}


class TranslateRotateScale(Operator):
    bl_idname = "translate.rotatescale"
    bl_label = "Translate Rotate Scale"
    bl_options = {'REGISTER', 'UNDO'}
    bl_description = "Show All"

    def execute(self, context):
        if context.space_data.show_manipulator is False:
            context.space_data.show_manipulator = True
            context.space_data.transform_manipulators = {'TRANSLATE', 'ROTATE', 'SCALE'}
        if context.space_data.transform_manipulators != {'TRANSLATE', 'ROTATE', 'SCALE'}:
            context.space_data.transform_manipulators = {'TRANSLATE', 'ROTATE', 'SCALE'}
        return {'FINISHED'}


class WManupulators(Operator):
    bl_idname = "w.manupulators"
    bl_label = "W Manupulators"
    bl_options = {'REGISTER', 'UNDO'}
    bl_description = " Show/Hide Manipulator"

    def execute(self, context):

        if context.space_data.show_manipulator is True:
            context.space_data.show_manipulator = False

        elif context.space_data.show_manipulator is False:
            context.space_data.show_manipulator = True

        return {'FINISHED'}


# Pie Manipulators - Ctrl + Space
class PieManipulator(Menu):
    bl_idname = "pie.manipulator"
    bl_label = "Pie Manipulator"

    def draw(self, context):
        layout = self.layout
        pie = layout.menu_pie()
        # 4 - LEFT
        pie.operator("rotate.scale", text="Rotate/Scale")
        # 6 - RIGHT
        pie.operator("manip.rotate", text="Rotate", icon='MAN_ROT')
        # 2 - BOTTOM
        pie.operator("translate.rotatescale", text="Translate/Rotate/Scale")
        # 8 - TOP
        pie.operator("w.manupulators", text="Show/Hide Toggle", icon='MANIPUL')
        # 7 - TOP - LEFT
        pie.operator("translate.rotate", text="Translate/Rotate")
        # 9 - TOP - RIGHT
        pie.operator("manip.translate", text="Translate", icon='MAN_TRANS')
        # 1 - BOTTOM - LEFT
        pie.operator("translate.scale", text="Translate/Scale")
        # 3 - BOTTOM - RIGHT
        pie.operator("manip.scale", text="Scale", icon='MAN_SCALE')


classes = (
    PieManipulator,
    ManipTranslate,
    ManipRotate,
    ManipScale,
    TranslateRotate,
    TranslateScale,
    RotateScale,
    TranslateRotateScale,
    WManupulators,
    )

addon_keymaps = []


def register():
    for cls in classes:
        bpy.utils.register_class(cls)

    wm = bpy.context.window_manager
    if wm.keyconfigs.addon:
        # Manipulators
        km = wm.keyconfigs.addon.keymaps.new(name='3D View Generic', space_type='VIEW_3D')
        kmi = km.keymap_items.new('wm.call_menu_pie', 'SPACE', 'PRESS', ctrl=True)
        kmi.properties.name = "pie.manipulator"
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
