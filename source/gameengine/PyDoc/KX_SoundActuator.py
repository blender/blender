# $Id$
# Documentation for KX_SoundActuator
from SCA_IActuator import *

class KX_SoundActuator(SCA_IActuator):
	"""
	Sound Actuator.
	
	The L{startSound()}, L{pauseSound()} and L{stopSound()} do not require
	the actuator to be activated - they act instantly.
	
	@group Play Methods: startSound, pauseSound, stopSound
	"""
	def setFilename(filename):
		"""
		Sets the filename of the sound this actuator plays.
		
		@type filename: string
		"""
	def getFilename():
		"""
		Returns the filename of the sound this actuator plays.
		
		@rtype: string
		"""
	def startSound():
		"""
		Starts the sound.
		"""
	def pauseSound():
		"""
		Pauses the sound.
		"""
	def stopSound():
		"""
		Stops the sound.
		"""
	def setGain(gain):
		"""
		Sets the gain (volume) of the sound
		
		@type gain: float
		@param gain: 0.0 (quiet) <= gain <= 1.0 (loud)
		"""
	def getGain():
		"""
		Gets the gain (volume) of the sound.
		
		@rtype: float
		"""
	def setPitch(pitch):
		"""
		Sets the pitch of the sound.
		
		@type pitch: float
		"""
	def getPitch():
		"""
		Returns the pitch of the sound.
		
		@rtype: float
		"""
	def setRollOffFactor(rolloff):
		"""
		Sets the rolloff factor for the sounds.
		
		Rolloff defines the rate of attenuation as the sound gets further away.
		Higher rolloff factors shorten the distance at which the sound can be heard.
		
		@type rolloff: float
		"""
	def getRollOffFactor():
		"""
		Returns the rolloff factor for the sound.
		
		@rtype: float
		"""
	def setLooping(loop):
		"""
		Sets the loop mode of the actuator.
		
		@bug: There are no constants defined for this method!
		@param loop: - Play Stop	1
		             - Play End		2
			     - Loop Stop	3
			     - Loop End		4
			     - Bidirection Stop	5
			     - Bidirection End	6
		@type loop: integer
		"""
	def getLooping():
		"""
		Returns the current loop mode of the actuator.
		
		@rtype: integer
		"""
	def setPosition(x, y, z):
		"""
		Sets the position this sound will come from.
		
		@type x: float
		@param x: The x coordinate of the sound.
		@type y: float
		@param y: The y coordinate of the sound.
		@type z: float
		@param z: The z coordinate of the sound.
		"""
	def setVelocity(vx, vy, vz):
		"""
		Sets the velocity this sound is moving at.  
		
		The sound's pitch is determined from the velocity.
		
		@type vx: float
		@param vx: The vx coordinate of the sound.
		@type vy: float
		@param vy: The vy coordinate of the sound.
		@type vz: float
		@param vz: The vz coordinate of the sound.
		"""
	def setOrientation(o11, o12, o13, o21, o22, o23, o31, o32, o33):
		"""
		Sets the orientation of the sound.
		
		The nine parameters specify a rotation matrix::
			| o11, o12, o13 |
			| o21, o22, o23 |
			| o31, o32, o33 |
		"""
