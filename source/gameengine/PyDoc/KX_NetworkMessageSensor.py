# $Id$
# Documentation for KX_NetworkMessageSensor
from SCA_ISensor import *

class KX_NetworkMessageSensor(SCA_ISensor):
	"""
	The Message Sensor logic brick.
	
	Currently only loopback (local) networks are supported.
	
	@ivar subject: The subject the sensor is looking for.
	@type subject: string
	@ivar frameMessageCount: The number of messages received since the last frame.
								(Read-only)
	@type framemessageCount: int
	@ivar subjects: The list of message subjects received. (Read-only)
	@type subjects: list of strings
	@ivar bodies: The list of message bodies received. (Read-only)
	@type bodies: list of strings
	"""
	
	
	def setSubjectFilterText(subject):
		"""
		DEPRECATED: Use the subject property instead.
		Change the message subject text that this sensor is listening to.
		
		@type subject: string
		@param subject: the new message subject to listen for.
		"""
	
	def getFrameMessageCount():
		"""
		DEPRECATED: Use the frameMessageCount property instead.
		Get the number of messages received since the last frame.
		
		@rtype: integer
		"""
	def getBodies():
		"""
		DEPRECATED: Use the bodies property instead.
		Gets the list of message bodies.
		
		@rtype: list
		"""
	def getSubject():
		"""
		DEPRECATED: Use the subject property instead.
		Gets the message subject this sensor is listening for from the Subject: field.
		
		@rtype: string
		"""
	def getSubjects():
		"""
		DEPRECATED: Use the subjects property instead.
		Gets the list of message subjects received.
		
		@rtype: list
		"""
	