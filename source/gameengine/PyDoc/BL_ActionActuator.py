# $Id$
# Documentation for BL_ActionActuator
import SCA_ILogicBrick
from SCA_IActuator import *


class BL_ActionActuator(SCA_IActuator):
	"""
	Action Actuators apply an action to an actor.
	
	@ivar action: The name of the action to set as the current action.
	@type action: string
	@ivar start: Specifies the starting frame of the animation.
	@type start: float
	@ivar end: Specifies the ending frame of the animation.
	@type end: float
	@ivar blendin: Specifies the number of frames of animation to generate when making transitions between actions.
	@type blendin: float
	@ivar priority: Sets the priority of this actuator. Actuators will lower
		                 priority numbers will override actuators with higher
		                 numbers.
	@type priority: integer
	@ivar frame: Sets the current frame for the animation.
	@type frame: float
	@ivar property: Sets the property to be used in FromProp playback mode.
	@type property: string
	@ivar blendTime: Sets the internal frame timer. This property must be in
						the range from 0.0 to blendin.
	@type blendTime: float
	@ivar type: The operation mode of the actuator. KX_ACTIONACT_PLAY, KX_ACTIONACT_PROPERTY, KX_ACTIONACT_FLIPPER, KX_ACTIONACT_LOOPSTOP, KX_ACTIONACT_LOOPEND
	@type type: integer
	@ivar continue: The actions continue option, True or False.
					When True, the action will always play from where last left off,
					otherwise negative events to this actuator will reset it to its start frame.
	@type: boolean
	@ivar frameProperty: The name of the property that is set to the current frame number.
	@type frameProperty: string
	"""
	def setChannel(channel, matrix, mode = False):
		"""
		@param channel: A string specifying the name of the bone channel.
		@type channel: string
		@param matrix: A 4x4 matrix specifying the overriding transformation
		               as an offset from the bone's rest position.
		@type matrix: list [[float]]
		@param mode: True for armature/world space, False for bone space
		@type mode: boolean
		"""

	#--The following methods are deprecated--
	def setAction(action, reset = True):
		"""
		DEPRECATED: use the 'action' property
		Sets the current action.
		
		@param action: The name of the action to set as the current action.
		@type action: string
		@param reset: Optional parameter indicating whether to reset the
		              blend timer or not.  A value of 1 indicates that the
		              timer should be reset.  A value of 0 will leave it
		              unchanged.  If reset is not specified, the timer will
		              be reset.
		"""

	def setStart(start):
		"""
		DEPRECATED: use the 'start' property
		Specifies the starting frame of the animation.
		
		@param start: the starting frame of the animation
		@type start: float
		"""

	def setEnd(end):
		"""
		DEPRECATED: use the 'end' property
		Specifies the ending frame of the animation.
		
		@param end: the ending frame of the animation
		@type end: float
		"""
	def setBlendin(blendin):
		"""
		DEPRECATED: use the 'blendin' property
		Specifies the number of frames of animation to generate
		when making transitions between actions.
		
		@param blendin: the number of frames in transition.
		@type blendin: float
		"""

	def setPriority(priority):
		"""
		DEPRECATED: use the 'priority' property
		Sets the priority of this actuator.
		
		@param priority: Specifies the new priority.  Actuators will lower
		                 priority numbers will override actuators with higher
		                 numbers.
		@type priority: integer
		"""
	def setFrame(frame):
		"""
		DEPRECATED: use the 'frame' property
		Sets the current frame for the animation.
		
		@param frame: Specifies the new current frame for the animation
		@type frame: float
		"""

	def setProperty(prop):
		"""
		DEPRECATED: use the 'property' property
		Sets the property to be used in FromProp playback mode.
		
		@param prop: the name of the property to use.
		@type prop: string.
		"""

	def setBlendtime(blendtime):
		"""
		DEPRECATED: use the 'blendTime' property
		Sets the internal frame timer.
		 
		Allows the script to directly modify the internal timer
		used when generating transitions between actions.  
		
		@param blendtime: The new time. This parameter must be in the range from 0.0 to 1.0.
		@type blendtime: float
		"""

	def setType(mode):
		"""
		DEPRECATED: use the 'type' property
		Sets the operation mode of the actuator

		@param mode: KX_ACTIONACT_PLAY, KX_ACTIONACT_PROPERTY, KX_ACTIONACT_FLIPPER, KX_ACTIONACT_LOOPSTOP, KX_ACTIONACT_LOOPEND
		@type mode: integer
		"""
	
	def setContinue(cont):
		"""
		DEPRECATED: use the 'continue' property
		Set the actions continue option True or False. see getContinue.

		@param cont: The continue option.
		@type cont: bool
		"""

	def getType():
		"""
		DEPRECATED: use the 'type' property
		Returns the operation mode of the actuator
	    
		@rtype: integer
		@return: KX_ACTIONACT_PLAY, KX_ACTIONACT_PROPERTY, KX_ACTIONACT_FLIPPER, KX_ACTIONACT_LOOPSTOP, KX_ACTIONACT_LOOPEND
		"""

	def getContinue():
		"""
		DEPRECATED: use the 'continue' property
		When True, the action will always play from where last left off, otherwise negative events to this actuator will reset it to its start frame.
	    
		@rtype: bool
		"""
	
	def getAction():
		"""
		DEPRECATED: use the 'action' property
		getAction() returns the name of the action associated with this actuator.
		
		@rtype: string
		"""
	
	def getStart():
		"""
		DEPRECATED: use the 'start' property
		Returns the starting frame of the action.
		
		@rtype: float
		"""
	def getEnd():
		"""
		DEPRECATED: use the 'end' property
		Returns the last frame of the action.
		
		@rtype: float
		"""
	def getBlendin():
		"""
		DEPRECATED: use the 'blendin' property
		Returns the number of interpolation animation frames to be generated when this actuator is triggered.
		
		@rtype: float
		"""
	def getPriority():
		"""
		DEPRECATED: use the 'priority' property
		Returns the priority for this actuator.  Actuators with lower Priority numbers will
		override actuators with higher numbers.
		
		@rtype: integer
		"""
	def getFrame():
		"""
		DEPRECATED: use the 'frame' property
		Returns the current frame number.
		
		@rtype: float
		"""
	def getProperty():
		"""
		DEPRECATED: use the 'property' property
		Returns the name of the property to be used in FromProp mode.
		
		@rtype: string
		"""
	def setFrameProperty(prop):
		"""
		DEPRECATED: use the 'frameProperty' property
		@param prop: A string specifying the property of the object that will be updated with the action frame number.
		@type prop: string
		"""
	def getFrameProperty():
		"""
		DEPRECATED: use the 'frameProperty' property
		Returns the name of the property that is set to the current frame number.
		
		@rtype: string
		"""
