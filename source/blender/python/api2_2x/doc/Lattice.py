# Blender.Lattice module and the Lattice PyType object

"""
The Blender.Lattice submodule.

Lattice Object
==============

This module provides access to B{Lattice} object in Blender.

Example::
  import Blender
  from Blender import Lattice
  from Blender.Lattice import *
  from Blender import Object
  from Blender import Scene

  myOb = Object.New('Lattice')
  myLat = Lattice.New()
  myLat.setPartitions(5,5,5)
  myLat.setKeyTypes(LINEAR, CARDINAL, BSPLINE)
  myLat.setMode(OUTSIDE)

  for y in range(125):
    vec = myLat.getPoint(y)
    co1 = vec[0] + vec[0] / 5
    co2 = vec[1] - vec[2] * 0.3
    co3 = vec[2] * 3
    myLat.setPoint(y,[co1,co2,co3])

  myOb.link(myLat)
  mySphere = Object.Get('Sphere')
  myOb.makeParent([mySphere])

  myLat.applyDeform()

  sc = Scene.getCurrent()
  sc.link(myOb)
  Blender.Redraw()
"""

def New (name = None):
  """
  Create a new Lattice object.
  Passing a name to this function will name the Lattice
  datablock, otherwise the Lattice data will be given a 
  default name.
  @type name: string
  @param name: The Lattice name.
  @rtype: Blender Lattice
  @return: The created Lattice Data object.
  """

def Get (name = None):
  """
  Get the Lattice object(s) from Blender.
  @type name: string
  @param name: The name of the Lattice object.
  @rtype: Blender Lattice or a list of Blender Lattices
  @return: It depends on the 'name' parameter:
      - (name): The Lattice object with the given name;
      - ():     A list with all Lattice objects in the current scene.
  """

class Lattice:
  """
  The Lattice object
  ==================
    This object gives access to Lattices in Blender.
  @cvar name: The Lattice name.
  @cvar width: The number of x dimension partitions.
  @cvar height: The number of y dimension partitions.
  @cvar depth: The number of z dimension partitions.
  @cvar widthType: The x dimension key type.
  @cvar heightType: The y dimension key type.
  @cvar depthType: The z dimension key type.
  @cvar mode: The current mode of the Lattice.
  @cvar latSize: The number of points in this Lattice.
  """

  def getName():
    """
    Get the name of this Lattice datablock.
    @rtype: string
    @return: The name of the Lattice datablock.
    """

  def setName(name):
    """
    Set the name of this Lattice datablock.
    @type name: string
    @param name: The new name.
    """

  def getPartitions():
    """
    Gets the number of 'walls' or partitions that the Lattice has 
    in the x, y, and z dimensions.
    @rtype: list of ints
    @return: A list corresponding to the number of partitions: [x,y,z]
    """

  def setPartitions(x,y,z):
    """
    Set the number of 'walls' or partitions that the 
    Lattice will be created with in the x, y, and z dimensions.
    @type x: int
    @param x: The number of partitions in the x dimension of the Lattice.
    @type y: int
    @param y: The number of partitions in the y dimension of the Lattice.
    @type z: int
    @param z: The number of partitions in the z dimension of the Lattice.
    """

  def getKeyTypes():
    """
    Returns the deformation key types for the x, y, and z dimensions of the
    Lattice.
    @rtype: list of strings
    @return: A list corresponding to the key types will be returned: [x,y,z]
    """

  def setKeyTypes(xType,yType,zType):
    """
    Sets the deformation key types for the x, y, and z dimensions of the
    Lattice.
    There are three key types possible:
      -  Lattice.CARDINAL
      -  Lattice.LINEAR
      -  Lattice.BSPLINE
    @type xType: enum constant
    @param xType: the deformation key type for the x dimension of the Lattice
    @type yType: enum constant
    @param yType: the deformation key type for the y dimension of the Lattice
    @type zType: enum constant
    @param zType: the deformation key type for the z dimension of the Lattice
    """

  def getMode():
    """
    Returns the current Lattice mode
    @rtype: string
    @return: A string representing the current Lattice mode
    """

  def setMode(modeType):
    """
    Sets the current Lattice mode
    There are two Lattice modes possible:
      -  Lattice.GRID
      -  Lattice.OUTSIDE
    @type modeType: enum constant
    @param modeType: the Lattice mode
    """

  def getPoint(index):
    """
    Returns the coordinates of a point in the Lattice by index.
    @type index: int
    @param index: The index of the point on the Lattice you want returned
    @rtype: list of floats
    @return: The x,y,z coordiates of the Lattice point : [x,y,z]
    """

  def setPoint(index, position):
    """
    Sets the coordinates of a point in the Lattice by index.
    @type index: int
    @param index: The index of the point on the Lattice you want set
    @type position: list of floats
    @param position: The x,y,z coordinates that you want the point to be: [x,y,z]
    """

  def applyDeform():
    """
    Applies the current Lattice deformation to any child objects that have this 
    Lattice as the parent.
    """

  def insertKey(frame):
    """
    Inserts the current state of the Lattice as a new absolute keyframe

    B{Example}::
      for z in range(5):
        for y in range(125):
          vec = myLat.getPoint(y)
          co1 = vec[0] + vec[2]
          co2 = vec[1] - vec[2]
          co3 = vec[2] + vec[1]
          myLat.setPoint(y,[co1,co2,co3])
        w = (z + 1) * 10
        myLat.insertKey(w)
      myLat.applyDeform()

    @type frame: int
    @param frame: the frame at which the Lattice will be set as a keyframe
    """



