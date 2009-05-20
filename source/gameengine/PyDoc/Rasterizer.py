# $Id$
"""
Documentation for the Rasterizer module.

Example Uses an L{SCA_MouseSensor}, and two L{KX_ObjectActuator}s to implement MouseLook::
	# To use a mouse movement sensor "Mouse" and a 
	# motion actuator to mouse look:
	import Rasterizer
	import GameLogic

	# SCALE sets the speed of motion
	SCALE=[1, 0.5]
	
	co = GameLogic.getCurrentController()
	obj = co.getOwner()
	mouse = co.getSensor("Mouse")
	lmotion = co.getActuator("LMove")
	wmotion = co.getActuator("WMove")
	
	# Transform the mouse coordinates to see how far the mouse has moved.
	def mousePos():
		x = (Rasterizer.getWindowWidth()/2 - mouse.getXPosition())*SCALE[0]
		y = (Rasterizer.getWindowHeight()/2 - mouse.getYPosition())*SCALE[1]
		return (x, y)
	
	pos = mousePos()
	
	# Set the amount of motion: X is applied in world coordinates...
	lmotion.setTorque(0.0, 0.0, pos[0], False)
	# ...Y is applied in local coordinates
	wmotion.setTorque(-pos[1], 0.0, 0.0, True)
	
	# Activate both actuators
	GameLogic.addActiveActuator(lmotion, True)
	GameLogic.addActiveActuator(wmotion, True)
	
	# Centre the mouse
	Rasterizer.setMousePosition(Rasterizer.getWindowWidth()/2, Rasterizer.getWindowHeight()/2)

@group Material Types: KX_TEXFACE_MATERIAL, KX_BLENDER_MULTITEX_MATERIAL, KX_BLENDER_GLSL_MATERIAL
@var KX_TEXFACE_MATERIAL: Materials as defined by the texture face settings.
@var KX_BLENDER_MULTITEX_MATERIAL: Materials approximating blender materials with multitexturing.
@var KX_BLENDER_GLSL_MATERIAL: Materials approximating blender materials with GLSL.

"""
def getWindowWidth():
	"""
	Gets the width of the window (in pixels)
	
	@rtype: integer
	"""
def getWindowHeight():
	"""
	Gets the height of the window (in pixels)
	
	@rtype: integer
	"""
def makeScreenshot(filename):
	"""
	Writes a screenshot to the given filename.
	
	If filename starts with // the image will be saved relative to the current directory.
	If the filename contains # it will be replaced with the frame number.
	
	The standalone player saves .png files. It does not support colour space conversion 
	or gamma correction.
	
	When run from Blender, makeScreenshot supports Iris, IrisZ, TGA, Raw TGA, PNG, HamX, and Jpeg.
	Gamma, Colourspace conversion and Jpeg compression are taken from the Render settings panels.
	
	@type filename: string
	"""

def enableVisibility(visible):
	"""
	Doesn't really do anything...
	"""

def showMouse(visible):
	"""
	Enables or disables the operating system mouse cursor.
	
	@type visible: boolean
	"""

def setMousePosition(x, y):
	"""
	Sets the mouse cursor position.
	
	@type x: integer
	@type y: integer
	"""

def setBackgroundColor(rgba):
	"""
	Sets the window background colour.
	
	@type rgba: list [r, g, b, a]
	"""

def setMistColor(rgb):
	"""
	Sets the mist colour.
	
	@type rgb: list [r, g, b]
	"""
	
def setAmbientColor(rgb):
	"""
	Sets the color of ambient light.
	
	@type rgb: list [r, g, b]
	"""

def setMistStart(start):
	"""
	Sets the mist start value.  Objects further away than start will have mist applied to them.
	
	@type start: float
	"""

def setMistEnd(end):
	"""
	Sets the mist end value.  Objects further away from this will be coloured solid with
	the colour set by setMistColor().
	
	@type end: float
	"""
	
def disableMist():
	"""
	Disables mist.
	
	@note: Set any of the mist properties to enable mist.
	"""
	
def setEyeSeparation(eyesep):
	"""
	Sets the eye separation for stereo mode.
	
	@param eyesep: The distance between the left and right eye.
	@type eyesep: float
	"""

def getEyeSeparation():
	"""
	Gets the current eye separation for stereo mode.
	
	@rtype: float
	"""
	
def setFocalLength(focallength):
	"""
	Sets the focal length for stereo mode.
	
	@param focallength: The focal length.  
	@type focallength: float
	"""

def getFocalLength():
	"""
	Gets the current focal length for stereo mode.
	
	@rtype: float
	"""

def setMaterialMode(mode):
	"""
	Set the material mode to use for OpenGL rendering.
	
	@type mode: KX_TEXFACE_MATERIAL, KX_BLENDER_MULTITEX_MATERIAL, KX_BLENDER_GLSL_MATERIAL
	@note: Changes will only affect newly created scenes.
	"""

def getMaterialMode(mode):
	"""
	Get the material mode to use for OpenGL rendering.
	
	@rtype: KX_TEXFACE_MATERIAL, KX_BLENDER_MULTITEX_MATERIAL, KX_BLENDER_GLSL_MATERIAL
	"""

def setGLSLMaterialSetting(setting, enable):
	"""
	Enables or disables a GLSL material setting.
	
	@type setting: string (lights, shaders, shadows, ramps, nodes, extra_textures)
	@type enable: boolean
	"""

def getGLSLMaterialSetting(setting, enable):
	"""
	Get the state of a GLSL material setting.
	
	@type setting: string (lights, shaders, shadows, ramps, nodes, extra_textures)
	@rtype: boolean
	"""

def drawLine(fromVec,toVec,color):
	"""
	Draw a line in the 3D scene.
	
	@param fromVec: the origin of the line
	@type fromVec: list [x, y, z]
	@param toVec: the end of the line
	@type toVec: list [x, y, z]
	@param color: the color of the line
	@type color: list [r, g, b]
	"""

def enableMotionBlur(factor):
	"""
	Enable the motion blue effect.
	
	@param factor: the ammount of motion blur to display.
	@type factor: float [0.0 - 1.0]
	"""

def disableMotionBlur():
	"""
	Disable the motion blue effect.
	"""
