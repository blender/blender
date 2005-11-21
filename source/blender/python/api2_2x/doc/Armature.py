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
  =======================
    This object gives access to Armature-specific data in Blender.
  @ivar name: The Armature name.
  @ivar bones: A Dictionary of Bones that make up this armature.
  @ivar vertexGroups: (bool) Whether vertex groups define deformation
  @ivar envelopes: (bool) Whether bone envelopes define deformation
  @ivar restPosition: (bool) Show rest position (no posing possible)
  @ivar delayDeform: (bool) Dont deform children when manipulating bones
  @ivar drawAxes: (bool) Draw bone axes
  @ivar drawNames: (bool) Draw bone names
  @ivar ghost: Draw ghosts around frame for current Action
  @ivar ghostStep: Number of frames between ghosts
  @ivar drawType: The drawing type that is used to display the armature
  Acceptable values are:
      - Armature.OCTAHEDRON: bones drawn as octahedrons
      - Armature.STICK: bones drawn as sticks
      - Armature.BBONE: bones drawn as b-bones
      - Armature.ENVELOPE: bones drawn as sticks with envelopes
  @ivar mirrorEdit: (bool) X-axis mirrored editing
  @ivar autoIK: (bool)  Adds temporary IK chains while grabbing bones
  """

  def __init__(name = 'myArmature'):
    """
    Initializer for the ArmatureType TypeObject.
    Example::
        myNewArmature = Blender.Armature.ArmatureType('AR_1')
    @param name: The name for the new armature
    @type name: string
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
    @note: Must have called makeEditable() first.
    """

class BonesDict:
  """
  The BonesDict object
  ====================
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
  ===================
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