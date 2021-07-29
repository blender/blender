Rotation
========

Functionality
-------------

This node is used to make general rotation over geometry. It works directly over vertices, not with matrixes. Just like Blender, it offers 3 different types of rotation:

=============  =================================================
Axis Rotation   Based on axis (X, Y, Z) and a rotation angle (W)
=============  =================================================

======================= ===========================================================================
Type of Rotation         Description
======================= ===========================================================================
Axis Rotation            Based on axis (X, Y, Z) and a rotation angle (W)
Euler Rotation           Using Euler Gimbal: 3 axis with a hierarchical relationship between them
Quaternion rotation      Based on four values (X, Y, Z, W). W value will avoid X, Y, Z rotation 
======================= ===========================================================================

If you want to learn deeply about all this types of rotation, visit this link: http://wiki.blender.org/index.php/User:Pepribal/Ref/Appendices/Rotation


Axis Rotation
-------------

This mode let us define an axis (X, y, Z), a center point and a rotation angle (W), in degrees, around the defined axis.

Inputs
^^^^^^

All inputs are vectorized and they will accept single or multiple values.
There is four inputs:

- **Vertices**
- **Center**
- **Axis**
- **Angle**

Parameters
^^^^^^^^^^

All parameters except **Vertices** has a default value. **Angle** can be given by the node or an external input.


+----------------+---------------+-----------------+----------------------------------------------------+
| Param          | Type          | Default         | Description                                        |  
+================+===============+=================+====================================================+
| **Vertices**   | Vertices      | none            | vertices to rotate                                 | 
+----------------+---------------+-----------------+----------------------------------------------------+
| **Center**     | Vertices      | (0.0, 0.0, 0.0) | point to place the rotation axis                   |
+----------------+---------------+-----------------+----------------------------------------------------+
| **Axis**       | Vector        | (0.0, 0.0, 1.0) | axis around which rotation will be done            |
+----------------+---------------+-----------------+----------------------------------------------------+
| **Angle**      | Float         | 0.00            | angle in degrees to make rotation                  |
+----------------+---------------+-----------------+----------------------------------------------------+

Outputs
^^^^^^^

Only **Vertices** will be generated. Depending on the type of the inputs, if more than one angle is set, then more objects will be outputted.

Example of usage
^^^^^^^^^^^^^^^^

.. image:: https://cloud.githubusercontent.com/assets/5990821/4216976/adb4043a-38e1-11e4-8ff1-0ae83cbd9ccf.png
  :alt: AxisRotationDemo1.PNG

In this example we use axis rotation with multiple inputs in axis an angle to create a complex geometry from just one plane.


Euler Rotation
--------------

This mode is used to perform Euler rotations, refered to an Eular gimbal. A gimbal is a set of 3 axis that have a hierarchical relationship between them.

Inputs
^^^^^^

All inputs are vectorized and they will accept single or multiple values.
There is four inputs:

- **Vertices**
- **X**
- **Y**
- **Z**

Parameters
^^^^^^^^^^

All parameters except **Vertices** has a default value. **X**, **Y** and **Z** can be given by the node or an external input.


+----------------+---------------+-----------------+-----------------------------------------------------+
| Param          | Type          | Default         | Description                                         |  
+================+===============+=================+=====================================================+
| **Vertices**   | Vertices      | none            | vertices to rotate                                  | 
+----------------+---------------+-----------------+-----------------------------------------------------+
| **X**          | Float         | 0.00            | value to X axis rotation                            |
+----------------+---------------+-----------------+-----------------------------------------------------+
| **Y**          | Float         | 0.00            | value to Y axis rotation                            |
+----------------+---------------+-----------------+-----------------------------------------------------+
| **Z**          | Float         | 0.00            | value to Z axis rotation                            |
+----------------+---------------+-----------------+-----------------------------------------------------+
| **Order**      | Enum          | XYZ             | order of the hierarchical relationship between axis |
+----------------+---------------+-----------------+-----------------------------------------------------+

Outputs
^^^^^^^

Only **Vertices** will be generated. Depending on the type of the inputs, if one or more inputs have multiple values, then more objects will be outputted.

Example of usage
^^^^^^^^^^^^^^^^

.. image:: https://cloud.githubusercontent.com/assets/5990821/4216977/adb5f682-38e1-11e4-88f9-decc6485b81f.png
  :alt: EulerRotationDemo1.PNG
.. image:: https://cloud.githubusercontent.com/assets/5990821/4216975/adb3a990-38e1-11e4-8e3b-1584f37573e3.png
  :alt: EulerRotationDemo2.PNG

In the first example we use Euler rotation to perfomr a simple operation, we just rotate a plane around Z axis multiple times.
The second is more complex, with multiple inputs in Y and Z to create a complex geometry from just one plane, simulating infinite loop.


Quaternion Rotation
-------------------

In this mode rotation is defined by 4 velues (X, Y, Z, W), but it works in a different way than Axis Rotation. The important thing es the relation between all four values. For example, X value rotate the object around X axis up to 180 degrees. The effect of W is to avoid that rotation and leave the element with zero rotation.
The final rotation is a combination of all four values.

Inputs
^^^^^^

All inputs are vectorized and they will accept single or multiple values.
There is five inputs:

- **Vertices**
- **X**
- **Y**
- **Z**
- **W**

Parameters
^^^^^^^^^^

All parameters except **Vertices** has a default value. **X**, **Y**, **Z** and **W** can be given by the node or an external input.


+----------------+---------------+-----------------+-----------------------------------------------------+
| Param          | Type          | Default         | Description                                         |  
+================+===============+=================+=====================================================+
| **Vertices**   | Vertices      | none            | vertices to rotate                                  | 
+----------------+---------------+-----------------+-----------------------------------------------------+
| **X**          | Float         | 0.00            | value to X axis rotation                            |
+----------------+---------------+-----------------+-----------------------------------------------------+
| **Y**          | Float         | 0.00            | value to Y axis rotation                            |
+----------------+---------------+-----------------+-----------------------------------------------------+
| **Z**          | Float         | 0.00            | value to Z axis rotation                            |
+----------------+---------------+-----------------+-----------------------------------------------------+
| **W**          | Float         | 1.00            | value to Z axis rotation                            |
+----------------+---------------+-----------------+-----------------------------------------------------+

Outputs
^^^^^^^

Only **Vertices** will be generated. Depending on the type of the inputs, if one or more inputs have multiple values, then more objects will be outputted.

Example of usage
^^^^^^^^^^^^^^^^

.. image:: https://cloud.githubusercontent.com/assets/5990821/4216974/adab7018-38e1-11e4-9c43-78a2fdff2fe1.png
  :alt: QuatRotationDemo1.PNG

As we can see in this example, we try to rotate the plan 45 degrees and then set W with multiple values, each higher than before, but the plane is never get to rotate 180 degrees.