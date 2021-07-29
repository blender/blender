Bend Object Along Surface
=========================

Functionality
-------------

This node "bends" an object in such a way, so that one of object's coordinate
planes (XY by default) will bend according the specified surface.
The surface is specified by series of vertices, which belong the surface.
Between these vertices, the surface can be interpolated either by linear or
cubic algorithm.

The surface is a 2D object, so we have two orthogonal directions on it, called
U and V.

The surface can be cyclic in one direction or in both direction, so it will be
cylindrical or toroidal surface.

The object is also rescaled so that it's sizes along two axes corresponds to
size of the surface. Scaling along third axis is optional.

Inputs
------

This node has the following inputs:

- **Vertices**. Vertices of object to be bent. This node can consume either
  list of vertices, or list of lists of vertices.
- **Surface**. Vertices of the surface along which the object should be bent.
  Please note that this input expects a list of lists of vertices for each
  object (sort of grid). Each list of vertices should describe a curve along
  one direction on the surface (for example, U). Usually you do not want to
  pass a lot of data into this input; 3x3 to 10x10 grids allow very wide range
  of useful surfaces.

Parameters
----------

This node has the following parameters:

- **Orientation**. The axis of object which should be perpendicular to the
  surface. Default value is Z, which means that XY plane of the object will be
  bent along surface.
- **Mode**. Surface interpolation mode. Available values are Linear and Cubic.
  Default value is Cubic.
- **Auto scale**. If checked, then the object is automatically scaled along the
  orientation axis, not only along the surface. Checked by default. Please note
  that calculation of this scaling value along orientation axis is not very
  precise, so probably you will want to rescale object along that axis by means
  of different node, before passing to this one.
- **Cycle U**. Whether the surface is cyclic in the U direction. Default value
  is false.
- **Cycle V**. Whether the surface is cyclic in the V direction. Default value
  is false.
- **Grouped**. If checked, then the node expects list of lists of vertices at
  **Vertices** input. If not checked, then node expects list of vertices at
  **Vertices** input. Nesting level of output is always corresponding to input.
  Default is checked.
- **Flip surface**. If checked, then the surface normal will be flipped, so the
  object will be flipped upside down with relation to the surface. Unchecked by
  default. This parameter is available only in the N panel.
- **Swap U/V**. If checked, then U and V directions of the surface will be
  swapped. This corresponds to "transposing" list of lists in the Surface
  input. Unchecked by default. This parameter is available only in the N panel.
- **Metric**. The metric to use to compute argument values of the spline, which
  correspond to surface vertices provided. Available values are: Euclidean,
  Manhattan, Chebyshev, Points. Default value is Euclidean. The default metric
  usually gives good results. If you do not like results, try other options.
  This parameter is available only in the N panel. 
- **Normal precision**. Step to be used to calculate normals of the spline.
  Lesser values correspond to better precision. In most cases, you will not
  have to change the default value. This parameter is available only in the N panel. 

Outputs
-------

This node has one output: **Vertices**.

Examples of usage
-----------------

Bend simple plane and Suzanne along surface, which is defined by 16 Empty objects (Objects In Mk3 is used):

.. image:: https://user-images.githubusercontent.com/284644/33626211-b59ac09c-da1b-11e7-88e3-ccbd3c530267.png

Hex grid bent along similar surface:

.. image:: https://user-images.githubusercontent.com/284644/33626542-b5ac9ac8-da1c-11e7-808b-858149e55ed0.png

Hex grid along toroidal surface, which is defined by 16 Empty objects as well:

.. image:: https://user-images.githubusercontent.com/284644/33627132-64f6a45a-da1e-11e7-8a72-09754362af63.png

