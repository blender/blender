# Blender.World module and the World PyType 

"""
The Blender.World submodule

INTRODUCTION

The module world allows you to access all the data of a Blender World.

Example::
  import Blender
  w = Blender.Get('World') #assume there exists a world named "world"
  print w.getName()
  w.hor = [1,1,.2]
  print w.getHor()	
"""

def New (name):
  """
  Creates a new World.
  @type name: string
  @param name: World's name (optionnal).
  @rtype: Blender World
  @return: The created World. If the "name" paraeter has not been provided, it will be automatically be set by blender.
  """

def Get (name):
  """
  Get an World from Blender.
  @type name: string
  @param name: The name of the world to retrieve.
  @rtype: Blender World or a list of Blender Worlds
  @return:
      - (name): The World corresponding to the name
      - ():     A list with all Worlds in the current scene.
  """


def GetActive ():
  """
  Get the active world of the scene.
  @rtype: Blender World or None
  """

class World:
  """
  The World object
  ================
  This object gives access to generic data from all worlds in Blender.
  Its attributes depend upon its type.
	
  @cvar name: the name of the world.
  @cvar skytype: type of the sky. Bit 0 : Blend; Bit 1 : Real; Bit 2 : paper.
  @cvar mode:
  @cvar mistype: type of mist : O : quadratic; 1 : linear; 2 : square
  @cvar hor:   the horizon color  of a world object.
  @cvar zen: the zenith color  of a world object.
  @cvar amb: the ambient color  of a world object.
  @cvar star: the star parameters  of a world object. See getStar for the semantics of these parameters. 
  @cvar mist: the mist parameters  of a world object. See getMist for the semantics of these parameters. 
  @type ipo: Blender Ipo
  @cvar ipo: The world type ipo linked to this world object.
  """
 
  def getRange():
    """
    Retrieves the range parameter of a world object.
    @rtype: float
    @return: the range
    """

  def setRange(range):
    """
    Sets the range parameter of a world object.
    @type range: float
    @param range: the new range parameter
    @rtype: PyNone
    @return: PyNone
    """

  def getName():
    """
    Retreives the name of an world object
    @rtype: string
    @return:  the name of the world object.
    """

  def setName(name):
    """
    Sets the name of a world object.
    @type name: string
    @param name : the new name. 
    @rtype: PyNone
    @return:  PyNone
    """

  def getIpo():
    """
    Get the Ipo associated with this world object, if any.
    @rtype: Ipo
    @return: the wrapped ipo or None.
    """

  def setIpo(ipo):
    """
    Link an ipo to this world object.
    @type ipo: Blender Ipo
    @param ipo: a "camera data" ipo.
    """

  def clearIpo():
    """
    Unlink the ipo from this world object.
    @return: True if there was an ipo linked or False otherwise.
    """

  def getSkytype():
    """
    Retreives the skytype of a world object.
    The skytype is a combination of 3 bits : Bit 0 : Blend; Bit 1 : Real; Bit 2 : paper.
    @rtype: int
    @return:  the skytype of the world object.
    """

	
  def setSkytype(skytype):
    """
    Sets the skytype of a world object.
    See getSkytype for the semantics of the parameter.
    @type skytype: int
    @param skytype : the new skytype. 
    @rtype: PyNone
    @return:  PyNone
    """

  def getMode():
    """
    Retreives the mode of a world object.
    The mode is a combination of 3 bits : Bit 0 : Blend; Bit 1 : Real; Bit 2 : paper.
    @rtype: int
    @return:  the mode of the world object.
    """

	
  def setMode(mode):
    """
    Sets the mode of a world object.
    See getMode for the semantics of the parameter.
    @type mode: int
    @param mode : the new mode. 
    @rtype: PyNone
    @return:  PyNone
    """

  def getMistype():
    """
    Retreives the mist type of a world object.
    The mist type is an integer 0 : quadratic;  1 : linear;  2 : square.
    @rtype: int
    @return:  the mistype of the world object.
    """

	
  def setMistype(mistype):
    """
    Sets the mist type of a world object.
    See getMistype for the semantics of the parameter.
    @type mistype: int
    @param mistype : the new mist type. 
    @rtype: PyNone
    @return:  PyNone
    """

  def getHor():
    """
    Retreives the horizon color  of a world object.
    This color is a list of 3 floats.
    @rtype: list of three floats
    @return:  the horizon color of the world object.
    """

	
  def setHor(hor):
    """
    Sets the horizon color of a world object.
    @type hor:  list of three floats
    @param hor : the new hor. 
    @rtype: PyNone
    @return:  PyNone
    """

  def getZen():
    """
    Retreives the zenith color  of a world object.
    This color is a list of 3 floats.
    @rtype: list of three floats
    @return:  the zenith color of the world object.
    """

	
  def setZen(zen):
    """
    Sets the zenith color of a world object.
    @type zen:  list of three floats
    @param zen : the new zenith color. 
    @rtype: PyNone
    @return:  PyNone
    """

  def getAmb():
    """
    Retreives the ambient color  of a world object.
    This color is a list of 3 floats.
    @rtype: list of three floats
    @return:  the ambient color of the world object.
    """

	
  def setAmb(amb):
    """
    Sets the ambient color of a world object.
    @type amb:  list of three floats
    @param amb : the new ambient color. 
    @rtype: PyNone
    @return:  PyNone
    """

  def getStar():
    """
    Retreives the star parameters  of a world object.
    It is a list of nine floats :
    red component of the color
    green component of the color
    blue component of the color
    size of the stars
    minimal distance between the stars
    average distance between the stars
    variations of the stars color
    @rtype: list of nine floats
    @return:  the star parameters
    """

	
  def setStar(star):
    """
    Sets the star parameters  of a world object.
    See getStar for the semantics of the parameter.
    @type star:  list of 9 floats
    @param star : the new star parameters. 
    @rtype: PyNone
    @return:  PyNone
    """

  def getMist():
    """
    Retreives the mist parameters  of a world object.
    It is a list of four floats :
    intensity of the mist
    start of the mist
    end of the mist
    height of the mist
    @rtype: list of four floats
    @return:  the mist parameters
    """

	
  def setMist(mist):
    """
    Sets the mist parameters  of a world object.
    See getMist for the semantics of the parameter.
    @type mist:  list of 4 floats
    @param mist : the new mist parameters. 
    @rtype: PyNone
    @return:  PyNone
    """
