# Documentation for SCA_KeyboardSensor
from SCA_ISensor import *

class SCA_KeyboardSensor(SCA_ISensor):
	"""
	A keyboard sensor detects player key presses.
	
	See module GameKeys for keycode values.
	"""
	
	def getKey():
		"""
		Returns the key code this sensor is looking for.
		"""
	
	def setKey(keycode):
		"""
		Set the key this sensor should listen for.
		
		@type keycode: keycode from GameKeys module
		"""
	
	def getHold1():
		"""
		Returns the key code for the first modifier this sensor is looking for.
		"""
	
	def setHold1():
		"""
		Sets the key code for the first modifier this sensor should look for.
		"""
	
	def getHold2():
		"""
		Returns the key code for the second modifier this sensor is looking for.
		"""
	
	def setHold2():
		"""
		Sets the key code for the second modifier this sensor should look for.
		"""
	
	def getPressedKeys():
		"""
		Get a list of keys that have either been pressed, or just released this frame.
		
		@rtype: list of key status. [[keycode, status]]
		"""
	
	def getCurrentlyPressedKeys():
		"""
		Get a list of currently pressed keys that have either been pressed, or just released
		
		@rtype: list of key status. [[keycode, status]]
		"""
	

