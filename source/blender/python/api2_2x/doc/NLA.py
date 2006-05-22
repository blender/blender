# Blender.Armature.NLA module and the Action PyType object

"""
The Blender.Armature.NLA submodule.

NLA
===

This module provides access to B{Action} objects in Blender.  Actions are a series of keyframes/Ipo curves
that define the movement of a bone. Actions are linked to objects of type armature.

Example::
  import Blender
  from Blender import *
  from Blender.Armature import *
  from Blender.Armature.Bone import *

  armObj = Object.New('Armature', "Armature_obj")
  armData = Armature.New()

  bn1=Blender.Armature.Bone.New("bone1")
  bn1.setHead(0.0,0.0,0.0)
  bn1.setTail(2.0,1.0,0.0)

  bn2=Blender.Armature.Bone.New("bone2")
  bn2.setHead(3.0,2.0,1.0)
  bn2.setTail(4.0,4.0,1.0)
  bn2.setRoll(.5)
  bn2.setParent(bn1)

  armData.addBone(bn1)
  armData.addBone(bn2)

  armObj.link(armData)
  scn = Blender.Scene.getCurrent()
  scn.link(armObj)

  armObj.makeDisplayList()
  Blender.Window.RedrawAll()

  action = Blender.Armature.NLA.NewAction()
  action.setActive(armObj)
  
  bn2.setPose([ROT,LOC,SIZE])  
  
  context = scn.getRenderingContext()
  context.currentFrame(2)

  quat = Blender.Mathutils.Quaternion([1,2,3,4])
  bn2.setQuat(quat)
  bn2.setLoc([3,4,5])
  
  bn2.setPose([ROT,LOC,SIZE])

  print action.name
  action2 = Blender.Armature.NLA.CopyAction(action)
  action2.name = "Copy"

@type Flags: readonly dictionary
@var Flags: Constant dict used by the L{ActionStrip.flag} attribute.
It is a bitmask and settings are ORed together.
  - SELECT: action strip is selected in NLA window
  - STRIDE_PATH: play action nased on path position and stride.
  - HOLD: continue displaying the last frame past the end of the strip
  - ACTIVE: action strip is active in NLA window
  - LOCK_ACTION: action start/end are automatically mapped to strip duration

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


class ActionStrips:
  """
  The ActionStrips object
  =======================
  This object gives access to sequence of L{ActionStrip} objects for
  a particular Object.
  """

  def __getitem__(index):
    """
    This operator returns one of the constraints in the stack.
    @type index: int
    @return: an Constraint object
    @rtype: Constraint
    @raise KeyError: index was out of range
    """

  def __len__():
    """
    Returns the number of action strips for the object.
    @return: number of Constraints
    @rtype: int
    """

  def append(action):
    """
    Appends a new action to the end of the actionstrip sequence.
    @type action: L{Action<NLA.Action>}
    @param action: the action to use in the action strip
    @rtype: Constraint
    @return: the new Constraint
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
    @rtype: PyNone
    """

  def resetStripSize():
    """
    Activates the functionality found in NLA Strip menu under "Reset  Strip
    Size".  This method resets the Action Strip size to its creation values.
    @rtype: PyNone
    """

  def snapToFrame():
    """
    Activates the functionality found in NLA Strip menu under "Snap to Frame".
    This function snaps the ends of the action strip to the nearest whole
    numbered frame.
    @rtype: PyNone
    """
