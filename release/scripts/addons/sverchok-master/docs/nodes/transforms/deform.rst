Simple Deformation
==================

Functionality
-------------

This node transforms vertices by one of deformations, similar to Blender's "Simple Deform" modifier.

Inputs
------

This node has the following inputs:

- **Vertices**
- **Origin**. This matrix defines origin and coordinate axis of deformation. Default value is identity matrix.
- **Angle**. Deformation angle. Available in **Twist**, **Bend** modes. 
- **Factor**. Deformation factor. Available in **Taper** mode.
- **Low limit**. Percentage value. Vertices below this limit will use the same transformation as vertices on the boundary.
- **High limit**. Percentage value. Vertices above this limit will use the same transformation as vertices on the boundary.

Parameters
----------

This node has the following parameters:

- **Mode**. Deformation mode. Supported modes are:

  - **Twist**
  - **Bend**
  - **Taper**

  These modes are similar to their namesakes in Blender's "Simple Deform" modifier.
- **Angle mode**. Defines which units are used for **Angle** input. Available values are **Radian** and **Degree**. Default is **Radian**. Available only in **Twist**, **Bend** modes.
- **Lock X**, **Lock Y**. If checked, then corresponding coordinates of vertices will not be changed. Note that this lock is applied to coordinates relative to **Origin**.

Outputs
-------

This node has one output: **Vertices**.

Examples of usage
-----------------

Bend deformation:

.. image:: https://cloud.githubusercontent.com/assets/284644/24372062/9cb51d1e-134e-11e7-8c23-bc7d12768606.png

Twist deformation:

.. image:: https://cloud.githubusercontent.com/assets/284644/24372066/9ce9b2d6-134e-11e7-9652-4ce697498bfd.png

Taper deformation:

.. image:: https://cloud.githubusercontent.com/assets/284644/24372065/9ce6e452-134e-11e7-9313-eba01bbc3542.png

