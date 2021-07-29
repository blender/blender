Vector Interpolation Stripes
============================
Functionality
-------------

Performs cubic spline STRIPES interpolation based on input points by creating a function ``x,y,z = f(t)`` with ``tU=[0,1]``, ``tU=[0,1]`` and attractor vertex.
The interpolation is based on the distance between the input points.
Stripes outputs as two lines of points for each object, so UVconnect node can handle it and make polygons for stripes.


Input & Output
--------------

+--------+-----------+---------------------------------------------+
| socket | name      | Description                                 |
+========+===========+=============================================+    
| input  | Vertices  | Points to interpolate                       |
+--------+-----------+---------------------------------------------+
| input  | tU        | Values to interpolate in U direction        |
+--------+-----------+---------------------------------------------+
| input  | tV        | Values to interpolate in V direction        |
+--------+-----------+---------------------------------------------+    
| input  | Attractor | Vertex point as attractor of influence      |
+--------+-----------+---------------------------------------------+
| output | vStripes  | Interpolated points as grouped stripes      |
|        |           | [[a,b],[a,b],[a,b]], where a and b groups   |
|        |           | [v,v,v,v,v], where v - is vertex            |
+--------+-----------+---------------------------------------------+
| output | vShape    | Interpolated points simple interpolation    |
+--------+-----------+---------------------------------------------+
| output | sCoefs    | String of float coefficients for each point |
+--------+-----------+---------------------------------------------+

Parameters
----------

**Factor** - is multiplyer after produce function as sinus/cosinus/etc.
**Scale** - is multiplyer before produce function as sinus/cosinus/etc.
**Function** - popup function between Simple/Multiplyed/Sinus/Cosinus/Power/Square

Parameters extended
-------------------

**minimum** - minimum value of stripe width (0.0 to 0.5)
**maximum** - maximum value of stripe width (0.5 to 1.0)

Examples
--------

Making surface with stripes separated in two groups of nodes for UVconnect node to process:

.. image:: https://cloud.githubusercontent.com/assets/5783432/20041842/bc459a26-a488-11e6-98ec-345e58bbcdc9.png
    :alt: interpolation stripes