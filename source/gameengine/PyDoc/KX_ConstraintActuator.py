# $Id$
# Documentation for KX_ConstraintActuator
from SCA_IActuator import *

class KX_ConstraintActuator(SCA_IActuator):
	"""
	A constraint actuator limits the position, rotation, distance or orientation of an object.
	"""
	def setDamp(time):
		"""
		Sets the time this constraint is delayed.
		
		@param time: The number of frames to delay.  
		             Negative values are ignored.
		@type time: integer
		"""
	def getDamp():
		"""
		Returns the damping time of the constraint.
		
		@rtype: integer
		"""
	def setMin(lower):
		"""
		Sets the lower bound of the constraint.
		
		For rotational and orientation constraints, lower is specified in degrees.
		
		@type lower: float
		"""
	def getMin():
		"""
		Gets the lower bound of the constraint.
		
		For rotational and orientation constraints, the lower bound is returned in radians.
		
		@rtype: float
		"""
	def setMax(upper):
		"""
		Sets the upper bound of the constraint.
		
		For rotational and orientation constraints, upper is specified in degrees.
		
		@type upper: float
		"""
	def getMax():
		"""
		Gets the upper bound of the constraint.
		
		For rotational and orientation constraints, the upper bound is returned in radians.
		
		@rtype: float
		"""
	def setLimit(limit):
		"""
		Sets the type of constraint.
		
		See module L{GameLogic} for valid constraint types.
		
		@param limit: Position constraints: KX_CONSTRAINTACT_LOCX, KX_CONSTRAINTACT_LOCY, KX_CONSTRAINTACT_LOCZ, 
		              Rotation constraints: KX_CONSTRAINTACT_ROTX, KX_CONSTRAINTACT_ROTY or KX_CONSTRAINTACT_ROTZ
		              Distance contraints: KX_ACT_CONSTRAINT_DIRPX, KX_ACT_CONSTRAINT_DIRPY, KX_ACT_CONSTRAINT_DIRPZ,
		                                   KX_ACT_CONSTRAINT_DIRNX, KX_ACT_CONSTRAINT_DIRNY, KX_ACT_CONSTRAINT_DIRNZ,
		              Orientation constraints: KX_ACT_CONSTRAINT_ORIX, KX_ACT_CONSTRAINT_ORIY, KX_ACT_CONSTRAINT_ORIZ
		"""
	def getLimit():
		"""
		Gets the type of constraint.
		
		See module L{GameLogic} for valid constraints.
		
		@return: Position constraints: KX_CONSTRAINTACT_LOCX, KX_CONSTRAINTACT_LOCY, KX_CONSTRAINTACT_LOCZ, 
		         Rotation constraints: KX_CONSTRAINTACT_ROTX, KX_CONSTRAINTACT_ROTY, KX_CONSTRAINTACT_ROTZ,
		         Distance contraints: KX_ACT_CONSTRAINT_DIRPX, KX_ACT_CONSTRAINT_DIRPY, KX_ACT_CONSTRAINT_DIRPZ,
		                              KX_ACT_CONSTRAINT_DIRNX, KX_ACT_CONSTRAINT_DIRNY, KX_ACT_CONSTRAINT_DIRNZ,
		         Orientation constraints: KX_ACT_CONSTRAINT_ORIX, KX_ACT_CONSTRAINT_ORIY, KX_ACT_CONSTRAINT_ORIZ
		"""
	def setRotDamp(duration):
		"""
		Sets the time constant of the orientation constraint.
		
		@param duration: If the duration is negative, it is set to 0.
		@type duration: integer
		"""
	def getRotDamp():
		""" 
		Returns the damping time for application of the constraint.
		
		@rtype: integer
		"""
	def setDirection(vector):
		"""
		Sets the reference direction in world coordinate for the orientation constraint
		
		@type vector: 3-tuple
		"""
	def getDirection():
		"""
		Returns the reference direction of the orientation constraint in world coordinate.
		
		@rtype: 3-tuple
		"""
	def setOption(option):
		"""
		Sets several options of the distance constraint.
		
		@type option: integer
		@param option: Binary combination of the following values:
		                64 : Activate alignment to surface
		               128 : Detect material rather than property
		               256 : No deactivation if ray does not hit target
		               512 : Activate distance control
		"""
	def getOption():
		"""
		Returns the option parameter.
		
		@rtype: integer
		"""
	def setTime(duration):
		"""
		Sets the activation time of the actuator.
		
		@type duration: integer
		@param duration: The actuator disables itself after this many frame.
		                 If set to 0 or negative, the actuator is not limited in time.
		"""
	def getTime():
		"""
		Returns the time parameter.
		
		@rtype: integer
		"""
	def setProperty(property):
		"""
		Sets the name of the property or material for the ray detection of the distance constraint.
		
		@type property: string
		@param property: If empty, the ray will detect any collisioning object.
		"""
	def getProperty():
		"""
		Returns the property parameter.
		
		@rtype: string
		"""
	def setDistance(distance):
		"""
		Sets the target distance in distance constraint.
		
		@type distance: float
		"""
	def getDistance():
		"""
		Returns the distance parameter.
		
		@rtype: float
		"""
	def setRayLength(length):
		"""
		Sets the maximum ray length of the distance constraint.
		
		@type length: float
		"""
	def getRayLength():
		"""
		Returns the length of the ray
		
		@rtype: float
		"""








		
