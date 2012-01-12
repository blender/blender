*******************
Python API Overview
*******************

This document is to give an understanding of how python and blender fit together, covering some of the functionality that isn't obvious from reading the API reference and example scripts.


Python in Blender
=================

Blender embeds a python interpreter which is started with blender and stays active. This interpreter runs scripts to draw the user interface and is used for some of Blender's internal tools too.

This is a typical python environment so tutorials on how to write python scripts will work running the scripts in blender too. Blender provides the :mod:`bpy` module to the python interpreter. This module can be imported in a script and gives access to blender data, classes, and functions. Scripts that deal with blender data will need to import this module. 

Here is a simple example of moving a vertex of the object named **Cube**:

.. code-block:: python

   import bpy
   bpy.data.objects["Cube"].data.vertices[0].co.x += 1.0

This modifies Blender's internal data directly. When you run this in the interactive console you will see the 3D viewport update.


The Default Environment
=======================

When developing your own scripts it may help to understand how blender sets up its python environment. Many python scripts come bundled with blender and can be used as a reference because they use the same API that script authors write tools in. Typical usage for scripts include: user interface, import/export, scene manipulation, automation, defining your own toolset and customization.

On startup blender scans the ``scripts/startup/`` directory for python modules and imports them. The exact location of this directory depends on your installation. `See the directory layout docs <http://wiki.blender.org/index.php/Doc:2.6/Manual/Introduction/Installing_Blender/DirectoryLayout>`_


Script Loading
==============

This may seem obvious but it's important to note the difference between executing a script directly or importing it as a module.

Scripts that extend blender - define classes that exist beyond the scripts execution, this makes future access to these classes (to unregister for example) more difficult than importing as a module where class instance is kept in the module and can be accessed by importing that module later on.

For this reason it's preferable to only use directly execute scripts that don't extend blender by registering classes.


Here are some ways to run scripts directly in blender.

* Loaded in the text editor and press **Run Script**.

* Typed or pasted into the interactive console.

* Execute a python file from the command line with blender, eg:

  ``blender --python /home/me/my_script.py``


To run as modules:

* The obvious way, ``import some_module`` command from the text window or interactive console.

* Open as a text block and tick "Register" option, this will load with the blend file.

* copy into one of the directories ``scripts/startup``, where they will be automatically imported on startup.

* define as an addon, enabling the addon will load it as a python module.


Addons
------

Some of blenders functionality is best kept optional, alongside scripts loaded at startup we have addons which are kept in their own directory ``scripts/addons``, and only load on startup if selected from the user preferences.

The only difference between addons and built-in python modules is that addons must contain a **bl_info** variable which blender uses to read metadata such as name, author, category and URL.

The user preferences addon listing uses **bl_info** to display information about each addon.

`See Addons <http://wiki.blender.org/index.php/Dev:2.6/Py/Scripts/Guidelines/Addons>`_ for details on the **bl_info** dictionary.


Integration through Classes
===========================

Running python scripts in the text editor is useful for testing but you’ll want to extend blender to make tools accessible like other built-in functionality.

The blender python api allows integration for:

* :class:`bpy.types.Panel`

* :class:`bpy.types.Menu`

* :class:`bpy.types.Operator`

* :class:`bpy.types.PropertyGroup`

* :class:`bpy.types.KeyingSet`

* :class:`bpy.types.RenderEngine`


This is intentionally limited. Currently, for more advanced features such as mesh modifiers, object types, or shader nodes, C/C++ must be used.

For python intergration Blender defines methods which are common to all types. This works by creating a python subclass of a Blender class which contains variables and functions specified by the parent class which are pre-defined to interface with Blender.

For example:

.. code-block:: python

   import bpy
   class SimpleOperator(bpy.types.Operator):
       bl_idname = "object.simple_operator"
       bl_label = "Tool Name"

       def execute(self, context):
           print("Hello World")
           return {'FINISHED'}

   bpy.utils.register_class(SimpleOperator)

First note that we subclass a member of :mod:`bpy.types`, this is common for all classes which can be integrated with blender and used so we know if this is an Operator and not a Panel when registering.

Both class properties start with a **bl_** prefix. This is a convention used to distinguish blender properties from those you add yourself.

Next see the execute function, which takes an instance of the operator and the current context. A common prefix is not used for functions.

Lastly the register function is called, this takes the class and loads it into blender. See `Class Registration`_.

Regarding inheritance, blender doesn't impose restrictions on the kinds of class inheritance used, the registration checks will use attributes and functions defined in parent classes.

class mix-in example:

.. code-block:: python

   import bpy
   class BaseOperator:
       def execute(self, context):
           print("Hello World BaseClass")
           return {'FINISHED'}

   class SimpleOperator(bpy.types.Operator, BaseOperator):
       bl_idname = "object.simple_operator"
       bl_label = "Tool Name"

   bpy.utils.register_class(SimpleOperator)

Notice these classes don't define an ``__init__(self)`` function. While ``__init__()`` and ``__del__()`` will be called if defined, the class instances lifetime only spans the execution. So a panel for example will have a new instance for every redraw, for this reason there is rarely a cause to store variables in the panel instance. Instead, persistent variables should be stored in Blenders data so that the state can be restored when blender is restarted.

.. note:: Modal operators are an exception, keeping their instance variable as blender runs, see modal operator template.

So once the class is registered with blender, instancing the class and calling the functions is left up to blender. In fact you cannot instance these classes from the script as you would expect with most python API's.

To run operators you can call them through the operator api, eg:

.. code-block:: python

   import bpy
   bpy.ops.object.simple_operator()

User interface classes are given a context in which to draw, buttons window, file header, toolbar etc, then they are drawn when that area is displayed so they are never called by python scripts directly.


Registration
============


Module Registration
-------------------

Blender modules loaded at startup require ``register()`` and ``unregister()`` functions. These are the *only* functions that blender calls from your code, which is otherwise a regular python module.

A simple blender/python module can look like this:

.. code-block:: python

   import bpy

   class SimpleOperator(bpy.types.Operator):
       """ See example above """

   def register():
       bpy.utils.register_class(SimpleOperator)

   def unregister():
       bpy.utils.unregister_class(SimpleOperator)    

   if __name__ == "__main__":
       register()

These functions usually appear at the bottom of the script containing class registration sometimes adding menu items. You can also use them for internal purposes setting up data for your own tools but take care since register won't re-run when a new blend file is loaded.

The register/unregister calls are used so it's possible to toggle addons and reload scripts while blender runs.
If the register calls were placed in the body of the script, registration would be called on import, meaning there would be no distinction between importing a module or loading its classes into blender.

This becomes problematic when a script imports classes from another module making it difficult to manage which classes are being loaded and when.

The last 2 lines are only for testing:

.. code-block:: python

   if __name__ == "__main__":
       register()

This allows the script to be run directly in the text editor to test changes.
This ``register()`` call won't run when the script is imported as a module since ``__main__`` is reserved for direct execution.


Class Registration
------------------

Registering a class with blender results in the class definition being loaded into blender, where it becomes available alongside existing functionality.

Once this class is loaded you can access it from :mod:`bpy.types`, using the bl_idname rather than the classes original name.

When loading a class, blender performs sanity checks making sure all required properties and functions are found, that properties have the correct type, and that functions have the right number of arguments.

Mostly you will not need concern yourself with this but if there is a problem with the class definition it will be raised on registering:

Using the function arguments ``def execute(self, context, spam)``, will raise an exception:

``ValueError: expected Operator, SimpleOperator class "execute" function to have 2 args, found 3``

Using ``bl_idname = 1`` will raise.

``TypeError: validating class error: Operator.bl_idname expected a string type, not int``


Multiple-Classes
^^^^^^^^^^^^^^^^

Loading classes into blender is described above, for simple cases calling :mod:`bpy.utils.register_class` (SomeClass) is sufficient, but when there are many classes or a packages submodule has its own classes it can be tedious to list them all for registration.

For more convenient loading/unloading :mod:`bpy.utils.register_module` (module) and :mod:`bpy.utils.unregister_module` (module) functions exist.

A script which defines many of its own operators, panels menus etc. you only need to write:

.. code-block:: python

   def register():
       bpy.utils.register_module(__name__)

   def unregister():
       bpy.utils.unregister_module(__name__)

Internally blender collects subclasses on registrable types, storing them by the module in which they are defined. By passing the module name to :mod:`bpy.utils.register_module` blender can register all classes created by this module and its submodules.


Inter Classes Dependencies
^^^^^^^^^^^^^^^^^^^^^^^^^^

When customizing blender you may want to group your own settings together, after all, they will likely have to co-exist with other scripts. To group these properties classes need to be defined, for groups within groups or collections within groups you can find yourself having to deal with order of registration/unregistration.

Custom properties groups are themselves classes which need to be registered.

Say you want to store material settings for a custom engine.

.. code-block:: python

   # Create new property
   # bpy.data.materials[0].my_custom_props.my_float
   import bpy

   class MyMaterialProps(bpy.types.PropertyGroup):
       my_float = bpy.props.FloatProperty()

   def register():
       bpy.utils.register_class(MyMaterialProps)
       bpy.types.Material.my_custom_props = bpy.props.PointerProperty(type=MyMaterialProps)

   def unregister():
       del bpy.types.Material.my_custom_props
       bpy.utils.unregister_class(MyMaterialProps)

   if __name__ == "__main__":
       register()

.. note::

   *The class must be registered before being used in a property, failing to do so will raise an error:*
   
   ``ValueError: bpy_struct "Material" registration error: my_custom_props could not register``


.. code-block:: python

   # Create new property group with a sub property
   # bpy.data.materials[0].my_custom_props.sub_group.my_float
   import bpy

   class MyMaterialSubProps(bpy.types.PropertyGroup):
       my_float = bpy.props.FloatProperty()

   class MyMaterialGroupProps(bpy.types.PropertyGroup):
       sub_group = bpy.props.PointerProperty(type=MyMaterialSubProps)

   def register():
       bpy.utils.register_class(MyMaterialSubProps)
       bpy.utils.register_class(MyMaterialGroupProps)
       bpy.types.Material.my_custom_props = bpy.props.PointerProperty(type=MyMaterialGroupProps)

   def unregister():
       del bpy.types.Material.my_custom_props
       bpy.utils.unregister_class(MyMaterialGroupProps)
       bpy.utils.unregister_class(MyMaterialSubProps)

   if __name__ == "__main__":
       register()

.. note::

   *The lower most class needs to be registered first and that unregister() is a mirror of register()*


Manipulating Classes
^^^^^^^^^^^^^^^^^^^^

Properties can be added and removed as blender runs, normally happens on register or unregister but for some special cases it may be useful to modify types as the script runs.

For example:

.. code-block:: python

   # add a new property to an existing type
   bpy.types.Object.my_float = bpy.props.FloatProperty()
   # remove
   del bpy.types.Object.my_float

This works just as well for PropertyGroup subclasses you define yourself.

.. code-block:: python

   class MyPropGroup(bpy.types.PropertyGroup):
       pass
   MyPropGroup.my_float = bpy.props.FloatProperty()

...this is equivalent to:

.. code-block:: python

   class MyPropGroup(bpy.types.PropertyGroup):
       my_float = bpy.props.FloatProperty()


Dynamic Defined-Classes (Advanced)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

In some cases the specifier for data may not be in blender, renderman shader definitions for example and it may be useful to define types and remove them on the fly.

.. code-block:: python

   for i in range(10):
       idname = "object.operator_%d" % i

       def func(self, context):
           print("Hello World", self.bl_idname)
           return {'FINISHED'}

       opclass = type("DynOp%d" % i,
                      (bpy.types.Operator, ),
                      {"bl_idname": idname, "bl_label": "Test", "execute": func},
                      )
       bpy.utils.register_class(opclass)

.. note::

   Notice ``type()`` is called to define the class. This is an alternative syntax for class creation in python, better suited to constructing classes dynamically.


Calling these operators:

   >>> bpy.ops.object.operator_1()
   Hello World OBJECT_OT_operator_1
   {'FINISHED'}

   >>> bpy.ops.object.operator_2()
   Hello World OBJECT_OT_operator_2
   {'FINISHED'}

