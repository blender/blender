# $Id$
# Documentation for KX_NetworkMessageSensor
from SCA_ISensor import *

class KX_NetworkMessageSensor(SCA_ISensor):
	"""
	The Message Sensor logic brick.
	
	Currently only loopback (local) networks are supported.
	"""
	def setSubjectFilterText(subject):
		"""
		Change the message subject text that this sensor is listening to.
		
		@type subject: string
		@param subject: the new message subject to listen for.
		"""
	
	def getFrameMessageCount():
		"""
		Get the number of messages received since the last frame.
		
		@rtype: integer
		"""
	def getBodies():
		"""
		Gets the list of message bodies.
		
		@rtype: list
		"""
	def getSubject():
		"""
		Gets the message subject this sensor is listening for from the Subject: field.
		
		@rtype: string
		"""
	def getSubjects():
		"""
		Gets the list of message subjects received.
		
		@rtype list
		"""
	