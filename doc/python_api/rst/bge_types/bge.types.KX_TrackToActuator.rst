KX_TrackToActuator(SCA_IActuator)
=================================

.. module:: bge.types

base class --- :class:`SCA_IActuator`

.. class:: KX_TrackToActuator(SCA_IActuator)

   Edit Object actuator in Track To mode.

   .. warning::
   
      Track To Actuators will be ignored if at game start, the object to track to is invalid.

      This will generate a warning in the console:

      .. code-block:: none

         GameObject 'Name' no object in EditObjectActuator 'ActuatorName'

   .. attribute:: object

      the object this actuator tracks.

      :type: :class:`KX_GameObject` or None

   .. attribute:: time

      the time in frames with which to delay the tracking motion.

      :type: integer

   .. attribute:: use3D

      the tracking motion to use 3D.

      :type: boolean

