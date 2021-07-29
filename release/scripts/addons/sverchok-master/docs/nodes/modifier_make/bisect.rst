Bisect
======

Functionality
-------------

This can give the cross section of an object shape from any angle. The implementation is from ``bmesh.ops.bisect_plane``. It can also provide either side of the cut, separate or joined.


Inputs 
------

*Vertices*, *PolyEdges* and *Matrix*


Parameters
----------

+-------------+------+---------------------------------------------------+
| Parameter   | Type | Description                                       |
+=============+======+===================================================+
| Clear Inner | bool | don't include the negative side of the Matrix cut |
+-------------+------+---------------------------------------------------+
| Clear Outer | bool | don't include the positive side of the Matrix cut |
+-------------+------+---------------------------------------------------+
| Fill cuts   | bool | generates a polygon from the bisections           |
+-------------+------+---------------------------------------------------+

Outputs
-------

*Vertices*, *Edges*, and *Polygons*. 



Examples
--------

.. image:: https://cloud.githubusercontent.com/assets/619340/4187440/f2a873f6-3769-11e4-9192-01ee23770ec8.PNG
  :alt: bisectdemo1.png

.. image:: https://cloud.githubusercontent.com/assets/619340/4187718/422d78a2-376c-11e4-8634-3d8b84b272d0.PNG
  :alt: bisectdemo2.png

Notes
-----

