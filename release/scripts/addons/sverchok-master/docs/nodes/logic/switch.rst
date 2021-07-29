Switch
=============

Functionality
-------------
.. image:: https://cloud.githubusercontent.com/assets/6241382/4316878/50c6c7cc-3f0c-11e4-8a76-ecb5a93605d4.png
Switches between to sets of inputs.

Inputs
------

+--------+--------------------------------------------------------------------------+
| Input  | Description                                                              |
+========+==========================================================================+
| state  | state that decides which set of sockets to use                           | 
+--------+--------------------------------------------------------------------------+
| T 0    | If state is false this socket is used                                    |
+--------+--------------------------------------------------------------------------+
| F 0    | If state is true this socket  used                                       |
+--------+--------------------------------------------------------------------------+


Parameters
----------


**Count**

Number of sockets in each set.

**state**

If set to 1 T sockets are used, otherwise the F socket are used.


Outputs
-------

Out 0 to Out N depending on count. Socket types are copied from first from the T set.

Examples
--------
.. image:: https://cloud.githubusercontent.com/assets/6241382/4316849/f9d3a462-3f0b-11e4-9560-3323f5586125.png
Switching between a sphere and cylinder for drawing using switch node.
