Viewer Index
============

Functionality
-------------

Displays indices of incoming geometry, much like what is possible in the debug mode of Blender. The individual indices of 
*Vertices, Edges and Faces* can be displayed with and without a small background polygon to help contrast the index numbers and the 3d view color.

Inputs
------

This node Accepts sets of `Verts, Edges, Faces, and Matrices`. In addition it accepts a Text input to display Strings at the locations passed in through the `Vertices` socket.

Parameters
----------

**default**

+-----------------------+------------+----------------------------------------------------------------------+
| parameters            | type       | description                                                          |
+=======================+============+======================================================================+
| show                  | bool       | *activation* of the node                                             | 
+-----------------------+------------+----------------------------------------------------------------------+
| background            | bool       | display *background polygons* beneath each numeric (element of text) |
+-----------------------+------------+----------------------------------------------------------------------+
| verts, edges, faces   | multi bool | set of toggles to choose which of the inputs are displayed.          |
+-----------------------+------------+----------------------------------------------------------------------+
| Bake *                | operator   | bake text to blender geometry objects                                |
+-----------------------+------------+----------------------------------------------------------------------+
| Font Size *           | float      | size of baked text                                                   |
+-----------------------+------------+----------------------------------------------------------------------+
| Font  *               | string     | font used to bake (import fonts to scene if you wish to use anything |
|                       |            | other than BFont)                                                    |
+-----------------------+------------+----------------------------------------------------------------------+

`* - only used for baking text meshes, not 3dview printing`

In the *Properties Panel* (N-Panel) of this active node, it is possible to specifiy the colors of text and background polygons.

**extended**

+-----------------------+------------+----------------------------------------------------------------------+
| parameters            | type       | description                                                          |
+=======================+============+======================================================================+
| colors font           | color      | colors for vertices, edges, polygons                                 |
+-----------------------+------------+----------------------------------------------------------------------+
| colors background     | color      | colors for vertices, edges, polygons background                      |
+-----------------------+------------+----------------------------------------------------------------------+
| show bake UI          | bool       | reveals extended bake UI features (Bake button, font properties)     | 
+-----------------------+------------+----------------------------------------------------------------------+

We added a way to show extended features in the main Node UI. 

**font**

With *show bake UI* toggled, the Node unhides a selection of UI elements considered useful for *Baking Text* in preparation for fabrication. If no font is selected the default BFont will be used. BFont won't be visible in this list until you have done at least one bake during the current Blender session.

**Glyph to Geometry**

Font glyph conversion is done by Blender. If it produces strange results then most likely the font's Glyph contains *invsibile mistakes*. Blender's font parser takes no extra precautions to catch inconsistant Glyph definitions.

**Bake locations**

Depending on the toggle set in ``Verts | Edges | Faces``, the text can be shown and baked at various locations. 

+-------+-------------------------------------------------------------------+
| Mode  | Location                                                          | 
+=======+===================================================================+
| Verts | directly on the vertex location (adjusted if Matrix is input too) |
+-------+-------------------------------------------------------------------+
| Edges | in-between the two vertices of the edge                           | 
+-------+-------------------------------------------------------------------+
| Faces | the average location of all vertices associated with the polygon  |
+-------+-------------------------------------------------------------------+

**Orientation of baked text**

Currently only flat on the XY plane. ``Z = 0``


Outputs
-------

No socket output, but does output to 3d-view as either openGL drawing instructions or proper Meshes when Baking.

Examples
--------

.. image:: https://cloud.githubusercontent.com/assets/619340/4186492/6903ce82-3761-11e4-9359-ebf4b51827d1.PNG
  :alt: IndexViewerDemo1.PNG
.. image:: https://cloud.githubusercontent.com/assets/619340/4186493/6908b712-3761-11e4-8cfb-bd487469f7ed.PNG
  :alt: IndexViewerDemo2.PNG
.. image:: https://cloud.githubusercontent.com/assets/619340/4186494/6910e9f0-3761-11e4-9496-3dd62ab58352.PNG
  :alt: IndexViewerDemo3.PNG
