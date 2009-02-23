# $Id$
# Documentation for SCA_KeyboardSensor
from SCA_ISensor import *

class SCA_KeyboardSensor(SCA_ISensor):
	"""
	A keyboard sensor detects player key presses.
	
	See module L{GameKeys} for keycode values.
	
	@ivar key: The key code this sensor is looking for.
	@type key: keycode from L{GameKeys} module
	@ivar hold1: The key code for the first modifier this sensor is looking for.
	@type hold1: keycode from L{GameKeys} module
	@ivar hold2: The key code for the second modifier this sensor is looking for.
	@type hold2: keycode from L{GameKeys} module
	@ivar toggleProperty: The name of the property that indicates whether or not to log keystrokes as a string.
	@type toggleProperty: string
	@ivar targetProperty: The name of the property that receives keystrokes in case in case a string is logged.
	@type targetProperty: string
	@ivar useAllKeys: Flag to determine whether or not to accept all keys.
	@type useAllKeys: boolean
	"""
	def getEventList():
		"""
		Get a list of pressed keys that have either been pressed, or just released, or are active this frame.
		
		@rtype: list of key status. [[keycode, status]]
		@return: A list of keyboard events
		"""
	
	def getKeyStatus(keycode):
		"""
		Get the status of a key.
		
		@rtype: key state (KX_NO_INPUTSTATUS, KX_JUSTACTIVATED, KX_ACTIVE or KX_JUSTRELEASED)
		@return: The state of the given key
		@type keycode: integer
		@param keycode: The code that represents the key you want to get the state of
		"""
	
	#--The following methods are deprecated--
	def getKey():
		"""
		Returns the key code this sensor is looking for.
		
		Deprecated: Use the "key" property instead.
		
		@rtype: keycode from L{GameKeys} module
		"""
	
	def setKey(keycode):
		"""
		Set the key this sensor should listen for.
		
		Deprecated: Use the "key" property instead.
		
		@type keycode: keycode from L{GameKeys} module
		"""
	
	def getHold1():
		"""
		Returns the key code for the first modifier this sensor is looking for.
		
		Deprecated: Use the "hold1" property instead.
		
		@rtype: keycode from L{GameKeys} module
		"""
	
	def setHold1(keycode):
		"""
		Sets the key code for the first modifier this sensor should look for.
		
		Deprecated: Use the "hold1" property instead.
		
		@type keycode: keycode from L{GameKeys} module
		"""
	
	def getHold2():
		"""
		Returns the key code for the second modifier this sensor is looking for.
		
		Deprecated: Use the "hold2" property instead.
		
		@rtype: keycode from L{GameKeys} module
		"""
	
	def setHold2(keycode):
		"""
		Sets the key code for the second modifier this sensor should look for.
		
		Deprecated: Use the "hold2" property instead.
		
		@type keycode: keycode from L{GameKeys} module
		"""
	
	def getPressedKeys():
		"""
		Get a list of keys that have either been pressed, or just released this frame.
		
		Deprecated: Use getEventList() instead.
		
		@rtype: list of key status. [[keycode, status]]
		"""
	
	def getCurrentlyPressedKeys():
		"""
		Get a list of currently pressed keys that have either been pressed, or just released
		
		Deprecated: Use getEventList() instead.
		
		@rtype: list of key status. [[keycode, status]]
		"""