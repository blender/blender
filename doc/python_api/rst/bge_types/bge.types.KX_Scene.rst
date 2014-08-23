KX_Scene(PyObjectPlus)
======================

.. module:: bge.types

base class --- :class:`PyObjectPlus`

.. class:: KX_Scene(PyObjectPlus)

   An active scene that gives access to objects, cameras, lights and scene attributes.

   The activity culling stuff is supposed to disable logic bricks when their owner gets too far
   from the active camera.  It was taken from some code lurking at the back of KX_Scene - who knows
   what it does!

   .. code-block:: python

      from bge import logic

      # get the scene
      scene = logic.getCurrentScene()

      # print all the objects in the scene
      for object in scene.objects:
         print(object.name)

      # get an object named 'Cube'
      object = scene.objects["Cube"]

      # get the first object in the scene.
      object = scene.objects[0]

   .. code-block:: python

      # Get the depth of an object in the camera view.
      from bge import logic

      object = logic.getCurrentController().owner
      cam = logic.getCurrentScene().active_camera

      # Depth is negative and decreasing further from the camera
      depth = object.position[0]*cam.world_to_camera[2][0] + object.position[1]*cam.world_to_camera[2][1] + object.position[2]*cam.world_to_camera[2][2] + cam.world_to_camera[2][3]

   @bug: All attributes are read only at the moment.

   .. attribute:: name

      The scene's name, (read-only).

      :type: string

   .. attribute:: objects

      A list of objects in the scene, (read-only).

      :type: :class:`CListValue` of :class:`KX_GameObject`

   .. attribute:: objectsInactive

      A list of objects on background layers (used for the addObject actuator), (read-only).

      :type: :class:`CListValue` of :class:`KX_GameObject`

   .. attribute:: lights

      A list of lights in the scene, (read-only).

      :type: :class:`CListValue` of :class:`KX_LightObject`

   .. attribute:: cameras

      A list of cameras in the scene, (read-only).

      :type: :class:`CListValue` of :class:`KX_Camera`

   .. attribute:: active_camera

      The current active camera.

      :type: :class:`KX_Camera`
      
      .. note::
         
         This can be set directly from python to avoid using the :class:`KX_SceneActuator`.

   .. attribute:: suspended

      True if the scene is suspended, (read-only).

      :type: boolean

   .. attribute:: activity_culling

      True if the scene is activity culling.

      :type: boolean

   .. attribute:: activity_culling_radius

      The distance outside which to do activity culling. Measured in manhattan distance.

      :type: float

   .. attribute:: dbvt_culling

      True when Dynamic Bounding box Volume Tree is set (read-only).

      :type: boolean

   .. attribute:: pre_draw

      A list of callables to be run before the render step.

      :type: list

   .. attribute:: post_draw

      A list of callables to be run after the render step.

      :type: list

   .. attribute:: gravity

      The scene gravity using the world x, y and z axis.

      :type: list [fx, fy, fz]

   .. method:: addObject(object, other, time=0)

      Adds an object to the scene like the Add Object Actuator would.

      :arg object: The object to add
      :type object: :class:`KX_GameObject` or string
      :arg other: The object's center to use when adding the object
      :type other: :class:`KX_GameObject` or string
      :arg time: The lifetime of the added object, in frames. A time of 0 means the object will last forever.
      :type time: integer
      :return: The newly added object.
      :rtype: :class:`KX_GameObject`

   .. method:: end()

      Removes the scene from the game.

   .. method:: restart()

      Restarts the scene.

   .. method:: replace(scene)

      Replaces this scene with another one.

      :arg scene: The name of the scene to replace this scene with.
      :type scene: string
      :return: True if the scene exists and was scheduled for addition, False otherwise.
      :rtype: boolean

   .. method:: suspend()

      Suspends this scene.

   .. method:: resume()

      Resume this scene.

   .. method:: get(key, default=None)

      Return the value matching key, or the default value if its not found.
      :return: The key value or a default.

   .. method:: drawObstacleSimulation()

      Draw debug visualization of obstacle simulation.

