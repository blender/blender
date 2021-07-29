Separate Loose Parts
====================

Functionality
-------------

Split a mesh into unconnected parts in a pure topological operation.

Input & Output
--------------

+--------+-----------+-------------------------------------------+
| socket | name      | Description                               |
+========+===========+===========================================+    
| input  | Vertices  | Inputs vertices                           |
+--------+-----------+-------------------------------------------+
| input  | Poly Edge | Polygon or Edge data                      |
+--------+-----------+-------------------------------------------+
| output | Vertices  | Vertices for each mesh part               |
+--------+-----------+-------------------------------------------+
| output | Poly Edge | Corresponding mesh data                   |
+--------+-----------+-------------------------------------------+

Examples
--------

.. image:: https://cloud.githubusercontent.com/assets/619340/4186249/46e799f2-375f-11e4-8fab-4bf1776b244a.png
  :alt: separate-looseDemo1.png

Notes
-------

Note that it doesn't take double vertices into account.
There is no guarantee about the order of the outputs
