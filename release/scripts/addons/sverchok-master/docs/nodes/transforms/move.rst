Vector Move
===========

Functionality
-------------

**equivalent to a Translate Transform**

Moves incoming sets of Vertex Lists by a *Vector*. The Vector is bound to a multiplier (Scalar) which amplifies all components of the Vector. The resulting Vector is added to the locations of the incoming Vertices. 

You might use this to translate the center of an object away or towards from [0,0,0] in order to apply other transforms like Rotation and Scale.


Inputs & Parameters
-------------------

+------------+-------------------------------------------------------------------------------------+
|            | Description                                                                         |
+============+=====================================================================================+
| Vertices   | Vertex or Vertex Lists representing one or more objects                             | 
+------------+-------------------------------------------------------------------------------------+
| Vector     | Vector to use for Translation, this is simple element wise addition to the Vector   | 
|            | representations of the incoming vertices. If the input is Nested, it is possible    |
|            | to translate each sub-list by a different Vector.                                   |
+------------+-------------------------------------------------------------------------------------+
| Multiplier | Straightforward ``Vector * Scalar``, amplifies each element in the Vector parameter |
+------------+-------------------------------------------------------------------------------------+


Outputs
-------

A Vertex or nested Lists of Vertices


Examples
--------

This works for one vertice or many vertices

.. image:: https://cloud.githubusercontent.com/assets/619340/4185766/ce3d6c1a-375a-11e4-86ea-6525a3e34dc3.PNG
   :alt: VectorMoveDemo1.PNG

*translate back to origin*

.. image:: https://cloud.githubusercontent.com/assets/619340/4185765/ce3c512c-375a-11e4-9986-dc4a96777f0e.PNG
   :alt: VectorMoveDemo2.PNG

Move lists of matching nestedness. (whats that?! - elaborate)

.. image:: https://cloud.githubusercontent.com/assets/619340/4185767/ce42339e-375a-11e4-926f-376f69b663bf.PNG
   :alt: VectorMoveDemo3.PNG

.. image:: https://cloud.githubusercontent.com/assets/619340/4185768/ce485684-375a-11e4-88dc-c35f1b2ce725.PNG
   :alt: VectorMoveDemo4.PNG

Notes
-------