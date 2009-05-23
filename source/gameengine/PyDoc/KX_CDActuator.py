# $Id$
# Documentation for CD Actuator
from SCA_IActuator import *

class KX_CDActuator(SCA_IActuator):
	"""
	CD Controller actuator.
	@ivar volume: controls the volume to set the CD to. 0.0 = silent, 1.0 = max volume.
	@type volume: float
	@ivar track: the track selected to be played
	@type track: integer
	@ivar gain: the gain (volume) of the CD between 0.0 and 1.0.
	@type gain: float
	"""
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
	def resumeCD():
		"""
		Resumes the CD after a pause.
		"""
	def playAll():
		"""
		Plays the CD from the beginning.
		"""
	def playTrack(trackNumber):
		"""
		Plays the track selected.
		"""
	def setGain(gain):
		"""
		DEPRECATED: Use the volume property.
		Sets the gain (volume) of the CD.
		
		@type gain: float
		@param gain: the gain to set the CD to. 0.0 = silent, 1.0 = max volume.
		"""
	def getGain():
		"""
		DEPRECATED: Use the volume property.
		Gets the current gain (volume) of the CD.
		
		@rtype: float
		@return: Between 0.0 (silent) and 1.0 (max volume)
		"""

