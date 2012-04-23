..
   This document is appended to the auto generated bmesh api doc to avoid clogging up the C files with details.
   to test this run:
   ./blender.bin -b -noaudio -P doc/python_api/sphinx_doc_gen.py -- --partial bmesh* ; cd doc/python_api ; sphinx-build sphinx-in sphinx-out ; cd ../../


Intro
-----

This API gives access the blenders internal mesh editing api, featuring geometry connectivity data and
access to editing operations such as split, separate, collapse and dissolve.

The features exposed closely follow the C API,
giving python access to the functions used by blenders own mesh editing tools.

For an overview of BMesh data types and how they reference each other see:
`BMesh Design Document <http://wiki.blender.org/index.php/Dev:2.6/Source/Modeling/BMesh/Design>`_ .


.. note::

   **Disk** and **Radial** data is not exposed by the python api since this is for internal use only.


.. warning::

   This API is still in development and experimental, while we don't expect to see large changes,
   many areas are not well tested yet and so its possible changes will be made that break scripts.

   *Campbell Barton, 13, March 2012*


.. warning::

   TODO items are...

   * add access to BMesh **walkers**
   * add api for calling BMesh operators (unrelated to bpy.ops)
   * add custom-data manipulation functions add/remove/rename.

Example Script
--------------

.. literalinclude:: ../../../release/scripts/templates/bmesh_simple.py


Stand-Alone Module
^^^^^^^^^^^^^^^^^^

The bmesh module is written to be standalone except for :mod:`mathutils`
which is used for vertex locations and normals.

The only other exception to this are when converting mesh data to and from :class:`bpy.types.Mesh`.


Mesh Access
-----------

There are 2 ways to access BMesh data, you can create a new BMesh by converting a mesh from
:class:`bpy.types.BlendData.meshes` or by accessing the current edit mode mesh.
see: :class:`bmesh.types.BMesh.from_mesh` and :mod:`bmesh.from_edit_mesh` respectively.

When explicitly converting from mesh data python **owns** the data, that is to say - that the mesh only exists while
python holds a reference to it, and the script is responsible for putting it back into a mesh data-block when the edits
are done.

Note that unlike :mod:`bpy`, a BMesh does not necessarily correspond to data in the currently open blend file,
a BMesh can be created, edited and freed without the user ever seeing or having access to it.
Unlike edit mode, the bmesh module can use multiple BMesh instances at once.

Take care when dealing with multiple BMesh instances since the mesh data can use a lot of memory, while a mesh that
python owns will be freed in when the script holds no references to it,
its good practice to call :class:`bmesh.types.BMesh.free` which will remove all the mesh data immediately and disable
further access.


EditMode Tessellation
^^^^^^^^^^^^^^^^^^^^^

When writing scripts that operate on editmode data you will normally want to re-calculate the tessellation after
running the  script, this needs to be called explicitly.

The BMesh its self does not store the triangulated faces, they are stored in the :class:`bpy.types.Mesh`,
to refresh tessellation faces call :class:`bpy.types.Mesh.calc_tessface`.


CustomData Access
-----------------

BMesh has a unified way to access mesh attributes such as UV's vertex colors, shape keys, edge crease etc.

This works by having a **layers** property on bmesh data sequences to access the custom data layers which can then be
used to access the actual data on each vert/edge/face/loop.

Here are some examples ...

.. code-block:: python

   uv_lay = bm.loops.layers.uv.active

   for face in bm.faces:
       for loop in f.loops:
           uv = loop[uv_lay]
           print("Loop UV: %f, %f" % (uv.x, uv.y))


.. code-block:: python

   shape_lay = bm.verts.layers.shape["Key.001"]

   for vert in bm.verts:
       shape = vert[shape_lay]
       print("Vert Shape: %f, %f, %f" % (shape.x, shape.y, shape.z))


.. code-block:: python

   # in this example the active vertex group index is used,
   # this is stored in the object, not the BMesh
   group_index = obj.vertex_groups.active_index

   # only ever one deform weight layer
   dvert_lay = bm.verts.layers.deform.active

   for vert in bm.verts:
       dvert = vert[dvert_lay]

       if group_index in dvert:
           print("Weight %f" % dvert[group_index])
       else:
           print("Setting Weight")
           dvert[group_index] = 0.5


Keeping a Correct State
-----------------------

When modeling in blender there are certain assumptions made about the state of the mesh.

* hidden geometry isn't selected.
* when an edge is selected, its vertices are selected too.
* when a face is selected, its edges and vertices are selected.
* duplicate edges / faces don't exist.
* faces have at least 3 vertices.

To give developers flexibility these conventions are not enforced,
however tools must leave the mesh in a valid state else other tools may behave incorrectly.

Any errors that arise from not following these conventions is considered a bug in the script,
not a bug in blender.


Selection / Flushing
^^^^^^^^^^^^^^^^^^^^

As mentioned above, it is possible to create an invalid selection state
(by selecting a state and then de-selecting one of its vertices's for example), mostly the best way to solve this is to
flush the selection after performing a series of edits. this validates the selection state.


Module Functions
----------------
