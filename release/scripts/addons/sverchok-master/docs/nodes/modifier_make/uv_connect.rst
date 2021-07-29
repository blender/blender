UV Connection
=============

Functionality
-------------

Making edges/polygons between vertices objects it several ways.

Inputs
------

Vertices. Multysockets can eat many objects. every object to be connecting with other.

Parameters
----------

table

+---------------+---------------+-----------------------------------------------------------------+
| Param         | Type          | Description                                                     |  
+===============+===============+=================================================================+
| **UVdir**     | Enum          | Direction to connect edges and polygons                         | 
+---------------+---------------+-----------------------------------------------------------------+
| **cicled**    | Bool, toggle  | For edges and polygons close loop                               |  
+---------------+---------------+-----------------------------------------------------------------+
| **polygons**  | Bool, toggle  | Active - make polygon, else edge                                | 
+---------------+---------------+-----------------------------------------------------------------+
| **slice**     | Bool, toggle  | Polygons can be as slices or quads                              |
+---------------+---------------+-----------------------------------------------------------------+

Outputs
-------

**Vertices** and **Edges/Polygons**. Verts and Polys will be generated. The Operator doesn't consider the ordering of the Vertex and Face indices that it outputs. This might make additional processing complicated, use IndexViewer to better understand the generated geometry. Faces will however have consistent Normals.

Example of usage
----------------

.. image:: https://cloud.githubusercontent.com/assets/5783432/4199915/97853346-380a-11e4-9968-3661e95bf80c.png
  :alt: ConnectingUV.PNG

