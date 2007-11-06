# Blender.World module and the World PyType 

"""
The Blender.World submodule

B{New}: L{World.clearScriptLinks} accepts a parameter now.

World
=====

The module world allows you to access all the data of a Blender World.

Example::
	import Blender
	w = Blender.Get('World') #assume there exists a world named "world"
	print w.getName()
	w.hor = [1,1,.2]
	print w.getHor()

Example::
	import Blender
	from Blender import *

	AllWorlds = Blender.World.Get()  # returns a list of created world objects
	AvailWorlds = len(AllWorlds)	#	returns the number of available world objects
	PropWorld = dir(AllWorlds[0])	# returns the properties of the class world
	NameWorld = AllWorlds[0].getName() # get name of the first world object

	MiType = AllWorlds[0].getMistype()	# get kind of mist from the first world object
	MiParam = AllWorlds[0].getMist()	# get the parameters intensity, start, end and height of the mist

	HorColor = AllWorlds[0].getHor()	# horizon color of the first world object
	HorColorR = HorColor[0]		# get the red channel (RGB) of the horizon color

	ZenColor = AllWorlds[0].getZen()	# zenith color of the first world object
	ZenColorB = ZenColor[2]		# get the blue channel (RGB) of the Zenith color

	blending = AllWorlds[0].getSkytype() # get the blending modes (real, blend, paper) of the first world object	
"""

def New (name):
	"""
	Creates a new World.
	@type name: string
	@param name: World's name (optional).
	@rtype: Blender World
	@return: The created World. If the "name" parameter has not been provided, it will be automatically be set by blender.
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


def GetCurrent ():
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

	@ivar skytype: type of the sky. Bit 0 : Blend; Bit 1 : Real; Bit 2 : paper.
	@ivar mode:
	@ivar mistype: type of mist : O : quadratic; 1 : linear; 2 : square
	@ivar hor:   the horizon color  of a world object.
	@ivar zen: the zenith color  of a world object.
	@ivar amb: the ambient color  of a world object.
	@ivar star: the star parameters  of a world object. See getStar for the semantics of these parameters. 
	@ivar mist: the mist parameters  of a world object. See getMist for the semantics of these parameters. 
	@type ipo: Blender Ipo
	@ivar ipo: The world type ipo linked to this world object.
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
		@rtype: None
		@return: None
		"""

	def getName():
		"""
		Retrieves the name of a world object
		@rtype: string
		@return:  the name of the world object.
		"""

	def setName(name):
		"""
		Sets the name of a world object.
		@type name: string
		@param name : the new name. 
		@rtype: None
		@return:  None
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
		Retrieves the skytype of a world object.
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
		@rtype: None
		@return:  None
		"""

	def getMode():
		"""
		Retrieves the mode of a world object.
		The mode is a combination of 5 bits:
			- Bit 0 : mist simulation
			- Bit 1 : starfield simulation
			- Bit 2,3 : reserved
			- Bit 4 : ambient occlusion
		@rtype: int
		@return:  the mode of the world object.
		"""

	def setMode(mode):
		"""
		Sets the mode of a world object.
		See getMode for the semantics of the parameter.
		@type mode: int
		@param mode : the new mode. 
		@rtype: None
		@return:  None
		"""

	def getMistype():
		"""
		Retrieves the mist type of a world object.
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
		@rtype: None
		@return:  None
		"""

	def getHor():
		"""
		Retrieves the horizon color  of a world object.
		This color is a list of 3 floats.
		@rtype: list of three floats
		@return:  the horizon color of the world object.
		"""

	def setHor(hor):
		"""
		Sets the horizon color of a world object.
		@type hor:  list of three floats
		@param hor : the new hor. 
		@rtype: None
		@return:  None
		"""

	def getZen():
		"""
		Retrieves the zenith color  of a world object.
		This color is a list of 3 floats.
		@rtype: list of three floats
		@return:  the zenith color of the world object.
		"""

	def setZen(zen):
		"""
		Sets the zenith color of a world object.
		@type zen:  list of three floats
		@param zen : the new zenith color. 
		@rtype: None
		@return:  None
		"""

	def getAmb():
		"""
		Retrieves the ambient color  of a world object.
		This color is a list of 3 floats.
		@rtype: list of three floats
		@return:  the ambient color of the world object.
		"""

	def setAmb(amb):
		"""
		Sets the ambient color of a world object.
		@type amb:  list of three floats
		@param amb : the new ambient color. 
		@rtype: None
		@return:  None
		"""

	def getStar():
		"""
		Retrieves the star parameters  of a world object.
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
		@rtype: None
		@return:  None
		"""

	def getMist():
		"""
		Retrieves the mist parameters  of a world object.
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
		@rtype: None
		@return:  None
		"""

	def getScriptLinks (event):
		"""
		Get a list with this World's script links of type 'event'.
		@type event: string
		@param event: "FrameChanged", "Redraw", "Render".
		@rtype: list
		@return: a list with Blender L{Text} names (the script links of the given
				'event' type) or None if there are no script links at all.
		"""

	def clearScriptLinks (links = None):
		"""
		Delete script links from this World :).  If no list is specified, all
		script links are deleted.
		@type links: list of strings
		@param links: None (default) or a list of Blender L{Text} names.
		"""

	def addScriptLink (text, event):
		"""
		Add a new script link to this World.
		@type text: string
		@param text: the name of an existing Blender L{Text}.
		@type event: string
		@param event: "FrameChanged", "Redraw" or "Render".
		"""
	
	def setCurrent ():
		"""
		Make this world active in the current scene.
		@rtype: None
		@return:  None    
		"""	
		
	def insertIpoKey(keytype):
		"""
		Inserts keytype values in world ipo at curframe. Uses module constants.
		@type keytype: Integer
		@param keytype:
			-ZENTIH
			-HORIZON
			-MIST
			-STARS
			-OFFSET
			-SIZE
		@return: py_none
		"""   

	def __copy__ ():
		"""
		Make a copy of this world
		@rtype: World
		@return:  a copy of this world
		"""

import id_generics
World.__doc__ += id_generics.attributes
