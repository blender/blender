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
  @param type: The Object type: 'Armature', 'Camera', 'Curve', 'Lamp', 'Mesh'
      or 'Empty'.
  @type name: string
  @param name: The name of the object. By default, the name will be the same
      as the object type.
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
    @cvar drot: The delta (X,Y,Z) rotation angles (in radians) of the object
        (vector).
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
    @cvar colbits: The Material usage mask. A set bit #n means: the Material
        #n in the Object's material list is used. Otherwise, the Material #n
        of the Objects Data material list is displayed.
    @cvar drawType: The object's drawing type used. 1 - Bounding box,
        2 - wire, 3 - Solid, 4- Shaded, 5 - Textured.
    @cvar drawMode: The object's drawing mode used. The value can be a sum
        of: 2 - axis, 4 - texspace, 8 - drawname, 16 - drawimage,
        32 - drawwire.
    @cvar name: The name of the object.
  """

  def buildParts():
    """
    Recomputes the particle system. This method only applies to an Object of
    the type Effect.
    """

  def clrParent(mode = 0, fast = 0):
    """
    Clears parent object.
    @type mode: Integer
    @type fast: Integer
    @param mode: A mode flag. If mode flag is 2, then the object transform will
        be kept. Any other value, or no value at all will update the object
        transform.
    @param fast: If the value is 0, the scene hierarchy will not be updated. Any
        other value, or no value at all will update the scene hierarchy.
    """

  def getData():
    """
    Returns the Datablock object containing the object's data. For example the
    Mesh, Lamp or the Camera.
    @rtype: Object type specific
    @return: Depending on the type of the Object, it returns a specific object
    for the data requested.
    """

  def getDeltaLocation():
    """
    Returns the object's delta location in a list (x, y, z)
    @rtype: A vector triple
    @return: (x, y, z)
    """

  def getDrawMode():
    """
    Returns the object draw mode.
    @rtype: Integer
    @return: a sum of the following:
        - 2  - axis
        - 4  - texspace
        - 8  - drawname
        - 16 - drawimage
        - 32 - drawwire
    """

  def getDrawType():
    """
    Returns the object draw type
    @rtype: Integer
    @return: One of the following:
        - 1 - Bounding box
        - 2 - Wire
        - 3 - Solid
        - 4 - Shaded
        - 5 - Textured
    """

  def getEuler():
    """
    Returns the object's rotation as Euler rotation vector (rotX, rotY, rotZ).
    @rtype: A vector triple
    @return: (rotX, rotY, rotZ)
    """

  def getInverseMatrix():
    """
    Returns the object's inverse matrix.
    @rtype: Blender Matrix object
    @return: the inverse of the matrix of the Object
    """

  def getLocation():
    """
    Returns the object's location (x, y, z).
    @rtype: A vector triple
    @return: (x, y, z)
    """

  def getMaterials():
    """
    Returns a list of materials assigned to the object.
    @rtype: list of Material Objects
    @return: list of Material Objects assigned to the object.
    """

  def getMatrix():
    """
    Returns the object matrix.
    @rtype: Blender Matrix object.
    @return: the matrix of the Object.
    """

  def getName():
    """
    Returns the name of the object
    @rtype: string
    @return: The name of the object
    """

  def getParent():
    """
    Returns the object's parent object.
    @rtype: Object
    @return: The parent object of the object. If not available, None will be
    returned.
    """

  def getTracked():
    """
    Returns the object's tracked object.
    @rtype: Object
    @return: The tracked object of the object. If not available, None will be
    returned.
    """

  def getType():
    """
    Returns the type of the object.
    @rtype: string
    @return: The type of object.
    """

  def link(object):
    """
    Links Object with data provided in the argument. The data must match the
    Object's type, so you cannot link a Lamp to a Mesh type object.
    @type object: Blender Object
    @param object: A Blender Object.
    """

  def makeParent(objects, noninverse = 0, fast = 0):
    """
    Makes the object the parent of the objects provided in the argument which
    must be a list of valid Objects.
    @type objects: Blender Object
    @param objects: A Blender Object.
    @type noninverse: Integer
    @param noninverse:
        0 - make parent with inverse
        1 - make parent without inverse
    @type fast: Integer
    @param fast:
        0 - update scene hierarchy automatically
        1 - don't update scene hierarchy (faster). In this case, you must
        explicitely update the Scene hierarchy.
    """

  def setDeltaLocation(delta_location):
    """
    Sets the object's delta location which must be a vector triple.
    @type delta_location: A vector triple
    @param delta_location: A vector triple (x, y, z) specifying the new
    location.
    """

  def setDrawMode(drawmode):
    """
    Sets the object's drawing mode. The drawing mode can be a mix of modes. To
    enable these, add up the values.
    @type drawmode: Integer
    @param drawmode: A sum of the following:
        - 2  - axis
        - 4  - texspace
        - 8  - drawname
        - 16 - drawimage
        - 32 - drawwire
    """

  def setDrawType(drawtype):
    """
    Sets the object's drawing type.
    @type drawtype: Integer
    @param drawtype: One of the following:
        - 1 - Bounding box
        - 2 - Wire
        - 3 - Solid
        - 4 - Shaded
        - 5 - Textured
    """

  def setEuler(rotation):
    """
    Sets the object's rotation according to the specified Euler angles. The
    argument must be a vector triple.
    @type rotation: A vector triple
    @param rotation: A vector triple (x, y, z) specifying the Euler angles.
    """

  def setLocation(location):
    """
    Sets the object's location. The argument must be a vector triple.
    @type location: A vector triple
    @param location: A vector triple (x, y, z) specifying the new location.
    """

  def setMaterials(materials):
    """
    Sets the materials. The argument must be a list of valid material objects.
    @type materials: Materials list
    @param materials: A list of Blender material objects.
    """

  def setName(name):
    """
    Sets the name of the object.
    @type name: String
    @param name: The new name for the object.
    """
 
  def shareFrom(object):
    """
    Link data of self with object specified in the argument. This works only
    if self and the object specified are of the same type.
    @type object: Blender Object
    @param object: A Blender Object of the same type.
    """
