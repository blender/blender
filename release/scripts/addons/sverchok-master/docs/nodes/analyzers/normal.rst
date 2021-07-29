Vertex normal
=============
*Alias - vector normal*

Functionality
-------------

Vertex normal node finds normals of vectors

Inputs
------

**Vertices** and **Polygons** are needed. 
Both inputs need to be of the kind Vertices and Strings, respectively

Parameters
----------

All parameters need to proceed from an external node.


+------------------+---------------+-------------+-----------------------------------------------+
| Param            | Type          | Default     | Description                                   |  
+==================+===============+=============+===============================================+
| **Vertices**     | Vertices      | None        | vertices of the polygons                      | 
+------------------+---------------+-------------+-----------------------------------------------+
| **Polygons**     | Strings       | None        | polygons referenced to vertices               |
+------------------+---------------+-------------+-----------------------------------------------+

Outputs
-------

**Vertices normals** will be calculated only if both **Vertices** and **Polygons** inputs are linked.


Example of usage
----------------

.. image:: https://cloud.githubusercontent.com/assets/5783432/18602881/e4cf2508-7c7d-11e6-8c63-8918c9a160a5.png
  :alt: Vector_normal.PNG

