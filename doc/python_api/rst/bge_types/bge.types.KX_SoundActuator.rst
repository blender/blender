KX_SoundActuator(SCA_IActuator)
===============================

.. module:: bge.types

base class --- :class:`SCA_IActuator`

.. class:: KX_SoundActuator(SCA_IActuator)

   Sound Actuator.

   The :data:`startSound`, :data:`pauseSound` and :data:`stopSound` do not require the actuator to be activated - they act instantly provided that the actuator has been activated once at least.

   .. attribute:: volume

      The volume (gain) of the sound.

      :type: float

   .. attribute:: time

      The current position in the audio stream (in seconds).

      :type: float

   .. attribute:: pitch

      The pitch of the sound.

      :type: float

   .. attribute:: mode

      The operation mode of the actuator. Can be one of :ref:`these constants<logic-sound-actuator>`

      :type: integer

   .. attribute:: sound

      The sound the actuator should play.

      :type: Audaspace factory

   .. attribute:: is3D

      Whether or not the actuator should be using 3D sound. (read-only)

      :type: boolean

   .. attribute:: volume_maximum

      The maximum gain of the sound, no matter how near it is.

      :type: float

   .. attribute:: volume_minimum

      The minimum gain of the sound, no matter how far it is away.

      :type: float

   .. attribute:: distance_reference

      The distance where the sound has a gain of 1.0.

      :type: float

   .. attribute:: distance_maximum

      The maximum distance at which you can hear the sound.

      :type: float

   .. attribute:: attenuation

      The influence factor on volume depending on distance.

      :type: float

   .. attribute:: cone_angle_inner

      The angle of the inner cone.

      :type: float

   .. attribute:: cone_angle_outer

      The angle of the outer cone.

      :type: float

   .. attribute:: cone_volume_outer

      The gain outside the outer cone (the gain in the outer cone will be interpolated between this value and the normal gain in the inner cone).

      :type: float

   .. method:: startSound()

      Starts the sound.

      :return: None

   .. method:: pauseSound()

      Pauses the sound.

      :return: None

   .. method:: stopSound()

      Stops the sound.

      :return: None

