Vector Rewire
=============

Functionality
-------------

Use this node to swap Vector components, for instance pass X to Y (and Y to X ). Or completely filter out a component by switching to the Scalar option. it will default to *0.0* when the Scalar socket is unconnected, when connected it will replace the component with the values from the socket. If the content of the Scalar input lists don't match the length of the Vectors list, the node will repeat the last value in the list or sublist (expected Sverchok behaviour).

.. image:: https://cloud.githubusercontent.com/assets/619340/22211977/3bb60a64-e18f-11e6-82ca-5afac681b195.png
  :alt: with vector rewire


Inputs
------

**Vectors** - Any list of Vector/Vertex lists
**Scalar** - value or series of values, will auto repeat last valid value to match Vector count.


Outputs
-------

**Vector** - Vertex or series of vertices



   
