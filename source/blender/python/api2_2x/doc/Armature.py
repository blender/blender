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
    print "- The root bones of %s: %s" % (a.name, a.getBones())
"""

def New (name = 'ArmatureData'):
  """
  Create a new Armature object.
  @type name: string
  @param name: The Armature name.
  @rtype: Blender Armature
  @return: The created Armature object.
  """

def Get (name = None):
  """
  Get the Armature object(s) from Blender.
  @type name: string
  @param name: The name of the Armature.
  @rtype: Blender Armature or a list of Blender Armatures
  @return: It depends on the I{name} parameter:
      - (name): The Armature object with the given I{name};
      - ():     A list with all Armature objects in the current scene.
  """

class Armature:
  """
  The Armature object
  ===================
    This object gives access to Armature-specific data in Blender.
  @cvar name: The Armature name.
  @cvar bones: The Armature root bones (cannot be set yet).
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
    Get the Armature root bones.
    @rtype: list of Blender Bones
    @return: a list of Armature bones.
    """

  def setBones(bones):
    """
    Set the Armature root bones (still unimplemented).
    @warn: This method wasn't implemented yet.
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
    @rtype: list of four floats.
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

  def setQuat(real,imag_i,imag_j,imag_k):
    """
    Set the new quaternion orientation for this Bone.
    @type real: float
    @type imag_i: float
    @type imag_j: float
    @type imag_k: float
    @param real: The new quat[0] value.
    @param imag_i: The new quat[1] value.
    @param imag_j: The new quat[2] value.
    @param imag_k: The new quat[3] value.
    """
