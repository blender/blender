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
    @rtype: list
    @return: a list of Armature bones.
    """

  def setBones(bones):
    """
    Set the Armature root bones (still unimplemented).
    @warn: This method wasn't implemented yet.
    """
