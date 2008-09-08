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
	@ivar timeOffset: adjust the slowparent delay at runtime.
	@type timeOffset: float
	"""
	def endObject(visible):
		"""
		Delete this object, can be used inpace of the EndObject Actuator.
		The actual removal of the object from the scene is delayed.
		"""	
	def getVisible():
		"""
		Gets the game object's visible flag.
		
		@rtype: boolean
		"""	
	def setVisible(visible):
		"""
		Sets the game object's visible flag.
		
		@type visible: boolean
		"""
	def getState():
		"""
		Gets the game object's state bitmask.
		
		@rtype: int
		@return: the objects state.
		"""	
	def setState(state):
		"""
		Sets the game object's state flag.
		The bitmasks for states from 1 to 30 can be set with (1<<0, 1<<1, 1<<2 ... 1<<29)
		
		@type state: integer
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
		Gets the game object's orientation.
		
		@rtype: 3x3 rotation matrix
		@return: The game object's rotation matrix
		@note: When using this matrix with Blender.Mathutils.Matrix() types, it will need to be transposed.
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
	def getChildren():
		"""
		Return a list of immediate children of this object.
		@rtype: list
		@return: a list of all this objects children.
		"""
	def getChildrenRecursive():
		"""
		Return a list of children of this object, including all their childrens children.
		@rtype: list
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


