Mask Converter
==============

Functionality
-------------

This node allows to convert masks that are to be applied to different types of mesh elements. For example, it can convert mask on vertices to mask for faces, or mask for edges to mask for vertices, and so on.

Type of mask which is provided at input is defined by **From** parameter. Masks of all other types are available as outputs.

Inputs
------

This node has the following inputs:

- **Vertices**. This input is available and required only when parameter **From** is set to **Edges** or **Faces**.
- **VerticesMask**. Mask for vertices. This input is available only when parameter **From** is set to **Vertices**.
- **EdgesMask**. Mask for edges. This input is available only when parameter **From** is set to **Edges**.
- **FacesMask**. Mask for faces. This input is available only when parameter **From** is set to **Faces**.

Parameters
----------

This node has the following parameters:

- **From**. This parameter determines what type of mask you have as input. The following values are supported:

  - **Vertices**. Convert mask for vertices to masks for edges and faces.
  - **Edges**. Convert mask for edges to masks for vertices and faces.
  - **Faces**. Convert mask for faces to masks for vertices and edges.
- **Include partial selection**. If checked, then partially selected elements will be accounted as selected.

  - Vertex can be never partially selected, it is either selected or not.
  - Edge is partially selected if it has only one of its vertices selected.
  - Face is partially selected if only some of its vertices or faces are selected.

Outputs
-------

This node has the following outputs:

- **VerticesMask**. Mask for vertices. This output is not available if parameter **From** is set to **Vertices**.
- **EdgesMask**. Mask for edges. This output is not available if parameter **From** is set to **Edges**.
- **FacesMask**. Mask for faces. This output is not available if parameter **From** is set to **Faces**.

Examples of usage
-----------------

Select face of cube by selecting its vertices, and extrude it:

.. image:: https://cloud.githubusercontent.com/assets/284644/25284911/57c33020-26da-11e7-93c8-c1148dbd8efc.png

Select faces of sphere with small area, and move corresponding vertices:

.. image:: https://cloud.githubusercontent.com/assets/284644/25284914/5843a476-26da-11e7-908a-5eb9ed694ccb.png

