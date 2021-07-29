Vector Polar Input
==================

*destination after Beta: Vector*

Functionality
-------------

This node generates a vector from it's cylindrical or spherical coordinates. Angles can be measured in radians or in degrees.

Inputs & Parameters
-------------------

All parameters except for ``Coordinates`` and ``Angles mode`` can be specified using corresponding inputs.

+-----------------+---------------+-------------+----------------------------------------------------+
| Parameter       | Type          | Default     | Description                                        |  
+=================+===============+=============+====================================================+
| **Coordinates** | Cylindrical   | Cylindrical | Which coordinates system to use.                   |
|                 | or Spherical  |             |                                                    |
+-----------------+---------------+-------------+----------------------------------------------------+
| **Angles mode** | Radians or    | Radians     | Interpret input angles as specified in radians or  |
|                 | Degrees       |             | degrees.                                           |
+-----------------+---------------+-------------+----------------------------------------------------+
| **rho**         | Float         | 0.0         | Rho coordinate.                                    |
+-----------------+---------------+-------------+----------------------------------------------------+
| **phi**         | Float         | 0.0         | Phi coordinate.                                    |
+-----------------+---------------+-------------+----------------------------------------------------+
| **z**           | Float         | 0.0         | Z coordinate. This input is used only for          |
|                 |               |             | cylindrical coordinates.                           |
+-----------------+---------------+-------------+----------------------------------------------------+
| **theta**       | Float         | 0.0         | Theta coordinate. This input is used only for      |
|                 |               |             | spherical coordinates.                             |
+-----------------+---------------+-------------+----------------------------------------------------+

Outputs
-------

This node has one output: **Vectors**. Inputs and outputs are vectorized, so if
you pass series of values to one of inputs, you will get series of vectors.

Examples of usage
-----------------

An archimedian spiral:

.. image:: https://cloud.githubusercontent.com/assets/284644/5840892/91800812-a1ba-11e4-84b3-c9347ed1b397.png

Logariphmic spiral:

.. image:: https://cloud.githubusercontent.com/assets/284644/5840891/917fddf6-a1ba-11e4-9cd1-a3b354a46348.png

Helix:

.. image:: https://cloud.githubusercontent.com/assets/284644/5840890/917b2ae0-a1ba-11e4-9432-33b68d8a19e3.png

With spherical coordinates, you can easily generate complex forms:

.. image:: https://cloud.githubusercontent.com/assets/284644/5840984/2e5bd24c-a1bb-11e4-97b3-99864881fa69.png

