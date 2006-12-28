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
  mod = mods.append(Modifier.Type.SUBSURF) # add a new subsurf modifier
  mod[Modifier.Settings.LEVELS] = 3     # set subsurf subdivision levels to 3


Example::
	# Apply a lattice to an object and get the deformed object
	# Uses an object called 'Cube' and a lattice called 'Lattice'
	
	from Blender import *
	ob_mesh= Object.Get('Cube')
	ob_lattice= Object.Get('Lattice')

	myMeshMod = ob_mesh.modifiers
	mod = myMeshMod.append(Modifier.Type.LATTICE)
	mod[Modifier.Settings.OBJECT] = ob_lattice

	ob_mesh.makeDisplayList() # Needed to apply the modifier

	Window.RedrawAll() # View the change

	deformed_mesh= Mesh.New()
	deformed_mesh.getFromObject(ob_mesh.name)


	# Print the deformed locations
	for v in deformed_mesh.verts:
		print v.co



@type Type: readonly dictionary
@var Type: Constant Modifier dict used for  L{ModSeq.append()} to a
  modifier sequence and comparing with L{Modifier.type}:
    - ARMATURE - type value for Armature modifiers
    - BOOLEAN - type value for Boolean modifiers
    - BUILD - type value for Build modifiers
    - CURVE - type value for Curve modifiers
    - DECIMATE - type value for Decimate modifiers
    - LATTICE - type value for Lattice modifiers
    - SUBSURF - type value for Subsurf modifiers
    - WAVE - type value for Wave modifiers

@type Settings: readonly dictionary
@var Settings: Constant Modifier dict used for changing modifier settings.
	- RENDER - Used for all modifiers (bool)
	- REALTIME - Used for all modifiers (bool)
	- EDITMODE - Used for all modifiers (bool)
	- ONCAGE - Used for all modifiers (bool)

	- OBJECT - Used for Armature, Lattice, Curve, Boolean and Array (Object)
	- VERTGROUP - Used for Armature, Lattice and Curve (String)
	- LIMIT - Array and Mirror (float [0.0 - 1.0])
	- FLAG - Mirror and Wave (int)
	- COUNT - Decimator Polycount (readonly) and Array (int)
	- LENGTH - Build [1.0-300000.0] and Array [0.0 - 10000.0] (float)
	
	- TYPES - Used for Subsurf only
	- LEVELS - Used for Subsurf only (int [0 - 6])
	- RENDLEVELS - Used for Subsurf only (int [0 - 6])
	- OPTIMAL - Used for Subsurf only (bool)
	- UV - Used for Subsurf only (bool)

	- OBJECT_OFFSET - Used for Array only (Object)
	- OBJECT_CURVE - Used for Array only (Curve Object)
	- OFFSET_VEC - Used for Array only (3d Vector)
	- SCALE_VEC - Used for Array only (3d Vector)
	- MERGE_DIST - Used for Array only (float)

	- ENVELOPES - Used for Armature only (bool)
	
	- START - Used for Build only (int)
	- SEED - Used for Build only (int)
	- RANDOMIZE - Used for Build only (bool)

	- AXIS - Used for Mirror only (int [0 - 2])

	- RATIO - Used for Decimate only (float [0.0 - 1.0])
	
	- STARTX - Used for Wave only (float [-100.0 - 100.0])
	- STARTY - Used for Wave only (float [-100.0 - 100.0])
	- HEIGHT - Used for Wave only (float [-2.0 - 2.0])
	- WIDTH - Used for Wave only (float [0.0 - 5.0])
	- NARROW - Used for Wave only (float [0.0 - 10.0])
	- SPEED - Used for Wave only (float [-2.0 - 2.0])
	- DAMP - Used for Wave only (float [-1000.0 - 1000.0])
	- LIFETIME - Used for Wave only (float [-1000.0 - 1000.0])
	- TIMEOFFS - Used for Wave only (float [-1000.0 - 1000.0])
	
	- OPERATION - Used for boolean only (int 0,1,2 : Intersect, Union, Difference)
"""

class ModSeq:
  """
  The ModSeq object
  =================
  This object provides access to list of L{modifiers<Modifier.Modifier>} for a particular object.
  Only accessed from L{Object.Object.modifiers}.
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
    @type type: a constant specifying the type of modifier to create. as from L{Type}
    @rtype: Modifier
    @return: the new Modifier
    """

  def remove(modifier):
    """
    Remove a modifier from this objects modifier sequence.
    @type modifier: a modifier from this sequence to remove.
    @note: Accessing attributes of the modifier after removing will raise an error.
    """

  def moveUp(modifier):
    """
    Moves the modifier up in the object's modifier stack.
    @type modifier: a modifier from this sequence to remove.
    @rtype: None
    @raise RuntimeError: request to move above another modifier requiring
    original data
    @note: Accessing attributes of the modifier after removing will raise an error.
    """

  def moveDown(modifier):
    """
    Moves the modifier down in the object's modifier stack.
    @type modifier: a modifier from this sequence to remove.
    @rtype: None
    @raise RuntimeError: request to move modifier beyond a non-deforming
    modifier
    @note: Accessing attributes of the modifier after removing will raise an error.
    """

class Modifier:
  """
  The Modifier object
  ===================
  This object provides access to a modifier for a particular object accessed
  from L{ModSeq}.
  @ivar name: The name of this modifier. 31 chars max.
  @type name: string
  @ivar type: The type of this modifier. Read-only.  The returned value
  matches the types in L{Type}.
  @type type: int
  """  

  def __getitem__(key):
    """
    This operator returns one of the modifier's data attributes.
    @type key: value from modifier's L{Modifier.Settings} constant
    @return: the requested data
    @rtype: varies
    @raise KeyError: the key does not exist for the modifier
    """

  def __setitem__(key):
    """
    This operator modifiers one of the modifier's data attributes.
    @type key: value from modifier's L{Modifier.Settings} constant
    @raise KeyError: the key does not exist for the modifier
    """

