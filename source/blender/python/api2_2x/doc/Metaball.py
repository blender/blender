# Blender.Metaball module and the Metaball PyType metaball

"""
The Blender.Metaball submodule

This module provides access to the B{Metaball Data} in Blender.

Example::
 import Blender

 ob =  Blender.Object.New("Mball","mb")
 mb = Blender.Metaball.New()
 for i in range(20):
   mb.addMetaelem([0, float(i),1.0,1.0, 2.0,1,2.0,1.0,1.0,1.0])
 ob.link(mb)
 sc = Blender.Scene.getCurrent()
 sc.link(ob)
"""


def New (name):
  """
  Creates a new Metaball.
  @type name: string
  @param name: The name of the metaball. If this parameter is not given (or not valid) blender will assign a name to the metaball.
  @rtype: Blender Metaball
  @return: The created Metaball.
  """

def Get (name):
  """
  Get the Metaball from Blender.
  @type name: string
  @param name: The name of the requested Metaball.
  @rtype: Blender Metaball or a list of Blender Metaballs
  @return: It depends on the 'name' parameter:
      - (name): The Metaball with the given name;
      - ():     A list with all Metaballs in the current scene.
  """

class Metaball:
  """
  The Metaball object
  ===================
  This metaball gives access to generic data from all metaballs in Blender.
  @cvar name: The name of the metaball.
  @cvar loc: The location of the metaball.
  @cvar rot: The rotation of the metaball.
  @cvar size: The size of the metaball.
  """

  def addMetaelem(paramslist):
    """
    Add a new metaelem to this metaball. 'paramslist' must have the
    following ten elements:
      - param 1: int - metaelem type:
        - 0 for a sphere
        - 1 for a tubex
        - 2 for a tubey
        - 3 for a tubez
        - 4 for a regular tube
        - 5 for a plane
        - 6 for an ellipsoid
        - 7 for a cube
      - params 2,3,4: floats - the x, y and z coordinates of the metaelem.
      - param 5: float - the rad value of the metaelem.
      - param 6: int - the lay value.
      - param 7: float - the s value of the metaelem.
      - params 8,9,10: floats - the expx, expy and expz values of the metaelem.
    @type paramslist: list
    @param paramslist: the list of the parameters for creating a new metaelem.
    @rtype: None
    @return:  None
    """

		
  def getMetaElemList():
    """
    retreives the list of the Metaelems of the Metaball
    @rtype: List
    @return:  List of Metaelem classes
		Each Metaelem object has NO function, but 4 read/write fields\n
		coords : a list of three floats, the coordinates of the Metaelem object\n
		dims :  a list of three floats, the dimensions of the meteelem object\n
		rad : a float, the radius of the object\n
		stif : a float, the stifness of the object\n
		Example : \n
		import Blender\n

		mb = Blender.Metaball.Get("mball")\n
		ob = Blender.Object.Get("ob")\n
		ll =  mb.getMetaElemList()\n
		me = ll[0]\n
		old = me.coords\n
		new = [old[0],old[1],old[2]-.02]\n
		me.coords = new\n

		ob.makeDisplayList()\n
		Blender.Window.QRedrawAll()
		"""

  def getName():
    """
    Retrieves the name of a metaball object.
    @rtype: string
    @return:  the name of a metaball object.
    """

  
  def setName(name):
    """
    Sets the name of a metaball object.
    @type name: string
    @param name : the new name
    @rtype: None
    @return:  None
    """

  

  def getBbox():
    """
    Retrieves the bounding box of a metaball object.
    @rtype: a list of 24 floats(8 points, 3 coordinates)
    @return:  the bounding box of a metaball object
    """

  def getNMetaElems():
    """
    Retrieves the number of metaelems (elementary spheres or cylinders) of a metaball object.
    @rtype: int
    @return: number of metaelems of a metaball object
    """

  def getLoc():
    """
    Retrieves the location of a metaball object.
    @rtype: a list of 3 floats
    @return: location of a metaball object
    """

  def setLoc(newloc):
    """
    Sets the location of a metaball object.
    @type newloc: list of 3 floats
    @param newloc: the new location
    @rtype: None
    @return: None
    """

  def getRot():
    """
    Retrieves the rotation of a metaball object.
    @rtype: a list of 3 floats
    @return: rotation of a metaball object
    """

  def setRot(newrot):
    """
    Sets the rotation of a metaball object.
    @type newrot: list of 3 floats
    @param newrot: the new rotation
    @rtype: None
    @return: None
    """

  def getSize():
    """
    Retrieves the size of a metaball object.
    @rtype: a list of 3 floats
    @return: size a metaball object
    """

  def setSize(newsize):
    """
    Sets the size of a metaball object.
    @type newsize: list of 3 floats
    @param newsize: the new size
    @rtype: None
    @return: None
    """

  def getWiresize():
    """
    Retrieves the wiresize of a metaball object.
    @rtype: float
    @return: wire size a metaball object
    """

  def setWiresize(newsize):
    """
    Sets the wire size of a metaball object.
    @type newsize: float
    @param newsize: the new size
    @rtype: None
    @return: None
    """
  def getRendersize():
    """
    Retrieves the rendersize of a metaball object.
    @rtype: float
    @return: render size a metaball object
    """

  def setRendersize(newsize):
    """
    Sets the render size of a metaball object.
    @type newsize: float
    @param newsize: the new size
    @rtype: None
    @return: None
    """

  def getThresh():
    """
    Retrieves the threshold of a metaball object.
    @rtype: float
    @return: threshold of the metaball object
    """

  def setThresh(threshold):
    """
    Sets the threshold of a metaball object.
    @type threshold: float
    @param threshold: the new size
    @rtype: None
    @return: None
    """

  def getMetadata(name,num):
    """
    Retrieves the metadata of a metaball object. A metaball is composed of one or several elementary objects, spheres or cylinders, which interact to create the smooth surface everybody knows. The get/set Metadata functions allow users to read/write the parameters of these elementary objects, called metaelements.
    @type name: string
    @param name: the name of the property to be read. The accepted values are :"type", "x", "y", "z", "expx", "expy", "expz", "rad", "rad2", "s", "len".
    @type num: int
    @param num: the position of the metaelem to be accessed.
    @rtype: float
    @return: the metaelement parameter value, generally a float, except for the parameter "type", which returns an int.
    """

  def setMetadata(name,num,val):
    """
    The setMetadata function has the same semantics as getMetadata,   except that it needs  the parameter value, and always returns None.
    @type name: string
    @param name: the name of the property to be read. The accepted values are :"type", "x", "y", "z", "expx", "expy", "expz", "rad", "rad2", "s", "len".
    @type num: int
    @param num: the position of the metaelem to be accessed.
    @type val: float, except if name is "type".
    @param val: the new value of the parameter.
    @rtype: None
    @return: None
    """

  def getMetatype(pos):
    """
    Retrieves the type of a metaelem object.
    @type pos: int
    @param pos: the position of the metaelement
    @rtype: int
    @return: type of the metaelem object
    """

  def setMetatype(pos,newtype):
    """
    Sets the type of a metaelem object.
    @type pos: int
    @param pos: the position of the metaelement
    @type newtype: int
    @param newtype: the new type
    @rtype: None
    @return: None
    """

  def getMetax(pos):
    """
    Retrieves the x parameter of a metaelem object.
    @type pos: int
    @param pos: the position of the metaelement
    @rtype: float
    @return: x parameter of the metaelem object
    """

  def setMetax(pos,newx):
    """
    Sets the x parameter of a metaelem object.
    @type pos: int
    @param pos: the position of the metaelement
    @type newx: float
    @param newx: the new x parameter value
    @rtype: None
    @return: None
    """

  def getMetay(pos):
    """
    Retrieves the y parameter of a metaelem object.
    @type pos: int
    @param pos: the position of the metaelement
    @rtype: float
    @return: y parameter of the metaelem object
    """

  def setMetay(pos,newy):
    """
    Sets the y parameter of a metaelem object.
    @type pos: int
    @param pos: the position of the metaelement
    @type newy: float
    @param newy: the new y parameter value
    @rtype: None
    @return: None
    """

  def getMetaz(pos):
    """
    Retrieves the z parameter of a metaelem object.
    @type pos: int
    @param pos: the position of the metaelement
    @rtype: float
    @return: z parameter of the metaelem object
    """

  def setMetaz(pos,newz):
    """
    Sets the z parameter of a metaelem object.
    @type pos: int
    @param pos: the position of the metaelement
    @type newz: float
    @param newz: the new z parameter value
    @rtype: None
    @return: None
    """


  def getMetas(pos):
    """
    Retrieves the s parameter of a metaelem object.
    @type pos: int
    @param pos: the position of the metaelement
    @rtype: float
    @return: s parameter of the metaelem object
    """

  def setMetas(pos,news):
    """
    Sets the s parameter of a metaelem object.
    @type pos: int
    @param pos: the position of the metaelement
    @type news: float
    @param news: the new x parameter value
    @rtype: None
    @return: None
    """

  def getMetalen(pos):
    """
    Retrieves the len parameter of a metaelem object.
    @type pos: int
    @param pos: the position of the metaelement
    @rtype: float
    @return: len parameter of the metaelem object
    """

  def setMetalen(pos,newlen):
    """
    Sets the len parameter of a metaelem object.
    @type pos: int
    @param pos: the position of the metaelement
    @type newlen: float
    @param newlen: the new x parameter value
    @rtype: None
    @return: None
    """
