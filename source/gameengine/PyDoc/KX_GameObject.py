# $Id$
# Documentation for game objects

# from SCA_IObject import *
# from SCA_ISensor import *
# from SCA_IController import *
# from SCA_IActuator import *


class KX_GameObject: # (SCA_IObject)
	"""
	All game objects are derived from this class.
	
	Properties assigned to game objects are accessible as attributes of this class.
		- note: Calling ANY method or attribute on an object that has been removed from a scene will raise a RuntimeError, if an object may have been removed since last accessing it use the L{isValid} attribute to check.

	@ivar name: The object's name. (Read only)
		- note: Currently (Blender 2.49) the prefix "OB" is added to all objects name. This may change in blender 2.5.
	@type name: string.
	@ivar mass: The object's mass
		- note: The object must have a physics controller for the mass to be applied, otherwise the mass value will be returned as 0.0
	@type mass: float
	@ivar linVelocityMin: Enforces the object keeps moving at a minimum velocity.
		- note: Applies to dynamic and rigid body objects only.
		- note: A value of 0.0 disables this option.
		- note: While objects are stationary the minimum velocity will not be applied.
	@type linVelocityMin: float
	@ivar linVelocityMax: Clamp the maximum linear velocity to prevent objects moving beyond a set speed.
		- note: Applies to dynamic and rigid body objects only.
		- note: A value of 0.0 disables this option (rather then setting it stationary).
	@type linVelocityMax: float
	@ivar localInertia: the object's inertia vector in local coordinates. Read only.
	@type localInertia: list [ix, iy, iz]
	@ivar parent: The object's parent object. (Read only)
	@type parent: L{KX_GameObject} or None
	@ivar visible: visibility flag.
		- note: Game logic will still run for invisible objects.
	@type visible: boolean
	@ivar occlusion: occlusion capability flag.
	@type occlusion: boolean
	@ivar position: The object's position. 
	                DEPRECATED: use localPosition and worldPosition
	@type position: list [x, y, z] On write: local position, on read: world position
	@ivar orientation: The object's orientation. 3x3 Matrix. You can also write a Quaternion or Euler vector.
	                   DEPRECATED: use localOrientation and worldOrientation
	@type orientation: 3x3 Matrix [[float]] On write: local orientation, on read: world orientation
	@ivar scaling: The object's scaling factor. list [sx, sy, sz]
	               DEPRECATED: use localScaling and worldScaling
	@type scaling: list [sx, sy, sz] On write: local scaling, on read: world scaling
	@ivar localOrientation: The object's local orientation. 3x3 Matrix. You can also write a Quaternion or Euler vector.
	@type localOrientation: 3x3 Matrix [[float]]
	@ivar worldOrientation: The object's world orientation. Read-only.
	@type worldOrientation: 3x3 Matrix [[float]]
	@ivar localScaling: The object's local scaling factor.
	@type localScaling: list [sx, sy, sz]
	@ivar worldScaling: The object's world scaling factor. Read-only
	@type worldScaling: list [sx, sy, sz]
	@ivar localPosition: The object's local position. 
	@type localPosition: list [x, y, z]
	@ivar worldPosition: The object's world position. 
	@type worldPosition: list [x, y, z]
	@ivar timeOffset: adjust the slowparent delay at runtime.
	@type timeOffset: float
	@ivar state: the game object's state bitmask, using the first 30 bits, one bit must always be set.
	@type state: int
	@ivar meshes: a list meshes for this object.
		- note: Most objects use only 1 mesh.
		- note: Changes to this list will not update the KX_GameObject.
	@type meshes: list of L{KX_MeshProxy}
	@ivar sensors: a list of L{SCA_ISensor} objects.
		- note: This attribute is experemental and may be removed (but probably wont be).
		- note: Changes to this list will not update the KX_GameObject.
	@type sensors: list
	@ivar controllers: a list of L{SCA_IController} objects.
		- note: This attribute is experemental and may be removed (but probably wont be).
		- note: Changes to this list will not update the KX_GameObject.
	@type controllers: list of L{SCA_ISensor}.
	@ivar actuators: a list of L{SCA_IActuator} objects.
		- note: This attribute is experemental and may be removed (but probably wont be).
		- note: Changes to this list will not update the KX_GameObject.
	@type actuators: list
	@ivar isValid: Retuerns fails when the object has been removed from the scene and can no longer be used.
	@type isValid: bool
	"""
	def endObject():
		"""
		Delete this object, can be used inpace of the EndObject Actuator.
		The actual removal of the object from the scene is delayed.
		"""	
	def replaceMesh(mesh_name):
		"""
		Replace the mesh of this object with a new mesh. This works the same was as the actuator.
		@type mesh_name: string
		"""	
	def getVisible():
		"""
		Gets the game object's visible flag. (B{deprecated})
		
		@rtype: boolean
		"""	
	def setVisible(visible, recursive):
		"""
		Sets the game object's visible flag.
		
		@type visible: boolean
		@type recursive: boolean
		@param recursive: optional argument to set all childrens visibility flag too.
		"""
	def setOcclusion(occlusion, recursive):
		"""
		Sets the game object's occlusion capability.
		
		@type visible: boolean
		@type recursive: boolean
		@param recursive: optional argument to set all childrens occlusion flag too.
		"""
	def getState():
		"""
		Gets the game object's state bitmask. (B{deprecated})
		
		@rtype: int
		@return: the objects state.
		"""	
	def setState(state):
		"""
		Sets the game object's state flag. (B{deprecated}).
		The bitmasks for states from 1 to 30 can be set with (1<<0, 1<<1, 1<<2 ... 1<<29) 
		
		@type state: integer
		"""
	def setPosition(pos):
		"""
		Sets the game object's position. (B{deprecated})
		Global coordinates for root object, local for child objects.
		
		
		@type pos: [x, y, z]
		@param pos: the new position, in local coordinates.
		"""
	def setWorldPosition(pos):
		"""
		Sets the game object's position in world coordinates regardless if the object is root or child.
		
		@type pos: [x, y, z]
		@param pos: the new position, in world coordinates.
		"""
	def getPosition():
		"""
		Gets the game object's position. (B{deprecated})
		
		@rtype: list [x, y, z]
		@return: the object's position in world coordinates.
		"""
	def setOrientation(orn):
		"""
		Sets the game object's orientation. (B{deprecated})
		
		@type orn: 3x3 rotation matrix, or Quaternion.
		@param orn: a rotation matrix specifying the new rotation.
		@note: When using this matrix with Blender.Mathutils.Matrix() types, it will need to be transposed.
		"""
	def alignAxisToVect(vect, axis):
		"""
		Aligns any of the game object's axis along the given vector.
		
		@type vect: 3d vector.
		@param vect: a vector to align the axis.
		@type axis: integer.
		@param axis:The axis you want to align
					- 0: X axis
					- 1: Y axis
					- 2: Z axis (default) 
		"""
	def getAxisVect(vect):
		"""
		Returns the axis vector rotates by the objects worldspace orientation.
		This is the equivalent if multiplying the vector by the orientation matrix.
		
		@type vect: 3d vector.
		@param vect: a vector to align the axis.
		@rtype: 3d vector.
		@return: The vector in relation to the objects rotation.

		"""
	def getOrientation():
		"""
		Gets the game object's orientation. (B{deprecated})
		
		@rtype: 3x3 rotation matrix
		@return: The game object's rotation matrix
		@note: When using this matrix with Blender.Mathutils.Matrix() types, it will need to be transposed.
		"""
	def applyMovement(movement, local = 0):
		"""
		Sets the game object's movement.
		
		@type movement: 3d vector.
		@param movement: movement vector.
		@type local: boolean
		@param local: - False: you get the "global" movement ie: relative to world orientation (default).
		              - True: you get the "local" movement ie: relative to object orientation.
		"""	
	def applyRotation(rotation, local = 0):
		"""
		Sets the game object's rotation.
		
		@type rotation: 3d vector.
		@param rotation: rotation vector.
		@type local: boolean
		@param local: - False: you get the "global" rotation ie: relative to world orientation (default).
					  - True: you get the "local" rotation ie: relative to object orientation.
		"""	
	def applyForce(force, local = 0):
		"""
		Sets the game object's force.
		
		This requires a dynamic object.
		
		@type force: 3d vector.
		@param force: force vector.
		@type local: boolean
		@param local: - False: you get the "global" force ie: relative to world orientation (default).
					  - True: you get the "local" force ie: relative to object orientation.
		"""	
	def applyTorque(torque, local = 0):
		"""
		Sets the game object's torque.
		
		This requires a dynamic object.
		
		@type torque: 3d vector.
		@param torque: torque vector.
		@type local: boolean
		@param local: - False: you get the "global" torque ie: relative to world orientation (default).
					  - True: you get the "local" torque ie: relative to object orientation.
		"""
	def getLinearVelocity(local = 0):
		"""
		Gets the game object's linear velocity.
		
		This method returns the game object's velocity through it's centre of mass,
		ie no angular velocity component.
		
		@type local: boolean
		@param local: - False: you get the "global" velocity ie: relative to world orientation (default).
		              - True: you get the "local" velocity ie: relative to object orientation.
		@rtype: list [vx, vy, vz]
		@return: the object's linear velocity.
		"""
	def setLinearVelocity(velocity, local = 0):
		"""
		Sets the game object's linear velocity.
		
		This method sets game object's velocity through it's centre of mass,
		ie no angular velocity component.
		
		This requires a dynamic object.
		
		@type velocity: 3d vector.
		@param velocity: linear velocity vector.
		@type local: boolean
		@param local: - False: you get the "global" velocity ie: relative to world orientation (default).
		              - True: you get the "local" velocity ie: relative to object orientation.
		"""
	def getAngularVelocity(local = 0):
		"""
		Gets the game object's angular velocity.
		
		@type local: boolean
		@param local: - False: you get the "global" velocity ie: relative to world orientation (default).
		              - True: you get the "local" velocity ie: relative to object orientation.
		@rtype: list [vx, vy, vz]
		@return: the object's angular velocity.
		"""
	def setAngularVelocity(velocity, local = 0):
		"""
		Sets the game object's angular velocity.
		
		This requires a dynamic object.
		
		@type velocity: 3d vector.
		@param velocity: angular velocity vector.
		@type local: boolean
		@param local: - False: you get the "global" velocity ie: relative to world orientation (default).
		              - True: you get the "local" velocity ie: relative to object orientation.
		"""
	def getVelocity(point):
		"""
		Gets the game object's velocity at the specified point.
		
		Gets the game object's velocity at the specified point, including angular
		components.
		
		@type point: list [x, y, z]
		@param point: the point to return the velocity for, in local coordinates. (optional: default = [0, 0, 0])
		@rtype: list [vx, vy, vz]
		@return: the velocity at the specified point.
		"""
	def getMass():
		"""
		Gets the game object's mass. (B{deprecated})
		
		@rtype: float
		@return: the object's mass.
		"""
	def getReactionForce():
		"""
		Gets the game object's reaction force.
		
		The reaction force is the force applied to this object over the last simulation timestep.
		This also includes impulses, eg from collisions.
		
		(B{This is not implimented for bullet physics at the moment})
		
		@rtype: list [fx, fy, fz]
		@return: the reaction force of this object.
		"""
	def applyImpulse(point, impulse):
		"""
		Applies an impulse to the game object.
		
		This will apply the specified impulse to the game object at the specified point.
		If point != getPosition(), applyImpulse will also change the object's angular momentum.
		Otherwise, only linear momentum will change.
		
		@type point: list [x, y, z]
		@param point: the point to apply the impulse to (in world coordinates)
		"""
	def suspendDynamics():
		"""
		Suspends physics for this object.
		"""
	def restoreDynamics():
		"""
		Resumes physics for this object.
		@Note: The objects linear velocity will be applied from when the dynamics were suspended.
		"""
	def enableRigidBody():
		"""
		Enables rigid body physics for this object.
		
		Rigid body physics allows the object to roll on collisions.
		@Note: This is not working with bullet physics yet.
		"""
	def disableRigidBody():
		"""
		Disables rigid body physics for this object.
		@Note: This is not working with bullet physics yet. The angular is removed but rigid body physics can still rotate it later.
		"""
	def getParent():
		"""
		Gets this object's parent. (B{deprecated})
		
		@rtype: L{KX_GameObject}
		@return: this object's parent object, or None if this object has no parent.
		"""
	def setParent(parent):
		"""
		Sets this object's parent.
		
		@type parent: L{KX_GameObject}
		@param parent: new parent object.
		"""
	def removeParent():
		"""
		Removes this objects parent.
		"""
	def getChildren():
		"""
		Return a list of immediate children of this object.
		@rtype: L{CListValue<CListValue.CListValue>} of L{KX_GameObject<KX_GameObject.KX_GameObject>}
		@return: a list of all this objects children.
		"""
	def getChildrenRecursive():
		"""
		Return a list of children of this object, including all their childrens children.
		@rtype: L{CListValue<CListValue.CListValue>} of L{KX_GameObject<KX_GameObject.KX_GameObject>}
		@return: a list of all this objects children recursivly.
		"""
	def getMesh(mesh):
		"""
		Gets the mesh object for this object.
		
		@type mesh: integer
		@param mesh: the mesh object to return (optional: default mesh = 0)
		@rtype: L{KX_MeshProxy}
		@return: the first mesh object associated with this game object, or None if this object has no meshs.
		"""
	def getPhysicsId():
		"""
		Returns the user data object associated with this game object's physics controller.
		"""
	def getPropertyNames():
		"""
		Gets a list of all property names.
		@rtype: list
		@return: All property names for this object.
		"""
	def getDistanceTo(other):
		"""
		Returns the distance to another object or point.
		
		@param other: a point or another L{KX_GameObject} to measure the distance to.
		@type other: L{KX_GameObject} or list [x, y, z]
		@rtype: float
		"""
	def getVectTo(other):
		"""
		Returns the vector and the distance to another object or point.
		The vector is normalized unless the distance is 0, in which a NULL vector is returned.
		
		@param other: a point or another L{KX_GameObject} to get the vector and distance to.
		@type other: L{KX_GameObject} or list [x, y, z]
		@rtype: 3-tuple (float, 3-tuple (x,y,z), 3-tuple (x,y,z))
		@return: (distance, globalVector(3), localVector(3))
		"""
	def rayCastTo(other,dist,prop):
		"""
		Look towards another point/object and find first object hit within dist that matches prop.

		The ray is always casted from the center of the object, ignoring the object itself.
		The ray is casted towards the center of another object or an explicit [x,y,z] point.
		Use rayCast() if you need to retrieve the hit point 

		@param other: [x,y,z] or object towards which the ray is casted
		@type other: L{KX_GameObject} or 3-tuple
		@param dist: max distance to look (can be negative => look behind); 0 or omitted => detect up to other
		@type dist: float
		@param prop: property name that object must have; can be omitted => detect any object
		@type prop: string
		@rtype: L{KX_GameObject}
		@return: the first object hit or None if no object or object does not match prop
		"""
	def rayCast(objto,objfrom,dist,prop,face,xray,poly):
		"""
		Look from a point/object to another point/object and find first object hit within dist that matches prop.
		if poly is 0, returns a 3-tuple with object reference, hit point and hit normal or (None,None,None) if no hit.
		if poly is 1, returns a 4-tuple with in addition a L{KX_PolyProxy} as 4th element.
		
		Ex::
			# shoot along the axis gun-gunAim (gunAim should be collision-free)
			ob,point,normal = gun.rayCast(gunAim,None,50)
			if ob:
				# hit something

		Notes:				
		The ray ignores the object on which the method is called.
		It is casted from/to object center or explicit [x,y,z] points.
		
		The face paremeter determines the orientation of the normal:: 
		  0 => hit normal is always oriented towards the ray origin (as if you casted the ray from outside)
		  1 => hit normal is the real face normal (only for mesh object, otherwise face has no effect)
		  
		The ray has X-Ray capability if xray parameter is 1, otherwise the first object hit (other than self object) stops the ray.
		The prop and xray parameters interact as follow::
		    prop off, xray off: return closest hit or no hit if there is no object on the full extend of the ray.
		    prop off, xray on : idem.
		    prop on,  xray off: return closest hit if it matches prop, no hit otherwise.
		    prop on,  xray on : return closest hit matching prop or no hit if there is no object matching prop on the full extend of the ray.
		The L{KX_PolyProxy} 4th element of the return tuple when poly=1 allows to retrieve information on the polygon hit by the ray.
		If there is no hit or the hit object is not a static mesh, None is returned as 4th element. 
		
		The ray ignores collision-free objects and faces that dont have the collision flag enabled, you can however use ghost objects.

		@param objto: [x,y,z] or object to which the ray is casted
		@type objto: L{KX_GameObject} or 3-tuple
		@param objfrom: [x,y,z] or object from which the ray is casted; None or omitted => use self object center
		@type objfrom: L{KX_GameObject} or 3-tuple or None
		@param dist: max distance to look (can be negative => look behind); 0 or omitted => detect up to to
		@type dist: float
		@param prop: property name that object must have; can be omitted => detect any object
		@type prop: string
		@param face: normal option: 1=>return face normal; 0 or omitted => normal is oriented towards origin
		@type face: int
		@param xray: X-ray option: 1=>skip objects that don't match prop; 0 or omitted => stop on first object
		@type xray: int
		@param poly: polygon option: 1=>return value is a 4-tuple and the 4th element is a L{KX_PolyProxy}
		@type poly: int
		@rtype:    3-tuple (L{KX_GameObject}, 3-tuple (x,y,z), 3-tuple (nx,ny,nz))
		        or 4-tuple (L{KX_GameObject}, 3-tuple (x,y,z), 3-tuple (nx,ny,nz), L{KX_PolyProxy})
		@return: (object,hitpoint,hitnormal) or (object,hitpoint,hitnormal,polygon)
		         If no hit, returns (None,None,None) or (None,None,None,None)
		         If the object hit is not a static mesh, polygon is None
		"""
	def setCollisionMargin(margin):
		"""
		Set the objects collision margin.
		
		note: If this object has no physics controller (a physics ID of zero), this function will raise RuntimeError.
		
		@type margin: float
		@param margin: the collision margin distance in blender units.
		"""
	def sendMessage(subject, body="", to=""):
		"""
		Sends a message.
	
		@param subject: The subject of the message
		@type subject: string
		@param body: The body of the message (optional)
		@type body: string
		@param to: The name of the object to send the message to (optional)
		@type to: string
		"""
