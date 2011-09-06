***************
Tips and Tricks
***************

Some of these are just python features that scripters may not have thaught to use with blender.


Use The Terminal
================

When writing python scripts, its useful to have a terminal open, this is not the built-in python console but a terminal application which is used to start blender.

There are 3 main uses for the terminal, these are:

* You can see the output of `print()` as you're script runs, which is useful to view debug info.

* The error tracebacks are printed in full to the terminal which wont always generate an error popup in blenders user interface (depending on how the script is executed).

* If the script runs for too long or you accidentally enter an infinate loop, Ctrl+C in the terminal (Ctrl+Break on Windows) will quit the script early.

.. note::
   For Linux and OSX users this means starting the terminal first, then running blender from within it. On Windows the terminal can be enabled from the help menu.


Run External Scripts
====================

Blenders text editor is fine for edits and writing small tests but it is not a full featured editor so for larger projects you'll probably want to use an external editor.

Editing a text file externally and having the same text open in blender does work but isn't that optimal so here are 2 ways you can easily use an external file from blender.


Executing External Scripts
^^^^^^^^^^^^^^^^^^^^^^^^^^

This is the equivilent to running the script directly, referencing a scripts path from a 2 line textblock.

.. code-block::

   filename = "/full/path/to/myscript.py"
   exec(compile(open(filename).read(), filename, 'exec'))


You might also want to reference the file relative to the blend file.

.. code-block::

   filename = "/full/path/to/script.py"
   exec(compile(open(filename).read(), filename, 'exec'))


You might want to reference a script thats at the same location as the blend file.

.. code-block::

   import bpy
   import os

   filename = os.path.join(os.path.basename(bpy.data.filepath), "myscript.py")
   exec(compile(open(filename).read(), filename, 'exec'))


Executing Modules
^^^^^^^^^^^^^^^^^

This example shows loading a script in as a module and executing a module function.

.. code-block::

   import myscript
   import imp

   imp.reload(myscript)
   myscript.main()


Notice that the script is reloaded every time, this forces an update, normally the module stays cached in `sys.modules`.

The main difference between this and executing the script directly is it has to call a function in the module, in this case `main()` but it can be any function, an advantage with this is you can pass argumnents to the function from this small script which is often useful for testing differnt settings quickly.

The other issue with this is the script has to be in pythons module search path.
While this is not best practice - for testing you can extend the search path, this example adds the current blend files directory to the search path, then loads the script as a module.

.. code-block::

   import sys
   import os
   impory bpy

   blend_dir = os.path.basename(bpy.data.filepath)
   if blend_dir not in sys.path:
      sys.path.append(blend_dir)

   import myscript
   import imp
   imp.reload(myscript)
   myscript.main()


Don't Use Blender!
==================


Use External Tools
==================


Bundled Python
==============

Blender from blender.org includes a compleate python installation on all platforms, this has the disadvantage that any extensions you have installed in you're systems python wont be found by blender.

There are 2 ways around this:

* remove blender python subdirectory, blender will then look for the systems python and use that instead **python version must match the one that blender comes with**.

* copy the extensions into blender's python subdirectry so blender can access them, you could also copy the entire python installation into blenders subdirectory, replacing the one blender comes with. This works as long as the python versions match and the paths are created in the same location relative locations. Doing this has the advantage that you can redistribute this bundle to others with blender and/or the game player, including any extensions you rely on.


Advanced
========


Blender as a module
-------------------


Python Safety (Build Option)
----------------------------


CTypes in Blender
-----------------
