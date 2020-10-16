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

#look up a armature pose bone and create a fcurve or access a existing Fcurve for location X for that pose bone
if 'Armature' in bpy.data.objects:
    armature  = bpy.data.objects['Armature']
    
    if not armature.animation_data:
        armature.animation_data_create()
        armature.animation_data.action = bpy.data.actions.new(name="MyAction")
        
    if 'BoneName' in bpy.data.objects['Armature'].pose.bones:
        poseBone =bpy.data.objects['Armature'].pose.bones['BoneName']
        
        path = poseBone.path_from_id("location")
         
        #if fcurve for location X does not exist create one                 
        fcu_x = armature.animation_data.action.fcurves.find(path, index=0)
        if not fcu_x:
            fcu_x =  armature.animation_data.action.fcurves.new(path, index=0)

# write images into a file next to the blend
import os
with open(os.path.splitext(bpy.data.filepath)[0] + ".txt", 'w') as fs:
    for image in bpy.data.images:
        fs.write("%s %d x %d\n" % (image.filepath, image.size[0], image.size[1]))
