Metaball Out Node
=================

Functionality
-------------

This node generates Blender's Meta objects (aka metaballs) from input data. It
creates a new Meta object or updates existing on each update of input data or
parameters.

Please refer to Blender's documentation and tutorials for more information
about what is metaballs and how do they work.

Inputs
------

This node has the following inputs:

- **Types**. Type of meta elements to create. If input is not connected, the
  value is selected from dropdown list. Values available are: Ball (1), Capsule (2),
  Plane (3), Ellipsoid (4), Cube (5). Default is Ball. When the input is used,
  it accepts integer numbers from 1 to 5, which corresponds to the meta types
  in the same order.
- **Origins**. This describes location, scale and rotation of metaball
  elements. Note that for different Meta element types interpretation of
  Rotation and Scale from input matrix differs. Please refer to the table
  below. This input can also accept vectors, in this case they are treated as
  location components of matrices.
- **Radius**. Radiuses of metaballs. Exact interpretation also depends on meta
  element type. This can be specified as input or as a parameter.
- **Stiffness**. Stiffness defines how much of the element to fill.  This can
  be specified as input or as a parameter.
- **Negation**. This input accepts a mask. Meta elements with Negation = true
  will be considered negative. If this input is not connected, all meta
  elements are considered positive.

Interpretation of Origin's Scale and Rotation
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

All types of meta object elements understand location component of Origin
matrix in usual way. However, interpretation of scale and rotation components
differ:

+---------------+----------------------+----------------------------------+
| Element type  | Rotation             | Scale                            |
+===============+======================+==================================+
| **Ball**      | Not used (ignored)   | Not used (ignored)               |
+---------------+----------------------+----------------------------------+
| **Capsule**   | Used                 | Only X component is used         |
+---------------+----------------------+----------------------------------+
| **Plane**     | Used                 | Only X and Y components are used |
+---------------+----------------------+----------------------------------+
| **Ellipsoid** | Used                 | All components are used          |
+---------------+----------------------+----------------------------------+
| **Cube**      | Used                 | All components are used          |
+---------------+----------------------+----------------------------------+

Parameters
----------

This node has the following parameters:

- **UPD**. The node will process data only if this button is enabled.
- **Hide View** (eye icon). Toggle visibility of generated object in viewport.
- **Hide Select** (pointer icon). Toggle ability to select for generated object.
- **Hide Render** (render icon). Toggle renderability for generated object.
- **Base name**. Base part of name for Meta object to create (or update).
  Default is "Meta_Alpha".
- **Select Toggle**. Select / deselect generated object.
- **Material**. Material to be assigned to created object.
- **Threshold**. Influence of meta elements. Default is 0.6.
- **Resolution (viewport)**. Resolution of Meta object for viewport. Lesser
  value mean better resolution. Default is 0.2. This parameter can be set only
  in the N panel.
- **Resolution (render)**. Resolution of Meta object for rendering. Lesser
  value mean better resolution. Default is 0.1. This parameter can be set only
  in the N panel.

Outputs
-------

The main function of this node is that it creates or updates Meta objects in
Blender's scene. Also, it has one output, named **Objects**, which contains a
reference to created metaball object.

Example of usage
----------------

Simple example:

.. image:: https://user-images.githubusercontent.com/284644/32993860-54cdf1be-cd80-11e7-99fc-63d8f773ba70.png

