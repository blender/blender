Cross Section
=============

Functionality
-------------

Sect object with blender operator to edges/polygons (F or Alt+F cases). In some cases work better than new bisect node.

Inputs
------

**Vertices** and **polygons** for object, that we cut, **matrix** for this object to deform, translate before cut. **Cut matrix** - it is plane, that defined by matrix (translation+rotation).

Parameters
----------

table

+------------------+---------------+-----------------------------------------------------------------+
| Param            | Type          | Description                                                     |  
+==================+===============+=================================================================+
| **Fill section** | Bool          | Make polygons or edges                                          | 
+------------------+---------------+-----------------------------------------------------------------+
| **Alt+F/F**      | Bool          | If polygons, than triangles or single polygon                   |  
+------------------+---------------+-----------------------------------------------------------------+

Outputs
-------

**Vertices** and **Edges/Polygons**.

Example of usage
----------------

.. image:: https://cloud.githubusercontent.com/assets/5783432/4222739/260e6252-3916-11e4-8044-66b70f3e15c9.jpg
  :alt: cross_section.jpg

