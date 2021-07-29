3 Point Arc
===========

Functionality
-------------

Given a *start coordinate*, a *through coordinate*, and an *end coordinate* this Node will find the Arc that passes through those points.

Inputs
------

- arc_pts input is `[begin, mid, end, begin, mid, end, begin, mid, end..... ]`

    - must be (len % 3 == 0 )

- num verts is either

    - constant
    - unique
    - or repeats last value if the number of arcs exceeds the number of values in the `num_vert` list


Parameters
----------

The UI is quite minimal.


- **num verts** can be changed via Slider input on the UI or as described above, it can be fed multiple values through the input sockets.


Output
------

- (verts, edges) : A set of each of these that correspond with a packet of commands like 'start, through, end, num_verts'
- verts needs to be connected to get output
- edges is optional

Examples
--------

.. image:: https://cloud.githubusercontent.com/assets/619340/3375992/3bbc0c86-fbd0-11e3-9456-353c77fd0d17.gif

See the progress of how this node came to life `here <https://github.com/nortikin/sverchok/issues/254>`_ (gifs, screenshots)

Basic example:

..image:: https://cloud.githubusercontent.com/assets/1275858/23209252/c5936418-f8f8-11e6-8e1c-3b1bbbf83202.png
