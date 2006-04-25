# Blender.Object.Pose module

"""
The Blender.Object.Pose submodule.

Pose
====

This module provides access to B{Pose} objects in Blender.  This Pose is the 
current object-level (as opposed to armature-data level) transformation.

Example::


@var ROT: 
@type ROT: Constant
@var LOC: 
@type LOC: Constant
@var SIZE: 
@type SIZE: Constant
"""

class Pose:
  """
  The Pose object
  ===============
    This object gives access to Pose-specific data in Blender.
  @ivar bones: A Dictionary of PosePoseBones (PoseDict) that make up this Pose.
  @type bones: PoseDict Object
  """

  def update():
    """
    Save all changes and update the Pose.
    @rtype: None
    """

class PoseBonesDict:
  """
  The PoseBonesDict object
  ========================
    This object gives dictionary like access to the PoseBones in a Pose. 
    It is internal to blender but is called as 'Pose.bones'
  """

  def items():
    """
    Return the key, value pairs in this dictionary
    @rtype: string, PosePoseBone
    @return: All strings, and PosePoseBones in the Pose (in that order)
    """

  def keys():
    """
    Return the keys in this dictionary
    @rtype: string
    @return: All strings representing the PosePoseBone names
    """

  def values():
    """
    Return the values in this dictionary
    @rtype: BPy_PoseBone
    @return: All PosePoseBones in this dictionary
    """

class PoseBone:
  """
  The PoseBone object
  ===================
    This object gives access to PoseBone-specific data in Blender. 
  @ivar name: The name of this PoseBone.
  @type name: String
  @ivar loc: The change in location for this PoseBone.
  @type loc: Vector object
  @ivar size: The change in size for this PoseBone (no change is 1,1,1)
  @type size: Vector object
  @ivar quat: The change in rotation for this PoseBone.
  @type quat: Quaternion object
  @ivar head: The final head location for this PoseBone. (not settable)
  @type head: Vector object
  @ivar tail: The final tail location for this PoseBone. (not settable)
  @type tail: Vector object
  @ivar localMatrix: The matrix combination of rot/quat/loc.
  @type localMatrix: Matrix object
  @ivar poseMatrix: The total transformation of this PoseBone including constraints. (not settable)
  @type poseMatrix: Matrix object
  """

  def insertKey(parentObject, frameNumber, type):
    """
    Insert a pose key for this PoseBone at a frame.
    @type parentObject: Object object
    @param parentObject: The object the pose came from.
    @type frameNumber: integer
    @param frameNumber: The frame number to insert the pose key on.
    @type type: Constant object
    @param type: Can be any combination of 3 Module constants:
       - Pose.LOC
       - Pose.ROT
       - Pose.QUAT
    @rtype: None
    """

