Proportional Edit Falloff
=========================

Functionality
-------------

This node implements Blender's concept of "proportional edit mode" in Sverchok. It converts vertex selection mask into selection coefficients. Vertices selected by mask get the coefficient of 1.0. Vertices that are farther than specified radius from selection, get the coefficient of 0.0. 

Supported falloff modes are basically the same as Blender's.

Inputs
------

This node has the following inputs:

- **Vertices**
- **Mask**
- **Radius**. Proportional edit radius.

Parameters
----------

This node has the following parameters:

- **Falloff type**. Proportional edit falloff type. Supported values are:

  * Smooth
  * Sharp
  * Root
  * Linear
  * Sphere
  * Inverse Square
  * Constant

  The meaning of values is all the same as for standard Blender's proportional edit mode.

- **Radius**. Proportional edit radius. This parameter can be also provided by input.

Outputs
-------

This node has one output: **Coeffs**. It contains one real value for each input vertex. All values are between 0.0 and 1.0.

Examples of usage
-----------------

Drag a circle on one side of the box, with Smooth falloff:

.. image:: https://cloud.githubusercontent.com/assets/284644/24107714/ae5db56a-0db5-11e7-860e-0156b8b10283.png

All the same, but with Const falloff:

.. image:: https://cloud.githubusercontent.com/assets/284644/24107713/ae5d8892-0db5-11e7-882e-15922c4a41de.png

Example of usage with Extrude Separate node:

.. image:: https://cloud.githubusercontent.com/assets/284644/24107716/ae923dbc-0db5-11e7-8954-11a552c12ecc.png

