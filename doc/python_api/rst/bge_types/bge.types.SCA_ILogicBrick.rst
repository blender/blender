SCA_ILogicBrick(CValue)
=======================

.. module:: bge.types

base class --- :class:`CValue`

.. class:: SCA_ILogicBrick(CValue)

   Base class for all logic bricks.

   .. attribute:: executePriority

      This determines the order controllers are evaluated, and actuators are activated (lower priority is executed first).

      :type: executePriority: int

   .. attribute:: owner

      The game object this logic brick is attached to (read-only).
      
      :type: :class:`KX_GameObject` or None in exceptional cases.

   .. attribute:: name

      The name of this logic brick (read-only).
      
      :type: string

