# $Id$
# Documentation for KX_SoundActuator
from SCA_IActuator import *

class KX_SoundActuator(SCA_IActuator):
	"""
	Sound Actuator.
	
	The L{startSound()}, L{pauseSound()} and L{stopSound()} do not require
	the actuator to be activated - they act instantly.

	@ivar filename: Sets the filename of the sound this actuator plays.
	@type filename: string

	@ivar volume: Sets the volume (gain) of the sound.
	@type volume: float

	@ivar pitch: Sets the pitch of the sound.
	@type pitch: float
	
	@ivar rollOffFactor: Sets the roll off factor. Rolloff defines the rate of attenuation as the sound gets further away.
	@type rollOffFactor: float
	
	@ivar looping: Sets the loop mode of the actuator.
	@type looping: integer
	
	@ivar position: Sets the position of the sound.
	@type position: float array
	
	@ivar velocity: Sets the speed of the sound; The speed of the sound alter the pitch.
	@type velocity: float array
	
	@ivar orientation: Sets the orientation of the sound. When setting the orientation you can 
	                   also use quaternion [float,float,float,float] or euler angles [float,float,float]
	@type orientation: 3x3 matrix [[float]]
	
	@ivar type: Sets the operation mode of the actuator. You can use one of the following constant:
	            KX_SOUNDACT_PLAYSTOP               (1)
			    KX_SOUNDACT_PLAYEND                (2)
			    KX_SOUNDACT_LOOPSTOP               (3)
			    KX_SOUNDACT_LOOPEND                (4)
			    KX_SOUNDACT_LOOPBIDIRECTIONAL      (5)
			    KX_SOUNDACT_LOOPBIDIRECTIONAL_STOP (6)
	@type type:	integer
	
	@group Play Methods: startSound, pauseSound, stopSound.
	"""
	def setFilename(filename):
		"""
		DEPRECATED: Use the filename property instead.
        Sets the filename of the sound this actuator plays.
		
		@type filename: string
		"""
	def getFilename():
		"""
		DEPRECATED: Use the filename property instead.
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
		DEPRECATED: Use the volume property instead
		Sets the gain (volume) of the sound
		
		@type gain: float
		@param gain: 0.0 (quiet) <= gain <= 1.0 (loud)
		"""
	def getGain():
		"""
		DEPRECATED: Use the volume property instead.
		Gets the gain (volume) of the sound.
		
		@rtype: float
		"""
	def setPitch(pitch):
		"""
		DEPRECATED: Use the pitch property instead.
		Sets the pitch of the sound.
		
		@type pitch: float
		"""
	def getPitch():
		"""
		DEPRECATED: Use the pitch property instead.
		Returns the pitch of the sound.
		
		@rtype: float
		"""
	def setRollOffFactor(rolloff):
		"""
		DEPRECATED: Use the rollOffFactor property instead.
		Sets the rolloff factor for the sounds.
		
		Rolloff defines the rate of attenuation as the sound gets further away.
		Higher rolloff factors shorten the distance at which the sound can be heard.
		
		@type rolloff: float
		"""
	def getRollOffFactor():
		"""
		DEPRECATED: Use the rollOffFactor property instead.
		Returns the rolloff factor for the sound.
		
		@rtype: float
		"""
	def setLooping(loop):
		"""
		DEPRECATED: Use the looping property instead.
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
		DEPRECATED: Use the looping property instead.
		Returns the current loop mode of the actuator.
		
		@rtype: integer
		"""
	def setPosition(x, y, z):
		"""
		DEPRECATED: Use the position property instead.
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
		DEPRECATED: Use the velocity property instead.
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
		DEPRECATED: Use the orientation property instead.
		Sets the orientation of the sound.
		
		The nine parameters specify a rotation matrix::
			| o11, o12, o13 |
			| o21, o22, o23 |
			| o31, o32, o33 |
		"""
	
	def setType(mode):
		"""
		DEPRECATED: Use the type property instead.
		Sets the operation mode of the actuator.
		
		@param mode: KX_SOUNDACT_PLAYSTOP, KX_SOUNDACT_PLAYEND, KX_SOUNDACT_LOOPSTOP, KX_SOUNDACT_LOOPEND, KX_SOUNDACT_LOOPBIDIRECTIONAL, KX_SOUNDACT_LOOPBIDIRECTIONAL_STOP
		@type mode: integer
		"""

	def getType():
		"""
		DEPRECATED: Use the type property instead.
		Returns the operation mode of the actuator.
		
		@rtype: integer
		@return:  KX_SOUNDACT_PLAYSTOP, KX_SOUNDACT_PLAYEND, KX_SOUNDACT_LOOPSTOP, KX_SOUNDACT_LOOPEND, KX_SOUNDACT_LOOPBIDIRECTIONAL, KX_SOUNDACT_LOOPBIDIRECTIONAL_STOP
		"""
