Matrix Shear
============

.. image:: https://cloud.githubusercontent.com/assets/619340/4186363/32974f5a-3760-11e4-9be7-5e16ce549d0d.PNG
  :alt: Matrix_Shear.PNG

Functionality
-------------

Similar in behaviour to the ``Transform -> Shear`` tool in Blender (`docs <http://wiki.blender.org/index.php/Doc:2.6/Manual/3D_interaction/Transformations/Advanced/Shear>`_). 

Matrix Shear generates a Transform Matrix which can be used to change the locations of vertices in two directions. The amount of transformation to introduce into the Matrix is given by two `Factor` values which operate on the corresponding axes of the selected *Plane*.

Inputs & Parameters
-------------------

+-------------------+--------------------------------------------------------------------------------------------------+
| Parameters        | Description                                                                                      |
+===================+==================================================================================================+
| Plane             | ``options = (XY, XZ, YZ)``                                                                       |
+-------------------+--------------------------------------------------------------------------------------------------+
| Factor1 & Factor2 | these are *Scalar float* values and indicate how much to affect the axes of the transform matrix |
+-------------------+--------------------------------------------------------------------------------------------------+

Outputs
-------

A single ``4*4`` Transform Matrix


Examples
--------

Usage: This is most commonly connected to Matrix Apply to produce the Shear effect.

.. image:: https://cloud.githubusercontent.com/assets/619340/4186364/3298a5f8-3760-11e4-83ad-e26989cb5133.PNG
  :alt: Matrix_Shear_Demo1.PNG