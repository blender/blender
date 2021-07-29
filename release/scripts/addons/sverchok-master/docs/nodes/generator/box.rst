Box
===

Functionality
-------------

Offers a Box primitive with variable X,Y and Z divisions, and overal Size.

Inputs
------

All inputs are expected to be scalar values. Divisions are given in *Integers* only, it will cast incoming `floats` to `int`.

- Size
- Div X
- Div Y
- Div Z

Parameters
----------

*None*

Outputs
-------

- Verts
- Edges
- Faces

Examples
--------

*None* other than variables given by *Inputs*.

Notes
-----

This is not a very fast implementation of Box, but it can act as an introduction to anyone interested in coding their own Sverchok Nodes. It is a very short node code-wise, it has a nice structure and shows how one migh construct a bmesh and use existing bmesh.ops to operate on it.

