Convex Hull
===========

Functionality
-------------

Use this to skin a simple cloud of points. The algorithm is known as `Convex Hull <http://en.wikipedia.org/wiki/Convex_hull_algorithms>`_, and implemented in ``bmesh.ops.convex_hull``. 


Input
------

*Vertices*


Outputs
-------

*Vertices* and *Polygons*. The number of vertices will be either equal or less than the original number. Any internal points to the system will be rejected and therefore not part of the output vertices. 


Examples
--------

.. image:: https://cloud.githubusercontent.com/assets/619340/4187179/8a1e6054-3767-11e4-9a18-97aa66629fcd.PNG
  :alt: ConvexHullDemo1.PNG

Notes
-----
