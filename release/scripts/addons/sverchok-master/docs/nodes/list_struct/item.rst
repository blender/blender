List Item
=========

Functionality
-------------

Select items from list based on index. The node is *data type agnostic*, meaning it makes no assumptions about the data you feed it. It shoudld accepts any type of data native to Sverchok..

Inputs
------

+--------+--------------------------------------------------------------------------+
| Input  | Description                                                              |
+========+==========================================================================+
| Data   | The data - can be anything                                               | 
+--------+--------------------------------------------------------------------------+
| item   | Item(s) to select, allows negative index python index                    |
+--------+--------------------------------------------------------------------------+

Parameters
----------


**Level**

It is essentially how many chained element look-ups you do on a list. If ``SomeList`` has a considerable *nestedness* then you might access the most atomic element of the list doing ``SomeList[0][0][0][0]``. Levels in this case would be 4.

**item**

A list of items to select, allow negative index python indexing so that -1 the last element. The items doesn't have to be in order and a single item can be selected more than a single time.

Outputs
-------

Item, the selected items on the specifed level.
Other, the list with the selected items deleted.

Examples
--------

Trying various inputs, adjusting the parameters, and piping the output to a *Debug Print* (or stethoscope) node will be the fastest way to acquaint yourself with the inner workings of the *List Item* Node.
