# $Id$
# Documentation for SCA_PropertySensor
from SCA_ISensor import *

class SCA_PropertySensor(SCA_ISensor):
	"""
	Activates when the game object property matches.
	"""
	
	def getType():
		"""
		Gets when to activate this sensor.
		
		KX_PROPSENSOR_EQUAL, KX_PROPSENSOR_NOTEQUAL,
			KX_PROPSENSOR_INTERVAL, KX_PROPSENSOR_CHANGED,
			or KX_PROPSENSOR_EXPRESSION.
		"""

	def setType(checktype):
		"""
		Set the type of check to perform.
		
		@type checktype: KX_PROPSENSOR_EQUAL, KX_PROPSENSOR_NOTEQUAL,
			KX_PROPSENSOR_INTERVAL, KX_PROPSENSOR_CHANGED,
			or KX_PROPSENSOR_EXPRESSION.
		"""
	
	def getProperty():
		"""
		Return the property with which the sensor operates.
		
		@rtype string
		@return the name of the property this sensor is watching.
		"""
	def setProperty(name):
		"""
		Sets the property with which to operate.  If there is no property
		of that name, this call is ignored.
		
		@type name: string.
		"""
	def getValue():
		"""
		Return the value with which the sensor compares to the value of the property.
		
		@rtype string
		@return the value of the property this sensor is watching.
		"""
	def setValue(value):
		"""
		Set the value with which the sensor operates. If the value
		is not compatible with the type of the property, the subsequent
		action is ignored.
		
		@type value: string
		"""

