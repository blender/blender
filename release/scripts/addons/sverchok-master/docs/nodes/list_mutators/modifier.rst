List Modifier
=============

This node offers an assortment of list modification functions. The node has both Unary and Binary modes.

-  In Unary mode it will use the input of either sockets, it will use data1 first, then check data2
-  If both are linked data1 is used.
-  The node will draw the name of the current mode into the node header, useful for minimized nodes.

Behaviour
---------

+----------------------+----------+--------------------------------------------------------------------------+
| Modes                | inputs   | Behaviour Description                                                    |
+======================+==========+==========================================================================+
| Set                  | unary    | turns the valid input into a set ::                                      |
|                      |          |                                                                          |
|                      |          |   input = [0,0,0,1,1,1,3,3,3,5,5,5,6,7,8,4,4,4,6,6,6,7,7,7,8]            |
|                      |          |   output = [set(input)]                                                  |
|                      |          |                                                                          |
+----------------------+----------+--------------------------------------------------------------------------+
| Ordered Set by input | unary    | only unique numbers but ordered by the original input sequence ::        |
|                      |          |                                                                          |
|                      |          |   input = [0,0,0,1,1,1,3,3,3,5,5,5,6,7,8,4,4,4,6,6,6,7,7,7,8]            |
|                      |          |   output = [0,1,3,5,6,7,8,4]                                             |
|                      |          |                                                                          |
+----------------------+----------+--------------------------------------------------------------------------+
| Unique Consecutives  | unary    | no consecutive repeats ::                                                |
|                      |          |                                                                          |
|                      |          |   input = [0,0,0,1,1,1,3,3,3,5,5,5,6,7,8,4,4,4,6,6,6,7,7,7,8]            |
|                      |          |   output = [0,1,3,5,6,7,8,4,6,7,8]                                       |
|                      |          |                                                                          |
+----------------------+----------+--------------------------------------------------------------------------+
| Sequential Set       | unary    | unique input values, ordered by their value ::                           |
|                      |          |                                                                          |
|                      |          |   input = [0,0,0,1,1,1,3,3,3,5,5,5,6,7,8,4,4,4,6,6,6,7,7,7,8]            |
|                      |          |   output = [0,1,3,4,5,6,7,8]                                             |
|                      |          |                                                                          |
+----------------------+----------+--------------------------------------------------------------------------+
| Sequential Set Rev   | unary    | unique input values, ordered by their value, reversed ::                 |
|                      |          |                                                                          |
|                      |          |   input = [0,0,0,1,1,1,3,3,3,5,5,5,6,7,8,4,4,4,6,6,6,7,7,7,8]            |
|                      |          |   output = [8,7,6,5,4,3,1,0]                                             |
|                      |          |                                                                          |
+----------------------+----------+--------------------------------------------------------------------------+
| Normalize            | unary    | scales down the values in the list to the range ``-1.0 .. 1.0``          |
+----------------------+----------+--------------------------------------------------------------------------+
| Accumulating Sum     | unary    | see ``itertools.accumulate`` ::                                          |
|                      |          |                                                                          |
|                      |          |   input = list(accumulate(range(10)))                                    |
|                      |          |   output = [0,1,3,6,10,15,21,28,36,45]                                   |
|                      |          |                                                                          |
+----------------------+----------+--------------------------------------------------------------------------+
| Mask Subset          | binary   | generates a mask to indicate for each value in A whether it appears in B |
|                      |          | ::                                                                       |
|                      |          |                                                                          |
|                      |          |   A = [0,1,2,3,4,5,6,7]                                                  |
|                      |          |   B = [2,3,4,5]                                                          |
|                      |          |   output = [False, False, True, True, True, True, False, False]          |
|                      |          |                                                                          |
+----------------------+----------+--------------------------------------------------------------------------+
| Intersection         | binary   | returns the set of items that appear in both A and B                     |
|                      |          |                                                                          |
|                      |          | |image1|                                                                 |
|                      |          |                                                                          |
+----------------------+----------+--------------------------------------------------------------------------+
| Union                | binary   | returns the set of items A joined with B                                 |
|                      |          |                                                                          |
|                      |          | |image2|                                                                 |
|                      |          |                                                                          |
+----------------------+----------+--------------------------------------------------------------------------+
| Difference           | binary   | returns the set of items from A that don’t appear in B                   |
|                      |          |                                                                          |
|                      |          | |image3|                                                                 |
|                      |          |                                                                          |
+----------------------+----------+--------------------------------------------------------------------------+
| Symmetric Diff       | binary   | returns the set of elements of A and B that don’t appear in Both         |
|                      |          |                                                                          |
|                      |          | |image4|                                                                 |
|                      |          |                                                                          |
+----------------------+----------+--------------------------------------------------------------------------+

*output as list*

The boolean switch to *output as list* will be on by default,
essentially it will wrap the output as a list because true sets don’t
have a defined order (which we do need most of the time).

Example
-------

See the pullrequest for details : https://github.com/nortikin/sverchok/pull/884

also see the original thread : https://github.com/nortikin/sverchok/issues/865



.. |image1| image:: https://cloud.githubusercontent.com/assets/619340/18662881/733c219c-7f1c-11e6-85fc-fcfc1ea7768d.png
.. |image2| image:: https://cloud.githubusercontent.com/assets/619340/18662921/a24aac7e-7f1c-11e6-80c1-684e513607a2.png
.. |image3| image:: https://cloud.githubusercontent.com/assets/619340/18663232/ec821d80-7f1d-11e6-83bc-3fd64ff037b4.png
.. |image4| image:: https://cloud.githubusercontent.com/assets/619340/18662983/f252aeba-7f1c-11e6-963b-e2b7d7111e17.png