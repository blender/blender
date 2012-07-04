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

# <pep8-80 compliant>

import bpy
from bpy.types import Operator
from mathutils import Vector


def GlobalBB_LQ(bb_world):

    # Initialize the variables with the 8th vertex
    left, right, front, back, down, up = (bb_world[7][0],
                                          bb_world[7][0],
                                          bb_world[7][1],
                                          bb_world[7][1],
                                          bb_world[7][2],
                                          bb_world[7][2],
                                          )

    # Test against the other 7 verts
    for i in range(7):

        # X Range
        val = bb_world[i][0]
        if val < left:
            left = val

        if val > right:
            right = val

        # Y Range
        val = bb_world[i][1]
        if val < front:
            front = val

        if val > back:
            back = val

        # Z Range
        val = bb_world[i][2]
        if val < down:
            down = val

        if val > up:
            up = val

    return (Vector((left, front, up)), Vector((right, back, down)))


def GlobalBB_HQ(obj):

    matrix_world = obj.matrix_world.copy()

    # Initialize the variables with the last vertex

    verts = obj.data.vertices

    val = matrix_world * verts[-1].co

    left, right, front, back, down, up = (val[0],
                                          val[0],
                                          val[1],
                                          val[1],
                                          val[2],
                                          val[2],
                                          )

    # Test against all other verts
    for i in range(len(verts) - 1):

        vco = matrix_world * verts[i].co

        # X Range
        val = vco[0]
        if val < left:
            left = val

        if val > right:
            right = val

        # Y Range
        val = vco[1]
        if val < front:
            front = val

        if val > back:
            back = val

        # Z Range
        val = vco[2]
        if val < down:
            down = val

        if val > up:
            up = val

    return Vector((left, front, up)), Vector((right, back, down))


def align_objects(align_x,
                  align_y,
                  align_z,
                  align_mode,
                  relative_to,
                  bb_quality):

    cursor = bpy.context.scene.cursor_location

    Left_Front_Up_SEL = [0.0, 0.0, 0.0]
    Right_Back_Down_SEL = [0.0, 0.0, 0.0]

    flag_first = True

    objs = []

    for obj in bpy.context.selected_objects:
        matrix_world = obj.matrix_world.copy()
        bb_world = [matrix_world * Vector(v[:]) for v in obj.bound_box]
        objs.append((obj, bb_world))

    if not objs:
        return False

    for obj, bb_world in objs:

        if bb_quality and obj.type == 'MESH':
            GBB = GlobalBB_HQ(obj)
        else:
            GBB = GlobalBB_LQ(bb_world)

        Left_Front_Up = GBB[0]
        Right_Back_Down = GBB[1]

        # Active Center

        if obj == bpy.context.active_object:

            center_active_x = (Left_Front_Up[0] + Right_Back_Down[0]) / 2.0
            center_active_y = (Left_Front_Up[1] + Right_Back_Down[1]) / 2.0
            center_active_z = (Left_Front_Up[2] + Right_Back_Down[2]) / 2.0

            size_active_x = (Right_Back_Down[0] - Left_Front_Up[0]) / 2.0
            size_active_y = (Right_Back_Down[1] - Left_Front_Up[1]) / 2.0
            size_active_z = (Left_Front_Up[2] - Right_Back_Down[2]) / 2.0

        # Selection Center

        if flag_first:
            flag_first = False

            Left_Front_Up_SEL[0] = Left_Front_Up[0]
            Left_Front_Up_SEL[1] = Left_Front_Up[1]
            Left_Front_Up_SEL[2] = Left_Front_Up[2]

            Right_Back_Down_SEL[0] = Right_Back_Down[0]
            Right_Back_Down_SEL[1] = Right_Back_Down[1]
            Right_Back_Down_SEL[2] = Right_Back_Down[2]

        else:
            # X axis
            if Left_Front_Up[0] < Left_Front_Up_SEL[0]:
                Left_Front_Up_SEL[0] = Left_Front_Up[0]
            # Y axis
            if Left_Front_Up[1] < Left_Front_Up_SEL[1]:
                Left_Front_Up_SEL[1] = Left_Front_Up[1]
            # Z axis
            if Left_Front_Up[2] > Left_Front_Up_SEL[2]:
                Left_Front_Up_SEL[2] = Left_Front_Up[2]

            # X axis
            if Right_Back_Down[0] > Right_Back_Down_SEL[0]:
                Right_Back_Down_SEL[0] = Right_Back_Down[0]
            # Y axis
            if Right_Back_Down[1] > Right_Back_Down_SEL[1]:
                Right_Back_Down_SEL[1] = Right_Back_Down[1]
            # Z axis
            if Right_Back_Down[2] < Right_Back_Down_SEL[2]:
                Right_Back_Down_SEL[2] = Right_Back_Down[2]

    center_sel_x = (Left_Front_Up_SEL[0] + Right_Back_Down_SEL[0]) / 2.0
    center_sel_y = (Left_Front_Up_SEL[1] + Right_Back_Down_SEL[1]) / 2.0
    center_sel_z = (Left_Front_Up_SEL[2] + Right_Back_Down_SEL[2]) / 2.0

    # Main Loop

    for obj, bb_world in objs:
        matrix_world = obj.matrix_world.copy()
        bb_world = [matrix_world * Vector(v[:]) for v in obj.bound_box]

        if bb_quality and obj.type == 'MESH':
            GBB = GlobalBB_HQ(obj)
        else:
            GBB = GlobalBB_LQ(bb_world)

        Left_Front_Up = GBB[0]
        Right_Back_Down = GBB[1]

        center_x = (Left_Front_Up[0] + Right_Back_Down[0]) / 2.0
        center_y = (Left_Front_Up[1] + Right_Back_Down[1]) / 2.0
        center_z = (Left_Front_Up[2] + Right_Back_Down[2]) / 2.0

        positive_x = Right_Back_Down[0]
        positive_y = Right_Back_Down[1]
        positive_z = Left_Front_Up[2]

        negative_x = Left_Front_Up[0]
        negative_y = Left_Front_Up[1]
        negative_z = Right_Back_Down[2]

        obj_loc = obj.location

        if align_x:

            # Align Mode

            if relative_to == 'OPT_4':  # Active relative
                if align_mode == 'OPT_1':
                    obj_x = obj_loc[0] - negative_x - size_active_x

                elif align_mode == 'OPT_3':
                    obj_x = obj_loc[0] - positive_x + size_active_x

            else:  # Everything else relative
                if align_mode == 'OPT_1':
                    obj_x = obj_loc[0] - negative_x

                elif align_mode == 'OPT_3':
                    obj_x = obj_loc[0] - positive_x

            if align_mode == 'OPT_2':  # All relative
                obj_x = obj_loc[0] - center_x

            # Relative To

            if relative_to == 'OPT_1':
                loc_x = obj_x

            elif relative_to == 'OPT_2':
                loc_x = obj_x + cursor[0]

            elif relative_to == 'OPT_3':
                loc_x = obj_x + center_sel_x

            elif relative_to == 'OPT_4':
                loc_x = obj_x + center_active_x

            obj.location[0] = loc_x

        if align_y:
            # Align Mode

            if relative_to == 'OPT_4':  # Active relative
                if align_mode == 'OPT_1':
                    obj_y = obj_loc[1] - negative_y - size_active_y

                elif align_mode == 'OPT_3':
                    obj_y = obj_loc[1] - positive_y + size_active_y

            else:  # Everything else relative
                if align_mode == 'OPT_1':
                    obj_y = obj_loc[1] - negative_y

                elif align_mode == 'OPT_3':
                    obj_y = obj_loc[1] - positive_y

            if align_mode == 'OPT_2':  # All relative
                obj_y = obj_loc[1] - center_y

            # Relative To

            if relative_to == 'OPT_1':
                loc_y = obj_y

            elif relative_to == 'OPT_2':
                loc_y = obj_y + cursor[1]

            elif relative_to == 'OPT_3':
                loc_y = obj_y + center_sel_y

            elif relative_to == 'OPT_4':
                loc_y = obj_y + center_active_y

            obj.location[1] = loc_y

        if align_z:
            # Align Mode
            if relative_to == 'OPT_4':  # Active relative
                if align_mode == 'OPT_1':
                    obj_z = obj_loc[2] - negative_z - size_active_z

                elif align_mode == 'OPT_3':
                    obj_z = obj_loc[2] - positive_z + size_active_z

            else:  # Everything else relative
                if align_mode == 'OPT_1':
                    obj_z = obj_loc[2] - negative_z

                elif align_mode == 'OPT_3':
                    obj_z = obj_loc[2] - positive_z

            if align_mode == 'OPT_2':  # All relative
                obj_z = obj_loc[2] - center_z

            # Relative To

            if relative_to == 'OPT_1':
                loc_z = obj_z

            elif relative_to == 'OPT_2':
                loc_z = obj_z + cursor[2]

            elif relative_to == 'OPT_3':
                loc_z = obj_z + center_sel_z

            elif relative_to == 'OPT_4':
                loc_z = obj_z + center_active_z

            obj.location[2] = loc_z

    return True


from bpy.props import EnumProperty, BoolProperty


class AlignObjects(Operator):
    """Align Objects"""
    bl_idname = "object.align"
    bl_label = "Align Objects"
    bl_options = {'REGISTER', 'UNDO'}

    bb_quality = BoolProperty(
            name="High Quality",
            description=("Enables high quality calculation of the "
                         "bounding box for perfect results on complex "
                         "shape meshes with rotation/scale (Slow)"),
            default=True,
            )
    align_mode = EnumProperty(
            name="Align Mode:",
            items=(('OPT_1', "Negative Sides", ""),
                   ('OPT_2', "Centers", ""),
                   ('OPT_3', "Positive Sides", ""),
                   ),
            default='OPT_2',
            )
    relative_to = EnumProperty(
            name="Relative To:",
            items=(('OPT_1', "Scene Origin", ""),
                   ('OPT_2', "3D Cursor", ""),
                   ('OPT_3', "Selection", ""),
                   ('OPT_4', "Active", ""),
                   ),
            default='OPT_4',
            )
    align_axis = EnumProperty(
            name="Align",
            description="Align to axis",
            items=(('X', "X", ""),
                   ('Y', "Y", ""),
                   ('Z', "Z", ""),
                   ),
            options={'ENUM_FLAG'},
            )

    @classmethod
    def poll(cls, context):
        return context.mode == 'OBJECT'

    def execute(self, context):
        align_axis = self.align_axis
        ret = align_objects('X' in align_axis,
                            'Y' in align_axis,
                            'Z' in align_axis,
                            self.align_mode,
                            self.relative_to,
                            self.bb_quality)

        if not ret:
            self.report({'WARNING'}, "No objects with bound-box selected")
            return {'CANCELLED'}
        else:
            return {'FINISHED'}
