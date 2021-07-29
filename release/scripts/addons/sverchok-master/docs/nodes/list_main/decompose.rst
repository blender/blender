List Decompose
==============

Functionality
-------------

Inverse to list join node. Separate list at some level of data to several sockets. Sockets count the same as items count in exact level.

Inputs
------

- **data** - adaptable socket

Parameters
----------

+----------------+---------------+-------------+----------------------------------------------------------+
| Parameter      | Type          | Default     | Description                                              |  
+================+===============+=============+==========================================================+
| **level**      | Int           | 1           | Level of data to operate.                                |
+----------------+---------------+-------------+----------------------------------------------------------+
| **Count**      | Int           | 1           | Output sockets' count. defined manually or with Auto set |
+----------------+---------------+-------------+----------------------------------------------------------+
| **Auto set**   | Button        |             | Calculate output sockets' count based on data count on   |
|                |               |             | choosen level                                            |
+----------------+---------------+-------------+----------------------------------------------------------+

Outputs
-------

- **data** - multisocket


Example of usage
----------------

Decomposed simple list in 2 level:

.. image::  https://cloud.githubusercontent.com/assets/5783432/18610849/4b14c4fc-7d38-11e6-90ac-6dcad29b0a7d.png
