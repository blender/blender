BL_ActionActuator(SCA_IActuator)
================================

.. module:: bge.types

base class --- :class:`SCA_IActuator`

.. class:: BL_ActionActuator(SCA_IActuator)

   Action Actuators apply an action to an actor.

   .. attribute:: action

      The name of the action to set as the current action.

      :type: string

   .. attribute:: frameStart

      Specifies the starting frame of the animation.

      :type: float

   .. attribute:: frameEnd

      Specifies the ending frame of the animation.

      :type: float

   .. attribute:: blendIn

      Specifies the number of frames of animation to generate when making transitions between actions.

      :type: float

   .. attribute:: priority

      Sets the priority of this actuator. Actuators will lower priority numbers will override actuators with higher numbers.

      :type: integer

   .. attribute:: frame

      Sets the current frame for the animation.

      :type: float

   .. attribute:: propName

      Sets the property to be used in FromProp playback mode.

      :type: string

   .. attribute:: blendTime

      Sets the internal frame timer. This property must be in the range from 0.0 to blendIn.

      :type: float

   .. attribute:: mode

      The operation mode of the actuator. Can be one of :ref:`these constants<action-actuator>`.

      :type: integer

   .. attribute:: useContinue

      The actions continue option, True or False. When True, the action will always play from where last left off,
      otherwise negative events to this actuator will reset it to its start frame.

      :type: boolean

   .. attribute:: framePropName

      The name of the property that is set to the current frame number.

      :type: string

