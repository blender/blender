"""The Blender Window module

This module currently only supports redrawing commands of windows.
Later on, it will allow screen manipulations and access to Window
properties"""

import _Blender.Window as _Window

t = _Window.Types 
Const = t # emulation

Types = { 'View'     : t.VIEW3D,
          'Ipo'      : t.IPO,
          'Oops'     : t.OOPS,
          'Button'   : t.BUTS,
          'File'     : t.FILE,
          'Image'    : t.IMAGE,
          'Text'     : t.TEXT,
          'Action'   : t.ACTION,
        }

del t

def Redraw(t= 'View'):
	"""Redraws all windows of the type 't' which must be one of:

* "View"   - The 3D view

* "Ipo"    - The Ipo Window

* "Oops"   - The OOPS (scenegraph) window

* "Button" - The Button Window

* "File"   - The File Window

* "Image"  - The Image Window (UV editor)

* "Text"   - The Text editor

* "Action" - The Action Window"""

	if type(t) == type(1):
		return _Window.Redraw(t)
	try:
		_Window.Redraw(Types[t])
	except:
		raise TypeError, "type must be one of %s" % Types.keys()

def RedrawAll():
	"""Redraws the whole screen"""
	_Window.RedrawAll()

def drawProgressBar(val, text):
	"""Draws a progress bar behind the Blender version information.
'val' is a float value <= 1.0, 'text' contains info about what is currently
being done.
This function must be called with 'val' = 0.0 at start and end of the executed
(and probably time consuming) action.
The user may cancel the progress with the 'Esc' key, in this case, 0 is returned,
1 else."""
	return _Window.draw_progressbar(val, text)

draw_progressbar = _Window.draw_progressbar # emulation
QRedrawAll = _Window.QRedrawAll
