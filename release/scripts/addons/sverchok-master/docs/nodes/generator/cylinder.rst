Cylinder
========

Functionality
-------------

Cylinder generator, as well as circle, is used to create a big variety of polyhedra based on the cyliner form: two polygons connected by a body. In the examples will see some possibilities.

Inputs
------

All inputs are vectorized and they will accept single or multiple values.
There is three inputs:

- **Radius Top**
- **Radius Bottom**
- **Vertices**
- **Height**
- **Subdivisions**

Parameters
----------

All parameters except **Separate** and **Caps** can be given by the node or an external input.


+-------------------+---------------+-------------+--------------------------------------------------------+
| Param             | Type          | Default     | Description                                            |  
+===================+===============+=============+========================================================+
| **Radius Top**    | Float         | 1.00        | radius of the top polygon                              | 
+-------------------+---------------+-------------+--------------------------------------------------------+
| **Radius Bottom** | Float         | 1.00        | radius of the bottom polygon                           | 
+-------------------+---------------+-------------+--------------------------------------------------------+
| **Vertices**      | Int           | 32          | number of vertices to generate top and bottom poygons  |
+-------------------+---------------+-------------+--------------------------------------------------------+
| **Height**        | Float         | 2.00        | height of the cylinder                                 |
+-------------------+---------------+-------------+--------------------------------------------------------+
| **Subdivisions**  | Int           | 0           | number of the height subdivisions                      |
+-------------------+---------------+-------------+--------------------------------------------------------+
| **Separate**      | Bollean       | False       | grouping vertices by V direction                       |
+-------------------+---------------+-------------+--------------------------------------------------------+
| **Caps**          | Bollean       | True        | turn on and off top and bottom cap                     |
+-------------------+---------------+-------------+--------------------------------------------------------+

Outputs
-------

**Vertices**, **Edges** and **Polygons**. 
All outputs will be generated. Depending on the type of the inputs, the node will generate only one or multiples independant cylinders.
If **Separate** is True, the only the top and the bottom polygons will be generated.
With **Caps** with can enable or disable the top and bottom caps.

Example of usage
----------------

.. image:: https://cloud.githubusercontent.com/assets/5990821/4186892/cb062d3e-3764-11e4-95c3-511fd668ce1e.png
  :alt: CylinderDemo1.PNG

In this example with can see some examples of what can be done with this node.
