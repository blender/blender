"""
Only works for 'basic type' properties (bool, int and float)!
Multi-dimensional arrays (like array of vectors) will be flattened into seq.
"""
import bpy

mesh = bpy.context.object.data
collection = mesh.vertices

# Allocate a flat list for the `co` property (X, Y, Z per vertex).
coords = [0.0] * len(collection) * 3

# Fast access.
collection.foreach_get("co", coords)

# Python equivalent (per-element iteration is much slower).
for i, vert in enumerate(collection):
    coords[i * 3], coords[i * 3 + 1], coords[i * 3 + 2] = vert.co
