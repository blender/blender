# $Id$
# Documentation for SCA_MouseSensor
from SCA_ISensor import *

class SCA_MouseSensor(SCA_ISensor):
	"""
	Mouse Sensor logic brick.
	"""

	def getXPosition():
		"""
		Gets the x coordinate of the mouse.
		
		@rtype: integer
		@return: the current x coordinate of the mouse, in frame coordinates (pixels)
		"""
	def getYPosition():
		"""
		Gets the y coordinate of the mouse.
		
		@rtype: integer
		@return: the current y coordinate of the mouse, in frame coordinates (pixels).
		"""	
