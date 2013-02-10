KX_SCA_AddObjectActuator(SCA_IActuator)
=======================================

.. module:: bge.types

base class --- :class:`SCA_IActuator`

.. class:: KX_SCA_AddObjectActuator(SCA_IActuator)

   Edit Object Actuator (in Add Object Mode)

   .. warning::

      An Add Object actuator will be ignored if at game start, the linked object doesn't exist (or is empty) or the linked object is in an active layer.

      .. code-block:: none

         Error: GameObject 'Name' has a AddObjectActuator 'ActuatorName' without object (in 'nonactive' layer) 
      
   .. attribute:: object

      the object this actuator adds.

      :type: :class:`KX_GameObject` or None

   .. attribute:: objectLastCreated

      the last added object from this actuator (read-only).

      :type: :class:`KX_GameObject` or None

   .. attribute:: time

      the lifetime of added objects, in frames. Set to 0 to disable automatic deletion.

      :type: integer

   .. attribute:: linearVelocity

      the initial linear velocity of added objects.

      :type: list [vx, vy, vz]

   .. attribute:: angularVelocity

      the initial angular velocity of added objects.

      :type: list [vx, vy, vz]

   .. method:: instantAddObject()

      adds the object without needing to calling SCA_PythonController.activate()

      .. note:: Use objectLastCreated to get the newly created object.

