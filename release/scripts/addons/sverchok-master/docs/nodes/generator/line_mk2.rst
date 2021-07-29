Line
====

Functionality
-------------

Line generator creates a series of connected segments based on the number of vertices and the length between them. Just a standard subdivided line along X axis

Inputs
------

**N Verts** and **Step** are vectorized. They will accept single or multiple values.
Both inputs will accept a single number or an array of them. It also will work an array of arrays::

    [2]
    [2, 4, 6]
    [[2], [4]]

Parameters
----------

All parameters except **Center** can be given by the node or an external input.


+-------------+---------------+-------------+-----------------------------------------------+
| Param       | Type          | Default     | Description                                   |
+=============+===============+=============+===============================================+
| **N Verts** | Int           | 2           | number of vertices. The minimum is 2          |
+-------------+---------------+-------------+-----------------------------------------------+
| **Step**    | Float         | 1.00        | length between vertices                       |
+-------------+---------------+-------------+-----------------------------------------------+
| **Center**  | Boolean       | False       | center line around 0                          |
+-------------+---------------+-------------+-----------------------------------------------+

Outputs
-------

**Vertices** and **Edges**. Verts and Edges will be generated. Depending on the inputs, the node will generate only one or multiples independant lines. See examples below.


Example of usage
----------------

.. image:: https://cloud.githubusercontent.com/assets/5990821/4186320/cb5517c8-375f-11e4-98ab-a1a9f873c5ef.png
  :alt: LineDemo1.PNG

The first example shows just an standard line with 6 vertices and 1.20 ud between them

.. image:: https://cloud.githubusercontent.com/assets/5990821/4186321/cb5a3708-375f-11e4-90dd-736b38e9dcaa.png
  :alt: LineDemo2.PNG

In this example the step is given by a series of numbers::

    [0.5, 1.0 , 1.5, 2.0, 2.5]