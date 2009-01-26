# $Id$
# Documentation for SCA_PropertySensor
from SCA_ISensor import *

class SCA_PropertySensor(SCA_ISensor):
	"""
	Activates when the game object property matches.
	
	Properties:
	
	@ivar type: type of check on the property: 
	            KX_PROPSENSOR_EQUAL(1), KX_PROPSENSOR_NOTEQUAL(2), KX_PROPSENSOR_INTERVAL(3), 
	            KX_PROPSENSOR_CHANGED(4), KX_PROPSENSOR_EXPRESSION(5)
	@type type: integer
	@ivar property: the property with which the sensor operates.
	@type property: string
	@ivar value: the value with which the sensor compares to the value of the property.
	@type value: string
	"""
	
	def getType():
		"""
		DEPRECATED: use the type property
		Gets when to activate this sensor.
		
		@return: KX_PROPSENSOR_EQUAL, KX_PROPSENSOR_NOTEQUAL,
			 KX_PROPSENSOR_INTERVAL, KX_PROPSENSOR_CHANGED,
			 or KX_PROPSENSOR_EXPRESSION.
		"""

	def setType(checktype):
		"""
		DEPRECATED: use the type property
		Set the type of check to perform.
		
		@type checktype: KX_PROPSENSOR_EQUAL, KX_PROPSENSOR_NOTEQUAL,
			KX_PROPSENSOR_INTERVAL, KX_PROPSENSOR_CHANGED,
			or KX_PROPSENSOR_EXPRESSION.
		"""
	
	def getProperty():
		"""
		DEPRECATED: use the property property
		Return the property with which the sensor operates.
		
		@rtype: string
		@return: the name of the property this sensor is watching.
		"""
	def setProperty(name):
		"""
		DEPRECATED: use the property property
		Sets the property with which to operate.  If there is no property
		of that name, this call is ignored.
		
		@type name: string.
		"""
	def getValue():
		"""
		DEPRECATED: use the value property
		Return the value with which the sensor compares to the value of the property.
		
		@rtype: string
		@return: the value of the property this sensor is watching.
		"""
	def setValue(value):
		"""
		DEPRECATED: use the value property
		Set the value with which the sensor operates. If the value
		is not compatible with the type of the property, the subsequent
		action is ignored.
		
		@type value: string
		"""

