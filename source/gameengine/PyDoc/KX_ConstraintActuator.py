# $Id$
# Documentation for KX_ConstraintActuator
from SCA_IActuator import *

class KX_ConstraintActuator(SCA_IActuator):
	"""
	A constraint actuator limits the position or orientation of an object.
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
		
		For rotational constraints, lower is specified in degrees.
		
		@type lower: float
		"""
	def getMin():
		"""
		Gets the lower bound of the constraint.
		
		For rotational constraints, the lower bound is returned in radians.
		
		@rtype: float
		"""
	def setMax(upper):
		"""
		Sets the upper bound of the constraint.
		
		For rotational constraints, upper is specified in degrees.
		
		@type upper: float
		"""
	def getMax():
		"""
		Gets the upper bound of the constraint.
		
		For rotational constraints, the upper bound is returned in radians.
		
		@rtype: float
		"""
	def setLimit(limit):
		"""
		Sets the type of constraint.
		
		See module L{GameLogic} for valid constraint types.
		
		@param limit: Position constraints: KX_CONSTRAINTACT_LOCX, KX_CONSTRAINTACT_LOCY, KX_CONSTRAINTACT_LOCZ, 
		              Rotation constraints: KX_CONSTRAINTACT_ROTX, KX_CONSTRAINTACT_ROTY or KX_CONSTRAINTACT_ROTZ
		"""
	def getLimit():
		"""
		Gets the type of constraint.
		
		See module L{GameLogic} for valid constraints.
		
		@return: Position constraints: KX_CONSTRAINTACT_LOCX, KX_CONSTRAINTACT_LOCY, KX_CONSTRAINTACT_LOCZ, 
		         Rotation constraints: KX_CONSTRAINTACT_ROTX, KX_CONSTRAINTACT_ROTY or KX_CONSTRAINTACT_ROTZ
		"""
