# Blender.Object module and the Object PyType object

"""
The Blender.Object submodule

This module provides access to the B{Object Data} in Blender.

Example::

  import Blender
  scene = Blender.Scene.getCurrent ()   # get the current scene
  ob = Blender.Object.New ('Camera')    # make camera object
  cam = Blender.Camera.New ('ortho')    # make ortho camera data object
  ob.link (cam)                         # link camera data with the object
  scene.link (ob)                       # link the object into the scene
  ob.setLocation (0.0, -5.0, 1.0)       # position the object in the scene

  Blender.Redraw()                      # redraw the scene to show the updates.
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
  @return: The created Object.

  I{B{Example:}}

  The example below creates a new Lamp object and puts it at the default
  location (0, 0, 0) in the current scene::
     import Blender
     
     object = Blender.Object.New ('Lamp')
     lamp = Blender.Lamp.New ('Spot')
     object.link (lamp)
     scene = Blender.Scene.getCurrent ()
     scene.link (object)

     Blender.Redraw()
  """

def Get (name = None):
  """
  Get the Object from Blender.
  @type name: string
  @param name: The name of the requested Object.
  @return: It depends on the 'name' parameter:
      - (name): The Object with the given name;
      - ():     A list with all Objects in the current scene.

  I{B{Example 1:}}

  The example below works on the default scene. The script returns the plane
  object and prints the location of the plane::
    import Blender

    object = Blender.Object.Get ('plane')
    print object.getLocation()

  I{B{Example 2:}}

  The example below works on the default scene. The script returns all objects
  in the scene and prints the list of object names::
    import Blender

    objects = Blender.Object.Get ()
    print objects
  """

def GetSelected ():
  """
  Get the selected objects from Blender. If no objects are selected, an empty
  list will be returned.
  @return: A list of all selected Objects in the current scene.

  I{B{Example:}}

  The example below works on the default scene. Select one or more objects and
  the script will print the selected objects::
    import Blender

    objects = Blender.Object.GetSelected ()
    print objects
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
        This variable applies to IPO Objects only.
    @cvar dLocY: The delta Y location coordinate of the object.
        This variable applies to IPO Objects only.
    @cvar dLocZ: The delta Z location coordinate of the object.
        This variable applies to IPO Objects only.
    @cvar dloc: The delta (X,Y,Z) location coordinates of the object (vector).
        This variable applies to IPO Objects only.
    @cvar RotX: The X rotation angle (in radians) of the object.
    @cvar RotY: The Y rotation angle (in radians) of the object.
    @cvar RotZ: The Z rotation angle (in radians) of the object.
    @cvar rot: The (X,Y,Z) rotation angles (in radians) of the object (vector).
    @cvar dRotX: The delta X rotation angle (in radians) of the object.
        This variable applies to IPO Objects only.
    @cvar dRotY: The delta Y rotation angle (in radians) of the object.
        This variable applies to IPO Objects only.
    @cvar dRotZ: The delta Z rotation angle (in radians) of the object.
        This variable applies to IPO Objects only.
    @cvar drot: The delta (X,Y,Z) rotation angles (in radians) of the object
        (vector).
        This variable applies to IPO Objects only.
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

  def clearIpo():
    """
    Unlinks the ipo from this object.
    @return: True if there was an ipo linked or False otherwise.
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
    Returns the object's rotation as Euler rotation vector (rotX, rotY, rotZ).  Angles are in radians.
    @rtype: A vector triple of floats
    @return: (rotX, rotY, rotZ)
    """

  def getInverseMatrix():
    """
    Returns the object's inverse matrix.
    @rtype: Blender Matrix object
    @return: the inverse of the matrix of the Object
    """

  def getIpo():
    """
    Returns the Ipo associated to this object or None if there's no linked ipo.
    @rtype: Ipo
    @return: the wrapped ipo or None.
    """

  def getLocation():
    """
    Returns the object's location (x, y, z).
    @return: (x, y, z)

    I{B{Example:}}

    The example below works on the default scene. It retrieves all objects in
    the scene and prints the name and location of each object::
      import Blender

      objects = Blender.Object.Get()

      for obj in objects:
          print obj.getName()
          print obj.getLocation()
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
    @return: The name of the object

    I{B{Example:}}

    The example below works on the default scene. It retrieves all objects in
    the scene and prints the name of each object::
      import Blender

      objects = Blender.Object.Get()

      for obj in objects:
          print obj.getName()
    """

  def getParent():
    """
    Returns the object's parent object.
    @rtype: Object
    @return: The parent object of the object. If not available, None will be
    returned.
    """

  def getSize():
    """
    Returns the object's size.
    @return: (SizeX, SizeY, SizeZ)
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
    @return: The type of object.

    I{B{Example:}}

    The example below works on the default scene. It retrieves all objects in
    the scene and updates the location and rotation of the camera. When run,
    the camera will rotate 180 degrees and moved to the oposite side of the X
    axis. Note that the number 'pi' in the example is an approximation of the
    true number 'pi'::
        import Blender

        objects = Blender.Object.Get()

        for obj in objects:
            if obj.getType() == 'Camera':
                obj.LocY = -obj.LocY
                obj.RotZ = 3.141592 - obj.RotZ

        Blender.Redraw()
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

  def setEuler(x, y, z):
    """
    Sets the object's rotation according to the specified Euler angles.
    @type x: float
    @param x: The rotation angle in radians for the X direction.
    @type y: float
    @param y: The rotation angle in radians for the Y direction.
    @type z: float
    @param z: The rotation angle in radians for the Z direction.
    """

  def setIpo(ipo):
    """
    Links an ipo to this object.
    @type ipo: Blender Ipo
    @param ipo: an object type ipo.
    """

  def setLocation(x, y, z):
    """
    Sets the object's location.
    @type x: float
    @param x: The X coordinate of the new location.
    @type y: float
    @param y: The Y coordinate of the new location.
    @type z: float
    @param z: The Z coordinate of the new location.
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

  def setSize(x, y, z):
    """
    Sets the object's size.
    @type x: float
    @param x: The X size multiplier.
    @type y: float
    @param y: The Y size multiplier.
    @type z: float
    @param z: The Z size multiplier.
    """

  def shareFrom(object):
    """
    Link data of self with object specified in the argument. This works only
    if self and the object specified are of the same type.
    @type object: Blender Object
    @param object: A Blender Object of the same type.
    """

  def getBoundBox():
    """
    Returns the bounding box of this object.  This works for meshes (out of
    edit mode) and curves.
    @rtype: list of 8 (x,y,z) float coordinate vectors
    @return: The coordinates of the 8 corners of the bounding box.
    """

  def makeDisplayList():
    """
    Updates this object's display list.  Blender uses display lists to store
    already transformed data (like a mesh with its vertices already modified
    by coordinate transformations and armature deformation).  If the object
    isn't modified, there's no need to recalculate this data.  This method is
    here for the *few cases* where a script may need it, like when toggling
    the "SubSurf" mode for a mesh:
    Example::
    object = Blender.Object.Get("Sphere")
    nmesh = object.getData()
    nmesh.setMode("SubSurf")
    nmesh.update() # don't forget to update!
    object.makeDisplayList()
    Blender.Window.RedrawAll() # and don't forget to redraw

    If you try this example without the line to update the display list, the
    object will disappear from the screen until you press "SubSurf".
    @warn: If after running your script objects disappear from the screen or
       are not displayed correctly, try this method function.  But if the script
       works properly without it, there's no reason to use it.
    """
