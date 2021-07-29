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

   .. attribute:: upAxis

      The axis that points upward.

      :type: integer from 0 to 2

      * KX_TRACK_UPAXIS_POS_X
      * KX_TRACK_UPAXIS_POS_Y
      * KX_TRACK_UPAXIS_POS_Z

   .. attribute:: trackAxis

      The axis that points to the target object.

      :type: integer from 0 to 5

      * KX_TRACK_TRAXIS_POS_X
      * KX_TRACK_TRAXIS_POS_Y
      * KX_TRACK_TRAXIS_POS_Z
      * KX_TRACK_TRAXIS_NEG_X
      * KX_TRACK_TRAXIS_NEG_Y
      * KX_TRACK_TRAXIS_NEG_Z
