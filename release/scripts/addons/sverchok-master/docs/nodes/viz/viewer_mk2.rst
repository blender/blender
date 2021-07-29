Viewer Draw MKII
================

Functionality
-------------

.. image:: https://cloud.githubusercontent.com/assets/619340/4320381/e0cd62b6-3f36-11e4-88ff-29eb165c72ad.png

Built on the core of the original ViewerDraw, this version allows all of the following features.

- Vertex Size (via N-Panel)
- Vertex Color
- Edge Width (via N-Panel)
- Edge Color
- Face shading: Flat vs Normal
- Vertex, Edges, Faces displays can be toggled.
- Defining Normal of Brigtness Source (via N-Panel)
- ``Faux Transparancy`` via dotted edges or checkered polygons.
- ``ngons tesselation`` (via N-Panel) - see description below 
- bake and bake all. (via N-Panel, show bake interface is not on by default)

**draws using display lists**

Uses OpenGL display list to cache the drawing function. This optimizes for rotating the viewport around static geometry. Changing the geometry clears the display cache, with big geometry inputs you may notice some lag on the initial draw + cache.

**ngons tesselation**

By default vdmk2 drawing routine fills all polygons using the standard ``GL_POLYGON``, which uses the triangle fan approach ( `see here <https://stackoverflow.com/a/8044252/1243487>`_ )

This is a fast way to draw large amounts of quads and triangles. This default (while faster) doesn't draw concave ngons correctly.

When enabled ``ngons tesselation`` will draw any Ngons using a slightly more involved but appropriate algorithm. The algorithm turns all ngons into individual triangles and fills them, edge drawing will be unchanged and still circumscribe the original polygon.

.. image:: https://user-images.githubusercontent.com/619340/30099972-9159fd40-92e7-11e7-9051-325011e6bec1.png

If you are always working on complex ngons, have a look at configuring defaults for this node.


Inputs
------

verts + edg_pol + matrices


Parameters
----------

Some info here.

+----------+--------------------------------------------------------------------------------------+
| Feature  | info                                                                                 |
+==========+======================================================================================+
| verts    | verts list or nested verts list.                                                     |
+----------+--------------------------------------------------------------------------------------+
| edge_pol | edge lists or polygon lists, if the first member of any atomic list has two keys,    |
|          | the rest of the list is considered edges. If it finds 3 keys it assumes Faces.       |
|          | Some of the slowness in the algorithm is down to actively preventing invalid key     |
|          | access if you accidentally mix edges+faces input.                                    |
+----------+--------------------------------------------------------------------------------------+
| matrices | matrices can multiply the incoming vert+edg_pol geometry. 1 set of vert+edges can be |
|          | turned into 20 'clones' by passing 20 matrices. See example                          |
+----------+--------------------------------------------------------------------------------------+



Outputs
-------

Directly to 3d view. Baking produces proper meshes and objects.


Examples
--------

development thread: `has examples <https://github.com/nortikin/sverchok/issues/401>`_

.. image:: https://cloud.githubusercontent.com/assets/619340/4265296/4c9c2fb4-3c48-11e4-8999-051c56511720.png


Notes
-----

**Tips on usage**

The viewer will stay responsive on larger geometry when you hide elements of the representation, especially while making updates to the geometry. If you don't need to see vertices, edges, or faces *hide them*. (how often do you need to see all topology when doing regular modeling?). If you see faces you can probably hide edges and verts. 

System specs will play a big role in how well this scripted BGL drawing performs. Don't expect miracles, but if you are conscious about what you feed the Node it should perform quite well given the circumstances.

