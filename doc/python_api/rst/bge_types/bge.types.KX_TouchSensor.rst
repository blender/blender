KX_TouchSensor(SCA_ISensor)
===========================

.. module:: bge.types

base class --- :class:`SCA_ISensor`

.. class:: KX_TouchSensor(SCA_ISensor)

   Touch sensor detects collisions between objects.

   .. attribute:: propName

      The property or material to collide with.

      :type: string

   .. attribute:: useMaterial

      Determines if the sensor is looking for a property or material. KX_True = Find material; KX_False = Find property.

      :type: boolean

   .. attribute:: usePulseCollision

      When enabled, changes to the set of colliding objects generate a pulse.

      :type: boolean

   .. attribute:: hitObject

      The last collided object. (read-only).

      :type: :class:`KX_GameObject` or None

   .. attribute:: hitObjectList

      A list of colliding objects. (read-only).

      :type: :class:`CListValue` of :class:`KX_GameObject`

   .. attribute:: hitMaterial

      The material of the object in the face hit by the ray. (read-only).

      :type: string

