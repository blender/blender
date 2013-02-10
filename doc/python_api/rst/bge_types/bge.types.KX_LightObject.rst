KX_LightObject(KX_GameObject)
=============================

.. module:: bge.types

base class --- :class:`KX_GameObject`

.. class:: KX_LightObject(KX_GameObject)

   A Light object.

   .. code-block:: python

      # Turn on a red alert light.
      import bge

      co = bge.logic.getCurrentController()
      light = co.owner

      light.energy = 1.0
      light.color = [1.0, 0.0, 0.0]

   .. data:: SPOT

      A spot light source. See attribute :data:`type`

   .. data:: SUN

      A point light source with no attenuation. See attribute :data:`type`

   .. data:: NORMAL

      A point light source. See attribute :data:`type`

   .. attribute:: type

      The type of light - must be SPOT, SUN or NORMAL

   .. attribute:: layer

      The layer mask that this light affects object on.

      :type: bitfield

   .. attribute:: energy

      The brightness of this light.

      :type: float

   .. attribute:: distance

      The maximum distance this light can illuminate. (SPOT and NORMAL lights only).

      :type: float

   .. attribute:: color

      The color of this light. Black = [0.0, 0.0, 0.0], White = [1.0, 1.0, 1.0].

      :type: list [r, g, b]

   .. attribute:: lin_attenuation

      The linear component of this light's attenuation. (SPOT and NORMAL lights only).

      :type: float

   .. attribute:: quad_attenuation

      The quadratic component of this light's attenuation (SPOT and NORMAL lights only).

      :type: float

   .. attribute:: spotsize

      The cone angle of the spot light, in degrees (SPOT lights only).

      :type: float in [0 - 180].

   .. attribute:: spotblend

      Specifies the intensity distribution of the spot light (SPOT lights only).

      :type: float in [0 - 1]

      .. note::
         
         Higher values result in a more focused light source.

