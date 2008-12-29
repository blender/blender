# $Id$
# Documentation for KX_ActuatorSensor
from SCA_IActuator import *
from SCA_ISensor import *

class KX_ActuatorSensor(SCA_ISensor):
	"""
	Actuator sensor detect change in actuator state of the parent object.
	It generates a positive pulse if the corresponding actuator is activated
	and a negative pulse if the actuator is deactivated.
	
	Properties:
	
	@ivar actuator: the name of the actuator that the sensor is monitoring.
	@type actuator: string
	"""
	def getActuator():
		"""
		DEPRECATED: use the actuator property
		Return the Actuator with which the sensor operates.
		
		@rtype: string
		"""
	def setActuator(name):
		"""
		DEPRECATED: use the actuator property
		Sets the Actuator with which to operate. If there is no Actuator
		of this name, the function has no effect.
		
		@param name: actuator name
		@type name: string
		"""
