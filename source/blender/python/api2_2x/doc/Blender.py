# The Blender Module

# The module files in this folder are used to create the API documentation.
# Doc system used: epydoc - http://epydoc.sf.net
# command line:

# epydoc -o BPY_API_228 --url "http://www.blender.org" -t Blender.py \
# -n "Blender" --no-private --no-frames Blender.py \
# Types.py Scene.py Object.py NMesh.py Material.py Camera.py Lamp.py \
# Armature.py Metaball.py Effect.py Curve.py Ipo.py World.py BGL.py Window.py \
# Draw.py Image.py Text.py

"""
The main Blender module.

The Blender Python API Reference
================================

 Submodules:
 -----------

  - L{Types}
  - L{Scene}
  - L{Object}
  - L{NMesh}
  - L{Material}
  - L{Armature}
  - L{Camera}
  - L{Lamp}
  - L{World}
  - L{Metaball}
  - L{Effect}
  - L{Curve}
  - L{Ipo}
  - L{BGL}
  - L{Draw}
  - L{Window}
  - L{Image}
  - L{Text}

 Introduction:
 -------------

 This Reference documents the Blender Python API, a growing collection of
 Python modules (libs) that give access to part of the program's internal data
 and functions.
 
 Through scripting, Blender can be extended in realtime.  Possibilities range
 from simple functionality to importers / exporters and even more complex
 "applications".  Blender scripts are written in
 U{Python <www.python.org>}, an impressive high level, multi-paradigm,
 open-source language.

@author: The Blender Python Team
@requires: Blender 2.28 pre-release or newer.
@version: 0.1
@see: U{www.blender.org<http://www.blender.org>}
@see: U{projects.blender.org<http://projects.blender.org>}
@see: U{www.python.org<http://www.python.org>}
@see: U{www.python.org/doc<http://www.python.org/doc>}
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
