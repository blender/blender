SCA_KeyboardSensor(SCA_ISensor)
===============================

.. module:: bge.types

base class --- :class:`SCA_ISensor`

.. class:: SCA_KeyboardSensor(SCA_ISensor)

   A keyboard sensor detects player key presses.

   See module :mod:`bge.events` for keycode values.

   .. attribute:: key

      The key code this sensor is looking for.

      :type: keycode from :mod:`bge.events` module

   .. attribute:: hold1

      The key code for the first modifier this sensor is looking for.

      :type: keycode from :mod:`bge.events` module

   .. attribute:: hold2

      The key code for the second modifier this sensor is looking for.

      :type: keycode from :mod:`bge.events` module

   .. attribute:: toggleProperty

      The name of the property that indicates whether or not to log keystrokes as a string.

      :type: string

   .. attribute:: targetProperty

      The name of the property that receives keystrokes in case in case a string is logged.

      :type: string

   .. attribute:: useAllKeys

      Flag to determine whether or not to accept all keys.

      :type: boolean

   .. attribute:: events

      a list of pressed keys that have either been pressed, or just released, or are active this frame. (read-only).

      :type: list [[:ref:`keycode<keyboard-keys>`, :ref:`status<input-status>`], ...]

   .. method:: getKeyStatus(keycode)

      Get the status of a key.

      :arg keycode: The code that represents the key you want to get the state of, use one of :ref:`these constants<keyboard-keys>`
      :type keycode: integer
      :return: The state of the given key, can be one of :ref:`these constants<input-status>`
      :rtype: int

