Matrix Tube
============

*destination after Beta: Modifier Make*

Functionality
-------------

Makes a tube or pipe from a list of matrices. This node takes a list of matrices and a list of vertices as input. The vertices are joined together to form a ring. This ring is transformed by each matrix to form a new ring. Each ring is joined to the previous ring to form a tube. 

Inputs
------

**Matrices** - List of transform matrices.

**Vertices** - Vertices of ring. Usually from a "Circle" or "NGon" node   
 
Outputs
-------

- **Vertices, Edges and Faces** - These outputs will define the mesh of the tube that skins the input matrices. 

Example of usage
------------------

.. image:: https://cloud.githubusercontent.com/assets/7930130/7645410/e7ce60f4-fb08-11e4-827e-f856e1874fec.png
  :alt: matrix tube example

