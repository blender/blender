# Blender.Armature.Bone module and the Bone PyType object

"""
The Blender.Armature.Bone submodule.

Bone
====

This module provides access to B{Bone} objects in Blender.  Bones are used to 
create armatures.

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

  bn3=Blender.Armature.Bone.New("bone3")
  bn3.setHead(5.0,6.0,2.0)
  bn3.setTail(5.0,8.0,2.0)
  bn3.setParent(bn2)

  armData.addBone(bn1)
  armData.addBone(bn2)
  armData.addBone(bn3)

  armData.drawAxes(1)
  armObj.link(armData)

  scn = Blender.Scene.getCurrent()
  scn.link(armObj)

  armObj.makeDisplayList()
  Blender.Window.RedrawAll()

  bn2.setPose([ROT,LOC,SIZE])  
  
@var PoseFlags: The available flags for setting keyframed poses.
    - ROT - add bone's rotation to keyframe
    - LOC - add bone's location to keyframe
    - SIZE- add bone's size to keyframe
    
@var BoneclassFlags: The available flags for setting boneclass.
    - SKINNABLE
    - UNSKINNABLE
    - HEAD
    - NECK
    - BACK
    - SHOULDER
    - ARM
    - HAND
    - FINGER
    - THUMB
    - PELVIS
    - LEG
    - FOOT
    - TOE
    - TENTACLE
"""

class Bone:
  """
  The Bone object
  ===============
    This object gives access to Bone-specific data in Blender.
  @cvar name: The name of this Bone.
  @cvar roll: This Bone's roll value.
  @cvar head: This Bone's "head" ending position when in rest state.
  @cvar tail: This Bone's "tail" ending position when in rest state.
  @cvar loc: This Bone's location.
  @cvar size: This Bone's size.
  @cvar quat: This Bone's quaternion.
  @cvar parent: The parent Bone.
  @cvar children: The children bones.
  @cvar weight: The bone's weight.
  @cvar ik: Whether the bone is set to IK.
  @cvar boneclass: The boneclass of this bone.
  """

  def getName():
    """
    Get the name of this Bone.
    @rtype: string
    """

  def getRoll():
    """
    Get the roll value.
    @rtype: float
    @warn: Roll values are local to parent's objectspace when
    bones are parented.
    """

  def getWeight():
    """
    Get the bone's weight..
    @rtype: float
    """
    
  def getHead():
    """
    Get the "head" ending position.
    @rtype: list of three floats
    """

  def getTail():
    """
    Get the "tail" ending position.
    @rtype: list of three floats
    """

  def getLoc():
    """
    Get the location of this Bone.
    @rtype: list of three floats
    """

  def getSize():
    """
    Get the size attribute.
    @rtype: list of three floats
    """

  def getQuat():
    """
    Get this Bone's quaternion.
    @rtype: Quaternion object.
    """

  def hasParent():
    """
    True if this Bone has a parent Bone.
    @rtype: true or false
    """

  def getParent():
    """
    Get this Bone's parent Bone, if available.
    @rtype: Blender Bone
    """

  def getWeight():
    """
    Get the bone's weight.
    @rtype: float
    """

  def getChildren():
    """
    Get this Bone's children Bones, if available.
    @rtype: list of Blender Bones
    """

  def setName(name):
    """
    Rename this Bone.
    @type name: string
    @param name: The new name.
    """

  def setRoll(roll):
    """
    Set the roll value.
    @type roll: float
    @param roll: The new value.
    @warn: Roll values are local to parent's objectspace when
    bones are parented.
    """

  def setHead(x,y,z):
    """
    Set the "head" ending position.
    @type x: float
    @type y: float
    @type z: float
    @param x: The new x value.
    @param y: The new y value.
    @param z: The new z value.
    """

  def setTail(x,y,z):
    """
    Set the "tail" ending position.
    @type x: float
    @type y: float
    @type z: float
    @param x: The new x value.
    @param y: The new y value.
    @param z: The new z value.
    """

  def setLoc(x,y,z):
    """
    Set the new location for this Bone.
    @type x: float
    @type y: float
    @type z: float
    @param x: The new x value.
    @param y: The new y value.
    @param z: The new z value.
    """

  def setSize(x,y,z):
    """
    Set the new size for this Bone.
    @type x: float
    @type y: float
    @type z: float
    @param x: The new x value.
    @param y: The new y value.
    @param z: The new z value.
    """

  def setQuat(quat):
    """
    Set the new quaternion for this Bone.
    @type quat: Quaternion object or PyList of floats
    @param quat: Can be a Quaternion or PyList of 4 floats.
    """

  def setParent(bone):
    """
    Set the bones's parent in the armature.
    @type bone: PyBone
    @param bone: The Python bone that is the parent to this bone.
    """

  def setWeight(weight):
    """
    Set the bones's weight.
    @type weight: float
    @param weight: set the the bone's weight.
    """
    
  def hide():
    """
    Hides this bone.
    """
    
  def unhide():
    """
    Unhide this bone.
    """
    
  def clearParent():
    """
    Attempts to clear the parenting of this bone. Because the bone is no longer parented
    it will be set in the armature as a root bone.
    """
    
  def clearChildren():
    """
    Attemps to remove all the children of this bone. Because each of the children no longer are
    parented they will be set a root bones in the armature.
    """
    
  def setPose(flags, Action):
    """
    Set the pose for this bone. The pose will be set at the Scene's current frame.
    If an action is passed as the optional second parameter the pose will be added as a keyframe
    to that action. Otherwise a default action will be created an the pose set to it.
    @type flags: PyList of enums
    @param flags: expects ROT, LOC, SIZE in a list. Whichever of these is passed the keyframe generated
    will set information about the rotation, and/or location, and/or size of the bone.
    @type Action: PyAction
    @param Action: a python action that has either been created or returned from an object
    """
    
  def setBoneclass(boneclass):
    """
    Set the bones's boneclass.
    @type boneclass: Enumeration constant. See above.
    @param boneclass: The boneclass of this bone.
    """

  def getBoneclass():
    """
    Get this Bone's boneclass.
    @rtype: Enumeration const (string)
    """

  def hasIK():
    """
    Determines whether or not this bone as flaged as IK.
    @rtype: true (1) or false (0)
    """

  def getRestMatrix(locale = 'worldspace'):
    """
    Return a matrix that represents the rotation and position 
    of this bone. There are two types of matrices that can be 
    returned - bonespace (in the coord. system of parent) or
    worldspace (in the coord system of blender). The rotation will
    be in either worldspace or bonespace. Translaction vectors (row 4)
    will be the bone's head position (if worldspace) or the difference
    from this bone's head to the parent head (if bonespace).
    @type locale: string Values are:
      - worldspace
      - bonespace
    @param locale: default is worldspace
    @rtype: 4x4 PyMatrix
    """