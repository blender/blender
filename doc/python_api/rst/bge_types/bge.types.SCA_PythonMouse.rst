SCA_PythonMouse(PyObjectPlus)
=============================

.. module:: bge.types

base class --- :class:`PyObjectPlus`

.. class:: SCA_PythonMouse(PyObjectPlus)

   The current mouse.

   .. attribute:: events

      a dictionary containing the status of each mouse event. (read-only).

      :type: dictionary {:ref:`keycode<mouse-keys>`::ref:`status<input-status>`, ...}

   .. attribute:: active_events

      a dictionary containing the status of only the active mouse events. (read-only).

      :type: dictionary {:ref:`keycode<mouse-keys>`::ref:`status<input-status>`, ...}
      
   .. attribute:: position

      The normalized x and y position of the mouse cursor.

      :type: list [x, y]

   .. attribute:: visible

      The visibility of the mouse cursor.
      
      :type: boolean

