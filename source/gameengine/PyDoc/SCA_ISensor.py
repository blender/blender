# $Id$
# Documentation for SCA_ISensor
from SCA_ILogicBrick import *

class SCA_ISensor(SCA_ILogicBrick):
	"""
	Base class for all sensor logic bricks.
	
	@ivar usePosPulseMode: Flag to turn positive pulse mode on and off.
	@type usePosPulseMode: boolean
	@ivar useNegPulseMode: Flag to turn negative pulse mode on and off.
	@type useNegPulseMode: boolean
	@ivar frequency: The frequency for pulse mode sensors.
	@type frequency: int
	@ivar level: Flag to set whether to detect level or edge transition when entering a state.
					It makes a difference only in case of logic state transition (state actuator).
					A level detector will immediately generate a pulse, negative or positive
					depending on the sensor condition, as soon as the state is activated.
					A edge detector will wait for a state change before generating a pulse.
	@type level: boolean
	@ivar invert: Flag to set if this sensor activates on positive or negative events.
	@type invert: boolean
	@ivar triggered: True if this sensor brick is in a positive state. (Read only)
	@type triggered: boolean
	@ivar positive: True if this sensor brick is in a positive state. (Read only)
	@type positive: boolean
	"""
	
	def reset():
		"""
		Reset sensor internal state, effect depends on the type of sensor and settings.
		
		The sensor is put in its initial state as if it was just activated.
		"""
		
	#--The following methods are deprecated--
	def isPositive():
		"""
		True if this sensor brick is in a positive state.
		"""
	
	def isTriggered():
		"""
		True if this sensor brick has triggered the current controller.
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
		
		@rtype: integer
		@return: the pulse frequency in 1/50 sec.
		"""
	def setFrequency(freq):
		"""
		Sets the frequency for pulse mode sensors.
		
		@type freq: integer
		@return: the pulse frequency in 1/50 sec.
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
	def getLevel():
		"""
		Returns whether this sensor is a level detector or a edge detector.
		It makes a difference only in case of logic state transition (state actuator).
		A level detector will immediately generate a pulse, negative or positive
		depending on the sensor condition, as soon as the state is activated.
		A edge detector will wait for a state change before generating a pulse.
		
		@rtype: boolean
		@return: true if sensor is level sensitive, false if it is edge sensitive
		"""
	def setLevel(level):
		"""
		Set whether to detect level or edge transition when entering a state.
		
		@param level: Detect level instead of edge? (KX_TRUE, KX_FALSE)
		@type level: boolean
		"""
