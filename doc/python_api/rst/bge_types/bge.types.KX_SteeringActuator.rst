KX_SteeringActuator(SCA_IActuator)
==================================

.. module:: bge.types

base class --- :class:`SCA_IActuator`

.. class:: KX_SteeringActuator(SCA_IActuator)

   Steering Actuator for navigation.

   .. attribute:: behavior

      The steering behavior to use.

      :type: one of :ref:`these constants <logic-steering-actuator>`

   .. attribute:: velocity

      Velocity magnitude

      :type: float

   .. attribute:: acceleration

      Max acceleration

      :type: float

   .. attribute:: turnspeed

      Max turn speed

      :type: float

   .. attribute:: distance

      Relax distance

      :type: float

   .. attribute:: target

      Target object

      :type: :class:`KX_GameObject`

   .. attribute:: navmesh

      Navigation mesh

      :type: :class:`KX_GameObject`

   .. attribute:: selfterminated

      Terminate when target is reached

      :type: boolean

   .. attribute:: enableVisualization

      Enable debug visualization

      :type: boolean

   .. attribute:: pathUpdatePeriod

      Path update period

      :type: int

