KX_LibLoadStatus(PyObjectPlus)
==============================

.. module:: bge.types

base class --- :class:`PyObjectPlus`

.. class:: KX_LibLoadStatus(PyObjectPlus)

   An object providing information about a LibLoad() operation.

   .. code-block:: python

      # Print a message when an async LibLoad is done
      import bge

      def finished_cb(status):
          print("Library (%s) loaded in %.2fms." % (status.libraryName, status.timeTaken))

      bge.logic.LibLoad('myblend.blend', 'Scene', async=True).onFinish = finished_cb

   .. attribute:: onFinish

      A callback that gets called when the lib load is done.

      :type: callable

   .. attribute:: progress

      The current progress of the lib load as a normalized value from 0.0 to 1.0.

      :type: float

   .. attribute:: libraryName

      The name of the library being loaded (the first argument to LibLoad).

      :type: string

   .. attribute:: timeTaken

      The amount of time, in seconds, the lib load took (0 until the operation is complete).

      :type: float

