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
	
	@type x, y: integer
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

