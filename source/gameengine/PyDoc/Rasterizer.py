# $Id$
"""
Documentation for the Rasterizer module.

Usage:
import Rasterizer
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

