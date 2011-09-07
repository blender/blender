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


# write images into a file next to the blend
import os
file = open(os.path.splitext(bpy.data.filepath)[0] + ".txt", 'w')

for image in bpy.data.images:
    file.write("%s %d x %d\n" % (image.filepath, image.size[0], image.size[1]))

file.close()
