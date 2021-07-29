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

   .. attribute:: shadowClipStart

      The shadowmap clip start, below which objects will not generate shadows.

      :type: float (read only)

   .. attribute:: shadowClipEnd

      The shadowmap clip end, beyond which objects will not generate shadows.

      :type: float (read only)

   .. attribute:: shadowFrustumSize

      Size of the frustum used for creating the shadowmap.

      :type: float (read only)

   .. attribute:: shadowBindId

      The OpenGL shadow texture bind number/id.

      :type: int (read only)

   .. attribute:: shadowMapType

      The shadow shadow map type (0 -> Simple; 1 -> Variance)

      :type: int (read only)

   .. attribute:: shadowBias

      The shadow buffer sampling bias.

      :type: float (read only)

   .. attribute:: shadowBleedBias

      The bias for reducing light-bleed on variance shadow maps.

      :type: float (read only)

   .. attribute:: useShadow

      Returns True if the light has Shadow option activated, else returns False.

      :type: boolean (read only)

   .. attribute:: shadowColor

      The color of this light shadows. Black = (0.0, 0.0, 0.0), White = (1.0, 1.0, 1.0).

      :type: :class:`mathutils.Color` (read only)

   .. attribute:: shadowMatrix

      Matrix that converts a vector in camera space to shadow buffer depth space.

      Computed as:
          mat4_perspective_to_depth * mat4_lamp_to_perspective * mat4_world_to_lamp * mat4_cam_to_world.

      mat4_perspective_to_depth is a fixed matrix defined as follow:

         0.5 0.0 0.0 0.5
         0.0 0.5 0.0 0.5
         0.0 0.0 0.5 0.5
         0.0 0.0 0.0 1.0

      .. note:

         There is one matrix of that type per lamp casting shadow in the scene.

      :type: Matrix4x4 (read only)

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

