Hilbert 3D
=======================

Functionality
-------------

Hilbert image recreator. Based on hilbert space this node recreates image by interpolating it on pixels.

Inputs
------

- **level**
- **size**
- **sensitivity**

Parameters
----------

All parameters can be given by the node or an external input.


+-----------------+---------------+-------------------+----------------------------------------------------------+
| Param           |  Type         |   Default         |    Description                                           |
+=================+===============+===================+==========================================================+
| **RGB**         |  float        |   0.3,0.59,0.11   |    RGB map of imported image, sensitivity to each color  |
+-----------------+---------------+-------------------+----------------------------------------------------------+
| **image name**  |  string       |   None            |    enumerate popup to choose image from stack            |
+-----------------+---------------+-------------------+----------------------------------------------------------+
| **level**       |  Int          |   2               |    level of division of hilbert square                   |
+-----------------+---------------+-------------------+----------------------------------------------------------+
| **size**        |  float        |   1.0             |    scale of hilbert mesh                                 |
+-----------------+---------------+-------------------+----------------------------------------------------------+
| **sensitivity** |  float        |   1.0             |    define scale of values to react and build image       |                           
+-----------------+---------------+-------------------+----------------------------------------------------------+

Outputs
-------

**Vertices**, **Edges**.


Example of usage
----------------

.. image:: https://cloud.githubusercontent.com/assets/5783432/4381109/5bca94dc-4371-11e4-8de0-eb3ee7356aa8.png
  :alt: hilbertimage.PNG

recreate image in hilbert
