Data Shape
==========

Functionality
-------------

This node displays information about "shape" and "nesting level" of data that
are passed to input.
It can be useful to quickly understand, whether given nodes setup produces list
of vertices, or list of lists of vertices, or whatever.

Inputs
------

This node has one input: **Data**. This input can accept any sort of data: vertices, edges, text and so on.

Parameters
----------

This node has no parameters. Human-readable description of shape of data passed
to **Data** input is displayed on the node itself.

Outputs
-------

This node has one output: **Text Out**. String representation of data shape is put to that output.

Example of usage
----------------

Simple example:

.. image:: https://user-images.githubusercontent.com/284644/34079935-a8cd1e5a-e358-11e7-8c64-ef656a5bd874.png

