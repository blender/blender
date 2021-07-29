IcoSphere Node
==============

Functionality
-------------

This node creates an IcoSphere primitive. In case of zero subdivisions, it simply produces right icosahedron.

Inputs
------

This node has the following inputs:

- **Subdivisions**
- **Radius**

Parameters
----------

This node has the following parameters:
  
- **Subdivisions**. How many times to recursively subdivide the sphere. In case this parameter is 0, the node will simply produce right icosahedron. This parameter can be provided via node input.
- **Radius**. Sphere radius. This parameter can be provided via node input.

Outputs
-------

This node has the following outputs:

- **Vertices**
- **Edges**
- **Faces**

Example of usage
----------------

Simple example:

.. image:: https://cloud.githubusercontent.com/assets/284644/24826849/e7c6f03c-1c60-11e7-9e41-c7450237e315.png

