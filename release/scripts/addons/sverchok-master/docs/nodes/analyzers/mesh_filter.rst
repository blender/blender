Mesh Filter
===========

*destination after Beta: Analyzers*

Functionality
-------------

This node sorts vertices, edges or faces of input mesh by several available criterias: boundary vs interior, convex vs concave and so on. For each criteria, it puts "good" and "bad" mesh elements to different outputs. Also mask output is available for each criteria.

Inputs
------

This node has the following inputs:

- **Vertices**
- **Edges**
- **Faces**

Parameters
----------

This node has the following parameters:

- **Mode**. Which sort of mesh elements to operate on. There are three modes available: Vertices, Edges and Faces.
- **Filter**. Criteria to be used for filtering. List of criterias available depends on mode selected. See below.

Outputs
-------

Set of outputs depends on selected mode. See description of modes below.

Modes
-----

Vertices
^^^^^^^^

The following filtering criteria are available for the ``Vertices`` mode:

Wire.
    Vertices that are not connected to any faces.
Boundary.
    Vertices that are connected to boundary edges.
Interior.
    Vertices that are not wire and are not boundary.

The following outputs are used in this mode:

- **YesVertices**. Vertices that comply to selected criteria. 
- **NoVertices**. Vertices that do not comply to selected criteria.
- **VerticesMask**. Mask output for vertices. True for vertex that comly selected criteria.
- **YesEdges**. Edges that connect vertices complying to selected criteria.
- **NoEdges**. Edges that connect vertices not complying to selected criteria.
- **YesFaces**. Faces, all vertices of which comply to selected criteria.
- **NoFaces**. Faces, all vertices of which do not comply to selected criteria.

Note that since in this mode the node filters vertices, the indicies of vertices in input list are not valid for lists in ``YesVertices`` and ``NoVertices`` outputs. So in edges and faces outputs, this node takes this filtering into account. Indicies in ``YesEdges`` output are valid for list of vertices in ``YesVertices`` output, and so on.

Edges
^^^^^

The following filtering criteria are available for the ``Edges`` mode:

Wire.
  Edges that are not connected to any faces.
Boundary.
  Edges that are at the boundary of manifold part of mesh.
Interior.
  Edges that are manifold and are not boundary.
Convex.
  Edges that joins two convex faces. This criteria depends on valid face normals.
Concave.
  Edges that joins two concave faces. This criteria also depends on valid face normals.
Contiguous.
  Manifold edges between two faces with the same winding; in other words, the edges which connect faces with the same normals direction (inside or outside).

The following outputs are used in this mode:

- **YesEdges**. Edges that comply to selected criteria.
- **NoEdges**. Edges that do not comply to selected criteria.
- **Mask**. Mask output.

Faces
^^^^^

For this mode, only one filtering criteria is available: interior faces vs boundary faces. Boundary face is a face, any edge of which is boundary. All other faces are considered interior.

The following outputs are used in this mode:

- **Interior**. Interior faces.
- **Boundary**. Boundary faces.
- **BoundaryMask**. Mask output. It contains True for faces which are boundary.

Examples of usage
-----------------

Move only boundary vertices of plane grid:

.. image:: https://cloud.githubusercontent.com/assets/284644/6081558/53b7ea70-ae3c-11e4-9c32-b147be11af5b.png

Bevel only concave edges:

.. image:: https://cloud.githubusercontent.com/assets/284644/6081559/53ed9486-ae3c-11e4-8fbc-82f7e8029434.png

Extrude only boundary faces:

.. image:: https://cloud.githubusercontent.com/assets/284644/6081560/53eea0f6-ae3c-11e4-889c-f60441accc1d.png

