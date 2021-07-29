Subdivide Node
==============

Functionality
-------------

This node applies Blender's Subvidide operation to the input mesh. Please note that of options available differs from usual editing operator.

Inputs
------

This node has the following inputs:

- **Vertrices**
- **Edges**. For this node to produce interesting result, this input must be provided.
- **Faces**
- **EdgeMask**. Selected edges to be subdivided. Faces surrounded by subdivided edges can optionally be subdivided too.
- **Number of Cuts**
- **Smooth**
- **Fractal**
- **Along normal**
- **Seed**

Parameters
----------

This node has the following parameters:

- **Show Old**. If checked, then outputs with "old" geometry will be shown. By default not checked.
- **Show New**. If checked, then outputs with newly created geometry will be shown. By default not checked.
- **Show Options**. If checked, then following parameters will be shown on the node itself. Otherwise, they will be available only in the N panel. By default not checked.
- **Falloff**. Smooth falloff type. Please refer to examples below for demonstration.
- **Corner cut type**. This controls the way quads with only two adjacent selected edges are subdivided. Available values are:

  - **Inner vertices**
  - **Path**
  - **Fan**
  - **Straight Cut**
- **Grid fill**. If checked, then fully-selected faces will be filled with a grid (subdivided). Otherwise, only edges will be subdiveded, not faces. Checked by default.
- **Only Quads**. If checked, then only quad faces will be subdivided, other will not. By default not checked.
- **Single edge**. If checked, tessellate the case of one edge selected in a quad or triangle. By default not checked.
- **Even smooth**. Maintain even offset when smoothing. By default not checked.
- **Number of Cuts**. Specifies the number of cuts per edge to make. By default this is 1, cutting edges in half. A value of 2 will cut it into thirds, and so on. This parameter can be also provided as input.
- **Smooth**. Displaces subdivisions to maintain approximate curvature, The effect is similar to the way the Subdivision Surface Modifier might deform the mesh. This parameter can be also provided as input.
- **Fractal**. Displaces the vertices in random directions after the mesh is subdivided. This parameter can be also provided as input.
- **Along normal**. If set to 1, causes the vertices to move along the their normals, instead of random directions. Values between 0 and 1 lead to intermediate results. This parameter can be also provided as input.
- **Seed**. Random seed. This parameter can be also provided as input.

Outputs
-------

This node has the following outputs:

- **Vertices**. All vertices of resulting mesh.
- **Edges**. All edges of resulting mesh.
- **Faces**. All faces of resulting mesh.
- **NewVertices**. All vertices that were created by subdivision. This output is only visible when **Show New** parameter is checked.
- **NewEdges**. Edges that were created by subdividing faces. This output is only visible when **Show New** parameter is checked.
- **NewFaces**. Faces that were created by subdividing faces. This output is only visible when **Show New** parameter is checked.
- **OldVertices**. Only vertices that were created on previously existing edges. This output is only visible when **Show Old** parameter is checked.
- **OldEdges**. Only edges that were created by subdividing existing edges. This output is only visible when **Show Old** parameter is checked.
- **OldFaces**. Only faces that were created by subdividing existing faces. This output is only visible when **Show Old** parameter is checked.

**Note**: Indicies in **NewEdges**, **NewFaces**, **OldEdges**, **OldFaces** outputs relate to vertices in **Vertices** output.

Examples of usage
-----------------

The simplest example, subdivide a cube:

.. image:: https://cloud.githubusercontent.com/assets/284644/25096716/476682dc-23c3-11e7-8788-624be2573d3b.png

Subdivide one face of a cube, with smoothing:

.. image:: https://cloud.githubusercontent.com/assets/284644/25096718/47976654-23c3-11e7-8da8-87ea420d8355.png

Subdivide a cube, with smooth falloff type = Smooth:

.. image:: https://cloud.githubusercontent.com/assets/284644/25096717/4794fed2-23c3-11e7-8c53-28fb1d18b69d.png

Subdivide a torus, with smooth falloff type = Sphere:

.. image:: https://cloud.githubusercontent.com/assets/284644/25096721/479a2c72-23c3-11e7-9012-612ce3fd1039.png

