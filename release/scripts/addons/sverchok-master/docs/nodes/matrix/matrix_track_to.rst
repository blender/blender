Matrix Track To
===============

Functionality
-------------

Generates the transformation **matrix** given a location, a scale and a rotation defined by the given set of Track & Up vectors.

The "Track/Up Axes" and the "Track/Up Mapping" settings allow to set any two of the X, Y, Z axes to be the **Track** axis and the **Up** axis and to map them to any of the two input vectors A and B (or to their negatives). The first axis listed in these dropdown lists is the Track axis/vector and the second is the Up axis/vector.

The axis selected to be the Track axis (the first letter in "Track/Up Axes" selection) will be precisely oriented along the Track vector (whichever the Track vector is mapped to, based on the "Track/Up mapping"), while the axis selected to be the Up axis (the second letter the "Track/Up Axes" selection) will be approximately oriented along the Up vector (whichever the Up vector is mapped to, based on the "Track/Up mapping" selection). If the A and B inputs happen to be orthogonal to eachother then the Track and Up axes will be precisely oriented along the selected Track/Up vectors, otherwise the Up axis will only approximately be oriented towards the Up vector, perpendicular to the Track axis, and reside within the AB plane. The third axis (X, Y or Z) is determined via cross product from the other two as to provide an ortho-normal, right handed, XYZ coordinate sytem.

Inputs
------

All inputs are vectorized and they will accept single or multiple values.

- **Location**
- **Scale**
- **A**
- **B**

Parameters
----------

- **Track/Up Axes**
- **Track/Up Mapping**

+----------------------+---------+-----------+------------------------------------------------+
| Param                | Type    | Default   | Description                                    |
+======================+=========+===========+================================================+
| **Track/Up Axes**    | Enum:   | X Y       | Selects which of the X,Y,Z axes are the Track  |
|                      |  X Y    |           | axis and the Up axis.                          |
|                      |  X Z    |           |                                                |
|                      |  Y X    |           |                                                |
|                      |  Y Z    |           |                                                |
|                      |  Z X    |           |                                                |
|                      |  Z Y    |           |                                                |
+----------------------+---------+-----------+------------------------------------------------+
| **Track/Up Mapping** | Enum:   | A B       | Maps the Track and Up vectors to one of the    |
|                      |   A  B  |           | A, B inputs or to their negatives.             |
|                      |   A -B  |           |                                                |
|                      |  -A  B  |           |                                                |
|                      |  -A -B  |           |                                                |
|                      |   B  A  |           |                                                |
|                      |   B -A  |           |                                                |
|                      |  -B  A  |           |                                                |
|                      |  -B -A  |           |                                                |
+----------------------+---------+-----------+------------------------------------------------+
| **Location**         | Vector  | (0, 0, 0) | Location component of the output matrix.       |
+----------------------+---------+-----------+------------------------------------------------+
| **Scale**            | Vector  | (1, 1, 1) | Scale component of the output matrix.          |
+----------------------+---------+-----------+------------------------------------------------+
| **A**                | Vector  | (1, 0, 0) | First vector (to be assigned to Track or Up).  |
+----------------------+---------+-----------+------------------------------------------------+
| **B**                | Vector  | (0, 1, 0) | Second vector (to be assigned to Track or Up). |
+----------------------+---------+-----------+------------------------------------------------+

Extra Parameters
----------------
A set of extra parameters are available on the property panel. These parameters do not receive external input.

- **Normalize Vectors**

+-------------------------+------------+------------+-----------------------------------------------+
| Extra Param             |  Type      |  Default   |  Description                                  |
+=========================+============+============+===============================================+
| **Normalize Vectors**   |  Bool      |  True      |  Normalize the output X, Y, Z vectors and     |
|                         |            |            |  the rotation component of the matrix.        |
|                         |            |            |                                               |
|                         |            |            |  Turn this OFF when normalization is not      |
|                         |            |            |  needed to optimize computation.              |
+-------------------------+------------+------------+-----------------------------------------------+

Outputs
-------

**Matrix**, **X**, **Y**, **Z**.
All outputs will be generated when connected.

The **Matrix** is the homogeneous 4x4 matrix composed by the given location, rotation and scale : m = T * R * S. [1]

The **X**, **Y**, **Z** are the orthonormal vectors, oriented along the Track and Up axes. [2]

Notes:
[1] : The rotation component is ortho-normal only if the "Normalize Vectors" option is ON (in the Propety Panel).
[2] : The axes are ortho-normal only if the "Normalize Vectors" option is ON (in the Propety Panel), otherwise the vectors are only orthogonal.


Example of usage
----------------

