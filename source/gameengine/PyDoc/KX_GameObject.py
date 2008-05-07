# $Id$
# Documentation for game objects

class KX_GameObject:
	"""
	All game objects are derived from this class.
	
	Properties assigned to game objects are accessible as attributes of this class.
	
	@ivar name: The object's name.
	@type name: string.
	@ivar mass: The object's mass (provided the object has a physics controller). Read only.
	@type mass: float
	@ivar parent: The object's parent object. (Read only)
	@type parent: L{KX_GameObject}
	@ivar visible: visibility flag.
	@type visible: boolean
	@ivar position: The object's position. 
	@type position: list [x, y, z]
	@ivar orientation: The object's orientation. 3x3 Matrix.  
	                   You can also write a Quaternion or Euler vector.
	@type orientation: 3x3 Matrix [[float]]
	@ivar scaling: The object's scaling factor. list [sx, sy, sz]
	@type scaling: list [sx, sy, sz]
	"""
	
	def setVisible(visible):
		"""
		Sets the game object's visible flag.
		
		@type visible: boolean
		"""
	def setPosition(pos):
		"""
		Sets the game object's position.
		
		@type pos: [x, y, z]
		@param pos: the new position, in world coordinates.
		"""
	def getPosition():
		"""
		Gets the game object's position.
		
		@rtype: list [x, y, z]
		@return: the object's position in world coordinates.
		"""
	def setOrientation(orn):
		"""
		Sets the game object's orientation.
		
		@type orn: 3x3 rotation matrix, or Quaternion.
		@param orn: a rotation matrix specifying the new rotation.
		"""
	def getOrientation():
		"""
		Gets the game object's orientation.
		
		@rtype: 3x3 rotation matrix
		@return: The game object's rotation matrix
		"""
	def getLinearVelocity(local):
		"""
		Gets the game object's linear velocity.
		
		This method returns the game object's velocity through it's centre of mass,
		ie no angular velocity component.
		
		@type local: boolean
		@param local: - False: you get the "global" velocity ie: relative to world orientation.
		              - True: you get the "local" velocity ie: relative to object orientation.
		@rtype: list [vx, vy, vz]
		@return: the object's linear velocity.
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
		Gets the game object's mass.
		
		@rtype: float
		@return: the object's mass.
		"""
	def getReactionForce():
		"""
		Gets the game object's reaction force.
		
		The reaction force is the force applied to this object over the last simulation timestep.
		This also includes impulses, eg from collisions.
		
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
		"""
	def enableRigidBody():
		"""
		Enables rigid body physics for this object.
		
		Rigid body physics allows the object to roll on collisions.
		"""
	def disableRigidBody():
		"""
		Disables rigid body physics for this object.
		"""
	def getParent():
		"""
		Gets this object's parent.
		
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
	def getDistanceTo(other):
		"""
		Returns the distance to another object or point.
		
		@param other: a point or another L{KX_GameObject} to measure the distance to.
		@type other: L{KX_GameObject} or list [x, y, z]
		@rtype: float
		"""
	def rayCastTo(other,dist,prop):
		"""
		Look towards another point/object and find first object hit within dist that matches prop.

		The ray is always casted from the center of the object, ignoring the object itself.
		The ray is casted towards the center of another object or an explicit [x,y,z] point.

		@param other: [x,y,z] or object towards which the ray is casted
		@type other: L{KX_GameObject} or string
		@param dist: max distance to look (can be negative => look behind); 0 or omitted => detect up to other
		@type dist: float
		@param prop: property name that object must have; can be omitted => detect any object
		@type prop: string
		@rtype: L{KX_GameObject}
		@return: the first object hit or None if no object or object does not match prop
		"""
	