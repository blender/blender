List Slice
==========
Functionality
-------------

Select a slice from a list. The node is *data type agnostic*, meaning it makes no assumptions about the data you feed it. It shoudld accepts any type of data native to Sverchok..
Functionality is a subset of python list slicing, the stride parameter functionality isn't implemented.

Inputs
------

+--------+--------------------------------------------------------------------------+
| Input  | Description                                                              |
+========+==========================================================================+
| Data   | The data - can be anything                                               | 
+--------+--------------------------------------------------------------------------+
| Start  | Slice start, allows negative python index                                |
+--------+--------------------------------------------------------------------------+
| Stop   | Slice stop, allows negative python index                                 |
+--------+--------------------------------------------------------------------------+

Parameters
----------

**Level**

It is essentially how many chained element look-ups you do on a list. If ``SomeList`` has a considerable *nestedness* then you might access the most atomic element of the list doing ``SomeList[0][0][0][0]``. Levels in this case would be 4.

**Start**

Start point for the slice

**Stop**

Stop point for the slice.

Outputs
-------

Slice, the selected slices.
Other, the list with the slices removed.

Examples
--------
    
Trying various inputs, adjusting the parameters, and piping the output to a *Debug Print* (or stethoscope) node will be the fastest way to acquaint yourself with the inner workings of the *List Item* Node.

Some slice examples.
>>> l
[0, 1, 2, 3, 4, 5, 6, 7, 8, 9]
>>> l[1:-1]
[1, 2, 3, 4, 5, 6, 7, 8]
>>> l[0:2]
[0, 1]
>>> l[-1:2]
[]

.. image:: https://cloud.githubusercontent.com/assets/5783432/5229789/771e02b8-7725-11e4-8970-ac33c87f55ec.png
 :alt: slice multiple objects from one list (may be many objects also)

Notes
-----


