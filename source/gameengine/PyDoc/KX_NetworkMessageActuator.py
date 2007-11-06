# $Id$
# Documentation for KX_NetworkMessageActuator
from SCA_IActuator import *

class KX_NetworkMessageActuator(SCA_IActuator):
	"""
	Message Actuator
	"""
	def setToPropName(name):
		"""
		Messages will only be sent to objects with the given property name.
		
		@type name: string
		"""
	def setSubject(subject):
		"""
		Sets the subject field of the message.
		
		@type subject: string
		"""
	def setBodyType(bodytype):
		"""
		Sets the type of body to send.
		
		@type bodytype: boolean
		@param bodytype: True to send the value of a property, False to send the body text.
		"""
	def setBody(body):
		"""
		Sets the message body.
		
		@type body: string
		@param body: if the body type is True, this is the name of the property to send.
		             if the body type is False, this is the text to send.
		"""

