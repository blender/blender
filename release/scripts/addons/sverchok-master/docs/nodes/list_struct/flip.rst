List Flip
=========

Functionality
-------------

Flips the data on selected level.
[[[1,2,3],[4,5,6],[7,8,9]],[[3,3,3],[1,1,1],[8,8,8]]] (two objects, three vertices)
with level 2 turns to:
[[[1, 2, 3], [3, 3, 3]], [[4, 5, 6], [1, 1, 1]], [[7, 8, 9], [8, 8, 8]]] (three objects, two vertices)
with level 3 turns to:
[[1, 4, 7], [2, 5, 8], [3, 6, 9], [3, 1, 8], [3, 1, 8], [3, 1, 8]] (six objects with three digits)

last example is not straight result, more as deviation.
Ideally Flip has to work with preserving data levels and with respect to other levels structure.
But for now working level is 2

Inputs
------

**data** - data to flip

Properties
----------

**level** - level to deal with

Outputs
-------

**data** - flipped data

Examples
--------

.. image:: https://cloud.githubusercontent.com/assets/5783432/5603155/cd1cc280-9386-11e4-9998-a066258ca94b.jpg
  :alt: flip