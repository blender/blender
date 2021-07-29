Bend Object Along Path
======================

Functionality
-------------

This node "bends" an object in such a way, that one of it's axes (Z by default)
bends along specified path.
The path is specified by series of vertices, which belong to that path. Between
these vertices, the path can be interpolated either by linear or cubic algorithm.

The object is also rescaled along specified axis, so that it's size along this
axis becomes equal to length of the path. Scaling along two other axes is
optional.

It is in general not a trivial task to rotate a 3D object along a vector,
because there are always 2 other axes of object and it is not clear where
should they be directed to. So, this node supports 3 different algorithms of
object rotation calculation. In many simple cases, all these algorithms will
give exactly the same result. But in more complex setups, or in some corner
cases, results can be very different. So, just try all algorithms and see which
one fits you better.

Inputs
------

This node has the following inputs:

- **Vertices**. Vertices of object to be bent.
- **Path**. Vertices of path along which the object should be bent. For most
  cases, you will not want to provide a lot of points into this input; 3 to 10
  vertices can describe very wide range of useful curves.

Parameters
----------

This node has the following parameters:

- **Orientation**. The axis of object which should be bent along path. Default
  value is Z.
- **Mode**. Path interpolation mode. Available values are Linear and Cubic.
  Default value is Cubic.
- **Scale all axes**. If checked, then the object will be scaled not only along
  orientation axis, but also along two other by the same scale. Otherwise, the
  object will be scaled along orientation axis only. Checked by default.
- **Cyclic**. Indicate whether the path is cyclic. Default value is false.
- **Algorithm**. Rotation calculation algorithm. Available values are:

  * Householder: calculate rotation by using Householder's reflection matrix
    (see Wikipedia_ article).                   
  * Tracking: use the same algorithm as in Blender's "TrackTo" kinematic
    constraint. This algorithm gives you a bit more flexibility comparing to
    other, by allowing to select the Up axis.                                                         
  * Rotation difference: calculate rotation as rotation difference between two
    vectors.                                         

  Default value is Householder.

- **Up axis**.  Axis of donor object that should point up in result. This
  parameter is available only when Tracking algorithm is selected.  Value of
  this parameter must differ from **Orientation** parameter, otherwise you will
  get an error. Default value is X.
- **Flip spline**. This parameter is available only in the N panel. If checked,
  then direction of the spline is inverted comparing to the order of path vertices
  provided. Unchecked by default.
- **Metric**. The metric to use to compute argument values of the spline, which
  correspond to path vertices provided. Available values are: Euclidean,
  Manhattan, Chebyshev, Points. Default value is Euclidean. The default metric
  usually gives good results. If you do not like results, try other options.
  This parameter is available only in the N panel. 
- **Tangent precision**. Step to be used to calculate tangents of the spline.
  Lesser values correspond to better precision. In most cases, you will not
  have to change the default value. This parameter is available only in the N panel. 

.. _Wikipedia: https://en.wikipedia.org/wiki/QR_decomposition#Using_Householder_reflections

Outputs
-------

This node has one output: **Vertices**.

Examples of usage
-----------------

Cylinder bent along curve made from NGon:

.. image:: https://user-images.githubusercontent.com/284644/33626075-6a94d0e2-da1b-11e7-83bb-fc859eda2cdc.png

Hexagonal grid bent along similar curve (the curve itself is drawn in red):

.. image:: https://user-images.githubusercontent.com/284644/33674756-3d0fa234-dad2-11e7-9e04-a043b94cb377.png

