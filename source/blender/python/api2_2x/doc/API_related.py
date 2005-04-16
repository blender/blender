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
   - Proper documentation data is used by the 'Scripts Help Browser' script to
     show help information for any registered script.  Your own GUI can use
     this facility with the L{Blender.ShowHelp} function.
   - Configuration is for data in your script that can be tweaked according to
     user taste or needs.  Like documentation, this is another helper
     functionality -- you don't need to provide a GUI yourself to edit config
     data.


 Command line usage:
 -------------------

 B{Specifying scripts}:

 The '-P' option followed either by:
   - a script filename (full pathname if not in the same folder where you run
     the command);
   - the name of a Text in a .blend file (that must also be specified)
 will open Blender and immediately run the given script.

 Example::

  # open Blender and execute the given script:
  blender -P script.py

 B{Passing parameters}:

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

 B{Background mode}:

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
 also be set via bpython (take a look at L{Render.RenderData}).  Their
 names will be the equivalent frame number followed by the extension of the
 chosen image type: 0001.png, for example.  To rename them to something else,
 coders can use the C{rename} function in the standard 'os' Python module.

 Reminder: if you just need to render, it's not necessary to have a script.
 Blender can create stills and animations with its own command line arguments.
 Example:
  - a single image at frame 44: blender -b myfile.blend -f 44
  - an animation from frame 1 to 10: blender -b myfile.blend -s 1 -e 10 -a


 Registering scripts:
 --------------------

 To be registered a script needs two things:
   - to be either in the default scripts dir or in the user defined scripts
     path (see User Preferences window -> File Paths tab -> Python path);
   - to have a proper header.

 Try 'blender -d' to know where your default dir for scripts is, it will
 inform either the dir or the file with that info already parsed, which is
 in the same dir of the scripts folder.

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
 --------------------

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
      options: "blender" and "elysiun", which link to the Python forums at
      blender.org and elysiun.com, respectively.

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
       "Support forum, http://somewhere.com/forum/", "blender", "elysiun"]
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
 --------------------

 Configuration data is simple data used by your script (bools, ints, floats,
 strings) to define default behaviors.

 For example, an exporter might have:
   - EXPORT_LIGHTS = False: a bool variable (True / False) to determine if it
     should also export lights setup information;
   - VERSION = 2.0: an int to define an specific version of the export format;
   - TEX_DIR = "/path/to/textures": a default texture dir to prepend to all
     exported texture filenames instead of their actual paths.

 To properly handle this, script writers had to keep this information in a
 separate config file (at L{Blender.Get}('udatadir') or, if not available,
 L{Blender.Get}('datadir')), provide a GUI to edit it and update the file
 whenever needed.

 There are facilities in BPython now to take care of this in a simplified (and
 much recommended) way.

 The L{Registry} module functions L{GetKey<Registry.GetKey>} and
 L{SetKey<Registry.SetKey>} take care of both keeping the data in Blender
 and (new) storing it in config files at the proper dir.  And the 'Scripts
 Configuration Editor' script provides a GUI for users to view and edit
 configuration data.

 Here's how it works::

  # sample_exporter.py
  import Blender
  from Blender import Registry

  # First define all config variables with their default values:
  EXPORT_LIGHTS = True
  VERBOSE = True
  EXPORT_DIR = ''

  # Then define a function to update the Registry:
  def registry_update():
    # populate a dict with current config values:
    d = {
      'EXPORT_LIGHTS': EXPORT_LIGHTS,
      'VERBOSE': VERBOSE,
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
    EXPORT_LIGHTS = regdict['EXPORT_LIGHTS']
    VERBOSE = regdict['VERBOSE']
    EXPORT_DIR = regdict['EXPORT_DIR']
  else: # if the key doesn't exist yet, use our function to create it:
    update_registry()

  # ...

 Hint: nicer code than the simplistic example above can be written by keeping
 config var names in a list of strings and using the exec function. 

 B{Note}: if you have a gui and the user uses it to change config vars,
 call the registry_update() function to save the changes.
 On the other hand, you don't need to handle configuration
 in your own gui, it can be left for the 'Scripts Config Editor',
 which should have access to your script's config key as soon as the
 above code is executed once.

 As written above, config vars can be bools, ints, floats or strings.  This is
 what the Config Editor supports, with sensible but generous limits for the
 number of vars and the size of each string.  Restrictions were suggested or
 imposed to these facilities related to the Registry module because it's meant
 for configuration info, not for large volumes of data.  For that you can
 trivially store it in a file or Blender Text yourself -- and tell the user
 about it, specially if your script keeps megabytes of data in the Registry
 memory.

 B{Scripts Configuration Editor}:

 This script should be available from the Help menu and provides a GUI to
 view and edit saved configuration data, both from the Registry dictionary in
 memory and the scripts config data dir.

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
  'tooltips' and 'limits') would appear along with EXPORT_LIGHTS, VERBOSE and
  EXPORT_DIR, wherever they are written.

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
     'EXPORT_LIGHTS': 'export scene lighting setup'
   }

  3.3 Int and float button sliders need min and max limits.  This can be passed
  to the editor via a dict called 'limits' (ivar1, ivar2 and fvar are meant as
  extra config vars that might have been in the example code above)::

   limits = {'ivar1': [-10, 10], 'ivar2': [0, 100], 'fvar1': [-12.3, 15.4]}

 L{Back to Main Page<API_intro>}
"""
