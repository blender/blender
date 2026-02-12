"""
Only works for 'basic type' properties (bool, int and float)!
seq must be uni-dimensional, multi-dimensional arrays (like array of vectors) will be re-created from it.
"""
import bpy

mesh = bpy.context.object.data
collection = mesh.vertices

# Flatten all Z coordinates to zero (X, Y, Z per vertex).
coords = [0.0] * len(collection) * 3
collection.foreach_get("co", coords)
for i in range(2, len(coords), 3):
    coords[i] = 0.0

# Fast assignment.
collection.foreach_set("co", coords)

# Python equivalent (per-element iteration is much slower).
for i, vert in enumerate(collection):
    vert.co = (coords[i * 3], coords[i * 3 + 1], coords[i * 3 + 2])
