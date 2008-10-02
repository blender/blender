# $Id$
# Documentation for SCA_DelaySensor
from SCA_ISensor import *

class SCA_DelaySensor(SCA_ISensor):
	"""
	The Delay sensor generates positive and negative triggers at precise time,
	expressed in number of frames. The delay parameter defines the length
	of the initial OFF period. A positive trigger is generated at the end of this period. 
	The duration parameter defines the length of the ON period following the OFF period.
	There is a negative trigger at the end of the ON period. If duration is 0, the sensor
	stays ON and there is no negative trigger.
	The sensor runs the OFF-ON cycle once unless the repeat option is set: the
	OFF-ON cycle repeats indefinately (or the OFF cycle if duration is 0).
	Use SCA_ISensor::reset() at any time to restart sensor.
	"""
	def setDelay(delay):
		"""
		Set the initial delay before the positive trigger.
		
		@param delay: length of the initial OFF period as number of frame, 0 for immediate trigger
		@type delay: integer
		"""
	def setDuration(duration):
		"""
		Set the duration of the ON pulse after initial delay and the generation of the positive trigger.
		If duration is greater than 0, a negative trigger is sent at the end of the ON pulse.
		
		@param duration: length of the ON period in number of frame after the initial OFF period
		@type duration: integer
		"""	
	def setRepeat(repeat):
		"""
		Set if the sensor repeat mode.
		
		@param repeat: 1 if the OFF-ON cycle should be repeated indefinately, 0 if it should run once.
		@type repeat: integer
		"""		
	def getDelay():
		"""
		Return the delay parameter value.
		
		@rtype: integer
		"""
	def getDuration():
		"""
		Return the duration parameter value
		
		@rtype: integer
		"""
	def getRepeat():
		"""
		Return the repeat parameter value
		
		@rtype: KX_TRUE or KX_FALSE
		"""
