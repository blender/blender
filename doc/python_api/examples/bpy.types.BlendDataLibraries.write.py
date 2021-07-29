import bpy

filepath = "//new_library.blend"

# write selected objects and their data to a blend file
data_blocks = set(bpy.context.selected_objects)
bpy.data.libraries.write(filepath, data_blocks)


# write all meshes starting with a capital letter and
# set them with fake-user enabled so they aren't lost on re-saving
data_blocks = {mesh for mesh in bpy.data.meshes if mesh.name[:1].isupper()}
bpy.data.libraries.write(filepath, data_blocks, fake_user=True)


# write all materials, textures and node groups to a library
data_blocks = {*bpy.data.materials, *bpy.data.textures, *bpy.data.node_groups}
bpy.data.libraries.write(filepath, data_blocks)
