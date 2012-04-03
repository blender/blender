# This example assumes we have a mesh object selected

import bpy
import bmesh

# Get the active mesh
me = bpy.context.object.data


# Get a BMesh representation
bm = bmesh.new()   # create an empty BMesh
bm.from_mesh(me)   # fill it in from a Mesh


# Modify the BMesh, can do anything here...
for v in bm.verts:
    v.co.x += 1.0


# Finish up, write the bmesh back to the mesh
bm.to_mesh(me)
