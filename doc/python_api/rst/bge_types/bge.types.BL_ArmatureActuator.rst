BL_ArmatureActuator(SCA_IActuator)
==================================

.. module:: bge.types

base class --- :class:`SCA_IActuator`

.. class:: BL_ArmatureActuator(SCA_IActuator)

   Armature Actuators change constraint condition on armatures.

   .. attribute:: type

      The type of action that the actuator executes when it is active.

      Can be one of :ref:`these constants <armatureactuator-constants-type>`

      :type: integer

   .. attribute:: constraint

      The constraint object this actuator is controlling.

      :type: :class:`BL_ArmatureConstraint`

   .. attribute:: target

      The object that this actuator will set as primary target to the constraint it controls.

      :type: :class:`KX_GameObject`

   .. attribute:: subtarget

      The object that this actuator will set as secondary target to the constraint it controls.

      :type: :class:`KX_GameObject`.
      
      .. note::
      
         Currently, the only secondary target is the pole target for IK constraint.

   .. attribute:: weight

      The weight this actuator will set on the constraint it controls.

      :type: float.

      .. note::
      
         Currently only the IK constraint has a weight. It must be a value between 0 and 1.

      .. note::
      
         A weight of 0 disables a constraint while still updating constraint runtime values (see :class:`BL_ArmatureConstraint`)

   .. attribute:: influence

      The influence this actuator will set on the constraint it controls.

      :type: float.

