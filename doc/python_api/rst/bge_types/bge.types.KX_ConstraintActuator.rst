KX_ConstraintActuator(SCA_IActuator)
====================================

.. module:: bge.types

base class --- :class:`SCA_IActuator`

.. class:: KX_ConstraintActuator(SCA_IActuator)

   A constraint actuator limits the position, rotation, distance or orientation of an object.

   .. attribute:: damp

      Time constant of the constraint expressed in frame (not use by Force field constraint).

      :type: integer

   .. attribute:: rotDamp

      Time constant for the rotation expressed in frame (only for the distance constraint), 0 = use damp for rotation as well.

      :type: integer

   .. attribute:: direction

      The reference direction in world coordinate for the orientation constraint.

      :type: 3-tuple of float: (x, y, z)

   .. attribute:: option

      Binary combination of :ref:`these constants <constraint-actuator-option>`

      :type: integer

   .. attribute:: time

      activation time of the actuator. The actuator disables itself after this many frame. If set to 0, the actuator is not limited in time.

      :type: integer

   .. attribute:: propName

      the name of the property or material for the ray detection of the distance constraint.

      :type: string

   .. attribute:: min

      The lower bound of the constraint. For the rotation and orientation constraint, it represents radiant.

      :type: float

   .. attribute:: distance

      the target distance of the distance constraint.

      :type: float

   .. attribute:: max

      the upper bound of the constraint. For rotation and orientation constraints, it represents radiant.

      :type: float

   .. attribute:: rayLength

      the length of the ray of the distance constraint.

      :type: float

   .. attribute:: limit

      type of constraint. Use one of the :ref:`these constants <constraint-actuator-limit>`

      :type: integer.

      
