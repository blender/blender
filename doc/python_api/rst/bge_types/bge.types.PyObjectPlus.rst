PyObjectPlus
============

.. module:: bge.types

.. class:: PyObjectPlus

   PyObjectPlus base class of most other types in the Game Engine.

   .. attribute:: invalid

      Test if the object has been freed by the game engine and is no longer valid.
       
      Normally this is not a problem but when storing game engine data in the GameLogic module, 
      KX_Scenes or other KX_GameObjects its possible to hold a reference to invalid data.
      Calling an attribute or method on an invalid object will raise a SystemError.
       
      The invalid attribute allows testing for this case without exception handling.

      :type: boolean

