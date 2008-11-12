# $Id: SCA_RandomSensor.py 15444 2008-07-05 17:05:05Z lukep $
# Documentation for SCA_RandomSensor
from SCA_ISensor import *

class SCA_JoystickSensor(SCA_ISensor):
	"""
	This sensor detects player joystick events.
	"""
	
	def getIndex():
		"""
		Returns the joystick index to use (from 1 to 8).
		@rtype: integer
		"""
	def setIndex(index):
		"""
		Sets the joystick index to use. 
		@param index: The index of this joystick sensor, Clamped between 1 and 8.
		@type index: integer
		@note: This is only useful when you have more then 1 joystick connected to your computer - multiplayer games.
		"""
	def getAxis():
		"""
		Returns the current axis this sensor reacts to. See L{getAxisValue()<SCA_JoystickSensor.getAxisValue>} for the current axis state.
		@rtype: list
		@return: 2 values returned are [axisIndex, axisDirection] - see L{setAxis()<SCA_JoystickSensor.setAxis>} for their purpose.
		@note: When the "All Events" toggle is set, this option has no effect.
		"""
	def setAxis(axisIndex, axisDirection):
		"""
		@param axisIndex: Set the axis index to use when detecting axis movement.
		@type axisIndex: integer from 1 to 2
		@param axisDirection: Set the axis direction used for detecting motion. 0:right, 1:up, 2:left, 3:down.
		@type axisDirection: integer from 0 to 3
		@note: When the "All Events" toggle is set, this option has no effect.
		"""
	def getAxisValue():
		"""
		Returns the state of the joysticks axis. See differs to L{getAxis()<SCA_JoystickSensor.getAxis>} returning the current state of the joystick.
		@rtype: list
		@return: 4 values, each spesifying the value of an axis between -32767 and 32767 depending on how far the axis is pushed, 0 for nothing. 

			The first 2 values are used by most joysticks and gamepads for directional control. 3rd and 4th values are only on some joysticks and can be used for arbitary controls.

			left:[-32767, 0, ...], right:[32767, 0, ...], up:[0, -32767, ...], down:[0, 32767, ...]
		@note: Some gamepads only set the axis on and off like a button.
		"""
	def getThreshold():
		"""
		Get the axis threshold. See L{setThreshold()<SCA_JoystickSensor.setThreshold>} for details.
		@rtype: integer
		"""
	def setThreshold(threshold):
		"""
		Set the axis threshold.
		@param threshold: Joystick axis motion below this threshold wont trigger an event. Use values between (0 and 32767), lower values are more sensitive.
		@type threshold: integer
		"""
	def getButton():
		"""
		Returns the button index the sensor reacts to. See L{getButtonValue()<SCA_JoystickSensor.getButtonValue>} for a list of pressed buttons.
		@rtype: integer
		@note: When the "All Events" toggle is set, this option has no effect.
		"""
	def setButton(index):
		"""
		Sets the button index the sensor reacts to when the "All Events" option is not set.
		@note: When the "All Events" toggle is set, this option has no effect.
		"""
	def getButtonValue():
		"""
		Returns a list containing the indicies of the currently pressed buttons.
		@rtype: list
		"""
	def getHat():
		"""
		Returns the current hat direction this sensor is set to.
		[hatNumber, hatDirection].
		@rtype: list
		@note: When the "All Events" toggle is set, this option has no effect.
		"""
	def setHat(index):
		"""
		Sets the hat index the sensor reacts to when the "All Events" option is not set.
		@type index: integer
		"""
	def getNumAxes():
		"""
		Returns the number of axes for the joystick at this index.
		@rtype: integer
		"""
	def getNumButtons():
		"""
		Returns the number of buttons for the joystick at this index.
		@rtype: integer
		"""
	def getNumHats():
		"""
		Returns the number of hats for the joystick at this index.
		@rtype: integer
		"""
	def isConnected():
		"""
		Returns True if a joystick is detected at this joysticks index.
		@rtype: bool
		"""
