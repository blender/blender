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

import bpy
from mathutils import *
from bl_operators import nla
from retarget import hasIKConstraint

### Utility Functions


def getConsObj(bone):
    #utility function - returns related IK target if bone has IK
    ik = [constraint for constraint in bone.constraints if constraint.type == "IK"]
    if ik:
        ik = ik[0]
        cons_obj = ik.target
        if ik.subtarget:
            cons_obj = ik.target.pose.bones[ik.subtarget]
    else:
        cons_obj = bone
    return cons_obj


def consObjToBone(cons_obj):
    if cons_obj.name[-3:] == "Org":
        return cons_obj.name[:-3]
    else:
        return cons_obj.name

### And and Remove Constraints (called from operators)


def addNewConstraint(m_constraint, cons_obj):
    if m_constraint.type == "point" or m_constraint.type == "freeze":
        c_type = "LIMIT_LOCATION"
    if m_constraint.type == "distance":
        c_type = "LIMIT_DISTANCE"
    if m_constraint.type == "floor":
        c_type = "FLOOR"
    real_constraint = cons_obj.constraints.new(c_type)
    real_constraint.name = "Mocap constraint " + str(len(cons_obj.constraints))
    m_constraint.real_constraint_bone = consObjToBone(cons_obj)
    m_constraint.real_constraint = real_constraint.name
    setConstraint(m_constraint, bpy.context)


def removeConstraint(m_constraint, cons_obj):
    oldConstraint = cons_obj.constraints[m_constraint.real_constraint]
    removeInfluenceFcurve(cons_obj, bpy.context.active_object, oldConstraint)
    cons_obj.constraints.remove(oldConstraint)

### Update functions. There are 2: UpdateType/UpdateBone
### and update for the others.


def updateConstraintBoneType(m_constraint, context):
    #If the constraint exists, we need to remove it
    #from the old bone
    obj = context.active_object
    bones = obj.pose.bones
    if m_constraint.real_constraint:
        bone = bones[m_constraint.real_constraint_bone]
        cons_obj = getConsObj(bone)
        removeConstraint(m_constraint, cons_obj)
    #Regardless, after that we create a new constraint
    if m_constraint.constrained_bone:
        bone = bones[m_constraint.constrained_bone]
        cons_obj = getConsObj(bone)
        addNewConstraint(m_constraint, cons_obj)


def setConstraintFraming(m_constraint, context):
    obj = context.active_object
    bones = obj.pose.bones
    bone = bones[m_constraint.constrained_bone]
    cons_obj = getConsObj(bone)
    real_constraint = cons_obj.constraints[m_constraint.real_constraint]
    removeInfluenceFcurve(cons_obj, obj, real_constraint)
    s, e = m_constraint.s_frame, m_constraint.e_frame
    s_in, s_out = m_constraint.smooth_in, m_constraint.smooth_out
    real_constraint.influence = 1
    real_constraint.keyframe_insert(data_path="influence", frame=s)
    real_constraint.keyframe_insert(data_path="influence", frame=e)
    real_constraint.influence = 0
    real_constraint.keyframe_insert(data_path="influence", frame=s - s_in)
    real_constraint.keyframe_insert(data_path="influence", frame=e + s_out)


def removeInfluenceFcurve(cons_obj, obj, real_constraint):
    if isinstance(cons_obj, bpy.types.PoseBone):
        fcurves = obj.animation_data.action.fcurves
    else:
        fcurves = cons_obj.animation_data.action.fcurves

    influence_RNA = real_constraint.path_from_id("influence")
    fcurve = [fcurve for fcurve in fcurves if fcurve.data_path == influence_RNA]
    #clear the fcurve and set the frames.
    if fcurve:
        fcurves.remove(fcurve[0])


# Function that copies all settings from m_constraint to the real Blender constraints
# Is only called when blender constraint already exists


def setConstraint(m_constraint, context):
    if not m_constraint.constrained_bone:
        return
    obj = context.active_object
    bones = obj.pose.bones
    bone = bones[m_constraint.constrained_bone]
    cons_obj = getConsObj(bone)
    real_constraint = cons_obj.constraints[m_constraint.real_constraint]

    #frame changing section
    #setConstraintFraming(m_constraint, cons_obj, obj, real_constraint)

    #Set the blender constraint parameters
    if m_constraint.type == "point":
        real_constraint.owner_space = m_constraint.targetSpace
        x, y, z = m_constraint.targetPoint
        real_constraint.max_x = x
        real_constraint.max_y = y
        real_constraint.max_z = z
        real_constraint.min_x = x
        real_constraint.min_y = y
        real_constraint.min_z = z
        real_constraint.use_max_x = True
        real_constraint.use_max_y = True
        real_constraint.use_max_z = True
        real_constraint.use_min_x = True
        real_constraint.use_min_y = True
        real_constraint.use_min_z = True

    if m_constraint.type == "freeze":
        real_constraint.owner_space = m_constraint.targetSpace
        bpy.context.scene.frame_set(m_constraint.s_frame)
        if isinstance(cons_obj, bpy.types.PoseBone):
            x, y, z = cons_obj.center + (cons_obj.vector / 2)
        else:
            x, y, z = cons_obj.matrix_world.to_translation()

        real_constraint.max_x = x
        real_constraint.max_y = y
        real_constraint.max_z = z
        real_constraint.min_x = x
        real_constraint.min_y = y
        real_constraint.min_z = z
        real_constraint.use_max_x = True
        real_constraint.use_max_y = True
        real_constraint.use_max_z = True
        real_constraint.use_min_x = True
        real_constraint.use_min_y = True
        real_constraint.use_min_z = True

    if m_constraint.type == "distance" and m_constraint.constrained_boneB:
        real_constraint.owner_space = "WORLD"
        real_constraint.target = getConsObj(bones[m_constraint.constrained_boneB])
        real_constraint.limit_mode = "LIMITDIST_ONSURFACE"
        real_constraint.distance = m_constraint.targetDist

    # active/baked check
    real_constraint.mute = (not m_constraint.active) and (m_constraint.baked)


def updateBake(self, context):
    if self.baked:
        print("baking...")
        bakeConstraint(self, context)
    else:
        print("unbaking...")
        unbakeConstraint(self, context)


def bakeTransformFK(anim_data, s_frame, e_frame, end_bone, bones, cons_obj):
    mute_ik = False
    for bone in bones:
        bone.bone.select = False
    ik = hasIKConstraint(end_bone)
    if not isinstance(cons_obj, bpy.types.PoseBone) and ik:
            if ik.chain_count == 0:
                selectedBones = bones
            else:
                selectedBones = [end_bone] + end_bone.parent_recursive[:ik.chain_count - 1]
            mute_ik = True
    else:
        selectedBones = [end_bone]
    print(selectedBones)
    for bone in selectedBones:
        bone.bone.select = True
    anim_data.action = nla.bake(s_frame,
        e_frame, action=anim_data.action)
    return mute_ik


def bakeConstraint(m_constraint, context):
    obj = context.active_object
    bones = obj.pose.bones
    end_bone = bones[m_constraint.constrained_bone]
    cons_obj = getConsObj(end_bone)
    s, e = m_constraint.s_frame, m_constraint.e_frame
    s_in, s_out = m_constraint.smooth_in, m_constraint.smooth_out
    s_frame = s - s_in
    e_frame = e + s_out
    mute_ik = bakeTransformFK(obj.animation_data, s_frame, e_frame, end_bone, bones, cons_obj)
    if mute_ik:
        ik_con = hasIKConstraint(end_bone)
        ik_con.mute = True
    real_constraint = cons_obj.constraints[m_constraint.real_constraint]
    real_constraint.mute = True
    constraintTrack = obj.animation_data.nla_tracks["Mocap constraints"]
    constraintStrip = constraintTrack.strips[0]
    constraintStrip.action_frame_start = s_frame
    constraintStrip.action_frame_end = e_frame
    constraintStrip.frame_start = s_frame
    constraintStrip.frame_end = e_frame


def unbakeConstraint(m_constraint, context):
    # to unbake a constraint we need to delete the whole strip
    # and rebake all the other constraints
    obj = context.active_object
    bones = obj.pose.bones
    end_bone = bones[m_constraint.constrained_bone]
    cons_obj = getConsObj(end_bone)
    scene = bpy.context.scene
    constraintTrack = obj.animation_data.nla_tracks["Mocap constraints"]
    constraintStrip = constraintTrack.strips[0]
    action = constraintStrip.action
    for fcurve in action.fcurves:
        action.fcurves.remove(fcurve)
    for other_m_constraint in obj.data.mocap_constraints:
        if m_constraint != other_m_constraint:
            bakeConstraint(other_m_constraint)
    # It's a control empty: turn the ik back on
    if not isinstance(cons_obj, bpy.types.PoseBone):
        ik_con = hasIKConstraint(end_bone)
        if ik_con:
            ik_con.mute = False
    real_constraint = cons_obj.constraints[m_constraint.real_constraint]
    real_constraint.mute = False
