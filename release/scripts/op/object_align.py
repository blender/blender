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


def align_objects(align_x, align_y, align_z, relative_to):

    from Mathutils import Vector

    cursor = bpy.context.scene.cursor_location

    for obj in bpy.context.selected_objects:
        
        loc_world = obj.location
        bb_world = [obj.matrix * Vector(v[:]) for v in obj.bound_box]
    
        Left_Up_Front = bb_world[1]
        Right_Down_Back = bb_world[7]
    
        center_x = ( Left_Up_Front[0] + Right_Down_Back[0] ) / 2
        center_y = ( Left_Up_Front[1] + Right_Down_Back[1] ) / 2
        center_z = ( Left_Up_Front[2] + Right_Down_Back[2] ) / 2

        obj_loc = obj.location
    
        if align_x:

            obj_x = obj_loc[0] - center_x

            if relative_to == 'OPT_1':
                loc_x = obj_x

            elif relative_to == 'OPT_2':
                print (cursor[0])
                loc_x = obj_x + cursor[0]
                
            obj.location[0] = loc_x


        if align_y:

            obj_y = obj_loc[1] - center_y

            if relative_to == 'OPT_1':
                loc_y = obj_y

            elif relative_to == 'OPT_2':
                print (cursor[1])
                loc_y = obj_y + cursor[1]
                
            obj.location[1] = loc_y


        if align_z:

            obj_z = obj_loc[2] - center_z

            if relative_to == 'OPT_1':
                loc_z = obj_z

            elif relative_to == 'OPT_2':
                print (cursor[2])
                loc_z = obj_z + cursor[2]
                
            obj.location[2] = loc_z


from bpy.props import *


class TestCrap(bpy.types.Operator):
    '''Align Objects'''
    bl_idname = "object.align"
    bl_label = "Align Objets"
    bl_register = True
    bl_undo = True

    relative_to = bpy.props.EnumProperty(items=(
            ('OPT_1', "Scene Origin", "blahblah"),
            ('OPT_2', "3D Cursor", "blahblah")
            ),
        name="Relative To:",
        description="blahbkah",
        default='OPT_1')
    
    align_x = BoolProperty(name="Align X",
        description="Align in the X axis", default=False)

    align_y = BoolProperty(name="Align Y",
        description="Align in the Y axis", default=False)

    align_z = BoolProperty(name="Align Z",
        description="Align in the Z axis", default=False)

    def execute(self, context):
    
        relative_to = self.properties.relative_to
        align_x = self.properties.align_x
        align_y = self.properties.align_y
        align_z = self.properties.align_z

        align_objects(align_x, align_y, align_z, relative_to)

        return {'FINISHED'}


bpy.types.register(TestCrap)

def menu_func(self, context):
    if context.mode == 'OBJECT':
        self.layout.operator(TestCrap.bl_idname,
        text="Align Objects")

bpy.types.VIEW3D_MT_transform.append(menu_func)