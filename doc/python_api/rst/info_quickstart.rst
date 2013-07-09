.. _info_quickstart:

***********************
Quickstart Introduction
***********************

Preface
=======

This API is generally stable but some areas are still being added and improved.

The Blender/Python API can do the following:

* Edit any data the user interface can (Scenes, Meshes, Particles etc.)

* Modify user preferences, keymaps and themes

* Run tools with own settings

* Create user interface elements such as menus, headers and panels

* Create new tools

* Create interactive tools

* Create new rendering engines that integrate with Blender

* Define new settings in existing Blender data

* Draw in the 3D view using OpenGL commands from Python


The Blender/Python API **can't** (yet)...

* Create new space types.

* Assign custom properties to every type.

* Define callbacks or listeners to be notified when data is changed.


Before Starting
===============

This document isn't intended to fully cover each topic. Rather, its purpose is to familiarize you with Blender Python API.


A quick list of helpful things to know before starting:

* Blender uses Python 3.x; some 3rd party extensions are not available yet.

* The interactive console is great for testing one-liners, It also has autocompleation so you can inspect the api quickly.

* Button tool tips show Python attributes and operator names.

* Right clicking on buttons and menu items directly links to API documentation.

* For more examples, the text menu has a templates section where some example operators can be found.

* To examine further scripts distributed with Blender, see ``~/.blender/scripts/startup/bl_ui`` for the user interface and ``~/.blender/scripts/startup/bl_op`` for operators.


Running Scripts
---------------

The two most common ways to execute python scripts are using the built-in text editor or entering commands in the python console.

Both the **Text Editor** and **Python Console** are space types you can select from the view header.

Rather then manually configuring your spaces for Python development, you may prefer to use the **Scripting** screen, included default with Blender, accessible from the top headers screen selector.

From the text editor you can open ``.py`` files or paste then from the clipboard, then test using **Run Script**.

The Python Console is typically used for typing in snippets and for testing to get immediate feedback, but can also have entire scripts pasted into it.

Scripts can also run from the command line with Blender but to learn Blender/Python this isn't essential.


Key Concepts
============

Data Access
-----------

Accessing datablocks
^^^^^^^^^^^^^^^^^^^^

Python accesses Blender's data in the same way as the animation system and user interface; this implies that any setting that can be changed via a button can also be changed from Python.

Accessing data from the currently loaded blend file is done with the module :mod:`bpy.data`. This gives access to library data. For example:

   >>> bpy.data.objects
   <bpy_collection[3], BlendDataObjects>

   >>> bpy.data.scenes
   <bpy_collection[1], BlendDataScenes>

   >>> bpy.data.materials
   <bpy_collection[1], BlendDataMaterials>


About Collections
^^^^^^^^^^^^^^^^^

You'll notice that an index as well as a string can be used to access members of the collection.

Unlike Python's dictionaries, both methods are acceptable; however, the index of a member may change while running Blender.

   >>> list(bpy.data.objects)
   [bpy.data.objects["Cube"], bpy.data.objects["Plane"]]

   >>> bpy.data.objects['Cube']
   bpy.data.objects["Cube"]

   >>> bpy.data.objects[0]
   bpy.data.objects["Cube"]


Accessing attributes
^^^^^^^^^^^^^^^^^^^^

Once you have a data block, such as a material, object, groups etc., its attributes can be accessed much like you would change a setting using the graphical interface. In fact, the tooltip for each button also displays the Python attribute which can help in finding what settings to change in a script.

   >>> bpy.data.objects[0].name 
   'Camera'

   >>> bpy.data.scenes["Scene"]
   bpy.data.scenes['Scene']

   >>> bpy.data.materials.new("MyMaterial")
   bpy.data.materials['MyMaterial']


For testing what data to access it's useful to use the "Console", which is its own space type. This supports auto-complete, giving you a fast way to dig into different data in your file.

Example of a data path that can be quickly found via the console:

   >>> bpy.data.scenes[0].render.resolution_percentage
   100
   >>> bpy.data.scenes[0].objects["Torus"].data.vertices[0].co.x
   1.0


Data Creation/Removal
^^^^^^^^^^^^^^^^^^^^^

Those of you familiar with other python api's may be surprised that new datablocks in the bpy api can't be created by calling the class:

   >>> bpy.types.Mesh()
   Traceback (most recent call last):
     File "<blender_console>", line 1, in <module>
   TypeError: bpy_struct.__new__(type): expected a single argument


This is an intentional part of the API design.
The blender/python api can't create blender data that exists outside the main blender database (accessed through bpy.data), because this data is managed by blender (save/load/undo/append... etc).

Data is added and removed via methods on the collections in bpy.data, eg:

   >>> mesh = bpy.data.meshes.new(name="MyMesh")
   >>> print(mesh)
   <bpy_struct, Mesh("MyMesh.001")>

   >>> bpy.data.meshes.remove(mesh)


Custom Properties
^^^^^^^^^^^^^^^^^

Python can access properties on any datablock that has an ID (data that can be linked in and accessed from :mod:`bpy.data`. When assigning a property, you can make up your own names, these will be created when needed or overwritten if they exist.

This data is saved with the blend file and copied with objects.

Example:

.. code-block:: python

   bpy.context.object["MyOwnProperty"] = 42

   if "SomeProp" in bpy.context.object:
       print("Property found")

   # Use the get function like a python dictionary
   # which can have a fallback value.
   value = bpy.data.scenes["Scene"].get("test_prop", "fallback value")

   # dictionaries can be assigned as long as they only use basic types.
   group = bpy.data.groups.new("MyTestGroup")
   group["GameSettings"] = {"foo": 10, "bar": "spam", "baz": {}}

   del group["GameSettings"]


Note that these properties can only be assigned  basic Python types.

* int, float, string

* array of ints/floats

* dictionary (only string keys are supported, values must be basic types too)

These properties are valid outside of Python. They can be animated by curves or used in driver paths.


Context
-------

While it's useful to be able to access data directly by name or as a list, it's more common to operate on the user's selection. The context is always available from '''bpy.context''' and can be used to get the active object, scene, tool settings along with many other attributes.

Common-use cases:

   >>> bpy.context.object
   >>> bpy.context.selected_objects
   >>> bpy.context.visible_bones

Note that the context is read-only. These values cannot be modified directly, though they may be changed by running API functions or by using the data API.

So ``bpy.context.object = obj`` will raise an error.

But ``bpy.context.scene.objects.active = obj`` will work as expected.


The context attributes change depending on where they are accessed. The 3D view has different context members than the console, so take care when accessing context attributes that the user state is known.

See :mod:`bpy.context` API reference


Operators (Tools)
-----------------

Operators are tools generally accessed by the user from buttons, menu items or key shortcuts. From the user perspective they are a tool but Python can run these with its own settings through the :mod:`bpy.ops` module.

Examples:

   >>> bpy.ops.mesh.flip_normals()
   {'FINISHED'}
   >>> bpy.ops.mesh.hide(unselected=False)
   {'FINISHED'}
   >>> bpy.ops.object.scale_apply()
   {'FINISHED'}

.. note::

   The menu item: Help -> Operator Cheat Sheet" gives a list of all operators and their default values in Python syntax, along with the generated docs. This is a good way to get an overview of all blender's operators.


Operator Poll()
^^^^^^^^^^^^^^^

Many operators have a "poll" function which may check that the mouse is a valid area or that the object is in the correct mode (Edit Mode, Weight Paint etc). When an operator's poll function fails within python, an exception is raised.

For example, calling bpy.ops.view3d.render_border() from the console raises the following error:

.. code-block:: python

   RuntimeError: Operator bpy.ops.view3d.render_border.poll() failed, context is incorrect

In this case the context must be the 3d view with an active camera.

To avoid using try/except clauses wherever operators are called you can call the operators own .poll() function to check if it can run in the current context.

.. code-block:: python

   if bpy.ops.view3d.render_border.poll():
       bpy.ops.view3d.render_border()


Integration
===========

Python scripts can integrate with Blender in the following ways:

* By defining a rendering engine.

* By defining operators.

* By defining menus, headers and panels.

* By inserting new buttons into existing menus, headers and panels


In Python, this is done by defining a class, which is a subclass of an existing type.


Example Operator
----------------

.. literalinclude:: ../../../release/scripts/templates_py/operator_simple.py

Once this script runs, ``SimpleOperator`` is registered with Blender and can be called from the operator search popup or added to the toolbar.

To run the script:

#. Highlight the above code then press Ctrl+C to copy it.

#. Start Blender

#. Press Ctrl+Right twice to change to the Scripting layout.

#. Click the button labeled ``New`` and the confirmation pop up in order to create a new text block.

#. Press Ctrl+V to paste the code into the text panel (the upper left frame).

#. Click on the button **Run Script**.

#. Move your mouse into the 3D view, press spacebar for the operator search menu, and type "Simple".

#. Click on the "Simple Operator" item found in search.


.. seealso:: The class members with the **bl_** prefix are documented in the API
   reference :class:`bpy.types.Operator`

.. note:: The output from the ``main`` function is sent to the terminal; in order to see this, be sure to :ref:`use the terminal <use_the_terminal>`.

Example Panel
-------------

Panels register themselves as a class, like an operator. Notice the extra **bl_** variables used to set the context they display in.

.. literalinclude:: ../../../release/scripts/templates_py/ui_panel_simple.py

To run the script:

#. Highlight the above code then press Ctrl+C to copy it

#. Start Blender

#. Press Ctrl+Right twice to change to the Scripting layout

#. Click the button labeled ``New`` and the confirmation pop up in order to create a new text block.

#. Press Ctrl+V to paste the code into the text panel (the upper left frame)

#. Click on the button **Run Script**.


To view the results:

#. Select the the default cube.

#. Click on the Object properties icon in the buttons panel (far right; appears as a tiny cube).

#. Scroll down to see a panel named **Hello World Panel**.

#. Changing the object name also updates **Hello World Panel's** Name: field.

Note the row distribution and the label and properties that are available through the code.

.. seealso:: :class:`bpy.types.Panel`


Types
=====

Blender defines a number of Python types but also uses Python native types.

Blender's Python API can be split up into 3 categories.


Native Types
------------

In simple cases returning a number or a string as a custom type would be cumbersome, so these are accessed as normal python types.

* blender float/int/boolean -> float/int/boolean

* blender enumerator -> string

     >>> C.object.rotation_mode = 'AXIS_ANGLE'


* blender enumerator (multiple) -> set of strings

  .. code-block:: python

     # setting multiple camera overlay guides
     bpy.context.scene.camera.data.show_guide = {'GOLDEN', 'CENTER'}

     # passing as an operator argument for report types
     self.report({'WARNING', 'INFO'}, "Some message!")


Internal Types
--------------

Used for Blender datablocks and collections: :class:`bpy.types.bpy_struct`

For data that contains its own attributes groups/meshes/bones/scenes... etc.

There are 2 main types that wrap Blenders data, one for datablocks (known internally as bpy_struct), another for properties.

   >>> bpy.context.object
   bpy.data.objects['Cube']

   >>> C.scene.objects
   bpy.data.scenes['Scene'].objects

Note that these types reference Blender's data so modifying them is immediately visible.


Mathutils Types
---------------

Used for vectors, quaternion, eulers, matrix and color types, accessible from :mod:`mathutils`

Some attributes such as :class:`bpy.types.Object.location`, :class:`bpy.types.PoseBone.rotation_euler` and :class:`bpy.types.Scene.cursor_location` can be accessed as special math types which can be used together and manipulated in various useful ways.

Example of a matrix, vector multiplication:

.. code-block:: python

   bpy.context.object.matrix_world * bpy.context.object.data.verts[0].co

.. note::

   mathutils types keep a reference to Blender's internal data so changes can
   be applied back.


   Example:

   .. code-block:: python

      # modifies the Z axis in place.
      bpy.context.object.location.z += 2.0

      # location variable holds a reference to the object too.
      location = bpy.context.object.location
      location *= 2.0

      # Copying the value drops the reference so the value can be passed to
      # functions and modified without unwanted side effects.
      location = bpy.context.object.location.copy()


Animation
=========

There are 2 ways to add keyframes through Python.

The first is through key properties directly, which is similar to inserting a keyframe from the button as a user. You can also manually create the curves and keyframe data, then set the path to the property. Here are examples of both methods.

Both examples insert a keyframe on the active object's Z axis.

Simple example:

.. code-block:: python

   obj = bpy.context.object
   obj.location[2] = 0.0
   obj.keyframe_insert(data_path="location", frame=10.0, index=2)
   obj.location[2] = 1.0
   obj.keyframe_insert(data_path="location", frame=20.0, index=2)

Using Low-Level Functions:

.. code-block:: python

   obj = bpy.context.object
   obj.animation_data_create()
   obj.animation_data.action = bpy.data.actions.new(name="MyAction")
   fcu_z = obj.animation_data.action.fcurves.new(data_path="location", index=2)
   fcu_z.keyframe_points.add(2)
   fcu_z.keyframe_points[0].co = 10.0, 0.0
   fcu_z.keyframe_points[1].co = 20.0, 1.0

