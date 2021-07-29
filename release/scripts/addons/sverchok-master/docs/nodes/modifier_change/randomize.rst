Randomize
=========

*destination after Beta: Modifier Change*

Functionality
-------------

This mode processes set of vertices by moving each of them by random distance
along X, Y, and Z axis. You can specify maximum distance of moving for each
axis.

Inputs
------

This node has the following inputs:

- **Vertices**
- **X amplitude**
- **Y amplitude**
- **Z amplitude**
- **Seed**

Parameters
----------

All parameters can be given by the node or an external input.
This node has the following parameters:

+-----------------+---------------+-------------+----------------------------------------------------+
| Parameter       | Type          | Default     | Description                                        |  
+=================+===============+=============+====================================================+
| **X amplitude** | Float         | 0.0         | Maximum distance to move vertices along X axis.    |
+-----------------+---------------+-------------+----------------------------------------------------+
| **Y amplitude** | Float         | 0.0         | Maximum distance to move vertices along Y axis.    |
+-----------------+---------------+-------------+----------------------------------------------------+
| **Z amplitude** | Float         | 0.0         | Maximum distance to move vertices along Z axis.    |
+-----------------+---------------+-------------+----------------------------------------------------+
| **Seed**        | Int           | 0           | Random seed.                                       |
+-----------------+---------------+-------------+----------------------------------------------------+

**Note**. Each amplitude input specifies maximum distance to move vertices
along corresponding axis. Vertices can be moved both in negative and positive
directions. For example, for vertex X coordinate = 10.0, and ``X amplitude`` = 1.0,
you can get output vertex coordinate from 9.0 to 11.0.

Outputs
-------

This node has one output: **Vertices**.

Example of usage
----------------

Given simplest nodes setup:

.. image:: https://cloud.githubusercontent.com/assets/284644/5693006/b7109768-992c-11e4-86ef-85cdba6b094d.png

you will have something like:

.. image:: https://cloud.githubusercontent.com/assets/284644/5693007/b71402fe-992c-11e4-9e1a-da2ff3d20947.png

