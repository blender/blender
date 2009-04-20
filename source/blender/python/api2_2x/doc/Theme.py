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
    "Write the current theme as a BPython script"

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
  @ivar name: The name of this Theme object.
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
        - int: -1 for UI or the types in L{Window.Types<Window.Types>} for the others;
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
  @ivar theme: the parent Theme for this object.
  @ivar outline: theme rgba var.
  @ivar neutral: theme rgba var.
  @ivar action: theme rgba var.
  @ivar setting: theme rgba var.
  @ivar setting1: theme rgba var.
  @ivar setting2: theme rgba var.
  @ivar num: theme rgba var.
  @ivar textfield: theme rgba var.
  @ivar textfield_hi: theme rgba var.
  @ivar popup: theme rgba var.
  @ivar text: theme rgba var.
  @ivar text_hi: theme rgba var.
  @ivar menu_back: theme rgba var.
  @ivar menu_item: theme rgba var.
  @ivar menu_hilite: theme rgba var.
  @ivar menu_text: theme rgba var.
  @ivar menu_text_hi: theme rgba var.
  @type drawType: int
  @ivar drawType: the draw type (minimal, rounded, etc) in the range [1, 4].
  @type iconTheme: string
  @ivar iconTheme: the filename (without path) for the icon theme PNG in .blender/icons/
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
  @ivar theme: the parent Theme for this object.
  @ivar back: theme rgba var.
  @ivar text: theme rgba var.
  @ivar text_hi: theme rgba var.
  @ivar header: theme rgba var.
  @ivar panel: theme rgba var.
  @ivar shade1: theme rgba var.
  @ivar shade2: theme rgba var.
  @ivar hilite: theme rgba var.
  @ivar grid: theme rgba var.
  @ivar wire: theme rgba var.
  @ivar select: theme rgba var.
  @ivar active: theme rgba var.
  @ivar transform: theme rgba var.
  @ivar vertex: theme rgba var.
  @ivar vertex_select: theme rgba var.
  @ivar edge: theme rgba var.
  @ivar edge_select: theme rgba var.
  @ivar edge_seam: theme rgba var.
  @ivar edge_facesel: theme rgba var.
  @ivar face: theme rgba var.
  @ivar face_select: theme rgba var.
  @ivar face_dot: theme rgba var.
  @ivar normal: theme rgba var.
  @ivar bone_solid: theme rgba var.
  @ivar bon_pose: theme rgba var.
  @ivar strip: theme rgba var.
  @ivar strip_select: theme rgba var.
  @ivar syntaxl: theme rgba var.
  @ivar syntaxn: theme rgba var.
  @ivar syntaxb: theme rgba var.
  @ivar syntaxv: theme rgba var.
  @ivar syntaxc: theme rgba var.
  @ivar movie: theme rgba var.
  @ivar image: theme rgba var.
  @ivar scene: theme rgba var.
  @ivar audio: theme rgba var.
  @ivar effect: theme rgba var.
  @ivar plugin: theme rgba var.
  @ivar transition: theme rgba var.
  @ivar meta: theme rgba var.
  @type vertex_size: int
  @ivar vertex_size: size of the vertices dots on screen in the range [1, 10].
  @type facedot_size: int
  @ivar facedot_size: size of the face dots on screen in the range [1, 10].
  """

