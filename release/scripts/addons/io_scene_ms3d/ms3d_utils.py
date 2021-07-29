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

###############################################################################
#234567890123456789012345678901234567890123456789012345678901234567890123456789
#--------1---------2---------3---------4---------5---------6---------7---------


# ##### BEGIN COPYRIGHT BLOCK #####
#
# initial script copyright (c)2011-2013 Alexander Nussbaumer
#
# ##### END COPYRIGHT BLOCK #####


#import python stuff
from os import (
        path
        )


# import io_scene_ms3d stuff
from io_scene_ms3d.ms3d_strings import (
        ms3d_str,
        )


#import blender stuff
from bpy import (
        ops,
        )


###############################################################################
def enable_edit_mode(enable, blender_context):
    if blender_context.active_object is None \
            or not blender_context.active_object.type in {'MESH', 'ARMATURE', }:
        return

    if enable:
        modeString = 'EDIT'
    else:
        modeString = 'OBJECT'

    if ops.object.mode_set.poll():
        ops.object.mode_set(mode=modeString)


###############################################################################
def enable_pose_mode(enable, blender_context):
    if blender_context.active_object is None \
            or not blender_context.active_object.type in {'ARMATURE', }:
        return

    if enable:
        modeString = 'POSE'
    else:
        modeString = 'OBJECT'

    if ops.object.mode_set.poll():
        ops.object.mode_set(mode=modeString)


###############################################################################
def select_all(select):
    if select:
        actionString = 'SELECT'
    else:
        actionString = 'DESELECT'

    if ops.object.select_all.poll():
        ops.object.select_all(action=actionString)

    if ops.mesh.select_all.poll():
        ops.mesh.select_all(action=actionString)

    if ops.pose.select_all.poll():
        ops.pose.select_all(action=actionString)


###############################################################################
def pre_setup_environment(porter, blender_context):
    # inject undo to porter
    # and turn off undo
    porter.undo = blender_context.user_preferences.edit.use_global_undo
    blender_context.user_preferences.edit.use_global_undo = False

    # inject active_object to self
    porter.active_object = blender_context.scene.objects.active

    # change to a well defined mode
    enable_edit_mode(True, blender_context)

    # enable face-selection-mode
    blender_context.tool_settings.mesh_select_mode = (False, False, True)

    # change back to object mode
    enable_edit_mode(False, blender_context)


###############################################################################
def post_setup_environment(porter, blender_context):
    # restore active object
    blender_context.scene.objects.active = porter.active_object

    if not blender_context.scene.objects.active \
            and blender_context.selected_objects:
        blender_context.scene.objects.active \
                = blender_context.selected_objects[0]

    # restore pre operator undo state
    blender_context.user_preferences.edit.use_global_undo = porter.undo


###############################################################################
def get_edge_split_modifier_add_if(blender_mesh_object):
    blender_modifier = blender_mesh_object.modifiers.get(
            ms3d_str['OBJECT_MODIFIER_SMOOTHING_GROUP'])

    if blender_modifier is None:
        blender_modifier = blender_mesh_object.modifiers.new(
                ms3d_str['OBJECT_MODIFIER_SMOOTHING_GROUP'],
                type='EDGE_SPLIT')
        blender_modifier.show_expanded = False
        blender_modifier.use_edge_angle = False
        blender_modifier.use_edge_sharp = True

        blender_mesh_object.data.show_edge_seams = True
        blender_mesh_object.data.show_edge_sharp = True

    return blender_modifier


###########################################################################
def rotation_matrix(v_track, v_up):
    ## rotation matrix from two vectors
    ## http://gamedev.stackexchange.com/questions/20097/how-to-calculate-a-3x3-rotation-matrix-from-2-direction-vectors
    ## http://www.fastgraph.com/makegames/3drotation/
    matrix = Matrix().to_3x3()

    c1 = v_track
    c1.normalize()

    c0 = c1.cross(v_up)
    c0.normalize()

    c2 = c0.cross(c1)
    c2.normalize()

    matrix.col[0] = c0
    matrix.col[1] = c1
    matrix.col[2] = c2

    return matrix


###############################################################################
def matrix_difference(mat_src, mat_dst):
    mat_dst_inv = mat_dst.inverted()
    return mat_dst_inv * mat_src


###############################################################################

###############################################################################
#234567890123456789012345678901234567890123456789012345678901234567890123456789
#--------1---------2---------3---------4---------5---------6---------7---------
# ##### END OF FILE #####
