
*******************
Reference API Usage
*******************

Blender has many interlinking data types which have an auto-generated reference api which often has the information
you need to write a script, but can be difficult to use.

This document is designed to help you understand how to use the reference api.


Reference API Scope
===================

The reference API covers :mod:`bpy.types`, which stores types accessed via :mod:`bpy.context` - *The user context*
or :mod:`bpy.data` - *Blend file data*.

Other modules such as :mod:`bge`, :mod:`bmesh` and :mod:`aud` are not using Blenders data API
so this document doesn't apply to those modules.


Data Access
===========

The most common case for using the reference API is to find out how to access data in the blend file.

Before going any further its best to be aware of ID Data-Blocks in Blender since you will often find properties
relative to them.


ID Data
-------

ID Data-Blocks are used in Blender as top-level data containers.

From the user interface this isn't so obvious, but when developing you need to know about ID Data-Blocks.

ID data types include Scene, Group, Object, Mesh, Screen, World, Armature, Image and Texture.
for a full list see the sub-classes of :class:`bpy.types.ID`

Here are some characteristics ID Data-Blocks share.

- ID's are blend file data, so loading a new blend file reloads an entire new set of Data-Blocks.
- ID's can be accessed in Python from ``bpy.data.*``
- Each data-block has a unique ``.name`` attribute, displayed in the interface.
- Animation data is stored in ID's ``.animation_data``.
- ID's are the only data types that can be linked between blend files.
- ID's can be added/copied and removed via Python.
- ID's have their own garbage-collection system which frees unused ID's when saving.
- When a data-block has a reference to some external data, this is typically an ID Data-Block.


Simple Data Access
------------------

Lets start with a simple case, say you wan't a python script to adjust the objects location.

Start by finding this setting in the interface ``Properties Window -> Object -> Transform -> Location``

From the button you can right click and select **Online Python Reference**, this will link you to:
:class:`bpy.types.Object.location`

Being an API reference, this link often gives little more information then the tool-tip, though some of the pages
include examples (normally at the top of the page).

At this point you may say *Now what?* - you know that you have to use ``.location`` and that its an array of 3 floats
but you're still left wondering how to access this in a script.

So the next step is to find out where to access objects, go down to the bottom of the page to the **References**
section, for objects there are many references, but one of the most common places to access objects is via the context.

It's easy to be overwhelmed at this point since there ``Object`` get referenced in so many places - modifiers,
functions, textures and constraints.

But if you want to access any data the user has selected
you typically only need to check the :mod:`bpy.context` references.

Even then, in this case there are quite a few though if you read over these - most are mode specific.
If you happen to be writing a tool that only runs in weight paint mode, then using ``weight_paint_object``
would be appropriate.
However to access an item the user last selected, look for the ``active`` members,
Having access to a single active member the user selects is a convention in Blender: eg. ``active_bone``,
``active_pose_bone``, ``active_node`` ... and in this case we can use - ``active_object``.


So now we have enough information to find the location of the active object.

.. code-block:: python

   bpy.context.active_object.location

You can type this into the python console to see the result.

The other common place to access objects in the reference is :class:`bpy.types.BlendData.objects`.

.. note::

   This is **not** listed as :mod:`bpy.data.objects`,
   this is because :mod:`bpy.data` is an instance of the :class:`bpy.types.BlendData` class,
   so the documentation points there.


With :mod:`bpy.data.objects`, this is a collection of objects so you need to access one of its members.

.. code-block:: python

   bpy.data.objects["Cube"].location


Nested Properties
-----------------

The previous example is quite straightforward because ``location`` is a property of ``Object`` which can be accessed
from the context directly.

Here are some more complex examples:

.. code-block:: python

   # access a render layers samples
   bpy.context.scene.render.layers["RenderLayer"].samples

   # access to the current weight paint brush size
   bpy.context.tool_settings.weight_paint.brush.size  

   # check if the window is fullscreen
   bpy.context.window.screen.show_fullscreen


As you can see there are times when you want to access data which is nested
in a way that causes you to go through a few indirections.

The properties are arranged to match how data is stored internally (in blenders C code) which is often logical but
not always quite what you would expect from using Blender.

So this takes some time to learn, it helps you understand how data fits together in Blender which is important
to know when writing scripts.


When starting out scripting you will often run into the problem where you're not sure how to access the data you want.

There are a few ways to do this.

- Use the Python console's auto-complete to inspect properties. *This can be hit-and-miss but has the advantage
  that you can easily see the values of properties and assign them to interactively see the results.*

- Copy the Data-Path from the user interface. *Explained further in :ref:`Copy Data Path <info_data_path_copy>`*

- Using the documentation to follow references. *Explained further in :ref:`Indirect Data Access <info_data_path_indirect>`*


.. _info_data_path_copy

Copy Data Path
--------------

Blender can compute the Python string to a property which is shown in the tool-tip, on the line below ``Python: ...``,
This saves having to use the API reference to click back up the references to find where data is accessed from.

There is a user-interface feature to copy the data-path which gives the path from an :class:`bpy.types.ID` data-block,
to its property.

To see how this works we'll get the path to the Subdivision-Surface modifiers subdivision setting.

Start with the default scene and select the **Modifiers** tab, then add a **Subdivision-Surface** modifier to the cube.

Now hover your mouse over the button labeled **View**, The tool-tip includes :class:`bpy.types.SubsurfModifier.levels`
but we want the path from the object to this property.

Note that the text copied won't include the ``bpy.data.collection["name"].`` component since its assumed that
you won't be doing collection look-ups on every access and typically you'll want to use the context rather
then access each :class:`bpy.types.ID` instance by name.


Type in the ID path into a Python console :mod:`bpy.context.active_object`. Include the trailing dot and don't hit "enter", yet. 

Now right-click on the button and select **Copy Data Path**, then paste the result into the console.

So now you should have the answer:

.. code-block:: python

   bpy.context.active_object.modifiers["Subsurf"].levels

Hit "enter" and you'll get the current value of 1. Now try changing the value to 2:

.. code-block:: python

  bpy.context.active_object.modifiers["Subsurf"].levels = 2

You can see the value update in the Subdivision-Surface modifier's UI as well as the cube.


.. _info_data_path_indirect

Indirect Data Access
--------------------

For this example we'll go over something more involved, showing the steps to access the active sculpt brushes texture.

Lets say we want to access the texture of a brush via Python, to adjust its ``contrast`` for example.

- Start in the default scene and enable 'Sculpt' mode from the 3D-View header.

- From the toolbar expand the **Texture** panel and add a new texture.

  *Notice the texture button its self doesn't have very useful links (you can check the tool-tips).*

- The contrast setting isn't exposed in the sculpt toolbar, so view the texture in the properties panel...

  - In the properties button select the Texture context.

  - Select the Brush icon to show the brush texture.

  - Expand the **Colors** panel to locate the **Contrast** button.

- Right click on the contrast button and select **Online Python Reference** This takes you to ``bpy.types.Texture.contrast``

- Now we can see that ``contrast`` is a property of texture, so next we'll check on how to access the texture from the brush.

- Check on the **References** at the bottom of the page, sometimes there are many references, and it may take
  some guess work to find the right one, but in this case its obviously ``Brush.texture``.

  *Now we know that the texture can be accessed from* ``bpy.data.brushes["BrushName"].texture``
  *but normally you won't want to access the brush by name, so we'll see now to access the active brush instead.*

- So the next step is to check on where brushes are accessed from via the **References**.
  In this case there is simply ``bpy.context.brush`` which is all we need.
  

Now you can use the Python console to form the nested properties needed to access brush textures contrast,
logically we now know.

*Context -> Brush -> Texture -> Contrast*

Since the attribute for each is given along the way we can compose the data path in the python console:

.. code-block:: python

   bpy.context.brush.texture.contrast


There can be multiple ways to access the same data, which you choose often depends on the task.

An alternate path to access the same setting is...

.. code-block:: python

   bpy.context.sculpt.brush.texture.contrast

Or access the brush directly...

.. code-block:: python

   bpy.data.brushes["BrushName"].texture.contrast


If you are writing a user tool normally you want to use the :mod:`bpy.context` since the user normally expects
the tool to operate on what they have selected.

For automation you are more likely to use :mod:`bpy.data` since you want to be able to access specific data and manipulate
it, no matter what the user currently has the view set at.


Operators
=========

Most key-strokes and buttons in Blender call an operator which is also exposed to python via :mod:`bpy.ops`,

To see the Python equivalent hover your mouse over the button and see the tool-tip,
eg ``Python: bpy.ops.render.render()``,
If there is no tool-tip or the ``Python:`` line is missing then this button is not using an operator and
can't be accessed from Python.


If you want to use this in a script you can press :kbd:`Control-C` while your mouse is over the button to copy it to the
clipboard.

You can also right click on the button and view the **Online Python Reference**, this mainly shows arguments and
their defaults however operators written in Python show their file and line number which may be useful if you
are interested to check on the source code.

.. note::

   Not all operators can be called usefully from Python, for more on this see :ref:`using operators <using_operators>`.


Info View
---------

Blender records operators you run and displays them in the **Info** space.
This is located above the file-menu which can be dragged down to display its contents.

Select the **Script** screen that comes default with Blender to see its output.
You can perform some actions and see them show up - delete a vertex for example.

Each entry can be selected (Right-Mouse-Button), then copied :kbd:`Control-C`, usually to paste in the text editor or python console.

.. note::

   Not all operators get registered for display,
   zooming the view for example isn't so useful to repeat so its excluded from the output.

   To display *every* operator that runs see :ref:`Show All Operators <info_show_all_operators>`

