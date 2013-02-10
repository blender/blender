KX_ObjectActuator(SCA_IActuator)
================================

.. module:: bge.types

base class --- :class:`SCA_IActuator`

.. class:: KX_ObjectActuator(SCA_IActuator)

   The object actuator ("Motion Actuator") applies force, torque, displacement, angular displacement, 
   velocity, or angular velocity to an object.
   Servo control allows to regulate force to achieve a certain speed target.

   .. attribute:: force

      The force applied by the actuator.

      :type: list [x, y, z]

   .. attribute:: useLocalForce

      A flag specifying if the force is local.

      :type: boolean

   .. attribute:: torque

      The torque applied by the actuator.

      :type: list [x, y, z]

   .. attribute:: useLocalTorque

      A flag specifying if the torque is local.

      :type: boolean

   .. attribute:: dLoc

      The displacement vector applied by the actuator.

      :type: list [x, y, z]

   .. attribute:: useLocalDLoc

      A flag specifying if the dLoc is local.

      :type: boolean

   .. attribute:: dRot

      The angular displacement vector applied by the actuator

      :type: list [x, y, z]
      
      .. note::
      
         Since the displacement is applied every frame, you must adjust the displacement based on the frame rate, or you game experience will depend on the player's computer speed.

   .. attribute:: useLocalDRot

      A flag specifying if the dRot is local.

      :type: boolean

   .. attribute:: linV

      The linear velocity applied by the actuator.

      :type: list [x, y, z]

   .. attribute:: useLocalLinV

      A flag specifying if the linear velocity is local.

      :type: boolean
      
      .. note::
      
         This is the target speed for servo controllers.

   .. attribute:: angV

      The angular velocity applied by the actuator.

      :type: list [x, y, z]

   .. attribute:: useLocalAngV

      A flag specifying if the angular velocity is local.

      :type: boolean

   .. attribute:: damping

      The damping parameter of the servo controller.

      :type: short

   .. attribute:: forceLimitX

      The min/max force limit along the X axis and activates or deactivates the limits in the servo controller.

      :type: list [min(float), max(float), bool]

   .. attribute:: forceLimitY

      The min/max force limit along the Y axis and activates or deactivates the limits in the servo controller.

      :type: list [min(float), max(float), bool]

   .. attribute:: forceLimitZ

      The min/max force limit along the Z axis and activates or deactivates the limits in the servo controller.

      :type: list [min(float), max(float), bool]

   .. attribute:: pid

      The PID coefficients of the servo controller.

      :type: list of floats [proportional, integral, derivate]

   .. attribute:: reference

      The object that is used as reference to compute the velocity for the servo controller.

      :type: :class:`KX_GameObject` or None

