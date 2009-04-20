# Blender.Text3d module and the Text3d PyType object

"""
The Blender.Text3d submodule.

Text3d Objects
==============

This module provides access to B{Font} objects in Blender.

Example::
	import Blender
	from Blender import Curve, Object, Scene, Text3d
	txt = Text3d.New("MyText")  # create a new Text3d object called MyText
	scn = Scene.GetCurrent()    # get current scene
	ob = scn.objects.new(txt)   # create an object from the obdata in the current scene
	ob.makeDisplayList()        # rebuild the display list for this object
	Window.RedrawAll()
"""

def New (name = None):
	"""
	Create a new Text3d object.
	@type name: string
	@param name: The name for the new object..
	@rtype: Blender Text3d
	@return: The created Text3d Data object.
	"""

def Get (name = None):
	"""
	Get the Text3d object(s) from Blender.
	@type name: string
	@param name: The name of the Text3d object.
	@rtype: Blender Text3d or a list of Blender Text3d's
	@return: It depends on the 'name' parameter:
			- (name): The Text3d object with the given name;
			- ():     A list with all Text3d objects in the current scene.
	"""
class Text3d:
	"""
	The Text3d object
	=================
		This object gives access  Blender's B{Font} objects
	@ivar frameWidth: The width of the active frame [0.0 - 50.0]
	@ivar frameHeight: The height of the active frame [0.0 - 50.0]
	@ivar frameX: The X position of the active frame [0.0 - 50.0]
	@ivar frameY: The Y position of the active frame [0.0 - 50.0]
	
	@ivar totalFrames: The total number of text frames (read only)
	@ivar activeFrame: The active frame for this text data.
	"""

	def getName():
		"""
		Get the name of this Text3d object.
		@rtype: string
		"""

	def setName( name ):
		"""
		Set the name of this Text3d object.
		@type name: string
		@param name: The new name.
		@returns: None
		"""

	def getText():
		"""
		Get text string for this object
		@rtype: string
		"""

	def setText( name ):
		"""
		Set the text string in this Text3d object
		@type name: string
		@param name:  The new text string for this object.
		@returns: None
		"""
	
	def getDrawMode():
		"""
		Get the drawing mode (3d, front, and/or back)
		Gets the text3d's drawing modes.  Uses module constants
			 - DRAW3D    :  "3D" is set
			 - DRAWFRONT :  "Front" is set
			 - DRAWBACK  :  "Back" is set      
		@rtype: tuple of module constants
		"""

	def setDrawMode(val):
		"""
		Set the text3d's drawing mode. Uses module constants
			- DRAW3D
			- DRAWFRONT
			- DRAWBACK
		@rtype: None
		@type val: single module constant or tuple of module constants
		@param val : The Text3d's modes.  See L{getDrawMode} for the meaning of
		the constants.
		"""

	def getUVordco():
		"""
		Return whether UV coords are used for Texture mapping 
		"""
		 
	def setUVordco(val):
		"""
		Set the font to use UV coords for Texture mapping 
		"""    
		 
	def getBevelAmount():
		"""
		Get the Text3d's bevel resolution value.
		@rtype: float
		"""

	def setBevelAmount(bevelresol):
		"""
		Set the Text3d's bevel resolution value.
		@rtype: None
		@type bevelresol: float
		@param bevelresol: The new Curve's bevel resolution value.
		"""

	def getDefaultResolution():
		"""
		Return Default text resolution.
		@rtype: float
		"""

	def setDefaultResolution(resolu):
		"""
		Sets Default text Resolution.
		@rtype: None
		@type resolu: float
		@param resolu: The new Curve's U-resolution value.
		"""

	def getWidth():
		"""
		Get the Text3d's width value.
		@rtype: float
		"""

	def setWidth(width):
		"""
		Set the Text3d's width value. 
		@rtype: None
		@type width: float
		@param width: The new text3d's width value. 
		"""

	def getExtrudeDepth():
		"""
		Get the text3d's ext1 value.
		@rtype: float
		"""

	def setExtrudeDepth(ext1):
		"""
		Set the text3d's ext1 value. 
		@rtype: None
		@type ext1: float
		@param ext1: The new text3d's ext1 value. 
		"""

	def getExtrudeBevelDepth():
		"""
		Get the text3d's ext2 value.
		@rtype: float
		"""

	def setExtrudeBevelDepth(ext2):
		"""
		Set the text3d's ext2 value.
		@rtype: None 
		@type ext2: float
		@param ext2: The new text3d's ext2 value. 
		"""

	def getShear():
		"""
		Get the text3d's shear value.
		@rtype: float
		"""

	def setShear(shear):
		"""
		Set the text3d's shear value.
		@rtype: None 
		@type shear: float
		@param shear: The new text3d's shear value. 
		"""

	def getSize():
		"""
		Get the text3d's size value.
		@rtype: float
		"""

	def setSize(size):
		"""
		Set the text3d's size value.
		@rtype: None 
		@type size: float
		@param size: The new text3d's size value. 
		"""

	def getLineSeparation():
		"""
		Get the text3d's ext2 value.
		@rtype: float
		"""

	def setLineSeparation(sep):
		"""
		Set the text3d's ext2 value.
		@rtype: None 
		@type sep: float
		@param sep: The new text3d's separation value. 
		"""

	def getSpacing():
		"""
		Get the text3d's spacing value.
		@rtype: float
		"""

	def setSpacing(spacing):
		"""
		Set the text3d's spacing value.
		@rtype: None 
		@type spacing: float
		@param spacing: The new text3d's spacing value. 
		"""

	def getXoffset():
		"""
		Get the text3d's Xoffset value.
		@rtype: float
		"""

	def setXoffset(xof):
		"""
		Set the text3d's Xoffset value.
		@rtype: None 
		@type xof: float
		@param xof: The new text3d's Xoffset value. 
		"""

	def getYoffset():
		"""
		Get the text3d's Yoffset value.
		@rtype: float
		"""

	def setYoffset(yof):
		"""
		Set the text3d's Yoffset value.
		@rtype: None 
		@type yof: float
		@param yof: The new text3d's Yoffset value. 
		"""

	def getAlignment():
		"""
		Get the text3d's alignment value. Uses module constants
				- LEFT
				- RIGHT
				- MIDDLE
				- FLUSH
		@rtype: module constant
		"""

	def setAlignment(align):
		"""
		Set the text3d's Alignment value. Uses module constants
				- LEFT
				- RIGHT
				- MIDDLE
				- FLUSH
		@rtype: None 
		@type align: module constant
		@param align: The new text3d's Alignment value. 
		"""
	
	def getMaterial(index):
		"""
		get the material index of a character.
		@rtype: int
		@return: the material index if the character
		@type index: int
		@param index: the index of the character in a string
		"""

	def setMaterial(index, material_index):
		"""
		Set a characters material.
		@note: after changing this youll need to update the object with object.makeDisplayList() to see the changes.
		@rtype: None
		@type index: int
		@param index: the index of the character in a string
		@type material_index: int
		@param material_index: the material index set set the character.
		"""
	
	def addFrame():
		"""
		Adds a text frame. maximum number of frames is 255.
		@rtype: None 
		"""

	def removeFrame(index):
		"""
		Removed the frame at this index
		@rtype: None 
		"""
import id_generics
Text3d.__doc__ += id_generics.attributes
