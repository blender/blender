List Repeater
=============

Functionality
-------------

Allows explicit repeat of lists and elements. The node is *data type agnostic*, meaning it makes no assumptions about the data you feed it. It shoudld accepts any type of data native to Sverchok..

Inputs
------

+--------+--------------------------------------------------------------------------+
| Input  | Description                                                              |
+========+==========================================================================+
| Data   | The data - can be anything                                               | 
+--------+--------------------------------------------------------------------------+
| Number | The amount of times to repeat elements selected by the `Level` parameter |
+--------+--------------------------------------------------------------------------+

Parameters
----------

Level and unwrap.

**Level**

It is essentially how many chained element look-ups you do on a list. If ``SomeList`` has a considerable *nestedness* then you might access the most atomic element of the list doing ``SomeList[0][0][0][0]``. Levels in this case would be 4.

**unwrap**

Removes any extra layers of wrapping (brackets or parentheses) found at the current Level. If the element pointed at is ``[[0,2,3,2]]``  it will become ``[0,2,3,2]``.


Outputs
-------

Lists (nested). The type of *Data out* will be appropriate for the operations defined by the parameters of the Node.

Examples
--------

Trying various inputs, adjusting the parameters, and piping the output to a *Debug Print* (or stethoscope) node will be the fastest way to acquaint yourself with the inner workings of the *List Repeater* Node.

A practical reason to use the node is when you need a series of copies of edge or polygon lists. Usually in conjunction with `Matrix Apply`, which outputs a series of `vertex lists` as a result of transform parameters.

.. image:: https://cloud.githubusercontent.com/assets/619340/4186432/efb79892-3760-11e4-9d17-5c7a7a22d9d9.PNG
  :alt: ListRepeater_Demo1.PNG