Overlap Polygons
=================

Functionality
-------------

For every polygon of one object search intersection at other object. 
Epsilon makes it harder to find intersaction. Based on BVHtree ``mathutils.bvhtree``. 

Inputs
------

+--------+--------------+--------------------------+
| Mode   | Input Name   | type                     |
+========+==============+==========================+
| All    | Vert(A)      | vertices                 |
+--------+--------------+--------------------------+
| All    | Poly(A)      | polygons                 |
+--------+--------------+--------------------------+
| All    | Vert(B)      | vertices                 |
+--------+--------------+--------------------------+
| All    | Poly(B)      | polygons                 |
+--------+--------------+--------------------------+


Parameters
----------

+---------------+-----------------------------------------------------------------------------------------+
| Mode          | Description                                                                             |
+===============+=========================================================================================+
| all triangles | Boolean to work with triangles makes it faster to calculate                             |
+---------------+-----------------------------------------------------------------------------------------+
| epsilon       | float threashold for cut weak results                                                   |
+---------------+-----------------------------------------------------------------------------------------+


Outputs
-------


+--------+-------------------+--------------------------+
| Mode   | Input Name        | type                     |
+========+===================+==========================+
| All    | PolyIndex(A)      | indices                  |
+--------+-------------------+--------------------------+
| All    | PolyIndex(B)      | indices                  |
+--------+-------------------+--------------------------+
| All    | OverlapPoly(A)    | polygons                 |
+--------+-------------------+--------------------------+
| All    | OverlapPoly(B)    | polygons                 |
+--------+-------------------+--------------------------+


Examples
--------


.. image:: https://user-images.githubusercontent.com/5783432/30777862-8d369f36-a0cd-11e7-8c8e-a72e7aa8ee7f.png
https://github.com/nortikin/sverchok/files/1326934/bvhtree-overlap_2017_09_23_23_07.zip


Notes
-----

pass

