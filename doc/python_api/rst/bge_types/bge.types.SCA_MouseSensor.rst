SCA_MouseSensor(SCA_ISensor)
============================

.. module:: bge.types

base class --- :class:`SCA_ISensor`

.. class:: SCA_MouseSensor(SCA_ISensor)

   Mouse Sensor logic brick.

   .. attribute:: position

      current [x, y] coordinates of the mouse, in frame coordinates (pixels).

      :type: [integer, interger]

   .. attribute:: mode

      sensor mode.

      :type: integer

         * KX_MOUSESENSORMODE_LEFTBUTTON(1)
         * KX_MOUSESENSORMODE_MIDDLEBUTTON(2)
         * KX_MOUSESENSORMODE_RIGHTBUTTON(3)
         * KX_MOUSESENSORMODE_WHEELUP(4)
         * KX_MOUSESENSORMODE_WHEELDOWN(5)
         * KX_MOUSESENSORMODE_MOVEMENT(6)

   .. method:: getButtonStatus(button)

      Get the mouse button status.
 
      :arg button: The code that represents the key you want to get the state of, use one of :ref:`these constants<mouse-keys>`
      :type button: int
      :return: The state of the given key, can be one of :ref:`these constants<input-status>`
      :rtype: int

