Variable Lacunarity
===================

This node takes a list of Vectors and outputs a list of equal length containing Floats in the range -1.0 to 1.0.
The seed value permits you to apply a different noise calculation to identical inputs.
This nodes "_returns variable lacunarity noise value, a distorted variety of noise,
from noise type 1 distorted by noise type 2 at the specified position_."


Inputs & Parameters
-------------------

+----------------+-------------------------------------------------------------------------+
| Parameters     | Description                                                             |
+================+=========================================================================+
| **Noise Type** | Pick between several noise types                                        |
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
| **Seed**       | Accepts int values.                                                     |
+----------------+-------------------------------------------------------------------------+
| **Distortion** | Accepts floats values, modulate the two noise basis.                    |
+----------------+-------------------------------------------------------------------------+

Examples
--------

.. image:: https://cloud.githubusercontent.com/assets/1275858/24367366/a2087bbc-131c-11e7-8d95-df90ce456034.png


Notes
-----

This documentation doesn't do the full world of noise any justice, feel free to send us layouts that you've made which rely on this node.



.. _Noise: http://www.blender.org/documentation/blender_python_api_current/mathutils.noise.html
