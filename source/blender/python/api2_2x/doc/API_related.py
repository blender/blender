# This is not a real module, it's simply an introductory text.

"""
Blender Python related features
===============================

 L{Back to Main Page<API_intro>}


Introduction:
=============

 This page describes special features available to BPython scripts:

   - Command line mode is accessible with the '-P' and '-b' Blender options.
   - Registration allows scripts to become available from some pre-defined menus
     in Blender, like Import, Export, Wizards and so on.
   - Script links are Blender Texts (scripts) executed when a particular event
     (redraws, .blend file loading, saving, frame changed, etc.) occurs.  Now
     there are also "Space Handlers" to draw onto or get events from a given
     space (only 3D View now) in some window.
   - Proper documentation data is used by the 'Scripts Help Browser' script to
     show help information for any registered script.  Your own GUI can use
     this facility with the L{Blender.ShowHelp} function.
   - Configuration is for data in your script that can be tweaked according to
     user taste or needs.  Like documentation, this is another helper
     functionality -- you don't need to provide a GUI yourself to edit config
     data.


 Command line usage:
 ===================

 Specifying scripts:
 -------------------

 The '-P' option followed either by:
   - a script filename (full pathname if not in the same folder where you run
     the command);
   - the name of a Text in a .blend file (that must also be specified)
 will open Blender and immediately run the given script.

 Example::

  # open Blender and execute the given script:
  blender -P script.py

 Passing parameters:
 -------------------

 To pass parameters to the script you can:
    - write them to a file before running Blender, then make your script parse that file;
    - set environment variables and access them with the 'os' module:

 Examples with parameters being passed to the script via command line::

  # execute a command like:

  myvar=value blender -P script.py

  # and in script.py access myvar with os.getenv
  # (os.environ and os.setenv are also useful):

  # script.py:
  import os
  val = os.getenv('myvar')

  # To pass multiple parameters, simply write them in sequence,
  # separated by spaces:

  myvar1=value1 myvar2=value2 mystr="some string data" blender -P script.py

 Background mode:
 ----------------

 In '-b' mode no windows will be opened: the program will run as a command
 line tool able to render stills and animations and execute any working Python
 script with complete access to loaded .blend's file contents.  Once the task
 is completed, the program will exit.

 Background mode examples::

  # Open Blender in background mode with file 'myfile.blend'
  # and run the script 'script.py':

  blender -b myfile.blend -P script.py

  # Note: a .blend file is always required.  'script.py' can be a file
  # in the file system or a Blender Text stored in 'myfile.blend'.

  # Let's assume 'script.py' has code to render the current frame;
  # this line will set the [s]tart and [e]nd (and so the current) frame to
  # frame 44 and call the script:

  blender -b myfile.blend -s 44 -e 44 -P script.py

  # Using now a script written to render animations, we set different
  # start and end frames and then execute this line:

  blender -b myfile.blend -s 1 -e 10 -P script.py

  # Note: we can also set frames and define if we want a single image or
  # an animation in the script body itself, naturally.

 The rendered pictures will be written to the default render folder, that can
 also be set via BPython (take a look at L{Render.RenderData}).  Their
 names will be the equivalent frame number followed by the extension of the
 chosen image type: 0001.png, for example.  To rename them to something else,
 coders can use the C{rename} function in the standard 'os' Python module.

 Reminder: if you just need to render, it's not necessary to have a script.
 Blender can create stills and animations with its own command line arguments.
 Example:
  - a single image at frame 44: blender -b myfile.blend -f 44
  - an animation from frame 1 to 10: blender -b myfile.blend -s 1 -e 10 -a


 Script links:
 =============

 Object script links:
 --------------------

 Users can link Blender Text scripts and objects to have the script
 code executed when specific events occur to the objects.  For example, if a
 Camera has an script link set to "FrameChanged", the script will be executed
 whenever the current frame is changed.  Links can either be manually added by
 users on the Buttons window -> Scripts tab or created by another script (see,
 for example, L{Object.addScriptLink<Object.Object.addScriptLink>}). 

 These are the types which can be linked to scripts:
  - Camera Data;
  - Lamp Data;
  - Materials;
  - Objects;
  - Scenes;
  - Worlds.

 And these are the available event choices:
  - Redraw;
  - FrameChanged;
  - Render;
  - OnLoad (*);
  - OnSave (*).

 (*) only available for scenes

 There are three L{Blender} module variables that script link authors should
 be aware of:
  - B{bylink}: True if the script is running as a script link;
  - B{link}: the object the running script was linked to (None if this is
    not a script link);
  - B{event}: the event type, if the running script is being executed as a
    script link.

 Example::
  #script link
  import Blender
  if Blender.bylink: # we're running as a script link
    print "Event: %s for %s" % (Blender.event, Blender.link)

 B{Important note about "Render" events}:

 Each "Render" script link is executed twice: before rendering and after, for
 reverting changes and for possible clean up actions.  Before rendering,
 'Blender.event' will be "Render" and after rendering it will be "PostRender".

 Example::
  # render script link
  import Blender
  event = Blender.event
  if event == "Render":
    # prepare for rendering
    create_my_very_detailed_mesh_data()
  elif event == "PostRender":
    # done rendering, clean up
    delete_my_very_detailed_mesh_data()

 As suggested by the example above, this is especially useful for script links
 that need to generate data only useful while rendering, or in case they need
 to switch between two mesh data objects, one meant for realtime display and
 the other, more detailed, for renders.

 Space Handler script links:
 ---------------------------

 This is a new kind of script linked to spaces in a given window.  Right now
 only the 3D View has the necessary hooks, but the plan is to add access to
 other types, too.  Just to clarify naming conventions: in Blender, a screen
 is partitioned in windows (also called areas) and each window can show any
 space.  Spaces are: 3D View, Text Editor, Scripts, Buttons, User Preferences,
 Oops, etc. 

 Space handlers are texts in the Text Editor, like other script links, but they
 need to have a special header to be recognized -- B{I{the first line in the
 text file}} must inform:
  1. that they are space handlers;
  2. the space they belong to;
  3. whether they are EVENT or DRAW handlers.

 Example header for a 3D View EVENT handler::

  # SPACEHANDLER.VIEW3D.EVENT

 Example header for a 3D View DRAW handler::

  # SPACEHANDLER.VIEW3D.DRAW

 Available space handlers can be toggled "on" or "off" in the space header's
 B{View->Space Handler Scripts} submenu, by the user.

 EVENT space handler scripts are called by that space's event handling callback
 in Blender.  The script receives the event B{before} it is further processed
 by the program.  An EVENT handler script should check Blender.event (compare
 it against L{Draw} events) and either:
  - process it (the script must set Blender.event to None then);
  - ignore it.

 Setting C{Blender.event = None} tells Blender not to go on processing itself
 the event, because it was grabbed by the script.

 Example::

  # SPACEHANDLER.VIEW3D.EVENT

  import Blender
  from Blender import Draw
  evt = Blender.event
  return_it = False

  if evt == Draw.LEFTMOUSE:
    print "Swallowing the left mouse button press"
  elif evt == Draw.AKEY:
    print "Swallowing an 'a' character"
  else:
    print "Let the 3D View itself process this event:", evt
    return_it = True

  # if Blender should not process itself the passed event:
  if not return_it: Blender.event = None

 DRAW space handlers are called by that space's drawing callback in Blender.
 The script is called B{after} the space has been drawn.

 Two of the L{Blender} module variables related to script links assume
 different roles for space handlers:
  - B{bylink} is the same: True if the script is running as a script link;
  - B{link}: integer from the L{Blender}.SpaceHandlers constant dictionary,
    tells what space this handler belongs to and the handler's type
    (EVENT, DRAW);
  - B{event}:
     - EVENT handlers: an input event (check keys and mouse events in L{Draw})
       to be processed or ignored.
     - DRAW handlers: 0 always.

 B{Guidelines (important)}:
  - EVENT handlers can access and change Blender objects just like any other
    script, but they should not draw to the screen, B{use a DRAW handler to do
    that}.  Specifically: L{Draw.Image} and the L{BGL} drawing functions
    should not be used inside an EVENT handler.
  - DRAW handlers should leave the space in the same state it was before they
    were executed.  OpenGL attributes and the modelview and projection matrices
    are automatically saved (pushed) before a DRAW handler runs and restored
    (popped) after it finishes, no need to worry about that.  Draw handlers
    should not grab events;
  - If script handlers need to pass information to each other (for example an
    EVENT handler passing info to a DRAW handler), use the L{Registry} module.
  - in short: use the event handler to deal with events and the draw handler to
    draw and your script will be following the recommended practices for
    Blender code.

 Registering scripts:
 ====================

 To be registered a script needs two things:
   - to be either in the default scripts directory or in the user defined scripts
     path (see User Preferences window -> File Paths tab -> Python path);
   - to have a proper header.

 Try 'blender -d' to know where your default directory for scripts is, it will
 inform either the directory or the file with that info already parsed, which is
 in the same directory of the scripts folder.

 The header should be like this one (all double and single apostrophes below
 are required)::
  #!BPY

  # \"\"\"
  # Name: 'Script Name'
  # Blender: 233
  # Group: 'Export'
  # Submenu: 'All' all
  # Submenu: 'Selected' sel
  # Submenu: 'Configure (gui)' gui
  # Tooltip: 'Export to some format.'
  # \"\"\"

 where:
  - B{Name} is the string that will appear in the menu;
  - B{Blender} is the minimum program version required to run the script;
  - B{Group} defines where the script will be put, see all groups in the
    Scripts Window's header, menu "Scripts";
  - B{Submenu} adds optional submenus for further control;
  - B{Tooltip} is the (short) tooltip string for the menu entry.

 note:
  - all double and single apostrophes above are required;
  - you can "comment out" the header above, by starting lines with
    '#', like we did.  This is not required (except for the first line, #!BPY,
    of course), but this way the header won't conflict with Python tools that
    you can use to generate documentation for your script code.  Just
    remember to keep this header above any other line with triple
    double-quotes (\"\"\") in your script.

 Submenu lines are not required, use them if you want to provide extra
 options.  To see which submenu the user chose, check the "__script__"
 dictionary in your code: __script__['arg'] has the defined keyword (the word
 after the submenu string name: all, sel or gui in the example above) of the
 chosen submenu.  For example, if the user clicked on submenu 'Selected' above,
 __script__['arg'] will be "sel".

 If your script requires extra data or configuration files, there is a special
 folder where they can be saved: see 'datadir' in L{Blender.Get}.


 Documenting scripts:
 ====================

 The "Scripts Help Browser" script in the Help menu can parse special variables
 from registered scripts and display help information for users.  For that,
 authors only need to add proper information to their scripts, after the
 registration header.

 The expected variables:

  - __bpydoc__ (or __doc__) (type: string):
    - The main help text.  Write a first short paragraph explaining what the
      script does, then add the rest of the help text, leaving a blank line
      between each new paragraph.  To force line breaks you can use <br> tags.

  - __author__ (type: string or list of strings):
    - Author name(s).

  - __version__ (type: string):
    - Script version.  A good recommendation is using a version number followed
      by the date in the format YYYY/MM/DD: "1.0 2005/12/31".

  - __url__ (type: string or list of strings):
    - Internet links that are shown as buttons in the help screen.  Clicking
      them opens the user's default browser at the specified location.  The
      expected format for each url entry is e.g.
      "Author's site, http://www.somewhere.com".  The first part, before the
      comma (','), is used as the button's tooltip.  There are two preset
      options: "blender" and "blenderartists.org", which link to the Python forums at
      blender.org and blenderartists.org, respectively.

  - __email__ (optional, type: string or list of strings):
    - Equivalent to __url__, but opens the user's default email client.  You
      can write the email as someone:somewhere*com and the help script will
      substitute accordingly: someone@somewhere.com.  This is only a minor help
      to hide emails from spammers, since your script may be available at some
      site.  "scripts" is the available preset, with the email address of the
      mailing list devoted to scripting in Blender, bf-scripts-dev@blender.org.
      You should only use this one if you are subscribed to the list:
      http://projects.blender.org/mailman/listinfo/bf-scripts-dev for more
      information.

 Example::
   __author__ = 'Mr. Author'
   __version__ = '1.0 2005/01/01'
   __url__ = ["Author's site, http://somewhere.com",
       "Support forum, http://somewhere.com/forum/", "blender", "blenderartists.org"]
   __email__ = ["Mr. Author, mrauthor:somewhere*com", "scripts"]
   __bpydoc__ = \"\"\"\\
   This script does this and that.

   Explaining better, this script helps you create ...

   You can write as many paragraphs as needed.

   Shortcuts:<br>
     Esc or Q: quit.<br>
     etc.

   Supported:<br>
     Meshes, metaballs.

   Known issues:<br>
     This is just an example, there's no actual script.

   Notes:<br>
     You can check scripts bundled with Blender to see more examples of how to
    add documentation to your own works.
 \"\"\"

 B{Note}: your own GUI or menu code can display documentation by calling the
 help browser with the L{Blender.ShowHelp} function.

 Configuring scripts:
 ====================

 The L{Blender.Registry<Registry>} module provides a simplified way to keep
 scripts configuration options in memory and also saved in config files.
 And with the "Scripts Config Editor" script in the System menu users can later 
 view and edit the options easily.

 Let's first clarify what we mean by config options: they are simple data
 (bools, ints, floats, strings) used by programs to conform to user
 preferences.  The buttons in Blender's User Preferences window are a good
 example.

 For example, a particular exporter might include:
   - SEPARATE_MATS = False: a bool variable (True / False) to determine if it
     should write materials to a separate file;
   - VERSION = 2: an int to define an specific version of the export format;
   - TEX_DIR = "/path/to/textures": a default texture dir to prepend to all
     exported texture filenames instead of their actual paths.

 The script needs to provide users a GUI to configure these options -- or else
 directly editing the source code would be the only way to change them.  And to
 store changes made to the GUI so they can be reloaded any time the script is
 executed, programmers have to write and load their own config files (ideally at
 L{Blender.Get}('udatadir') or, if not available, L{Blender.Get}('datadir')).

 This section describes BPython facilities (based on the L{Registry} module and
 the config editor) that can take care of this in a simplified (and much
 recommended) way.

 Here's how it works::

  # sample_exporter.py
  import Blender
  from Blender import Registry

  # First define all config variables with their default values:
  SEPARATE_MATERIALS = True
  VERSION = True
  TEX_DIR = ''
  EXPORT_DIR = ''

  # Then define a function to update the Registry:
  def registry_update():
    # populate a dict with current config values:
    d = {
      'SEPARATE_MATERIALS': SEPARATE_MATERIALS,
      'VERSION': VERSION,
      'TEX_DIR': TEX_DIR,
      'EXPORT_DIR': EXPORT_DIR
    }
    # store the key (optional 3rd arg tells if
    # the data should also be written to a file):
    Registry.SetKey('sample_exporter', d, True)

  # (A good convention is to use the script name as Registry key)

  # Now we check if our key is available in the Registry or file system:
  regdict = Registry.GetKey('sample_exporter', True)

  # If this key already exists, update config variables with its values:
  if regdict:
    try:
      SEPARATE_MATERIALS = regdict['SEPARATE_MATERIALS']
      VERSION = regdict['VERSION']
      TEX_DIR = regdict['TEX_DIR']
      EXPORT_DIR = regdict['EXPORT_DIR']

    # if data was corrupted (or a new version of the script changed
    # (expanded, removed, renamed) the config vars and users may have
    # the old config file around):
    except: update_registry() # rewrite it

  else: # if the key doesn't exist yet, use our function to create it:
    update_registry()

  # ...

 Hint: nicer code than the simplistic example above can be written by keeping
 config var names in a list of strings and using the exec function. 

 B{Note}: if your script's GUI lets users change config vars, call the
 registry_update() function in the button events callback to save the changes.
 On the other hand, you don't need to handle configuration
 in your own gui, it can be left for the 'Scripts Config Editor',
 which should have access to your script's config key as soon as the
 above code is executed once (as soon as SetKey is executed).

 B{Note} (limits for config vars): strings longer than 300 characters are
 clamped and the number of items in dictionaries, sequences and the config key
 itself is limited to 60.


 Scripts Configuration Editor:
 -----------------------------

 This script should be available from the System menu in the Scripts window.
 It provides a GUI to view and edit saved configuration data, both from the
 Registry dictionary in memory and the scripts config data dir.  This is
 useful for all scripts with config vars, but especially for those without GUIs,
 like most importers and exporters, since this editor will provide one for them.

 The example above already gives a good idea of how the information can be
 prepared to be accessible from this editor, but there is more worth knowing:

  1. String vars that end with '_dir' or '_file' (can be upper case, too) are
  recognized as input boxes for dirs or files and a 'browse' button is added to
  their right side, to call the file selector.

  2. Both key names and configuration variables names starting with an
  underscore ('_') are ignored by the editor.  Programmers can use this feature
  for any key or config var that is not meant to be configured by this editor.

  3. The following information refers to extra config variables that may be
  added specifically to aid the configuration editor script.  To clarify, in the
  example code above these variables (the string 'script' and the dictionaries
  'tooltips' and 'limits') would appear along with SEPARATE_MATERIALS, VERSION,
  TEX_DIR and EXPORT_DIR, wherever they are written.

  Minor note: these names are case insensitive: tooltips, TOOLTIPS, etc. are all
  recognized.

  3.1 The config editor will try to display a 'help' button for a key, to show
  documentation for the script that owns it. To find this "owner script", it
  will first look for a config variable called 'script', a string containing
  the name of the owner Python file (with or without '.py' extension)::

   script = 'sample_exporter.py'

  If there is no such variable, the editor will check if the file formed by the
  key name and the '.py' extension exists. If both alternatives fail, no help
  button will be displayed.

  3.2 You can define tooltips for the buttons that the editor creates for your
  config data (string input, toggle, number sliders).  Simply create a dict
  called 'tooltips', where config var names are keys and their tooltips,
  values::

   tooltips = {
     'EXPORT_DIR': 'default folder where exported files should be saved',
     'VERBOSE': 'print info and warning messages to the console',
     'SEPARATE_MATERIALS': 'write materials to their own file'
   }

  3.3 Int and float button sliders need min and max limits.  This can be passed
  to the editor via a dict called 'limits' (ivar1, ivar2 and fvar are meant as
  extra config vars that might have been in the example code above)::

   limits = {'ivar1': [-10, 10], 'ivar2': [0, 100], 'fvar1': [-12.3, 15.4]}

  4. The Config Editor itself maintains a Registry key called "General", with
  general options relevant to many scripts, like "verbose" to tell if the user
  wants messages printed to the console and "confirm overwrite", to know if
  a script should ask for confirmation before overwriting files (all exporters
  are recommended to access the General key and check this var -- L{sys.exists
  <Sys.exists>} tells if files or folders already exist).

 Hint: for actual examples, try the ac3d importer and exporter (it's enough to
 call them from the menus then cancel with ESC), as those have been updated to
 use this config system.  After calling them their config data will be available
 in the Config Editor.  We also recommend adding a section about config vars
 in your script's help info, as done in the ac3d ones.

 L{Back to Main Page<API_intro>}
 ===============================
"""
