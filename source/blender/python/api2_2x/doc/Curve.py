# Blender.Curve module and the Curve PyType object

"""
The Blender.Curve submodule.

Curve Data
==========

This module provides access to B{Curve Data} objects in Blender.

Example::
  from Blender import Curve, Object, Scene
  c = Curve.New()             # create new  curve data
  cur = Scene.getCurrent()    # get current scene
  ob = Object.New('Curve')    # make curve object
  ob.link(c)                  # link curve data with this object
  cur.link(ob)                # link object into scene
"""

def New ( name = 'CurData'):
    """
  Create a new Curve Data object.
  @type name: string
  @param name: The Curve Data name.
  @rtype: Blender Curve
  @return: The created Curve Data object.
  """

def Get (name = None):
  """
  Get the Curve Data object(s) from Blender.
  @type name: string
  @param name: The name of the Curve Data.
  @rtype: Blender Curve or a list of Blender Curves
  @return: It depends on the 'name' parameter:
        - (name): The Curve Data object with the given name;
        - ():     A list with all Curve Data objects in the current scene.
  """

class Curve:
  """
  The Curve Data object
  =====================
  This object gives access to Curve-specific data in Blender.
  @cvar name: The Curve Data name.
  @cvar pathlen: The Curve Data path length.
  @cvar totcol: The Curve Data maximal number of linked materials.
  @cvar flag: The Curve Data flag value; see function getFlag for the semantics.
  @cvar bevresol: The Curve Data bevel resolution.
  @cvar resolu: The Curve Data U-resolution.
  @cvar resolv: The Curve Data V-resolution.
  @cvar width: The Curve Data width.
  @cvar ext1: The Curve Data extent 1(for bevels).
  @cvar ext2: The Curve Data extent2 (for bevels).
  @cvar loc: The Curve Data location(from the center).
  @cvar rot: The Curve Data rotation(from the center).
  @cvar size: The Curve Data size(from the center).
  """

  def getName():
    """
    Get the name of this Curve Data object.
    @rtype: string
    """

  def setName(name):
    """
    Set the name of this Curve Data object.
    @rtype: PyNone
    @type name: string
    @param name: The new name.
    """

  def getPathlen():
    """
    Get this Curve's path length.
    @rtype: int
    @return: the path length.
    """

  def setPathlen(len):
    """
    Set this Curve's path length.
    @rtype: PyNone
    @type len: int
    @param len: the new curve's length.
    """

  def getTotcol():
    """
    Get the number of materials linked to the Curve.
    @rtype: int
    @return: number of materials linked.
    """

  def setTotcol(totcol):
    """
    Set the number of materials linked to the Curve.
    @rtype: PyNone
    @type totcol: int
    @param totcol: number of materials linked.
    """

  def getMode():
    """
    Get the Curve mode value.
    The mode of the curve is a combination of 4 parameters.
    Bits 0,1,2 : "Back", "Front" and "3D".
    Bit 3 :  "CurvePath" is set.
    Bit 4 :  "CurveFollow" is set.
    @rtype: int
    """

  def setMode(val):
    """
    Set the  Curve mode  value.
    @rtype: PyNone
    @type val: int
    @param val : The new Curve's mode value. 
    """

  def getBevresol():
    """
    Get the Curve's bevel resolution value.
    @rtype: float
    """

  def setBevresol(bevelresol):
    """
    Set the Curve's bevel resolution value.
    @rtype: PyNone
    @type bevelresol: float
    @param bevelresol: The new Curve's bevel resolution value.
    """

  def getResolu():
    """
    Get the Curve's U-resolution value.
    @rtype: float
    """

  def setResolu(resolu):
    """
    Set the Curve's U-resolution value.
    @rtype: PyNone
    @type resolu: float
    @param resolu: The new Curve's U-resolution value.
    """

  def getResolv():
    """
    Get the Curve's V-resolution value.
    @rtype: float
    """

  def setResolv(resolv):
    """
    Set the Curve's V-resolution value.
    @rtype: PyNone
    @type resolv: float
    @param resolv: The new Curve's V-resolution value.
    """

  def getWidth():
    """
    Get the Curve's width value.
    @rtype: float
    """

  def setWidth(width):
    """
    Set the Curve's width value. 
    @rtype: PyNone
    @type width: float
    @param width: The new Curve's width value. 
    """

  def getExt1():
    """
    Get the Curve's ext1 value.
    @rtype: float
    """

  def setExt1(ext1):
    """
    Set the Curve's ext1 value. 
    @rtype: PyNone
    @type ext1: float
    @param ext1: The new Curve's ext1 value. 
    """

  def getExt2():
    """
    Get the Curve's ext2 value.
    @rtype: float
    """

  def setExt2(ext2):
    """
    Set the Curve's ext2 value.
    @rtype: PyNone 
    @type ext2: float
    @param ext2: The new Curve's ext2 value. 
    """

  def getControlpoint(numcurve,numpoint):
    """
    Get the curve's control point value. 
    @type numcurve: int
    @type numpoint: int
    @rtype: list
    @return: depends upon the curve's type.\n
    type bezier : a list of three coordinates\n
    type nurbs : a list of nine coordinates.
    """

  def setControlpoint(controlpoint):
    """
    Set the Curve's controlpoint value. 
    @rtype: PyNone
    @type controlpoint: list
    @param controlpoint: The new Curve's controlpoint value.\n
    see getControlpoint for the length of the list.
    """

  def getLoc():
    """
    Get the curve's location value.
    @rtype: a list of 3 floats.
    """

  def setLoc(location):
    """
    Set the curve's location value.
    @rtype: PyNone 
    @type location: list[3]
    @param location: The new Curve's location values. 
    """

  def getRot():
    """
    Get the curve's rotation value.
    @rtype: a list of 3 floats.
    """

  def setRot(rotation):
    """
    Set the Curve's rotation value. 
    @rtype: PyNone
    @type rotation: list[3]
    @param rotation: The new Curve's rotation values. 
    """

  def getSize():
    """
    Get the curve's size value.
    @rtype: a list of 3 floats.
    """

  def setSize(size):
    """
    Set the curve size value.
    @rtype: PyNone 
    @type size: list[3]
    @param size: The new Curve's size values. 
    """
