<<<<<<< .working
<<<<<<< .working
# $Id: SCA_JoystickSensor.py 19805 2009-04-20 00:19:16Z genscher $
=======
# $Id: SCA_RandomSensor.py 15444 2008-07-05 17:05:05Z lukep $
=======
# $Id$
>>>>>>> .merge-right.r19825
>>>>>>> .merge-right.r19804
# Documentation for SCA_RandomSensor
from SCA_ISensor import *

class SCA_JoystickSensor(SCA_ISensor):
	"""
	This sensor detects player joystick events.
	
	Properties:
	
	@ivar axisValues: (read-only) The state of the joysticks axis as a list of values L{numAxis} long.
						each spesifying the value of an axis between -32767 and 32767 depending on how far the axis is pushed, 0 for nothing. 
						The first 2 values are used by most joysticks and gamepads for directional control. 3rd and 4th values are only on some joysticks and can be used for arbitary controls.
						left:[-32767, 0, ...], right:[32767, 0, ...], up:[0, -32767, ...], down:[0, 32767, ...]
	@type axisValues: list of ints
	
	@ivar axisSingle: (read-only) like L{axisValues} but returns a single axis value that is set by the sensor.
						Only use this for "Single Axis" type sensors otherwise it will raise an error.
	@type axisSingle: int
	
	@ivar numAxis: (read-only) The number of axes for the joystick at this index.
	@type numAxis: integer
	@ivar numButtons: (read-only) The number of buttons for the joystick at this index.
	@type numButtons: integer
	@ivar numHats: (read-only) The number of hats for the joystick at this index.
	@type numHats: integer
	@ivar connected: (read-only) True if a joystick is connected at this joysticks index.
	@type connected: boolean
	@ivar index: The joystick index to use (from 0 to 7). The first joystick is always 0.
	@type index: integer
	@ivar threshold: Axis threshold. Joystick axis motion below this threshold wont trigger an event. Use values between (0 and 32767), lower values are more sensitive.
	@type threshold: integer
	@ivar button: The button index the sensor reacts to (first button = 0). When the "All Events" toggle is set, this option has no effect.
	@type button: integer
	@ivar axis: The axis this sensor reacts to, as a list of two values [axisIndex, axisDirection]
	            axisIndex: the axis index to use when detecting axis movement, 1=primary directional control, 2=secondary directional control.
	            axisDirection: 0=right, 1=up, 2=left, 3=down
	@type axis: [integer, integer]
	@ivar hat: The hat the sensor reacts to, as a list of two values: [hatIndex, hatDirection]
	            hatIndex: the hat index to use when detecting hat movement, 1=primary hat, 2=secondary hat.
	            hatDirection: 0-11
	@type hat: [integer, integer]
	"""
	
	def getButtonActiveList():
		"""
		Returns a list containing the indicies of the currently pressed buttons.
		@rtype: list
		"""
	def getButtonStatus(buttonIndex):
		"""
		Returns a bool of the current pressed state of the specified button.
		@param buttonIndex: the button index, 0=first button
		@type buttonIndex: integer
		@rtype: bool
		"""
	def getIndex():
		"""
		DEPRECATED: use the 'index' property.
		Returns the joystick index to use (from 1 to 8).
		@rtype: integer
		"""
	def setIndex(index):
		"""
		DEPRECATED: use the 'index' property.
		Sets the joystick index to use. 
		@param index: The index of this joystick sensor, Clamped between 1 and 8.
		@type index: integer
		@note: This is only useful when you have more then 1 joystick connected to your computer - multiplayer games.
		"""
	def getAxis():
		"""
		DEPRECATED: use the 'axis' property.
		Returns the current axis this sensor reacts to. See L{getAxisValue()<SCA_JoystickSensor.getAxisValue>} for the current axis state.
		@rtype: list
		@return: 2 values returned are [axisIndex, axisDirection] - see L{setAxis()<SCA_JoystickSensor.setAxis>} for their purpose.
		@note: When the "All Events" toggle is set, this option has no effect.
		"""
	def setAxis(axisIndex, axisDirection):
		"""
		DEPRECATED: use the 'axis' property.
		@param axisIndex: Set the axis index to use when detecting axis movement.
		@type axisIndex: integer from 1 to 2
		@param axisDirection: Set the axis direction used for detecting motion. 0:right, 1:up, 2:left, 3:down.
		@type axisDirection: integer from 0 to 3
		@note: When the "All Events" toggle is set, this option has no effect.
		"""
	def getAxisValue():
		"""
		DEPRECATED: use the 'axisPosition' property.
		Returns the state of the joysticks axis. See differs to L{getAxis()<SCA_JoystickSensor.getAxis>} returning the current state of the joystick.
		@rtype: list
		@return: 4 values, each spesifying the value of an axis between -32767 and 32767 depending on how far the axis is pushed, 0 for nothing. 

			The first 2 values are used by most joysticks and gamepads for directional control. 3rd and 4th values are only on some joysticks and can be used for arbitary controls.

			left:[-32767, 0, ...], right:[32767, 0, ...], up:[0, -32767, ...], down:[0, 32767, ...]
		@note: Some gamepads only set the axis on and off like a button.
		"""
	def getThreshold():
		"""
		DEPRECATED: use the 'threshold' property.
		Get the axis threshold. See L{setThreshold()<SCA_JoystickSensor.setThreshold>} for details.
		@rtype: integer
		"""
	def setThreshold(threshold):
		"""
		DEPRECATED: use the 'threshold' property.
		Set the axis threshold.
		@param threshold: Joystick axis motion below this threshold wont trigger an event. Use values between (0 and 32767), lower values are more sensitive.
		@type threshold: integer
		"""
	def getButton():
		"""
		DEPRECATED: use the 'button' property.
		Returns the button index the sensor reacts to. See L{getButtonValue()<SCA_JoystickSensor.getButtonValue>} for a list of pressed buttons.
		@rtype: integer
		@note: When the "All Events" toggle is set, this option has no effect.
		"""
	def setButton(index):
		"""
		DEPRECATED: use the 'button' property.
		Sets the button index the sensor reacts to when the "All Events" option is not set.
		@note: When the "All Events" toggle is set, this option has no effect.
		"""
	def getButtonValue():
		"""
		DEPRECATED: use the 'getButtonActiveList' method.
		Returns a list containing the indicies of the currently pressed buttons.
		@rtype: list
		"""
	def getHat():
		"""
		DEPRECATED: use the 'hat' property.
		Returns the current hat direction this sensor is set to.
		[hatNumber, hatDirection].
		@rtype: list
		@note: When the "All Events" toggle is set, this option has no effect.
		"""
	def setHat(index,direction):
		"""
		DEPRECATED: use the 'hat' property.
		Sets the hat index the sensor reacts to when the "All Events" option is not set.
		@type index: integer
		"""
	def getNumAxes():
		"""
		DEPRECATED: use the 'numAxis' property.
		Returns the number of axes for the joystick at this index.
		@rtype: integer
		"""
	def getNumButtons():
		"""
		DEPRECATED: use the 'numButtons' property.
		Returns the number of buttons for the joystick at this index.
		@rtype: integer
		"""
	def getNumHats():
		"""
		DEPRECATED: use the 'numHats' property.
		Returns the number of hats for the joystick at this index.
		@rtype: integer
		"""
	def isConnected():
		"""
		DEPRECATED: use the 'connected' property.
		Returns True if a joystick is detected at this joysticks index.
		@rtype: bool
		"""
