Vector Lerp
===========

Functionality
-------------

This node's primary function is to perform the linear interpolation between two Vectors, or streams of Vectors.
If we have two Vectors A and B, and a factor 0.5, then the output of the node will be a Vector exactly half way on the imaginary finite-line between A and B. Values beyond 1.0 or lower than 0.0 will be extrapolated to beyond the line A-B.

Inputs
------

.. image:: https://cloud.githubusercontent.com/assets/619340/22403415/97f342a2-e616-11e6-8494-668a28c3cfb9.png

Vector Evaluate needs two Vertex stream inputs (each containing 1 or more vertices). If one list is shorter than the other then the shortest stream is extended to match the length of the longer stream by repeating the last valid vector found in the shorter stream.


Parameters
----------

+------------------+---------------+-------------+-------------------------------------------------+
| Param            | Type          | Default     | Description                                     |  
+==================+===============+=============+=================================================+
| mode             | Enum          | Lerp        | - **Lerp** will linear interpolate once between |
|                  |               |             |   each corresponding Vector                     |   
|                  |               |             |                                                 | 
|                  |               |             | - **Evaluate** will repeatedly interpolate      |
|                  |               |             |   between each member of vectors A and B for    |
|                  |               |             |   all items in Factor input (see example)       |
+------------------+---------------+-------------+-------------------------------------------------+
| **Vertex A**     | Vector        | None        | first group of vertices (Stream)                | 
+------------------+---------------+-------------+-------------------------------------------------+
| **Vertex B**     | Vector        | None        | second group of vertices (Stream)               |
+------------------+---------------+-------------+-------------------------------------------------+
| **Factor**       | Float         | 0.50        | distance ratio between vertices A and B.        |
|                  |               |             | values outside of the 0.0...1.0 range are       |
|                  |               |             | extrapolated on the infinite line A, B          |
+------------------+---------------+-------------+-------------------------------------------------+

Outputs
-------

The content of **EvPoint** depends on the current mode of the node, but it will always be a list (or multiple lists) of Vectors. 


Example of usage
----------------

**0.5**

.. image:: https://cloud.githubusercontent.com/assets/619340/22403445/b6ffef46-e617-11e6-8d84-f47295be5230.png

**-0.5**  

.. image:: https://cloud.githubusercontent.com/assets/619340/22403447/e18ff24c-e617-11e6-82cd-abd28c22a517.png

**1.5**  

.. image:: https://cloud.githubusercontent.com/assets/619340/22403454/fc9b6030-e617-11e6-9f18-822fb0ab5a50.png

**range float `0.0 ... 1.0   n=10`  Evaluate**  

.. image:: https://cloud.githubusercontent.com/assets/619340/22403473/72919a98-e618-11e6-8ada-955595368766.png

**range float `0.0 ... 1.0   n=10`  Lerp**  

.. image:: https://cloud.githubusercontent.com/assets/619340/22403494/ec8b0fd2-e618-11e6-8830-96b01f31bcd4.png

**Lerp interpolation with noise**

.. image:: https://cloud.githubusercontent.com/assets/619340/22403515/aedf276c-e619-11e6-91e3-bfabc94be1ab.png





The development thread also has examples: https://github.com/nortikin/sverchok/issues/1098



