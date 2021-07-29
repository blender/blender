Lathe
=====

Functionality
-------------

Analogous to the `spin` operator and the Screw modifier. It takes a profile shape as input in the form of *vertices* and *edges* and produces *vertices* and *faces* based on a rotation axis, angle, center, delta and step count. Internally the node is powered by the `bmesh.spin <http://www.blender.org/documentation/blender_python_api_2_71_release/bmesh.ops.html#bmesh.ops.spin>`_  operator.

Inputs
------

It's vectorized, meaning it accepts nested and multiple inputs and produces multiple sets of output

Parameters
----------

All Vector parameters (except axis) default to (0,0,0) if no input is given. 

+-------------+---------------+-----------------------------------------------------------------+
| Param       | Type          | Description                                                     |  
+=============+===============+=================================================================+
| **cent**    | Vector        | central coordinate around which to pivot                        | 
+-------------+---------------+-----------------------------------------------------------------+
| **axis**    | Vector        | axis around which to rotate around the pivot, default (0, 0, 1) |  
+-------------+---------------+-----------------------------------------------------------------+
| **dvec**    | Vector        | is used to push the center Vector by a Vector quantity per step | 
+-------------+---------------+-----------------------------------------------------------------+
| **Degrees** | Scalar, Float | angle of the total rotation. Default 360.0                      |
+-------------+---------------+-----------------------------------------------------------------+
| **Steps**   | Scalar, Int   | numer of rotation steps. Default 20                             | 
+-------------+---------------+-----------------------------------------------------------------+
| **Merge**   | Bool, toggle  | removes double vertices if the geometry can be merged,          |  
|             |               | usually used to prevent doubles of first profile and last       |
|             |               | profile copy. Default `off`.                                    | 
+-------------+---------------+-----------------------------------------------------------------+


Outputs
-------

**Vertices** and **Poly**. Verts and Polys will be generated. The ``bmesh.spin`` operator doesn't consider the ordering of the Vertex and Face indices that it outputs. This might make additional processing complicated, use IndexViewer to better understand the generated geometry. Faces will however have consistent Normals.


Example of usage
----------------

.. image:: https://cloud.githubusercontent.com/assets/619340/3172893/08952296-ebdd-11e3-8e9b-574495b1a92c.png

See the progress of how this node came to life `here <https://github.com/nortikin/sverchok/issues/203>`_ (gifs, screenshots)

Glass, Vase.
