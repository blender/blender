SCA_PythonKeyboard(PyObjectPlus)
================================

base class --- :class:`PyObjectPlus`

.. class:: SCA_PythonKeyboard(PyObjectPlus)

   The current keyboard.

   .. attribute:: events

      A dictionary containing the status of each keyboard event or key. (read-only).

      :type: dictionary {:ref:`keycode<keyboard-keys>`::ref:`status<input-status>`, ...}

   .. attribute:: active_events

      A dictionary containing the status of only the active keyboard events or keys. (read-only).

      :type: dictionary {:ref:`keycode<keyboard-keys>`::ref:`status<input-status>`, ...}


   .. method:: getClipboard()

      Gets the clipboard text.

      :rtype: string

   .. method:: setClipboard(text)

      Sets the clipboard text.

      :arg text: New clipboard text
      :type text: string
