List Reverse
============

Functionality
-------------

Reverse items from list based on index. It should accept any type of data from Sverchok: Vertices, Strings (Edges, Polygons) or Matrix.

Inputs
------

Takes any kind of data.

Parameters
----------


**Level:** Set the level at which to observe the List.

Outputs
-------

Depends on incoming data and can be nested. Level 0 is top level (totally zoomed out), higher levels get more granular (zooming in) until no higher level is found (atomic). The node will just reverse the data at the level selected.

Examples
--------

.. image:: https://cloud.githubusercontent.com/assets/5990821/4190715/1452079c-3788-11e4-8ce2-716b5046cf56.png
  :alt: ListReverseDemo1.PNG

In this example the node reverse a list a integers