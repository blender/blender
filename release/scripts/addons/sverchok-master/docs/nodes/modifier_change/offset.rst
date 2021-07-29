Offset
======

*destination after Beta: Modifier Change*

Functionality
-------------

Make offset for polygons with bevel in corners. Output inner and outer polygons separately.

Inputs
------

This node has the following inputs:

- **Vers** - vertices of objects
- **Pols** - polygons of objects
- **offset** - offset values. Vectorized for every polygon as [[f,f,f,f,f]]
- **nsides** - number of rounded sides
- **radius** - bevel radius. Vectorized for every polygon as [[f,f,f,f,f]]

Parameters
----------

All parameters can be given by the node or an external input.
``offset`` and ``radius`` are vectorized and they will accept single or multiple values.

+-----------------+---------------+-------------+-------------------------------------------------------------+
| Param           | Type          | Default     | Description                                                 |
+=================+===============+=============+=============================================================+
| **offset**      | Float         | 0.04        | offset values.                                              |
+-----------------+---------------+-------------+-------------------------------------------------------------+
| **nsides**      | Integer       | 1           | number of rounded sides.                                    |
+-----------------+---------------+-------------+-------------------------------------------------------------+
| **radius**      | Float         | 0.04        | bevel radius.                                               |
+-----------------+---------------+-------------+-------------------------------------------------------------+


Outputs
-------

This node has the following outputs:

- **Vers**
- **Edgs**
- **OutPols** - get polygons that lay in outer polygon's line.
- **InPols** - get polygons that lay in inner polygon's line.

Examples of usage
-----------------

Offset and radius are defined by distance between point and polygon's center, divided by some number:

.. image:: https://cloud.githubusercontent.com/assets/5783432/18608956/88a1eaba-7d07-11e6-8f69-763ac4172c6a.png

Parameters' cases, that make different polygons (decomposer list node used to separate):

.. image:: https://cloud.githubusercontent.com/assets/5783432/18608957/8e4a5696-7d07-11e6-8f7f-ccfc6886a781.png

Upper image can be defined by one offset and list (range) of numbers, plugget to offset/radius, wich are vectorised:

.. image:: https://cloud.githubusercontent.com/assets/5783432/18608958/9194df92-7d07-11e6-9652-457776d42aaa.png
