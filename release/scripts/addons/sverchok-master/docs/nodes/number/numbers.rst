A Number
========

Functionality
-------------

This node lets you output a number, either Int or Float. It also lets you set the Min and Max of the slider to ensure that the node never outputs beyond a certain range. 


Warning
-------

Currently: 

The node will pass any input directly to the output, it will not first recast ints to floats if you feed it integers while the node is in Integer mode. The reverse is also true. When the node's input socket is connected it will not limit the values of the incoming data. (you probably want to use a Remap Range node in that case anyway)


Inputs & Parameters
-------------------

**float or integer**  

Extended parameters
-------------------

**Show Limits** - boolean switch will show the Min and Max sliders on the UI when pressed. Unpressed the node only shows the choice between Integer and Float mode.


Outputs
-------

**float or integer** - only one digit. when unlinked

Examples
--------

see https://github.com/nortikin/sverchok/pull/1450 for examples