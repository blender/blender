KX_VisibilityActuator(SCA_IActuator)
====================================

.. module:: bge.types

base class --- :class:`SCA_IActuator`

.. class:: KX_VisibilityActuator(SCA_IActuator)

   Visibility Actuator.

   .. attribute:: visibility

      whether the actuator makes its parent object visible or invisible.

      :type: boolean

   .. attribute:: useOcclusion

      whether the actuator makes its parent object an occluder or not.

      :type: boolean

   .. attribute:: useRecursion

      whether the visibility/occlusion should be propagated to all children of the object.

      :type: boolean

