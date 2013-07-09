KX_SCA_DynamicActuator(SCA_IActuator)
=====================================

.. module:: bge.types

base class --- :class:`SCA_IActuator`

.. class:: KX_SCA_DynamicActuator(SCA_IActuator)

   Dynamic Actuator.

   .. attribute:: mode

      :type: integer

      the type of operation of the actuator, 0-4

      * KX_DYN_RESTORE_DYNAMICS(0)
      * KX_DYN_DISABLE_DYNAMICS(1)
      * KX_DYN_ENABLE_RIGID_BODY(2)
      * KX_DYN_DISABLE_RIGID_BODY(3)
      * KX_DYN_SET_MASS(4)

   .. attribute:: mass

      the mass value for the KX_DYN_SET_MASS operation.

      :type: float

