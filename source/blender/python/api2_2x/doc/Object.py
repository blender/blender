# Blender.Object module and the Object PyType object

"""
The Blender.Object submodule

B{New}: L{Object.getData} now accepts an optional bool keyword argument to
define if the user wants the data object or just its name.

Object
======

This module provides access to the B{Objects} in Blender.

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
    @cvar Layer: The object layer.  This value is a bitmask with one position
        set for each of the 20 possible layers starting from the low order bit.
        The easiest way to deal with these values in in hexadecimal notation.
        Example::
          ob.Layer = 0x04 # sets layer 3 ( bit pattern 0100 )
        After setting the Layer value, call Blender.Redraw( -1 ) to update
        the interface.
    @cvar parent: The parent object of the object. (Read-only)
    @cvar track: The object tracking this object. (Read-only)
    @cvar data: The data of the object. (Read-only)
    @cvar ipo: The ipo data associated with the object. (Read-only)
    @cvar mat: The matrix of the object relative to its parent. (Read-only)
    @cvar matrix: The matrix of the object in world space. (Read-only)
    @cvar matrixLocal: The matrix of the object relative to its parent. (Read-only)
    @cvar matrixWorld: The matrix of the object in world space. (Read-only)
    @cvar colbits: The Material usage mask. A set bit #n means: the Material
        #n in the Object's material list is used. Otherwise, the Material #n
        of the Objects Data material list is displayed.
    @cvar drawType: The object's drawing type used. 1 - Bounding box,
        2 - wire, 3 - Solid, 4- Shaded, 5 - Textured.
    @cvar drawMode: The object's drawing mode used. The value can be a sum
        of: 2 - axis, 4 - texspace, 8 - drawname, 16 - drawimage,
        32 - drawwire.
    @cvar name: The name of the object.
    @cvar sel: The selection state of the object, 1/0.  
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

  def getData(name_only = False):
    """
    Returns the Datablock object (Mesh, Lamp, Camera, etc.) linked to this Object.  If the keyword parameter 'name_only' is True, only the Datablock name is returned as a string. 
    @type name_only: bool
    @param name_only: This is a keyword parameter.  if True (or nonzero), only the name of the data object is returned.  The default value is False.
    @rtype: specific Object type or string
    @return: Depends on the type of Datablock linked to the Object.  If name_only is True, it returns a string.
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
    @rtype: Py_Euler
    @return: A python euler 
    """

  def getInverseMatrix():
    """
    Returns the object's inverse matrix.
    @rtype: Py_Matrix
    @return: A python matrix 4x4
    """

  def getIpo():
    """
    Returns the Ipo associated to this object or None if there's no linked ipo.
    @rtype: Ipo
    @return: the wrapped ipo or None.
    """
  def isSelected():
    """
    Returns the objects selection state as a boolean value True or False.
    @rtype: Boolean
    @return: Selection state as True or False
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

  def getAction():
    """
    Returns an action if one is associated with this object (only useful for armature types).
    @rtype: Py_Action
    @return: a python action.
    """

  def getMaterials(what = 0):
    """
    Returns a list of materials assigned to the object.
    @type what: int
    @param what: if nonzero, empty slots will be returned as None's instead
        of being ignored (default way). See L{NMesh.NMesh.getMaterials}.
    @rtype: list of Material Objects
    @return: list of Material Objects assigned to the object.
    """

  def getMatrix(space = 'worldspace'):
    """
    Returns the object matrix.
    @type space: string
    @param space: The desired matrix:
      - worldspace (default): absolute, taking vertex parents, tracking and
          ipo's into account;
      - localspace: relative to the object's parent;
      - old_worldspace: old behavior, prior to Blender 2.34, where eventual
          changes made by the script itself were not taken into account until
          a redraw happened, either called by the script or upon its exit.
    Returns the object matrix.
    @rtype: Py_Matrix
    @return: a python 4x4 matrix object
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

  def getTimeOffset():
    """
    Returns the time offset of the object's animation.
    @return: TimeOffset
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

  def getDupliVerts():
    """
    Get state of DupliVerts anim propertie
    @return: a boolean value.
    """

  def insertIpoKey(keytype):
    """
    Inserts keytype values in object ipo at curframe. Uses module constants.
    @type keytype: Integer
    @param keytype:
           -LOC
           -ROT
           -SIZE
           -LOCROT
           -LOCROTSIZE
           -PI_STRENGTH
           -PI_FALLOFF
           -PI_PERM
           -PI_SURFACEDAMP
           -PI_RANDOMDAMP
    @return: py_none
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

  def setEuler(euler):
    """
    Sets the object's rotation according to the specified Euler angles.
    @type euler: Py_Euler or a list of floats
    @param euler: a python euler or x,y,z rotations as floats
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

  def setMatrix(matrix):
    """
    Sets the object's matrix and updates it's tranformation. 
    @type matrix: Py_Matrix 4x4
    @param matrix: a python matrix 4x4.
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

  def setTimeOffset(timeOffset):
    """
    Sets the time offset of the object's animation.
    @type timeOffset: float
    @param timeOffset: The new time offset for the object's animation.
    """
  
  def shareFrom(object):
    """
    Link data of object specified in the argument with self. This works only
    if self and the object specified are of the same type.
    @type object: Blender Object
    @param object: A Blender Object of the same type.
    """
  
  def select(boolean):
    """
    Sets the object's selection state.
    @type boolean: Integer
    @param boolean:
        - 0  - unselected
        - 1  - selected
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
     Blender.Window.Redraw()

    If you try this example without the line to update the display list, the
    object will disappear from the screen until you press "SubSurf".
    @warn: If after running your script objects disappear from the screen or
       are not displayed correctly, try this method function.  But if the script
       works properly without it, there's no reason to use it.
    """

  def getScriptLinks (event):
    """
    Get a list with this Object's script links of type 'event'.
    @type event: string
    @param event: "FrameChanged" or "Redraw".
    @rtype: list
    @return: a list with Blender L{Text} names (the script links of the given
        'event' type) or None if there are no script links at all.
    """

  def clearScriptLinks ():
    """
    Delete all this Object's script links.
    @rtype: bool
    @return: 0 if some internal problem occurred or 1 if successful.
    """

  def addScriptLink (text, event):
    """
    Add a new script link to this Object.
    @type text: string
    @param text: the name of an existing Blender L{Text}.
    @type event: string
    @param event: "FrameChanged" or "Redraw".
    """

  def makeTrack (tracked, fast = 0):
    """
    Make this Object track another.
    @type tracked: Blender Object
    @param tracked: the object to be tracked.
    @type fast: int (bool)
    @param fast: if zero, the scene hierarchy is updated automatically.  If
        you set 'fast' to a nonzero value, don't forget to update the scene
        yourself (see L{Scene.Scene.update}).
    @note: you also need to clear the rotation (L{setEuler}) of this object
       if it was not (0,0,0) already.
    """

  def clearTrack (mode = 0, fast = 0):
    """
    Make this Object not track another anymore.
    @type mode: int (bool)
    @param mode: if nonzero the matrix transformation used for tracking is kept.
    @type fast: int (bool)
    @param fast: if zero, the scene hierarchy is updated automatically.  If
        you set 'fast' to a nonzero value, don't forget to update the scene
        yourself (see L{Scene.Scene.update}).
    """

  def getAllProperties ():
    """
    Return a list of properties from this object.
    @rtype: PyList
    @return: List of Property objects.
    """

  def getProperty (name):
    """
    Return a properties from this object based on name.
    @type name: string
    @param name: the name of the property to get.
    @rtype: Property object
    @return: The first property that matches name.
    """

  def addProperty (name, data, type):
    """
    Add a property to object.
    @type name: string
    @param name: the property name.
    @type data: string, int or float
    @param data: depends on what is passed in:
      - string:  string type property
      - int:  integer type property
      - float:  float type property
    @type type: string (optional)
    @param type: can be the following:
      - 'BOOL'
      - 'INT'
      - 'FLOAT'
      - 'TIME'
      - 'STRING'
    @warn: If a type is not declared string data will
    become string type, int data will become int type
    and float data will become float type. Override type
    to declare bool type, and time type.
    """

  def addProperty (property):
    """
    Add a property to object.
    @type property: Property object
    @param property: property object to add to object.
    @warn:  A property object can be added only once to an object'
    you must remove the property from an object to add it elsewhere.
    """

  def removeProperty (name):
    """
    Remove a property from an object by name.
    @type property: Property object
    @param property: property to remove by name.
    """

  def removeProperty (property):
    """
    Remove a property from an object.
    @type property: Property object
    @param property: property object to remove.
    """

  def removeAllProperties():
    """
    Removes all properties from an object. 
    """

  def copyAllPropertiesTo (object):
    """
    Copies all properties from one object to another.
    @type object: Object object
    @param object: Object that will recieve the properties.
    """

  def setDupliVerts(data):
    """
    Set state of DupliVerts anim propertie
    @param data: boolean value True, False, 0 or not 0.
    """

class Property:
  """
  The Property object
  ===================
    This property gives access to object property data in Blender.
    @cvar name: The property name.
    @cvar data: Data for this property. Depends on property type.
    @cvar type: The property type.
    @warn:  Comparisons between properties will only be true when
    both the name and data pairs are the same.
  """

  def getName ():
    """
    Get the name of this property.
    @rtype: string
    @return: The property name.
    """

  def setName (name):
    """
    Set the name of this property.
    @type name: string
    @param name: The new name of the property
    """

  def getData ():
    """
    Get the data for this property.
    @rtype: string, int, or float
    """

  def setData (data):
    """
    Set the data for this property.
    @type data: string, int, or float
    @param data: The data to set for this property.
    @warn:  See object.setProperty().  Changing data
    which is of a different type then the property is 
    set to (i.e. setting an int value to a float type'
    property) will change the type of the property 
    automatically.
    """

  def getType ():
    """
    Get the type for this property.
    @rtype: string
    """


  def getPIStregth():
    """
    Get the Object's Particle Interaction Strength.
    @rtype: float
    """

  def setPIStrength(strength):
    """
    Set the the Object's Particle Interaction Strength.
    Values between -1000.0 to 1000.0
    @rtype: PyNone
    @type strength: float
    @param strength: the Object's Particle Interaction New Strength.
    """

  def getPIFalloff():
    """
    Get the Object's Particle Interaction falloff.
    @rtype: float
    """

  def setPIFalloff(falloff):
    """
    Set the the Object's Particle Interaction falloff.
    Values between 0 to 10.0
    @rtype: PyNone
    @type falloff: float
    @param falloff: the Object's Particle Interaction New falloff.
    """    
    
  def getPIMaxDist():
    """
    Get the Object's Particle Interaction MaxDist.
    @rtype: float
    """

  def setPIMaxDist(MaxDist):
    """
    Set the the Object's Particle Interaction MaxDist.
    Values between 0 to 1000.0
    @rtype: PyNone
    @type MaxDist: float
    @param MaxDist: the Object's Particle Interaction New MaxDist.
    """    
    
  def getPIType():
    """
    Get the Object's Particle Interaction Type.
    @rtype: int
    """

  def setPIType(type):
    """
    Set the the Object's Particle Interaction type.
    Use Module Constants
      - NONE
      - WIND
      - FORCE
      - VORTEX
      - MAGNET
    @rtype: PyNone
    @type type: int
    @param type: the Object's Particle Interaction Type.
    """   
 
  def getPIUseMaxDist():
    """
    Get the Object's Particle Interaction if using MaxDist.
    @rtype: int
    """

  def setPIUseMaxDist(status):
    """
    Set the the Object's Particle Interaction MaxDist.
    0 = Off, 1 = on
    @rtype: PyNone
    @type status: int
    @param status: the new status
    """ 

  def getPIDeflection():
    """
    Get the Object's Particle Interaction Deflection Setting.
    @rtype: int
    """

  def setPIDeflection(status):
    """
    Set the the Object's Particle Interaction Deflection Setting.
    0 = Off, 1 = on
    @rtype: PyNone
    @type status: int
    @param status: the new status
    """ 

  def getPIPermf():
    """
    Get the Object's Particle Interaction Permiability.
    @rtype: float
    """

  def setPIPerm(perm):
    """
    Set the the Object's Particle Interaction Permiability.
    Values between 0 to 10.0
    @rtype: PyNone
    @type perm: float
    @param perm: the Object's Particle Interaction New Permiability.
    """    

  def getPIRandomDamp():
    """
    Get the Object's Particle Interaction RandomDamp.
    @rtype: float
    """

  def setPIRandomDamp(damp):
    """
    Set the the Object's Particle Interaction RandomDamp.
    Values between 0 to 10.0
    @rtype: PyNone
    @type damp: float
    @param damp: the Object's Particle Interaction New RandomDamp.
    """    

  def getPISurfaceDamp():
    """
    Get the Object's Particle Interaction SurfaceDamp.
    @rtype: float
    """

  def setPISurfaceDamp(damp):
    """
    Set the the Object's Particle Interaction SurfaceDamp.
    Values between 0 to 10.0
    @rtype: PyNone
    @type damp: float
    @param damp: the Object's Particle Interaction New SurfaceDamp.
    """    


  def getSBMass():
    """
    Get the Object's SB Mass.
    @rtype: float
    """

  def setSBMass(mass):
    """
    Set the the Object's SB Mass.
    Values between 0 to 50.0
    @rtype: PyNone
    @type mass: float
    @param mass: the Object's SB New mass.
    """  
  
  def getSBGravity():
    """
    Get the Object's SB Gravity.
    @rtype: float
    """

  def setSBGravity(grav):
    """
    Set the the Object's SB Gravity.
    Values between 0 to 10.0
    @rtype: PyNone
    @type grav: float
    @param grav: the Object's SB New Gravity.
    """ 
    
  def getSBFriction():
    """
    Get the Object's SB Friction.
    @rtype: float
    """

  def setSBFriction(frict):
    """
    Set the the Object's SB Friction.
    Values between 0 to 10.0
    @rtype: PyNone
    @type frict: float
    @param frict: the Object's SB New Friction.
    """ 

  def getSBErrorLimit():
    """
    Get the Object's SB ErrorLimit.
    @rtype: float
    """

  def setSBErrorLimit(err):
    """
    Set the the Object's SB ErrorLimit.
    Values between 0 to 1.0
    @rtype: PyNone
    @type err: float
    @param err: the Object's SB New ErrorLimit.
    """ 
    
  def getSBGoalSpring():
    """
    Get the Object's SB GoalSpring.
    @rtype: float
    """

  def setSBGoalSpring(gs):
    """
    Set the the Object's SB GoalSpring.
    Values between 0 to 0.999
    @rtype: PyNone
    @type gs: float
    @param gs: the Object's SB New GoalSpring.
    """ 
    
  def getSBGoalFriction():
    """
    Get the Object's SB GoalFriction.
    @rtype: float
    """

  def setSBGoalFriction(gf):
    """
    Set the the Object's SB GoalFriction.
    Values between 0 to 10.0
    @rtype: PyNone
    @type gf: float
    @param gf: the Object's SB New GoalFriction.
    """ 
    
  def getSBMinGoal():
    """
    Get the Object's SB MinGoal.
    @rtype: float
    """

  def setSBMinGoal(mg):
    """
    Set the the Object's SB MinGoal.
    Values between 0 to 1.0
    @rtype: PyNone
    @type mg: float
    @param mg: the Object's SB New MinGoal.
    """ 
    
  def getSBMaxGoal():
    """
    Get the Object's SB MaxGoal.
    @rtype: float
    """

  def setSBMaxGoal(mg):
    """
    Set the the Object's SB MaxGoal.
    Values between 0 to 1.0
    @rtype: PyNone
    @type mg: float
    @param mg: the Object's SB New MaxGoal.
    """ 
    
  def getSBInnerSpring():
    """
    Get the Object's SB InnerSpring.
    @rtype: float
    """

  def setSBInnerSpring(sprr):
    """
    Set the the Object's SB InnerSpring.
    Values between 0 to 0.999
    @rtype: PyNone
    @type sprr: float
    @param sprr: the Object's SB New InnerSpring.
    """ 
    
  def getSBInnerSpringFriction():
    """
    Get the Object's SB InnerSpringFriction.
    @rtype: float
    """

  def setSBInnerSpringFriction(sprf):
    """
    Set the the Object's SB InnerSpringFriction.
    Values between 0 to 10.0
    @rtype: PyNone
    @type sprf: float
    @param sprf: the Object's SB New InnerSpringFriction.
    """ 
    
  def getSBDefaultGoal():
    """
    Get the Object's SB DefaultGoal.
    @rtype: float
    """

  def setSBDefaultGoal(goal):
    """
    Set the the Object's SB DefaultGoal.
    Values between 0 to 1.0
    @rtype: PyNone
    @type goal: float
    @param goal: the Object's SB New DefaultGoal.
    """   

  def getSBEnable():
    """
    Get if the Object's SB is Enabled.
    @rtype: int
    """

  def setSBEnable(switch):
    """
    Enable / Disable Softbodies.
    1: on
    0: off
    @rtype: PyNone
    @type switch: int
    @param switch: the Object's SB New Enable Value.
    """  

  def getSBPostDef():
    """
    get Softbodies PostDef option
    @rtype: int
    """

  def setSBPostDef(switch):
    """
    Enable / Disable Softbodies PostDef option
    1: on
    0: off
    @rtype: PyNone
    @type switch: int
    @param switch: the Object's SB New PostDef Value.
    """ 

  def getSBUseGoal():
    """
    get Softbodies UseGoal option
    @rtype: int
    """

  def setSBUseGoal(switch):
    """
    Enable / Disable Softbodies UseGoal option
    1: on
    0: off
    @rtype: PyNone
    @type switch: int
    @param switch: the Object's SB New UseGoal Value.
    """ 
  def getSBUseEdges():
    """
    get Softbodies UseEdges option
    @rtype: int
    """

  def setSBUseEdges(switch):
    """
    Enable / Disable Softbodies UseEdges option
    1: on
    0: off
    @rtype: PyNone
    @type switch: int
    @param switch: the Object's SB New UseEdges Value.
    """ 
    
  def getSBStiffQuads():
    """
    get Softbodies StiffQuads option
    @rtype: int
    """

  def setSBStiffQuads(switch):
    """
    Enable / Disable Softbodies StiffQuads option
    1: on
    0: off
    @rtype: PyNone
    @type switch: int
    @param switch: the Object's SB New StiffQuads Value.
    """     
