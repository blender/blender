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
    "name": "Hotkey: 'Alt X'",
    "description": "V/E/F Align tools",
    "author": "pitiwazou, meta-androcto",
    "version": (0, 1, 2),
    "blender": (2, 77, 0),
    "location": "Mesh Edit Mode",
    "warning": "",
    "wiki_url": "",
    "category": "Edit Align Pie"
    }

import bpy
from bpy.types import (
        Menu,
        Operator,
        )
from bpy.props import EnumProperty


# Pie Align - Alt + X
class PieAlign(Menu):
    bl_idname = "pie.align"
    bl_label = "Pie Align"

    def draw(self, context):
        layout = self.layout
        pie = layout.menu_pie()
        # 4 - LEFT
        pie.operator("align.selected2xyz",
                    text="Align X", icon='TRIA_LEFT').axis = 'X'
        # 6 - RIGHT
        pie.operator("align.selected2xyz",
                    text="Align Z", icon='TRIA_DOWN').axis = 'Z'
        # 2 - BOTTOM
        pie.operator("align.selected2xyz",
                    text="Align Y", icon='PLUS').axis = 'Y'
        # 8 - TOP
        pie.operator("align.2xyz", text="Align To Y-0").axis = '1'
        # 7 - TOP - LEFT
        pie.operator("align.2xyz", text="Align To X-0").axis = '0'
        # 9 - TOP - RIGHT
        pie.operator("align.2xyz", text="Align To Z-0").axis = '2'
        # 1 - BOTTOM - LEFT
        pie.separator()
        # 3 - BOTTOM - RIGHT
        # pie.menu("align.xyz")
        box = pie.split().box().column()

        row = box.row(align=True)
        row.label("X")
        align_1 = row.operator("alignxyz.all", text="Neg")
        align_1.axis = '0'
        align_1.side = 'NEGATIVE'
        align_2 = row.operator("alignxyz.all", text="Pos")
        align_2.axis = '0'
        align_2.side = 'POSITIVE'

        row = box.row(align=True)
        row.label("Y")
        align_3 = row.operator("alignxyz.all", text="Neg")
        align_3.axis = '1'
        align_3.side = 'NEGATIVE'
        align_4 = row.operator("alignxyz.all", text="Pos")
        align_4.axis = '1'
        align_4.side = 'POSITIVE'

        row = box.row(align=True)
        row.label("Z")
        align_5 = row.operator("alignxyz.all", text="Neg")
        align_5.axis = '2'
        align_5.side = 'NEGATIVE'
        align_6 = row.operator("alignxyz.all", text="Pos")
        align_6.axis = '2'
        align_6.side = 'POSITIVE'


# Align to X, Y, Z
class AlignSelectedXYZ(Operator):
    bl_idname = "align.selected2xyz"
    bl_label = "Align to X, Y, Z"
    bl_description = "Align Selected Along the chosen axis"
    bl_options = {'REGISTER', 'UNDO'}

    axis = EnumProperty(
        name="Axis",
        items=[
            ('X', "X", "X Axis"),
            ('Y', "Y", "Y Axis"),
            ('Z', "Z", "Z Axis")
            ],
        description="Choose an axis for alignment",
        default='X'
        )

    @classmethod
    def poll(cls, context):
        obj = context.active_object
        return obj and obj.type == "MESH"

    def execute(self, context):
        values = {
            'X': [(0, 1, 1), (True, False, False)],
            'Y': [(1, 0, 1), (False, True, False)],
            'Z': [(1, 1, 0), (False, False, True)]
            }
        chosen_value = values[self.axis][0]
        constraint_value = values[self.axis][1]
        for vert in bpy.context.object.data.vertices:
            bpy.ops.transform.resize(
                    value=chosen_value, constraint_axis=constraint_value,
                    constraint_orientation='GLOBAL',
                    mirror=False, proportional='DISABLED',
                    proportional_edit_falloff='SMOOTH',
                    proportional_size=1
                    )
        return {'FINISHED'}


# ################# #
#    Align To 0     #
# ################# #

class AlignToXYZ0(Operator):
    bl_idname = "align.2xyz"
    bl_label = "Align To X, Y or Z = 0"
    bl_description = "Align Active Object To a chosen X, Y or Z equals 0 Location"
    bl_options = {'REGISTER', 'UNDO'}

    axis = EnumProperty(
            name="Axis",
            items=[
                ('0', "X", "X Axis"),
                ('1', "Y", "Y Axis"),
                ('2', "Z", "Z Axis")
                ],
            description="Choose an axis for alignment",
            default='0'
            )

    @classmethod
    def poll(cls, context):
        obj = context.active_object
        return obj and obj.type == "MESH"

    def execute(self, context):
        bpy.ops.object.mode_set(mode='OBJECT')
        align = int(self.axis)
        for vert in bpy.context.object.data.vertices:
            if vert.select:
                vert.co[align] = 0
        bpy.ops.object.editmode_toggle()

        return {'FINISHED'}


# Align X Left
class AlignXYZAll(Operator):
    bl_idname = "alignxyz.all"
    bl_label = "Align to Front/Back Axis"
    bl_description = "Align to a Front or Back along the chosen Axis"
    bl_options = {'REGISTER', 'UNDO'}

    axis = EnumProperty(
            name="Axis",
            items=[
                ('0', "X", "X Axis"),
                ('1', "Y", "Y Axis"),
                ('2', "Z", "Z Axis")
                ],
            description="Choose an axis for alignment",
            default='0'
            )
    side = EnumProperty(
            name="Side",
            items=[
                ('POSITIVE', "Front", "Align on the positive chosen axis"),
                ('NEGATIVE', "Back", "Align acriss the negative chosen axis"),
                ],
            description="Choose a side for alignment",
            default='POSITIVE'
            )

    @classmethod
    def poll(cls, context):
        obj = context.active_object
        return obj and obj.type == "MESH"

    def execute(self, context):

        bpy.ops.object.mode_set(mode='OBJECT')
        count = 0
        axe = int(self.axis)
        for vert in bpy.context.object.data.vertices:
            if vert.select:
                if count == 0:
                    maxv = vert.co[axe]
                    count += 1
                    continue
                count += 1
                if self.side == 'POSITIVE':
                    if vert.co[axe] > maxv:
                        maxv = vert.co[axe]
                else:
                    if vert.co[axe] < maxv:
                        maxv = vert.co[axe]

        bpy.ops.object.mode_set(mode='OBJECT')

        for vert in bpy.context.object.data.vertices:
            if vert.select:
                vert.co[axe] = maxv
        bpy.ops.object.mode_set(mode='EDIT')

        return {'FINISHED'}


classes = (
    PieAlign,
    AlignSelectedXYZ,
    AlignToXYZ0,
    AlignXYZAll,
    )

addon_keymaps = []


def register():
    for cls in classes:
        bpy.utils.register_class(cls)

    wm = bpy.context.window_manager
    if wm.keyconfigs.addon:
        # Align
        km = wm.keyconfigs.addon.keymaps.new(name='Mesh')
        kmi = km.keymap_items.new('wm.call_menu_pie', 'X', 'PRESS', alt=True)
        kmi.properties.name = "pie.align"
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
