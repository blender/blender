List Shift
==========

Functionality
-------------

Shifting data in selected level on selected integer value as:
  
  [0,1,2,3,4,5,6,7,8,9] with shift integer 4 will be
  [4,5,6,7,8,9]
  But with enclose flag:
  [4,5,6,7,8,9,0,1,2,3]
  
Inputs
------

**data** - list of data any type to shift
**Shift** - value that defines shift

Properties
----------

**level** - manipulation level, 0 - is objects shifting
**enclose** - close data when shifting, that way ending cutted numbers turns to beginning

Outputs
-------

**data** - shifter data, adaptive socket

Examples
--------

.. image:: https://cloud.githubusercontent.com/assets/5783432/5603102/bec2bc6e-9384-11e4-9e4a-905da01b7ac1.gif
  :alt: shift