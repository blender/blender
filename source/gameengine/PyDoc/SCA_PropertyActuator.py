# $Id$
# Documentation for SCA_PropertyActuator
from SCA_IActuator import *

class SCA_PropertyActuator(SCA_IActuator):
	"""
	Property Actuator
	"""
	def setProperty(prop):
		"""
		Set the property on which to operate. 
		
		If there is no property of this name, the call is ignored.
		
		@type prop: string
		@param prop: The name of the property to set.
		"""
	def getProperty():
		"""
		Returns the name of the property on which to operate.
		
		@rtype: string
		"""
	def setValue(value):
		"""
		Set the value with which the actuator operates. 
		
		If the value is not compatible with the type of the 
		property, the subsequent action is ignored.
		
		@type value: string
		"""
	def getValue():
		"""
		Gets the value with which this actuator operates.
		
		@rtype: string
		"""
