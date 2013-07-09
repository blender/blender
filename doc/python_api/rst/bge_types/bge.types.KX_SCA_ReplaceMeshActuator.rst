KX_SCA_ReplaceMeshActuator(SCA_IActuator)
=========================================

.. module:: bge.types

base class --- :class:`SCA_IActuator`

.. class:: KX_SCA_ReplaceMeshActuator(SCA_IActuator)

   Edit Object actuator, in Replace Mesh mode.

   .. warning::

      Replace mesh actuators will be ignored if at game start, the named mesh doesn't exist.

      This will generate a warning in the console

      .. code-block:: none
      
         Error: GameObject 'Name' ReplaceMeshActuator 'ActuatorName' without object

   .. code-block:: python

      # Level-of-detail
      # Switch a game object's mesh based on its depth in the camera view.
      # +----------+     +-----------+     +-------------------------------------+
      # | Always   +-----+ Python    +-----+ Edit Object (Replace Mesh) LOD.Mesh |
      # +----------+     +-----------+     +-------------------------------------+
      from bge import logic

      # List detail meshes here
      # Mesh (name, near, far)
      # Meshes overlap so that they don't 'pop' when on the edge of the distance.
      meshes = ((".Hi", 0.0, -20.0),
            (".Med", -15.0, -50.0),
            (".Lo", -40.0, -100.0)
          )
      
      cont = logic.getCurrentController()
      object = cont.owner
      actuator = cont.actuators["LOD." + obj.name]
      camera = logic.getCurrentScene().active_camera
      
      def Depth(pos, plane):
        return pos[0]*plane[0] + pos[1]*plane[1] + pos[2]*plane[2] + plane[3]
      
      # Depth is negative and decreasing further from the camera
      depth = Depth(object.position, camera.world_to_camera[2])
      
      newmesh = None
      curmesh = None
      # Find the lowest detail mesh for depth
      for mesh in meshes:
        if depth < mesh[1] and depth > mesh[2]:
          newmesh = mesh
        if "ME" + object.name + mesh[0] == actuator.getMesh():
            curmesh = mesh
      
      if newmesh != None and "ME" + object.name + newmesh[0] != actuator.mesh:
        # The mesh is a different mesh - switch it.
        # Check the current mesh is not a better fit.
        if curmesh == None or curmesh[1] < depth or curmesh[2] > depth:
          actuator.mesh = object.name + newmesh[0]
          cont.activate(actuator)

   .. attribute:: mesh

      :class:`MeshProxy` or the name of the mesh that will replace the current one.
   
      Set to None to disable actuator.

      :type: :class:`MeshProxy` or None if no mesh is set

   .. attribute:: useDisplayMesh

      when true the displayed mesh is replaced.

      :type: boolean

   .. attribute:: usePhysicsMesh

      when true the physics mesh is replaced.

      :type: boolean

   .. method:: instantReplaceMesh()

      Immediately replace mesh without delay.

