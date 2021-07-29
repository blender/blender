Image Decomposer
================

*destination after Beta: analyzers*

.. image:: https://cloud.githubusercontent.com/assets/619340/4495748/bbd3fe56-4a5e-11e4-9202-3b86f7c57c22.png


Functionality
-------------

To get output from this node you must connect something to the first 2 output sockets (xya and rgb), polygons is optional
and only outputs faces when ``Filter?`` is off.

Takes the ``rgba`` components of an image currently loaded in Blender and decomposes 
them into ``xya`` and ``rgb`` Vertex-style sockets. ``xy`` are inferred by the number of pixels in the image and the image width. ``z`` doesn't make much sense in relation to a pixel and was replaced by the Alpha channel of the pixel (``a``). 

If you don't have images loaded in the UV editor, they can be imported from N panel 
into Blender and loaded from there. 


Inputs & Parameters
-------------------

+---------------+-------------------------------------------------------------------+
| name          | function                                                          |
+===============+===================================================================+
| Skip n pixels | allows to sample a reduced grid of the image, every nth pixel in  |
|               | either direction.                                                 |
+---------------+-------------------------------------------------------------------+
| xy_spread     | the ``xy`` component of the ``xya`` socket can be multiplied to   |
|               | get a wider spread.                                               | 
+---------------+-------------------------------------------------------------------+
| z_spread      | this amplifies ``rgb``, not ``a`` (which you can amplify yourself | 
|               | if that was needed.)                                              |
+---------------+-------------------------------------------------------------------+
| Filter?       | uses a restricted eval to drop pixels using a simple typed command|
|               | : example ``r < 0.8 and g > 0.4`` (more below)                    |
+---------------+-------------------------------------------------------------------+


Outputs
-------


+-------------+--------------------------------------------------------------------+
| name        | function                                                           |
+=============+====================================================================+
| ``xya``     | the **x** and **y** of the pixel, combined with the Alpha channel. |
|             | The value of **x** and **y** are multiplied by ``xy_spread``.      |
+-------------+--------------------------------------------------------------------+
| ``rgb``     | each (unfiltered) pixel component is multiplied by ``z_spread``    |
+-------------+--------------------------------------------------------------------+
| polygons    | this output will generate sensible polygon index list for ``xya``  |
|             | when pixels are unfiltered.                                        |
+-------------+--------------------------------------------------------------------+

Examples
--------

`The development thread <https://github.com/nortikin/sverchok/issues/405>`_ contains working examples of this Node used as preprocessor for game maps.

Notes
-----

The loaded image gets a fake user automagically, tho perhaps this should be optional.