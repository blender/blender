import bpy

filepath = "//link_library.blend"

# Load a single scene we know the name of.
with bpy.data.libraries.load(filepath) as (data_src, data_dst):
    data_dst.scenes = ["Scene"]


# Load all meshes.
with bpy.data.libraries.load(filepath) as (data_src, data_dst):
    data_dst.meshes = data_src.meshes


# Link all objects starting with "A".
with bpy.data.libraries.load(filepath, link=True) as (data_src, data_dst):
    data_dst.objects = [name for name in data_src.objects if name.startswith("A")]


# Append everything.
with bpy.data.libraries.load(filepath) as (data_src, data_dst):
    for attr in dir(data_dst):
        setattr(data_dst, attr, getattr(data_src, attr))


# The loaded objects can be accessed from `data_dst` outside of the context
# since loading the data replaces the strings for the data-blocks or None
# if the data-block could not be loaded.
with bpy.data.libraries.load(filepath) as (data_src, data_dst):
    data_dst.meshes = data_src.meshes
# Now operate directly on the loaded data.
for mesh in data_dst.meshes:
    if mesh is not None:
        print(mesh.name)
