# Documentation for SCA_ISensor
from SCA_ILogicBrick import *

class SCA_ISensor(SCA_ILogicBrick):
	"""
	Base class for all sensor logic bricks.
	"""
	
	def isPositive():
		"""
		True if this sensor brick has been activated.
		"""
	
	def getUsePosPulseMode():
		"""
		True if the sensor is in positive pulse mode.
		"""
	def setUsePosPulseMode(pulse):
		"""
		Sets positive pulse mode.
		
		@type pulse: boolean
		@param pulse: If True, will activate positive pulse mode for this sensor.
		"""
	def getFrequency():
		"""
		The frequency for pulse mode sensors.
		
		@rtype integer
		@return the pulse frequency in 1/50 sec.
		"""
	def setFrequency(freq):
		"""
		Sets the frequency for pulse mode sensors.
		
		@type freq: integer
		@return the pulse frequency in 1/50 sec.
		"""
	def getUseNegPulseMode():
		"""
		True if the sensor is in negative pulse mode.
		"""
	def setUseNegPulseMode(pulse):
		"""
		Sets negative pulse mode.
		
		@type pulse: boolean
		@param pulse: If True, will activate negative pulse mode for this sensor.
		"""
	def getInvert():
		"""
		True if this sensor activates on negative events.
		"""
	def setInvert(invert):
		"""
		Sets if this sensor activates on positive or negative events.
		
		@type invert: boolean
		@param invert: true if activates on negative events; false if activates on positive events.
		"""

