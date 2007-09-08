# Blender.Key module and the Key and KeyBlock PyType objects

"""
The Blender.Key submodule.

This module provides access to B{Key} objects in Blender.

@type Types: readonly dictionary
@var Types: The type of a key owner, indicating the type of data in the
data blocks.
	- MESH - the key is a Mesh key; data blocks contain
	L{NMVert<NMesh.NMVert>} vertices.
	- CURVE - the key is a Curve key; data blocks contains either
	L{BezTriples<BezTriple.BezTriple>} or points (represented by a list of
	3 floating point numbers).
	- LATTICE - the key is a Lattice key; data blocks contain
	BPoints, each point represented by a list of 3 floating point numbers.
"""

def Get(name = None):
	"""
	Get the named Key object from Blender. If the name is omitted, it
	will retrieve a list of all keys in Blender.
	@type name: string
	@param name: the name of the requested key
	@return: If name was given, return that Key object (or None if not
	found). If a name was not given, return a list of every Key object
	in Blender.
	"""

class Key:
	"""
	The Key object
	==============
	An object with keyframes (L{Lattice}, L{NMesh} or
	L{Curve}) will contain a Key object representing the
	keyframe data.
	
	@ivar ipo:  Key Ipo.  Contains the Ipo if one is assigned to the
	object, B{None} otherwise.  Setting to B{None} clears the current Ipo.
	@type ipo:  Blender Ipo
	@ivar value: The value of the key. Read-only.
	@type value: float
	@ivar type: An integer from the L{Types} dictionary
	representing the Key type.  Read-only.
	@type type: int
	@ivar blocks: A list of KeyBlocks for the key.  Read-only.
	@type blocks: Blender KeyBlock.
	@ivar relative: Indicates whether the key is relative(=True) or normal.
	@type relative: bool
	"""

	def getIpo():
		"""
		Get the L{Ipo} object associated with this key.
		"""
	def getBlocks():
		"""
		Get a list of L{KeyBlock}s, containing the keyframes defined for
		this Key.
		"""

class KeyBlock:
	"""	
	The KeyBlock object
	===================
	Each Key object has a list of KeyBlocks attached, each KeyBlock
	representing a keyframe.

	@ivar curval: Current value of the corresponding IpoCurve.  Read-only.
	@type curval: float
	@ivar name: The name of the Keyblock.  Truncated to 32 characters.
	@type name: string
	@ivar pos: The position of the keyframe.
	@type pos: float
	@ivar slidermin: The minimum value for the action slider.
	Value is clamped to the range [-10.0,10.0].
	@type slidermin: float
	@ivar slidermax: The maximum value for the action slider.
	Value is clamped to the range [-10.0,10.0].
	@type slidermax: float
	@ivar vgroup: The assigned VGroup for the Key Block.
	@type vgroup: string
	@ivar data: The data of the KeyBlock (see L{getData}). This
	attribute is read-only.
	@type data: varies
	"""

	def getData():
		"""
		Get the data of a KeyBlock, as a list of data items. Each item
		will have a different data format depending on the type of this
		Key.
		
		Note that prior to 2.45 the behaviour of this function
		was different (and very wrong).  Old scripts might need to be
		updated.
		
			- Mesh keys have a list of L{Vectors<Mathutils.Vector>} objects in the data
			block.
			- Lattice keys have a list of L{Vectors<Mathutils.Vector>} objects in the data
			block.
			- Curve keys return either a list of tuples, eacn containing
			 four L{Vectors<Mathutils.Vector>} (if the curve is a Bezier curve),
			 or otherwise just a list of L{Vectors<Mathutils.Vector>}.
			 
			 For bezier keys, the first three vectors in the tuple are the Bezier
			 triple vectors, while the fourth vector's first element is the curve tilt
			 (the other two elements are reserved and are currently unused).
			 
			 For non-Bezier keys, the first three elements of the returned vector is
			 the curve handle point, while the fourth element is the tilt.
			 
		
		A word on relative shape keys; relative shape keys are not actually
		stored as offsets to the base shape key (like you'd expect).  Instead, 
		the additive relative offset is calculated on the fly by comparing a 
		shape key with its base key, which is always the very first shapekey 
		in the keyblock list.
		"""

