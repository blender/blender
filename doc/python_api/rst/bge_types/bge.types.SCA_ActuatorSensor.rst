SCA_ActuatorSensor(SCA_ISensor)
===============================

.. module:: bge.types

base class --- :class:`SCA_ISensor`

.. class:: SCA_ActuatorSensor(SCA_ISensor)

   Actuator sensor detect change in actuator state of the parent object.
   It generates a positive pulse if the corresponding actuator is activated
   and a negative pulse if the actuator is deactivated.

   .. attribute:: actuator

      the name of the actuator that the sensor is monitoring.

      :type: string

