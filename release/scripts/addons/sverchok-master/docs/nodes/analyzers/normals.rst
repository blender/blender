Calculate Normals
=================

Functionality
-------------

This node calculates normals for faces and edges of given mesh. Normals can be calculated even for meshes without faces, i.e. curves.

Inputs
------

This node has the following inputs:

- **Vertices**
- **Edges**
- **Polygons**

Outputs
-------

This node has the following outputs:

- **FaceNormals**. Normals of faces. This output will be empty if **Polygons** input is empty.
- **VertexNormals**. Normals of vertices.

Examples of usage
-----------------

Move each face of cube along its normal:

.. image:: https://cloud.githubusercontent.com/assets/284644/5989203/f86367f2-a9a0-11e4-9292-d303838d561d.png

Visualization of vertex normals for bezier curve:

.. image:: https://cloud.githubusercontent.com/assets/284644/5989204/f8655fbc-a9a0-11e4-94d5-caf403d3a64a.png

Normals can be also calculated for closed curves:

.. image:: https://cloud.githubusercontent.com/assets/284644/5989202/f8632a44-a9a0-11e4-8745-19065eb13bcd.png

