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
    bpy.data.meshes.unlink(mesh)


# write images into a file next to the blend
file = open(bpy.data.filepath.replace(".blend", ".txt"), 'w')

for image in bpy.data.images:
    file.write("%s %dx%d\n" % (image.filepath, image.size[0], image.size[1]))

file.close()


