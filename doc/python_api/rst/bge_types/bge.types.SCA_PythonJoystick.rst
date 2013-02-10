SCA_PythonJoystick(PyObjectPlus)
================================

.. module:: bge.types

base class --- :class:`PyObjectPlus`

.. class:: SCA_PythonJoystick(PyObjectPlus)

   A Python interface to a joystick.

   .. attribute:: name

      The name assigned to the joystick by the operating system. (read-only)
	  
      :type: string

   .. attribute:: activeButtons

      A list of active button values. (read-only)
	  
      :type: list

   .. attribute:: axisValues

      The state of the joysticks axis as a list of values :data:`numAxis` long. (read-only).

      :type: list of ints.

      Each specifying the value of an axis between -1.0 and 1.0 depending on how far the axis is pushed, 0 for nothing.
      The first 2 values are used by most joysticks and gamepads for directional control. 3rd and 4th values are only on some joysticks and can be used for arbitary controls.

      * left:[-1.0, 0.0, ...]
      * right:[1.0, 0.0, ...]
      * up:[0.0, -1.0, ...]
      * down:[0.0, 1.0, ...]

   .. attribute:: hatValues

      The state of the joysticks hats as a list of values :data:`numHats` long. (read-only).

      :type: list of ints

      Each specifying the direction of the hat from 1 to 12, 0 when inactive.

      Hat directions are as follows...

      * 0:None
      * 1:Up
      * 2:Right
      * 4:Down
      * 8:Left
      * 3:Up - Right
      * 6:Down - Right
      * 12:Down - Left
      * 9:Up - Left

   .. attribute:: numAxis

      The number of axes for the joystick at this index. (read-only).

      :type: integer

   .. attribute:: numButtons

      The number of buttons for the joystick at this index. (read-only).

      :type: integer

   .. attribute:: numHats

      The number of hats for the joystick at this index. (read-only).

      :type: integer

