Circle
======

Functionality
-------------

Circle generator creates circles based on the radius and the number of vertices. What does that mean? It means that if the number of vertices is too low, ir will stop being a circle and will be a regular polygon, in example::

    - 3 vertices = triangle.
    - 4 vertices = square
    - ...
    - 6 vertices =  hexagon
    - ...
    - Many vertices =  circle

This node will also create sector or semgent of circles using the **Degrees** option. See the examples below to see it working also with the **mode** option.

Inputs
------

All inputs are vectorized and they will accept single or multiple values.
There is three inputs:

- **Radius**
- **N Vertices**
- **Degrees**

Same as other generators, all inputs will accept a single number, an array or even an array of arrays::

    [2]
    [2, 4, 6]
    [[2], [4]]

Parameters
----------

All parameters except **Mode** can be given by the node or an external input.


+----------------+---------------+-------------+----------------------------------------------------+
| Param          | Type          | Default     | Description                                        |  
+================+===============+=============+====================================================+
| **Radius**     | Float         | 1.00        | radius of the circle                               | 
+----------------+---------------+-------------+----------------------------------------------------+
| **N Vertices** | Int           | 24          | number of vertices to generate the circle          |
+----------------+---------------+-------------+----------------------------------------------------+
| **Degrees**    | Float         | 360.0       | angle for a sector/segment circle                  |
+----------------+---------------+-------------+----------------------------------------------------+
| **Mode**       | Bollean       | False       | switch between two sector and segment circle       |
+----------------+---------------+-------------+----------------------------------------------------+

Outputs
-------

**Vertices**, **Edges** and **Polygons**. 
All outputs will be generated. Depending on the type of the inputs, the node will generate only one or multiples independant circles. In example:

.. image:: https://cloud.githubusercontent.com/assets/5990821/4187227/07366302-3768-11e4-8e9c-4068c9ce6773.png
  :alt: CircleDemo1.PNG
.. image:: https://cloud.githubusercontent.com/assets/5990821/4187228/0759a754-3768-11e4-80a4-458e286edf20.png
  :alt: CircleDemo2.PNG

As you can see in the red rounded values, depending on how many inputs have the node, will be generated those same number of outputs.

If **Degrees** is minor than 0, depending of the **mode** state, will be generated a sector or a segment of a circle with that degrees angle.

Example of usage
----------------

.. image:: https://cloud.githubusercontent.com/assets/5990821/4186877/ab2f2e98-3764-11e4-9cd6-502228eec31c.png
  :alt: CircleDemo3.PNG

In this first example we see that circle generator can be a circle but also any regular polygon that you want.

.. image:: https://cloud.githubusercontent.com/assets/5990821/4186876/ab2edf4c-3764-11e4-980e-d9beb10b16d8.png
  :alt: CircleDemo4.PNG

The second example shows the use of **mode** option and how it generates sector or segment of a circle based on the **degrees** value.