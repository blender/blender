Extrude Edges
=============

*This node testing is in progress, so it can be found under Beta menu*

Functionality
-------------

This node applies Extrude operator to edges of input mesh. After that, matrix transformation can be applied to new vertices.
It is possible to provide specific transformation matrix for each of extruded vertices.

Inputs
------

This node has the following inputs:

- **Vertices**
- **Edges**
- **Polygons**
- **ExtrudeEdges**. Edges of input mesh that are to be extruded. If this input is empty or not connected, then by default all edges will be processed.
- **Matrices**. Transformation matrices to be applied to extruded vertices. This input can contain separate matrix for each vertex. In simplest case, it can contain one matrix to be applied to all vertices.

Outputs
-------

This node has the following outputs:

- **Vertices**
- **Edges**
- **Polygons**. All faces of resulting mesh.
- **NewVertices**. Newly created vertices only.
- **NewEdges**. Newly created edges only.
- **NewFaces**. Newly created faces only.

Examples of usage
-----------------

Extrude only boundary edges of plane grid, along Z axis:

.. image:: https://cloud.githubusercontent.com/assets/284644/6318599/6ee0d474-babc-11e4-8d3a-f9f86963bf10.png

Extrude all edges of bitted circle, and scale new vertices:

.. image:: https://cloud.githubusercontent.com/assets/284644/6318598/6eb0c3ba-babc-11e4-8cab-ccb2d4fe39a9.png

