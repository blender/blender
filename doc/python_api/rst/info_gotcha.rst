
*******
Gotchas
*******

This document attempts to help you work with the Blender API in areas
that can be troublesome and avoid practices that are known to give instability.


.. _using_operators:

Using Operators
===============

Blender's operators are tools for users to access, that Python can access them too is very useful
nevertheless operators have limitations that can make them cumbersome to script.

Main limits are...

- Can't pass data such as objects, meshes or materials to operate on (operators use the context instead)
- The return value from calling an operator gives the success (if it finished or was canceled),
  in some cases it would be more logical from an API perspective to return the result of the operation.
- Operators poll function can fail where an API function would raise an exception giving details on exactly why.


Why does an operator's poll fail?
---------------------------------

When calling an operator gives an error like this:

   >>> bpy.ops.action.clean(threshold=0.001)
   RuntimeError: Operator bpy.ops.action.clean.poll() failed, context is incorrect

Which raises the question as to what the correct context might be?

Typically operators check for the active area type, a selection or active object they can operate on,
but some operators are more picky about when they run.

In most cases you can figure out what context an operator needs
simply be seeing how it's used in Blender and thinking about what it does.

Unfortunately if you're still stuck - the only way to **really** know
what's going on is to read the source code for the poll function and see what its checking.

For Python operators it's not so hard to find the source
since it's included with Blender and the source file/line is included in the operator reference docs.

Downloading and searching the C code isn't so simple,
especially if you're not familiar with the C language but by searching the
operator name or description you should be able to find the poll function with no knowledge of C.

.. note::

   Blender does have the functionality for poll functions to describe why they fail,
   but its currently not used much, if you're interested to help improve our API
   feel free to add calls to ``CTX_wm_operator_poll_msg_set`` where its not obvious why poll fails.

      >>> bpy.ops.gpencil.draw()
      RuntimeError: Operator bpy.ops.gpencil.draw.poll() Failed to find Grease Pencil data to draw into


The operator still doesn't work!
--------------------------------

Certain operators in Blender are only intended for use in a specific context,
some operators for example are only called from the properties window where they check the current material,
modifier or constraint.

Examples of this are:

- :mod:`bpy.ops.texture.slot_move`
- :mod:`bpy.ops.constraint.limitdistance_reset`
- :mod:`bpy.ops.object.modifier_copy`
- :mod:`bpy.ops.buttons.file_browse`

Another possibility is that you are the first person to attempt to use this operator
in a script and some modifications need to be made to the operator to run in a different context,
if the operator should logically be able to run but fails when accessed from a script
it should be reported to the bug tracker.


Stale Data
==========

No updates after setting values
-------------------------------

Sometimes you want to modify values from Python and immediately access the updated values, eg:

Once changing the objects :class:`bpy.types.Object.location`
you may want to access its transformation right after from :class:`bpy.types.Object.matrix_world`,
but this doesn't work as you might expect.

Consider the calculations that might go into working out the object's final transformation, this includes:

- animation function curves.
- drivers and their Python expressions.
- constraints
- parent objects and all of their f-curves, constraints etc.

To avoid expensive recalculations every time a property is modified,
Blender defers making the actual calculations until they are needed.

However, while the script runs you may want to access the updated values.
In this case you need to call :class:`bpy.types.ViewLayer.update` after modifying values, for example:

.. code-block:: python

   bpy.context.object.location = 1, 2, 3
   bpy.context.view_layer.update()


Now all dependent data (child objects, modifiers, drivers... etc)
has been recalculated and is available to the script within active view layer.


Can I redraw during the script?
-------------------------------

The official answer to this is no, or... *"You don't want to do that"*.

To give some background on the topic...

While a script executes Blender waits for it to finish and is effectively locked until its done,
while in this state Blender won't redraw or respond to user input.
Normally this is not such a problem because scripts distributed with Blender
tend not to run for an extended period of time,
nevertheless scripts *can* take ages to execute and its nice to see what's going on in the view port.

Tools that lock Blender in a loop and redraw are highly discouraged
since they conflict with Blenders ability to run multiple operators
at once and update different parts of the interface as the tool runs.

So the solution here is to write a **modal** operator, that is - an operator which defines a modal() function,
See the modal operator template in the text  editor.

Modal operators execute on user input or setup their own timers to run frequently,
they can handle the events or pass through to be handled by the keymap or other modal operators.

Transform, Painting, Fly-Mode and File-Select are example of a modal operators.

Writing modal operators takes more effort than a simple ``for`` loop
that happens to redraw but is more flexible and integrates better with Blenders design.


**Ok, Ok! I still want to draw from Python**

If you insist - yes its possible, but scripts that use this hack won't be considered
for inclusion in Blender and any issues with using it won't be considered bugs,
this is also not guaranteed to work in future releases.

.. code-block:: python

   bpy.ops.wm.redraw_timer(type='DRAW_WIN_SWAP', iterations=1)


Modes and Mesh Access
=====================

When working with mesh data you may run into the problem where a script fails to run as expected in edit-mode.
This is caused by edit-mode having its own data which is only written back to the mesh when exiting edit-mode.

A common example is that exporters may access a mesh through ``obj.data`` (a :class:`bpy.types.Mesh`)
but the user is in edit-mode, where the mesh data is available but out of sync with the edit mesh.

In this situation you can...

- Exit edit-mode before running the tool.
- Explicitly update the mesh by calling :class:`bmesh.types.BMesh.to_mesh`.
- Modify the script to support working on the edit-mode data directly, see: :mod:`bmesh.from_edit_mesh`.
- Report the context as incorrect and only allow the script to run outside edit-mode.


.. _info_gotcha_mesh_faces:

N-Gons and Tessellation
=======================

Since 2.63 NGons are supported, this adds some complexity
since in some cases you need to access triangles still (some exporters for example).

There are now 3 ways to access faces:

- :class:`bpy.types.MeshPolygon` -
  this is the data structure which now stores faces in object mode
  (access as ``mesh.polygons`` rather than ``mesh.faces``).
- :class:`bpy.types.MeshLoopTriangle` -
  the result of tessellating polygons into triangles
  (access as ``mesh.loop_triangles``).
- :class:`bmesh.types.BMFace` -
  the polygons as used in editmode.

For the purpose of the following documentation,
these will be referred to as polygons, loop triangles and bmesh-faces respectively.

5+ sided faces will be referred to as ``ngons``.


Support Overview
----------------

.. list-table::
   :header-rows: 1
   :stub-columns: 1

   * - Usage
     - :class:`bpy.types.MeshPolygon`
     - :class:`bpy.types.MeshTessFace`
     - :class:`bmesh.types.BMFace`
   * - Import/Create
     - Poor *(inflexible)*
     - Good *(supported as upgrade path)*
     - Best
   * - Manipulate
     - Poor *(inflexible)*
     - Poor *(loses ngons)*
     - Best
   * - Export/Output
     - Good *(ngon support)*
     - Good *(When ngons can't be used)*
     - Good *(ngons, extra memory overhead)*

.. note::

   Using the :mod:`bmesh` API is completely separate API from :mod:`bpy`,
   typically you would would use one or the other based on the level of editing needed,
   not simply for a different way to access faces.


Creating
--------

All 3 datatypes can be used for face creation.

- polygons are the most efficient way to create faces but the data structure is _very_ rigid and inflexible,
  you must have all your vertices and faces ready and create them all at once.
  This is further complicated by the fact that each polygon does not store its own verts,
  rather they reference an index and size in :class:`bpy.types.Mesh.loops` which are a fixed array too.
- bmesh-faces are most likely the easiest way for new scripts to create faces,
  since faces can be added one by one and the api has features intended for mesh manipulation.
  While :class:`bmesh.types.BMesh` uses more memory it can be managed by only operating on one mesh at a time.


Editing
-------

Editing is where the 3 data types vary most.

- Polygons are very limited for editing,
  changing materials and options like smooth works but for anything else
  they are too inflexible and are only intended for storage.
- Tessfaces should not be used for editing geometry because doing so will cause existing ngons to be tessellated.
- BMesh-Faces are by far the best way to manipulate geometry.


Exporting
---------

All 3 data types can be used for exporting,
the choice mostly depends on whether the target format supports ngons or not.

- Polygons are the most direct & efficient way to export providing they convert into the output format easily enough.
- Tessfaces work well for exporting to formats which don't support ngons,
  in fact this is the only place where their use is encouraged.
- BMesh-Faces can work for exporting too but may not be necessary if polygons can be used
  since using bmesh gives some overhead because its not the native storage format in object mode.


EditBones, PoseBones, Bone... Bones
===================================

Armature Bones in Blender have three distinct data structures that contain them.
If you are accessing the bones through one of them, you may not have access to the properties you really need.

.. note::

   In the following examples ``bpy.context.object`` is assumed to be an armature object.


Edit Bones
----------

``bpy.context.object.data.edit_bones`` contains a editbones;
to access them you must set the armature mode to edit mode first (editbones do not exist in object or pose mode).
Use these to create new bones, set their head/tail or roll, change their parenting relationships to other bones, etc.

Example using :class:`bpy.types.EditBone` in armature editmode:

This is only possible in edit mode.

   >>> bpy.context.object.data.edit_bones["Bone"].head = Vector((1.0, 2.0, 3.0))

This will be empty outside of editmode.

   >>> mybones = bpy.context.selected_editable_bones

Returns an editbone only in edit mode.

   >>> bpy.context.active_bone


Bones (Object Mode)
-------------------

``bpy.context.object.data.bones`` contains bones.
These *live* in object mode, and have various properties you can change,
note that the head and tail properties are read-only.

Example using :class:`bpy.types.Bone` in object or pose mode:

Returns a bone (not an editbone) outside of edit mode

   >>> bpy.context.active_bone

This works, as with blender the setting can be edited in any mode

   >>> bpy.context.object.data.bones["Bone"].use_deform = True

Accessible but read-only

   >>> tail = myobj.data.bones["Bone"].tail


Pose Bones
----------

``bpy.context.object.pose.bones`` contains pose bones.
This is where animation data resides, i.e. animatable transformations
are applied to pose bones, as are constraints and ik-settings.

Examples using :class:`bpy.types.PoseBone` in object or pose mode:

.. code-block:: python

   # Gets the name of the first constraint (if it exists)
   bpy.context.object.pose.bones["Bone"].constraints[0].name

   # Gets the last selected pose bone (pose mode only)
   bpy.context.active_pose_bone


.. note::

   Notice the pose is accessed from the object rather than the object data,
   this is why blender can have 2 or more objects sharing the same armature in different poses.

.. note::

   Strictly speaking PoseBone's are not bones, they are just the state of the armature,
   stored in the :class:`bpy.types.Object` rather than the :class:`bpy.types.Armature`,
   the real bones are however accessible from the pose bones - :class:`bpy.types.PoseBone.bone`


Armature Mode Switching
-----------------------

While writing scripts that deal with armatures you may find you have to switch between modes,
when doing so take care when switching out of edit-mode not to keep references
to the edit-bones or their head/tail vectors.
Further access to these will crash blender so its important the script
clearly separates sections of the code which operate in different modes.

This is mainly an issue with editmode since pose data can be manipulated without having to be in pose mode,
however for operator access you may still need to enter pose mode.


Data Names
==========


Naming Limitations
------------------

A common mistake is to assume newly created data is given the requested name.

This can cause bugs when you add some data (normally imported) then reference it later by name.

.. code-block:: python

   bpy.data.meshes.new(name=meshid)

   # normally some code, function calls...
   bpy.data.meshes[meshid]


Or with name assignment...

.. code-block:: python

   obj.name = objname

   # normally some code, function calls...
   obj = bpy.data.meshes[objname]


Data names may not match the assigned values if they exceed the maximum length, are already used or an empty string.


Its better practice not to reference objects by names at all,
once created you can store the data in a list, dictionary, on a class etc,
there is rarely a reason to have to keep searching for the same data by name.

If you do need to use name references, its best to use a dictionary to maintain
a mapping between the names of the imported assets and the newly created data,
this way you don't run this risk of referencing existing data from the blend file, or worse modifying it.

.. code-block:: python

   # typically declared in the main body of the function.
   mesh_name_mapping = {}

   mesh = bpy.data.meshes.new(name=meshid)
   mesh_name_mapping[meshid] = mesh

   # normally some code, or function calls...

   # use own dictionary rather than bpy.data
   mesh = mesh_name_mapping[meshid]


Library Collisions
------------------

Blender keeps data names unique - :class:`bpy.types.ID.name` so you can't name two objects,
meshes, scenes etc the same thing by accident.

However when linking in library data from another blend file naming collisions can occur,
so its best to avoid referencing data by name at all.

This can be tricky at times and not even blender handles this correctly in some case
(when selecting the modifier object for eg you can't select between multiple objects with the same name),
but its still good to try avoid problems in this area.


If you need to select between local and library data, there is a feature in ``bpy.data`` members to allow for this.

.. code-block:: python

   # typical name lookup, could be local or library.
   obj = bpy.data.objects["my_obj"]

   # library object name look up using a pair
   # where the second argument is the library path matching bpy.types.Library.filepath
   obj = bpy.data.objects["my_obj", "//my_lib.blend"]

   # local object name look up using a pair
   # where the second argument excludes library data from being returned.
   obj = bpy.data.objects["my_obj", None]

   # both the examples above also works for 'get'
   obj = bpy.data.objects.get(("my_obj", None))


Relative File Paths
===================

Blenders relative file paths are not compatible with standard Python modules such as ``sys`` and ``os``.

Built in Python functions don't understand blenders ``//`` prefix which denotes the blend file path.

A common case where you would run into this problem is when exporting a material with associated image paths.

   >>> bpy.path.abspath(image.filepath)


When using blender data from linked libraries there is an unfortunate complication
since the path will be relative to the library rather than the open blend file.
When the data block may be from an external blend file pass the library argument from the :class:`bpy.types.ID`.

   >>> bpy.path.abspath(image.filepath, library=image.library)


These returns the absolute path which can be used with native Python modules.


Unicode Problems
================

Python supports many different encodings so there is nothing stopping you from
writing a script in ``latin1`` or ``iso-8859-15``.

See `pep-0263 <https://www.python.org/dev/peps/pep-0263/>`_

However this complicates matters for Blender's Python API because ``.blend`` files don't have an explicit encoding.

To avoid the problem for Python integration and script authors we have decided all strings in blend files
**must** be ``UTF-8``, ``ASCII`` compatible.

This means assigning strings with different encodings to an object names for instance will raise an error.

Paths are an exception to this rule since we cannot ignore the existence of non ``UTF-8`` paths on users file-system.

This means seemingly harmless expressions can raise errors, eg.

   >>> print(bpy.data.filepath)
   UnicodeEncodeError: 'ascii' codec can't encode characters in position 10-21: ordinal not in range(128)

   >>> bpy.context.object.name = bpy.data.filepath
   Traceback (most recent call last):
     File "<blender_console>", line 1, in <module>
   TypeError: bpy_struct: item.attr= val: Object.name expected a string type, not str


Here are 2 ways around filesystem encoding issues:

   >>> print(repr(bpy.data.filepath))

   >>> import os
   >>> filepath_bytes = os.fsencode(bpy.data.filepath)
   >>> filepath_utf8 = filepath_bytes.decode('utf-8', "replace")
   >>> bpy.context.object.name = filepath_utf8


Unicode encoding/decoding is a big topic with comprehensive Python documentation,
to avoid getting stuck too deep in encoding problems - here are some suggestions:

- Always use utf-8 encoding or convert to utf-8 where the input is unknown.
- Avoid manipulating filepaths as strings directly, use ``os.path`` functions instead.
- Use ``os.fsencode()`` / ``os.fsdecode()`` instead of built in string decoding functions when operating on paths.
- To print paths or to include them in the user interface use ``repr(path)`` first
  or ``"%r" % path`` with string formatting.

.. note::

   Sometimes it's preferable to avoid string encoding issues by using bytes instead of Python strings,
   when reading some input its less trouble to read it as binary data
   though you will still need to decide how to treat any strings you want to use with Blender,
   some importers do this.


Strange errors using 'threading' module
=======================================

Python threading with Blender only works properly when the threads finish up before the script does.
By using ``threading.join()`` for example.

Here is an example of threading supported by Blender:

.. code-block:: python

   import threading
   import time

   def prod():
       print(threading.current_thread().name, "Starting")

       # do something vaguely useful
       import bpy
       from mathutils import Vector
       from random import random

       prod_vec = Vector((random() - 0.5, random() - 0.5, random() - 0.5))
       print("Prodding", prod_vec)
       bpy.data.objects["Cube"].location += prod_vec
       time.sleep(random() + 1.0)
       # finish

       print(threading.current_thread().name, "Exiting")

   threads = [threading.Thread(name="Prod %d" % i, target=prod) for i in range(10)]


   print("Starting threads...")

   for t in threads:
       t.start()

   print("Waiting for threads to finish...")

   for t in threads:
       t.join()


This an example of a timer which runs many times a second and moves
the default cube continuously while Blender runs **(Unsupported)**.

.. code-block:: python

   def func():
       print("Running...")
       import bpy
       bpy.data.objects['Cube'].location.x += 0.05

   def my_timer():
       from threading import Timer
       t = Timer(0.1, my_timer)
       t.start()
       func()

   my_timer()

Use cases like the one above which leave the thread running once the script finishes
may seem to work for a while but end up causing random crashes or errors in Blender's own drawing code.

So far, no work has gone into making Blender's Python integration thread safe,
so until its properly supported, best not make use of this.

.. note::

   Pythons threads only allow co-currency and won't speed up your scripts on multi-processor systems,
   the ``subprocess`` and ``multiprocess`` modules can be used with Blender and make use of multiple CPU's too.


Help! My script crashes Blender
===============================

**TL;DR:** Do not keep direct references to Blender data (of any kind) when modifying the container
of that data, and/or when some undo/redo may happen (e.g. during modal operators execution...).
Instead, use indices (or other data always stored by value in Python, like string keys...),
that allow you to get access to the desired data.

Ideally it would be impossible to crash Blender from Python
however there are some problems with the API where it can be made to crash.

Strictly speaking this is a bug in the API but fixing it would mean adding memory verification
on every access since most crashes are caused by the Python objects referencing Blenders memory directly,
whenever the memory is freed or re-allocated, further Python access to it can crash the script.
But fixing this would make the scripts run very slow,
or writing a very different kind of API which doesn't reference the memory directly.

Here are some general hints to avoid running into these problems.

- Be aware of memory limits,
  especially when working with large lists since Blender can crash simply by running out of memory.
- Many hard to fix crashes end up being because of referencing freed data,
  when removing data be sure not to hold any references to it.
- Re-allocation can lead to the same issues
  (e.g. if you add a lot of items to some Collection,
  this can lead to re-allocating the underlying container's memory,
  invalidating all previous references to existing items).
- Modules or classes that remain active while Blender is used,
  should not hold references to data the user may remove, instead,
  fetch data from the context each time the script is activated.
- Crashes may not happen every time, they may happen more on some configurations/operating-systems.
- Be wary of recursive patterns, those are very efficient at hiding the issues described here.
- See last sub-section about `Unfortunate Corner Cases`_ for some known breaking exceptions.

.. note::

   To find the line of your script that crashes you can use the ``faulthandler`` module.
   See the `faulthandler docs <https://docs.python.org/dev/library/faulthandler.html>`_.

   While the crash may be in Blenders C/C++ code,
   this can help a lot to track down the area of the script that causes the crash.

.. note::

   Some container modifications are actually safe, because they will never re-allocate existing data
   (e.g. linked lists containers will never re-allocate existing items when adding or removing others).

   But knowing which cases are safe and which aren't implies a deep understanding of Blender's internals.
   That's why, unless you are willing to dive into the RNA C implementation, it's simpler to
   always assume that data references will become invalid when modifying their containers,
   in any possible way.


**Don’t:**

.. code-block:: python

   class TestItems(bpy.types.PropertyGroup):
       name: bpy.props.StringProperty()

   bpy.utils.register_class(TestItems)
   bpy.types.Scene.test_items = bpy.props.CollectionProperty(type=TestItems)

   first_item = bpy.context.scene.test_items.add()
   for i in range(100):
       bpy.context.scene.test_items.add()

   # This is likely to crash, as internal code may re-allocate
   # the whole container (the collection) memory at some point.
   first_item.name = "foobar"


**Do:**

.. code-block:: python

   class TestItems(bpy.types.PropertyGroup):
       name: bpy.props.StringProperty()

   bpy.utils.register_class(TestItems)
   bpy.types.Scene.test_items = bpy.props.CollectionProperty(type=TestItems)

   first_item = bpy.context.scene.test_items.add()
   for i in range(100):
       bpy.context.scene.test_items.add()

   # This is safe, we are getting again desired data *after*
   # all modifications to its container are done.
   first_item = bpy.context.scene.test_items[0]
   first_item.name = "foobar"


Undo/Redo
---------

Undo invalidates all :class:`bpy.types.ID` instances (Object, Scene, Mesh, Lamp... etc).

This example shows how you can tell undo changes the memory locations.

   >>> hash(bpy.context.object)
   -9223372036849950810
   >>> hash(bpy.context.object)
   -9223372036849950810

   # ... move the active object, then undo

   >>> hash(bpy.context.object)
   -9223372036849951740

As suggested above, simply not holding references to data when Blender is used
interactively by the user is the only way to ensure the script doesn't become unstable.


Undo & Library Data
^^^^^^^^^^^^^^^^^^^

One of the advantages with Blenders library linking system that undo
can skip checking changes in library data since it is assumed to be static.

Tools in Blender are not allowed to modify library data.

Python however does not enforce this restriction.

This can be useful in some cases, using a script to adjust material values for example.
But its also possible to use a script to make library data point to newly created local data,
which is not supported since a call to undo will remove the local data
but leave the library referencing it and likely crash.

So it's best to consider modifying library data an advanced usage of the API
and only to use it when you know what you're doing.


Edit Mode / Memory Access
-------------------------

Switching edit-mode ``bpy.ops.object.mode_set(mode='EDIT')`` / ``bpy.ops.object.mode_set(mode='OBJECT')``
will re-allocate objects data,
any references to a meshes vertices/polygons/uvs, armatures bones,
curves points etc cannot be accessed after switching edit-mode.

Only the reference to the data its self can be re-accessed, the following example will crash.

.. code-block:: python

   mesh = bpy.context.active_object.data
   polygons = mesh.polygons
   bpy.ops.object.mode_set(mode='EDIT')
   bpy.ops.object.mode_set(mode='OBJECT')

   # this will crash
   print(polygons)


So after switching edit-mode you need to re-access any object data variables,
the following example shows how to avoid the crash above.

.. code-block:: python

   mesh = bpy.context.active_object.data
   polygons = mesh.polygons
   bpy.ops.object.mode_set(mode='EDIT')
   bpy.ops.object.mode_set(mode='OBJECT')

   # polygons have been re-allocated
   polygons = mesh.polygons
   print(polygons)


These kinds of problems can happen for any functions which re-allocate
the object data but are most common when switching edit-mode.


Array Re-Allocation
-------------------

When adding new points to a curve or vertices/edges/polygons to a mesh,
internally the array which stores this data is re-allocated.

.. code-block:: python

   bpy.ops.curve.primitive_bezier_curve_add()
   point = bpy.context.object.data.splines[0].bezier_points[0]
   bpy.context.object.data.splines[0].bezier_points.add()

   # this will crash!
   point.co = 1.0, 2.0, 3.0

This can be avoided by re-assigning the point variables after adding the new one or by storing
indices to the points rather than the points themselves.

The best way is to sidestep the problem altogether add all the points to the curve at once.
This means you don't have to worry about array re-allocation and its faster too
since reallocating the entire array for every point added is inefficient.


Removing Data
-------------

**Any** data that you remove shouldn't be modified or accessed afterwards,
this includes f-curves, drivers, render layers, timeline markers, modifiers, constraints
along with objects, scenes, collections, bones.. etc.

The ``remove()`` api calls will invalidate the data they free to prevent common mistakes.

The following example shows how this precaution works.

.. code-block:: python

   mesh = bpy.data.meshes.new(name="MyMesh")
   # normally the script would use the mesh here...
   bpy.data.meshes.remove(mesh)
   print(mesh.name)  # <- give an exception rather than crashing:

   # ReferenceError: StructRNA of type Mesh has been removed


But take care because this is limited to scripts accessing the variable which is removed,
the next example will still crash.

.. code-block:: python

   mesh = bpy.data.meshes.new(name="MyMesh")
   vertices = mesh.vertices
   bpy.data.meshes.remove(mesh)
   print(vertices)  # <- this may crash


Unfortunate Corner Cases
------------------------

Besides all expected cases listed above, there are a few others that should not be
an issue but, due to internal implementation details, currently are:

- ``Object.hide_viewport``, ``Object.hide_select`` and ``Object.hide_render``:
  Setting any of those booleans will trigger a rebuild of Collection caches, hence breaking
  any current iteration over ``Collection.all_objects``.


sys.exit
========

Some Python modules will call ``sys.exit()`` themselves when an error occurs,
while not common behavior this is something to watch out for because it may seem
as if Blender is crashing since ``sys.exit()`` will close Blender immediately.

For example, the ``argparse`` module will print an error and exit if the arguments are invalid.

An ugly way of troubleshooting this is to set ``sys.exit = None`` and see what line of Python code is quitting,
you could of course replace ``sys.exit`` with your own function but manipulating Python in this way is bad practice.
