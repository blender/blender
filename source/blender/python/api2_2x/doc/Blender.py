# The Blender Module

# The module files in this folder are used to create the API documentation.
# Doc system used: epydoc - http://epydoc.sf.net
# command line:

# epydoc -o BPY_API_23x --url "http://www.blender.org" -t Blender.py \
# -n "Blender" --no-private --no-frames Blender.py \
# Types.py Scene.py Object.py NMesh.py Material.py Camera.py Lamp.py \
# Armature.py Metaball.py Effect.py Curve.py Ipo.py World.py BGL.py Window.py \
# Draw.py Image.py Text.py Lattice.py Texture.py Registry.py Sys.py Mathutils.py

"""
The main Blender module (*).

The Blender Python API Reference
================================

 Submodules:
 -----------

  - L{Armature}
     - L{Bone}
     - L{NLA}
  - L{BGL}
  - L{Camera} (*)
  - L{Curve}
  - L{Draw} (*)
  - L{Effect}
  - L{Image} (*)
  - L{Ipo}
  - L{Lamp} (*)
  - L{Lattice}
  - L{Library}
  - L{Material} (*)
  - L{Mathutils}
  - L{Metaball} (*)
  - L{NMesh}
  - L{Noise}
  - L{Object} (*)
  - L{Registry}
  - L{Scene} (*)
     - L{Render}
  - L{Text}
  - L{Texture}
  - L{Types}
  - L{Window} (* important: L{Window.EditMode})
  - L{World} (*)
  - L{sys<Sys>} (*)

 (*) - marks updated.

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
@requires: Blender 2.34 or newer.
@version: 2.34
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
  Load a Blender .blend file or any of the other supported file formats.

  Supported formats:
    - Blender's .blend;
    - DXF;
    - Open Inventor 1.0 ASCII;
    - Radiogour;
    - STL;
    - Videoscape;
    - VRML 1.0 asc.

  @type filename: string
  @param filename: the pathname to the desired file.  If 'filename'
      isn't given or if it contains the substring '.B.blend', the default
      .B.blend file is loaded.

  @warn: loading a new .blend file removes the current data in Blender.  For
     safety, this function saves the current data as an autosave file in
     the temporary dir used by Blender before loading a new Blender file.
  @warn: after a call to Load(blendfile), current data in Blender is lost,
     including the Python dictionaries.  Any posterior references in the
     script to previously defined data will generate a NameError.  So it's
     better to put Blender.Load as the last executed command in the script,
     when this function is used to open .blend files.
  """

def Save (filename, overwrite = 0):
  """
  Save a Blender .blend file with the current program data or export to
  one of the builtin file formats.
  
  Supported formats:
    - Blender (.blend);
    - DXF (.dxf);
    - STL (.stl);
    - Videoscape (.obj);
    - VRML 1.0 (.wrl).

  @type filename: string
  @param filename: the filename for the file to be written.  It must have one
      of the supported extensions or an error will be returned.
  @type overwrite: int (bool)
  @param overwrite: if non-zero, file 'filename' will be overwritten if it
      already exists.  By default existing files are not overwritten (an error
      is returned).

  @note: The substring ".B.blend" is not accepted inside 'filename'.
  @note: DXF, STL and Videoscape export only B{selected} meshes.
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
