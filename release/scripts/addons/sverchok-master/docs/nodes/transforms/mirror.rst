Mirror
======

Functionality
-------------

This node is used to make general mirroring over geometry. It works directly over vertices, not with matrixes. It offers 3 different types of mirror:

+=======================+=============================================================+
|Type of Mirror         |Description                                                  |
+=======================+=============================================================+
|Vertex Mirror          | Based on one single point                                   |
|Axis Mirror            | Mirror around an axis defined by two points                 |
|Plane Mirror           | Mirror over a plane given by a matrix                       |
+=======================+=============================================================+


Vertex Mirror
-------------

This mode let us define a center point and a make mirror of the incoming Vertices

Inputs
^^^^^^

All inputs are vectorized and they will accept single or multiple values.
There is two inputs:

- **Vertices**
- **Vert A**


Parameters
^^^^^^^^^^

Defult value for **Vert A** is equal to ``(0.0, 0.0, 0.0)``. Vertices need an input.

+----------------+---------------+-----------------+----------------------------------------------------+
| Param          | Type          | Default         | Description                                        |  
+================+===============+=================+====================================================+
| **Vertices**   | Vertices      | none            | vertices to mirror                                 | 
+----------------+---------------+-----------------+----------------------------------------------------+
| **Vert A**     | Vertices      | (0.0, 0.0, 0.0) | center of the mirroring                            |
+----------------+---------------+-----------------+----------------------------------------------------+


Outputs
^^^^^^^

Only **Vertices** will be generated. Depending on the type of the inputs, if they have more than one object, then more objects will be outputted.

Example of usage
^^^^^^^^^^^^^^^^

.. image:: https://cloud.githubusercontent.com/assets/5990821/4220321/a14a9c58-3900-11e4-8f98-a30dbe7a8b34.png
  :alt: VertexMirrorDemo1.PNG

In this example we use Vertex mirror to mirroring an arbitrary shape. As you can see every point goes through the center point.


Axis Mirror
-----------

This mode is used to make mirroring around an axis defined by two vertices.

Inputs
^^^^^^

All inputs are vectorized and they will accept single or multiple values.
There is three inputs:

- **Vertices**
- **Vert A**
- **Vert B**

Parameters
^^^^^^^^^^

There is default values for **Vert A** and **vert B**. **Vertices** need an input.

+----------------+---------------+-----------------+----------------------------------------------------+
| Param          | Type          | Default         | Description                                        |  
+================+===============+=================+====================================================+
| **Vertices**   | Vertices      | none            | vertices to mirror                                 | 
+----------------+---------------+-----------------+----------------------------------------------------+
| **Vert A**     | Vertices      | (0.0, 0.0, 0.0) | first point to define the axis                     |
+----------------+---------------+-----------------+----------------------------------------------------+
| **Vert B**     | Vertices      | (1.0, 0.0, 0.0) | second point to define the axis                    |
+----------------+---------------+-----------------+----------------------------------------------------+

Outputs
^^^^^^^

Only **Vertices** will be generated. Depending on the type of the inputs, if one or more inputs have multiple values, then more objects will be mirrored.

Example of usage
^^^^^^^^^^^^^^^^

.. image:: https://cloud.githubusercontent.com/assets/5990821/4220319/a1340b8c-3900-11e4-93f6-d78e458c77d4.png
  :alt: AxisMirrorDemo1.PNG

We define an axis with two vertices and use them to make a mirror. All the points reflect through the chosen axis.


Plane Mirror
------------

This is the most common method of mirroring. We'll use a plane defined by a matrix.

Inputs
^^^^^^

All inputs are vectorized and they will accept single or multiple values.
There is two inputs:

- **Vertices**
- **Plane**

Parameters
^^^^^^^^^^

Plane has a defult value, but Vertices need an input.

+----------------+---------------+-----------------+----------------------------------------------------+
| Param          | Type          | Default         | Description                                        |  
+================+===============+=================+====================================================+
| **Vertices**   | Vertices      | none            | vertices to mirror                                 | 
+----------------+---------------+-----------------+----------------------------------------------------+
| **Plane**      | Matrix        | Identity        | matrix to define the mirror plane                  |
+----------------+---------------+-----------------+----------------------------------------------------+

Outputs
^^^^^^^

Only **Vertices** will be generated. Depending on the type of the inputs, if one or more planes are defined, then more objects will be mirrored.

Example of usage
^^^^^^^^^^^^^^^^

.. image:: https://cloud.githubusercontent.com/assets/5990821/4220320/a13edcd8-3900-11e4-9ae6-088583f7560c.png
  :alt: PlaneMirrorDemo1.PNG

In this last case we just mirror the shape over the selected plane, defined by a matrix.
