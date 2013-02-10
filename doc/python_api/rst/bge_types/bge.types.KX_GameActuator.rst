KX_GameActuator(SCA_IActuator)
==============================

.. module:: bge.types

base class --- :class:`SCA_IActuator`

.. class:: KX_GameActuator(SCA_IActuator)

   The game actuator loads a new .blend file, restarts the current .blend file or quits the game.

   .. attribute:: fileName

      the new .blend file to load.

      :type: string

   .. attribute:: mode

      The mode of this actuator. Can be on of :ref:`these constants <game-actuator>`

      :type: Int

