KX_StateActuator(SCA_IActuator)
===============================

.. module:: bge.types

base class --- :class:`SCA_IActuator`

.. class:: KX_StateActuator(SCA_IActuator)

   State actuator changes the state mask of parent object.

   .. attribute:: operation

      Type of bit operation to be applied on object state mask.
      
      You can use one of :ref:`these constants <state-actuator-operation>`

      :type: integer

   .. attribute:: mask

      Value that defines the bits that will be modified by the operation.

      The bits that are 1 in the mask will be updated in the object state.

      The bits that are 0 are will be left unmodified expect for the Copy operation which copies the mask to the object state.

      :type: integer

