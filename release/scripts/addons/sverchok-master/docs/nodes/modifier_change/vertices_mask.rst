Mask Vertices
=============

Functionality
-------------

Filter vertices with False/True bool values and automatically removes not connected edges and polygons.

Inputs
------

- **Mask**
- **Vertices**
- **Poly Edge**

Parameters
----------

+-----------+------------------+-----------+----------------------------------------------------------------+
| Param     | Type             | Default   | Description                                                    |
+===========+==================+===========+================================================================+    
| Mask      | list of booleans | [1,0]     | Mask can be defined with ListInput node or Formula node        |   
|           |                  |           | or other as list [n,n1,n2...ni] where n's can be 0 or 1        |  
|           |                  |           | (False or True)                                                |
+-----------+------------------+-----------+----------------------------------------------------------------+

Outputs
-------

- **Vertices**
- **Poly Edge**

Examples of usage
-----------------

.. image:: https://cloud.githubusercontent.com/assets/5783432/18615011/cc68a924-7dab-11e6-82f0-6f72bfde7ba7.png
