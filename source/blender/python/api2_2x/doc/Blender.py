# The Blender Module

# The module files in this folder are used to create the API documentation.
# Doc system used: epydoc - http://epydoc.sf.net
# command line:

# epydoc -o BPY_API_230 --url "http://www.blender.org" -t Blender.py \
# -n "Blender" --no-private --no-frames Blender.py \
# Types.py Scene.py Object.py NMesh.py Material.py Camera.py Lamp.py \
# Armature.py Metaball.py Effect.py Curve.py Ipo.py World.py BGL.py Window.py \
# Draw.py Image.py Text.py Lattice.py Texture.py Registry.py Sys.py Mathutils.py

"""
The main Blender module.

The Blender Python API Reference
================================

 Submodules:
 -----------

  - L{Armature}
     - L{Bone}
     - L{NLA}
  - L{BGL}
  - L{Camera}
  - L{Curve}
  - L{Draw}
  - L{Effect}
  - L{Image}
  - L{Ipo}
  - L{Lamp}
  - L{Lattice}
  - L{Library}
  - L{Material}
  - L{Mathutils}
  - L{Metaball}
  - L{NMesh}
  - L{Noise}
  - L{Object}
  - L{Registry}
  - L{Scene}
     - L{Render}
  - L{Text}
  - L{Texture}
  - L{Types}
  - L{Window}
  - L{World}
  - L{sys<Sys>}

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
@requires: Blender 2.33+ or newer.
@version: 2.33a-cvs
@see: U{www.blender.org<http://www.blender.org>}
@see: U{projects.blender.org<http://projects.blender.org>}
@see: U{www.python.org<http://www.python.org>}
@see: U{www.python.org/doc<http://www.python.org/doc>}
"""

def Set (request, data):
  """
  Update settings in Blender.
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
      - 'datadir' : the path to the dir where scripts should store and
            retrieve their data files, including saved configuration (can
            be None, if not found).
      - 'version' : the Blender version number
  @return: The requested data.
  """

def Redraw ():
  """
  Redraw all 3D windows.
  """

def Load (filename = None):
  """
  Load a Blender .blend file.
  @type filename: string
  @param filename: the pathname to the desired .blend file.  If 'filename'
      isn't given or if it contains the substring '.B.blend', the default
      .B.blend file is loaded.

  @warn: loading a new .blend file removes the current data in Blender.  For
     safety, this function saves the current data as an autosave file in
     the temporary dir used by Blender before loading the new file.
  """

def Quit ():
  """
  Exit from Blender immediately.
  @warn: the use of this function should obviously be avoided, it is available
     because there are some cases where it can be useful, like in automated
     tests.  For safety, a "quit.blend" file is saved (normal Blender behavior
     upon exiting) when this function is called, so the data in Blender isn't
     lost.
  """
