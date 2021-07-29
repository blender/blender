KX_NetworkMessageActuator(SCA_IActuator)
========================================

.. module:: bge.types

base class --- :class:`SCA_IActuator`

.. class:: KX_NetworkMessageActuator(SCA_IActuator)

   Message Actuator

   .. attribute:: propName

      Messages will only be sent to objects with the given property name.

      :type: string

   .. attribute:: subject

      The subject field of the message.

      :type: string

   .. attribute:: body

      The body of the message.

      :type: string

   .. attribute:: usePropBody

      Send a property instead of a regular body message.

      :type: boolean

