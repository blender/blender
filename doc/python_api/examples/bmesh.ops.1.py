# This script uses bmesh operators to make 2 links of a chain.

import bpy
import bmesh
import math
import mathutils

# Make a new BMesh
bm = bmesh.new()

# Add a circle XXX, should return all geometry created, not just verts.
bmesh.ops.create_circle(
    bm,
    cap_ends=False,
    radius=0.2,
    segments=8)


# Spin and deal with geometry on side 'a'
edges_start_a = bm.edges[:]
geom_start_a = bm.verts[:] + edges_start_a
ret = bmesh.ops.spin(
    bm,
    geom=geom_start_a,
    angle=math.radians(180.0),
    steps=8,
    axis=(1.0, 0.0, 0.0),
    cent=(0.0, 1.0, 0.0))
edges_end_a = [ele for ele in ret["geom_last"]
               if isinstance(ele, bmesh.types.BMEdge)]
del ret


# Extrude and create geometry on side 'b'
ret = bmesh.ops.extrude_edge_only(
    bm,
    edges=edges_start_a)
geom_extrude_mid = ret["geom"]
del ret


# Collect the edges to spin XXX, 'extrude_edge_only' could return this.
verts_extrude_b = [ele for ele in geom_extrude_mid
                   if isinstance(ele, bmesh.types.BMVert)]
edges_extrude_b = [ele for ele in geom_extrude_mid
                   if isinstance(ele, bmesh.types.BMEdge) and ele.is_boundary]
bmesh.ops.translate(
    bm,
    verts=verts_extrude_b,
    vec=(0.0, 0.0, 1.0))


# Create the circle on side 'b'
ret = bmesh.ops.spin(
    bm,
    geom=verts_extrude_b + edges_extrude_b,
    angle=-math.radians(180.0),
    steps=8,
    axis=(1.0, 0.0, 0.0),
    cent=(0.0, 1.0, 1.0))
edges_end_b = [ele for ele in ret["geom_last"]
               if isinstance(ele, bmesh.types.BMEdge)]
del ret


# Bridge the resulting edge loops of both spins 'a & b'
bmesh.ops.bridge_loops(
    bm,
    edges=edges_end_a + edges_end_b)


# Now we have made a links of the chain, make a copy and rotate it
# (so this looks something like a chain)

ret = bmesh.ops.duplicate(
    bm,
    geom=bm.verts[:] + bm.edges[:] + bm.faces[:])
geom_dupe = ret["geom"]
verts_dupe = [ele for ele in geom_dupe if isinstance(ele, bmesh.types.BMVert)]
del ret

# position the new link
bmesh.ops.translate(
    bm,
    verts=verts_dupe,
    vec=(0.0, 0.0, 2.0))
bmesh.ops.rotate(
    bm,
    verts=verts_dupe,
    cent=(0.0, 1.0, 0.0),
    matrix=mathutils.Matrix.Rotation(math.radians(90.0), 3, 'Z'))

# Done with creating the mesh, simply link it into the scene so we can see it

# Finish up, write the bmesh into a new mesh
me = bpy.data.meshes.new("Mesh")
bm.to_mesh(me)
bm.free()


# Add the mesh to the scene
obj = bpy.data.objects.new("Object", me)
bpy.context.collection.objects.link(obj)

# Select and make active
bpy.context.view_layer.objects.active = obj
obj.select_set(True)
