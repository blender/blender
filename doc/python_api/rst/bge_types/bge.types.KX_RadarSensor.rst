KX_RadarSensor(KX_NearSensor)
=============================

.. module:: bge.types

base class --- :class:`KX_NearSensor`

.. class:: KX_RadarSensor(KX_NearSensor)

   Radar sensor is a near sensor with a conical sensor object.

   .. attribute:: coneOrigin

      The origin of the cone with which to test. The origin is in the middle of the cone. (read-only).

      :type: list of floats [x, y, z]

   .. attribute:: coneTarget

      The center of the bottom face of the cone with which to test. (read-only).

      :type: list of floats [x, y, z]

   .. attribute:: distance

      The height of the cone with which to test.

      :type: float

   .. attribute:: angle

      The angle of the cone (in degrees) with which to test.

      :type: float

   .. attribute:: axis

      The axis on which the radar cone is cast.

      :type: integer from 0 to 5

      KX_RADAR_AXIS_POS_X, KX_RADAR_AXIS_POS_Y, KX_RADAR_AXIS_POS_Z, 
      KX_RADAR_AXIS_NEG_X, KX_RADAR_AXIS_NEG_Y, KX_RADAR_AXIS_NEG_Z

