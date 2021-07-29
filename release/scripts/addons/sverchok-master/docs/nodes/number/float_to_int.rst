Float to Integer
================

Functionality
-------------

Converts incoming *Float* values to the nearest whole number *(Integer)*. Accepts lists and preserves levels of nestedness.

Inputs
------

A `float`, alone or in a list

Outputs
-------

An `int`, alone or in a list

Examples
--------

::

    1.0 becomes 1
    -1.9 becomes -2
    4.3 becomes 4
    4.7 becomes 5

    [1.0, 3.0, 2.4, 5.7] becomes [1, 3, 2, 6]

