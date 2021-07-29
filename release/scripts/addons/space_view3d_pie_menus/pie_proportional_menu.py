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
    "name": "Hotkey: 'O'",
    "description": "Proportional Object/Edit Tools",
    "author": "pitiwazou, meta-androcto",
    "version": (0, 1, 1),
    "blender": (2, 77, 0),
    "location": "3D View Object & Edit modes",
    "warning": "",
    "wiki_url": "",
    "category": "Proportional Edit Pie"
    }

import bpy
from bpy.types import (
        Menu,
        Operator,
        )


# Proportional Edit Object
class ProportionalEditObj(Operator):
    bl_idname = "proportional_obj.active"
    bl_label = "Proportional Edit Object"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        ts = context.tool_settings

        if ts.use_proportional_edit_objects is True:
            ts.use_proportional_edit_objects = False

        elif ts.use_proportional_edit_objects is False:
            ts.use_proportional_edit_objects = True

        return {'FINISHED'}


class ProportionalSmoothObj(Operator):
    bl_idname = "proportional_obj.smooth"
    bl_label = "Proportional Smooth Object"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        ts = context.tool_settings
        if ts.use_proportional_edit_objects is False:
            ts.use_proportional_edit_objects = True
            ts.proportional_edit_falloff = 'SMOOTH'

        if ts.proportional_edit_falloff != 'SMOOTH':
            ts.proportional_edit_falloff = 'SMOOTH'
        return {'FINISHED'}


class ProportionalSphereObj(Operator):
    bl_idname = "proportional_obj.sphere"
    bl_label = "Proportional Sphere Object"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        ts = context.tool_settings
        if ts.use_proportional_edit_objects is False:
            ts.use_proportional_edit_objects = True
            ts.proportional_edit_falloff = 'SPHERE'

        if ts.proportional_edit_falloff != 'SPHERE':
            ts.proportional_edit_falloff = 'SPHERE'
        return {'FINISHED'}


class ProportionalRootObj(Operator):
    bl_idname = "proportional_obj.root"
    bl_label = "Proportional Root Object"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        ts = context.tool_settings
        if ts.use_proportional_edit_objects is False:
            ts.use_proportional_edit_objects = True
            ts.proportional_edit_falloff = 'ROOT'

        if ts.proportional_edit_falloff != 'ROOT':
            ts.proportional_edit_falloff = 'ROOT'
        return {'FINISHED'}


class ProportionalSharpObj(Operator):
    bl_idname = "proportional_obj.sharp"
    bl_label = "Proportional Sharp Object"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        ts = context.tool_settings
        if ts.use_proportional_edit_objects is False:
            ts.use_proportional_edit_objects = True
            ts.proportional_edit_falloff = 'SHARP'

        if ts.proportional_edit_falloff != 'SHARP':
            ts.proportional_edit_falloff = 'SHARP'
        return {'FINISHED'}


class ProportionalLinearObj(Operator):
    bl_idname = "proportional_obj.linear"
    bl_label = "Proportional Linear Object"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        ts = context.tool_settings
        if ts.use_proportional_edit_objects is False:
            ts.use_proportional_edit_objects = True
            ts.proportional_edit_falloff = 'LINEAR'

        if ts.proportional_edit_falloff != 'LINEAR':
            ts.proportional_edit_falloff = 'LINEAR'
        return {'FINISHED'}


class ProportionalConstantObj(Operator):
    bl_idname = "proportional_obj.constant"
    bl_label = "Proportional Constant Object"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        ts = context.tool_settings
        if ts.use_proportional_edit_objects is False:
            ts.use_proportional_edit_objects = True
            ts.proportional_edit_falloff = 'CONSTANT'

        if ts.proportional_edit_falloff != 'CONSTANT':
            ts.proportional_edit_falloff = 'CONSTANT'
        return {'FINISHED'}


class ProportionalRandomObj(Operator):
    bl_idname = "proportional_obj.random"
    bl_label = "Proportional Random Object"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        ts = context.tool_settings
        if ts.use_proportional_edit_objects is False:
            ts.use_proportional_edit_objects = True
            ts.proportional_edit_falloff = 'RANDOM'

        if ts.proportional_edit_falloff != 'RANDOM':
            ts.proportional_edit_falloff = 'RANDOM'
        return {'FINISHED'}


# Proportional Edit Edit Mode
class ProportionalEditEdt(Operator):
    bl_idname = "proportional_edt.active"
    bl_label = "Proportional Edit EditMode"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        ts = context.tool_settings

        if ts.proportional_edit != ('DISABLED'):
            ts.proportional_edit = 'DISABLED'
        elif ts.proportional_edit != ('ENABLED'):
            ts.proportional_edit = 'ENABLED'
        return {'FINISHED'}


class ProportionalConnectedEdt(Operator):
    bl_idname = "proportional_edt.connected"
    bl_label = "Proportional Connected EditMode"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        ts = context.tool_settings
        if ts.proportional_edit != ('CONNECTED'):
            ts.proportional_edit = 'CONNECTED'
        return {'FINISHED'}


class ProportionalProjectedEdt(Operator):
    bl_idname = "proportional_edt.projected"
    bl_label = "Proportional projected EditMode"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        ts = context.tool_settings

        if ts.proportional_edit != ('PROJECTED'):
            ts.proportional_edit = 'PROJECTED'
        return {'FINISHED'}


class ProportionalSmoothEdt(Operator):
    bl_idname = "proportional_edt.smooth"
    bl_label = "Proportional Smooth EditMode"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        ts = context.tool_settings
        if ts.proportional_edit == 'DISABLED':
            ts.proportional_edit = 'ENABLED'
            ts.proportional_edit_falloff = 'SMOOTH'

        if ts.proportional_edit_falloff != 'SMOOTH':
            ts.proportional_edit_falloff = 'SMOOTH'
        return {'FINISHED'}


class ProportionalSphereEdt(Operator):
    bl_idname = "proportional_edt.sphere"
    bl_label = "Proportional Sphere EditMode"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        ts = context.tool_settings
        if ts.proportional_edit == 'DISABLED':
            ts.proportional_edit = 'ENABLED'
            ts.proportional_edit_falloff = 'SPHERE'

        if ts.proportional_edit_falloff != 'SPHERE':
            ts.proportional_edit_falloff = 'SPHERE'
        return {'FINISHED'}


class ProportionalRootEdt(Operator):
    bl_idname = "proportional_edt.root"
    bl_label = "Proportional Root EditMode"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        ts = context.tool_settings
        if ts.proportional_edit == 'DISABLED':
            ts.proportional_edit = 'ENABLED'
            ts.proportional_edit_falloff = 'ROOT'

        if ts.proportional_edit_falloff != 'ROOT':
            ts.proportional_edit_falloff = 'ROOT'
        return {'FINISHED'}


class ProportionalSharpEdt(Operator):
    bl_idname = "proportional_edt.sharp"
    bl_label = "Proportional Sharp EditMode"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        ts = context.tool_settings
        if ts.proportional_edit == 'DISABLED':
            ts.proportional_edit = 'ENABLED'
            ts.proportional_edit_falloff = 'SHARP'

        if ts.proportional_edit_falloff != 'SHARP':
            ts.proportional_edit_falloff = 'SHARP'
        return {'FINISHED'}


class ProportionalLinearEdt(Operator):
    bl_idname = "proportional_edt.linear"
    bl_label = "Proportional Linear EditMode"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        ts = context.tool_settings
        if ts.proportional_edit == 'DISABLED':
            ts.proportional_edit = 'ENABLED'
            ts.proportional_edit_falloff = 'LINEAR'

        if ts.proportional_edit_falloff != 'LINEAR':
            ts.proportional_edit_falloff = 'LINEAR'
        return {'FINISHED'}


class ProportionalConstantEdt(Operator):
    bl_idname = "proportional_edt.constant"
    bl_label = "Proportional Constant EditMode"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        ts = context.tool_settings
        if ts.proportional_edit == 'DISABLED':
            ts.proportional_edit = 'ENABLED'
            ts.proportional_edit_falloff = 'CONSTANT'

        if ts.proportional_edit_falloff != 'CONSTANT':
            ts.proportional_edit_falloff = 'CONSTANT'
        return {'FINISHED'}


class ProportionalRandomEdt(Operator):
    bl_idname = "proportional_edt.random"
    bl_label = "Proportional Random EditMode"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        ts = context.tool_settings
        if ts.proportional_edit == 'DISABLED':
            ts.proportional_edit = 'ENABLED'
            ts.proportional_edit_falloff = 'RANDOM'

        if ts.proportional_edit_falloff != 'RANDOM':
            ts.proportional_edit_falloff = 'RANDOM'
        return {'FINISHED'}


# Pie ProportionalEditObj - O
class PieProportionalObj(Menu):
    bl_idname = "pie.proportional_obj"
    bl_label = "Pie Proportional Obj"

    def draw(self, context):
        layout = self.layout
        pie = layout.menu_pie()
        # 4 - LEFT
        pie.operator("proportional_obj.sphere", text="Sphere", icon='SPHERECURVE')
        # 6 - RIGHT
        pie.operator("proportional_obj.root", text="Root", icon='ROOTCURVE')
        # 2 - BOTTOM
        pie.operator("proportional_obj.smooth", text="Smooth", icon='SMOOTHCURVE')
        # 8 - TOP
        pie.prop(context.tool_settings, "use_proportional_edit_objects", text="Proportional On/Off")
        # 7 - TOP - LEFT
        pie.operator("proportional_obj.linear", text="Linear", icon='LINCURVE')
        # 9 - TOP - RIGHT
        pie.operator("proportional_obj.sharp", text="Sharp", icon='SHARPCURVE')
        # 1 - BOTTOM - LEFT
        pie.operator("proportional_obj.constant", text="Constant", icon='NOCURVE')
        # 3 - BOTTOM - RIGHT
        pie.operator("proportional_obj.random", text="Random", icon='RNDCURVE')


# Pie ProportionalEditEdt - O
class PieProportionalEdt(Menu):
    bl_idname = "pie.proportional_edt"
    bl_label = "Pie Proportional Edit"

    def draw(self, context):
        layout = self.layout
        pie = layout.menu_pie()
        # 4 - LEFT
        pie.operator("proportional_edt.connected", text="Connected", icon='PROP_CON')
        # 6 - RIGHT
        pie.operator("proportional_edt.projected", text="Projected", icon='PROP_ON')
        # 2 - BOTTOM
        pie.operator("proportional_edt.smooth", text="Smooth", icon='SMOOTHCURVE')
        # 8 - TOP
        pie.operator("proportional_edt.active", text="Proportional On/Off", icon='PROP_ON')
        # 7 - TOP - LEFT
        pie.operator("proportional_edt.sphere", text="Sphere", icon='SPHERECURVE')
        # 9 - TOP - RIGHT
        pie.operator("proportional_edt.root", text="Root", icon='ROOTCURVE')
        # 1 - BOTTOM - LEFT
        pie.operator("proportional_edt.constant", text="Constant", icon='NOCURVE')
        # 3 - BOTTOM - RIGHT
        pie.menu("pie.proportional_more", text="More", icon='LINCURVE')


# Pie ProportionalEditEdt - O
class PieProportionalMore(Menu):
    bl_idname = "pie.proportional_more"
    bl_label = "Pie Proportional More"

    def draw(self, context):
        layout = self.layout
        pie = layout.menu_pie()
        box = pie.split().column()
        box.operator("proportional_edt.linear", text="Linear", icon='LINCURVE')
        box.operator("proportional_edt.sharp", text="Sharp", icon='SHARPCURVE')
        box.operator("proportional_edt.random", text="Random", icon='RNDCURVE')


classes = (
    ProportionalEditObj,
    ProportionalSmoothObj,
    ProportionalSphereObj,
    ProportionalRootObj,
    ProportionalSharpObj,
    ProportionalLinearObj,
    ProportionalConstantObj,
    ProportionalRandomObj,
    ProportionalEditEdt,
    ProportionalConnectedEdt,
    ProportionalProjectedEdt,
    ProportionalSmoothEdt,
    ProportionalSphereEdt,
    ProportionalRootEdt,
    ProportionalSharpEdt,
    ProportionalLinearEdt,
    ProportionalConstantEdt,
    ProportionalRandomEdt,
    PieProportionalObj,
    PieProportionalEdt,
    PieProportionalMore,
    )

addon_keymaps = []


def register():
    for cls in classes:
        bpy.utils.register_class(cls)

    wm = bpy.context.window_manager
    if wm.keyconfigs.addon:
        # ProportionalEditObj
        km = wm.keyconfigs.addon.keymaps.new(name='Object Mode')
        kmi = km.keymap_items.new('wm.call_menu_pie', 'O', 'PRESS')
        kmi.properties.name = "pie.proportional_obj"
        addon_keymaps.append((km, kmi))

        # ProportionalEditEdt
        km = wm.keyconfigs.addon.keymaps.new(name='Mesh')
        kmi = km.keymap_items.new('wm.call_menu_pie', 'O', 'PRESS')
        kmi.properties.name = "pie.proportional_edt"
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
