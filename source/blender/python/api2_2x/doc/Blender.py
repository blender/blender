# The Blender Module

# The module files in this folder are used to create the API documentation.
# Doc system used: epydoc - http://epydoc.sf.net
# command line: epydoc -t Blender.py -n "Blender" --no-private MODULES
# where MODULES is a list of all such modules in this folder, like:
# Blender.py Camera.py Lamp.py BGL.py etc.

"""
This is the main Blender module.

Blender Python
==============

 Submodules:
 -----------

  - L{Material}
  - L{Camera}
  - L{Lamp}
  - L{BGL}
  - L{Window}
  - L{Text}

 Introduction:
 -------------
  - What scripting is, why have it
  - What Python is, links
  - More about what scripting can give us
  - Overview of the Blender Python modules
  - Links to Blender, Blender Python, later: script lists

@author: The Blender Python Team
@requires: Blender 2.28 or newer.
@version: 0.1
@see: U{www.blender.org<http://www.blender.org>}
@see: U{projects.blender.org<http://projects.blender.org>}
"""

def Set (request, data):
  """
  Update settings in Blender
  @type request: string
  @param request: The setting to change:
      - 'curframe': the current animation frame
  @type data: int
  @param data: The new value.
  """

def Get (request):
  """
  Retrieve settings from Blender.
  @type request: string
  @param request: The setting data to be returned:
      - 'curframe': the current animation frame
      - 'curtime' : the current animation time
      - 'staframe': the start frame of the animation
      - 'endframe': the end frame of the animation
      - 'filename': the name of the last file read or written
      - 'version' : the Blender version number
  @return: The requested data.
  """

def Redraw ():
  """
  Redraw all 3D windows
  """

def ReleaseGlobalDict (bool = None):
  """
  Define whether the global Python Interpreter dictionary should be cleared
  after the script is run.  Default is to clear (non-zero bool).
  @type bool: an int, actually
  @param bool: The flag to release (non-zero bool) or not (bool = 0) the dict.
      if no argument is passed, this function simply returns the current
      behavior.
  @rtype: int
  @return: A bool value (0 or 1) telling the current behavior.
  """
