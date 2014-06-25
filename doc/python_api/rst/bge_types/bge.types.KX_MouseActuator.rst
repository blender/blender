KX_MouseActuator(SCA_IActuator)
====================================

.. module:: bge.types

base class --- :class:`SCA_IActuator`

.. class:: KX_MouseActuator(SCA_IActuator)

   The mouse actuator gives control over the visibility of the mouse cursor and rotates the parent object according to mouse movement.

   .. method:: reset()

      Undoes the rotation caused by the mouse actuator.

   .. attribute:: visible

      The visibility of the mouse cursor.

      :type: boolean

   .. attribute:: use_axis_x

      Mouse movement along the x axis effects object rotation.

      :type: boolean

   .. attribute:: use_axis_y

      Mouse movement along the y axis effects object rotation.

      :type: boolean

   .. attribute:: threshold

      Amount of movement from the mouse required before rotation is triggered.

      :type: list (vector of 2 floats)

      The values in the list should be between 0.0 and 0.5.

   .. attribute:: reset_x

      Mouse is locked to the center of the screen on the x axis.

      :type: boolean

   .. attribute:: reset_y

      Mouse is locked to the center of the screen on the y axis.

      :type: boolean

   .. attribute:: object_axis

      The object's 3D axis to rotate with the mouse movement. ([x, y])

      :type: list (vector of 2 integers from 0 to 2)

      * KX_ACT_MOUSE_OBJECT_AXIS_X
      * KX_ACT_MOUSE_OBJECT_AXIS_Y
      * KX_ACT_MOUSE_OBJECT_AXIS_Z

   .. attribute:: local_x

      Rotation caused by mouse movement along the x axis is local.

      :type: boolean

   .. attribute:: local_y

      Rotation caused by mouse movement along the y axis is local.

      :type: boolean

   .. attribute:: sensitivity

      The amount of rotation caused by mouse movement along the x and y axis.

      :type: list (vector of 2 floats)

      Negative values invert the rotation.

   .. attribute:: limit_x

      The minimum and maximum angle of rotation caused by mouse movement along the x axis in degrees.
      limit_x[0] is minimum, limit_x[1] is maximum.

      :type: list (vector of 2 floats)

   .. attribute:: limit_y

      The minimum and maximum angle of rotation caused by mouse movement along the y axis in degrees.
      limit_y[0] is minimum, limit_y[1] is maximum.

      :type: list (vector of 2 floats)

   .. attribute:: angle

      The current rotational offset caused by the mouse actuator in degrees.

      :type: list (vector of 2 floats)

