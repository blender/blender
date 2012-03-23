*******
Gotchas
*******

This document attempts to help you work with the Blender API in areas that can be troublesome and avoid practices that are known to give instability.


Using Operators
===============

Blender's operators are tools for users to access, that python can access them too is very useful nevertheless operators have limitations that can make them cumbersome to script.

Main limits are...

* Can't pass data such as objects, meshes or materials to operate on (operators use the context instead)

* The return value from calling an operator gives the success (if it finished or was canceled),
  in some cases it would be more logical from an API perspective to return the result of the operation.

* Operators poll function can fail where an API function would raise an exception giving details on exactly why.


Why does an operator's poll fail?
---------------------------------

When calling an operator gives an error like this:

   >>> bpy.ops.action.clean(threshold=0.001)
   RuntimeError: Operator bpy.ops.action.clean.poll() failed, context is incorrect

Which raises the question as to what the correct context might be?

Typically operators check for the active area type, a selection or active object they can operate on, but some operators are more picky about when they run.

In most cases you can figure out what context an operator needs simply be seeing how it's used in Blender and thinking about what it does.


Unfortunately if you're still stuck - the only way to **really** know whats going on is to read the source code for the poll function and see what its checking.

For python operators it's not so hard to find the source since it's included with Blender and the source file/line is included in the operator reference docs.

Downloading and searching the C code isn't so simple, especially if you're not familiar with the C language but by searching the operator name or description you should be able to find the poll function with no knowledge of C.

.. note::

   Blender does have the functionality for poll functions to describe why they fail, but its currently not used much, if you're interested to help improve our API feel free to add calls to ``CTX_wm_operator_poll_msg_set`` where its not obvious why poll fails.

      >>> bpy.ops.gpencil.draw()
      RuntimeError: Operator bpy.ops.gpencil.draw.poll() Failed to find Grease Pencil data to draw into


The operator still doesn't work!
--------------------------------

Certain operators in Blender are only intended for use in a specific context, some operators for example are only called from the properties window where they check the current material, modifier or constraint.

Examples of this are:

* :mod:`bpy.ops.texture.slot_move`
* :mod:`bpy.ops.constraint.limitdistance_reset`
* :mod:`bpy.ops.object.modifier_copy`
* :mod:`bpy.ops.buttons.file_browse`

Another possibility is that you are the first person to attempt to use this operator in a script and some modifications need to be made to the operator to run in a different context, if the operator should logically be able to run but fails when accessed from a script it should be reported to the bug tracker.


Stale Data
==========

No updates after setting values
-------------------------------

Sometimes you want to modify values from python and immediately access the updated values, eg:

Once changing the objects :class:`bpy.types.Object.location` you may want to access its transformation right after from :class:`bpy.types.Object.matrix_world`, but this doesn't work as you might expect.

Consider the calculations that might go into working out the object's final transformation, this includes:

* animation function curves.
* drivers and their pythons expressions.
* constraints
* parent objects and all of their f-curves, constraints etc.

To avoid expensive recalculations every time a property is modified, Blender defers making the actual calculations until they are needed.

However, while the script runs you may want to access the updated values.

This can be done by calling :class:`bpy.types.Scene.update` after modifying values which recalculates all data that is tagged to be updated.


Can I redraw during the script?
-------------------------------

The official answer to this is no, or... *"You don't want to do that"*.

To give some background on the topic...

While a script executes Blender waits for it to finish and is effectively locked until its done, while in this state Blender won't redraw or respond to user input.
Normally this is not such a problem because scripts distributed with Blender tend not to run for an extended period of time, nevertheless scripts *can* take ages to execute and its nice to see whats going on in the view port.

Tools that lock Blender in a loop and redraw are highly discouraged since they conflict with Blenders ability to run multiple operators at once and update different parts of the interface as the tool runs.

So the solution here is to write a **modal** operator, that is - an operator which defines a modal() function, See the modal operator template in the text  editor.

Modal operators execute on user input or setup their own timers to run frequently, they can handle the events or pass through to be handled by the keymap or other modal operators.

Transform, Painting, Fly-Mode and File-Select are example of a modal operators.

Writing modal operators takes more effort than a simple ``for`` loop that happens to redraw but is more flexible and integrates better with Blenders design.


**Ok, Ok! I still want to draw from python**

If you insist - yes its possible, but scripts that use this hack wont be considered for inclusion in Blender and any issues with using it wont be considered bugs, this is also not guaranteed to work in future releases.

.. code-block:: python

   bpy.ops.wm.redraw_timer(type='DRAW_WIN_SWAP', iterations=1)


I can't edit the mesh in edit-mode!
===================================

Blender's EditMesh is an internal data structure (not saved and not exposed to python), this gives the main annoyance that you need to exit edit-mode to edit the mesh from python.

The reason we have not made much attempt to fix this yet is because we
will likely move to BMesh mesh API eventually, so any work on the API now will be wasted effort.

With the BMesh API we may expose mesh data to python so we can
write useful tools in python which are also fast to execute while in edit-mode.

For the time being this limitation just has to be worked around but we're aware its frustrating needs to be addressed.


NGons and Tessellation Faces
============================

Since 2.63 NGons are supported, this adds some complexity since in some cases you need to access triangles still (some exporters for example).

There are now 3 ways to access faces:

* :class:`bpy.types.MeshPolygon` - this is the data stricture which now stores faces in object mode (access as ``mesh.polygons`` rather then ``mesh.faces``).
* :class:`bpy.types.MeshTessFace` - the result of triangulating (tessellated) polygons, the main method of face access in 2.62 or older (access as ``mesh.tessfaces``).
* :class:`bmesh.types.BMFace` - the polygons as used in editmode.

For the purpose of the following documentation, these will be referred to as polygons, tessfaces and bmesh-faces respectively.

5+ sided faces will be referred to as ``ngons``.

Support Overview
----------------

+--------------+------------------------------+--------------------------------+--------------------------------+
|Usage         |:class:`bpy.types.MeshPolygon`|:class:`bpy.types.MeshTessFace` |:class:`bmesh.types.BMFace`     |
+==============+==============================+================================+================================+
|Import/Create |Bad (inflexible)              |Fine (supported as upgrade path)|Best                            |
+--------------+------------------------------+--------------------------------+--------------------------------+
|Manipulate    |Bad (inflexible)              |Bad (loses ngons)               |Best                            |
+--------------+------------------------------+--------------------------------+--------------------------------+
|Export/Output |Good (ngons)                  |Good (When ngons can't be used) |Good (ngons, memory overhead)   |
+--------------+------------------------------+--------------------------------+--------------------------------+


.. note::

   Using the :mod:`bmesh` api is completely separate api from :mod:`bpy`, typically you would would use one or the other based on the level of editing needed, not simply for a different way to access faces.


Creating
--------

All 3 datatypes can be used for face creation.

* polygons are the most efficient way to create faces but the data structure is _very_ rigid and inflexible, you must have all your vertes and faces ready and create them all at once. This is further complicated by the fact that each polygon does not store its own verts (as with tessfaces), rather they reference an index and size in :class:`bpy.types.Mesh.loops` which are a fixed array too.
* tessfaces ideally should not be used for creating faces since they are really only tessellation cache of polygons, however for scripts upgrading from 2.62 this is by far the most straightforward option. This works by creating tessfaces and when finished - they can be converted into polygons by calling :class:`bpy.types.Mesh.update`. The obvious limitation is ngons can't be created this way.
* bmesh-faces are most likely the easiest way for new scripts to create faces, since faces can be added one by one and the api has features intended for mesh manipulation. While :class:`bmesh.types.BMesh` uses more memory it can be managed by only operating on one mesh at a time.


Editing
-------

Editing is where the 3 data types vary most.

* polygons are very limited for editing, changing materials and options like smooth works but for anything else they are too inflexible and are only intended for storage.
* tessfaces should not be used for editing geometry because doing so will cause existing ngons to be tessellated.
* bmesh-faces are by far the best way to manipulate geometry.

Exporting
---------

All 3 data types can be used for exporting, the choice mostly depends on whether the target format supports ngons or not.

* polygons are the most direct & efficient way to export providing they convert into the output format easily enough.
* tessfaces work well for exporting to formats which dont support ngons, in fact this is the only place where their use is encouraged.
* bmesh-faces can work for exporting too but may not be necessary if polygons can be used since using bmesh gives some overhead because its not the native storage format in object mode.


Upgrading Importers from 2.62
-----------------------------

Importers can be upgraded to work with only minor changes.

The main change to be made is used the tessellation versions of each attribute.

* mesh.faces --> :class:`bpy.types.Mesh.tessfaces`
* mesh.uv_textures --> :class:`bpy.types.Mesh.tessface_uv_textures`
* mesh.vertex_colors --> :class:`bpy.types.Mesh.tessface_vertex_colors`

Once the data is created call :class:`bpy.types.Mesh.update` to convert the tessfaces into polygons.


Upgrading Exporters from 2.62
-----------------------------

For exporters the most direct way to upgrade is to use tessfaces as with importing however its important to know that tessfaces may **not** exist for a mesh, the array will be empty as if there are no faces.

So before accessing tessface data call: :class:`bpy.types.Mesh.update` ``(calc_tessface=True)``.


EditBones, PoseBones, Bone... Bones
===================================

Armature Bones in Blender have three distinct data structures that contain them. If you are accessing the bones through one of them, you may not have access to the properties you really need.

.. note::

   In the following examples ``bpy.context.object`` is assumed to be an armature object.


Edit Bones
----------

``bpy.context.object.data.edit_bones`` contains a editbones; to access them you must set the armature mode to edit mode first (editbones do not exist in object or pose mode). Use these to create new bones, set their head/tail or roll, change their parenting relationships to other bones, etc.

Example using :class:`bpy.types.EditBone` in armature editmode:

This is only possible in edit mode.

   >>> bpy.context.object.data.edit_bones["Bone"].head = Vector((1.0, 2.0, 3.0)) 

This will be empty outside of editmode.

   >>> mybones = bpy.context.selected_editable_bones

Returns an editbone only in edit mode.

   >>> bpy.context.active_bone


Bones (Object Mode)
-------------------

``bpy.context.object.data.bones`` contains bones. These *live* in object mode, and have various properties you can change, note that the head and tail properties are read-only.

Example using :class:`bpy.types.Bone` in object or pose mode:

Returns a bone (not an editbone) outside of edit mode

   >>> bpy.context.active_bone

This works, as with blender the setting can be edited in any mode

   >>> bpy.context.object.data.bones["Bone"].use_deform = True

Accessible but read-only

   >>> tail = myobj.data.bones["Bone"].tail


Pose Bones
----------

``bpy.context.object.pose.bones`` contains pose bones. This is where animation data resides, i.e. animatable transformations are applied to pose bones, as are constraints and ik-settings.

Examples using :class:`bpy.types.PoseBone` in object or pose mode:

.. code-block:: python

   # Gets the name of the first constraint (if it exists)
   bpy.context.object.pose.bones["Bone"].constraints[0].name 

   # Gets the last selected pose bone (pose mode only)
   bpy.context.active_pose_bone


.. note::

   Notice the pose is accessed from the object rather than the object data, this is why blender can have 2 or more objects sharing the same armature in different poses.

.. note::

   Strictly speaking PoseBone's are not bones, they are just the state of the armature, stored in the :class:`bpy.types.Object` rather than the :class:`bpy.types.Armature`, the real bones are however accessible from the pose bones - :class:`bpy.types.PoseBone.bone`


Armature Mode Switching
-----------------------

While writing scripts that deal with armatures you may find you have to switch between modes, when doing so take care when switching out of editmode not to keep references to the edit-bones or their head/tail vectors. Further access to these will crash blender so its important the script clearly separates sections of the code which operate in different modes.

This is mainly an issue with editmode since pose data can be manipulated without having to be in pose mode, however for operator access you may still need to enter pose mode.


Data Names
==========


Naming Limitations
------------------

A common mistake is to assume newly created data is given the requested name.

This can cause bugs when you add some data (normally imported) and then reference it later by name.

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


Its better practice not to reference objects by names at all, once created you can store the data in a list, dictionary, on a class etc, there is rarely a reason to have to keep searching for the same data by name.


If you do need to use name references, its best to use a dictionary to maintain a mapping between the names of the imported assets and the newly created data, this way you don't run this risk of referencing existing data from the blend file, or worse modifying it.

.. code-block:: python

   # typically declared in the main body of the function.
   mesh_name_mapping = {}
   
   mesh = bpy.data.meshes.new(name=meshid)
   mesh_name_mapping[meshid] = mesh
   
   # normally some code, or function calls...
   
   # use own dictionary rather then bpy.data
   mesh = mesh_name_mapping[meshid]


Library Collisions
------------------

Blender keeps data names unique - :class:`bpy.types.ID.name` so you can't name two objects, meshes, scenes etc the same thing by accident.

However when linking in library data from another blend file naming collisions can occur, so its best to avoid referencing data by name at all.

This can be tricky at times and not even blender handles this correctly in some case (when selecting the modifier object for eg you can't select between multiple objects with the same name), but its still good to try avoid problems in this area.


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

Blenders relative file paths are not compatible with standard python modules such as ``sys`` and ``os``.

Built in python functions don't understand blenders ``//`` prefix which denotes the blend file path.

A common case where you would run into this problem is when exporting a material with associated image paths.

>>> bpy.path.abspath(image.filepath)


When using blender data from linked libraries there is an unfortunate complication since the path will be relative to the library rather then the open blend file. When the data block may be from an external blend file pass the library argument from the :class:`bpy.types.ID`.

>>> bpy.path.abspath(image.filepath, library=image.library)


These returns the absolute path which can be used with native python modules.


Unicode Problems
================

Python supports many different encodings so there is nothing stopping you from writing a script in latin1 or iso-8859-15.

See `pep-0263 <http://www.python.org/dev/peps/pep-0263/>`_

However this complicates things for the python api because blend files themselves don't have an encoding.

To simplify the problem for python integration and script authors we have decided all strings in blend files **must** be UTF-8 or ASCII compatible.

This means assigning strings with different encodings to an object names for instance will raise an error.

Paths are an exception to this rule since we cannot ignore the existane of non-utf-8 paths on peoples filesystems.

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


Unicode encoding/decoding is a big topic with comprehensive python documentation, to avoid getting stuck too deep in encoding problems - here are some suggestions:

* Always use utf-8 encoiding or convert to utf-8 where the input is unknown.

* Avoid manipulating filepaths as strings directly, use ``os.path`` functions instead.

* Use ``os.fsencode()`` / ``os.fsdecode()`` rather then the built in string decoding functions when operating on paths.

* To print paths or to include them in the user interface use ``repr(path)`` first or ``"%r" % path`` with string formatting.

* **Possibly** - use bytes instead of python strings, when reading some input its less trouble to read it as binary data though you will still need to decide how to treat any strings you want to use with Blender, some importers do this.


Strange errors using 'threading' module
=======================================

Python threading with Blender only works properly when the threads finish up before the script does. By using ``threading.join()`` for example.

Heres an example of threading supported by Blender:

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


This an example of a timer which runs many times a second and moves the default cube continuously while Blender runs (Unsupported).

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

Use cases like the one above which leave the thread running once the script finishes may seem to work for a while but end up causing random crashes or errors in Blender's own drawing code.

So far, no work has gone into making Blender's python integration thread safe, so until its properly supported, best not make use of this.

.. note::

   Pythons threads only allow co-currency and won't speed up your scripts on multi-processor systems, the ``subprocess`` and ``multiprocess`` modules can be used with blender and make use of multiple CPU's too.


Help! My script crashes Blender
===============================

Ideally it would be impossible to crash Blender from python however there are some problems with the API where it can be made to crash.

Strictly speaking this is a bug in the API but fixing it would mean adding memory verification on every access since most crashes are caused by the python objects referencing Blenders memory directly, whenever the memory is freed, further python access to it can crash the script. But fixing this would make the scripts run very slow, or writing a very different kind of API which doesn't reference the memory directly.

Here are some general hints to avoid running into these problems.

* Be aware of memory limits, especially when working with large lists since Blender can crash simply by running out of memory.

* Many hard to fix crashes end up being because of referencing freed data, when removing data be sure not to hold any references to it.

* Modules or classes that remain active while Blender is used, should not hold references to data the user may remove, instead, fetch data from the context each time the script is activated.

* Crashes may not happen every time, they may happen more on some configurations/operating-systems.


Undo/Redo
---------

Undo invalidates all :class:`bpy.types.ID` instances (Object, Scene, Mesh etc).

This example shows how you can tell undo changes the memory locations.

   >>> hash(bpy.context.object)
   -9223372036849950810
   >>> hash(bpy.context.object)
   -9223372036849950810

   # ... move the active object, then undo

   >>> hash(bpy.context.object)
   -9223372036849951740

As suggested above, simply not holding references to data when Blender is used interactively by the user is the only way to ensure the script doesn't become unstable.


Edit Mode / Memory Access
-------------------------

Switching edit-mode ``bpy.ops.object.mode_set(mode='EDIT')`` / ``bpy.ops.object.mode_set(mode='OBJECT')`` will re-allocate objects data, any references to a meshes vertices/faces/uvs, armatures bones, curves points etc cannot be accessed after switching edit-mode.

Only the reference to the data its self can be re-accessed, the following example will crash.

.. code-block:: python

   mesh = bpy.context.active_object.data
   faces = mesh.faces
   bpy.ops.object.mode_set(mode='EDIT')
   bpy.ops.object.mode_set(mode='OBJECT')

   # this will crash
   print(faces)


So after switching edit-mode you need to re-access any object data variables, the following example shows how to avoid the crash above.

.. code-block:: python

   mesh = bpy.context.active_object.data
   faces = mesh.faces
   bpy.ops.object.mode_set(mode='EDIT')
   bpy.ops.object.mode_set(mode='OBJECT')

   # faces have been re-allocated
   faces = mesh.faces
   print(faces)


These kinds of problems can happen for any functions which re-allocate the object data but are most common when switching edit-mode.


Array Re-Allocation
-------------------

When adding new points to a curve or vertices's/edges/faces to a mesh, internally the array which stores this data is re-allocated.

.. code-block:: python

   bpy.ops.curve.primitive_bezier_curve_add()
   point = bpy.context.object.data.splines[0].bezier_points[0]
   bpy.context.object.data.splines[0].bezier_points.add()

   # this will crash!
   point.co = 1.0, 2.0, 3.0

This can be avoided by re-assigning the point variables after adding the new one or by storing indices's to the points rather then the points themselves.

The best way is to sidestep the problem altogether add all the points to the curve at once. This means you don't have to worry about array re-allocation and its faster too since reallocating the entire array for every point added is inefficient.


Removing Data
-------------

**Any** data that you remove shouldn't be modified or accessed afterwards, this includes f-curves, drivers, render layers, timeline markers, modifiers, constraints along with objects, scenes, groups, bones.. etc.

This is a problem in the API at the moment that we should eventually solve.


sys.exit
========

Some python modules will call sys.exit() themselves when an error occurs, while not common behavior this is something to watch out for because it may seem as if blender is crashing since sys.exit() will quit blender immediately.

For example, the ``optparse`` module will print an error and exit if the arguments are invalid.

An ugly way of troubleshooting this is to set ``sys.exit = None`` and see what line of python code is quitting, you could of course replace ``sys.exit``/ with your own function but manipulating python in this way is bad practice.

