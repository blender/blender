Fill Holes
==========

Functionality
-------------

It fills closed countors from edges that own minimum vertices-sides with polygons.

Inputs
------

- **Vertices**
- **Edges**

Parameters
----------

+-----------------+---------------+-------------+-------------------------------------------------------------+
| Param           | Type          | Default     | Description                                                 |
+=================+===============+=============+=============================================================+
| **Sides**       | Float         | 4           | Number of sides that will be collapsed to polygon.          |
+-----------------+---------------+-------------+-------------------------------------------------------------+

Outputs
-------

- **Vertices**
- **Edges**
- **Polygons**. All faces of resulting mesh.

Examples of usage
-----------------

Fill holes of formula shape, edges of initial shape + voronoi grid + fill holes

.. image:: https://cloud.githubusercontent.com/assets/5783432/18611146/24f2afaa-7d42-11e6-9822-5637a752a1b6.png
