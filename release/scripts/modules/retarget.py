import bpy
from mathutils import *
from math import radians, acos
performer_obj = bpy.data.objects["performer"]
enduser_obj = bpy.data.objects["enduser"]
end_arm = bpy.data.armatures["enduser_arm"] 
scene = bpy.context.scene

#TODO: Only selected bones get retargeted.
#      Selected Bones/chains get original pos empties, if ppl want IK instead of FK
#      Some "magic" numbers - frame start and end, eulers of all orders instead of just quats keyframed



# dictionary of mapping
# this is currently manuall input'ed, but will
# be created from a more comfortable UI in the future
bonemap = { "LeftFoot": ("DEF_Foot.L","DEF_Toes.L"),
            "LeftUpLeg": "DEF_Thigh.L",
            "Hips": "DEF_Hip",
            "LowerBack": "DEF_Spine",
            "Spine": "DEF_Torso",
            "Neck": "DEF_Neck",
            "Neck1": "DEF_Neck",
            "Head": "DEF_Head",
            "LeftShoulder": "DEF_Shoulder.L",
            "LeftArm": "DEF_Forearm.L",
            "LeftForeArm": "DEF_Arm.L",
            "LeftHand": "DEF_Hand.L",
            "RightShoulder": "DEF_Shoulder.R",
            "RightArm": "DEF_Forearm.R",
            "RightForeArm": "DEF_Arm.R",
            "RightHand": "DEF_Hand.R",
            "RightFoot": ("DEF_Foot.R","DEF_Toes.R"),
            "RightUpLeg": "DEF_Thigh.R",
            "RightLeg": "DEF_Shin.R",
            "LeftLeg": "DEF_Shin.L"}
            
# creation of a reverse map
# multiple keys get mapped to list values
bonemapr = {}
for key in bonemap.keys():
    if not bonemap[key] in bonemapr:
        if type(bonemap[key])==type((0,0)):
            for key_x in bonemap[key]:
                bonemapr[key_x] = [key]
        else:
            bonemapr[bonemap[key]] = [key]
    else:
        bonemapr[bonemap[key]].append(key)
        
# list of empties created to keep track of "original"
# position data
# in final product, these locations can be stored as custom props
# these help with constraining, etc.

constraints = []


#creation of intermediate armature
# the intermediate armature has the hiearchy of the end user,
# does not have rotation inheritence
# and bone roll is identical to the performer
# its purpose is to copy over the rotations
# easily while concentrating on the hierarchy changes
def createIntermediate():
    
    #creates and keyframes an empty with its location
    #the original position of the tail bone
    #useful for storing the important data in the original motion
    #i.e. using this empty to IK the chain to that pos.
    def locOfOriginal(inter_bone,perf_bone):
        if not perf_bone.name+"Org" in bpy.data.objects:
            bpy.ops.object.add()
            empty = bpy.context.active_object
            empty.name = perf_bone.name+"Org"
        empty = bpy.data.objects[perf_bone.name+"Org"]
        offset = perf_bone.vector
        scaling = perf_bone.length / inter_bone.length
        offset/=scaling
        empty.location = inter_bone.head + offset
        empty.keyframe_insert("location")
    
    #Simple 1to1 retarget of a bone
    def singleBoneRetarget(inter_bone,perf_bone):
            perf_world_rotation = perf_bone.matrix * performer_obj.matrix_world         
            inter_world_base_rotation = inter_bone.bone.matrix_local * inter_obj.matrix_world
            inter_world_base_inv = Matrix(inter_world_base_rotation)
            inter_world_base_inv.invert()
            return (inter_world_base_inv.to_3x3() * perf_world_rotation.to_3x3()).to_4x4()
        
    #uses 1to1 and interpolation/averaging to match many to 1 retarget    
    def manyPerfToSingleInterRetarget(inter_bone,performer_bones_s):
        retarget_matrices = [singleBoneRetarget(inter_bone,perf_bone) for perf_bone in performer_bones_s]
        lerp_matrix = Matrix()
        for i in range(len(retarget_matrices)-1):
            first_mat = retarget_matrices[i]
            next_mat = retarget_matrices[i+1]
            lerp_matrix = first_mat.lerp(next_mat,0.5)
        return lerp_matrix
    
    #determines the type of hierachy change needed and calls the 
    #right function        
    def retargetPerfToInter(inter_bone):
        if inter_bone.name in bonemapr.keys():
            perf_bone_name = bonemapr[inter_bone.name]
            #is it a 1 to many?
            if type(bonemap[perf_bone_name[0]])==type((0,0)):
                perf_bone = performer_bones[perf_bone_name[0]]
                if inter_bone.name == bonemap[perf_bone_name[0]][0]:
                    locOfOriginal(inter_bone,perf_bone)
            else:
                # then its either a many to 1 or 1 to 1
                
                if len(perf_bone_name) > 1:
                    performer_bones_s = [performer_bones[name] for name in perf_bone_name]
                    #we need to map several performance bone to a single
                    for perf_bone in performer_bones_s:
                        locOfOriginal(inter_bone,perf_bone)
                    inter_bone.matrix_basis = manyPerfToSingleInterRetarget(inter_bone,performer_bones_s)
                else:
                    perf_bone = performer_bones[perf_bone_name[0]]
                    locOfOriginal(inter_bone,perf_bone)
                    inter_bone.matrix_basis = singleBoneRetarget(inter_bone,perf_bone)
                    
        inter_bone.keyframe_insert("rotation_quaternion")
        for child in inter_bone.children:
            retargetPerfToInter(child)
            
    #creates the intermediate armature object        
    bpy.ops.object.select_name(name="enduser",extend=False)
    bpy.ops.object.duplicate(linked=False)
    bpy.context.active_object.name = "intermediate"
    inter_obj = bpy.context.active_object
    bpy.ops.object.mode_set(mode='EDIT')
    #resets roll 
    bpy.ops.armature.calculate_roll(type='Z')
    bpy.ops.object.mode_set(mode="OBJECT")
    inter_arm = bpy.data.armatures["enduser_arm.001"]
    inter_arm.name = "inter_arm"
    performer_bones = performer_obj.pose.bones
    inter_bones =  inter_obj.pose.bones
    
    #clears inheritance
    for inter_bone in inter_bones:
        inter_bone.bone.use_inherit_rotation = False
        
    for t in range(1,150):
        scene.frame_set(t)
        inter_bone = inter_bones["DEF_Hip"]
        retargetPerfToInter(inter_bone)
        
    return inter_obj,inter_arm

# this procedure copies the rotations over from the intermediate
# armature to the end user one.
# As the hierarchies are 1 to 1, this is a simple matter of 
# copying the rotation, while keeping in mind bone roll, parenting, etc.
# TODO: Control Bones: If a certain bone is constrained in a way
#       that its rotation is determined by another (a control bone)
#       We should determine the right pos of the control bone.
#       Scale: ? Should work but needs testing.          
def retargetEnduser():
    inter_bones =  inter_obj.pose.bones
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
        
    for t in range(1,150):
        scene.frame_set(t)
        end_bone = end_bones["DEF_Hip"]
        bakeTransform(end_bone)

inter_obj, inter_arm = createIntermediate()
retargetEnduser()