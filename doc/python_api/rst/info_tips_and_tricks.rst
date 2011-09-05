###############
Tips and Tricks
###############

Some of these are just python features that scripters may not have thaught to use with blender.


****************
Use The Terminal
****************

For Linux and OSX users this means starting the terminal first, then running blender from within it. on Windows the terminal can be enabled from the help menu.

********************
Run External Scripts
********************


******************
Don't Use Blender!
******************


******************
Use External Tools
******************


**************
Bundled Python
**************

Blender from blender.org includes a compleate python installation on all platforms, this has the disadvantage that any extensions you have installed in you're systems python wont be found by blender.

There are 2 ways around this:

* remove blender python subdirectory, blender will then look for the systems python and use that instead **python version must match the one that blender comes with**.

* copy the extensions into blender's python subdirectry so blender can access them, you could also copy the entire python installation into blenders subdirectory, replacing the one blender comes with. This works as long as the python versions match and the paths are created in the same location relative locations. Doing this has the advantage that you can redistribute this bundle to others with blender and/or the game player, including any extensions you rely on.

********
Advanced
********


===================
Blender as a module
===================


============================
Python Safety (Build Option)
============================

=================
CTypes in Blender
=================
