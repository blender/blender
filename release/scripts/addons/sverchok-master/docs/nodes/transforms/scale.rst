Scale
=====

Functionality
-------------

This node will allow you to scale any king of geometry. It works directly with vertices, not with matrixes, so the output will be just scaled geometry from your original vertices.

Inputs
------

All inputs are vectorized and they will accept single or multiple values.
There is three inputs:

- **Vertices**
- **Center**
- **Factor**

Parameters
----------

All parameters except **Vertices** has a default value. **Factor** can be given by the node or an external input.


+----------------+---------------+-----------------+----------------------------------------------------+
| Param          | Type          | Default         | Description                                        |  
+================+===============+=================+====================================================+
| **Vertices**   | Vertices      | none            | vertices to scale                                  | 
+----------------+---------------+-----------------+----------------------------------------------------+
| **Center**     | Vertices      | (0.0, 0.0, 0.0) | point from which the scaling will be done          |
+----------------+---------------+-----------------+----------------------------------------------------+
| **Factor**     | Float         | 1.0             | factor of scaling                                  |
+----------------+---------------+-----------------+----------------------------------------------------+

Outputs
-------

Only **Vertices** will be generated. Depending on the type of the inputs, if more than one factor or centers are set, then more objects will be outputted.
If you generate more outputs than inputs were given, then is probably that you need to use list Repeater with your edges or polygons.

Example of usage
----------------

.. image:: https://cloud.githubusercontent.com/assets/5990821/4220292/73e9430e-3900-11e4-949f-02749baa7751.png
  :alt: ScaleDemo1.PNG

In this example we use scale to convert a simple circle into a kind of parabola.