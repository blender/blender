# The Blender Module

# The module files in this folder are used to create the API documentation.
# Doc system used: epydoc - http://epydoc.sf.net
# pseudo command line (check the epy_docgen.sh file):
# epydoc -o BPY_API_23x --url "http://www.blender.org" -t Blender.py \
# -n "Blender" --no-private --no-frames Blender.py \
# Types.py Scene.py Object.py [ ... etc]

"""
The main Blender module.

B{New}: L{Run}, L{UpdateMenus}, new options to L{Get}.

Blender
=======

@type bylink: bool
@var bylink: True if the current script is being executed as a script link.
@type link: Blender Object or None
@var link: if this script is a running script link, 'link' points to the
    linked Object (can be a scene, object (mesh, camera, lamp), material or
    world).  If this is not a script link, 'link' is None.
@type event: string
@var event: if this script is a running script link, 'event' tells what
    kind of link triggered it (ex: OnLoad, FrameChanged, Redraw, etc.).
@type mode: string
@var mode: Blender's current mode:
    - 'interactive': normal mode, with an open window answering to user input;
    - 'background': Blender was started as 'C{blender -b <blender file>}' and
      will exit as soon as it finishes rendering or executing a script
      (ex: 'C{blender -b <blender file> -P <script>}').  Try 'C{blender -h}'
      for more detailed informations.
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
      - 'curframe': the current animation frame.
      - 'curtime' : the current animation time.
      - 'staframe': the start frame of the animation.
      - 'endframe': the end frame of the animation.
      - 'filename': the name of the last file read or written.
      - 'homedir':  Blender's home dir.
      - 'datadir' : the path to the dir where scripts should store and
            retrieve their data files, including saved configuration (can
            be None, if not found).
      - 'udatadir': the path to the user defined data dir.  This may not be
            available (is None if not found), but users that define uscriptsdir
            have a place for their own scripts and script data that won't be
            erased when a new version of Blender is installed.
      - 'scriptsdir': the path to the main dir where scripts are stored
            (can be None, if not found).
      - 'uscriptsdir': the path to the user defined dir for scripts, see
            the paths tab in the User Preferences window in Blender
            (can be None, if not found).
      - 'version' : the Blender version number.
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

def Run (script):
  """
  Execute the given script.
  @type script: string
  @param script: the name of an available Blender Text (use L{Text.Get}() to
      get a complete list) or the full pathname to a Python script file in the
      system.
  @note: the script is executed in its own context (with its own global
      dictionary), as if you had called it with ALT+P or chosen from a menu.
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
