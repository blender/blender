# Blender.Camera module and the Camera PyType object

"""
The Blender.Camera submodule.

B{New}: L{Camera.clearScriptLinks} accepts a parameter now.

Camera Data
===========

This module provides access to B{Camera Data} objects in Blender.

Example::

	from Blender import Camera, Object, Scene
	cam = Camera.New('ortho')   # create new ortho camera data
	cam.scale = 6.0             # set scale value for ortho view
	scn = Scene.GetCurrent()    # get current scene
	ob = scn.objects.new(cam)   # add a new camera object from the data
	scn.setCurrentCamera(ob)    # make this camera the active
"""

def New (type = 'persp', name = 'CamData'):
	"""
	Create a new Camera Data object.
	@type type: string
	@param type: The Camera type: 'persp' or 'ortho'.
	@type name: string
	@param name: The Camera Data name.
	@rtype: Blender Camera
	@return: The created Camera Data object.
	"""

def Get (name = None):
	"""
	Get the Camera Data object(s) from Blender.
	@type name: string
	@param name: The name of the Camera Data.
	@rtype: Blender Camera or a list of Blender Cameras
	@return: It depends on the I{name} parameter:
			- (name): The Camera Data object with the given I{name};
			- ():     A list with all Camera Data objects in the current scene.
	"""

class Camera:
	"""
	The Camera Data object
	======================
		This object gives access to Camera-specific data in Blender.
	@ivar type: The Camera type: 'persp' or 'ortho'
	@ivar mode: The mode flags: B{ORed value}: 'showLimits':1, 'showMist':2.
	@ivar lens: The lens value in [1.0, 250.0], only relevant to *persp* cameras.
	@ivar angle: The lens value in degrees [7.323871, 172.847331], only relevant to *persp* cameras.
	@ivar scale: The scale value in [0.01, 1000.00], only relevant to *ortho* cameras.
	@ivar clipStart: The clip start value in [0.0, 100.0].
	@ivar clipEnd: The clip end value in [1.0, 5000.0].
	@ivar dofDist: The dofDist value in [0.0, 5000.0].
	@ivar shiftX: The horizontal offset of the camera [-2.0, 2.0].
	@ivar shiftY: The vertical offset of the camera [-2.0, 2.0].
	@ivar alpha: The PassePart alpha [0.0, 1.0].
	@ivar drawSize: The display size for the camera an the 3d view [0.1, 10.0].
	@type ipo: Blender Ipo
	@ivar ipo: The "camera data" ipo linked to this camera data object.
		Set to None to clear the ipo.

	@ivar drawLimits: Toggle the option to show limits in the 3d view.
	@ivar drawName: Toggle the option to show the camera name in the 3d view.
	@ivar drawMist: Toggle the option to show mist in the 3d view.
	@ivar drawTileSafe: Toggle the option to show tile safe in the 3d view.
	@ivar drawPassepartout: Toggle the option to show pass part out in the 3d view.

	@warning: Most member variables assume values in some [Min, Max] interval.
			When trying to set them, the given parameter will be clamped to lie in
			that range: if val < Min, then val = Min, if val > Max, then val = Max.
	"""

	def getName():
		"""
		Get the name of this Camera Data object.  (B{deprecated}) See the L{name} attribute.
		@rtype: string
		"""

	def setName(name):
		"""
		Set the name of this Camera Data object. (B{deprecated}) See the L{name} attribute.
		@type name: string
		@param name: The new name.
		"""

	def getIpo():
		"""
		Get the Ipo associated with this camera data object, if any. (B{deprecated})
		@rtype: Ipo
		@return: the wrapped ipo or None. (B{deprecated}) See the L{ipo} attribute.
		"""

	def setIpo(ipo):
		"""
		Link an ipo to this camera data object.  (B{deprecated}) See the L{ipo} attribute.
		@type ipo: Blender Ipo
		@param ipo: a "camera data" ipo.
		"""

	def clearIpo():
		"""
		Unlink the ipo from this camera data object.  (B{deprecated}) See the L{ipo} attribute.
		@return: True if there was an ipo linked or False otherwise.
		"""

	def getType():
		"""
		Get this Camera's type.  (B{deprecated}) See the L{type} attribute.
		@rtype: int
		@return: 0 for 'persp' or 1 for 'ortho'.
		"""

	def setType(type):
		"""
		Set this Camera's type.  (B{deprecated}) See the L{type} attribute.
		@type type: string
		@param type: The Camera type: 'persp' or 'ortho'.
		"""

	def getMode():
		"""
		Get this Camera's mode flags. (B{deprecated}) See the L{mode} attribute.
		@rtype: int
		@return: B{OR'ed value}: 'showLimits' is 1, 'showMist' is 2, or
			 respectively, 01 and 10 in binary.
		"""

	def setMode(mode1 = None, mode2 = None):
		"""
		Set this Camera's mode flags. Mode strings given are turned 'on'.  (B{deprecated}) See the L{mode} attribute.
		Those not provided are turned 'off', so cam.setMode() -- without 
		arguments -- turns off all mode flags for Camera cam.
		@type mode1: string
		@type mode2: string
		@param mode1: A mode flag: 'showLimits' or 'showMist'.
		@param mode2: A mode flag: 'showLimits' or 'showMist'.
		"""

	def getLens():
		"""
		Get the lens value.  (B{deprecated}) See the L{lens} attribute.
		@rtype: float
		@warn: lens is only relevant for perspective (L{getType}) cameras.
		"""

	def setLens(lens):
		"""
		Set the lens value. (B{deprecated}) See the L{lens} attribute.
		@type lens: float
		@param lens: The new lens value. 
		@warn: lens is only relevant for perspective (L{type}) cameras.
		"""

	def getScale():
		"""
		Get the scale value. (B{deprecated}) See the L{scale} attribute.
		@rtype: float
		@warn: scale is only relevant for ortho (L{type}) cameras.
		"""

	def setScale(scale):
		"""
		Set the scale value. (B{deprecated}) See the L{scale} attribute.
		@type scale: float
		@param scale: The new scale value in [0.01, 1000.00]. 
		@warn: scale is only relevant for ortho (L{getType}) cameras.
		"""

	def getClipStart():
		"""
		Get the clip start value. (B{deprecated}) See the L{clipStart} attribute.
		@rtype: float
		"""

	def setClipStart(clipstart):
		"""
		Set the clip start value. (B{deprecated}) See the L{clipStart} attribute.
		@type clipstart: float
		@param clipstart: The new lens value.
		"""

	def getClipEnd():
		"""
		Get the clip end value. (B{deprecated}) See the L{clipEnd} attribute.
		@rtype: float
		"""

	def setClipEnd(clipend):
		"""
		Set the clip end value. (B{deprecated}) See the L{clipEnd} attribute.
		@type clipend: float
		@param clipend: The new clip end value.
		"""

	def getDrawSize():
		"""
		Get the draw size value. (B{deprecated}) See the L{drawSize} attribute.
		@rtype: float
		"""

	def setDrawSize(drawsize):
		"""
		Set the draw size value. (B{deprecated}) See the L{drawSize} attribute.
		@type drawsize: float
		@param drawsize: The new draw size value.
		"""

	def getScriptLinks (event):
		"""
		Get a list with this Camera's script links of type 'event'.
		@type event: string
		@param event: "FrameChanged", "Redraw" or "Render".
		@rtype: list
		@return: a list with Blender L{Text} names (the script links of the given
			  'event' type) or None if there are no script links at all.
		"""

	def clearScriptLinks (links = None):
		"""
		Delete script links from this Camera.  If no list is specified, all
		script links are deleted.
		@type links: list of strings
		@param links: None (default) or a list of Blender L{Text} names.
		"""

	def addScriptLink (text, event):
		"""
		Add a new script link to this Camera.
		@type text: string
		@param text: the name of an existing Blender L{Text}.
		@type event: string
		@param event: "FrameChanged", "Redraw" or "Render".
		"""

	def insertIpoKey(keytype):
		"""
		Inserts keytype values in camera ipo at curframe. Uses module constants.
		@type keytype: Integer
		@param keytype:
			     -LENS
			     -CLIPPING
		@return: py_none
		"""  

	def __copy__ ():
		"""
		Make a copy of this camera
		@rtype: Camera
		@return:  a copy of this camera
		"""

import id_generics
Camera.__doc__ += id_generics.attributes
