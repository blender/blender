Bricks Grid
===========

Functionality
-------------

This node generates bricks-like grid, i.e. a grid each row of which is shifted
with relation to another. It is also possible to specify toothing, so it will
be like engaged bricks.
All parameters of bricks can be randomized with separate control over
randomization of each parameter.

Optionally the grid may be cyclic in one or two directions. This can be useful
if you are going to map the grid onto some sort of cyclic or toroidal surface.


Inputs & Parameters
-------------------

All parameters except for ``Faces mode`` can be given by the node or an external input.
All inputs are vectorized and they will accept single or multiple values.

+-----------------+---------------+-------------+-------------------------------------------------------------+
| Param           | Type          | Default     | Description                                                 |
+=================+===============+=============+=============================================================+
| **Cycle U**     | Boolean       | False       | Make the grid cyclic in U direction, i.e. the direction of  |
|                 |               |             | brick rows. This makes sense only if you are going to map   |
|                 |               |             | the grid on to some sort of cyclic surface (e.g. cylinder). |
|                 |               |             | **Note**: after such mapping, you may want to use "Remove   |
|                 |               |             | doubles" node.                                              |
+-----------------+---------------+-------------+-------------------------------------------------------------+
| **Cycle V**     | Boolean       | False       | Make the grid cyclic in V direction, i.e. the direction of  |
|                 |               |             | brick columns. This makes sense only if you are going to map|
|                 |               |             | the grid on to some sort of cyclic surface (e.g. cylinder). |
|                 |               |             | **Note**: after such mapping, you may want to use "Remove   |
|                 |               |             | doubles" node.                                              |
+-----------------+---------------+-------------+-------------------------------------------------------------+
| **Faces mode**  | Flat or       | Flat        | What kind of polygons to generate:                          |
|                 |               |             |                                                             |
|                 | Stitch or     |             | * Flat - generate one polygon (n-gon, in general) for each  |
|                 |               |             |   brick.                                                    |
|                 | Center        |             | * Stitch - split each brick into several triangles, with    |
|                 |               |             |   edges going across brick.                                 |
|                 |               |             | * Center - split each brick into triangles by adding new    |
|                 |               |             |   vertex in the center of the brick. This mode is not       |
|                 |               |             |   available if *any* of **Cycle U**, **Cycle V**            |
|                 |               |             |   parameters is checked.                                    |
+-----------------+---------------+-------------+-------------------------------------------------------------+
| **Unit width**  | Float         | 2.0         | Width of one unit (brick).                                  |
+-----------------+---------------+-------------+-------------------------------------------------------------+
| **Unit height** | Float         | 1.0         | Height of one unit (brick).                                 |
+-----------------+---------------+-------------+-------------------------------------------------------------+
| **Width**       | Float         | 10.0        | Width of overall grid.                                      |
+-----------------+---------------+-------------+-------------------------------------------------------------+
| **Height**      | Float         | 10.0        | Height of overall grid.                                     |
+-----------------+---------------+-------------+-------------------------------------------------------------+
| **Toothing**    | Float         | 0.0         | Bricks toothing amount. Default value of zero means no      |
|                 |               |             | toothing.                                                   |
+-----------------+---------------+-------------+-------------------------------------------------------------+
| **Toothing      | Float         | 0.0         | Toothing randomization factor. Default value of zero means  |
| random**        |               |             | that all toothings will be equal. Maximal value of 1.0      |
|                 |               |             | means toothing will be random in range from zero to value   |
|                 |               |             | of ``Toothing`` parameter.                                  |
+-----------------+---------------+-------------+-------------------------------------------------------------+
| **Random U**    | Float         | 0.0         | Randomization amplitude along bricks rows. Default value of |
|                 |               |             | zero means all bricks will be of same width.                |
+-----------------+---------------+-------------+-------------------------------------------------------------+
| **Random V**    | Float         | 0.0         | Randomization amplitude across bricks rows. Default value   |
|                 |               |             | of zero means all grid rows will be of same height.         |
+-----------------+---------------+-------------+-------------------------------------------------------------+
| **Shift**       | Float         | 0.5         | Brick rows shift factor. Default value of 0.5 means each    |
|                 |               |             | row of bricks will be shifted by half of brick width in     |
|                 |               |             | relation to previous row. Minimum value of zero means no    |
|                 |               |             | shift.                                                      |
+-----------------+---------------+-------------+-------------------------------------------------------------+
| **Seed**        | Int           | 0           | Random seed.                                                |
+-----------------+---------------+-------------+-------------------------------------------------------------+

Outputs
-------

This node has the following outputs:

- **Vertices**
- **Edges**. Note that this output will contain only edges that are between bricks, not that splitting bricks into triangles.
- **Polygons**
- **Centers**. Centers of bricks.

Examples of usage
-----------------

Default settings:

.. image:: https://cloud.githubusercontent.com/assets/284644/5981695/2fd5badc-a8de-11e4-858e-10ce590b19ec.png

The same with Stitch faces mode:

.. image:: https://cloud.githubusercontent.com/assets/284644/5981673/06e0c5fe-a8de-11e4-822f-b1b44681982c.png

The same with Centers faces mode:

.. image:: https://cloud.githubusercontent.com/assets/284644/5981672/0693c66e-a8de-11e4-8326-a75a15e37e74.png

Using ``toothing`` parameter together with randomization, it is possible to generate something like stone wall:

.. image:: https://cloud.githubusercontent.com/assets/284644/5981643/bb85d572-a8dd-11e4-88c0-ebd018bfcdd1.png

A honeycomb structure:

.. image:: https://cloud.githubusercontent.com/assets/284644/5981635/a75e5a74-a8dd-11e4-9d1f-64e19da8f01a.png

Wooden floor:

.. image:: https://cloud.githubusercontent.com/assets/284644/5981642/bb7ceda4-a8dd-11e4-9319-c9728199d44e.png

You can also find some more examples `in the development thread <https://github.com/portnov/sverchok/issues/19>`_.

