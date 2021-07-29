Smooth Vertices
===============

Functionality
-------------

This node applies Blender's Smooth or Laplacian Smooth operation to the input mesh.

Inputs
------

This node has the following inputs:

- **Vertices**
- **Edges**
- **Faces**
- **VertMask**. Selected vertices to be smoothed.
- **Iterations**
- **Clip threshold**
- **Factor**
- **Border factor**

Parameters
----------

This node has the following parameters:

- **X**, **Y**, **Z**. Toggle axes vertices will be smoothed along. By default mesh is smoothed along all axes.
- **Laplacian Smooth**. Toggles smoothing algorithm: when checked, Laplacian smoothing is used; otherwise, simple averaging scheme will be used. By default not checked.
- **Clip X**, **Clip Y**, **Clip Z**. Toggle axes along which "Mirror Clipping" procedure will be applied. This procedure merges vertices that have X/Y/Z coordinate near zero, withing specified threshold. For example, it can merge vertices `(0.01, 3, 5)` and `(- 0.01, 3, 5)` into one vertex `(0, 3, 5)`. These parameters are available only when **Laplacian Smooth** is off. Not checked by default.
- **Preserve volume**. If checked, the mesh will be "blown" a bit after smoothing, to preserve its volume. Available only when **Laplacian Smooth** is on. Checked by default.
- **Iterations**. Number of smoothing operation iterations. Default value is 1. This parameter can also be provided as input.
- **Clip threshold**. Threshold for "Mirror Clipping" procedure. Available only when **Laplacian Smooth** is off. This parameter can also be provided as input.
- **Factor**. Smoothing factor. Zero means no smoothing. For simple mode, 1.0 is maximum sensible value, bigger values will create degenerated forms in most cases. For Laplacian mode, sensible values can be much bigger. This parameter can also be provided as input.
- **Border factor**. Smoothing factor for border. This parameter is only available when **Laplacian Smooth** is on. This parameter can also be provided as input.

Outputs
-------

This node has the following outputs:

- **Vertices**. All vertices of resulting mesh.
- **Edges**. All edges of resulting mesh.
- **Faces**. All faces of resulting mesh.

Examples of usage
-----------------

Laplacian smooth applied to the cube, along X and Y axes only:

.. image:: https://cloud.githubusercontent.com/assets/284644/25193047/6cf54980-2557-11e7-858c-a34dfb487de2.png

Smoothing applied only to selected vertices:

.. image:: https://cloud.githubusercontent.com/assets/284644/25193048/6d21ab4c-2557-11e7-8caa-d92cbc17a824.png

Suzanne smoothed:

.. image:: https://cloud.githubusercontent.com/assets/284644/25193049/6d22489a-2557-11e7-8818-4394eb39bf55.png

