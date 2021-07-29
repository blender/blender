KDT Closest Edges
=================

*Alias: KDTree Edges*

Functionality
-------------

On each update it takes an incoming pool of Vertices and places them in a K-dimensional Tree. 
It will return the Edges it can make between those vertices pairs that satisfy the constraints 
imposed by the 4 parameters. 

Inputs
------

- Verts, a pool of vertices to iterate through

Parameters
----------

+------------+-------+-----------------------------------------------------------+
| Parmameter | Type  | Description                                               |  
+============+=======+===========================================================+
| mindist    | float | Minimum Distance to accept a pair                         |   
+------------+-------+-----------------------------------------------------------+
| maxdist    | float | Maximum Distance to accept a pair                         |
+------------+-------+-----------------------------------------------------------+
| maxNum     | int   | Max number of edges to associate with the incoming vertex |
+------------+-------+-----------------------------------------------------------+
| Skip       | int   | Skip first n found matches if possible                    |
+------------+-------+-----------------------------------------------------------+

Outputs
-------

- Edges, which can connect the pool of incoming Verts to eachother.

Examples
--------

development thread `has examples <https://github.com/nortikin/sverchok/issues/108>`_

.. image:: https://cloud.githubusercontent.com/assets/619340/2847594/4c5246d2-d0b0-11e3-9f2c-8887ab926ab3.PNG
