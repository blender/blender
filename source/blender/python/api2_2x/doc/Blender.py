# The Blender Module

# The module files in this folder are used to create the API documentation.
# Doc system used: epydoc - http://epydoc.sf.net
# pseudo command line (check the epy_docgen.sh file):
# epydoc -o BPY_API_23x --url "http://www.blender.org" -t Blender.py \
# -n "Blender" --no-private --no-frames Blender.py \
# Types.py Scene.py Object.py [ ... etc]

"""
The main Blender module.

B{New}: L{UpdateMenus}.

Blender
=======
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
      - 'homedir':  Blender's home dir
      - 'datadir' : the path to the dir where scripts should store and
            retrieve their data files, including saved configuration (can
            be None, if not found).
      - 'scriptsdir': the path to the main dir where scripts are stored
            (can be None, if not found).
      - 'uscriptsdir': the path to the user defined dir for scripts, see
            the paths tab in the User Preferences window in Blender
            (can be None, if not found).
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
  @warn: if in edit mode, this function leaves it, since Blender itself
     requires that.
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

def UpdateMenus ():
  """
  Update the menus that list registered scripts.  This will scan the default
  and user defined (if available) folder(s) for scripts that have registration
  data and will make them accessible via menus.
  @note: only scripts that save other new scripts in the default or user
    defined folders need to call this function.
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
