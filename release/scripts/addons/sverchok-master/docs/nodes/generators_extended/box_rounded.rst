Rounded box
===========

Functionality
-------------
See the BlenderArtists thread by original author Phymec. This node merely encapsulates 
the code into a form that works for Sverchok. Internally the main driver is the amount of 
input vectors, each vector represents the x y z dimensions of a box. Each box can have
unique settings. If fewer parameters are provided than sizes, then a default or the last
parameter is repeated.

Inputs & Parameters
-------------------

+----------------+-----------------------+----------------------------------------------------------------------------+
| name           | type                  | info                                                                       |
+================+=======================+============================================================================+
| radius         | single value or list  | radius of corner fillets                                                   |
+----------------+-----------------------+----------------------------------------------------------------------------+
| arc div        | single value or list  | number of divisions in the fillet                                          | 
+----------------+-----------------------+----------------------------------------------------------------------------+
| lin div        | single value or list  | number of internal divisions on straight parts (``[0..1]`` or ``[1..20]``) |
+----------------+-----------------------+----------------------------------------------------------------------------+
| Vector Size    | single vector or list | x y z dimensions for each box                                              |
+----------------+-----------------------+----------------------------------------------------------------------------+
| div type       | 3way switch, integers | just corners, corners and edges, all                                       |  
+----------------+-----------------------+----------------------------------------------------------------------------+
| odd axis align | 0..1 on or off        | internal rejiggery, not sure.                                              |
+----------------+-----------------------+----------------------------------------------------------------------------+

Outputs
-------

Depending on how many objects the input asks for, you get a Verts and Polygons list of rounded box representations.


Examples
--------

.. image:: https://cloud.githubusercontent.com/assets/619340/4471754/4987c79a-493e-11e4-89fe-bb9210af45c9.png

.. image:: https://cloud.githubusercontent.com/assets/619340/4470969/f7dca97c-4930-11e4-9cae-63f8b17826be.png

Notes
-----

see: 

**Round Cube, real Quadsphere, Capsule (snipped thread title):**

`original thread <http://blenderartists.org/forum/showthread.php?348741-Round-Cube-real-Quadsphere-Capsule-Rounded-Cuboid-3D-Grid-Convex-Hull-Margin>`_