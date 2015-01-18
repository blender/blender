KX_GameObject(SCA_IObject)
==========================

.. module:: bge.types

base class --- :class:`SCA_IObject`

.. class:: KX_GameObject(SCA_IObject)

   All game objects are derived from this class.

   Properties assigned to game objects are accessible as attributes of this class.

   .. note::

      Calling ANY method or attribute on an object that has been removed from a scene will raise a SystemError,
      if an object may have been removed since last accessing it use the :data:`invalid` attribute to check.

   KX_GameObject can be subclassed to extend functionality. For example:

   .. code-block:: python

        import bge

        class CustomGameObject(bge.types.KX_GameObject):
            RATE = 0.05

            def __init__(self, old_owner):
                # "old_owner" can just be ignored. At this point, "self" is
                # already the object in the scene, and "old_owner" has been
                # destroyed.

                # New attributes can be defined - but we could also use a game
                # property, like "self['rate']".
                self.rate = CustomGameObject.RATE

            def update(self):
                self.worldPosition.z += self.rate

                # switch direction
                if self.worldPosition.z > 1.0:
                    self.rate = -CustomGameObject.RATE
                elif self.worldPosition.z < 0.0:
                    self.rate = CustomGameObject.RATE

        # Called first
        def mutate(cont):
            old_object = cont.owner
            mutated_object = CustomGameObject(cont.owner)

            # After calling the constructor above, references to the old object
            # should not be used.
            assert(old_object is not mutated_object)
            assert(old_object.invalid)
            assert(mutated_object is cont.owner)

        # Called later - note we are now working with the mutated object.
        def update(cont):
            cont.owner.update()

   When subclassing objects other than empties and meshes, the specific type
   should be used - e.g. inherit from :class:`BL_ArmatureObject` when the object
   to mutate is an armature.

   .. attribute:: name

      The object's name. (read-only).

      :type: string

   .. attribute:: mass

      The object's mass

      :type: float

      .. note::

         The object must have a physics controller for the mass to be applied, otherwise the mass value will be returned as 0.0.

   .. attribute:: linearDamping

      The object's linear damping, also known as translational damping. Can be set simultaneously with angular damping using the :py:meth:`setDamping` method.

      :type: float between 0 and 1 inclusive.

      .. note::

         The object must have a physics controller for the linear damping to be applied, otherwise the value will be returned as 0.0.

   .. attribute:: angularDamping

      The object's angular damping, also known as rotationation damping. Can be set simultaneously with linear damping using the :py:meth:`setDamping` method.

      :type: float between 0 and 1 inclusive.

      .. note::

         The object must have a physics controller for the angular damping to be applied, otherwise the value will be returned as 0.0.


   .. attribute:: linVelocityMin

      Enforces the object keeps moving at a minimum velocity.

      :type: float

      .. note::

         Applies to dynamic and rigid body objects only.

      .. note::

         A value of 0.0 disables this option.

      .. note::

         While objects are stationary the minimum velocity will not be applied.

   .. attribute:: linVelocityMax

      Clamp the maximum linear velocity to prevent objects moving beyond a set speed.

      :type: float

      .. note::

         Applies to dynamic and rigid body objects only.

      .. note::

         A value of 0.0 disables this option (rather then setting it stationary).

   .. attribute:: localInertia

      the object's inertia vector in local coordinates. Read only.

      :type: Vector((ix, iy, iz))

   .. attribute:: parent

      The object's parent object. (read-only).

      :type: :class:`KX_GameObject` or None

   .. attribute:: groupMembers

      Returns the list of group members if the object is a group object, otherwise None is returned.

      :type: :class:`CListValue` of :class:`KX_GameObject` or None

   .. attribute:: groupObject

      Returns the group object that the object belongs to or None if the object is not part of a group.

      :type: :class:`KX_GameObject` or None

   .. attribute:: collisionCallbacks

      A list of callables to be run when a collision occurs.

      :type: list

   .. attribute:: scene

      The object's scene. (read-only).

      :type: :class:`KX_Scene` or None

   .. attribute:: visible

      visibility flag.

      :type: boolean

      .. note::

         Game logic will still run for invisible objects.

   .. attribute:: record_animation

      Record animation for this object.

      :type: boolean

   .. attribute:: color

      The object color of the object. [r, g, b, a]

      :type: :class:`mathutils.Vector`

   .. attribute:: occlusion

      occlusion capability flag.

      :type: boolean

   .. attribute:: position

      The object's position. [x, y, z] On write: local position, on read: world position

      .. deprecated:: use :data:`localPosition` and :data:`worldPosition`.

      :type: :class:`mathutils.Vector`

   .. attribute:: orientation

      The object's orientation. 3x3 Matrix. You can also write a Quaternion or Euler vector. On write: local orientation, on read: world orientation

      .. deprecated:: use :data:`localOrientation` and :data:`worldOrientation`.

      :type: :class:`mathutils.Matrix`

   .. attribute:: scaling

      The object's scaling factor. [sx, sy, sz] On write: local scaling, on read: world scaling

      .. deprecated:: use :data:`localScale` and :data:`worldScale`.

      :type: :class:`mathutils.Vector`

   .. attribute:: localOrientation

      The object's local orientation. 3x3 Matrix. You can also write a Quaternion or Euler vector.

      :type: :class:`mathutils.Matrix`

   .. attribute:: worldOrientation

      The object's world orientation. 3x3 Matrix.

      :type: :class:`mathutils.Matrix`

   .. attribute:: localScale

      The object's local scaling factor. [sx, sy, sz]

      :type: :class:`mathutils.Vector`

   .. attribute:: worldScale

      The object's world scaling factor. [sx, sy, sz]

      :type: :class:`mathutils.Vector`

   .. attribute:: localPosition

      The object's local position. [x, y, z]

      :type: :class:`mathutils.Vector`

   .. attribute:: worldPosition

      The object's world position. [x, y, z]

      :type: :class:`mathutils.Vector`

   .. attribute:: localTransform

      The object's local space transform matrix. 4x4 Matrix.

      :type: :class:`mathutils.Matrix`

   .. attribute:: worldTransform

      The object's world space transform matrix. 4x4 Matrix.

      :type: :class:`mathutils.Matrix`

   .. attribute:: localLinearVelocity

      The object's local linear velocity. [x, y, z]

      :type: :class:`mathutils.Vector`

   .. attribute:: worldLinearVelocity

      The object's world linear velocity. [x, y, z]

      :type: :class:`mathutils.Vector`

   .. attribute:: localAngularVelocity

      The object's local angular velocity. [x, y, z]

      :type: :class:`mathutils.Vector`

   .. attribute:: worldAngularVelocity

      The object's world angular velocity. [x, y, z]

      :type: :class:`mathutils.Vector`

   .. attribute:: timeOffset

      adjust the slowparent delay at runtime.

      :type: float

   .. attribute:: state

      the game object's state bitmask, using the first 30 bits, one bit must always be set.

      :type: int

   .. attribute:: meshes

      a list meshes for this object.

      :type: list of :class:`KX_MeshProxy`

      .. note::

         Most objects use only 1 mesh.

      .. note::

         Changes to this list will not update the KX_GameObject.

   .. attribute:: sensors

      a sequence of :class:`SCA_ISensor` objects with string/index lookups and iterator support.

      :type: list

      .. note::

         This attribute is experemental and may be removed (but probably wont be).

      .. note::

         Changes to this list will not update the KX_GameObject.

   .. attribute:: controllers

      a sequence of :class:`SCA_IController` objects with string/index lookups and iterator support.

      :type: list of :class:`SCA_ISensor`

      .. note::

         This attribute is experemental and may be removed (but probably wont be).

      .. note::

         Changes to this list will not update the KX_GameObject.

   .. attribute:: actuators

      a list of :class:`SCA_IActuator` with string/index lookups and iterator support.

      :type: list

      .. note::

         This attribute is experemental and may be removed (but probably wont be).

      .. note::

         Changes to this list will not update the KX_GameObject.

   .. attribute:: attrDict

      get the objects internal python attribute dictionary for direct (faster) access.

      :type: dict

   .. attribute:: children

      direct children of this object, (read-only).

      :type: :class:`CListValue` of :class:`KX_GameObject`'s

   .. attribute:: childrenRecursive

      all children of this object including childrens children, (read-only).

      :type: :class:`CListValue` of :class:`KX_GameObject`'s

   .. attribute:: life

      The number of seconds until the object ends, assumes 50fps.
      (when added with an add object actuator), (read-only).

      :type: float

   .. attribute:: debug

      If true, the object's debug properties will be displayed on screen.

      :type: boolean

   .. attribute:: debugRecursive

      If true, the object's and children's debug properties will be displayed on screen.

      :type: boolean

   .. method:: endObject()

      Delete this object, can be used in place of the EndObject Actuator.

      The actual removal of the object from the scene is delayed.

   .. method:: replaceMesh(mesh, useDisplayMesh=True, usePhysicsMesh=False)

      Replace the mesh of this object with a new mesh. This works the same was as the actuator.

      :arg mesh: mesh to replace or the meshes name.
      :type mesh: :class:`MeshProxy` or string
      :arg useDisplayMesh: when enabled the display mesh will be replaced (optional argument).
      :type useDisplayMesh: boolean
      :arg usePhysicsMesh: when enabled the physics mesh will be replaced (optional argument).
      :type usePhysicsMesh: boolean

   .. method:: setVisible(visible, recursive)

      Sets the game object's visible flag.

      :arg visible: the visible state to set.
      :type visible: boolean
      :arg recursive: optional argument to set all childrens visibility flag too.
      :type recursive: boolean

   .. method:: setOcclusion(occlusion, recursive)

      Sets the game object's occlusion capability.

      :arg occlusion: the state to set the occlusion to.
      :type occlusion: boolean
      :arg recursive: optional argument to set all childrens occlusion flag too.
      :type recursive: boolean

   .. method:: alignAxisToVect(vect, axis=2, factor=1.0)

      Aligns any of the game object's axis along the given vector.


      :arg vect: a vector to align the axis.
      :type vect: 3D vector
      :arg axis: The axis you want to align

         * 0: X axis
         * 1: Y axis
         * 2: Z axis

      :type axis: integer
      :arg factor: Only rotate a feaction of the distance to the target vector (0.0 - 1.0)
      :type factor: float

   .. method:: getAxisVect(vect)

      Returns the axis vector rotates by the objects worldspace orientation.
      This is the equivalent of multiplying the vector by the orientation matrix.

      :arg vect: a vector to align the axis.
      :type vect: 3D Vector
      :return: The vector in relation to the objects rotation.
      :rtype: 3d vector.

   .. method:: applyMovement(movement, local=False)

      Sets the game object's movement.

      :arg movement: movement vector.
      :type movement: 3D Vector
      :arg local:
         * False: you get the "global" movement ie: relative to world orientation.
         * True: you get the "local" movement ie: relative to object orientation.
      :arg local: boolean

   .. method:: applyRotation(rotation, local=False)

      Sets the game object's rotation.

      :arg rotation: rotation vector.
      :type rotation: 3D Vector
      :arg local:
         * False: you get the "global" rotation ie: relative to world orientation.
         * True: you get the "local" rotation ie: relative to object orientation.
      :arg local: boolean

   .. method:: applyForce(force, local=False)

      Sets the game object's force.

      This requires a dynamic object.

      :arg force: force vector.
      :type force: 3D Vector
      :arg local:
         * False: you get the "global" force ie: relative to world orientation.
         * True: you get the "local" force ie: relative to object orientation.
      :type local: boolean

   .. method:: applyTorque(torque, local=False)

      Sets the game object's torque.

      This requires a dynamic object.

      :arg torque: torque vector.
      :type torque: 3D Vector
      :arg local:
         * False: you get the "global" torque ie: relative to world orientation.
         * True: you get the "local" torque ie: relative to object orientation.
      :type local: boolean

   .. method:: getLinearVelocity(local=False)

      Gets the game object's linear velocity.

      This method returns the game object's velocity through it's centre of mass, ie no angular velocity component.

      :arg local:
         * False: you get the "global" velocity ie: relative to world orientation.
         * True: you get the "local" velocity ie: relative to object orientation.
      :type local: boolean
      :return: the object's linear velocity.
      :rtype: Vector((vx, vy, vz))

   .. method:: setLinearVelocity(velocity, local=False)

      Sets the game object's linear velocity.

      This method sets game object's velocity through it's centre of mass,
      ie no angular velocity component.

      This requires a dynamic object.

      :arg velocity: linear velocity vector.
      :type velocity: 3D Vector
      :arg local:
         * False: you get the "global" velocity ie: relative to world orientation.
         * True: you get the "local" velocity ie: relative to object orientation.
      :type local: boolean

   .. method:: getAngularVelocity(local=False)

      Gets the game object's angular velocity.

      :arg local:
         * False: you get the "global" velocity ie: relative to world orientation.
         * True: you get the "local" velocity ie: relative to object orientation.
      :type local: boolean
      :return: the object's angular velocity.
      :rtype: Vector((vx, vy, vz))

   .. method:: setAngularVelocity(velocity, local=False)

      Sets the game object's angular velocity.

      This requires a dynamic object.

      :arg velocity: angular velocity vector.
      :type velocity: boolean
      :arg local:
         * False: you get the "global" velocity ie: relative to world orientation.
         * True: you get the "local" velocity ie: relative to object orientation.

   .. method:: getVelocity(point=(0, 0, 0))

      Gets the game object's velocity at the specified point.

      Gets the game object's velocity at the specified point, including angular
      components.

      :arg point: optional point to return the velocity for, in local coordinates.
      :type point: 3D Vector
      :return: the velocity at the specified point.
      :rtype: Vector((vx, vy, vz))

   .. method:: getReactionForce()

      Gets the game object's reaction force.

      The reaction force is the force applied to this object over the last simulation timestep.
      This also includes impulses, eg from collisions.

      :return: the reaction force of this object.
      :rtype: Vector((fx, fy, fz))

      .. note::

         This is not implimented at the moment.

   .. method:: applyImpulse(point, impulse, local=False)

      Applies an impulse to the game object.

      This will apply the specified impulse to the game object at the specified point.
      If point != position, applyImpulse will also change the object's angular momentum.
      Otherwise, only linear momentum will change.

      :arg point: the point to apply the impulse to (in world or local coordinates)
      :type point: point [ix, iy, iz] the point to apply the impulse to (in world or local coordinates)
      :arg impulse: impulse vector.
      :type impulse: 3D Vector
      :arg local:
         * False: you get the "global" impulse ie: relative to world coordinates with world orientation.
         * True: you get the "local" impulse ie: relative to local coordinates with object orientation.
      :type local: boolean

   .. method:: setDamping(linear_damping, angular_damping)

      Sets both the :py:attr:`linearDamping` and :py:attr:`angularDamping` simultaneously. This is more efficient than setting both properties individually.

      :arg linear_damping: Linear ("translational") damping factor.
      :type linear_damping: float ∈ [0, 1]
      :arg angular_damping: Angular ("rotational") damping factor.
      :type angular_damping: float ∈ [0, 1]

   .. method:: suspendDynamics()

      Suspends physics for this object.

   .. method:: restoreDynamics()

      Resumes physics for this object.

      .. note::

         The objects linear velocity will be applied from when the dynamics were suspended.

   .. method:: enableRigidBody()

      Enables rigid body physics for this object.

      Rigid body physics allows the object to roll on collisions.

   .. method:: disableRigidBody()

      Disables rigid body physics for this object.

   .. method:: setParent(parent, compound=True, ghost=True)

      Sets this object's parent.
      Control the shape status with the optional compound and ghost parameters:

      In that case you can control if it should be ghost or not:

      :arg parent: new parent object.
      :type parent: :class:`KX_GameObject`
      :arg compound: whether the shape should be added to the parent compound shape.

         * True: the object shape should be added to the parent compound shape.
         * False: the object should keep its individual shape.

      :type compound: boolean
      :arg ghost: whether the object should be ghost while parented.

         * True: if the object should be made ghost while parented.
         * False: if the object should be solid while parented.

      :type ghost: boolean

      .. note::

         If the object type is sensor, it stays ghost regardless of ghost parameter

   .. method:: removeParent()

      Removes this objects parent.

   .. method:: getPhysicsId()

      Returns the user data object associated with this game object's physics controller.

   .. method:: getPropertyNames()

      Gets a list of all property names.

      :return: All property names for this object.
      :rtype: list

   .. method:: getDistanceTo(other)

      :arg other: a point or another :class:`KX_GameObject` to measure the distance to.
      :type other: :class:`KX_GameObject` or list [x, y, z]
      :return: distance to another object or point.
      :rtype: float

   .. method:: getVectTo(other)

      Returns the vector and the distance to another object or point.
      The vector is normalized unless the distance is 0, in which a zero length vector is returned.

      :arg other: a point or another :class:`KX_GameObject` to get the vector and distance to.
      :type other: :class:`KX_GameObject` or list [x, y, z]
      :return: (distance, globalVector(3), localVector(3))
      :rtype: 3-tuple (float, 3-tuple (x, y, z), 3-tuple (x, y, z))

   .. method:: rayCastTo(other, dist, prop)

      Look towards another point/object and find first object hit within dist that matches prop.

      The ray is always casted from the center of the object, ignoring the object itself.
      The ray is casted towards the center of another object or an explicit [x, y, z] point.
      Use rayCast() if you need to retrieve the hit point

      :arg other: [x, y, z] or object towards which the ray is casted
      :type other: :class:`KX_GameObject` or 3-tuple
      :arg dist: max distance to look (can be negative => look behind); 0 or omitted => detect up to other
      :type dist: float
      :arg prop: property name that object must have; can be omitted => detect any object
      :type prop: string
      :return: the first object hit or None if no object or object does not match prop
      :rtype: :class:`KX_GameObject`

   .. method:: rayCast(objto, objfrom, dist, prop, face, xray, poly)

      Look from a point/object to another point/object and find first object hit within dist that matches prop.
      if poly is 0, returns a 3-tuple with object reference, hit point and hit normal or (None, None, None) if no hit.
      if poly is 1, returns a 4-tuple with in addition a :class:`KX_PolyProxy` as 4th element.
      if poly is 2, returns a 5-tuple with in addition a 2D vector with the UV mapping of the hit point as 5th element.

      .. code-block:: python

         # shoot along the axis gun-gunAim (gunAim should be collision-free)
         obj, point, normal = gun.rayCast(gunAim, None, 50)
         if obj:
            # do something
            pass

      The face paremeter determines the orientation of the normal.

      * 0 => hit normal is always oriented towards the ray origin (as if you casted the ray from outside)
      * 1 => hit normal is the real face normal (only for mesh object, otherwise face has no effect)

      The ray has X-Ray capability if xray parameter is 1, otherwise the first object hit (other than self object) stops the ray.
      The prop and xray parameters interact as follow.

      * prop off, xray off: return closest hit or no hit if there is no object on the full extend of the ray.
      * prop off, xray on : idem.
      * prop on, xray off: return closest hit if it matches prop, no hit otherwise.
      * prop on, xray on : return closest hit matching prop or no hit if there is no object matching prop on the full extend of the ray.

      The :class:`KX_PolyProxy` 4th element of the return tuple when poly=1 allows to retrieve information on the polygon hit by the ray.
      If there is no hit or the hit object is not a static mesh, None is returned as 4th element.

      The ray ignores collision-free objects and faces that dont have the collision flag enabled, you can however use ghost objects.

      :arg objto: [x, y, z] or object to which the ray is casted
      :type objto: :class:`KX_GameObject` or 3-tuple
      :arg objfrom: [x, y, z] or object from which the ray is casted; None or omitted => use self object center
      :type objfrom: :class:`KX_GameObject` or 3-tuple or None
      :arg dist: max distance to look (can be negative => look behind); 0 or omitted => detect up to to
      :type dist: float
      :arg prop: property name that object must have; can be omitted or "" => detect any object
      :type prop: string
      :arg face: normal option: 1=>return face normal; 0 or omitted => normal is oriented towards origin
      :type face: integer
      :arg xray: X-ray option: 1=>skip objects that don't match prop; 0 or omitted => stop on first object
      :type xray: integer
      :arg poly: polygon option: 0, 1 or 2 to return a 3-, 4- or 5-tuple with information on the face hit.

         * 0 or omitted: return value is a 3-tuple (object, hitpoint, hitnormal) or (None, None, None) if no hit
         * 1: return value is a 4-tuple and the 4th element is a :class:`KX_PolyProxy` or None if no hit or the object doesn't use a mesh collision shape.
         * 2: return value is a 5-tuple and the 5th element is a 2-tuple (u, v) with the UV mapping of the hit point or None if no hit, or the object doesn't use a mesh collision shape, or doesn't have a UV mapping.

      :type poly: integer
      :return: (object, hitpoint, hitnormal) or (object, hitpoint, hitnormal, polygon) or (object, hitpoint, hitnormal, polygon, hituv).

         * object, hitpoint and hitnormal are None if no hit.
         * polygon is valid only if the object is valid and is a static object, a dynamic object using mesh collision shape or a soft body object, otherwise it is None
         * hituv is valid only if polygon is valid and the object has a UV mapping, otherwise it is None

      :rtype:

         * 3-tuple (:class:`KX_GameObject`, 3-tuple (x, y, z), 3-tuple (nx, ny, nz))
         * or 4-tuple (:class:`KX_GameObject`, 3-tuple (x, y, z), 3-tuple (nx, ny, nz), :class:`KX_PolyProxy`)
         * or 5-tuple (:class:`KX_GameObject`, 3-tuple (x, y, z), 3-tuple (nx, ny, nz), :class:`KX_PolyProxy`, 2-tuple (u, v))

      .. note::

         The ray ignores the object on which the method is called. It is casted from/to object center or explicit [x, y, z] points.

   .. method:: setCollisionMargin(margin)

      Set the objects collision margin.

      :arg margin: the collision margin distance in blender units.
      :type margin: float

      .. note::

         If this object has no physics controller (a physics ID of zero), this function will raise RuntimeError.

   .. method:: sendMessage(subject, body="", to="")

      Sends a message.

      :arg subject: The subject of the message
      :type subject: string
      :arg body: The body of the message (optional)
      :type body: string
      :arg to: The name of the object to send the message to (optional)
      :type to: string

   .. method:: reinstancePhysicsMesh(gameObject, meshObject)

      Updates the physics system with the changed mesh.

      If no arguments are given the physics mesh will be re-created from the first mesh assigned to the game object.

      :arg gameObject: optional argument, set the physics shape from this gameObjets mesh.
      :type gameObject: string, :class:`KX_GameObject` or None
      :arg meshObject: optional argument, set the physics shape from this mesh.
      :type meshObject: string, :class:`MeshProxy` or None

      :return: True if reinstance succeeded, False if it failed.
      :rtype: boolean

      .. note::

         If this object has instances the other instances will be updated too.

      .. note::

         The gameObject argument has an advantage that it can convert from a mesh with modifiers applied (such as subsurf).

      .. warning::

         Only triangle mesh type objects are supported currently (not convex hull)

      .. warning::

         If the object is a part of a combound object it will fail (parent or child)

      .. warning::

         Rebuilding the physics mesh can be slow, running many times per second will give a performance hit.

   .. method:: get(key, default=None)

      Return the value matching key, or the default value if its not found.
      :return: The key value or a default.

   .. method:: playAction(name, start_frame, end_frame, layer=0, priority=0, blendin=0, play_mode=KX_ACTION_MODE_PLAY, layer_weight=0.0, ipo_flags=0, speed=1.0, blend_mode=KX_ACTION_BLEND_BLEND)

      Plays an action.

      :arg name: the name of the action
      :type name: string
      :arg start: the start frame of the action
      :type start: float
      :arg end: the end frame of the action
      :type end: float
      :arg layer: the layer the action will play in (actions in different layers are added/blended together)
      :type layer: integer
      :arg priority: only play this action if there isn't an action currently playing in this layer with a higher (lower number) priority
      :type priority: integer
      :arg blendin: the amount of blending between this animation and the previous one on this layer
      :type blendin: float
      :arg play_mode: the play mode
      :type play_mode: one of :ref:`these constants <gameobject-playaction-mode>`
      :arg layer_weight: how much of the previous layer to use for blending
      :type layer_weight: float
      :arg ipo_flags: flags for the old IPO behaviors (force, etc)
      :type ipo_flags: int bitfield
      :arg speed: the playback speed of the action as a factor (1.0 = normal speed, 2.0 = 2x speed, etc)
      :type speed: float
      :arg blend_mode: how to blend this layer with previous layers
      :type blend_mode: one of :ref:`these constants <gameobject-playaction-blend>`

   .. method:: stopAction(layer=0)

      Stop playing the action on the given layer.

      :arg layer: The layer to stop playing.
      :type layer: integer

   .. method:: getActionFrame(layer=0)

      Gets the current frame of the action playing in the supplied layer.

      :arg layer: The layer that you want to get the frame from.
      :type layer: integer

      :return: The current frame of the action
      :rtype: float

   .. method:: setActionFrame(frame, layer=0)

      Set the current frame of the action playing in the supplied layer.

      :arg layer: The layer where you want to set the frame
      :type layer: integer
      :arg frame: The frame to set the action to
      :type frame: float

   .. method:: isPlayingAction(layer=0)

      Checks to see if there is an action playing in the given layer.

      :arg layer: The layer to check for a playing action.
      :type layer: integer

      :return: Whether or not the action is playing
      :rtype: boolean

   .. method:: addDebugProperty (name, debug = True)

      Adds a single debug property to the debug list.

      :arg name: name of the property that added to the debug list.
      :type name: string
      :arg debug: the debug state.
      :type debug: boolean
