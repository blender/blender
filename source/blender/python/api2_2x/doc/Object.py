# Blender.Object module and the Object PyType object

"""
The Blender.Object submodule

This module provides access to the B{Object Data} in Blender.

Example::

  import Blender
  scene = Blencer.Scene.getCurrent ()   # get the current scene
  ob = Blender.Object.New ('Camera')    # make camera object
  cam = Blender.Camera.New ('ortho')    # make ortho camera data object
  ob.link (cam)                         # link camera data with the object
  scene.link (ob)                       # link the object into the scene
  ob.setLocation (0.0, -5.0, 1.0)       # position the object in the scene
"""

def New (type, name='type'):
  """
  Creates a new Object.
  @type type: string
  @param type: The Object type: 'Armature', 'Camera', 'Curve', 'Lamp', 'Mesh' or 'Empty'.
  @type name: string
  @param name: The name of the object. By default, the name will be the same as the object type.
  @rtype: Blender Object
  @return: The created Object.
  """

def Get (name = None):
  """
  Get the Object from Blender.
  @type name: string
  @param name: The name of the requested Object.
  @rtype: Blender Object or a list of Blender Objects
  @return: It depends on the 'name' parameter:
      - (name): The Object with the given name;
      - ():     A list with all Objects in the current scene.
  """

def GetSelected ():
  """
  Get the selected objects from Blender.
  @rtype: A list of Blender Objects.
  @return: A list of all selected Objects in the current scene.
  """

class Object:
  """
  The Object object
  =================
    This object gives access to generic data from all objects in Blender.
    @cvar LocX: The X location coordinate of the object.
    @cvar LocY: The Y location coordinate of the object.
    @cvar LocZ: The Z location coordinate of the object.
    @cvar loc: The (X,Y,Z) location coordinates of the object (vector).
    @cvar dLocX: The delta X location coordinate of the object.
    @cvar dLocY: The delta Y location coordinate of the object.
    @cvar dLocZ: The delta Z location coordinate of the object.
    @cvar dloc: The delta (X,Y,Z) location coordinates of the object (vector).
    @cvar RotX: The X rotation angle (in radians) of the object.
    @cvar RotY: The Y rotation angle (in radians) of the object.
    @cvar RotZ: The Z rotation angle (in radians) of the object.
    @cvar rot: The (X,Y,Z) rotation angles (in radians) of the object (vector).
    @cvar dRotX: The delta X rotation angle (in radians) of the object.
    @cvar dRotY: The delta Y rotation angle (in radians) of the object.
    @cvar dRotZ: The delta Z rotation angle (in radians) of the object.
    @cvar drot: The delta (X,Y,Z) rotation angles (in radians) of the object (vector).
    @cvar SizeX: The X size of the object.
    @cvar SizeY: The Y size of the object.
    @cvar SizeZ: The Z size of the object.
    @cvar size: The (X,Y,Z) size of the object (vector).
    @cvar dSizeX: The delta X size of the object.
    @cvar dSizeY: The delta Y size of the object.
    @cvar dSizeZ: The delta Z size of the object.
    @cvar dsize: The delta (X,Y,Z) size of the object.
    @cvar EffX: The X effector coordinate of the object. Only applies to IKA.
    @cvar EffY: The Y effector coordinate of the object. Only applies to IKA.
    @cvar EffZ: The Z effector coordinate of the object. Only applies to IKA.
    @cvar Layer: The object layer (as a bitmask).
    @cvar parent: The parent object of the object. (Read-only)
    @cvar track: The object tracking this object. (Read-only)
    @cvar data: The data of the object. (Read-only)
    @cvar ipo: The ipo data associated with the object. (Read-only)
    @cvar mat: The actual matrix of the object. (Read-only)
    @cvar matrix: The actual matrix of the object. (Read-only)
    @cvar colbits: The Material usage mask. A set bit #n means: the Material #n in the Object's material list is used. Otherwise, the Material #n of the Objects Data material list is displayed.
    @cvar drawType: The object's drawing type used. 1 - Bounding box, 2 - wire, 3 - Solid, 4- Shaded, 5 - Textured.
    @cvar drawMode: The object's drawing mode used. The value can be a sum of: 2 - axis, 4 - texspace, 8 - drawname, 16 - drawimage, 32 - drawwire.
    @cvar name: The name of the object.
  """

  def clrParent(mode = 0, fast = 0):
  """
  Clears parent object.
  @type mode: int
  @type fast: int
  @param mode: A mode flag. If mode flag is 2, then the object transform will be kept. Any other value, or no value at all will update the object transform.
  @param fast: If the value is 0, the scene hierarchy will not be updated. Any other value, or no value at all will update the scene hierarchy.
  """

  def getData():
  """
  """

  def getDeformData():
  """
  """

  def getDeltaLocation():
  """
  """

  def getDrawMode():
  """
  """

  def getDrawType():
  """
  """

  def getEuler():
  """
  """

  def getInverseMatrix():
  """
  """

  def getLocation():
  """
  """

  def getMaterials():
  """
  """

  def getMatrix():
  """
  """

  def getName():
  """
  """

  def getParent():
  """
  """

  def getTracked():
  """
  """

  def getType():
  """
  """

  def link(object):
  """
  """

  def makeParent(objects, noninverse = 0, fast = 0):
  """
  """

  def materialUsage(material_source = 'Data'):
  """
  """

  def setDeltaLocation(float, float, float):
  """
  """

  def setDrawMode(char):
  """
  """

  def setDrawType(char):
  """
  """

  def setEuler(float, float, float):
  """
  """

  def setLocation(float, float, float):
  """
  """

  def setMaterials(materials):
  """
  """

  def setName(String):
  """
  """

  def shareFrom(Object):
  """
  """
