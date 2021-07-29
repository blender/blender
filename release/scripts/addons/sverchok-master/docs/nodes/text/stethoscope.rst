Stethoscope
===========

*destination after Beta: basic data*

Functionality
-------------

The Node is designed to give a general sense of
the connected data stream. After a short preprocessing step Stethoscope draws the data directly to the Node view. 

**The processing step**

+---------------------------------------------------+
| The first and last 20 sublists will be displayed. | 
| The data in between is dropped and represented by |
| placeholder ellipses.                             | 
+---------------------------------------------------+
| Float values are rounded if possible.             |
+---------------------------------------------------+


Inputs
------

Any known Sverchok data type.


Parameters
----------

Currently a *visibility* toggle and *text drawing color* chooser.


Outputs
-------

Direct output to Node View


Examples
--------

Notes
-----

Implementation is ``POST_PIXEL`` using ``bgl`` and ``blf``