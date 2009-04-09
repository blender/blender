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
	@ivar events: a list of pressed keys that have either been pressed, or just released, or are active this frame. (read only).

			- 'keycode' matches the values in L{GameKeys}.
			- 'status' uses...
				- L{GameLogic.KX_INPUT_NONE}
				- L{GameLogic.KX_INPUT_JUST_ACTIVATED}
				- L{GameLogic.KX_INPUT_ACTIVE}
				- L{GameLogic.KX_INPUT_JUST_RELEASED}
			
	@type events: list [[keycode, status], ...]
	"""
	def getEventList():
		"""
		Get a list of pressed keys that have either been pressed, or just released, or are active this frame.
		
		B{DEPRECATED: Use the "events" property instead}.
		
		@rtype: list of key status. [[keycode, status]]
		@return: A list of keyboard events
		"""
	
	def getKeyStatus(keycode):
		"""
		Get the status of a key.
		
		@rtype: key state L{GameLogic} members (KX_INPUT_NONE, KX_INPUT_JUST_ACTIVATED, KX_INPUT_ACTIVE, KX_INPUT_JUST_RELEASED)
		@return: The state of the given key
		@type keycode: integer
		@param keycode: The code that represents the key you want to get the state of
		"""
	
	#--The following methods are DEPRECATED--
	def getKey():
		"""
		Returns the key code this sensor is looking for.
		
		B{DEPRECATED: Use the "key" property instead}.
		
		@rtype: keycode from L{GameKeys} module
		"""
	
	def setKey(keycode):
		"""
		Set the key this sensor should listen for.
		
		B{DEPRECATED: Use the "key" property instead}.
		
		@type keycode: keycode from L{GameKeys} module
		"""
	
	def getHold1():
		"""
		Returns the key code for the first modifier this sensor is looking for.
		
		B{DEPRECATED: Use the "hold1" property instead}.
		
		@rtype: keycode from L{GameKeys} module
		"""
	
	def setHold1(keycode):
		"""
		Sets the key code for the first modifier this sensor should look for.
		
		B{DEPRECATED: Use the "hold1" property instead}.
		
		@type keycode: keycode from L{GameKeys} module
		"""
	
	def getHold2():
		"""
		Returns the key code for the second modifier this sensor is looking for.
		
		B{DEPRECATED: Use the "hold2" property instead}.
		
		@rtype: keycode from L{GameKeys} module
		"""
	
	def setHold2(keycode):
		"""
		Sets the key code for the second modifier this sensor should look for.
		
		B{DEPRECATED: Use the "hold2" property instead.}
		
		@type keycode: keycode from L{GameKeys} module
		"""
	
	def getPressedKeys():
		"""
		Get a list of keys that have either been pressed, or just released this frame.
		
		B{DEPRECATED: Use "events" instead.}
		
		@rtype: list of key status. [[keycode, status]]
		"""
	
	def getCurrentlyPressedKeys():
		"""
		Get a list of currently pressed keys that have either been pressed, or just released
		
		B{DEPRECATED: Use "events" instead.}
		
		@rtype: list of key status. [[keycode, status]]
		"""