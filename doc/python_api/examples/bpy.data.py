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

# get a pose bone from an armature and get it's data path to create F-curve if it 
# does not exist or access it if it does exist
if 'Armature' in bpy.data.armatures:
    if 'BoneName' in bpy.data.armatures['Armature'].pose.bones:
        poseBone = bpy.data.armatures['ArmatureName'].pose.bones['BoneName']
        path = poseBone.path_from_id("location") 

        #if fcurve for location X does not exist create one                 
        fcu_x = b_obj.animation_data.action.fcurves.find(dpath, index=0)
        if not fcu_x:
            fcu_x = b_obj.animation_data.action.fcurves.new(dpath, index=0)


# write images into a file next to the blend
import os
with open(os.path.splitext(bpy.data.filepath)[0] + ".txt", 'w') as fs:
    for image in bpy.data.images:
        fs.write("%s %d x %d\n" % (image.filepath, image.size[0], image.size[1]))
