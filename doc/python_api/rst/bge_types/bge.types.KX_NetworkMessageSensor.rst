KX_NetworkMessageSensor(SCA_ISensor)
====================================

.. module:: bge.types

base class --- :class:`SCA_ISensor`

.. class:: KX_NetworkMessageSensor(SCA_ISensor)

   The Message Sensor logic brick.

   Currently only loopback (local) networks are supported.

   .. attribute:: subject

      The subject the sensor is looking for.

      :type: string

   .. attribute:: frameMessageCount

      The number of messages received since the last frame. (read-only).

      :type: integer

   .. attribute:: subjects

      The list of message subjects received. (read-only).

      :type: list of strings

   .. attribute:: bodies

      The list of message bodies received. (read-only).

      :type: list of strings


