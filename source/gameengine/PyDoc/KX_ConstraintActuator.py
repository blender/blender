# $Id$
# Documentation for KX_ConstraintActuator
from SCA_IActuator import *

class KX_ConstraintActuator(SCA_IActuator):
	"""
	A constraint actuator limits the position, rotation, distance or orientation of an object.
	
	Properties:
	
	@ivar damp: time constant of the constraint expressed in frame (not use by Force field constraint)
	@type damp: integer
	
	@ivar rotDamp: time constant for the rotation expressed in frame (only for the distance constraint)
	               0 = use damp for rotation as well
	@type rotDamp: integer
	
	@ivar direction: the reference direction in world coordinate for the orientation constraint
	@type direction: 3-tuple of float: [x,y,z]
	
	@ivar option: Binary combination of the following values:
				Applicable to Distance constraint:
					- KX_ACT_CONSTRAINT_NORMAL    (  64) : Activate alignment to surface
					- KX_ACT_CONSTRAINT_DISTANCE  ( 512) : Activate distance control
					- KX_ACT_CONSTRAINT_LOCAL		(1024) : direction of the ray is along the local axis
				Applicable to Force field constraint:					
					- KX_ACT_CONSTRAINT_DOROTFH   (2048) : Force field act on rotation as well
				Applicable to both:
					- KX_ACT_CONSTRAINT_MATERIAL  ( 128) : Detect material rather than property
					- KX_ACT_CONSTRAINT_PERMANENT ( 256) : No deactivation if ray does not hit target
	@type option: integer
	
	@ivar time: activation time of the actuator. The actuator disables itself after this many frame.
		        If set to 0, the actuator is not limited in time.
	@type time: integer
	
	@ivar property: the name of the property or material for the ray detection of the distance constraint.
	@type property: string
	
	@ivar min: The lower bound of the constraint
	           For the rotation and orientation constraint, it represents radiant
	@type min: float
	
	@ivar distance: the target distance of the distance constraint
	@type distance: float
	
	@ivar max: the upper bound of the constraint.
	           For rotation and orientation constraints, it represents radiant.
	@type max: float
	
	@ivar rayLength: the length of the ray of the distance constraint.
	@type rayLength: float
	
	@ivar limit: type of constraint, use one of the following constant:
	              KX_ACT_CONSTRAINT_LOCX  ( 1) : limit X coord
	              KX_ACT_CONSTRAINT_LOCY  ( 2) : limit Y coord
	              KX_ACT_CONSTRAINT_LOCZ  ( 3) : limit Z coord
	              KX_ACT_CONSTRAINT_ROTX  ( 4) : limit X rotation
	              KX_ACT_CONSTRAINT_ROTY  ( 5) : limit Y rotation
	              KX_ACT_CONSTRAINT_ROTZ  ( 6) : limit Z rotation
	              KX_ACT_CONSTRAINT_DIRPX ( 7) : set distance along positive X axis
	              KX_ACT_CONSTRAINT_DIRPY ( 8) : set distance along positive Y axis
	              KX_ACT_CONSTRAINT_DIRPZ ( 9) : set distance along positive Z axis
	              KX_ACT_CONSTRAINT_DIRNX (10) : set distance along negative X axis
	              KX_ACT_CONSTRAINT_DIRNY (11) : set distance along negative Y axis
	              KX_ACT_CONSTRAINT_DIRNZ (12) : set distance along negative Z axis
	              KX_ACT_CONSTRAINT_ORIX  (13) : set orientation of X axis
	              KX_ACT_CONSTRAINT_ORIY  (14) : set orientation of Y axis
	              KX_ACT_CONSTRAINT_ORIZ  (15) : set orientation of Z axis
	              KX_ACT_CONSTRAINT_FHPX  (16) : set force field along positive X axis
	              KX_ACT_CONSTRAINT_FHPY  (17) : set force field along positive Y axis
	              KX_ACT_CONSTRAINT_FHPZ  (18) : set force field along positive Z axis
	              KX_ACT_CONSTRAINT_FHNX  (19) : set force field along negative X axis
	              KX_ACT_CONSTRAINT_FHNY  (20) : set force field along negative Y axis
	              KX_ACT_CONSTRAINT_FHNZ  (21) : set force field along negative Z axis
	@type limit: integer
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
		
		@param limit:
			Position constraints: KX_CONSTRAINTACT_LOCX, KX_CONSTRAINTACT_LOCY, KX_CONSTRAINTACT_LOCZ
			Rotation constraints: KX_CONSTRAINTACT_ROTX, KX_CONSTRAINTACT_ROTY or KX_CONSTRAINTACT_ROTZ
			Distance contraints: KX_ACT_CONSTRAINT_DIRPX, KX_ACT_CONSTRAINT_DIRPY, KX_ACT_CONSTRAINT_DIRPZ, KX_ACT_CONSTRAINT_DIRNX, KX_ACT_CONSTRAINT_DIRNY, KX_ACT_CONSTRAINT_DIRNZ
			Orientation constraints: KX_ACT_CONSTRAINT_ORIX, KX_ACT_CONSTRAINT_ORIY, KX_ACT_CONSTRAINT_ORIZ
		"""
	def getLimit():
		"""
		Gets the type of constraint.
		
		See module L{GameLogic} for valid constraints.
		
		@return:
			Position constraints: KX_CONSTRAINTACT_LOCX, KX_CONSTRAINTACT_LOCY, KX_CONSTRAINTACT_LOCZ, 
			Rotation constraints: KX_CONSTRAINTACT_ROTX, KX_CONSTRAINTACT_ROTY, KX_CONSTRAINTACT_ROTZ,
			Distance contraints: KX_ACT_CONSTRAINT_DIRPX, KX_ACT_CONSTRAINT_DIRPY, KX_ACT_CONSTRAINT_DIRPZ, KX_ACT_CONSTRAINT_DIRNX, KX_ACT_CONSTRAINT_DIRNY, KX_ACT_CONSTRAINT_DIRNZ,
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
		               64  : Activate alignment to surface
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








		
