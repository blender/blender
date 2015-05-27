KX_FontObject(KX_GameObject)
============================

.. module:: bge.types

base class --- :class:`KX_GameObject`

.. class:: KX_FontObject(KX_GameObject)

   A Font object.

   .. code-block:: python

      # Display a message about the exit key using a Font object.
      import bge

      co = bge.logic.getCurrentController()
      font = co.owner

      exit_key = bge.events.EventToString(bge.logic.getExitKey())

      if exit_key.endswith("KEY"):
          exit_key = exit_key[:-3]

      font.text = "Press key '%s' to quit the game." % exit_key

   .. attribute:: text

      The text displayed by this Font object.

      :type: string

