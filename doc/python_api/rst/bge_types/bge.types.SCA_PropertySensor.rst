SCA_PropertySensor(SCA_ISensor)
===============================

.. module:: bge.types

base class --- :class:`SCA_ISensor`

.. class:: SCA_PropertySensor(SCA_ISensor)

   Activates when the game object property matches.

   .. attribute:: mode

      Type of check on the property. Can be one of :ref:`these constants <logic-property-sensor>`

      :type: integer.

   .. attribute:: propName

      the property the sensor operates.

      :type: string

   .. attribute:: value

      the value with which the sensor compares to the value of the property.

      :type: string

   .. attribute:: min

      the minimum value of the range used to evaluate the property when in interval mode.

      :type: string

   .. attribute:: max

      the maximum value of the range used to evaluate the property when in interval mode.

      :type: string

