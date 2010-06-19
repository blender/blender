# $Id$
"""
Documentation for the PhysicsConstraints module.
================================================

Example::
	
	
	#  Adding a point constraint  #
	###############################
	
	
	# import BGE internal module
	import PhysicsConstraints
	
	# get object list
	obj_list = GameLogic.getCurrentScene().objects
	
	# get object named Obj_1
	root = obj_list["root"]
	obj = obj_list["obj"]

	# get object physics ID
	phido = obj.getPhysicsId()
	
	# get root physics ID
	phidr = root.getPhysicsId()

	# want to use point constraint type
	constraint_type = 1
	
	# Use bottom right front corner of object for point constraint position
	point_pos_x = 1.0
	point_pos_y = -1.0
	point_pos_z = -1.0
	
	# create a point constraint
	const =	PhysicsConstraints.createConstraint( phido, phidr, constraint_type, point_pos_x, point_pos_y, point_pos_z)

	# stores the new constraint ID to be used later
	obj["constraint_ID"] = const.getConstraintId()	
	
	
Example::
	
	
	#  Removing a point constraint  #
	#################################
	
	
	# import BGE internal module
	import PhysicsConstraints
	
	# get object list
	obj_list = GameLogic.getCurrentScene().objects
	
	# get object 1
	obj = obj_list["obj"]
	
	# get constraint ID that was saved as an obj property
	# when the constraint was created
	constraint_ID = obj["constraint_ID"]
	
	# remove constraint
	PhysicsConstraints.removeConstraint(constraint_ID)

"""

def createConstraint(obj_PhysicsID, root_PhysicsID, constraintType, pointPos_x, pointPos_y, pointPos_z, edgePos_x, edgePos_y, edgePos_z, edgeAngle_x, edgeAngle_y, edgeAngle_z):
	"""	
	Create a point constraint between two objects, an edge constraint between two objects, or a vehicle constraint on an object.
	
	You only have to input the needed parammeters depending on the type of constraint you are trying to create.
	

	B{Point Constraint} ::
	
		While creating a point constraint, the "pointPos" values define where you want the pivot point to be located.
		If you are creating a point constraint be sure to assing the integer "1" as the constraintType value.
	
		Parameters to use:
		obj_PhysicsID, root_PhysicsID, constraintType, pointPos_x, pointPos_y, pointPos_z
	
	B{Edge Constraint} ::
		
		While creating an edge constraint, the "edgePos" values define where you want the center of the edge constraint to be.
		Also, the "edgeAngle" values define in which direction you want the edge constraint to point (As a 3 dimensions vector).
		If you want to create an edge constraint be sure to assing the integer "2" as the constraintType value.

		Parameters to use:
		obj_PhysicsID, root_PhysicsID, constraintType, edgePos_x, edgePos_y, edgePos_z, edgeAngle_x, edgeAngle_y, edgeAngle_z}		
	
	B{Vehicle Constraint} ::
		
		While creating a point constraint, the "pointPos" values define where you want the pivot point to be located.
		If you want to create an edge constraint be sure to assing the integer "0" as the constraintType value.

		Parameters to use :
		obj_PhysicsID, root_PhysicsID, constraintType
	
	@type obj_PhysicsID: integer
	@param obj_PhysicsID: The physic ID of the first object to constraint.

	@type root_PhysicsID: integer
	@param root_PhysicsID: The physic ID of the second object to constraint.

	@type constraintType: integer
	@param constraintType: The type of constraint.

	@type pointPos_x: float
	@param pointPos_x: The X position of the point constraint.

	@type pointPos_y: float
	@param pointPos_y: The Y position of the point constraint.

	@type pointPos_z: float
	@param pointPos_z: The Z position of the point constraint.

	@type edgePos_x: float
	@param edgePos_x: The X value of the center of the edge constraint.

	@type edgePos_y: float
	@param edgePos_y: The Y value of the center of the edge constraint.

	@type edgePos_z: float
	@param edgePos_z: The Z value of the center of the edge constraint.

	@type edgeAngle_x: float
	@param edgeAngle_x: The X value of the edge's orientation vector.

	@type edgeAngle_y: float
	@param edgeAngle_y: The Y value of the edge's orientation vector.

	@type edgeAngle_z: float
	@param edgeAngle_z: The Z value of the edge's orientation vector.

	@rtype: integer
	@return: The created constraint ID
	"""
	

def getAppliedImpulse(constraint_ID):
	"""
	Returns the applied impulse.
	
	@param constraint_ID: The constraint ID that was saved on the creation of the constraint.
	@type constraint_ID: integer
	@rtype: float
	@return: Measure the stress on a constraint.
	"""


def getVehicleConstraint(constraint_ID):
	"""
	Returns the vehicle constraint ID.
	
	@param constraint_ID: The constraint ID that was saved on the creation of the constraint.
	@type constraint_ID: integer
	@rtype: integer
	"""
def removeConstraint(constraint_ID):
	"""
	
	Removes the constraint between 2 game objects (point and edge constraints).
	
	It does not remove vehicle constraints.
	
	@param constraint_ID: The constraint ID that was saved on the creation of the constraint.
	@type constraint_ID: integer
	"""
def setDeactivationLinearTreshold(linearTreshold):
	"""
	
	Sets the linear velocity that an object must be below before the deactivation timer can start.
	
	This affects every object in the scene, except for game objects that have 'No sleeping' turned on.
	
	@param linearTreshold: The linear velocity.
	@type linearTreshold: float
	"""
def setDeactivationAngularTreshold(angularTreshold):
	"""
	
	Sets the angular velocity that an object must be below before the deactivation timer can start.
	
	This affects every object in the scene, except for game objects that have 'No sleeping' turned on.
	
	@param angularTreshold: The angular velocity.
	@type angularTreshold: float
	"""
def setDeactivationTime(time):
	"""
	
	Time (in seconds) after objects with velocity less then thresholds (see below) are deactivated.
	
	This affects every object in the scene, except for game objects that have 'No sleeping' turned on.
	
	This function is directly related with the 2 above functions.
	
	
	@param time: The time in seconds.
	@type time: float
	"""
def setGravity(gx, gy, gz):
	"""
	Sets the gravity for the actual scene only.
	
	All other scenes remain unaffected.
	
	This affects every object in the scene that has physics enabled.
	
	@param gx: The force of gravity on world x axis.
	@type gx: float
	@param gy: The force of gravity on world y axis.
	@type gy: float
	@param gz: The force of gravity on world z axis.
	@type gz: float
	"""
def setLinearAirDamping(damping):
	"""
	
	Sets the linear air resistance for all objects in the scene.
	
	@param damping: The linear air resistance.
	@type damping: float
	"""
def setNumIterations(numIter):
	"""
	Sets the number of times an iterative constraint solver is repeated.
	
	Increasing the number of iterations improves the constraint solver at the cost of performances & the speed of the game engine.
	
	@param numIter: The number of timesubsteps. (Input 0 to suspend simulation numSubStep)
	@type numIter: integer
	"""
def setNumTimeSubSteps(numSubStep):
	"""
	Set the quality of the entire physics simulation including collision detection and constraint solver.
	
	Increase the number of time substeps to improves the quality of the entire physics simulation at the cost of the performance & the speed of the game engine.
	
	@param numSubStep: The number of timesubsteps. (Input 0 to suspend simulation numSubStep)
	@type numSubStep: integer
	"""
#def setDebugMode():
#	"""
#	
#	
#	
#	@param numIter: 
#	@type numIter: 
#	"""
#def setCcdMode():
#	"""
#	Does something
#	
#	@rtype: 
#	"""
#def setContactBreakingTreshold():
#	"""
#	Does something
#	
#	@rtype: 
#	"""
#def setSolverDamping():
#	"""
#	Does something
#	
#	@rtype: 
#	"""
#def setSolverTau():
#	"""
#	Does something
#	
#	@rtype: 
#	"""
#def setSolverType():
#	"""
#	Does something
#	
#	@rtype: 
#	"""
#def setSorConstant():
#	"""
#	Does something
#	
#	@rtype: 
#	"""
#def setUseEpa():
#	"""
#	Does something
#	
#	@rtype: 
#	"""