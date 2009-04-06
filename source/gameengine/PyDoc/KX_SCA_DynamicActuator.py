# $Id$
# Documentation for  KX_SCA_DynamicActuator
from SCA_IActuator import *

class KX_SCA_DynamicActuator(SCA_IActuator):
	"""
	Dynamic Actuator.
	@ivar operation: the type of operation of the actuator, 0-4
						KX_DYN_RESTORE_DYNAMICS, KX_DYN_DISABLE_DYNAMICS, 
						KX_DYN_ENABLE_RIGID_BODY, KX_DYN_DISABLE_RIGID_BODY, KX_DYN_SET_MASS
	@type operation: integer
	@ivar mass: the mass value for the KX_DYN_SET_MASS operation
	@type mass: float
	"""
	def setOperation(operation):
		"""
		DEPRECATED: Use the operation property instead.
		Set the type of operation when the actuator is activated:
				- 0 = restore dynamics
				- 1 = disable dynamics
				- 2 = enable rigid body
				- 3 = disable rigid body
				- 4 = set mass
		"""
	def getOperation():
		"""
		DEPRECATED: Use the operation property instead.
		return the type of operation
		"""
	
