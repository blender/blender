Triangulate Mesh
================

*This node testing is in progress, so it can be found under Beta menu*

Functionality
-------------

This node applies Triangulate operator (Ctrl+T in normal mode) to the mesh. It can triangulate all faces or only selected ones.
This node is useful mainly when other node generates ngons, especially not-convex ones.

Inputs
------

This node has the following inputs:

- **Vertices**
- **Edges**
- **Polygons**
- **Mask**. List of boolean or integer flags. Zero or False means do not triangulate face with corresponding index. If this input is not connected, then all faces will be triangulated.

Parameters
----------

This node has the following parameters:

- **Quads mode**. Method of quads processing. Available modes are:

  Beauty.
      Split the quads in nice triangles, slower method. 
  Fixed
        Split the quads on the 1st and 3rd vertices. 
  Fixed Alternate
        Split the quads on the 2nd and 4th vertices. 
  Shortest Diagonal
        Split the quads based on the distance between the vertices. 

- **Ngon mode**. Method of ngons processing. Available modes are:

  Beauty.
        Arrange the new triangles nicely, slower method. 
  Scanfill.
        Split the ngons using a scanfill algorithm. 

Outputs
-------

This node has the following outputs:

- **Vertices**. This is just copy of input vertices for convinience.
- **Edges**.
- **Polygons**.
- **NewEdges**. This contains only new edges created by triangulation procedure.
- **NewPolys**. This contains only new faces created by triangulation procedure. If ``Mask`` input is not used, then this output will contain the same as ``Polygons`` output.

Examples of usage
-----------------

Triangulated cube:

.. image:: https://cloud.githubusercontent.com/assets/284644/6314915/4fb6a9c0-ba12-11e4-9600-4d08a5d67f86.png

Triangulate only two faces of extruded polygon:

.. image:: https://cloud.githubusercontent.com/assets/284644/6314914/4f821eb2-ba12-11e4-91c6-ea464efdfea5.png

