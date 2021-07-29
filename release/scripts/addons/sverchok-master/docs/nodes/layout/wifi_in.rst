Wifi In & Out
=============

Functionality
-------------

.. image :: https://cloud.githubusercontent.com/assets/6241382/4332088/30276d50-3fcf-11e4-8920-abaf3107025a.png

Create a invisble noodle, useful for keeping layout clean for example constants that are resued in many place.

Concept
-------
A named Wifi Input node can be listened to by any number of Wifi Output nodes. 
A Wifi Output node needs to be linked to a specific Wifi Input node useing the dropdown list.



Inputs
------

In the Wifi In node there are N inputs named after variable name. 

Outputs
-------

In a linked Wifi Out there are N-1 output of matching type.

Notes
-----
Variable names for Wifi Input nodes need to be unique.

Sharing of data is at the moment only possible within one layout.

The virtual noodle has a small overhead that is small enough that it can ignored for most practical scenarios. In the future even this should disappear.
