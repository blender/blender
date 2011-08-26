########
Gotcha's
########

This document attempts to help you work with the Blender API in areas that can be troublesome and avoid practices that are known to give instability.

***************
Using Operators
***************

Blender's operators are tools for users to access, that python can access them too is very useful but does not change the fact that operators have limitations that can make them cumbersome to script.

Main limits are...

* Can't pass data such as objects, meshes or materials to operate on (operators use the context instead)

* The return value from calling an operator gives the success (if it finished or was canceled),
  in some cases it would be more logical from an API perspective to return the result of the operation.

* Operators poll function can fail where an API function would raise an exception giving details on exactly why.

=================================
Why does an operator's poll fail?
=================================

When calling an operator gives an error like this:

.. code-block:: python

   >>> bpy.ops.action.clean(threshold=0.001)
   Traceback (most recent call last):
     File "<blender_console>", line 1, in <module>
     File "scripts/modules/bpy/ops.py", line 179, in __call__
       ret = op_call(self.idname_py(), None, kw)
   RuntimeError: Operator bpy.ops.action.clean.poll() failed, context is incorrect

Which raises the question as to what the correct context might be?

Typically operators check for the active area type, a selection or active object they can operate on, but some operators are more picky about when they run.

In most cases you can figure out what context an operator needs simply be seeing how its used in Blender and thinking about what it does.


Unfortunately if you're still stuck - the only way to **really** know whats going on is to read the source code for the poll function and see what its checking.

For python operators its not so hard to find the source since its included with with Blender and the source file/line is included in the operator reference docs.

Downloading and searching the C code isn't so simple, especially if you're not familiar with the C language but by searching the operator name or description you should be able to find the poll function with no knowledge of C.

.. note::

   Blender does have the functionality for poll functions to describe why they fail, but its currently not used much, if you're interested to help improve our API feel free to add calls to ``CTX_wm_operator_poll_msg_set`` where its not obvious why poll fails.

   .. code-block:: python

      >>> bpy.ops.gpencil.draw()
      RuntimeError: Operator bpy.ops.gpencil.draw.poll() Failed to find Grease Pencil data to draw into

================================
The operator still doesn't work!
================================

Certain operators in Blender are only intended for use in a specific context, some operators for example are only called from the properties window where they check the current material, modifier or constraint.

Examples of this are:

* :mod:`bpy.ops.texture.slot_move`
* :mod:`bpy.ops.constraint.limitdistance_reset`
* :mod:`bpy.ops.object.modifier_copy`
* :mod:`bpy.ops.buttons.file_browse`

Another possibility is that you are the first person to attempt to use this operator in a script and some modifications need to be made to the operator to run in a different context, if the operator should logically be able to run but fails when accessed from a script it should be reported to the bug tracker.


**********
Stale Data
**********

===============================
No updates after setting values
===============================

Sometimes you want to modify values from python and immediately access the updated values, eg:

Once changing the objects :class:`Object.location` you may want to access its transformation right after from :class:`Object.matrix_world`, but this doesn't work as you might expect.

Consider the calculations that might go into working out the objects final transformation, this includes:

* animation function curves.
* drivers and their pythons expressions.
* constraints
* parent objects and all of their f-curves, constraints etc.

To avoid expensive recalculations every time a property is modified, Blender defers making the actual calculations until they are needed.

However, while the script runs you may want to access the updated values.

This can be done by calling :class:`bpy.types.Scene.update` after modifying values which recalculates all data that is tagged to be updated.

===============================
Can I redraw during the script?
===============================

The official answer to this is no, or... *"You don't want to do that"*.

To give some background on the topic...

While a script executes Blender waits for it to finish and is effectively locked until its done, while in this state Blender won't redraw or respond to user input.
Normally this is not such a problem because scripts distributed with Blender tend not to run for an extended period of time, nevertheless scripts *can* take ages to execute and its nice to see whats going on in the view port.

Tools that lock Blender in a loop and redraw are highly discouraged since they conflict with Blenders ability to run multiple operators at once and update different parts of the interface as the tool runs.

So the solution here is to write a **modal** operator, that is - an operator which defines a modal() function, See the modal operator template in the text  editor.

Modal operators execute on user input or setup their own timers to run frequently, they can handle the events or pass through to be handled by the keymap or other modal operators.

Transform, Painting, Fly-Mode and File-Select are example of a modal operators.

Writing modal operators takes more effort then a simple ``for`` loop that happens to redraw but is more flexible and integrates better with Blenders design.


**Ok, Ok! I still want to draw from python**

If you insist - yes its possible, but scripts that use this hack wont be considered for inclusion in Blender and any issues with using it wont be considered bugs, this is also not guaranteed to work in future releases.

.. code-block:: python

   bpy.ops.wm.redraw_timer(type='DRAW_WIN_SWAP', iterations=1)


***************************************
Strange errors using 'threading' module
***************************************

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

Use cases like the one above which leave the thread running once the script finishes may seem to work for a while but end up causing random crashes or errors in Blenders own drawing code.

So far no work has gone into making Blenders python integration thread safe, so until its properly supported, best not make use of this.

.. note::

   Pythons threads only allow co-currency and wont speed up you're scripts on multi-processor systems, the ``subprocess`` and ``multiprocess`` modules can be used with blender and make use of multiple CPU's too.


******************************
Matrix multiplication is wrong
******************************

Every so often we get complaints that Blenders matrix math is wrong, the confusion comes from mathutils matrices being column-major to match OpenGL and the rest of Blenders matrix operations and stored matrix data.

This is different to **numpy** which is row-major which matches what you would expect when using conventional matrix math notation.


***********************************
I can't edit the mesh in edit-mode!
***********************************

Blenders EditMesh is an internal data structure (not saved and not exposed to python), this gives the main annoyance that you need to exit edit-mode to edit the mesh from python.

The reason we have not made much attempt to fix this yet is because we
will likely move to BMesh mesh API eventually, so any work on the API now will be wasted effort.

With the BMesh API we may expose mesh data to python so we can
write useful tools in python which are also fast to execute while in edit-mode.

For the time being this limitation just has to be worked around but we're aware its frustrating needs to be addressed.


*******************************
Help! My script crashes Blender
*******************************

Ideally it would be impossible to crash Blender from python however there are some problems with the API where it can be made to crash.

Strictly speaking this is a bug in the API but fixing it would mean adding memory verification on every access since most crashes are caused by the python objects referencing Blenders memory directly, whenever the memory is freed, further python access to it can crash the script. But fixing this would make the scripts run very slow, or writing a very different kind of API which doesn't reference the memory directly.

Here are some general hints to avoid running into these problems.

* Be aware of memory limits, especially when working with large lists since Blender can crash simply by running out of memory.

* Many hard to fix crashes end up being because of referencing freed data, when removing data be sure not to hold any references to it.

* Modules or classes that remain active while Blender is used, should not hold references to data the user may remove, instead, fetch data from the context each time the script is activated.

* Crashes may not happen every time, they may happen more on some configurations/operating-systems.


=========
Undo/Redo
=========

Undo invalidates all :class:`bpy.types.ID` instances (Object, Scene, Mesh etc).

This example shows how you can tell undo changes the memory locations.

.. code-block:: python

   >>> hash(bpy.context.object)
   -9223372036849950810
   >>> hash(bpy.context.object)
   -9223372036849950810

   # ... move the active object, then undo

   >>> hash(bpy.context.object)
   -9223372036849951740

As suggested above, simply not holding references to data when Blender is used interactively by the user is the only way to ensure the script doesn't become unstable.


===================
Array Re-Allocation
===================

When adding new points to a curve or vertices's/edges/faces to a mesh, internally the array which stores this data is re-allocated.

.. code-block:: python

   bpy.ops.curve.primitive_bezier_curve_add()
   point = bpy.context.object.data.splines[0].bezier_points[0]
   bpy.context.object.data.splines[0].bezier_points.add()

   # this will crash!
   point.co = 1.0, 2.0, 3.0

This can be avoided by re-assigning the point variables after adding the new one or by storing indices's to the points rather then the points themselves.

The best way is to sidestep the problem altogether add all the points to the curve at once. This means you don't have to worry about array re-allocation and its faster too since reallocating the entire array for every point added is inefficient.


=============
Removing Data
=============

**Any** data that you remove shouldn't be modified or accessed afterwards, this includes f-curves, drivers, render layers, timeline markers, modifiers, constraints along with objects, scenes, groups, bones.. etc.

This is a problem in the API at the moment that we should eventually solve.
