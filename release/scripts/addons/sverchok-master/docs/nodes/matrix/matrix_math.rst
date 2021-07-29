Matrix Math
===========

Functionality
-------------

Matrix Math node allows for various matrix operations to be performed on the input matrices, such as: Multiply, Invert, Filter, Basis.

Inputs
------

All inputs are vectorized and they will accept single or multiple values.

- **A**
- **B**  [1]
...
- **Z**  [2]

Notes:
[1] : The second input is only available for the operations that require a second operand, like Multiply.
[2] : For multiple input operation (e.g. Multiply) the input sockets extend to allow arbitrary number of input matrices and their socket name progress alphabetically.

Parameters
----------

The **Operation** parameter allows to select one of following operations: Multiply, Invert, Filter, Basis.

All parameters except **Operation**, **PrePost** and **Filter T/R/S** can be given as an external input.

+---------------+------------+----------+--------------------------------------------------+
| Param         | Type       | Default  | Description                                      |
+===============+============+==========+==================================================+
| **Operation** | Enum:      | Multiply | Multiply: A,B => C = A*B  [1]                    |
|               |  Multiply  |          |   Invert: A => C = A^-1                          |
|               |  Invert    |          |   Filter: A => C = [AT]*[AR]*[AS]                |
|               |  Filter    |          |   Basis:  A => X, Y, Z                           |
|               |  Basis     |          |                                                  |
+---------------+------------+----------+--------------------------------------------------+
| **PrePost**   | Enum:      | Pre      | Determines the order the operands  [2]           |
|               |  Pre       |          |                                                  |
|               |  Post      |          |                                                  |
+---------------+------------+----------+--------------------------------------------------+
| **Filter T**  | Bool       | False    | Filter out the Translation component  [3]        |
+---------------+------------+----------+--------------------------------------------------+
| **Filter R**  | Bool       | False    | Filter out the Rotation component  [3]           |
+---------------+------------+----------+--------------------------------------------------+
| **Filter S**  | Bool       | False    | Filter out the Scale component  [3]              |
+---------------+------------+----------+--------------------------------------------------+
| **A**         | Matrix     | identity | First matrix input                               |
+---------------+------------+----------+--------------------------------------------------+
| **B**         | Matrix     | identity | Second matrix input  [4]                         |
+---------------+------------+----------+--------------------------------------------------+

Notes:
 [1] : The order of multiplication is given by the PrePost setting.
 [2] : The PrePost setting is only available for the Multiply operation.
 [3] : The Filter T/R/S toggle settings are only available for the Filter operation.
 [4] : Second input is only available for Multiply operation.

Operations
----------
**Multiply**
The multiplication of the 4x4 homogeneous matrices result in a composite 4x4 homogeneous matrix having a composed Translation, a composed Rotation and a composed Scale. The order of multiplication is given by the PrePost setting, which is set to PRE-multiplication by default (C = A*B). The POST multiplication reverses the order of multiplication (C = B*A).

Note: When using a PRE multiply composition A*B to transform a mesh, essentially the overall operation is equivalent to applying the matrix B to the mesh first, then matrix A.

For this operation the node allows for an arbitrary number of input matrices to be multiplied together: A * B * .. * Z (PRE or POST).

**Filter**
A 4x4 homogeneous matrix is composed by a translation (T), rotation (R) and a scale (S) matrix and is defined as: T*R*S. The filter operation allows you to set the individual components T, R and/or S to the identity matrix so that they are filtered out of the output composite matrix.

Note: filtering out all components will result in an identity matrix output.

**Invert**
The inversion of a 4x4 homogeneous matrix A is a 4x4 homogenous matrix A' for which A * A' is the identity matrix.

**Basis**
This operation extacts the basis vector from the rotation part of the 4x4 homogenous matrix. Essentially these are the vectors the XYZ ortho-normal axes of a coordinate system would be if transformed by the 4x4 homogeneous matrix.

Outputs
-------

**Matrix**
Outputs will be generated when connected.

**X**
**Y**
**Z**
These ouputs are available only for the Basis operation and will be generated when connected.

Example of usage
----------------

