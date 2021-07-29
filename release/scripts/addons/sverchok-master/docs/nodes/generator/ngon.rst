NGon
====

*destination after Beta: generators*

Functionality
-------------

NGon generator creates regular (or not exactly, see below) polygons of given
radius with given number of sides. As an example, it can create triangles,
squares, hexagons and so on. In this sence, it is similar to Circle node.

Location of vertices can be randomized, with separate control of randomization
along radius and randomization of angle. See the examples below.

Each vertex can be connected by edge to next vertex (and produce usual
polygon), or some number of vertices can be skipped, to produce star-like
polygons. In the last case, you most probably will want to pass output of this
node to Intersect Edges node.

Inputs
------

All inputs are vectorized and they will accept single or multiple values.
This node has the following inputs:

- **Radius**
- **N Sides**
- **RandomR**
- **RandomPhi**
- **Seed**
- **Shift**

Same as other generators, all inputs will accept a single number, an array or even an array of arrays::

    [2]
    [2, 4, 6]
    [[2], [4]]

Parameters
----------

All parameters can be given by the node or an external input.


+----------------+---------------+-------------+-------------------------------------------------------------+
| Param          | Type          | Default     | Description                                                 |  
+================+===============+=============+=============================================================+
| **Radius**     | Float         | 1.00        | Radius of escribed circle. When ``RandomR`` is zero,        |
|                |               |             | then all vertices will be at this distance from origin.     | 
+----------------+---------------+-------------+-------------------------------------------------------------+
| **N Sides**    | Int           | 5           | Number of sides of polygon to generate. With higher         |
|                |               |             | values and ``Shift`` = 0, ``RandomR`` = 0, ``RandomPhi``    |
|                |               |             | = 0, you will get the same output as from Circle node.      |
+----------------+---------------+-------------+-------------------------------------------------------------+
| **RandomR**    | Float         | 0.0         | Amplitude of randomization of vertices along radius.        |
+----------------+---------------+-------------+-------------------------------------------------------------+
| **RandomPhi**  | Float         | 0.0         | Amplitude of randomizaiton of angles. In radians.           |
+----------------+---------------+-------------+-------------------------------------------------------------+
| **Seed**       | Float         | 0.0         | Random seed. Affects output only when ``RandomR`` != 0 or   |
|                |               |             | ``RandomPhi`` != 0.                                         |
+----------------+---------------+-------------+-------------------------------------------------------------+
| **Shift**      | Int           | 0           | Also known as "star factor". When this is zero, each vertex |
|                |               |             | is connected by edge to next one, and you will get usual    |
|                |               |             | polygon. Otherwise, n'th vertex will be connected to        |
|                |               |             | (n+shift+1)'th. In this case, you will get sort of star.    |
+----------------+---------------+-------------+-------------------------------------------------------------+

Outputs
-------

This node has the following outputs:

- **Vertices**
- **Edges**
- **Polygons**

If ``Shift`` input is not zero, then ``Polygons`` output will be empty - this
node does not create degenerated polygons.

Depending on the type of the inputs, the node will generate only one or multiples independant circles. 

Examples
--------

Sides=5, Shift=0, RandomR=0, RandomPhi=0 (default values):

.. image:: https://cloud.githubusercontent.com/assets/284644/5680574/bd0ee2e8-9830-11e4-92b7-8c031fb9ff08.png

Sides=6, RandomPhi=0.3:

.. image:: https://cloud.githubusercontent.com/assets/284644/5680573/bd0e5d5a-9830-11e4-83d2-350a03740d98.png

Sides=6, RandomR=0.3:

.. image:: https://cloud.githubusercontent.com/assets/284644/5680571/bd0b8602-9830-11e4-993c-b7f43e4f76ec.png

Sides=7, Shift=1, RandomR=0.24, RandomPhi=0.15:

.. image:: https://cloud.githubusercontent.com/assets/284644/5680572/bd0d9f3c-9830-11e4-9706-0b6f15d7dc4c.png

Sides=29, Shift=9, RandomR=0, RandomPhi=0:

.. image:: https://cloud.githubusercontent.com/assets/284644/5680575/bd1095e8-9830-11e4-8942-a281b6ab8a8d.png
