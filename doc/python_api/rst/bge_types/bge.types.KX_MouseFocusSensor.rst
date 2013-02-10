KX_MouseFocusSensor(SCA_MouseSensor)
====================================

.. module:: bge.types

base class --- :class:`SCA_MouseSensor`

.. class:: KX_MouseFocusSensor(SCA_MouseSensor)

   The mouse focus sensor detects when the mouse is over the current game object.

   The mouse focus sensor works by transforming the mouse coordinates from 2d device
   space to 3d space then raycasting away from the camera.

   .. attribute:: raySource

      The worldspace source of the ray (the view position).

      :type: list (vector of 3 floats)

   .. attribute:: rayTarget

      The worldspace target of the ray.

      :type: list (vector of 3 floats)

   .. attribute:: rayDirection

      The :data:`rayTarget` - :class:`raySource` normalized.

      :type: list (normalized vector of 3 floats)

   .. attribute:: hitObject

      the last object the mouse was over.

      :type: :class:`KX_GameObject` or None

   .. attribute:: hitPosition

      The worldspace position of the ray intersecton.

      :type: list (vector of 3 floats)

   .. attribute:: hitNormal

      the worldspace normal from the face at point of intersection.

      :type: list (normalized vector of 3 floats)

   .. attribute:: hitUV

      the UV coordinates at the point of intersection.

      :type: list (vector of 2 floats)

      If the object has no UV mapping, it returns [0, 0].

      The UV coordinates are not normalized, they can be < 0 or > 1 depending on the UV mapping.

   .. attribute:: usePulseFocus

      When enabled, moving the mouse over a different object generates a pulse. (only used when the 'Mouse Over Any' sensor option is set).

      :type: boolean

