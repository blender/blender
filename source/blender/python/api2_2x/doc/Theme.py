# Blender.Window.Theme submodule and the Theme PyType object

"""
The Blender.Window.Theme submodule.

Theme
=====

This module provides access to B{Theme} objects in Blender.

Example::
  # this is a simplified version of the save_theme.py script
  # shipped with Blender:
  import Blender
  from Blender.Window import Theme, FileSelector

  theme = Theme.Get()[0] # get current theme

  def write_theme(filename):
    "Write the current theme as a bpython script"

    f = file(filename, "w")

    f.write("import Blender")
    f.write("from Blender.Window import Theme")
    f.write("theme = Theme.New('%s')" % theme.name)

    for tsp in theme.get(): # write each theme space
      command = "\\n%s = theme.get('%s')" % (tsp, tsp)
      f.write(command + "\\n")
      exec(command)
      exec("vars = dir(%s)" % tsp)
      vars.remove('theme')

      for var in vars: # write each variable from each theme space
        v = "%s.%s" % (tsp, var)
        exec("value = %s" % v)
        f.write("%s = %s\\n" % (v, value))

    f.write('\\nBlender.Redraw(-1)') # redraw to update the screen
    f.close()

  FileSelector(write_theme, "Save Current Theme", default_fname)
"""

def New (name = "New Theme", theme = '<default>'):
  """
  Create a new Theme object.
  @type name: string
  @param name: The name of the new theme.
  @type theme: Blender Theme
  @param theme: a base theme to copy all data from.  It defaults to the current
      one.
  @rtype:  Blender Theme
  @return: A new Blender Theme object.
  """

def Get (name = None):
  """
  Get the Theme object(s) from Blender.
  @type name: string
  @param name: The name of the Theme object.
  @rtype: Blender Theme or a list of Blender Themes
  @return: It depends on the I{name} parameter:
      - (name): The Theme object called I{name}, None if not found;
      - (): A list with all Theme objects currently in Blender.
  """


class Theme:
  """
  The Theme object
  ================
    This object gives access to Themes in Blender.  Each Theme object is
    composed of one UI (Use Interface) theme and many Space themes
    (3d view, Text Editor, Buttons window, etc).
  @cvar name: The name of this Theme object.
  """

  def getName():
    """
    Get the name of this Theme object.
    @rtype: string
    @return: the name of this Theme object.
    """

  def setName(s):
    """
    Rename this theme.
    @type s: string
    @param s: the new name.
    """

  def get(t = None):
    """
    Get a space or the ui (sub)theme from this Theme.
    @type t: string, int or None
    @param t: the wanted sub-theme as either:
        - int: -1 for UI or the types in L{Window.Types} for the others;
        - string: use get() to know them (they are case insensitive);
        - nothing: as written above, get() returns a list of names.
    @rtype: Blender ThemeSpace or ThemeUI or list of sub-theme types as strings.
    @return: It depends on the given parameter:
      - (): a list with all available types, as strings;
      - (type): the chosen sub-theme.
    """

  class ThemeUI:
    """
    The User Interface sub-theme
    ============================
      This can be accessed with theme.get(t), where t can be 'ui' or -1.
      The available variables follow the internal (C coded) ThemeUI struct in
      Blender.  Most of them represent rgba (red, green, blue, alpha) colors,
      with each component in the range [0, 255].  There is more than one way to
      access them.

      Examples::
        print outline.R
        outline.r = 180 # it's case insensitive
        outline[0] = 94 # 0 for red, 1 for green, ...
        outline = [200, 200, 200, 255] # setting all components at once
    @type theme: string
    @cvar theme: the parent Theme for this object.
    @cvar outline: theme rgba var.
    @cvar neutral: theme rgba var.
    @cvar action: theme rgba var.
    @cvar setting: theme rgba var.
    @cvar setting1: theme rgba var.
    @cvar setting2: theme rgba var.
    @cvar num: theme rgba var.
    @cvar textfield: theme rgba var.
    @cvar popup: theme rgba var.
    @cvar text: theme rgba var.
    @cvar text_hi: theme rgba var.
    @cvar menu_back: theme rgba var.
    @cvar menu_item: theme rgba var.
    @cvar menu_hilite: theme rgba var.
    @cvar menu_text: theme rgba var.
    @cvar menu_text_hi: theme rgba var.
    @type drawType: int
    @cvar drawType: the draw type (minimal, rounded, etc) in the range [1, 4].
    """

  class ThemeSpace:
    """
    The Space sub-themes
    ====================
      There is a sub-theme for each space in Blender (except for the Scripts
      window, but it will be added soon).  Please read the information about
      L{Theme.ThemeUI}, since it is also relevant here.  In Blender,
      all theme spaces share the same C structure.  For this reason, all of
      them here share the same variables, event though some spaces only use
      a few of them.  This lower-level access is acceptable because generally
      users will prefer to use the interface to change single theme options
      and only use scripting to save or restore themes.  But anyway, checking
      the Themes tab in the User Preferences space in Blender and using the
      bundled "Save current theme" script (or its simplified version written
      on the top of this page) can help you finding out any specific info you
      may need.
    @type theme: string
    @cvar theme: the parent Theme for this object.
    @cvar back: theme rgba var.
    @cvar text: theme rgba var.
    @cvar text_hi: theme rgba var.
    @cvar header: theme rgba var.
    @cvar panel: theme rgba var.
    @cvar shade1: theme rgba var.
    @cvar shade2: theme rgba var.
    @cvar hilite: theme rgba var.
    @cvar grid: theme rgba var.
    @cvar wire: theme rgba var.
    @cvar select: theme rgba var.
    @cvar active: theme rgba var.
    @cvar transform: theme rgba var.
    @cvar vertex: theme rgba var.
    @cvar vertex_select: theme rgba var.
    @cvar edge: theme rgba var.
    @cvar edge_select: theme rgba var.
    @cvar edge_seam: theme rgba var.
    @cvar edge_facesel: theme rgba var.
    @cvar face: theme rgba var.
    @cvar face_select: theme rgba var.
    @cvar face_dot: theme rgba var.
    @cvar normal: theme rgba var.
    @type vertex_size: int
    @cvar vertex_size: size of the vertices dots on screen in the range [1, 10].
    @type facedot_size: int
    @cvar facedot_size: size of the face dots on screen in the range [1, 10].
    """

