Plane MK2
==========

Functionality
-------------

Plane generator creates a grid in the plane XY/YZ or ZX, based on the number of vertices and the length between them in X and Y directions. It works in a similar way than Line, but creating a grid instead of a line.

Inputs
------

Just like in Line Node, all inputs are vectorized and they will accept single or multiple values.
There is two basic inputs **N Verts** and **Step**, but referenced to both X and Y directions, so it results in 4 inputs:

- **N Verts X**
- **N Verts Y**
- **Step X**
- **Step Y**

Same as Line, all inputs will accept a single number or an array of them or even an array of arrays::

    [2]
    [2, 4, 6]
    [[2], [4]]

Parameters
----------

All parameters except **Separate**, **Direction**, **Center**, **Normalize**, **Size X** and **Size Y** can be given by the node or an external input.

+---------------+------------+-----------+----------------------------------------------------+
| Param         | Type       | Default   | Description                                        |
+===============+============+===========+====================================================+
| **N Verts X** | Int        | 2         | number of vertices in X. The minimum is 2.         |
+---------------+------------+-----------+----------------------------------------------------+
| **N Verts Y** | Int        | 2         | number of vertices in X. The minimum is 2.         |
+---------------+------------+-----------+----------------------------------------------------+
| **Step X**    | Float      | 1.00      | length between vertices in X axis                  |
+---------------+------------+-----------+----------------------------------------------------+
| **Step Y**    | Float      | 1.00      | length between vertices in Y axis                  |
+---------------+------------+-----------+----------------------------------------------------+
| **Separate**  | Boolean    | False     | grouping vertices by V direction                   |
+---------------+------------+-----------+----------------------------------------------------+
| **Direction** | Enum       | XY        | generate grid in XY, YZ or ZX plane                |
+---------------+------------+-----------+----------------------------------------------------+
| **Center**    | Boolean    | False     | center the plane around origin                     |
+---------------+------------+-----------+----------------------------------------------------+
| **Normalize** | Boolean    | False     | normalize the plane sizes to specific values       |
+---------------+------------+-----------+----------------------------------------------------+
| **Size X**    | Float      | 10.00     | normalized plane size along X direction [1]        |
+---------------+------------+-----------+----------------------------------------------------+
| **Size Y**    | Float      | 10.00     | normalized plane size along Y direction [1]        |
+---------------+------------+-----------+----------------------------------------------------+

Notes:
[1] - the **Size X** / **Size Y** parameters are only available when the **Normalize** is on.

Outputs
-------

**Vertices**, **Edges** and **Polygons**.
All outputs will be generated. Depending on the type of the inputs, the node will generate only one or multiples independant grids.

If **Separate** is True, the output is totally different. The grid disappear (no more **polygons** are generated) and instead it generates a series of lines repeated along Y axis. See examples below to a better understanding.

Example of usage
----------------
