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
    @rtype: PyIpo
    @return: the Ipo for the channel
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
    @rtype: Dictionary [channel : PyIpo]
    @return: the Ipos for all the channels in the action
    """