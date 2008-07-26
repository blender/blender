# $Id$
# Documentation for BL_ShapeActionActuator
from SCA_IActuator import *

class BL_ShapeActionActuator(SCA_IActuator):
	"""
	ShapeAction Actuators apply an shape action to an mesh object.
	"""
	def setAction(action, reset = True):
		"""
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
		Specifies the starting frame of the animation.
		
		@param start: the starting frame of the animation
		@type start: float
		"""

	def setEnd(end):
		"""
		Specifies the ending frame of the animation.
		
		@param end: the ending frame of the animation
		@type end: float
		"""
	def setBlendin(blendin):
		"""
		Specifies the number of frames of animation to generate
		when making transitions between actions.
		
		@param blendin: the number of frames in transition.
		@type blendin: float
		"""

	def setPriority(priority):
		"""
		Sets the priority of this actuator.
		
		@param priority: Specifies the new priority.  Actuators will lower
		                 priority numbers will override actuators with higher
		                 numbers.
		@type priority: integer
		"""
	def setFrame(frame):
		"""
		Sets the current frame for the animation.
		
		@param frame: Specifies the new current frame for the animation
		@type frame: float
		"""

	def setProperty(prop):
		"""
		Sets the property to be used in FromProp playback mode.
		
		@param prop: the name of the property to use.
		@type prop: string.
		"""

	def setBlendtime(blendtime):
		"""
		Sets the internal frame timer.
		 
		Allows the script to directly modify the internal timer
		used when generating transitions between actions.  
		
		@param blendtime: The new time. This parameter must be in the range from 0.0 to 1.0.
		@type blendtime: float
		"""

	def setType(mode):
		"""
		Sets the operation mode of the actuator

		@param mode: KX_ACTIONACT_PLAY, KX_ACTIONACT_PROPERTY, KX_ACTIONACT_FLIPPER, KX_ACTIONACT_LOOPSTOP, KX_ACTIONACT_LOOPEND
		@type mode: integer
		"""
	
	def setContinue(cont):
		"""
		Set the actions continue option True or False. see getContinue.

		@param cont: The continue option.
		@type cont: bool
		"""

	def getType():
		"""
		Returns the operation mode of the actuator
	    
		@rtype: integer
		@return: KX_ACTIONACT_PLAY, KX_ACTIONACT_PROPERTY, KX_ACTIONACT_FLIPPER, KX_ACTIONACT_LOOPSTOP, KX_ACTIONACT_LOOPEND
		"""

	def getContinue():
		"""
		When True, the action will always play from where last left off, otherwise negative events to this actuator will reset it to its start frame.
	    
		@rtype: bool
		"""
	
	def getAction():
		"""
		getAction() returns the name of the action associated with this actuator.
		
		@rtype: string
		"""
	
	def getStart():
		"""
		Returns the starting frame of the action.
		
		@rtype: float
		"""
	def getEnd():
		"""
		Returns the last frame of the action.
		
		@rtype: float
		"""
	def getBlendin():
		"""
		Returns the number of interpolation animation frames to be generated when this actuator is triggered.
		
		@rtype: float
		"""
	def getPriority():
		"""
		Returns the priority for this actuator.  Actuators with lower Priority numbers will
		override actuators with higher numbers.
		
		@rtype: integer
		"""
	def getFrame():
		"""
		Returns the current frame number.
		
		@rtype: float
		"""
	def getProperty():
		"""
		Returns the name of the property to be used in FromProp mode.
		
		@rtype: string
		"""


