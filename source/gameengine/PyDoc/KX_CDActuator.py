# $Id$
# Documentation for CD Actuator
from SCA_ILogicBrick import *

class KX_CDActuator(SCA_ILogicBrick):
	def startCD():
		"""
		Starts the CD playing.
		"""
	def stopCD():
		"""
		Stops the CD playing.
		"""
	def pauseCD():
		"""
		Pauses the CD.
		"""
	def setGain(gain):
		"""
		Sets the gain (volume) of the CD.
		
		@type gain: float
		@param gain: the gain to set the CD to. 0.0 = silent, 1.0 = max volume.
		"""
	def getGain():
		"""
		Gets the current gain (volume) of the CD.
		
		@rtype: float
		@return: Between 0.0 (silent) and 1.0 (max volume)
		"""

