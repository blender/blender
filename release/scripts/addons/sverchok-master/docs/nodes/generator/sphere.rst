Sphere
========

Functionality
-------------

Sphere generator will create a sphere based on its Radius and de U and V subdivisions.

Inputs
------

All inputs are vectorized and they will accept single or multiple values.
There is three inputs:

- **Radius**
- **U**
- **V**

Parameters
----------

All parameters except **Separate** and **Caps** can be given by the node or an external input.


+--------------+---------------+-------------+------------------------------------------+
| Param        |  Type         |   Default   |    Description                           |
+==============+===============+=============+==========================================+
| **Radius**   |  Float        |   1.00      |    radius of the sphere                  |    
+--------------+---------------+-------------+------------------------------------------+
| **U**        |  Int          |   24        |    U subdivisions                        |    
+--------------+---------------+-------------+------------------------------------------+
| **V**        |  Int          |   24        |    V subdivisions                        |   
+--------------+---------------+-------------+------------------------------------------+
| **Separate** |  Bolean       |   False     |    Grouping vertices by V direction      |
+--------------+---------------+-------------+------------------------------------------+

Outputs
-------

**Vertices**, **Edges** and **Polygons**. 
All outputs will be generated. Depending on the type of the inputs, the node will generate only one or multiples independant spheres.


Example of usage
----------------

.. image:: https://cloud.githubusercontent.com/assets/5990821/4187465/2a08ffdc-376a-11e4-8359-b4f98567dedb.png
  :alt: SphereDemo1.PNG

As you can see, lot of different outputs can be generated with this node.
