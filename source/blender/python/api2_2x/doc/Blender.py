# The Blender Module

# The module files in this folder are used to create the API documentation.
# Doc system used: epydoc - http://epydoc.sf.net
# pseudo command line (check the epy_docgen.sh file):
# epydoc -o BPY_API_23x --url "http://www.blender.org" -t Blender.py \
# -n "Blender" --no-private --no-frames Blender.py \
# Types.py Scene.py Object.py [ ... etc]

"""
The main Blender module.

B{New}: L{Run}, L{UpdateMenus}, new options to L{Get}, L{ShowHelp},
L{SpaceHandlers} dictionary.
L{UnpackModes} dictionary.

Blender
=======

@type bylink: bool
@var bylink: True if the current script is being executed as a script link.
@type link: Blender Object or None; integer (space handlers)
@var link: for normal script links, 'link' points to the linked Object (can be
    a scene, object (mesh, camera, lamp), material or
    world).  For space handler script links, 'link' is an integer from the
    Blender.L{SpaceHandlers} dictionary.  For script not running as script
    links, 'link' is None.
@type event: string or int
@var event: this has three possible uses: script link type or events callback
    ascii value:
      - for normal script links it is a string representing the link type
        (OnLoad, FrameChanged, Redraw, etc.).
      - for EVENT space handler script links it is the passed event.
      - for normal L{GUI<Draw.Register>} scripts I{during the events callback},
        it holds the ascii value of the current event, if it is a valid one.
        Users interested in this should also check the builtin 'ord' and 'chr'
        Python functions. 
@type mode: string
@var mode: Blender's current mode:
    - 'interactive': normal mode, with an open window answering to user input;
    - 'background': Blender was started as 'C{blender -b <blender file>}' and
      will exit as soon as it finishes rendering or executing a script
      (ex: 'C{blender -b <blender file> -P <script>}').  Try 'C{blender -h}'
      for more detailed informations.
@type UnpackModes: constant dictionary
@var UnpackModes: dictionary with available unpack modes.
    - USE_LOCAL - use files in current directory (create when necessary)
    - WRITE_LOCAL - write files in current directory (overwrite when necessary)
    - USE_ORIGINAL - use files in original location (create when necessary)
    - WRITE_ORIGINAL - write files in original location (overwrite when necessary)
@type SpaceHandlers: constant dictionary
@var SpaceHandlers: dictionary with space handler types.
    - VIEW3D_EVENT;
    - VIEW3D_DRAW.
"""

def Set (request, data):
  """
  Update settings in Blender.
  @type request: string
  @param request: The setting to change:
      - 'curframe': the current animation frame
      - 'compressfile' : compress file writing a blend file (Use a boolean value True/False).
      - 'uscriptsdir': user scripts dir
      - 'yfexportdir': yafray temp xml storage dir
      - 'fontsdir': font dir
      - 'texturesdir': textures dir
      - 'seqpluginsdir': sequencer plugin dir
      - 'renderdir': default render output dir
      - 'soundsdir': sound dir
      - 'tempdir': temp file storage dir
	  - 'mipmap' : Use mipmapping in the 3d view (Use a boolean value True/False).
  @type data: int or string
  @param data: The new value.
  """

def Get (request):
  """
  Retrieve settings from Blender.
  @type request: string
  @param request: The setting data to be returned:
      - 'curframe': the current animation frame.
      - 'curtime' : the current animation time.
      - 'compressfile' : compress setting from the file menu, return  0 for false or 1 for true.
      - 'staframe': the start frame of the animation.
      - 'endframe': the end frame of the animation.
      - 'rt': the value of the 'rt' button for general debugging
      - 'filename': the name of the last file read or written.
      - 'homedir':  Blender's home directory.
      - 'datadir' : the path to the dir where scripts should store and
            retrieve their data files, including saved configuration (can
            be None, if not found).
      - 'udatadir': the path to the user defined data dir.  This may not be
            available (is None if not found), but users that define uscriptsdir
            have a place for their own scripts and script data that won't be
            erased when a new version of Blender is installed.  For this reason
            we recommend scripts check this dir first and use it, if available.
      - 'scriptsdir': the path to the main dir where scripts are stored.
      - 'uscriptsdir': the path to the user defined dir for scripts. (*)
      - 'icondir': the path to blenders icon theme files.
      - 'yfexportdir': the path to the user defined dir for yafray export. (*)
      - 'fontsdir': the path to the user defined dir for fonts. (*)
      - 'texturesdir': the path to the user defined dir for textures. (*)
      - 'texpluginsdir': the path to the user defined dir for texture plugins. (*)
      - 'seqpluginsdir': the path to the user defined dir for sequence plugins. (*)
      - 'renderdir': the path to the user defined dir for render output. (*)
      - 'soundsdir': the path to the user defined dir for sound files. (*)
      - 'tempdir': the path to the user defined dir for storage of Blender
            temporary files. (*)
	  - 'mipmap' : Use mipmapping in the 3d view. (*)
      - 'version' : the Blender version number.
  @note: (*) these can be set in Blender at the User Preferences window -> File
      Paths tab.
  @warn: this function returns None for requested dir paths that have not been
      set or do not exist in the user's file system.
  @return: The requested data or None if not found.
  """

def GetPaths (absolute=0):
  """
  Returns a list of files this blend file uses: (libraries, images, sounds, fonts, sequencer movies).
  @type absolute: bool
  @param absolute: When true, the absolute paths of every file will be returned.
  @return: A list for paths (strings) that this blend file uses.
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
     safety, this function saves the current data as an auto-save file in
     the temporary dir used by Blender before loading a new Blender file.
  @warn: after a call to Load(blendfile), current data in Blender is lost,
     including the Python dictionaries.  Any posterior references in the
     script to previously defined data will generate a NameError.  So it's
     better to put Blender.Load as the last executed command in the script,
     when this function is used to open .blend files.
  @warn: if in edit mode, this function leaves it, since Blender itself
     requires that.
  @note: for all types except .blend files, this function only works in
     interactive mode, not in background, following what Blender itself does.
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
      already exists (can be checked with L{Blender.sys.exists<Sys.exists>}.
      By default existing files are not overwritten (an error is returned).

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
  @note: the script is executed in its own context -- with its own global
      dictionary -- as if it had been executed from the Text Editor or chosen
      from a menu.
  """

def ShowHelp (script):
  """
  Show help for the given script.  This is a time-saver ("code-saver") for
  scripts that need to feature a 'help' button in their GUIs or a 'help'
  submenu option.  With proper documentation strings, calling this function is
  enough to present a screen with help information plus link and email buttons.
  @type script: string
  @param script: the filename of a registered Python script.
  @note: this function uses L{Run} and the "Scripts Help Browser" script.  This
     means that it expects proper doc strings in the script to be able to show
     help for it (otherwise it offers to load the script source code as text).
     The necessary information about doc strings is L{given here<API_related>}.
  @note: 'script' doesn't need to be a full path name: "filename.py" is enough.
     Note, though, that this function only works for properly registered
     scripts (those that appear in menus).
  """

def UpdateMenus ():
  """
  Update the menus that list registered scripts.  This will scan the default
  and user defined (if available) folder(s) for scripts that have registration
  data and will make them accessible via menus.
  @note: only scripts that save other new scripts in the default or user
    defined folders need to call this function.
  """
def UnpackAll (mode):
  """
  Unpack all files with specified mode.
  @param mode: The Mode for unpacking. Must be one of the modes in 
  Blender.UnpackModes dictionary.
  @type mode: int
  """
def PackAll ():
  """
  Pack all files.
  """

def CountPackedFiles():
  """
  Returns the number of packed files.
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
def SaveUndoState (message):
  """
  Sets an undo at the current state.
  @param message: Message that appiers in the undo menu
  @type message: string
  """
