
Game Types (bge.types)
======================

.. module:: bge.types

************
Introduction
************

This module contains the classes that appear as instances in the Game Engine. A
script must interact with these classes if it is to affect the behaviour of
objects in a game.

The following example would move an object (i.e. an instance of
:class:`KX_GameObject`) one unit up.

.. code-block:: python

   # bge.types.SCA_PythonController
   cont = bge.logic.getCurrentController()

   # bge.types.KX_GameObject
   obj = cont.owner
   obj.worldPosition.z += 1

To run the code, it could be placed in a Blender text block and executed with
a :class:`SCA_PythonController` logic brick.

*****
Types
*****

.. toctree::
   :glob:

   bge.types.*

