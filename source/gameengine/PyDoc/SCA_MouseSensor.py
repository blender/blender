# $Id$
# Documentation for SCA_MouseSensor
from SCA_ISensor import *

class SCA_MouseSensor(SCA_ISensor):
	"""
	Mouse Sensor logic brick.
	
	Properties:
	
	@ivar position: current [x,y] coordinates of the mouse, in frame coordinates (pixels)
	@type position: [integer,interger]
	@ivar mode: sensor mode: 1=KX_MOUSESENSORMODE_LEFTBUTTON  2=KX_MOUSESENSORMODE_MIDDLEBUTTON
	                         3=KX_MOUSESENSORMODE_RIGHTBUTTON 4=KX_MOUSESENSORMODE_WHEELUP
	                         5=KX_MOUSESENSORMODE_WHEELDOWN   9=KX_MOUSESENSORMODE_MOVEMENT
	@type mode: integer
	"""

	def getXPosition():
		"""
		DEPRECATED: use the position property
		Gets the x coordinate of the mouse.
		
		@rtype: integer
		@return: the current x coordinate of the mouse, in frame coordinates (pixels)
		"""
	def getYPosition():
		"""
		DEPRECATED: use the position property
		Gets the y coordinate of the mouse.
		
		@rtype: integer
		@return: the current y coordinate of the mouse, in frame coordinates (pixels).
		"""	
	def getButtonStatus(button):
		"""
		Get the mouse button status.
		
		@type button: int
		@param button: value in GameLogic members KX_MOUSE_BUT_LEFT, KX_MOUSE_BUT_MIDDLE, KX_MOUSE_BUT_RIGHT
		
		@rtype: integer
		@return: value in GameLogic members KX_INPUT_NONE, KX_INPUT_NONE, KX_INPUT_JUST_ACTIVATED, KX_INPUT_ACTIVE, KX_INPUT_JUST_RELEASED
		"""
