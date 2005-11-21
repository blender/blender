# Blender.Armature module and the Armature PyType object

"""
The Blender.Armature submodule.

Armature
========

This module provides access to B{Armature} objects in Blender.  These are
"skeletons", used to deform and animate other objects -- meshes, for
example.

Example::
  import Blender
  from Blender import Armature
  #
  armatures = Armature.Get()
  for a in armatures:
    print "Armature ", a
    for bone_name, bone in a.bones.items():
       print bone_name, bone.weight

"""

def Get (name = None):
  """
  Get the Armature object(s) from Blender.
  @type name: string, nothing, or list of strings
  @param name: The string name of an armature.
  @rtype: Blender Armature or a list of Blender Armatures
  @return: It depends on the I{name} parameter:
      - (name): The Armature object with the given I{name};
      - (name, name, ...): A list of Armature objects
      - ():     A list with all Armature objects in the current scene.
  """

class ArmatureType:
  """
  The ArmatureType object
  ===================
    This object gives access to Armature-specific data in Blender.
  @ivar name: The Armature name.
  @ivar bones: A Dictionary of Bones that make up this armature.
  """

  def __init__(name = myArmature):
    """
    Initializer for the ArmatureType TypeObject.
    @param name: The name for the new armature
    @type name: string
    Example::
        myNewArmature = Blender.Armature.ArmatureType('AR_1')
    """

  def getName():
    """
    Get the name of this Armature object.
    @rtype: string
    """

  def setName(name):
    """
    Set the name of this Armature object.
    @type name: string
    @param name: The new name.
    """

  def getBones():
    """
    Get all the Armature bones.
    @rtype: PyBonesDict
    @return: a list of PyBone objects that make up the armature.
    """
   
  def makeEditable():
    """
    Put the armature into EditMode for editing purposes.
    @warning: The armature should not be in manual editmode
    prior to calling this method.
    """

  def saveChanges():
    """
    Save all changes and update the armature.
    @preconditions: Must have called makeEditable() first.
    """

class BonesDict:
  """
  The BonesDict object
  ===============
    This object gives gives dictionary like access to the bones in an armature.
  """

  def items():
    """
    Retun the key, value pairs in this dictionary
    @rtype: string, BPy_bone
    @return: All strings, and py_bones in the armature (in that order)
    """

  def keys():
    """
    Retun the keys in this dictionary
    @rtype: string
    @return: All strings representing the bone names
    """

  def values():
    """
    Retun the values in this dictionary
    @rtype: BPy_bone
    @return: All BPy_bones in this dictionary
    """

class BoneType:
  """
  The BoneType object
  ===============
    This object gives access to Bone-specific data in Blender.
  @ivar name: The name of this Bone.
  @ivar roll: This Bone's roll value.
  @ivar head: This Bone's "head" ending position when in rest state.
  @ivar tail: This Bone's "tail" ending position when in rest state.
  @ivar matrix: This Bone's matrix.
  @ivar parent: The parent Bone.
  @ivar children: The children bones.
  @ivar weight: The bone's weight.
  @ivar options: Various bone options which can be:
      -CONNECTED: IK to parent
      -HINGE: No parent rotation or scaling
      -NO_DEFORM: The bone does not deform geometetry
      -MULTIPLY: Multiply vgroups by envelope
      -HIDDEN_EDIT: Hide bones in editmode
  @ivar subdivision: The number of bone subdivisions.
  @ivar deform_dist: The deform distance of the bone
  """