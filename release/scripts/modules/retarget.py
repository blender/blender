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
from math import radians, acos

#TODO: Only selected bones get retargeted.
#      Selected Bones/chains get original pos empties,
#      if ppl want IK instead of FK
#      Some "magic" numbers - frame start and end,
#	eulers of all orders instead of just quats keyframed

# dictionary of mapping
# this is currently manuall input'ed, but willW
# be created from a more comfortable UI in the future


def createDictionary(perf_arm):
    bonemap = {}
    for bone in perf_arm.bones:
        bonemap[bone.name] = bone.map
    #root is the root of the enduser
    root = "root"
    # creation of a reverse map
    # multiple keys get mapped to list values
    bonemapr = {}
    for key, value in bonemap.items():
        if not value in bonemapr:
            if isinstance(bonemap[key], tuple):
                for key_x in bonemap[key]:
                    bonemapr[key_x] = [key]
            else:
                bonemapr[bonemap[key]] = [key]
        else:
            bonemapr[bonemap[key]].append(key)
    return bonemap, bonemapr, root
# list of empties created to keep track of "original"
# position data
# in final product, these locations can be stored as custom props
# these help with constraining, etc.

#creation of intermediate armature
# the intermediate armature has the hiearchy of the end user,
# does not have rotation inheritence
# and bone roll is identical to the performer
# its purpose is to copy over the rotations
# easily while concentrating on the hierarchy changes


def createIntermediate(performer_obj, enduser_obj, bonemap, bonemapr, root, s_frame, e_frame, scene):
    #creates and keyframes an empty with its location
    #the original position of the tail bone
    #useful for storing the important data in the original motion
    #i.e. using this empty to IK the chain to that pos / DEBUG
    def locOfOriginal(inter_bone, perf_bone):
        if not inter_bone.name + "Org" in bpy.data.objects:
            bpy.ops.object.add()
            empty = bpy.context.active_object
            empty.name = inter_bone.name + "Org"
            empty.empty_draw_size = 0.1
            #empty.parent = enduser_obj
        empty = bpy.data.objects[inter_bone.name + "Org"]
        offset = perf_bone.vector
        if inter_bone.length == 0 or perf_bone.length == 0:
            scaling = 1
        else:
            scaling = perf_bone.length / inter_bone.length
        offset /= scaling
        empty.location = inter_bone.head + offset
        empty.keyframe_insert("location")

    #Simple 1to1 retarget of a bone
    def singleBoneRetarget(inter_bone, perf_bone):
            perf_world_rotation = perf_bone.matrix * performer_obj.matrix_world
            inter_world_base_rotation = inter_bone.bone.matrix_local * inter_obj.matrix_world
            inter_world_base_inv = Matrix(inter_world_base_rotation)
            inter_world_base_inv.invert()
            return (inter_world_base_inv.to_3x3() * perf_world_rotation.to_3x3()).to_4x4()

    #uses 1to1 and interpolation/averaging to match many to 1 retarget
    def manyPerfToSingleInterRetarget(inter_bone, performer_bones_s):
        retarget_matrices = [singleBoneRetarget(inter_bone, perf_bone) for perf_bone in performer_bones_s]
        lerp_matrix = Matrix()
        for i in range(len(retarget_matrices) - 1):
            first_mat = retarget_matrices[i]
            next_mat = retarget_matrices[i + 1]
            lerp_matrix = first_mat.lerp(next_mat, 0.5)
        return lerp_matrix

    #determines the type of hierachy change needed and calls the
    #right function
    def retargetPerfToInter(inter_bone):
        if inter_bone.name in bonemapr:
            perf_bone_name = bonemapr[inter_bone.name]
            #is it a 1 to many?
            if isinstance(bonemap[perf_bone_name[0]], tuple):
                perf_bone = performer_bones[perf_bone_name[0]]
                if inter_bone.name == bonemap[perf_bone_name[0]][0]:
                    locOfOriginal(inter_bone, perf_bone)
            else:
                # then its either a many to 1 or 1 to 1

                if len(perf_bone_name) > 1:
                    performer_bones_s = [performer_bones[name] for name in perf_bone_name]
                    #we need to map several performance bone to a single
                    for perf_bone in performer_bones_s:
                        locOfOriginal(inter_bone, perf_bone)
                    inter_bone.matrix_basis = manyPerfToSingleInterRetarget(inter_bone, performer_bones_s)
                else:
                    perf_bone = performer_bones[perf_bone_name[0]]
                    locOfOriginal(inter_bone, perf_bone)
                    inter_bone.matrix_basis = singleBoneRetarget(inter_bone, perf_bone)

        inter_bone.keyframe_insert("rotation_quaternion")
        for child in inter_bone.children:
            retargetPerfToInter(child)

    #creates the intermediate armature object
    inter_obj = enduser_obj.copy()
    inter_obj.data = inter_obj.data.copy()  # duplicate data
    bpy.context.scene.objects.link(inter_obj)
    inter_obj.name = "intermediate"
    bpy.context.scene.objects.active = inter_obj
    bpy.ops.object.mode_set(mode='EDIT')
    #resets roll
    bpy.ops.armature.calculate_roll(type='Z')
    bpy.ops.object.mode_set(mode="OBJECT")
    inter_obj.data.name = "inter_arm"
    inter_arm = inter_obj.data
    performer_bones = performer_obj.pose.bones
    inter_bones = inter_obj.pose.bones
    #clears inheritance
    for inter_bone in inter_bones:
        inter_bone.bone.use_inherit_rotation = False

    for t in range(s_frame, e_frame):
        scene.frame_set(t)
        inter_bone = inter_bones[root]
        retargetPerfToInter(inter_bone)

    return inter_obj, inter_arm

# this procedure copies the rotations over from the intermediate
# armature to the end user one.
# As the hierarchies are 1 to 1, this is a simple matter of
# copying the rotation, while keeping in mind bone roll, parenting, etc.
# TODO: Control Bones: If a certain bone is constrained in a way
#       that its rotation is determined by another (a control bone)
#       We should determine the right pos of the control bone.
#       Scale: ? Should work but needs testing.


def retargetEnduser(inter_obj, enduser_obj, root, s_frame, e_frame, scene):
    inter_bones = inter_obj.pose.bones
    end_bones = enduser_obj.pose.bones

    def bakeTransform(end_bone):
        src_bone = inter_bones[end_bone.name]
        trg_bone = end_bone
        bake_matrix = src_bone.matrix
        rest_matrix = trg_bone.bone.matrix_local

        if trg_bone.parent and trg_bone.bone.use_inherit_rotation:
            parent_mat = src_bone.parent.matrix
            parent_rest = trg_bone.parent.bone.matrix_local
            parent_rest_inv = parent_rest.copy()
            parent_rest_inv.invert()
            parent_mat_inv = parent_mat.copy()
            parent_mat_inv.invert()
            bake_matrix = parent_mat_inv * bake_matrix
            rest_matrix = parent_rest_inv * rest_matrix

        rest_matrix_inv = rest_matrix.copy()
        rest_matrix_inv.invert()
        bake_matrix = rest_matrix_inv * bake_matrix
        trg_bone.matrix_basis = bake_matrix
        end_bone.keyframe_insert("rotation_quaternion")

        for bone in end_bone.children:
            bakeTransform(bone)

    for t in range(s_frame, e_frame):
        scene.frame_set(t)
        end_bone = end_bones[root]
        bakeTransform(end_bone)

#recieves the performer feet bones as a variable
# by "feet" I mean those bones that have plants
# (they don't move, despite root moving) somewhere in the animation.


def copyTranslation(performer_obj, enduser_obj, perfFeet, bonemap, bonemapr, root, s_frame, e_frame, scene):
    endFeet = [bonemap[perfBone] for perfBone in perfFeet]
    perfRoot = bonemapr[root][0]
    locDictKeys = perfFeet + endFeet + [perfRoot]
    perf_bones = performer_obj.pose.bones
    end_bones = enduser_obj.pose.bones

    def tailLoc(bone):
        return bone.center + (bone.vector / 2)

    #Step 1 - we create a dict that contains these keys:
    #(Performer) Hips, Feet
    #(End user) Feet
    # where the values are their world position on each (1,120) frame

    locDict = {}
    for key in locDictKeys:
        locDict[key] = []

    for t in range(scene.frame_start, scene.frame_end):
        scene.frame_set(t)
        for bone in perfFeet:
            locDict[bone].append(tailLoc(perf_bones[bone]))
        locDict[perfRoot].append(tailLoc(perf_bones[perfRoot]))
        for bone in endFeet:
            locDict[bone].append(tailLoc(end_bones[bone]))

    # now we take our locDict and analyze it.
    # we need to derive all chains

    locDeriv = {}
    for key in locDictKeys:
        locDeriv[key] = []

    for key in locDict.keys():
        graph = locDict[key]
        for t in range(len(graph) - 1):
            x = graph[t]
            xh = graph[t + 1]
            locDeriv[key].append(xh - x)

    # now find the plant frames, where perfFeet don't move much

    linearAvg = []

    for key in perfFeet:
        for i in range(len(locDeriv[key]) - 1):
            v = locDeriv[key][i]
            hipV = locDeriv[perfRoot][i]
            endV = locDeriv[bonemap[key]][i]
            if (v.length < 0.1):
                #this is a plant frame.
                #lets see what the original hip delta is, and the corresponding
                #end bone's delta
                if endV.length != 0:
                    linearAvg.append(hipV.length / endV.length)

    bpy.ops.object.add()
    stride_bone = bpy.context.active_object
    stride_bone.name = "stride_bone"

    if linearAvg:
        avg = sum(linearAvg) / len(linearAvg)
        for t in range(s_frame, e_frame):
            scene.frame_set(t)
            newTranslation = (tailLoc(perf_bones[perfRoot]) / avg)
            stride_bone.location = newTranslation * enduser_obj.matrix_world
            stride_bone.keyframe_insert("location")
    return stride_bone


def IKRetarget(bonemap, bonemapr, performer_obj, enduser_obj, s_frame, e_frame, scene):
    end_bones = enduser_obj.pose.bones
    for pose_bone in end_bones:
        if "IK" in [constraint.type for constraint in pose_bone.constraints]:
            # set constraint target to corresponding empty if targetless,
            # if not, keyframe current target to corresponding empty
            perf_bone = bonemapr[pose_bone.name]
            if isinstance(perf_bone, list):
                perf_bone = bonemapr[pose_bone.name][-1]
            end_empty = bpy.data.objects[pose_bone.name + "Org"]
            ik_constraint = [constraint for constraint in pose_bone.constraints if constraint.type == "IK"][0]
            if not ik_constraint.target:
                ik_constraint.target = end_empty
            else:
                #Bone target
                target_is_bone = False
                if ik_constraint.subtarget:
                    target = ik_constraint.target.pose.bones[ik_constraint.subtarget]
                    target.bone.use_local_location = False
                    target_is_bone = True
                else:
                    target = ik_constraint.target
                for t in range(s_frame, e_frame):
                    scene.frame_set(t)
                    if target_is_bone:
                        final_loc = end_empty.location - target.bone.matrix_local.to_translation()
                    else:
                        final_loc = end_empty.location
                    target.location = final_loc
                    target.keyframe_insert("location")
            ik_constraint.mute = False


def turnOffIK(enduser_obj):
    end_bones = enduser_obj.pose.bones
    for pose_bone in end_bones:
        if pose_bone.is_in_ik_chain:
            pass
            # TODO:
            # set stiffness according to place on chain
            # and values from analysis that is stored in the bone
            #pose_bone.ik_stiffness_x = 0.5
            #pose_bone.ik_stiffness_y = 0.5
            #pose_bone.ik_stiffness_z = 0.5
        if "IK" in [constraint.type for constraint in pose_bone.constraints]:
            ik_constraint = [constraint for constraint in pose_bone.constraints if constraint.type == "IK"][0]
            ik_constraint.mute = True


def cleanAndStoreObjMat(performer_obj, enduser_obj):
    perf_obj_mat = performer_obj.matrix_world.copy()
    enduser_obj_mat = enduser_obj.matrix_world.copy()
    zero_mat = Matrix()  # Matrix(((0,0,0,0),(0,0,0,0),(0,0,0,0),(0,0,0,0)))
    performer_obj.matrix_world = zero_mat
    enduser_obj.matrix_world = zero_mat
    return perf_obj_mat, enduser_obj_mat


def restoreObjMat(performer_obj, enduser_obj, perf_obj_mat, enduser_obj_mat, stride_bone):
    pose_bones = enduser_obj.pose.bones
    for pose_bone in pose_bones:
        if pose_bone.name + "Org" in bpy.data.objects:
            empty = bpy.data.objects[pose_bone.name + "Org"]
            empty.parent = enduser_obj
    performer_obj.matrix_world = perf_obj_mat
    enduser_obj.matrix_world = enduser_obj_mat


def totalRetarget():
    print("retargeting...")
    enduser_obj = bpy.context.active_object
    performer_obj = [obj for obj in bpy.context.selected_objects if obj != enduser_obj]
    if enduser_obj is None or len(performer_obj) != 1:
        print("Need active and selected armatures")
    else:
        performer_obj = performer_obj[0]
    perf_arm = performer_obj.data
    end_arm = enduser_obj.data
    scene = bpy.context.scene
    s_frame = scene.frame_start
    e_frame = scene.frame_end
    bonemap, bonemapr, root = createDictionary(perf_arm)
    perf_obj_mat, enduser_obj_mat = cleanAndStoreObjMat(performer_obj, enduser_obj)
    turnOffIK(enduser_obj)
    inter_obj, inter_arm = createIntermediate(performer_obj, enduser_obj, bonemap, bonemapr, root, s_frame, e_frame, scene)
    retargetEnduser(inter_obj, enduser_obj, root, s_frame, e_frame, scene)
    stride_bone = copyTranslation(performer_obj, enduser_obj, ["RightFoot", "LeftFoot"], bonemap, bonemapr, root, s_frame, e_frame, scene)
    IKRetarget(bonemap, bonemapr, performer_obj, enduser_obj, s_frame, e_frame, scene)
    restoreObjMat(performer_obj, enduser_obj, perf_obj_mat, enduser_obj_mat, stride_bone)
    bpy.ops.object.mode_set(mode='OBJECT')
    bpy.ops.object.select_name(name=inter_obj.name, extend=False)
    bpy.ops.object.delete()

if __name__ == "__main__":
    totalRetarget()
