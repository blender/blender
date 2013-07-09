SCA_2DFilterActuator(SCA_IActuator)
===================================

.. module:: bge.types

base class --- :class:`SCA_IActuator`

.. class:: SCA_2DFilterActuator(SCA_IActuator)

   Create, enable and disable 2D filters

   The following properties don't have an immediate effect.
   You must active the actuator to get the result.
   The actuator is not persistent: it automatically stops itself after setting up the filter
   but the filter remains active. To stop a filter you must activate the actuator with 'type'
   set to :data:`~bge.logic.RAS_2DFILTER_DISABLED` or :data:`~bge.logic.RAS_2DFILTER_NOFILTER`.

   .. attribute:: shaderText

      shader source code for custom shader.

      :type: string

   .. attribute:: disableMotionBlur

      action on motion blur: 0=enable, 1=disable.

      :type: integer

   .. attribute:: mode

      Type of 2D filter, use one of :ref:`these constants <Two-D-FilterActuator-mode>`

      :type: integer

   .. attribute:: passNumber

      order number of filter in the stack of 2D filters. Filters are executed in increasing order of passNb.

      Only be one filter can be defined per passNb.

      :type: integer (0-100)

   .. attribute:: value

      argument for motion blur filter.

      :type: float (0.0-100.0)

