# Blender.Ipo module and the Ipo PyType object

"""
The Blender.Ipo submodule

This module provides access to the Ipo Data in Blender.


Example::
  import Blender
  ob = Blender.Ipo.Get('ipo')    # retreives an ipo object
  ob.setName('ipo1')
  print ob.name
  print ipo.getRctf()
  ipo.setRctf(1,2,3,4)
	
"""

def New (name,idcode):
  """
  Creates a new Ipo.
  @type name: string
  @type idcode: int
  @param name: The Ipo's name
  @param idcode: The Ipo's blocktype. Depends to the object the ipo will be linked.
  @rtype: Blender Ipo
  @return: The created Ipo.
  """

def Get (name = None):
  """
  Get the Ipo from Blender.
  @type name: string
  @param name: The name of the requested Ipo, or nothing.
  @rtype: Blender Ipo or a list of Blender Ipos
  @return: It depends on the 'name' parameter:
      - (name): The Ipo with the given name;
      - ():     A list with all Ipos in the current scene.
  """


class Ipo:
  """
  The Ipo object
  ==============
  This object gives access to generic data from all objects in Blender.
  This object has no attribute.
  """

  def getName():
    """
		Gets the name of the Ipo.
		@rtype: string
		@return: the name of the Ipo.
    """
  def setName(newname):
    """
		Sets the name of the Ipo.
		@type newname: string
		@rtype: PyNone
		@return: PyNone
    """

  def getBlocktype():
    """
		Gets the blocktype of the Ipo.
		@rtype: int
		@return: the blocktype of the Ipo.
    """
  def setBlocktype(newblocktype):
    """
		Sets the blocktype of the Ipo.
		@type newblocktype: int. This value should not be changed, unless you really know what you do...
		@rtype: PyNone
		@return: PyNone
    """

  def getRctf():
    """
		Gets the rctf of the Ipo.
		Kind of bounding box...
		@rtype: list of floats
		@return: the rctf of the Ipo.
    """
  def setRctf(newrctf):
    """
		Sets the rctf of the Ipo.
		@type newrctf: four floats . This value should not be changed, unless you really know what you do...
		@rtype: PyNone
		@return: PyNone
    """

  def getNcurves():
    """
		Gets the number of curves of the Ipo.
		@rtype: int 
		@return: the number of curve of the Ipo.
    """
		
  def getCurveBP(curvepos):
    """
		Gets the basepoint of a curve of the ipo.
		@type curvepos: int
		@param curvepos: the position of the curve.
		@rtype: a list of 4 floats
		@return: the coordinates of the basepoint, or an error is raised.
    """
		
  def getBeztriple(curvepos,pointpos):
    """
		Gets a beztriple of the Ipo.
		@type curvepos: int
		@param curvepos: the position of the curve in the ipo
		@type pointpos: int
		@param pointpos: the position of the point in the curve.
		@rtype: list of 9 floats
		@return: the beztriple of the Ipo, or an error is raised.
    """
  def setBeztriple(curvepos,pointpos,newbeztriple):
    """
		Sets the beztriple of the Ipo.
		@type curvepos: int
		@param curvepos: the position of the curve in the ipo
		@type pointpos: int
		@param pointpos: the position of the point in the curve.
		@type newbeztriple: list of 9 floats
		@param newbeztriple: the new value for the point
		@rtype: PyNone
		@return: PyNone
    """
		
  def getCurvecurval(curvepos):
    """
		Gets the current value of a curve of the Ipo.
		@type curvepos: int
		@param curvepos: the position of the curve in the ipo
		@rtype: float
		@return: the current value of the selected curve of the Ipo.
    """
