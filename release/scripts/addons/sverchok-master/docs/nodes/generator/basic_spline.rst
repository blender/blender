2 Point Spline
==============

Functionality
-------------

Single section Bezier Spline. Creates a *Spline Curve* from 2 sets of points. Analogue to the Blender native Curve object, but limited to 2 pairs of *knots* and *control points* per curve.

Inputs
------

+-----------+--------+-------------------------------------------------------------+
| Parameter | Type   | Description                                                 |
+===========+========+=============================================================+
| num verts | int    | per curve this sets how many verts define the curve         |
+-----------+--------+-------------------------------------------------------------+
| knot 1    | Vector | These place and adjust the shape of the curve. The knots    |
+-----------+--------+ are vectors on the curve, the controls are vectors to which |
| control 1 | Vector | the curve is mathematically attracted                       | 
+-----------+--------+                                                             | 
| control 2 | Vector |                                                             |
+-----------+--------+                                                             |
| knot 2    | Vector |                                                             | 
+-----------+--------+-------------------------------------------------------------+

The node accepts these 
The node will adjust to make sure the length of 


Parameters
----------

The Node is vectorized in the following way. If any of the *knots* or *control points* are given in a list that doesn't match the length of the other lists, then the last value of that shorter list is repeated to match the length of the longest. 

This means; if *knot1, control1, knot2* are length 3, 4 and 8 and control2 is length 20 then 
*knot1, control1, knot2* will all get their last value repeated till the full list matches 20 values.

The same *filling* procedure is applied to the *Num Verts* parameter.

Outputs
-------

- (verts, edges) : A set of each of these that correspond with a packet of commands like 'knot1, ctrl1, ctrl2, knot2'
- verts needs to be connected to get output
- edges is optional

**optionals for visualizing the curve handles**

- hnd. Verts 
- hnd. Edges

Passing hnd.Verts and hnd.Edges to a ViewerDraw node helps visualize the Handles that operate on your Spline curve. 


Examples
--------

.. image:: https://cloud.githubusercontent.com/assets/619340/3362071/c3b7f346-fb05-11e3-9af7-35dfda973712.png
.. image:: https://cloud.githubusercontent.com/assets/619340/3362910/c18e4eea-fb0e-11e3-9a80-4624d30c65e9.gif

See the progress of how this node came to life `here <https://github.com/nortikin/sverchok/issues/247>`_ (gifs, screenshots)
