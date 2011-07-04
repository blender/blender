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

### Utility Functions

def hasIKConstraint(pose_bone):
    #utility function / predicate, returns True if given bone has IK constraint
    return ("IK" in [constraint.type for constraint in pose_bone.constraints])


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
    setConstraint(m_constraint)


def removeConstraint(m_constraint, cons_obj):
    oldConstraint = cons_obj.constraints[m_constraint.real_constraint]
    cons_obj.constraints.remove(oldConstraint)

### Update functions. There are 2: UpdateType/UpdateBone
### and update for the others.


def updateConstraint(self, context):
    setConstraint(self)

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


# Function that copies all settings from m_constraint to the real Blender constraints
# Is only called when blender constraint already exists
def setConstraint(m_constraint):
    if not m_constraint.constrained_bone:
        return
    obj = bpy.context.active_object
    bones = obj.pose.bones
    bone = bones[m_constraint.constrained_bone]
    cons_obj = getConsObj(bone)
    real_constraint = cons_obj.constraints[m_constraint.real_constraint]

    #frame changing section
    if isinstance(cons_obj, bpy.types.PoseBone):
        fcurves = obj.animation_data.action.fcurves
    else:
        fcurves = cons_obj.animation_data.action.fcurves
        
    influence_RNA = real_constraint.path_from_id("influence")
    fcurve = [fcurve for fcurve in fcurves if fcurve.data_path == influence_RNA]
    #clear the fcurve and set the frames.
    if fcurve:
        fcurve = fcurve[0]
        for i in range(len(fcurve.keyframe_points) - 1, 0, -1):
            fcurve.keyframe_points.remove(fcurve.keyframe_points[i])
    s, e = m_constraint.s_frame, m_constraint.e_frame
    s_in, s_out = m_constraint.smooth_in, m_constraint.smooth_out
    real_constraint.influence = 1
    real_constraint.keyframe_insert(data_path="influence", frame=s)
    real_constraint.keyframe_insert(data_path="influence", frame=e)
    real_constraint.influence = 0
    real_constraint.keyframe_insert(data_path="influence", frame=s - s_in)
    real_constraint.keyframe_insert(data_path="influence", frame=e + s_out)   
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
        bpy.context.scene.frame_set(s)
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
    
    # active check
    real_constraint.mute = not m_constraint.active