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


def align_objects(align_x, align_y, align_z):

    from Mathutils import Vector

    for obj in bpy.context.selected_objects:
        
        loc_world = obj.location
        bb_world = [obj.matrix * Vector(v[:]) for v in obj.bound_box]
    
        Left_Up_Front = bb_world[1]
        Right_Down_Back = bb_world[7]
    
        center_x = ( Left_Up_Front[0] + Right_Down_Back[0] ) / 2
        center_y = ( Left_Up_Front[1] + Right_Down_Back[1] ) / 2
        center_z = ( Left_Up_Front[2] + Right_Down_Back[2] ) / 2
    
        if align_x:
            obj.location[0] = obj.location[0] - center_x
        if align_y:
            obj.location[1] = obj.location[1] - center_y
        if align_z:
            obj.location[2] = obj.location[2] - center_z


from bpy.props import *


class TestCrap(bpy.types.Operator):
    '''Align Objects'''
    bl_idname = "object.align"
    bl_label = "Align Objets"
    bl_register = True
    bl_undo = True
    
    align_x = BoolProperty(name="Align X",
        description="Align in the X axis", default=False)

    align_y = BoolProperty(name="Align Y",
        description="Align in the Y axis", default=False)

    align_z = BoolProperty(name="Align Z",
        description="Align in the Z axis", default=False)

    def execute(self, context):
    
        align_X = self.properties.align_x
        align_Y = self.properties.align_y
        align_Z = self.properties.align_z

        align_objects(align_x, align_y, align_z)

        return {'FINISHED'}


bpy.types.register(TestCrap)

def menu_func(self, context):
    if context.mode == 'OBJECT':
        self.layout.operator(TestCrap.bl_idname,
        text="Align Objects")

bpy.types.VIEW3D_MT_transform.append(menu_func)