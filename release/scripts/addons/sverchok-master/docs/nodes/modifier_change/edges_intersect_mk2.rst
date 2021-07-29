Intersect Edges
===============

Functionality
-------------

The code is straight out of TinyCAD plugin's XALL operator, which is part of Blender Contrib distributions.

It operates on Edge based geometry only and will create new vertices on all intersections of the given geometry. 
This node goes through a recursive process (divide and conquer) and its speed is directly proportional to the 
number of intersecting edges passed into it. The algorithm is not optimized for large edges counts, but tends 
to work well in most cases. 

**implementation note**

An Edge that touches the vertex of another edge is not considered an `Intersection` in the current implementation. 
*Touching* might be included as an intersection type in the future via an extra toggle in the Properties Panel.

Inputs
------

Verts and Edges only. Warning: Does not support faces, or Vectorized (nested lists) input


Parameters
----------

Currently no parameters, but in the future could include a tolerance parameter and a setting to consider Touching Verts-to-Edge as an Intersection.


Outputs
-------

Vertices and Edges, the mesh does not preserve any old vertex or edge index ordering due to the Hashing algorithm used for fast intersection lookups.


Examples
--------

.. image:: https://cloud.githubusercontent.com/assets/619340/2811581/16032c72-ce26-11e3-9055-925d2cd03719.png

See the progress of how this node came to life `here <https://github.com/nortikin/sverchok/issues/109>`_ (gifs, screenshots)