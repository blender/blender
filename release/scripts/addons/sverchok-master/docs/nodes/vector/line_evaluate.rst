Vector Evaluate
===============

Functionality
-------------

Vector Evaluate need two groups of vertices (or just 2) as inputs to evaluate al the global positions between them.

Inputs
------

- **Factor**
- **Vertice A**
- **Vertice B**

Only **Factor** can be set inside the node. There is no default values for Vertice A or B.

Parameters
----------

All parameters need to proceed from an external node.


+------------------+---------------+-------------+-----------------------------------------------+
| Param            | Type          | Default     | Description                                   |  
+==================+===============+=============+===============================================+
| **Vertice A**    | Vertices      | None        | first group of vertices                       | 
+------------------+---------------+-------------+-----------------------------------------------+
| **Vectice B**    | Vertices      | None        | second group of vertices                      |
+------------------+---------------+-------------+-----------------------------------------------+
| **Factor**       | Float         | 0.50        | distance percentage between vertices A and B  |
+------------------+---------------+-------------+-----------------------------------------------+

Outputs
-------

**EvPoint** will need Vertices A and B to be generated. The output will be a new group of vertices between groups A and B, based on the factor setting. See example below.


Example of usage
----------------

.. image:: https://cloud.githubusercontent.com/assets/5990821/4188727/aaceebe4-3775-11e4-85cd-df80606b1509.gif

In this example just two vertices are evaluated. The gif shows the output based on the factor setting.