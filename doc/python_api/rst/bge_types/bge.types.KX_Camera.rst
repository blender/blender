KX_Camera(KX_GameObject)
========================

.. module:: bge.types

base class --- :class:`KX_GameObject`

.. class:: KX_Camera(KX_GameObject)

   A Camera object.

   .. data:: INSIDE

      See :data:`sphereInsideFrustum` and :data:`boxInsideFrustum`

   .. data:: INTERSECT

      See :data:`sphereInsideFrustum` and :data:`boxInsideFrustum`

   .. data:: OUTSIDE

      See :data:`sphereInsideFrustum` and :data:`boxInsideFrustum`

   .. attribute:: lens

      The camera's lens value.

      :type: float

   .. attribute:: fov

      The camera's field of view value.

      :type: float

   .. attribute:: ortho_scale

      The camera's view scale when in orthographic mode.

      :type: float

   .. attribute:: near

      The camera's near clip distance.

      :type: float

   .. attribute:: far

      The camera's far clip distance.

      :type: float

   .. attribute:: perspective

      True if this camera has a perspective transform, False for an orthographic projection.

      :type: boolean

   .. attribute:: frustum_culling

      True if this camera is frustum culling.

      :type: boolean

   .. attribute:: projection_matrix

      This camera's 4x4 projection matrix.

      .. note::
      
         This is the identity matrix prior to rendering the first frame (any Python done on frame 1). 

      :type: 4x4 Matrix [[float]]

   .. attribute:: modelview_matrix

      This camera's 4x4 model view matrix. (read-only).

      :type: 4x4 Matrix [[float]]

      .. note::
      
         This matrix is regenerated every frame from the camera's position and orientation. Also, this is the identity matrix prior to rendering the first frame (any Python done on frame 1).

   .. attribute:: camera_to_world

      This camera's camera to world transform. (read-only).

      :type: 4x4 Matrix [[float]]

      .. note::
      
         This matrix is regenerated every frame from the camera's position and orientation.

   .. attribute:: world_to_camera

      This camera's world to camera transform. (read-only).

      :type: 4x4 Matrix [[float]]

      .. note::
         
         Regenerated every frame from the camera's position and orientation.

      .. note::
      
         This is camera_to_world inverted.

   .. attribute:: useViewport

      True when the camera is used as a viewport, set True to enable a viewport for this camera.

      :type: boolean

   .. method:: sphereInsideFrustum(centre, radius)

      Tests the given sphere against the view frustum.

      :arg centre: The centre of the sphere (in world coordinates.)
      :type centre: list [x, y, z]
      :arg radius: the radius of the sphere
      :type radius: float
      :return: :data:`~bge.types.KX_Camera.INSIDE`, :data:`~bge.types.KX_Camera.OUTSIDE` or :data:`~bge.types.KX_Camera.INTERSECT`
      :rtype: integer

      .. note::

         When the camera is first initialized the result will be invalid because the projection matrix has not been set.

      .. code-block:: python

         from bge import logic
         cont = logic.getCurrentController()
         cam = cont.owner
         
         # A sphere of radius 4.0 located at [x, y, z] = [1.0, 1.0, 1.0]
         if (cam.sphereInsideFrustum([1.0, 1.0, 1.0], 4) != cam.OUTSIDE):
             # Sphere is inside frustum !
             # Do something useful !
         else:
             # Sphere is outside frustum

   .. method:: boxInsideFrustum(box)

      Tests the given box against the view frustum.

      :arg box: Eight (8) corner points of the box (in world coordinates.)
      :type box: list of lists
      :return: :data:`~bge.types.KX_Camera.INSIDE`, :data:`~bge.types.KX_Camera.OUTSIDE` or :data:`~bge.types.KX_Camera.INTERSECT`

      .. note::
      
         When the camera is first initialized the result will be invalid because the projection matrix has not been set.

      .. code-block:: python

         from bge import logic
         cont = logic.getCurrentController()
         cam = cont.owner

         # Box to test...
         box = []
         box.append([-1.0, -1.0, -1.0])
         box.append([-1.0, -1.0,  1.0])
         box.append([-1.0,  1.0, -1.0])
         box.append([-1.0,  1.0,  1.0])
         box.append([ 1.0, -1.0, -1.0])
         box.append([ 1.0, -1.0,  1.0])
         box.append([ 1.0,  1.0, -1.0])
         box.append([ 1.0,  1.0,  1.0])
         
         if (cam.boxInsideFrustum(box) != cam.OUTSIDE):
           # Box is inside/intersects frustum !
           # Do something useful !
         else:
           # Box is outside the frustum !
           
   .. method:: pointInsideFrustum(point)

      Tests the given point against the view frustum.

      :arg point: The point to test (in world coordinates.)
      :type point: 3D Vector
      :return: True if the given point is inside this camera's viewing frustum.
      :rtype: boolean

      .. note::
      
         When the camera is first initialized the result will be invalid because the projection matrix has not been set.

      .. code-block:: python

         from bge import logic
         cont = logic.getCurrentController()
         cam = cont.owner

         # Test point [0.0, 0.0, 0.0]
         if (cam.pointInsideFrustum([0.0, 0.0, 0.0])):
           # Point is inside frustum !
           # Do something useful !
         else:
           # Box is outside the frustum !

   .. method:: getCameraToWorld()

      Returns the camera-to-world transform.

      :return: the camera-to-world transform matrix.
      :rtype: matrix (4x4 list)

   .. method:: getWorldToCamera()

      Returns the world-to-camera transform.

      This returns the inverse matrix of getCameraToWorld().

      :return: the world-to-camera transform matrix.
      :rtype: matrix (4x4 list)

   .. method:: setOnTop()

      Set this cameras viewport ontop of all other viewport.

   .. method:: setViewport(left, bottom, right, top)

      Sets the region of this viewport on the screen in pixels.

      Use :data:`bge.render.getWindowHeight` and :data:`bge.render.getWindowWidth` to calculate values relative to the entire display.

      :arg left: left pixel coordinate of this viewport
      :type left: integer
      :arg bottom: bottom pixel coordinate of this viewport
      :type bottom: integer
      :arg right: right pixel coordinate of this viewport
      :type right: integer
      :arg top: top pixel coordinate of this viewport
      :type top: integer

   .. method:: getScreenPosition(object)

      Gets the position of an object projected on screen space.

      .. code-block:: python

         # For an object in the middle of the screen, coord = [0.5, 0.5]
         coord = camera.getScreenPosition(object)

      :arg object: object name or list [x, y, z]
      :type object: :class:`KX_GameObject` or 3D Vector
      :return: the object's position in screen coordinates.
      :rtype: list [x, y]

   .. method:: getScreenVect(x, y)

      Gets the vector from the camera position in the screen coordinate direction.

      :arg x: X Axis
      :type x: float
      :arg y: Y Axis
      :type y: float
      :rtype: 3D Vector
      :return: The vector from screen coordinate.

      .. code-block:: python

         # Gets the vector of the camera front direction:
         m_vect = camera.getScreenVect(0.5, 0.5)

   .. method:: getScreenRay(x, y, dist=inf, property=None)

      Look towards a screen coordinate (x, y) and find first object hit within dist that matches prop.
      The ray is similar to KX_GameObject->rayCastTo.

      :arg x: X Axis
      :type x: float
      :arg y: Y Axis
      :type y: float
      :arg dist: max distance to look (can be negative => look behind); 0 or omitted => detect up to other
      :type dist: float
      :arg property: property name that object must have; can be omitted => detect any object
      :type property: string
      :rtype: :class:`KX_GameObject`
      :return: the first object hit or None if no object or object does not match prop

      .. code-block:: python

         # Gets an object with a property "wall" in front of the camera within a distance of 100:
         target = camera.getScreenRay(0.5, 0.5, 100, "wall")
         
