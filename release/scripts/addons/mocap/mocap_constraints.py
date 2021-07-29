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
from mathutils import Vector
from bpy_extras import anim_utils
from .  import retarget


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
    #Utility function - returns related bone from ik object
    if cons_obj.name[-3:] == "Org":
        return cons_obj.name[:-3]
    else:
        return cons_obj.name

### And and Remove Constraints (called from operators)


def addNewConstraint(m_constraint, cons_obj):
     #Decide the correct Blender constraint according to the Mocap constraint type
    if m_constraint.type == "point" or m_constraint.type == "freeze":
        c_type = "LIMIT_LOCATION"
    if m_constraint.type == "distance":
        c_type = "LIMIT_DISTANCE"
    if m_constraint.type == "floor":
        c_type = "LIMIT_LOCATION"
        #create and store the new constraint within m_constraint
    real_constraint = cons_obj.constraints.new(c_type)
    real_constraint.name = "Auto fixes " + str(len(cons_obj.constraints))
    m_constraint.real_constraint_bone = consObjToBone(cons_obj)
    m_constraint.real_constraint = real_constraint.name
    #set the rest of the constraint properties
    setConstraint(m_constraint, bpy.context)


def removeConstraint(m_constraint, cons_obj):
    #remove the influence fcurve and Blender constraint
    oldConstraint = cons_obj.constraints[m_constraint.real_constraint]
    removeFcurves(cons_obj, bpy.context.active_object, oldConstraint, m_constraint)
    cons_obj.constraints.remove(oldConstraint)

### Update functions. There are 3: UpdateType/Bone
### update framing (deals with changes in the desired frame range)
### And setConstraint which deals with the rest


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
    #remove the old keyframes
    removeFcurves(cons_obj, obj, real_constraint, m_constraint)
    #set the new ones according to the m_constraint properties
    s, e = m_constraint.s_frame, m_constraint.e_frame
    s_in, s_out = m_constraint.smooth_in, m_constraint.smooth_out
    real_constraint.influence = 1
    real_constraint.keyframe_insert(data_path="influence", frame=s)
    real_constraint.keyframe_insert(data_path="influence", frame=e)
    real_constraint.influence = 0
    real_constraint.keyframe_insert(data_path="influence", frame=s - s_in)
    real_constraint.keyframe_insert(data_path="influence", frame=e + s_out)


def removeFcurves(cons_obj, obj, real_constraint, m_constraint):
    #Determine if the constrained object is a bone or an empty
    if isinstance(cons_obj, bpy.types.PoseBone):
        fcurves = obj.animation_data.action.fcurves
    else:
        fcurves = cons_obj.animation_data.action.fcurves
    #Find the RNA data path of the constraint's influence
    RNA_paths = []
    RNA_paths.append(real_constraint.path_from_id("influence"))
    if m_constraint.type == "floor" or m_constraint.type == "point":
        RNA_paths += [real_constraint.path_from_id("max_x"), real_constraint.path_from_id("min_x")]
        RNA_paths += [real_constraint.path_from_id("max_y"), real_constraint.path_from_id("min_y")]
        RNA_paths += [real_constraint.path_from_id("max_z"), real_constraint.path_from_id("min_z")]
    #Retrieve the correct fcurve via the RNA data path and remove it
    fcurves_del = [fcurve for fcurve in fcurves if fcurve.data_path in RNA_paths]
    #clear the fcurve and set the frames.
    if fcurves_del:
        for fcurve in fcurves_del:
            fcurves.remove(fcurve)
    #remove armature fcurves (if user keyframed m_constraint properties)
    if obj.data.animation_data and m_constraint.type == "point":
        if obj.data.animation_data.action:
            path = m_constraint.path_from_id("targetPoint")
            m_fcurves = [fcurve for fcurve in obj.data.animation_data.action.fcurves if fcurve.data_path == path]
            for curve in m_fcurves:
                obj.data.animation_data.action.fcurves.remove(curve)

#Utility function for copying property fcurves over


def copyFCurve(newCurve, oldCurve):
    for point in oldCurve.keyframe_points:
        newCurve.keyframe_points.insert(frame=point.co.x, value=point.co.y)

#Creates new fcurves for the constraint properties (for floor and point)


def createConstraintFCurves(cons_obj, obj, real_constraint):
    if isinstance(cons_obj, bpy.types.PoseBone):
        c_fcurves = obj.animation_data.action.fcurves
    else:
        c_fcurves = cons_obj.animation_data.action.fcurves
    c_x_path = [real_constraint.path_from_id("max_x"), real_constraint.path_from_id("min_x")]
    c_y_path = [real_constraint.path_from_id("max_y"), real_constraint.path_from_id("min_y")]
    c_z_path = [real_constraint.path_from_id("max_z"), real_constraint.path_from_id("min_z")]
    c_constraints_path = c_x_path + c_y_path + c_z_path
    existing_curves = [fcurve for fcurve in c_fcurves if fcurve.data_path in c_constraints_path]
    if existing_curves:
        for curve in existing_curves:
            c_fcurves.remove(curve)
    xCurves, yCurves, zCurves = [], [], []
    for path in c_constraints_path:
        newCurve = c_fcurves.new(path)
        if path in c_x_path:
            xCurves.append(newCurve)
        elif path in c_y_path:
            yCurves.append(newCurve)
        else:
            zCurves.append(newCurve)
    return xCurves, yCurves, zCurves


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
    NLATracks = obj.data.mocapNLATracks[obj.data.active_mocap]
    obj.animation_data.action = bpy.data.actions[NLATracks.auto_fix_track]

    #frame changing section
    setConstraintFraming(m_constraint, context)
    s, e = m_constraint.s_frame, m_constraint.e_frame
    s_in, s_out = m_constraint.smooth_in, m_constraint.smooth_out
    s -= s_in
    e += s_out
    #Set the blender constraint parameters
    if m_constraint.type == "point":
        constraint_settings = False  # are fix settings keyframed?
        if not m_constraint.targetSpace == "constrained_boneB":
            real_constraint.owner_space = m_constraint.targetSpace
        else:
            real_constraint.owner_space = "LOCAL"
        if obj.data.animation_data:
            if obj.data.animation_data.action:
                path = m_constraint.path_from_id("targetPoint")
                m_fcurves = [fcurve for fcurve in obj.data.animation_data.action.fcurves if fcurve.data_path == path]
                if m_fcurves:
                    constraint_settings = True
                    xCurves, yCurves, zCurves = createConstraintFCurves(cons_obj, obj, real_constraint)
                    for curve in xCurves:
                        copyFCurve(curve, m_fcurves[0])
                    for curve in yCurves:
                        copyFCurve(curve, m_fcurves[1])
                    for curve in zCurves:
                        copyFCurve(curve, m_fcurves[2])
        if m_constraint.targetSpace == "constrained_boneB" and m_constraint.constrained_boneB:
            c_frame = context.scene.frame_current
            bakedPos = {}
            src_bone = bones[m_constraint.constrained_boneB]
            if not constraint_settings:
                xCurves, yCurves, zCurves = createConstraintFCurves(cons_obj, obj, real_constraint)
            print("please wait a moment, calculating fix")
            for t in range(s, e):
                context.scene.frame_set(t)
                src_bone_pos = src_bone.matrix.to_translation()
                bakedPos[t] = src_bone_pos + m_constraint.targetPoint  # final position for constrained bone in object space
            context.scene.frame_set(c_frame)
            for frame in bakedPos.keys():
                pos = bakedPos[frame]
                for xCurve in xCurves:
                    xCurve.keyframe_points.insert(frame=frame, value=pos.x)
                for yCurve in yCurves:
                    yCurve.keyframe_points.insert(frame=frame, value=pos.y)
                for zCurve in zCurves:
                    zCurve.keyframe_points.insert(frame=frame, value=pos.z)

        if not constraint_settings:
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
        context.scene.frame_set(s)
        real_constraint.owner_space = m_constraint.targetSpace
        bpy.context.scene.frame_set(m_constraint.s_frame)
        if isinstance(cons_obj, bpy.types.PoseBone):
            vec = obj.matrix_world * (cons_obj.matrix.to_translation())
            #~ if obj.parent:
                #~ vec = obj.parent.matrix_world * vec
            x, y, z = vec
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
        real_constraint.target = obj
        real_constraint.subtarget = getConsObj(bones[m_constraint.constrained_boneB]).name
        real_constraint.limit_mode = "LIMITDIST_ONSURFACE"
        if m_constraint.targetDist < 0.01:
            m_constraint.targetDist = 0.01
        real_constraint.distance = m_constraint.targetDist

    if m_constraint.type == "floor" and m_constraint.targetMesh:
        real_constraint.mute = True
        real_constraint.owner_space = "WORLD"
        #calculate the positions thoughout the range
        s, e = m_constraint.s_frame, m_constraint.e_frame
        s_in, s_out = m_constraint.smooth_in, m_constraint.smooth_out
        s -= s_in
        e += s_out
        bakedPos = {}
        floor = bpy.data.objects[m_constraint.targetMesh]
        c_frame = context.scene.frame_current
        print("please wait a moment, calculating fix")
        for t in range(s, e):
            context.scene.frame_set(t)
            axis = obj.matrix_world.to_3x3() * Vector((0, 0, 1))
            offset = obj.matrix_world.to_3x3() * Vector((0, 0, m_constraint.targetDist))
            ray_origin = (cons_obj.matrix * obj.matrix_world).to_translation() - offset  # world position of constrained bone
            ray_target = ray_origin + axis
            #convert ray points to floor's object space
            ray_origin = floor.matrix_world.inverted() * ray_origin
            ray_target = floor.matrix_world.inverted() * ray_target
            ray_direction = ray_target - ray_origin
            ok, hit, nor, ind = floor.ray_cast(ray_origin, ray_direction)
            if ok:
                bakedPos[t] = (floor.matrix_world * hit)
                bakedPos[t] += Vector((0, 0, m_constraint.targetDist))
            else:
                bakedPos[t] = (cons_obj.matrix * obj.matrix_world).to_translation()
        context.scene.frame_set(c_frame)
        #create keyframes for real constraint
        xCurves, yCurves, zCurves = createConstraintFCurves(cons_obj, obj, real_constraint)
        for frame in bakedPos.keys():
            pos = bakedPos[frame]
            for xCurve in xCurves:
                xCurve.keyframe_points.insert(frame=frame, value=pos.x)
            for yCurve in yCurves:
                yCurve.keyframe_points.insert(frame=frame, value=pos.y)
            for zCurve in zCurves:
                zCurve.keyframe_points.insert(frame=frame, value=pos.z)
        real_constraint.use_max_x = True
        real_constraint.use_max_y = True
        real_constraint.use_max_z = True
        real_constraint.use_min_x = True
        real_constraint.use_min_y = True
        real_constraint.use_min_z = True

    # active/baked check
    real_constraint.mute = (not m_constraint.active)


def locBake(s_frame, e_frame, bones):
    scene = bpy.context.scene
    bakeDict = {}
    for bone in bones:
        bakeDict[bone.name] = {}
    for t in range(s_frame, e_frame):
        scene.frame_set(t)
        for bone in bones:
            bakeDict[bone.name][t] = bone.matrix.copy()
    for t in range(s_frame, e_frame):
        for bone in bones:
            print(bone.bone.matrix_local.to_translation())
            bone.matrix = bakeDict[bone.name][t]
            bone.keyframe_insert("location", frame=t)


# Baking function which bakes all bones effected by the constraint
def bakeAllConstraints(obj, s_frame, e_frame, bones):
    for bone in bones:
        bone.bone.select = False
    selectedBones = []  # Marks bones that need a full bake
    simpleBake = []  # Marks bones that need only a location bake
    for end_bone in bones:
        if end_bone.name in [m_constraint.real_constraint_bone for m_constraint in obj.data.mocap_constraints]:
            #For all bones that have a constraint:
            ik = retarget.hasIKConstraint(end_bone)
            cons_obj = getConsObj(end_bone)
            if ik:
                #If it's an auto generated IK:
                if ik.chain_count == 0:
                    selectedBones += bones  # Chain len 0, bake everything
                else:
                    selectedBones += [end_bone] + end_bone.parent_recursive[:ik.chain_count - 1]  # Bake the chain
            else:
                #It's either an FK bone which we should just bake
                #OR a user created IK target bone
                simpleBake += [end_bone]
    for bone in selectedBones:
        bone.bone.select = True
    NLATracks = obj.data.mocapNLATracks[obj.data.active_mocap]
    obj.animation_data.action = bpy.data.actions[NLATracks.auto_fix_track]
    constraintTrack = obj.animation_data.nla_tracks[NLATracks.auto_fix_track]
    constraintStrip = constraintTrack.strips[0]
    constraintStrip.action_frame_start = s_frame
    constraintStrip.action_frame_end = e_frame
    constraintStrip.frame_start = s_frame
    constraintStrip.frame_end = e_frame
    if selectedBones:
        # Use bake function from NLA Bake Action operator
        anim_utils.bake_action(s_frame,
                               e_frame,
                               action=constraintStrip.action,
                               only_selected=True,
                               do_pose=True,
                               do_object=False,
                               )
    if simpleBake:
        #Do a "simple" bake, location only, world space only.
        locBake(s_frame, e_frame, simpleBake)


#Calls the baking function and decativates releveant constraints
def bakeConstraints(context):
    obj = context.active_object
    bones = obj.pose.bones
    s_frame, e_frame = context.scene.frame_start, context.scene.frame_end
    #Bake relevant bones
    bakeAllConstraints(obj, s_frame, e_frame, bones)
    for m_constraint in obj.data.mocap_constraints:
        end_bone = bones[m_constraint.real_constraint_bone]
        cons_obj = getConsObj(end_bone)
        # It's a control empty: turn the ik off
        if not isinstance(cons_obj, bpy.types.PoseBone):
            ik_con = retarget.hasIKConstraint(end_bone)
            if ik_con:
                ik_con.mute = True
        # Deactivate related Blender Constraint
        m_constraint.active = False


#Deletes the baked fcurves and reactivates relevant constraints
def unbakeConstraints(context):
    # to unbake constraints we delete the whole strip
    obj = context.active_object
    bones = obj.pose.bones
    scene = bpy.context.scene
    NLATracks = obj.data.mocapNLATracks[obj.data.active_mocap]
    obj.animation_data.action = bpy.data.actions[NLATracks.auto_fix_track]
    constraintTrack = obj.animation_data.nla_tracks[NLATracks.auto_fix_track]
    constraintStrip = constraintTrack.strips[0]
    action = constraintStrip.action
    # delete the fcurves on the strip
    for fcurve in action.fcurves:
        action.fcurves.remove(fcurve)
    # reactivate relevant constraints
    for m_constraint in obj.data.mocap_constraints:
        end_bone = bones[m_constraint.real_constraint_bone]
        cons_obj = getConsObj(end_bone)
        # It's a control empty: turn the ik back on
        if not isinstance(cons_obj, bpy.types.PoseBone):
            ik_con = retarget.hasIKConstraint(end_bone)
            if ik_con:
                ik_con.mute = False
        m_constraint.active = True


def updateConstraints(obj, context):
    fixes = obj.data.mocap_constraints
    for fix in fixes:
        fix.active = False
        fix.active = True
