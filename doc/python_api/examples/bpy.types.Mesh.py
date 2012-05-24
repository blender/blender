"""
Mesh Data
+++++++++

The mesh data is accessed in object mode and intended for compact storage,
for more flexible mesh editing from python see :mod:`bmesh`.

Blender stores 4 main arrays to define mesh geometry.

* :class:`Mesh.vertices` (3 points in space)
* :class:`Mesh.edges` (reference 2 vertices)
* :class:`Mesh.loops` (reference a single vertex and edge)
* :class:`Mesh.polygons`: (reference a range of loops)


Each polygon reference a slice in the loop array, this way, polygons do not store vertices or corner data such as UV's directly,
only a reference to loops that the polygon uses.

:class:`Mesh.loops`, :class:`Mesh.uv_layers` :class:`Mesh.vertex_colors` are all aligned so the same polygon loop
indicies can be used to find the UV's and vertex colors as with as the vertices.

To compare mesh API options see: :ref:`NGons and Tessellation Faces <info_gotcha_mesh_faces>`


This example script prints the vertices and UV's for each polygon, assumes the active object is a mesh with UVs.
"""

import bpy

me = bpy.context.object.data
uv_layer = me.uv_layers.active.data

for poly in me.polygons:
    print("Polygon index: %d, length: %d" % (poly.index, poly.loop_total))

    # range is used here to show how the polygons reference loops,
    # for convenience 'poly.loop_indices' can be used instead.
    for loop_index in range(poly.loop_start, poly.loop_start + poly.loop_total):
        print("    Vertex: %d" % me.loops[loop_index].vertex_index)
        print("    UV: %r" % uv_layer[loop_index].uv)
