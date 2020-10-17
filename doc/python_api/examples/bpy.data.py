import bpy


# print all objects
for obj in bpy.data.objects:
    print(obj.name)


# print all scene names in a list
print(bpy.data.scenes.keys())


# remove mesh Cube
if "Cube" in bpy.data.meshes:
    mesh = bpy.data.meshes["Cube"]
    print("removing mesh", mesh)
    bpy.data.meshes.remove(mesh)

#a helpful snippet for learning about armatures and f-curves and datapath 
def keyframe_bone_loc_x(object_name, bone_name):
    try:
        armature_ob = bpy.data.objects[object_name]
    except KeyError:
        print(f"Armature object {object_name} not found")
        return

    # animation_data_create() will just return animation_data if it already exists.
    anim_data = armature_ob.animation_data_create()
    if not anim_data.action:
        anim_data.action = bpy.data.actions.new(name="MyAction")

    try:
        pose_bone = armature_ob.pose.bones[bone_name]
    except KeyError:
        print(f"Bone {bone_name} not found in {object_name}")
        return

    data_path = pose_bone.path_from_id("location")

    # Get the FCurve, creating it if it doesn't exist yet.
    fcu_x = anim_data.action.fcurves.find(data_path, index=0)
    if not fcu_x:
        fcu_x = anim_data.action.fcurves.new(data_path, index=0)
    
    # Insert keyframes.
    fcu_x.keyframe_points.insert(1, 0)
    fcu_x.keyframe_points.insert(10, 3.27)
    fcu_x.keyframe_points.insert(15, 0.47)
    fcu_x.update()

#call the code we defined above
keyframe_bone_loc_x('Armature', 'Bone')




# write images into a file next to the blend
import os
with open(os.path.splitext(bpy.data.filepath)[0] + ".txt", 'w') as fs:
    for image in bpy.data.images:
        fs.write("%s %d x %d\n" % (image.filepath, image.size[0], image.size[1]))
