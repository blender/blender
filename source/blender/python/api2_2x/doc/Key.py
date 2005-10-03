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
    @cvar blocks: A list of KeyBlocks.
    @cvar ipo: The L{Ipo.Ipo} object associated with this key.
    @cvar type: An integer from the L{Types} dictionary
    representing the Key type.
    """

    def getType():
        """
        Get the type of this Key object. It will be one of the
        integers defined in the L{Types} dictionary.
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

class KeyBlock:
    """
    The KeyBlock object
    ===================
    Each Key object has a list of KeyBlocks attached, each KeyBlock
    representing a keyframe.

    @cvar data: The data of the KeyBlock (see L{getData}). This
    attribute is read-only.
    @cvar pos: The position of the keyframe (see L{getPos}). This
    attribute is read-only.
    @cvar name: The name of the KeyBlock. This attribute is read-only.
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

    def getPos():
        """
        Get the position of the keyframe represented by this KeyBlock,
        normally between 0.0 and 1.0. The time point when the Speed
        Ipo intersects the KeyBlock position is the actual time of the
        keyframe.
        """

    def getName():
        """Get the name of the keyframe represented by this KeyBlock."""
