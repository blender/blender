SCA_JoystickSensor(SCA_ISensor)
===============================

.. module:: bge.types

base class --- :class:`SCA_ISensor`

.. class:: SCA_JoystickSensor(SCA_ISensor)

   This sensor detects player joystick events.

   .. attribute:: axisValues

      The state of the joysticks axis as a list of values :data:`numAxis` long. (read-only).

      :type: list of ints.

      Each specifying the value of an axis between -32767 and 32767 depending on how far the axis is pushed, 0 for nothing.
      The first 2 values are used by most joysticks and gamepads for directional control. 3rd and 4th values are only on some joysticks and can be used for arbitary controls.

      * left:[-32767, 0, ...]
      * right:[32767, 0, ...]
      * up:[0, -32767, ...]
      * down:[0, 32767, ...]

   .. attribute:: axisSingle

      like :data:`axisValues` but returns a single axis value that is set by the sensor. (read-only).

      :type: integer

      .. note::
         
         Only use this for "Single Axis" type sensors otherwise it will raise an error.

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

   .. attribute:: hatSingle

      Like :data:`hatValues` but returns a single hat direction value that is set by the sensor. (read-only).

      :type: integer

   .. attribute:: numAxis

      The number of axes for the joystick at this index. (read-only).

      :type: integer

   .. attribute:: numButtons

      The number of buttons for the joystick at this index. (read-only).

      :type: integer

   .. attribute:: numHats

      The number of hats for the joystick at this index. (read-only).

      :type: integer

   .. attribute:: connected

      True if a joystick is connected at this joysticks index. (read-only).

      :type: boolean

   .. attribute:: index

      The joystick index to use (from 0 to 7). The first joystick is always 0.

      :type: integer

   .. attribute:: threshold

      Axis threshold. Joystick axis motion below this threshold wont trigger an event. Use values between (0 and 32767), lower values are more sensitive.

      :type: integer

   .. attribute:: button

      The button index the sensor reacts to (first button = 0). When the "All Events" toggle is set, this option has no effect.

      :type: integer

   .. attribute:: axis

      The axis this sensor reacts to, as a list of two values [axisIndex, axisDirection]

      * axisIndex: the axis index to use when detecting axis movement, 1=primary directional control, 2=secondary directional control.
      * axisDirection: 0=right, 1=up, 2=left, 3=down.

      :type: [integer, integer]

   .. attribute:: hat

      The hat the sensor reacts to, as a list of two values: [hatIndex, hatDirection]

      * hatIndex: the hat index to use when detecting hat movement, 1=primary hat, 2=secondary hat (4 max).
      * hatDirection: 1-12.

      :type: [integer, integer]

   .. method:: getButtonActiveList()

      :return: A list containing the indicies of the currently pressed buttons.
      :rtype: list

   .. method:: getButtonStatus(buttonIndex)

      :arg buttonIndex: the button index, 0=first button
      :type buttonIndex: integer
      :return: The current pressed state of the specified button.
      :rtype: boolean

