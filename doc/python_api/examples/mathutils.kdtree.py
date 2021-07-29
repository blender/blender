import mathutils

# create a kd-tree from a mesh
from bpy import context
obj = context.object

# 3d cursor relative to the object data
co_find = context.scene.cursor_location * obj.matrix_world.inverted()

mesh = obj.data
size = len(mesh.vertices)
kd = mathutils.kdtree.KDTree(size)

for i, v in enumerate(mesh.vertices):
    kd.insert(v.co, i)

kd.balance()


# Find the closest point to the center
co_find = (0.0, 0.0, 0.0)
co, index, dist = kd.find(co_find)
print("Close to center:", co, index, dist)


# Find the closest 10 points to the 3d cursor
print("Close 10 points")
for (co, index, dist) in kd.find_n(co_find, 10):
    print("    ", co, index, dist)


# Find points within a radius of the 3d cursor
print("Close points within 0.5 distance")
co_find = context.scene.cursor_location
for (co, index, dist) in kd.find_range(co_find, 0.5):
    print("    ", co, index, dist)
