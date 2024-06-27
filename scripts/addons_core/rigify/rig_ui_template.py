# SPDX-FileCopyrightText: 2010-2022 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy

from collections import OrderedDict
from typing import Union, Optional, Any

from .utils.animation import SCRIPT_REGISTER_BAKE, SCRIPT_UTILITIES_BAKE
from .utils.mechanism import quote_property

from . import base_generate

from rna_prop_ui import rna_idprop_quote_path


UI_IMPORTS = [
    'import bpy',
    'import math',
    'import json',
    'import collections',
    'import traceback',
    'from math import pi',
    'from bpy.props import StringProperty',
    'from mathutils import Euler, Matrix, Quaternion, Vector',
    'from rna_prop_ui import rna_idprop_quote_path',
]


UI_BASE_UTILITIES = '''
rig_id = "%s"


############################
## Math utility functions ##
############################

def perpendicular_vector(v):
    """ Returns a vector that is perpendicular to the one given.
        The returned vector is _not_ guaranteed to be normalized.
    """
    # Create a vector that is not aligned with v.
    # It doesn't matter what vector.  Just any vector
    # that's guaranteed to not be pointing in the same
    # direction.
    if abs(v[0]) < abs(v[1]):
        tv = Vector((1,0,0))
    else:
        tv = Vector((0,1,0))

    # Use cross product to generate a vector perpendicular to
    # both tv and (more importantly) v.
    return v.cross(tv)


def rotation_difference(mat1, mat2):
    """ Returns the shortest-path rotational difference between two
        matrices.
    """
    q1 = mat1.to_quaternion()
    q2 = mat2.to_quaternion()
    angle = math.acos(min(1,max(-1,q1.dot(q2)))) * 2
    if angle > pi:
        angle = -angle + (2*pi)
    return angle

def find_min_range(f,start_angle,delta=pi/8):
    """ finds the range where lies the minimum of function f applied on bone_ik and bone_fk
        at a certain angle.
    """
    angle = start_angle
    while (angle > (start_angle - 2*pi)) and (angle < (start_angle + 2*pi)):
        l_dist = f(angle-delta)
        c_dist = f(angle)
        r_dist = f(angle+delta)
        if min((l_dist,c_dist,r_dist)) == c_dist:
            return (angle-delta,angle+delta)
        else:
            angle=angle+delta

def ternarySearch(f, left, right, absolutePrecision):
    """
    Find minimum of uni-modal function f() within [left, right]
    To find the maximum, revert the if/else statement or revert the comparison.
    """
    while True:
        #left and right are the current bounds; the maximum is between them
        if abs(right - left) < absolutePrecision:
            return (left + right)/2

        leftThird = left + (right - left)/3
        rightThird = right - (right - left)/3

        if f(leftThird) > f(rightThird):
            left = leftThird
        else:
            right = rightThird

def flatten_children(iterable):
    """Enumerate the iterator items as well as their children in the tree order."""
    for item in iterable:
        yield item
        yield from flatten_children(item.children)

'''

UTILITIES_FUNC_COMMON_IK_FK = ['''
#########################################
## "Visual Transform" helper functions ##
#########################################

def get_pose_matrix_in_other_space(mat, pose_bone):
    """ Returns the transform matrix relative to pose_bone's current
        transform space.  In other words, presuming that mat is in
        armature space, slapping the returned matrix onto pose_bone
        should give it the armature-space transforms of mat.
    """
    return pose_bone.id_data.convert_space(matrix=mat, pose_bone=pose_bone, from_space='POSE', to_space='LOCAL')


def convert_pose_matrix_via_rest_delta(mat, from_bone, to_bone):
    """Convert pose of one bone to another bone, preserving the rest pose difference between them."""
    return mat @ from_bone.bone.matrix_local.inverted() @ to_bone.bone.matrix_local


def convert_pose_matrix_via_pose_delta(mat, from_bone, to_bone):
    """Convert pose of one bone to another bone, preserving the current pose difference between them."""
    return mat @ from_bone.matrix.inverted() @ to_bone.matrix


def get_local_pose_matrix(pose_bone):
    """ Returns the local transform matrix of the given pose bone.
    """
    return get_pose_matrix_in_other_space(pose_bone.matrix, pose_bone)


def set_pose_translation(pose_bone, mat):
    """ Sets the pose bone's translation to the same translation as the given matrix.
        Matrix should be given in bone's local space.
    """
    pose_bone.location = mat.to_translation()


def set_pose_rotation(pose_bone, mat):
    """ Sets the pose bone's rotation to the same rotation as the given matrix.
        Matrix should be given in bone's local space.
    """
    q = mat.to_quaternion()

    if pose_bone.rotation_mode == 'QUATERNION':
        pose_bone.rotation_quaternion = q
    elif pose_bone.rotation_mode == 'AXIS_ANGLE':
        pose_bone.rotation_axis_angle[0] = q.angle
        pose_bone.rotation_axis_angle[1] = q.axis[0]
        pose_bone.rotation_axis_angle[2] = q.axis[1]
        pose_bone.rotation_axis_angle[3] = q.axis[2]
    else:
        pose_bone.rotation_euler = q.to_euler(pose_bone.rotation_mode)


def set_pose_scale(pose_bone, mat):
    """ Sets the pose bone's scale to the same scale as the given matrix.
        Matrix should be given in bone's local space.
    """
    pose_bone.scale = mat.to_scale()


def match_pose_translation(pose_bone, target_bone):
    """ Matches pose_bone's visual translation to target_bone's visual
        translation.
        This function assumes you are in pose mode on the relevant armature.
    """
    mat = get_pose_matrix_in_other_space(target_bone.matrix, pose_bone)
    set_pose_translation(pose_bone, mat)


def match_pose_rotation(pose_bone, target_bone):
    """ Matches pose_bone's visual rotation to target_bone's visual
        rotation.
        This function assumes you are in pose mode on the relevant armature.
    """
    mat = get_pose_matrix_in_other_space(target_bone.matrix, pose_bone)
    set_pose_rotation(pose_bone, mat)


def match_pose_scale(pose_bone, target_bone):
    """ Matches pose_bone's visual scale to target_bone's visual
        scale.
        This function assumes you are in pose mode on the relevant armature.
    """
    mat = get_pose_matrix_in_other_space(target_bone.matrix, pose_bone)
    set_pose_scale(pose_bone, mat)


##############################
## IK/FK snapping functions ##
##############################

def correct_rotation(view_layer, bone_ik, target_matrix, *, ctrl_ik=None):
    """ Corrects the ik rotation in ik2fk snapping functions
    """

    axis = target_matrix.to_3x3().col[1].normalized()
    ctrl_ik = ctrl_ik or bone_ik

    def distance(angle):
        # Rotate the bone and return the actual angle between bones
        ctrl_ik.rotation_euler[1] = angle
        view_layer.update()

        return -(bone_ik.vector.normalized().dot(axis))

    if ctrl_ik.rotation_mode in {'QUATERNION', 'AXIS_ANGLE'}:
        ctrl_ik.rotation_mode = 'ZXY'

    start_angle = ctrl_ik.rotation_euler[1]

    alpha_range = find_min_range(distance, start_angle)
    alpha_min = ternarySearch(distance, alpha_range[0], alpha_range[1], pi / 180)

    ctrl_ik.rotation_euler[1] = alpha_min
    view_layer.update()


def correct_scale(view_layer, bone_ik, target_matrix, *, ctrl_ik=None):
    """ Correct the scale of the base IK bone. """
    input_scale = target_matrix.to_scale()
    ctrl_ik = ctrl_ik or bone_ik

    for i in range(3):
        cur_scale = bone_ik.matrix.to_scale()

        ctrl_ik.scale = [
            v * i / c for v, i, c in zip(bone_ik.scale, input_scale, cur_scale)
        ]

        view_layer.update()

        if all(abs((c - i)/i) < 0.01 for i, c in zip(input_scale, cur_scale)):
            break


def match_pole_target(view_layer, ik_first, ik_last, pole, match_bone_matrix, length):
    """ Places an IK chain's pole target to match ik_first's
        transforms to match_bone.  All bones should be given as pose bones.
        You need to be in pose mode on the relevant armature object.
        ik_first: first bone in the IK chain
        ik_last:  last bone in the IK chain
        pole:  pole target bone for the IK chain
        match_bone:  bone to match ik_first to (probably first bone in a matching FK chain)
        length:  distance pole target should be placed from the chain center
    """
    a = ik_first.matrix.to_translation()
    b = ik_last.matrix.to_translation() + ik_last.vector

    # Vector from the head of ik_first to the
    # tip of ik_last
    ikv = b - a

    # Get a vector perpendicular to ikv
    pv = perpendicular_vector(ikv).normalized() * length

    def set_pole(pvi):
        """ Set pole target's position based on a vector
            from the arm center line.
        """
        # Translate pvi into armature space
        pole_loc = a + (ikv/2) + pvi

        # Set pole target to location
        mat = get_pose_matrix_in_other_space(Matrix.Translation(pole_loc), pole)
        set_pose_translation(pole, mat)

        view_layer.update()

    set_pole(pv)

    # Get the rotation difference between ik_first and match_bone
    angle = rotation_difference(ik_first.matrix, match_bone_matrix)

    # Try compensating for the rotation difference in both directions
    pv1 = Matrix.Rotation(angle, 4, ikv) @ pv
    set_pole(pv1)
    ang1 = rotation_difference(ik_first.matrix, match_bone_matrix)

    pv2 = Matrix.Rotation(-angle, 4, ikv) @ pv
    set_pole(pv2)
    ang2 = rotation_difference(ik_first.matrix, match_bone_matrix)

    # Do the one with the smaller angle
    if ang1 < ang2:
        set_pole(pv1)

##########
## Misc ##
##########

def parse_bone_names(names_string):
    if names_string[0] == '[' and names_string[-1] == ']':
        return eval(names_string)
    else:
        return names_string

''']

# noinspection SpellCheckingInspection
UTILITIES_FUNC_OLD_ARM_FKIK = ['''
######################
## IK Arm functions ##
######################

def fk2ik_arm(obj, fk, ik):
    """ Matches the fk bones in an arm rig to the ik bones.
        obj: armature object
        fk:  list of fk bone names
        ik:  list of ik bone names
    """
    view_layer = bpy.context.view_layer
    uarm  = obj.pose.bones[fk[0]]
    farm  = obj.pose.bones[fk[1]]
    hand  = obj.pose.bones[fk[2]]
    uarmi = obj.pose.bones[ik[0]]
    farmi = obj.pose.bones[ik[1]]
    handi = obj.pose.bones[ik[2]]

    if 'auto_stretch' in handi.keys():
        # This is kept for compatibility with legacy rigify Human
        # Stretch
        if handi['auto_stretch'] == 0.0:
            uarm['stretch_length'] = handi['stretch_length']
        else:
            diff = (uarmi.vector.length + farmi.vector.length) / (uarm.vector.length + farm.vector.length)
            uarm['stretch_length'] *= diff

        # Upper arm position
        match_pose_rotation(uarm, uarmi)
        match_pose_scale(uarm, uarmi)
        view_layer.update()

        # Forearm position
        match_pose_rotation(farm, farmi)
        match_pose_scale(farm, farmi)
        view_layer.update()

        # Hand position
        match_pose_rotation(hand, handi)
        match_pose_scale(hand, handi)
        view_layer.update()
    else:
        # Upper arm position
        match_pose_translation(uarm, uarmi)
        match_pose_rotation(uarm, uarmi)
        match_pose_scale(uarm, uarmi)
        view_layer.update()

        # Forearm position
        #match_pose_translation(hand, handi)
        match_pose_rotation(farm, farmi)
        match_pose_scale(farm, farmi)
        view_layer.update()

        # Hand position
        match_pose_translation(hand, handi)
        match_pose_rotation(hand, handi)
        match_pose_scale(hand, handi)
        view_layer.update()


def ik2fk_arm(obj, fk, ik):
    """ Matches the ik bones in an arm rig to the fk bones.
        obj: armature object
        fk:  list of fk bone names
        ik:  list of ik bone names
    """
    view_layer = bpy.context.view_layer
    uarm  = obj.pose.bones[fk[0]]
    farm  = obj.pose.bones[fk[1]]
    hand  = obj.pose.bones[fk[2]]
    uarmi = obj.pose.bones[ik[0]]
    farmi = obj.pose.bones[ik[1]]
    handi = obj.pose.bones[ik[2]]

    main_parent = obj.pose.bones[ik[4]]

    if ik[3] != "" and main_parent['pole_vector']:
        pole  = obj.pose.bones[ik[3]]
    else:
        pole = None


    if pole:
        # Stretch
        # handi['stretch_length'] = uarm['stretch_length']

        # Hand position
        match_pose_translation(handi, hand)
        match_pose_rotation(handi, hand)
        match_pose_scale(handi, hand)
        view_layer.update()

        # Pole target position
        match_pole_target(view_layer, uarmi, farmi, pole, uarm.matrix, (uarmi.length + farmi.length))

    else:
        # Hand position
        match_pose_translation(handi, hand)
        match_pose_rotation(handi, hand)
        match_pose_scale(handi, hand)
        view_layer.update()

        # Upper Arm position
        match_pose_translation(uarmi, uarm)
        #match_pose_rotation(uarmi, uarm)
        set_pose_rotation(uarmi, Matrix())
        match_pose_scale(uarmi, uarm)
        view_layer.update()

        # Rotation Correction
        correct_rotation(view_layer, uarmi, uarm.matrix)

    correct_scale(view_layer, uarmi, uarm.matrix)
''']

# noinspection SpellCheckingInspection
UTILITIES_FUNC_OLD_LEG_FKIK = ['''
######################
## IK Leg functions ##
######################

def fk2ik_leg(obj, fk, ik):
    """ Matches the fk bones in a leg rig to the ik bones.
        obj: armature object
        fk:  list of fk bone names
        ik:  list of ik bone names
    """
    view_layer = bpy.context.view_layer
    thigh  = obj.pose.bones[fk[0]]
    shin   = obj.pose.bones[fk[1]]
    foot   = obj.pose.bones[fk[2]]
    mfoot  = obj.pose.bones[fk[3]]
    thighi = obj.pose.bones[ik[0]]
    shini  = obj.pose.bones[ik[1]]
    footi  = obj.pose.bones[ik[2]]
    mfooti = obj.pose.bones[ik[3]]

    if 'auto_stretch' in footi.keys():
        # This is kept for compatibility with legacy rigify Human
        # Stretch
        if footi['auto_stretch'] == 0.0:
            thigh['stretch_length'] = footi['stretch_length']
        else:
            diff = (thighi.vector.length + shini.vector.length) / (thigh.vector.length + shin.vector.length)
            thigh['stretch_length'] *= diff

        # Thigh position
        match_pose_rotation(thigh, thighi)
        match_pose_scale(thigh, thighi)
        view_layer.update()

        # Shin position
        match_pose_rotation(shin, shini)
        match_pose_scale(shin, shini)
        view_layer.update()

        # Foot position
        footmat = get_pose_matrix_in_other_space(mfooti.matrix, foot)
        footmat = convert_pose_matrix_via_rest_delta(footmat, mfoot, foot)
        set_pose_rotation(foot, footmat)
        set_pose_scale(foot, footmat)
        view_layer.update()

    else:
        # Thigh position
        match_pose_translation(thigh, thighi)
        match_pose_rotation(thigh, thighi)
        match_pose_scale(thigh, thighi)
        view_layer.update()

        # Shin position
        match_pose_rotation(shin, shini)
        match_pose_scale(shin, shini)
        view_layer.update()

        # Foot position
        footmat = get_pose_matrix_in_other_space(mfooti.matrix, foot)
        footmat = convert_pose_matrix_via_rest_delta(footmat, mfoot, foot)
        set_pose_rotation(foot, footmat)
        set_pose_scale(foot, footmat)
        view_layer.update()


def ik2fk_leg(obj, fk, ik):
    """ Matches the ik bones in a leg rig to the fk bones.
        obj: armature object
        fk:  list of fk bone names
        ik:  list of ik bone names
    """
    view_layer = bpy.context.view_layer
    thigh    = obj.pose.bones[fk[0]]
    shin     = obj.pose.bones[fk[1]]
    mfoot    = obj.pose.bones[fk[2]]
    if fk[3] != "":
        foot      = obj.pose.bones[fk[3]]
    else:
        foot = None
    thighi   = obj.pose.bones[ik[0]]
    shini    = obj.pose.bones[ik[1]]
    footi    = obj.pose.bones[ik[2]]
    footroll = obj.pose.bones[ik[3]]

    main_parent = obj.pose.bones[ik[6]]

    if ik[4] != "" and main_parent['pole_vector']:
        pole     = obj.pose.bones[ik[4]]
    else:
        pole = None
    mfooti   = obj.pose.bones[ik[5]]

    if (not pole) and (foot):

        # Clear footroll
        set_pose_rotation(footroll, Matrix())
        view_layer.update()

        # Foot position
        footmat = get_pose_matrix_in_other_space(foot.matrix, footi)
        footmat = convert_pose_matrix_via_rest_delta(footmat, mfooti, footi)
        set_pose_translation(footi, footmat)
        set_pose_rotation(footi, footmat)
        set_pose_scale(footi, footmat)
        view_layer.update()

        # Thigh position
        match_pose_translation(thighi, thigh)
        #match_pose_rotation(thighi, thigh)
        set_pose_rotation(thighi, Matrix())
        match_pose_scale(thighi, thigh)
        view_layer.update()

        # Rotation Correction
        correct_rotation(view_layer, thighi, thigh.matrix)

    else:
        # Stretch
        if 'stretch_length' in footi.keys() and 'stretch_length' in thigh.keys():
            # Kept for compat with legacy rigify Human
            footi['stretch_length'] = thigh['stretch_length']

        # Clear footroll
        set_pose_rotation(footroll, Matrix())
        view_layer.update()

        # Foot position
        footmat = get_pose_matrix_in_other_space(mfoot.matrix, footi)
        footmat = convert_pose_matrix_via_rest_delta(footmat, mfooti, footi)
        set_pose_translation(footi, footmat)
        set_pose_rotation(footi, footmat)
        set_pose_scale(footi, footmat)
        view_layer.update()

        # Pole target position
        match_pole_target(view_layer, thighi, shini, pole, thigh.matrix, (thighi.length + shini.length))

    correct_scale(view_layer, thighi, thigh.matrix)
''']

# noinspection SpellCheckingInspection
UTILITIES_FUNC_OLD_POLE = ['''
################################
## IK Rotation-Pole functions ##
################################

def rotPoleToggle(rig, limb_type, controls, ik_ctrl, fk_ctrl, parent, pole):

    rig_id = rig.data['rig_id']
    leg_fk2ik = eval('bpy.ops.pose.rigify_leg_fk2ik_' + rig_id)
    arm_fk2ik = eval('bpy.ops.pose.rigify_arm_fk2ik_' + rig_id)
    leg_ik2fk = eval('bpy.ops.pose.rigify_leg_ik2fk_' + rig_id)
    arm_ik2fk = eval('bpy.ops.pose.rigify_arm_ik2fk_' + rig_id)

    controls = parse_bone_names(controls)
    ik_ctrl = parse_bone_names(ik_ctrl)
    fk_ctrl = parse_bone_names(fk_ctrl)
    parent = parse_bone_names(parent)
    pole = parse_bone_names(pole)

    pbones = bpy.context.selected_pose_bones
    bpy.ops.pose.select_all(action='DESELECT')

    for b in pbones:

        new_pole_vector_value = not rig.pose.bones[parent]['pole_vector']

        if b.name in controls or b.name in ik_ctrl:
            if limb_type == 'arm':
                func1 = arm_fk2ik
                func2 = arm_ik2fk
                rig.pose.bones[controls[0]].bone.select = not new_pole_vector_value
                rig.pose.bones[controls[4]].bone.select = not new_pole_vector_value
                rig.pose.bones[parent].bone.select = not new_pole_vector_value
                rig.pose.bones[pole].bone.select = new_pole_vector_value

                kwargs1 = {'uarm_fk': controls[1], 'farm_fk': controls[2], 'hand_fk': controls[3],
                          'uarm_ik': controls[0], 'farm_ik': ik_ctrl[1],
                          'hand_ik': controls[4]}
                kwargs2 = {'uarm_fk': controls[1], 'farm_fk': controls[2], 'hand_fk': controls[3],
                          'uarm_ik': controls[0], 'farm_ik': ik_ctrl[1], 'hand_ik': controls[4],
                          'pole': pole, 'main_parent': parent}
            else:
                func1 = leg_fk2ik
                func2 = leg_ik2fk
                rig.pose.bones[controls[0]].bone.select = not new_pole_vector_value
                rig.pose.bones[controls[6]].bone.select = not new_pole_vector_value
                rig.pose.bones[controls[5]].bone.select = not new_pole_vector_value
                rig.pose.bones[parent].bone.select = not new_pole_vector_value
                rig.pose.bones[pole].bone.select = new_pole_vector_value

                kwargs1 = {'thigh_fk': controls[1], 'shin_fk': controls[2], 'foot_fk': controls[3],
                          'mfoot_fk': controls[7], 'thigh_ik': controls[0], 'shin_ik': ik_ctrl[1],
                          'foot_ik': ik_ctrl[2], 'mfoot_ik': ik_ctrl[2]}
                kwargs2 = {'thigh_fk': controls[1], 'shin_fk': controls[2], 'foot_fk': controls[3],
                          'mfoot_fk': controls[7], 'thigh_ik': controls[0], 'shin_ik': ik_ctrl[1],
                          'foot_ik': controls[6], 'pole': pole, 'footroll': controls[5],
                          'mfoot_ik': ik_ctrl[2], 'main_parent': parent}

            func1(**kwargs1)
            rig.pose.bones[parent]['pole_vector'] = new_pole_vector_value
            func2(**kwargs2)

            bpy.ops.pose.select_all(action='DESELECT')
''']

# noinspection SpellCheckingInspection
REGISTER_OP_OLD_ARM_FKIK = ['Rigify_Arm_FK2IK', 'Rigify_Arm_IK2FK']

# noinspection SpellCheckingInspection
UTILITIES_OP_OLD_ARM_FKIK = ['''
##################################
## IK/FK Arm snapping operators ##
##################################

class Rigify_Arm_FK2IK(bpy.types.Operator):
    """ Snaps an FK arm to an IK arm.
    """
    bl_idname = "pose.rigify_arm_fk2ik_" + rig_id
    bl_label = "Rigify Snap FK arm to IK"
    bl_options = {'UNDO', 'INTERNAL'}

    uarm_fk: StringProperty(name="Upper Arm FK Name")
    farm_fk: StringProperty(name="Forerm FK Name")
    hand_fk: StringProperty(name="Hand FK Name")

    uarm_ik: StringProperty(name="Upper Arm IK Name")
    farm_ik: StringProperty(name="Forearm IK Name")
    hand_ik: StringProperty(name="Hand IK Name")

    @classmethod
    def poll(cls, context):
        return (context.active_object != None and context.mode == 'POSE')

    def execute(self, context):
        fk2ik_arm(context.active_object, fk=[self.uarm_fk, self.farm_fk, self.hand_fk],
                  ik=[self.uarm_ik, self.farm_ik, self.hand_ik])
        return {'FINISHED'}


class Rigify_Arm_IK2FK(bpy.types.Operator):
    """ Snaps an IK arm to an FK arm.
    """
    bl_idname = "pose.rigify_arm_ik2fk_" + rig_id
    bl_label = "Rigify Snap IK arm to FK"
    bl_options = {'UNDO', 'INTERNAL'}

    uarm_fk: StringProperty(name="Upper Arm FK Name")
    farm_fk: StringProperty(name="Forerm FK Name")
    hand_fk: StringProperty(name="Hand FK Name")

    uarm_ik: StringProperty(name="Upper Arm IK Name")
    farm_ik: StringProperty(name="Forearm IK Name")
    hand_ik: StringProperty(name="Hand IK Name")
    pole   : StringProperty(name="Pole IK Name")

    main_parent: StringProperty(name="Main Parent", default="")

    @classmethod
    def poll(cls, context):
        return (context.active_object != None and context.mode == 'POSE')

    def execute(self, context):
        ik2fk_arm(context.active_object, fk=[self.uarm_fk, self.farm_fk, self.hand_fk],
                  ik=[self.uarm_ik, self.farm_ik, self.hand_ik, self.pole, self.main_parent])
        return {'FINISHED'}
''']

# noinspection SpellCheckingInspection
REGISTER_OP_OLD_LEG_FKIK = ['Rigify_Leg_FK2IK', 'Rigify_Leg_IK2FK']

# noinspection SpellCheckingInspection
UTILITIES_OP_OLD_LEG_FKIK = ['''
##################################
## IK/FK Leg snapping operators ##
##################################

class Rigify_Leg_FK2IK(bpy.types.Operator):
    """ Snaps an FK leg to an IK leg.
    """
    bl_idname = "pose.rigify_leg_fk2ik_" + rig_id
    bl_label = "Rigify Snap FK leg to IK"
    bl_options = {'UNDO', 'INTERNAL'}

    thigh_fk: StringProperty(name="Thigh FK Name")
    shin_fk:  StringProperty(name="Shin FK Name")
    foot_fk:  StringProperty(name="Foot FK Name")
    mfoot_fk: StringProperty(name="MFoot FK Name")

    thigh_ik: StringProperty(name="Thigh IK Name")
    shin_ik:  StringProperty(name="Shin IK Name")
    foot_ik:  StringProperty(name="Foot IK Name")
    mfoot_ik: StringProperty(name="MFoot IK Name")

    @classmethod
    def poll(cls, context):
        return (context.active_object != None and context.mode == 'POSE')

    def execute(self, context):
        fk2ik_leg(context.active_object,
                  fk=[self.thigh_fk, self.shin_fk, self.foot_fk, self.mfoot_fk],
                  ik=[self.thigh_ik, self.shin_ik, self.foot_ik, self.mfoot_ik])
        return {'FINISHED'}


class Rigify_Leg_IK2FK(bpy.types.Operator):
    """ Snaps an IK leg to an FK leg.
    """
    bl_idname = "pose.rigify_leg_ik2fk_" + rig_id
    bl_label = "Rigify Snap IK leg to FK"
    bl_options = {'UNDO', 'INTERNAL'}

    thigh_fk: StringProperty(name="Thigh FK Name")
    shin_fk:  StringProperty(name="Shin FK Name")
    mfoot_fk: StringProperty(name="MFoot FK Name")
    foot_fk:  StringProperty(name="Foot FK Name", default="")
    thigh_ik: StringProperty(name="Thigh IK Name")
    shin_ik:  StringProperty(name="Shin IK Name")
    foot_ik:  StringProperty(name="Foot IK Name")
    footroll: StringProperty(name="Foot Roll Name")
    pole:     StringProperty(name="Pole IK Name")
    mfoot_ik: StringProperty(name="MFoot IK Name")

    main_parent: StringProperty(name="Main Parent", default="")

    @classmethod
    def poll(cls, context):
        return (context.active_object != None and context.mode == 'POSE')

    def execute(self, context):
        ik2fk_leg(context.active_object,
                  fk=[self.thigh_fk, self.shin_fk, self.mfoot_fk, self.foot_fk],
                  ik=[self.thigh_ik, self.shin_ik, self.foot_ik, self.footroll, self.pole,
                      self.mfoot_ik, self.main_parent])
        return {'FINISHED'}
''']

REGISTER_OP_OLD_POLE = ['Rigify_Rot2PoleSwitch']

UTILITIES_OP_OLD_POLE = ['''
###########################
## IK Rotation Pole Snap ##
###########################

class Rigify_Rot2PoleSwitch(bpy.types.Operator):
    bl_idname = "pose.rigify_rot2pole_" + rig_id
    bl_label = "Rotation - Pole toggle"
    bl_description = "Toggles IK chain between rotation and pole target"

    bone_name: StringProperty(default='')
    limb_type: StringProperty(name="Limb Type")
    controls: StringProperty(name="Controls string")
    ik_ctrl: StringProperty(name="IK Controls string")
    fk_ctrl: StringProperty(name="FK Controls string")
    parent: StringProperty(name="Parent name")
    pole: StringProperty(name="Pole name")

    def execute(self, context):
        rig = context.object

        if self.bone_name:
            bpy.ops.pose.select_all(action='DESELECT')
            rig.pose.bones[self.bone_name].bone.select = True

        rotPoleToggle(rig, self.limb_type, self.controls, self.ik_ctrl, self.fk_ctrl,
                      self.parent, self.pole)
        return {'FINISHED'}
''']

REGISTER_RIG_OLD_ARM = REGISTER_OP_OLD_ARM_FKIK + REGISTER_OP_OLD_POLE

UTILITIES_RIG_OLD_ARM = [
    *UTILITIES_FUNC_COMMON_IK_FK,
    *UTILITIES_FUNC_OLD_ARM_FKIK,
    *UTILITIES_FUNC_OLD_POLE,
    *UTILITIES_OP_OLD_ARM_FKIK,
    *UTILITIES_OP_OLD_POLE,
]

REGISTER_RIG_OLD_LEG = REGISTER_OP_OLD_LEG_FKIK + REGISTER_OP_OLD_POLE

UTILITIES_RIG_OLD_LEG = [
    *UTILITIES_FUNC_COMMON_IK_FK,
    *UTILITIES_FUNC_OLD_LEG_FKIK,
    *UTILITIES_FUNC_OLD_POLE,
    *UTILITIES_OP_OLD_LEG_FKIK,
    *UTILITIES_OP_OLD_POLE,
]

############################
# Default set of utilities #
############################

UI_REGISTER = [
    'RigUI',
    'RigLayers',
]

UI_UTILITIES = [
]

UI_SLIDERS = '''
###################
## Rig UI Panels ##
###################

class RigUI(bpy.types.Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_label = "Rig Main Properties"
    bl_idname = "VIEW3D_PT_rig_ui_" + rig_id
    bl_category = 'Item'

    @classmethod
    def poll(self, context):
        if context.mode != 'POSE':
            return False
        try:
            return (context.active_object.data.get("rig_id") == rig_id)
        except (AttributeError, KeyError, TypeError):
            return False

    def draw(self, context):
        layout = self.layout
        pose_bones = context.active_object.pose.bones
        try:
            selected_bones = set(bone.name for bone in context.selected_pose_bones)
            selected_bones.add(context.active_pose_bone.name)
        except (AttributeError, TypeError):
            return

        def is_selected(names):
            # Returns whether any of the named bones are selected.
            if isinstance(names, list) or isinstance(names, set):
                return not selected_bones.isdisjoint(names)
            elif names in selected_bones:
                return True
            return False

        num_rig_separators = [-1]

        def emit_rig_separator():
            if num_rig_separators[0] >= 0:
                layout.separator()
            num_rig_separators[0] += 1
'''

UI_REGISTER_BAKE_SETTINGS = ['RigBakeSettings']

UI_BAKE_SETTINGS = '''
class RigBakeSettings(bpy.types.Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_label = "Rig Bake Settings"
    bl_idname = "VIEW3D_PT_rig_bake_settings_" + rig_id
    bl_category = 'Item'

    @classmethod
    def poll(self, context):
        return RigUI.poll(context) and find_action(context.active_object) is not None

    def draw(self, context):
        RigifyBakeKeyframesMixin.draw_common_bake_ui(context, self.layout)
'''


UI_LAYERS_PANEL = '''
class RigLayers(bpy.types.Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_label = "Rig Layers"
    bl_idname = "VIEW3D_PT_rig_layers_" + rig_id
    bl_category = 'Item'

    @classmethod
    def poll(self, context):
        try:
            return (context.active_object.data.get("rig_id") == rig_id)
        except (AttributeError, KeyError, TypeError):
            return False

    def draw(self, context):
        layout = self.layout
        row_table = collections.defaultdict(list)
        for coll in flatten_children(context.active_object.data.collections):
            row_id = coll.get('rigify_ui_row', 0)
            if row_id > 0:
                row_table[row_id].append(coll)
        col = layout.column()
        for row_id in range(min(row_table.keys()), 1 + max(row_table.keys())):
            row = col.row()
            row_buttons = row_table[row_id]
            if row_buttons:
                for coll in row_buttons:
                    title = coll.get('rigify_ui_title') or coll.name
                    row2 = row.row()
                    row2.active = coll.is_visible_ancestors
                    row2.prop(coll, 'is_visible', toggle=True, text=title)
            else:
                row.separator()
'''


class PanelExpression(object):
    """A runtime expression involving bone properties"""

    _rigify_expr: str

    def __init__(self, expr: str):
        self._rigify_expr = expr

    def __repr__(self):
        return self._rigify_expr

    def __add__(self, other):
        return PanelExpression(f"({self._rigify_expr} + {repr(other)})")

    def __sub__(self, other):
        return PanelExpression(f"({self._rigify_expr} - {repr(other)})")

    def __mul__(self, other):
        return PanelExpression(f"({self._rigify_expr} * {repr(other)})")

    def __matmul__(self, other):
        return PanelExpression(f"({self._rigify_expr} @ {repr(other)})")

    def __truediv__(self, other):
        return PanelExpression(f"({self._rigify_expr} / {repr(other)})")

    def __floordiv__(self, other):
        return PanelExpression(f"({self._rigify_expr} // {repr(other)})")

    def __mod__(self, other):
        return PanelExpression(f"({self._rigify_expr} % {repr(other)})")

    def __lshift__(self, other):
        return PanelExpression(f"({self._rigify_expr} << {repr(other)})")

    def __rshift__(self, other):
        return PanelExpression(f"({self._rigify_expr} >> {repr(other)})")

    def __and__(self, other):
        return PanelExpression(f"({self._rigify_expr} & {repr(other)})")

    def __xor__(self, other):
        return PanelExpression(f"({self._rigify_expr} ^ {repr(other)})")

    def __or__(self, other):
        return PanelExpression(f"({self._rigify_expr} | {repr(other)})")

    def __radd__(self, other):
        return PanelExpression(f"({repr(other)} + {self._rigify_expr})")

    def __rsub__(self, other):
        return PanelExpression(f"({repr(other)} - {self._rigify_expr})")

    def __rmul__(self, other):
        return PanelExpression(f"({repr(other)} * {self._rigify_expr})")

    def __rmatmul__(self, other):
        return PanelExpression(f"({repr(other)} @ {self._rigify_expr})")

    def __rtruediv__(self, other):
        return PanelExpression(f"({repr(other)} / {self._rigify_expr})")

    def __rfloordiv__(self, other):
        return PanelExpression(f"({repr(other)} // {self._rigify_expr})")

    def __rmod__(self, other):
        return PanelExpression(f"({repr(other)} % {self._rigify_expr})")

    def __rlshift__(self, other):
        return PanelExpression(f"({repr(other)} << {self._rigify_expr})")

    def __rrshift__(self, other):
        return PanelExpression(f"({repr(other)} >> {self._rigify_expr})")

    def __rand__(self, other):
        return PanelExpression(f"({repr(other)} & {self._rigify_expr})")

    def __rxor__(self, other):
        return PanelExpression(f"({repr(other)} ^ {self._rigify_expr})")

    def __ror__(self, other):
        return PanelExpression(f"({repr(other)} | {self._rigify_expr})")

    def __neg__(self):
        return PanelExpression(f"-{self._rigify_expr}")

    def __pos__(self):
        return PanelExpression(f"+{self._rigify_expr}")

    def __abs__(self):
        return PanelExpression(f"abs({self._rigify_expr})")

    def __invert__(self):
        return PanelExpression(f"~{self._rigify_expr}")

    def __round__(self, digits=None):
        return PanelExpression(f"round({self._rigify_expr}, {digits})")

    def __trunc__(self):
        return PanelExpression(f"trunc({self._rigify_expr})")

    def __floor__(self):
        return PanelExpression(f"floor({self._rigify_expr})")

    def __ceil__(self):
        return PanelExpression(f"ceil({self._rigify_expr})")

    def __lt__(self, other):
        return PanelExpression(f"({self._rigify_expr} < {repr(other)})")

    def __le__(self, other):
        return PanelExpression(f"({self._rigify_expr} <= {repr(other)})")

    def __eq__(self, other):
        return PanelExpression(f"({self._rigify_expr} == {repr(other)})")

    def __ne__(self, other):
        return PanelExpression(f"({self._rigify_expr} != {repr(other)})")

    def __gt__(self, other):
        return PanelExpression(f"({self._rigify_expr} > {repr(other)})")

    def __ge__(self, other):
        return PanelExpression(f"({self._rigify_expr} >= {repr(other)})")

    def __bool__(self):
        raise NotImplementedError("This object wraps an expression, not a value; casting to boolean is meaningless")


class PanelReferenceExpression(PanelExpression):
    """
    A runtime expression referencing an object.
    @DynamicAttrs
    """

    def __getitem__(self, item):
        return PanelReferenceExpression(f"{self._rigify_expr}[{repr(item)}]")

    def __getattr__(self, item):
        return PanelReferenceExpression(f"{self._rigify_expr}.{item}")

    def get(self, item, default=None):
        return PanelReferenceExpression(f"{self._rigify_expr}.get({repr(item)}, {repr(default)})")


def quote_parameters(positional: list[Any], named: dict[str, Any]):
    """Quote the given positional and named parameters as a code string."""
    positional_list = [repr(v) for v in positional]
    named_list = ["%s=%r" % (k, v) for k, v in named.items()]
    return ', '.join(positional_list + named_list)


def indent_lines(lines: list[str], indent=4):
    if indent > 0:
        prefix = ' ' * indent
        return [prefix + line for line in lines]
    else:
        return lines


class PanelLayout(object):
    """Utility class that builds code for creating a layout."""

    parent: Optional['PanelLayout']
    script: 'ScriptGenerator'

    header: list[str]
    items: list[Union[str, 'PanelLayout']]

    def __init__(self, parent: Union['PanelLayout', 'ScriptGenerator'], index=0):
        if isinstance(parent, PanelLayout):
            self.parent = parent
            self.script = parent.script
        else:
            self.parent = None
            self.script = parent

        self.header = []
        self.items = []
        self.indent = 0
        self.index = index
        self.layout = self._get_layout_var(index)
        self.is_empty = True

    @staticmethod
    def _get_layout_var(index):
        return 'layout' if index == 0 else 'group' + str(index)

    def clear_empty(self):
        self.is_empty = False

        if self.parent:
            self.parent.clear_empty()

    def get_lines(self) -> list[str]:
        lines = []

        for item in self.items:
            if isinstance(item, PanelLayout):
                lines += item.get_lines()
            else:
                lines.append(item)

        if len(lines) > 0:
            return self.wrap_lines(lines)
        else:
            return []

    def wrap_lines(self, lines):
        return self.header + indent_lines(lines, self.indent)

    def add_line(self, line: str):
        assert isinstance(line, str)

        self.items.append(line)

        if self.is_empty:
            self.clear_empty()

    def use_bake_settings(self):
        """This panel contains operators that need the common Bake settings."""
        self.parent.use_bake_settings()

    def custom_prop(self, bone_name: str, prop_name: str, **params):
        """Add a custom property input field to the panel."""
        param_str = quote_parameters([rna_idprop_quote_path(prop_name)], params)
        self.add_line(
            "%s.prop(pose_bones[%r], %s)" % (self.layout, bone_name, param_str)
        )

    def operator(self, operator_name: str, *,
                 properties: Optional[dict[str, Any]] = None,
                 **params):
        """Add an operator call button to the panel."""
        name = operator_name.format_map(self.script.format_args)
        param_str = quote_parameters([name], params)
        call_str = "%s.operator(%s)" % (self.layout, param_str)
        if properties:
            self.add_line("props = " + call_str)
            for k, v in properties.items():
                self.add_line("props.%s = %r" % (k, v))
        else:
            self.add_line(call_str)

    def add_nested_layout(self, method_name: str, params: dict[str, Any]) -> 'PanelLayout':
        param_str = quote_parameters([], params)
        sub_panel = PanelLayout(self, self.index + 1)
        sub_panel.header.append(f'{sub_panel.layout} = {self.layout}.{method_name}({param_str})')
        self.items.append(sub_panel)
        return sub_panel

    def row(self, **params):
        """Add a nested row layout to the panel."""
        return self.add_nested_layout('row', params)

    def column(self, **params):
        """Add a nested column layout to the panel."""
        return self.add_nested_layout('column', params)

    def split(self, **params):
        """Add a split layout to the panel."""
        return self.add_nested_layout('split', params)

    @staticmethod
    def expr_bone(bone_name):
        """Returns an expression referencing the specified pose bone."""
        return PanelReferenceExpression(f"pose_bones[%r]" % bone_name)

    @staticmethod
    def expr_and(*expressions):
        """Returns a boolean and expression of its parameters."""
        return PanelExpression("(" + " and ".join(repr(e) for e in expressions) + ")")

    @staticmethod
    def expr_or(*expressions):
        """Returns a boolean or expression of its parameters."""
        return PanelExpression("(" + " or ".join(repr(e) for e in expressions) + ")")

    @staticmethod
    def expr_if_else(condition, true_expr, false_expr):
        """Returns a conditional expression."""
        return PanelExpression(f"({repr(true_expr)} if {repr(condition)} else {repr(false_expr)})")

    @staticmethod
    def expr_call(func: str, *expressions):
        """Returns an expression calling the specified function with given parameters."""
        return PanelExpression(func + "(" + ", ".join(repr(e) for e in expressions) + ")")

    def set_layout_property(self, prop_name: str, prop_value: Any):
        assert self.index > 0  # Don't change properties on the root layout
        self.add_line("%s.%s = %r" % (self.layout, prop_name, prop_value))

    @property
    def active(self):
        raise NotImplementedError("This is a write only property")

    @active.setter
    def active(self, value):
        self.set_layout_property('active', value)

    @property
    def enabled(self):
        raise NotImplementedError("This is a write only property")

    @enabled.setter
    def enabled(self, value):
        self.set_layout_property('enabled', value)


class BoneSetPanelLayout(PanelLayout):
    """Panel restricted to a certain set of bones."""

    parent: 'RigPanelLayout'

    def __init__(self, rig_panel: 'RigPanelLayout', bones: frozenset[str]):
        assert isinstance(bones, frozenset)
        super().__init__(rig_panel)
        self.bones = bones
        self.show_bake_settings = False

    def clear_empty(self):
        self.parent.bones |= self.bones

        super().clear_empty()

    def wrap_lines(self, lines):
        if self.bones != self.parent.bones:
            header = ["if is_selected(%r):" % (set(self.bones))]
            return header + indent_lines(lines)
        else:
            return lines

    def use_bake_settings(self):
        self.show_bake_settings = True
        if not self.script.use_bake_settings:
            self.script.use_bake_settings = True
            self.script.add_utilities(SCRIPT_UTILITIES_BAKE)
            self.script.register_classes(SCRIPT_REGISTER_BAKE)


class RigPanelLayout(PanelLayout):
    """Panel owned by a certain rig."""

    def __init__(self, script: 'ScriptGenerator', _rig):
        super().__init__(script)
        self.bones = set()
        self.sub_panels = OrderedDict()

    def wrap_lines(self, lines):
        header = ["if is_selected(%r):" % (set(self.bones))]
        prefix = ["emit_rig_separator()"]
        return header + indent_lines(prefix + lines)

    def panel_with_selected_check(self, control_names):
        selected_set = frozenset(control_names)

        if selected_set in self.sub_panels:
            return self.sub_panels[selected_set]
        else:
            panel = BoneSetPanelLayout(self, selected_set)
            self.sub_panels[selected_set] = panel
            self.items.append(panel)
            return panel


class ScriptGenerator(base_generate.GeneratorPlugin):
    """Generator plugin that builds the python script attached to the rig."""

    priority = -100

    format_args: dict[str, str]

    def __init__(self, generator):
        super().__init__(generator)

        self.ui_scripts = []
        self.ui_imports = UI_IMPORTS.copy()
        self.ui_utilities = UI_UTILITIES.copy()
        self.ui_register = UI_REGISTER.copy()
        self.ui_register_drivers = []
        self.ui_register_props = []

        self.ui_rig_panels = OrderedDict()

        self.use_bake_settings = False

    # Structured panel code generation
    def panel_with_selected_check(self, rig, control_names):
        """Add a panel section with restricted selection."""
        rig_key = id(rig)

        if rig_key in self.ui_rig_panels:
            panel = self.ui_rig_panels[rig_key]
        else:
            panel = RigPanelLayout(self, rig)
            self.ui_rig_panels[rig_key] = panel

        return panel.panel_with_selected_check(control_names)

    # Raw output
    def add_panel_code(self, str_list: list[str]):
        """Add raw code to the panel."""
        self.ui_scripts += str_list

    def add_imports(self, str_list: list[str]):
        self.ui_imports += str_list

    def add_utilities(self, str_list: list[str]):
        self.ui_utilities += str_list

    def register_classes(self, str_list: list[str]):
        self.ui_register += str_list

    def register_driver_functions(self, str_list: list[str]):
        self.ui_register_drivers += str_list

    def register_property(self, name: str, definition):
        self.ui_register_props.append((name, definition))

    def initialize(self):
        self.format_args = {
            'rig_id': self.generator.rig_id,
        }

    def finalize(self):
        metarig = self.generator.metarig
        rig_id = self.generator.rig_id

        # Generate the UI script
        script = metarig.data.rigify_rig_ui

        if script:
            script.clear()
        else:
            script_name = self.generator.obj.name + "_ui.py"
            script = bpy.data.texts.new(script_name)
            metarig.data.rigify_rig_ui = script

        for s in OrderedDict.fromkeys(self.ui_imports):
            script.write(s + "\n")

        script.write(UI_BASE_UTILITIES % rig_id)

        for s in OrderedDict.fromkeys(self.ui_utilities):
            script.write(s + "\n")

        script.write(UI_SLIDERS)

        for s in self.ui_scripts:
            script.write("\n        " + s.replace("\n", "\n        ") + "\n")

        if len(self.ui_scripts) > 0:
            script.write("\n        num_rig_separators[0] = 0\n")

        for panel in self.ui_rig_panels.values():
            lines = panel.get_lines()
            if len(lines) > 1:
                script.write("\n        ".join([''] + lines) + "\n")

        if self.use_bake_settings:
            self.ui_register = UI_REGISTER_BAKE_SETTINGS + self.ui_register
            script.write(UI_BAKE_SETTINGS)

        script.write(UI_LAYERS_PANEL)

        script.write("\ndef register():\n")

        ui_register = OrderedDict.fromkeys(self.ui_register)
        for s in ui_register:
            script.write("    bpy.utils.register_class("+s+")\n")

        ui_register_drivers = OrderedDict.fromkeys(self.ui_register_drivers)
        for s in ui_register_drivers:
            script.write("    bpy.app.driver_namespace['"+s+"'] = "+s+"\n")

        ui_register_props = OrderedDict.fromkeys(self.ui_register_props)
        for classname, text in ui_register_props:
            script.write(f"    bpy.types.{classname} = {text}\n ")

        script.write("\ndef unregister():\n")

        for s in ui_register_props:
            script.write("    del bpy.types.%s\n" % s[0])

        for s in ui_register:
            script.write("    bpy.utils.unregister_class("+s+")\n")

        for s in ui_register_drivers:
            script.write("    del bpy.app.driver_namespace['"+s+"']\n")

        script.write("\nregister()\n")
        script.use_module = True

        # Run UI script
        exec(script.as_string(), {})

        # Attach the script to the rig
        self.obj['rig_ui'] = script
