
Addon Tutorial
##############

************
Introduction
************


Intended Audience
=================

This tutorial is designed to help technical artists or developers learn to extend Blender.
An understanding of the basics of Python is expected for those working through this tutorial.


Prerequisites
-------------

Before going through the tutorial you should...

* Familiarity with the basics of working in Blender.

* Know how to run a script in Blender's text editor (as documented in the quick-start)

* Have an understanding of Python primitive types (int, boolean, string, list, tuple, dictionary, and set).

* Be familiar with the concept of Python modules.

* Basic understanding of classes (object orientation) in Python.


Suggested reading before starting this tutorial.

* `Dive Into Python <http://getpython3.com/diveintopython3/index.html>`_ sections (1, 2, 3, 4, and 7).
* :ref:`Blender API Quickstart <info_quickstart>`
  to help become familiar with Blender/Python basics.


To best troubleshoot any error message Python prints while writing scripts you run blender with from a terminal,
see :ref:`Use The Terminal <use_the_terminal>`.

Documentation Links
===================

While going through the tutorial you may want to look into our reference documentation.

* :ref:`Blender API Overview <info_overview>`. -
  *This document is rather detailed but helpful if you want to know more on a topic.*

* :mod:`bpy.context` api reference. -
  *Handy to have a list of available items your script may operate on.*

* :class:`bpy.types.Operator`. -
  *The following addons define operators, these docs give details and more examples of operators.*


******
Addons
******


What is an Addon?
=================

An addon is simply a Python module with some additional requirements so Blender can display it in a list with useful
information.

To give an example, here is the simplest possible addon.


.. code-block:: python

   bl_info = {"name": "My Test Addon", "category": "Object"}
   def register():
       print("Hello World")
   def unregister():
       print("Goodbye World")


* ``bl_info`` is a dictionary containing addon meta-data such as the title, version and author to be displayed in the
  user preferences addon list.
* ``register`` is a function which only runs when enabling the addon, this means the module can be loaded without
  activating the addon.
* ``unregister`` is a function to unload anything setup by ``register``, this is called when the addon is disabled.



Notice this addon does not do anything related to Blender, (the :mod:`bpy` module is not imported for example).

This is a contrived example of an addon that serves to illustrate the point
that the base requirements of an addon are simple.

An addon will typically register operators, panels, menu items etc, but its worth noting that _any_ script can do this,
when executed from the text editor or even the interactive console - there is nothing inherently different about an
addon that allows it to integrate with Blender, such functionality is just provided by the :mod:`bpy` module for any
script to access.

So an addon is just a way to encapsulate a Python module in a way a user can easily utilize.

.. note::

   Running this script within the text editor won't print anything,
   to see the output it must be installed through the user preferences.
   Messages will be printed when enabling and disabling.


Your First Addon
================

The simplest possible addon above was useful as an example but not much else.
This next addon is simple but shows how to integrate a script into Blender using an ``Operator``
which is the typical way to define a tool accessed from menus, buttons and keyboard shortcuts.

For the first example we'll make a script that simply moves all objects in a scene.


Write The Script
----------------

Add the following script to the text editor in Blender.

.. code-block:: python

   import bpy

   scene = bpy.context.scene
   for obj in scene.objects:
       obj.location.x += 1.0


.. image:: run_script.png
   :width: 924px
   :align: center
   :height: 574px
   :alt: Run Script button

Click the Run Script button, all objects in the active scene are moved by 1.0 Blender unit.
Next we'll make this script into an addon.


Write the Addon (Simple)
------------------------

This addon takes the body of the script above, and adds them to an operator's ``execute()`` function.


.. code-block:: python

   bl_info = {
       "name": "Move X Axis",
       "category": "Object",
   }

   import bpy


   class ObjectMoveX(bpy.types.Operator):
       """My Object Moving Script"""      # blender will use this as a tooltip for menu items and buttons.
       bl_idname = "object.move_x"        # unique identifier for buttons and menu items to reference.
       bl_label = "Move X by One"         # display name in the interface.
       bl_options = {'REGISTER', 'UNDO'}  # enable undo for the operator.

       def execute(self, context):        # execute() is called by blender when running the operator.

           # The original script
           scene = context.scene
           for obj in scene.objects:
               obj.location.x += 1.0

           return {'FINISHED'}            # this lets blender know the operator finished successfully.

   def register():
       bpy.utils.register_class(ObjectMoveX)


   def unregister():
       bpy.utils.unregister_class(ObjectMoveX)


   # This allows you to run the script directly from blenders text editor
   # to test the addon without having to install it.
   if __name__ == "__main__":
       register()


.. note:: ``bl_info`` is split across multiple lines, this is just a style convention used to more easily add items.

.. note:: Rather than using ``bpy.context.scene``, we use the ``context.scene`` argument passed to ``execute()``.
          In most cases these will be the same however in some cases operators will be passed a custom context
          so script authors should prefer the ``context`` argument passed to operators.
   

To test the script you can copy and paste this into Blender text editor and run it, this will execute the script
directly and call register immediately.

However running the script wont move any objects, for this you need to execute the newly registered operator.

.. image:: spacebar.png
   :width: 924px
   :align: center
   :height: 574px
   :alt: Spacebar

Do this by pressing ``SpaceBar`` to bring up the operator search dialog and type in "Move X by One" (the ``bl_label``),
then press ``Enter``.



The objects should move as before.

*Keep this addon open in Blender for the next step - Installing.*

Install The Addon
-----------------

Once you have your addon within in Blender's text editor, you will want to be able to install it so it can be enabled in
the user preferences to load on startup.

Even though the addon above is a test, lets go through the steps anyway so you know how to do it for later.

To install the Blender text as an addon you will first have to save it to disk, take care to obey the naming
restrictions that apply to Python modules and end with a ``.py`` extension.

Once the file is on disk, you can install it as you would for an addon downloaded online.

Open the user **File -> User Preferences**, Select the **Addon** section, press **Install Addon...** and select the file. 

Now the addon will be listed and you can enable it by pressing the check-box, if you want it to be enabled on restart,
press **Save as Default**.

.. note::

   The destination of the addon depends on your Blender configuration.
   When installing an addon the source and destination path are printed in the console.
   You can also find addon path locations by running this in the Python console.

   .. code-block:: python

      import addon_utils
      print(addon_utils.paths())

   More is written on this topic here:
   `Directory Layout <http://wiki.blender.org/index.php/Doc:2.6/Manual/Introduction/Installing_Blender/DirectoryLayout>`_


Your Second Addon
=================

For our second addon, we will focus on object instancing - this is - to make linked copies of an object in a
similar way to what you may have seen with the array modifier.


Write The Script
----------------

As before, first we will start with a script, develop it, then convert into an addon.

.. code-block:: python

   import bpy
   from bpy import context

   # Get the current scene
   scene = context.scene

   # Get the 3D cursor
   cursor = scene.cursor_location

   # Get the active object (assume we have one)
   obj = scene.objects.active

   # Now make a copy of the object
   obj_new = obj.copy()

   # The object won't automatically get into a new scene
   scene.objects.link(obj_new)

   # Now we can place the object
   obj_new.location = cursor


Now try copy this script into Blender and run it on the default cube.
Make sure you click to move the 3D cursor before running as the duplicate will appear at the cursor's location.


... go off and test ...


After running, notice that when you go into edit-mode to change the cube - all of the copies change,
in Blender this is known as *Linked-Duplicates*.


Next, we're going to do this in a loop, to make an array of objects between the active object and the cursor.


.. code-block:: python

   import bpy
   from bpy import context

   scene = context.scene
   cursor = scene.cursor_location
   obj = scene.objects.active

   # Use a fixed value for now, eventually make this user adjustable
   total = 10

   # Add 'total' objects into the scene
   for i in range(total):
       obj_new = obj.copy()
       scene.objects.link(obj_new)

       # Now place the object in between the cursor
       # and the active object based on 'i'
       factor = i / total
       obj_new.location = (obj.location * factor) + (cursor * (1.0 - factor))


Try run this script with with the active object and the cursor spaced apart to see the result.

With this script you'll notice we're doing some math with the object location and cursor, this works because both are
3D :class:`mathutils.Vector` instances, a convenient class provided by the :mod:`mathutils` module and
allows vectors to be multiplied by numbers and matrices.

If you are interested in this area, read into :class:`mathutils.Vector` - there are many handy utility functions
such as getting the angle between vectors, cross product, dot products
as well as more advanced functions in :mod:`mathutils.geometry` such as bezier spline interpolation and
ray-triangle intersection.

For now we'll focus on making this script an addon, but its good to know that this 3D math module is available and
can help you with more advanced functionality later on.


Write the Addon
---------------

The first step is to convert the script as-is into an addon.


.. code-block:: python

   bl_info = {
       "name": "Cursor Array",
       "category": "Object",
   }

   import bpy


   class ObjectCursorArray(bpy.types.Operator):
       """Object Cursor Array"""
       bl_idname = "object.cursor_array"
       bl_label = "Cursor Array"
       bl_options = {'REGISTER', 'UNDO'}

       def execute(self, context):
           scene = context.scene
           cursor = scene.cursor_location
           obj = scene.objects.active

           total = 10

           for i in range(total):
               obj_new = obj.copy()
               scene.objects.link(obj_new)

               factor = i / total
               obj_new.location = (obj.location * factor) + (cursor * (1.0 - factor))

           return {'FINISHED'}

   def register():
       bpy.utils.register_class(ObjectCursorArray)


   def unregister():
       bpy.utils.unregister_class(ObjectCursorArray)


   if __name__ == "__main__":
       register()


Everything here has been covered in the previous steps, you may want to try run the addon still
and consider what could be done to make it more useful.


... go off and test ...


The two of the most obvious missing things are - having the total fixed at 10, and having to access the operator from
space-bar is not very convenient.

Both these additions are explained next, with the final script afterwards.


Operator Property
^^^^^^^^^^^^^^^^^

There are a variety of property types that are used for tool settings, common property types include:
int, float, vector, color, boolean and string.

These properties are handled differently to typical Python class attributes
because Blender needs to be display them in the interface,
store their settings in key-maps and keep settings for re-use.

While this is handled in a fairly Pythonic way, be mindful that you are in fact defining tool settings that
are loaded into Blender and accessed by other parts of Blender, outside of Python.


To get rid of the literal 10 for `total`, we'll us an operator property.
Operator properties are defined via bpy.props module, this is added to the class body.

.. code-block:: python

   # moved assignment from execute() to the body of the class...
   total = bpy.props.IntProperty(name="Steps", default=2, min=1, max=100)

   # and this is accessed on the class
   # instance within the execute() function as...
   self.total


These properties from :mod:`bpy.props` are handled specially by Blender when the class is registered
so they display as buttons in the user interface.
There are many arguments you can pass to properties to set limits, change the default and display a tooltip.

.. seealso:: :mod:`bpy.props.IntProperty`

This document doesn't go into details about using other property types,
however the link above includes examples of more advanced property usage.


Menu Item
^^^^^^^^^

Addons can add to the user interface of existing panels, headers and menus defined in Python.

For this example we'll add to an existing menu.

.. image:: menu_id.png
   :width: 334px
   :align: center
   :height: 128px
   :alt: Menu Identifier

To find the identifier of a menu you can hover your mouse over the menu item and the identifier is displayed.

The method used for adding a menu item is to append a draw function into an existing class.


.. code-block:: python

   def menu_func(self, context):
       self.layout.operator(ObjectCursorArray.bl_idname)

   def register():
       bpy.types.VIEW3D_MT_object.append(menu_func)


For docs on extending menus see: :doc:`bpy.types.Menu`.


Keymap
^^^^^^

In Blender addons have their own key-maps so as not to interfere with Blenders built in key-maps.

In the example below, a new object-mode :class:`bpy.types.KeyMap` is added,
then a :class:`bpy.types.KeyMapItem` is added to the key-map which references our newly added operator,
using :kbd:`Ctrl-Shift-Space` as the key shortcut to activate it.


.. code-block:: python

   # store keymaps here to access after registration
   addon_keymaps = []

   def register():

       # handle the keymap
       wm = bpy.context.window_manager
       km = wm.keyconfigs.addon.keymaps.new(name='Object Mode', space_type='EMPTY')

       kmi = km.keymap_items.new(ObjectCursorArray.bl_idname, 'SPACE', 'PRESS', ctrl=True, shift=True)
       kmi.properties.total = 4

       addon_keymaps.append((km, kmi))


   def unregister():

       # handle the keymap
       for km, kmi in addon_keymaps:
           km.keymap_items.remove(kmi)
       addon_keymaps.clear()


Notice how the key-map item can have a different ``total`` setting then the default set by the operator,
this allows you to have multiple keys accessing the same operator with different settings.


.. note::

   While :kbd:`Ctrl-Shift-Space` isn't a default Blender key shortcut, its hard to make sure addons won't
   overwrite each others keymaps, At least take care when assigning keys that they don't
   conflict with important functionality within Blender.

For API documentation on the functions listed above, see:
:class:`bpy.types.KeyMaps.new`,
:class:`bpy.types.KeyMap`,
:class:`bpy.types.KeyMapItems.new`,
:class:`bpy.types.KeyMapItem`.


Bringing it all together
^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: python

   bl_info = {
       "name": "Cursor Array",
       "category": "Object",
   }

   import bpy


   class ObjectCursorArray(bpy.types.Operator):
       """Object Cursor Array"""
       bl_idname = "object.cursor_array"
       bl_label = "Cursor Array"
       bl_options = {'REGISTER', 'UNDO'}

       total = bpy.props.IntProperty(name="Steps", default=2, min=1, max=100)

       def execute(self, context):
           scene = context.scene
           cursor = scene.cursor_location
           obj = scene.objects.active

           for i in range(self.total):
               obj_new = obj.copy()
               scene.objects.link(obj_new)

               factor = i / self.total
               obj_new.location = (obj.location * factor) + (cursor * (1.0 - factor))

           return {'FINISHED'}


   def menu_func(self, context):
       self.layout.operator(ObjectCursorArray.bl_idname)

   # store keymaps here to access after registration
   addon_keymaps = []


   def register():
       bpy.utils.register_class(ObjectCursorArray)
       bpy.types.VIEW3D_MT_object.append(menu_func)

       # handle the keymap
       wm = bpy.context.window_manager
       # Note that in background mode (no GUI available), keyconfigs are not available either, so we have to check this
       # to avoid nasty errors in background case.
       kc = wm.keyconfigs.addon
       if kc:
           km = wm.keyconfigs.addon.keymaps.new(name='Object Mode', space_type='EMPTY')
           kmi = km.keymap_items.new(ObjectCursorArray.bl_idname, 'SPACE', 'PRESS', ctrl=True, shift=True)
           kmi.properties.total = 4
           addon_keymaps.append((km, kmi))

   def unregister():
       # Note: when unregistering, it's usually good practice to do it in reverse order you registered.
       # Can avoid strange issues like keymap still referring to operators already unregistered...
       # handle the keymap
       for km, kmi in addon_keymaps:
           km.keymap_items.remove(kmi)
       addon_keymaps.clear()

       bpy.utils.unregister_class(ObjectCursorArray)
       bpy.types.VIEW3D_MT_object.remove(menu_func)


   if __name__ == "__main__":
       register()

.. image:: in_menu.png
   :width: 591px
   :align: center
   :height: 649px
   :alt: In the menu

Run the script (or save it and add it through the Preferences like before) and it will appear in the menu.

.. image:: op_prop.png
   :width: 669px
   :align: center
   :height: 644px
   :alt: Operator Property

After selecting it from the menu, you can choose how many instance of the cube you want created.


.. note::

   Directly executing the script multiple times will add the menu each time too.
   While not useful behavior, theres nothing to worry about since addons won't register them selves multiple
   times when enabled through the user preferences.


Conclusions
===========

Addons can encapsulate certain functionality neatly for writing tools to improve your work-flow or for writing utilities
for others to use.

While there are limits to what Python can do within Blender, there is certainly a lot that can be achieved without
having to dive into Blender's C/C++ code.

The example given in the tutorial is limited, but shows the Blender API used for common tasks that you can expand on
to write your own tools.


Further Reading
---------------

Blender comes commented templates which are accessible from the text editor header, if you have specific areas
you want to see example code for, this is a good place to start.


Here are some sites you might like to check on after completing this tutorial.

* :ref:`Blender/Python API Overview <info_overview>` -
  *For more background details on Blender/Python integration.*

* `How to Think Like a Computer Scientist <http://interactivepython.org/courselib/static/thinkcspy/index.html>`_ -
  *Great info for those who are still learning Python.*

* `Blender Development (Wiki) <http://wiki.blender.org/index.php/Dev:Contents>`_ -
  *Blender Development, general information and helpful links.*

* `Blender Artists (Coding Section) <http://blenderartists.org/forum/forumdisplay.php?47-Coding>`_ -
  *forum where people ask Python development questions*

