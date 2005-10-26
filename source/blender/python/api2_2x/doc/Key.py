# Blender.Key module and the Key and KeyBlock PyType objects

"""
The Blender.Key submodule.

This module provides access to B{Key} objects in Blender.

@type Types: readonly dictionary
@var Types: The type of a key, indicating the type of data in the
data blocks.
    - MESH - the key is a Mesh key; data blocks contain
    L{NMesh.NMVert} vertices.
    - CURVE - the key is a Curve key; data blocks contain
    L{Ipo.BezTriple} points.
    - LATTICE - the key is a Lattice key; data blocks contain
    BPoints, each point represented as a list of 4 floating point numbers.

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
    An object with keyframes (L{Lattice.Lattice}, L{NMesh.NMesh} or
    L{Curve.Curve}) will contain a Key object representing the
    keyframe data.
    
    @ivar value: The Value of the Key - Read Only
    @type value: float
    @ivar type: An integer from the L{Types} dictionary
    representing the Key type.
    @type type: int

    @cvar blocks: A list of KeyBlocks.
    @cvar ipo: The L{Ipo.Ipo} object associated with this key.
    """

    def getIpo():
        """
        Get the L{Ipo.Ipo} object associated with this key.
        """
    def getBlocks():
        """
        Get a list of L{KeyBlock}s, containing the keyframes defined for
        this Key.
        """
	def setDriverChannel(index):
		""" 
		Get the IpoCurve object associated with the shape key referenced
		by the index of that key in getBlocks
  		@type index: int
  		@param index: the keyblock index to retrieve an IPO for.
  		@rtype: Blender IpoCurve
  		@return: Ipo Data Object:
		"""
		
class KeyBlock:
  """	
  The KeyBlock object
  ===================
  Each Key object has a list of KeyBlocks attached, each KeyBlock
  representing a keyframe.

  @ivar name: The Name of the Keyblock
  Truncated to 32 Characters
  @type name: string
  @ivar pos: The position of the keyframe
  @type pos: float
  @ivar slidermin: The minimum value for the action slider 
  @type slidermin: float
  @ivar slidermax: The maximum value for the action slider
  @type slidermax: float
  @ivar vgroup: The assigned VGroup for the Key Block
  @type vgroup: string
  
  @cvar data: The data of the KeyBlock (see L{getData}). This
  attribute is read-only.
  """
  def getData():
    """
    Get the data of a KeyBlock, as a list of data items. Each item
    will have a different data type depending on the type of this
    Key.
    Mesh keys have a list of L{NMesh.NMVert} objects in the data
    block.

    Lattice keys have a list of BPoints in the data block. These
    don't have corresponding Python objects yet, so each BPoint is
    represented using a list of four floating-point numbers.

    Curve keys have a list of L{Ipo.BezTriple} objects in the data
    block.
    """

