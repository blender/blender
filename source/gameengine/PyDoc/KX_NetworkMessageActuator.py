# $Id$
# Documentation for KX_NetworkMessageActuator
from SCA_IActuator import *

class KX_NetworkMessageActuator(SCA_IActuator):
	"""
	Message Actuator
	
	@ivar propName: Messages will only be sent to objects with the given property name.
	@type propName: string
	@ivar subject: The subject field of the message.
	@type subject: string
	@ivar body: The body of the message.
	@type body: string
	@ivar usePropBody: Send a property instead of a regular body message.
	@type usePropBody: boolean
	"""
	def setToPropName(name):
		"""
		DEPRECATED: Use the propName property instead.
		Messages will only be sent to objects with the given property name.
		
		@type name: string
		"""
	def setSubject(subject):
		"""
		DEPRECATED: Use the subject property instead.
		Sets the subject field of the message.
		
		@type subject: string
		"""
	def setBodyType(bodytype):
		"""
		DEPRECATED: Use the usePropBody property instead.
		Sets the type of body to send.
		
		@type bodytype: boolean
		@param bodytype: True to send the value of a property, False to send the body text.
		"""
	def setBody(body):
		"""
		DEPRECATED: Use the body property instead.
		Sets the message body.
		
		@type body: string
		@param body: if the body type is True, this is the name of the property to send.
		             if the body type is False, this is the text to send.
		"""

