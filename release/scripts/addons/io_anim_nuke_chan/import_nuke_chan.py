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

""" This script is an importer for the nuke's .chan files"""

from mathutils import Vector, Matrix, Euler
from math import radians


def read_chan(context, filepath, z_up, rot_ord, sensor_width, sensor_height):

    # get the active object
    scene = context.scene
    obj = context.active_object
    camera = obj.data if obj.type == 'CAMERA' else None

    # prepare the correcting matrix
    rot_mat = Matrix.Rotation(radians(90.0), 4, 'X').to_4x4()

    # read the file
    filehandle = open(filepath, 'r')

    # iterate throug the files lines
    for line in filehandle:
        # reset the target objects matrix
        # (the one from whitch one we'll extract the final transforms)
        m_trans_mat = Matrix()

        # strip the line
        data = line.split()

        # test if the line is not commented out
        if data and not data[0].startswith("#"):

            # set the frame number basing on the chan file
            scene.frame_set(int(data[0]))

            # read the translation values from the first three columns of line
            v_transl = Vector((float(data[1]),
                               float(data[2]),
                               float(data[3])))
            translation_mat = Matrix.Translation(v_transl)
            translation_mat.to_4x4()

            # read the rotations, and set the rotation order basing on the
            # order set during the export (it's not being saved in the chan
            # file you have to keep it noted somewhere
            # the actual objects rotation order doesn't matter since the
            # rotations are being extracted from the matrix afterwards
            e_rot = Euler((radians(float(data[4])),
                           radians(float(data[5])),
                           radians(float(data[6]))))
            e_rot.order = rot_ord
            mrot_mat = e_rot.to_matrix()
            mrot_mat.resize_4x4()

            # merge the rotation and translation
            m_trans_mat = translation_mat * mrot_mat

            # correct the world space
            # (nuke's and blenders scene spaces are different)
            if z_up:
                m_trans_mat = rot_mat * m_trans_mat

            # break the matrix into a set of the coordinates
            trns = m_trans_mat.decompose()

            # set the location and the location's keyframe
            obj.location = trns[0]
            obj.keyframe_insert("location")

            # convert the rotation to euler angles (or not)
            # basing on the objects rotation mode
            if obj.rotation_mode == 'QUATERNION':
                obj.rotation_quaternion = trns[1]
                obj.keyframe_insert("rotation_quaternion")
            elif obj.rotation_mode == 'AXIS_ANGLE':
                tmp_rot = trns[1].to_axis_angle()
                obj.rotation_axis_angle = (tmp_rot[1], *tmp_rot[0])
                obj.keyframe_insert("rotation_axis_angle")
                del tmp_rot
            else:
                obj.rotation_euler = trns[1].to_euler(obj.rotation_mode)
                obj.keyframe_insert("rotation_euler")

            # check if the object is camera and fov data is present
            if camera and len(data) > 7:
                camera.sensor_fit = 'HORIZONTAL'
                camera.sensor_width = sensor_width
                camera.sensor_height = sensor_height
                camera.angle_y = radians(float(data[7]))
                camera.keyframe_insert("lens")
    filehandle.close()

    return {'FINISHED'}
