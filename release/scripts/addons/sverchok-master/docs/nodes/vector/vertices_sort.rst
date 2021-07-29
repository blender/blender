Vector Sort
===========

Functionality
-------------

This Node sort the sequence of index according to python sort, with different criteria.

Inputs
------

Inputs are vertices lists(vectors, tuple) and polygons/edges as integer lists,
and optional inputs(Vector, matrix and user data)

Parameters
----------

+--------------+---------------+-------------+----------------------------------------------------+
| Param        | Type          | Default     | Description                                        |
+==============+===============+=============+====================================================+
| **Vertices** | Vector        |             | vertices from nodes generators or lists (in, out)  |
+--------------+---------------+-------------+----------------------------------------------------+
| **PolyEdge** | Int           |             | index of polgons or edges     (in, out)            |
+--------------+---------------+-------------+----------------------------------------------------+
| **Sortmode** | XYZ, Dist,    | XYZ         | will sort the index according to different criteria|
|              | Axis, Connect,|             |                                                    |
|              | User          |             |                                                    |
+--------------+---------------+-------------+----------------------------------------------------+
| **Item order** | Int         |             | output the index sequence                          |
+--------------+---------------+-------------+----------------------------------------------------+

Outputs
-------

The node will output the Vertices as list of Vectors(tuples), Polys/edges as int
and the sorted list of Vertices.

Example of usage
----------------

Example with an Hilbert 3d node and polyline viewer with Vector sort set to Dist:

.. image:: https://cloud.githubusercontent.com/assets/1275858/24357298/7c3e0f6a-12fd-11e7-9852-0d800ec51742.png

The *Connect* mode it is meant to work with paths. Sorting the vertices along the edges.
Example used to sort the vertices after the *Mesh Filter* node

.. image:: https://user-images.githubusercontent.com/10011941/35187803-3f88191c-fe2a-11e7-874b-da8cb4ec3751.png

Example after *Subdivide* node to prepare vertices for *Vector Evaluation* node and/or *UV Connection* node

.. image:: https://user-images.githubusercontent.com/10011941/35187937-6352b0c0-fe2d-11e7-864c-a3893f94f6dc.png

link to pull request: https://github.com/nortikin/sverchok/pull/88
