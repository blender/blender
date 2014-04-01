KX_CharacterWrapper(PyObjectPlus)
=================================

.. module:: bge.types

base class --- :class:`PyObjectPlus`

.. class:: KX_CharacterWrapper(PyObjectPlus)

   A wrapper to expose character physics options.

   .. attribute:: onGround

      Whether or not the character is on the ground. (read-only)

      :type: boolean

   .. attribute:: gravity

      The gravity value used for the character.

      :type: float

   .. attribute:: maxJumps

      The maximum number of jumps a character can perform before having to touch the ground. By default this is set to 1. 2 allows for a double jump, etc.

      :type: int

   .. attribute:: jumpCount

      The current jump count. This can be used to have different logic for a single jump versus a double jump. For example, a different animation for the second jump.

      :type: int

   .. attribute:: walkDirection

      The speed and direction the character is traveling in using world coordinates. This should be used instead of applyMovement() to properly move the character.

      :type: list [x, y, z]

   .. method:: jump()

      The character jumps based on it's jump speed.

