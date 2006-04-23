# Blender.Modifier module and the Modifier PyType object

"""
The Blender.Modifier submodule

B{New}: 
  -  provides access to Blender's modifier stack

This module provides access to the Modifier Data in Blender.

Example::
  from Blender import *

  ob = Object.Get('Cube')        # retrieve an object
  mods = ob.modifiers            # get the object's modifiers
  for mod in mods:
    print mod,mod.name           # print each modifier and its name
  mod = mods.append(mod.SUBSURF) # add a new subsurf modifier
  mod[mod.keys().LEVELS] = 3     # set subsurf subdivision levels to 3
"""


class ModSeq:
  """
  The ModSeq object
  =================
  This object provides access to list of modifiers for a particular object.
  """

  def __getitem__(index):
    """
    This operator returns one of the object's modifiers.
    @type index: int
    @return: an Modifier object
    @rtype: Modifier
    @raise KeyError: index was out of range
    """

  def __len__():
    """
    Returns the number of modifiers in the object's modifier stack.
    @return: number of Modifiers
    @rtype: int
    """

  def append(type):
    """
    Appends a new modifier to the end of the object's modifier stack.
    @type type: a constant specifying the type of modifier to create
    @rtype: Modifier
    @return: the new Modifier
    """

class Modifier:
  """
  The Modifier object
  ===================
  This object provides access to a modifier for a particular object.
  @ivar name: The name of this modifier. 31 chars max.
  """  

  def __getitem__(key):
    """
    This operator returns one of the modifier's data attributes.
    @type key: value from modifier's L{key()} constant
    @return: the requested data
    @rtype: varies
    @raise KeyError: the key does not exist for the modifier
    """

  def __setitem__(key):
    """
    This operator modifiers one of the modifier's data attributes.
    @type key: value from modifier's L{key()} constant
    @raise KeyError: the key does not exist for the modifier
    """

  def up():
    """
    Moves the modifier up in the object's modifier stack.
    @rtype: PyNone
    @raise RuntimeError: request to move above another modifier requiring
    original data
    """

  def down():
    """
    Moves the modifier down in the object's modifier stack.
    @rtype: PyNone
    @raise RuntimeError: request to move modifier beyond a non-deforming
    modifier
    """

  def keys():
    """
    Get the sequence of keys for the modifier.
    For example, a subsurf modifier can be accessed by::
       from Blender import *

       ob = Object.Get('Cube')        # retrieve an object
       mod = ob.modifiers[0]          # get the object's modifiers
       mod[mod.keys().LEVELS] = 3     # set subsurf subdivision levels to 3

    The valid keys are:
      - Common keys (all modifiers contain these keys): RENDER, REALTIME,
        EDITMODE, ONCAGE
      - Armature keys: ENVELOPES, OBJECT, VERTGROUPS
      - Boolean keys: OBJECT, OPERATION 
      - Build keys: START, LENGTH, SEED, RANDOMIZE
      - Curve keys: OBJECT, VERTGROUP
      - Decimate keys: RATIO, FACE_COUNT
      - Lattice keys: OBJECT, VERTGROUP
      - Mirror keys: LIMIT, FLAG, AXIS
      - Subsurf keys: TYPE, LEVELS, RENDER_LEVELS, OPTIMAL, UV
      - Wave keys: START_X, START_Y, HEIGHT, WIDTH, NARROW, SPEED, DAMP,
        LIFETIME, TIME_OFFS, FLAG

    @rtype: PyConstant
    @return: the keys for the modifier
    @raise RuntimeError: request to move modifier beyond a non-deforming
    modifier
    """

