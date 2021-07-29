List Delete Levels
==================

Functionality
-------------

This helps flatten lists, or make them less nested. 

The analogy to keep in mind might be: 

.. pull-quote::
    knocking through walls in a house to join two spaces, or knock non load bearing walls between buildings to join them.

Incoming nested lists can be made less nested.

.. code-block:: python

    # del level 0, remove outer wrapping
    
    [[0,1,2,3,4]] 
    >>> [0,1,2,3,4]

    [[4, 5, 6], [4, 7, 10], [4, 9, 14]]
    >>> [4, 5, 6, 4, 7, 10, 4, 9, 14]

    [[5], [5], [5], [5], [5], [5]]
    >>> [5, 5, 5, 5, 5, 5]

Usage
-----
Type 1,2 or 2,3 or 1,3 or 1,2,3 or 3,4 etc to leave this levels and remove others.

Throughput
----------

====== =================================================
Socket Description
====== =================================================
Input  Any meaningful input, lists, nested lists
Output Modified according to Levels parameter, or None
====== =================================================


Parameters
----------

Levels, this text field 


Examples
--------

.. image:: https://cloud.githubusercontent.com/assets/619340/4198552/851ac6f4-37fa-11e4-9c8e-4715ded8c717.PNG
  :alt: delete_levels_demo1


Notes
-----
