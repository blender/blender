Bevel
=====

*destination after Beta: Modifier Change*

Functionality
-------------

This node applies Bevel operator to the input mesh. You can specify edges to be beveled.

Inputs
------

This node has the following inputs:

- **Vertices**
- **Edges**
- **Polygons**
- **BevelEdges**. Edges to be beveled. If this input is not connected, then by default all edges will be beveled. This parameter is used only when ``Vertex only`` flag is not checked.
- **Amount**. Amount to offset beveled edge.
- **Segments**. Number of segments in bevel.
- **Profile**. Profile shape.

Parameters
----------

This node has the following parameters:

+------------------+---------------+-------------+----------------------------------------------------+
| Parameter        | Type          | Default     | Description                                        |
+==================+===============+=============+====================================================+
| **Amount type**  | Offset or     | Offset      | * Offset - Amount is offset of new edges from      |
|                  |               |             |   original.                                        |
|                  | Width or      |             | * Width - Amount is width of new face.             |
|                  | Depth or      |             | * Depth - Amount is perpendicular distance from    |
|                  |               |             |   original edge to bevel face.                     |
|                  | Percent       |             | * Percent - Amount is percent of adjacent edge     |
|                  |               |             |   length.                                          |
+------------------+---------------+-------------+----------------------------------------------------+
| **Vertex only**  | Bool          | False       | Only bevel edges, not faces.                       |
+------------------+---------------+-------------+----------------------------------------------------+
| **Amount**       | Float         | 0.0         | Amount to offset beveled edge. Exact               |
|                  |               |             | interpretation of this parameter depends on        |
|                  |               |             | ``Amount type`` parameter. Default value of zero   |
|                  |               |             | means do not bevel. This parameter can also be     |
|                  |               |             | specified via corresponding input.                 |
+------------------+---------------+-------------+----------------------------------------------------+
| **Segments**     | Int           | 1           | Number of segments in bevel. This parameter can    |
|                  |               |             | also be specified via corresponding input.         |
+------------------+---------------+-------------+----------------------------------------------------+
| **Profile**      | Float         | 0.5         | Profile shape - a float nubmer from 0 to 1;        |
|                  |               |             | default value of 0.5 means round shape.  This      |
|                  |               |             | parameter can also be specified via corresponding  |
|                  |               |             | input.                                             |
+------------------+---------------+-------------+----------------------------------------------------+

Outputs
-------

This node has the following outputs:

- **Vertices**
- **Edges**
- **Polygons**
- **NewPolys** - only bevel faces.

Examples of usage
-----------------

Beveled cube:

.. image:: https://cloud.githubusercontent.com/assets/284644/5888719/add3aebe-a42c-11e4-8da5-3321f93e1ff0.png

Only two edges of cube beveled:

.. image:: https://cloud.githubusercontent.com/assets/284644/5888718/adc718b6-a42c-11e4-80d6-7793e682f8e4.png

Another sort of cage:

.. image:: https://cloud.githubusercontent.com/assets/284644/5888727/dc332794-a42c-11e4-9007-d86610405164.png

You can work with multiple objects:

.. image:: https://cloud.githubusercontent.com/assets/5783432/18603141/322847ce-7c80-11e6-8357-6ef4673add4d.png