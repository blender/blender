# Blender.Armature.NLA module and the Action PyType object

"""
The Blender.Armature.NLA submodule.

NLA
===

This module provides access to B{Action} objects in Blender.  Actions are a
series of keyframes/Ipo curves that define the movement of a bone. 
Actions are linked to objects of type armature.

@type Flags: readonly dictionary
@var Flags: Constant dict used by the L{ActionStrip.flag} attribute.
It is a bitmask and settings are ORed together.
	- SELECT: action strip is selected in NLA window
	- STRIDE_PATH: play action based on path position and stride.
	- HOLD: continue displaying the last frame past the end of the strip
	- ACTIVE: action strip is active in NLA window
	- LOCK_ACTION: action start/end are automatically mapped to strip duration
	- MUTE: action strip does not contribute to the NLA solution
	- USEX: Turn off automatic single-axis cycling and use X as an offset axis.  Note that you can use multiple axes at once.
	- USEY: Turn off automatic single-axis cycling and use Y as an offset axis.  Note that you can use multiple axes at once.
	- USEZ: Turn off automatic single-axis cycling and use Z as an offset axis.  Note that you can use multiple axes at once.
	- AUTO_BLEND: Automatic calculation of blend in/out values
	
@type StrideAxes: readonly dictionary
@var StrideAxes: Constant dict used by the L{ActionStrip.strideAxis} attribute.
Values are STRIDEAXIS_X, STRIDEAXIS_Y, and STRIDEAXIS_Z.

@type Modes: readonly dictionary
@var Modes: Constant dict used by the L{ActionStrip.mode} attribute.
Currently the only value is MODE_ADD.
"""

def NewAction  (name = 'DefaultAction'):
	"""
	Create a new Action object.
	@type name: string
	@param name: The Action name.
	@rtype: PyAction
	"""
	
def CopyAction (action):
	"""
	Copy an action and it's keyframes
	@type action: PyAction
	@param action: The action to be copied.
	@rtype: PyAction
	@return: A copied action
	"""

def GetActions ():
	"""
	Get all actions and return them as a Key : Value Dictionary.
	@rtype: Dictionary of PyActions
	@return: All the actions in blender
	"""
	
class Action:
	"""
	The Action object
	=================
		This object gives access to Action-specific data in Blender.
	"""

	def getName():
		"""
		Get the name of this Action.
		@rtype: string
		"""
		
	def setName(name):
		"""
		Set the name of this Action.
		@type name: string
		@param name: The new name
		"""
		
	def setActive(object):
		"""
		Set this action as the current action for an object.
		@type object: PyObject 
		@param object: The object whose action is to be set
		"""
		
	def getChannelIpo(channel):
		"""
		Get the Ipo for the named channel in this action
		@type channel: string
		@param channel: The name of a channel in this action
		@rtype: PyIpo or None
		@return: the Ipo for the channel
		"""

	def getFrameNumbers():
		"""
		Gets the frame numbers at which a key was inserted into this action
		@rtype: PyList
		@return: a list of ints
		"""
		
	def removeChannel(channel):
		"""
		Remove a named channel from this action
		@type channel: string
		@param channel: The name of a channel in this action to be removed
		"""
		
	def getAllChannelIpos():
		"""
		Get the all the Ipos for this action
		@rtype: Dictionary [channel : PyIpo or None]
		@return: the Ipos for all the channels in the action
		"""

	def getChannelNames():
		"""
		Returns a list of channel names
		@rtype: list
		@return: the channel names that match bone and constraint names.
		"""

	def renameChannel(nameFrom, nameTo):
		"""
		rename an existing channel to a new name.
		
		if the nameFrom channel dosnt exist or the nameTo exists, an error will be raised.
		@return: None
		"""

import id_generics
Action.__doc__ += id_generics.attributes 


class ActionStrips:
	"""
	The ActionStrips object
	=======================
	This object gives access to sequence of L{ActionStrip} objects for
	a particular Object.
	"""

	def __getitem__(index):
		"""
		This operator returns one of the action strips in the stack.
		@type index: int
		@return: an action strip object
		@rtype: ActionStrip
		@raise KeyError: index was out of range
		"""

	def __len__():
		"""
		Returns the number of action strips for the object.
		@return: number of action strips
		@rtype: int
		"""

	def append(action):
		"""
		Appends a new action to the end of the action strip sequence.
		@type action: L{Action<NLA.Action>}
		@param action: the action to use in the action strip
		@rtype: ActionStrip
		@return: the new action strip
		"""

	def remove(actionstrip):
		"""
		Remove an action strip from this object's actionstrip sequence.
		@type actionstrip: an action strip from this sequence to remove.
		@note: Accessing attributes of the action strip after it is removed will 
		throw an exception.
		"""

	def moveDown(actionstrip):
		"""
		Move the action strip down in the object's actionstrip sequence.
		@type actionstrip: an action strip from this sequence.
		"""
	
	def moveUp(actionstrip):
		"""
		Move the action strip up in the object's actionstrip sequence.
		@type actionstrip: an action strip from this sequence.
		"""

class ActionStrip:
	"""
	The ActionStrip object
	======================
	This object gives access to a particular action strip.
	@ivar action: Action associated with the strip.
	@type action: BPy Action object
	@ivar stripStart: Starting frame of the strip.
	@type stripStart: float 
	@ivar stripEnd: Ending frame of the strip.
	@type stripEnd: float 
	@ivar actionStart: Starting frame of the action.
	@type actionStart: float 
	@ivar actionEnd: Ending frame of the action.
	@type actionEnd: float 
	@ivar repeat: The number of times to repeat the action range.
	@type repeat: float 
	@ivar mode: Controls the ActionStrip mode.  See L{Modes} for
	valid values.
	@type mode: int
	@ivar flag: Controls various ActionStrip attributes.  Values can be ORed.
	See L{Flags} for valid values.
	@type flag: int
	@ivar strideAxis: Dominant axis for stride bone. See L{StrideAxes} for
	valid values.
	@type strideAxis: int 
	@ivar strideLength: Distance covered by one complete cycle of the action
	specified in the Action Range.
	@type strideLength: float 
	@ivar strideBone: Name of Bone used for stride
	@type strideBone: string 
	@ivar groupTarget: Armature object within DupliGroup for local animation
	@type groupTarget: object
	@ivar blendIn: Number of frames of motion blending.
	@type blendIn: float 
	@ivar blendOut: Number of frames of ease-out.
	@type blendOut: float 
	"""

	def resetActionLimits():
		"""
		Activates the functionality found in NLA Strip menu under "Reset  Action
		Start/End". This method restores the values of ActionStart and
		ActionEnd to  their defaults, usually the first and last frames within
		an action  that contain keys.
		@rtype: None
		"""

	def resetStripSize():
		"""
		Activates the functionality found in NLA Strip menu under "Reset  Strip
		Size".  This method resets the action strip size to its creation values.
		@rtype: None
		"""

	def snapToFrame():
		"""
		Activates the functionality found in NLA Strip menu under "Snap to Frame".
		This function snaps the ends of the action strip to the nearest whole
		numbered frame.
		@rtype: None
		"""
