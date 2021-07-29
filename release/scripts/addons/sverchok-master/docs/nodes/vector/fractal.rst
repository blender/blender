Fractal
=======

This fractal node takes a list of Vectors and outputs a list of equal length containing Floats in the range 0.0 to 1.0.

Inputs & Parameters
-------------------

+----------------+-------------------------------------------------------------------------+
| Parameters     | Description                                                             |
+================+=========================================================================+
| Noise Function | The node output only Scalar values                                      |
+----------------+-------------------------------------------------------------------------+
| Noise Type     | Pick between several noise types                                        |
|                |                                                                         |
|                | - Blender                                                               |
|                | - Cell Noise                                                            |
|                | - New Perlin                                                            |
|                | - Standard Perlin                                                       |
|                | - Voronoi Crackle                                                       |
|                | - Voronoi F1                                                            |
|                | - Voronoi F2                                                            |
|                | - Voronoi F2F1                                                          |
|                | - Voronoi F3                                                            |
|                | - Voronoi F4                                                            |
|                |                                                                         |
|                | See mathutils.noise docs ( Noise_ )                                     |
+----------------+-------------------------------------------------------------------------+
| Fractal Type   | Pick between several fractal types                                      |
|                |                                                                         |
|                | - Fractal                                                               |
|                | - MultiFractal                                                          |
|                | - Hetero terrain                                                        |
|                | - Ridged multi fractal                                                  |
|                | - Hybrid multi fractal                                                  |
+----------------+-------------------------------------------------------------------------+
| H_factor       | Accepts float values, they are hashed into *Integers* internally.       |
+----------------+-------------------------------------------------------------------------+
| Lacunarity     | Accepts float values                                                    |
+----------------+-------------------------------------------------------------------------+
| Octaves        | Accepts integers values                                                 |
+----------------+-------------------------------------------------------------------------+
| Offset         | Accepts float values                                                    |
+----------------+-------------------------------------------------------------------------+
| Gain           | Accepts float values                                                    |
+----------------+-------------------------------------------------------------------------+

Examples
--------

.. image:: https://cloud.githubusercontent.com/assets/1275858/22591683/15af2118-ea16-11e6-9851-b697926cefb8.png

Basic example with a Vector rewire node.

json file: https://gist.github.com/kalwalt/5ef4f6b6018724874e3c51eaa255930c

Notes
-----

This documentation doesn't do the full world of fractals any justice, feel free to send us layouts that you've made which rely on this node.

Links
-----
Fractals description from wikipedia: https://en.wikipedia.org/wiki/Fractal

A very interesting resource is "the book of shaders", it's about shader programming but there is a very useful fractal paragraph:

http://thebookofshaders.com/13/ and on github repo: https://github.com/patriciogonzalezvivo/thebookofshaders/tree/master/13



.. _Noise: http://www.blender.org/documentation/blender_python_api_current/mathutils.noise.html
..
