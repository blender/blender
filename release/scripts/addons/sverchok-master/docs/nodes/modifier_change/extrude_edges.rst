Extrude edges
=============

*destination after Beta: Modifier Change*

Functionality
-------------

You can extrude edges along matrices. Every matrix influence on separate vertex of initial mesh.

Inputs
------

This node has the following inputs:

- **Vertices**
- **Edgs/Pols**
- **Matrices**

Parameters
----------

Nope

Outputs
-------

This node has the following outputs:

- **Vertices**
- **Edges**
- **Polygons**
- **NewVertices** - only new vertices
- **NewEdges** - only new edges
- **NewPolys** - only new faces.

Examples of usage
-----------------

Extruded circle in Z direction by sinus, drived by pi*N:

.. image:: https://cloud.githubusercontent.com/assets/5783432/18603880/8d81f272-7c86-11e6-8514-e241557730b0.png

Extruded circle in XY directions by sinus and cosinus drived by pi*N:

.. image:: https://cloud.githubusercontent.com/assets/5783432/18603878/8d80896e-7c86-11e6-8f4a-8d7024ae597b.png

Matrix input node can make skew in one or another direction:

.. image:: https://cloud.githubusercontent.com/assets/5783432/18603891/9fbcc44e-7c86-11e6-8f43-e48ef1eacd59.png

Matrix input node can also scale extruded edges, so you will get bell:

.. image:: https://cloud.githubusercontent.com/assets/5783432/18603881/8d81e9e4-7c86-11e6-9a77-a9104bd234cc.png