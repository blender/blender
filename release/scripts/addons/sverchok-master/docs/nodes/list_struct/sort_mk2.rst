List Sort
=========

Functionality
-------------

Sort items from list based on index. It should accept any type of data from Sverchok: Vertices, Strings (Edges, Polygons) or Matrix.

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

.. image:: https://cloud.githubusercontent.com/assets/5990821/4190847/0979eeba-3789-11e4-9e51-7ca4c532c418.png
  :alt: ListSortDemo1.PNG

In this example the node sort a list a integers previously shuffled.