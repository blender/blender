Offset line
======

Functionality
-------------

This node works only in XY surface. It takes X and Y coordinate from vectors input. Also Z coordinate is used now but this node is still work like 2D node. Z coordinate of the new points is equal z coordinate of the nearest old points. So if you use z coordinate you have to remember:

- All points are projected to the XY surface so you can use Z coordinate for showing different levels of a flat mesh.
- Vertical edges will broke down work of the node.
- Normals of all faces always look to up.

Use ``delete loose`` node before if your input mesh has points without edges. You will receive surface along an input mesh edges with width equal offset value. It is also available to receive outer edges and mask of new and old points.

Inputs
------

This node has the following inputs:

- **Vers** - vertices of objects.
- **Edgs** - polygons of objects.
- **offset** - offset values - available multiple value per object.

Parameters
----------

All parameters can be given by the node or an external input.
``offset`` is vectorized and it will accept single or multiple values.

+-----------------+---------------+-------------+-------------------------------------------------------------+
| Param           | Type          | Default     | Description                                                 |
+=================+===============+=============+=============================================================+
| **offset**      | Float         | 0.10        | offset values.                                              |
+-----------------+---------------+-------------+-------------------------------------------------------------+

Outputs
-------

This node has the following outputs:

- **Vers**
- **Faces**
- **OuterEdges** - get outer edges, use together with ``delete loose`` node after or ``mask verts``. The list of edges is unordered.
- **VersMask** - 0 for new points and 1 for old.

Examples of usage
-----------------

To receive offset from input object from scene:

.. image:: https://user-images.githubusercontent.com/28003269/34199193-5e1281a4-e586-11e7-97b8-f1984facdfcb.png

Using of outer edges:

.. image:: https://user-images.githubusercontent.com/28003269/34199326-dadbf508-e586-11e7-9542-7b7ff4a9521f.png

Using of vertices mask with ``transform select`` node:

.. image:: https://user-images.githubusercontent.com/28003269/34199698-125ed63e-e588-11e7-9e34-83c5eb33cde9.png

Different values for each object and each point:

.. image:: https://user-images.githubusercontent.com/28003269/34353407-47f2d918-ea41-11e7-92c0-f0f9751e4cab.png

Using of Z coordinate:

.. image:: https://user-images.githubusercontent.com/28003269/34353545-3e4c0a6e-ea42-11e7-9b63-da65bd45f07a.png
