# Blender.Draw module and the Button PyType object

"""
The Blender.Draw submodule.

Draw
====

This module provides access to a B{windowing interface} in Blender.  Its widgets
include many kinds of buttons: push, toggle, menu, number, string, slider,
scrollbar, plus support for text drawing.  It also includes keyboard keys and
mouse button code values in its dictionary (print dir(Blender.Draw)).

Example::
  import Blender
  from Blender import Draw, BGL
  #
  mystring = ""
  mymsg = ""
  toggle = 0
  #
  def event(evt, val):    # the function to handle input events
    global mystring, mymsg

    if not val:  # val = 0: it's a key/mbutton release
      if evt in [Draw.LEFTMOUSE, Draw.MIDDLEMOUSE, Draw.RIGHTMOUSE]:
        mymsg = "You released a mouse button."
        Draw.Redraw(1)
      return

    if evt == Draw.ESCKEY:
      Draw.Exit()                 # exit when user presses ESC
      return

    elif Draw.AKEY <= evt <= Draw.ZKEY: mystring += chr(evt)
    elif evt == Draw.SPACEKEY: mystring += ' '
    elif evt == Draw.BACKSPACEKEY and len(mystring):
      mystring = mystring[:-1]
    else: return # this is important: only re-register if an event was caught

    Draw.Register(gui, event, button_event)  # re-register to stay in the loop
  #
  def button_event(evt):  # the function to handle Draw Button events
    global mymsg, toggle
    if evt == 1:
      mymsg = "You pressed the toggle button."
      toggle = 1 - toggle
      Draw.Redraw(1)
    else:
      Draw.Register(gui, event, button_event)
  #
  def gui():              # the function to draw the screen
    global mystring, mymsg, toggle
    if len(mystring) > 90: mystring = ""
    BGL.glClearColor(0,0,1,1)
    BGL.glClear(BGL.GL_COLOR_BUFFER_BIT)
    BGL.glColor3f(1,1,1)
    Draw.Toggle("Toggle", 1, 10, 10, 55, 20, toggle,"A toggle button")
    BGL.glRasterPos2i(72, 16)
    if toggle: toggle_state = "down"
    else: toggle_state = "up"
    Draw.Text("The toggle button is %s." % toggle_state, "small")
    BGL.glRasterPos2i(10, 230)
    Draw.Text("Type letters from a to z, ESC to leave.")
    BGL.glRasterPos2i(20, 200)
    Draw.Text(mystring)
    BGL.glColor3f(1,0.4,0.3)
    BGL.glRasterPos2i(340, 70)
    Draw.Text(mymsg, "tiny")
  #
  Draw.Register(gui, event, button_event)  # registering the 3 callbacks

@warn: Inside the windowing loop (after Draw.Register() has been executed and
before Draw.Exit() is called), don't use the redraw functions from other
modules (Blender and Window).  The Draw submodule has its own Draw.Redraw() and
Draw.Draw() functions that can be used inside the windowing loop.
"""

def Exit():
  """
  Exit the windowing interface.
  """

def Register(draw = None, event = None, button = None):
  """
  Register callbacks for windowing.
  @type draw: function
  @type event: function
  @type button: function
  @param draw: A function to draw the screen, taking no arguments: f().
  @param event: A function to handle keyboard and mouse input events, taking
      two arguments: f(evt, val), where:
       - 'evt' (int) is the event number;
       - 'val' (int) is the value modifier.  If val = 0, the event refers to a
       key or mouse button being released.  Otherwise it's a key/button press.
  @param button: A function to handle Draw Button events, taking one argument:
      f(evt), where:
        - 'evt' is the button number (see the I{event} parameter in L{Button}).
  """

def Redraw(after = 0):
  """
  Queue a redraw event.  Redraw events are buffered so that, regardless of how
  many events are queued, the window only receives one redraw event.
  @type after: int
  @param after: If non-zero, the redraw is processed before other input events.
  """

def Draw():
  """
  Force an immediate redraw.  Forced redraws are not buffered.  In other words,
  the window is redrawn once every time this function is called.
  """

def Create(value):
  """
  Create a default Button object.
  @type value: int, float or string
  @param value: The value to store in the button.
  @rtype: Blender Button
  @return: The Button created.
  """

def Button(name, event, x, y, width, height, tooltip = None):
  """
  Create a new (push) Button object.
  @type name: string
  @param name: The string to display on the button.
  @type event: int
  @param event: The event number to pass to the button event function when
      activated.
  @type x: int
  @type y: int
  @param x: The lower left x (horizontal) coordinate of the button.
  @param y: The lower left y (vertical) coordinate of the button.
  @type width: int
  @type height: int
  @param width: The button width.
  @param height: The button height.
  @type tooltip: string
  @param tooltip: The button's tooltip (the string that appears when the mouse
      is kept over the button).
  """

def PupMenu(name, maxrow = None):
  """
  Create a pop-up menu.

  The menu options are specified through the 'name' parameter, like with
  L{Menu}: options are followed by a format code and separated by the '|'
  character.  Valid format codes are:
    - %t - The option should be used as the title of the pop-up;
    - %l - insert a separating line (only works if 'maxrow' isn't given);
    - %xB{N} - Chosen this option, PupMenu should return the integer B{N}.

  Example::
    name = "OK?%t|QUIT BLENDER"  # if no %xN int is set, indices start from 1
    result = Draw.PupMenu(name)
    if result:
      Draw.PupMenu("Really?%t|Yes|No")

  @type name: string
  @param name: The format string to define the contents of the button.
  @type maxrow: int
  @param maxrow: The maximum number of rows for each column in the pop-up.
  @rtype: int
  @return: the chosen entry number or -1 if none was chosen.
  """

def Menu(name, event, x, y, width, height, default, tooltip = None):
  """
  Create a new Menu Button object.
  
  The menu options are specified through the 'name' of the button.  Options are
  I{followed} by a format code and separated by the '|' (pipe) character.  Valid
  format codes are:
    - %t - The option should be used as the title;
    - %l - Insert a separating line;
    - %xB{N} - The option should set the integer B{N} in the button value.

  Example::
    name = "The Title %t|First Entry %x1|Second Entry %x2|Third Entry %x3"
    menu = Draw.Menu(name, 2, 60, 120, 200, 40, 3, "Just a test menu.")
    # note that, since default = 3, the "Third Entry"
    # will appear as the default choice in the Menu.

  @type name: string
  @param name: The format string to define the contents of the button.
  @type event: int
  @param event: The event number to pass to the button event function when
      activated.
  @type x: int
  @type y: int
  @param x: The lower left x (horizontal) coordinate of the button.
  @param y: The lower left y (vertical) coordinate of the button.
  @type width: int
  @type height: int
  @param width: The button width.
  @param height: The button height.
  @type default: int
  @param default: The number of the option to be selected by default.
  @type tooltip: string
  @param tooltip: The button's tooltip (the string that appears when the mouse
      is kept over the button).
  @rtype: Blender Button
  @return: The Button created.
  """

def Toggle(name, event, x, y, width, height, default, tooltip = None):
  """
  Create a new Toggle Button object.
  @type name: string
  @param name: The string to display on the button.
  @type event: int
  @param event: The event number to pass to the button event function when
      activated.
  @type x: int
  @type y: int
  @param x: The lower left x (horizontal) coordinate of the button.
  @param y: The lower left y (vertical) coordinate of the button.
  @type width: int
  @type height: int
  @param width: The button width.
  @param height: The button height.
  @type default: int
  @param default:  The value specifying the default state:
      (0 for "up", 1 for "down").
  @type tooltip: string
  @param tooltip: The button's tooltip (the string that appears when the mouse
      is kept over the button).
  @rtype: Blender Button
  @return: The Button created.
  """

def Slider(name, event, x, y, width, height, initial, min, max, realtime = 1,
           tooltip = None):
  """
  Create a new Toggle Button object.
  @type name: string
  @param name: The string to display on the button.
  @type event: int
  @param event: The event number to pass to the button event function when
      activated.
  @type x: int
  @type y: int
  @param x: The lower left x (horizontal) coordinate of the button.
  @param y: The lower left y (vertical) coordinate of the button.
  @type width: int
  @type height: int
  @param width: The button width.
  @param height: The button height.
  @type initial: int or float
  @type min: int or float
  @type max: int or float
  @param initial:  The initial value.
  @param min:  The minimum value.
  @param max:  The maximum value.
  @type realtime: int
  @param realtime: If non-zero (the default), the slider will emit events as
      it is edited.
  @type tooltip: string
  @param tooltip: The button's tooltip (the string that appears when the mouse
      is kept over the button).
  @rtype: Blender Button
  @return: The Button created.
  """

def Scrollbar(event, x, y, width, height, initial, min, max, realtime = 1,
           tooltip = None):
  """
  Create a new Scrollbar Button object.
  @type event: int
  @param event: The event number to pass to the button event function when
      activated.
  @type x: int
  @type y: int
  @param x: The lower left x (horizontal) coordinate of the button.
  @param y: The lower left y (vertical) coordinate of the button.
  @type width: int
  @type height: int
  @param width: The button width.
  @param height: The button height.
  @type initial: int or float
  @type min: int or float
  @type max: int or float
  @param initial:  The initial value.
  @param min:  The minimum value.
  @param max:  The maximum value.
  @type realtime: int
  @param realtime: If non-zero (the default), the slider will emit events as
      it is edited.
  @type tooltip: string
  @param tooltip: The button's tooltip (the string that appears when the mouse
      is kept over the button).
  @rtype: Blender Button
  @return: The Button created.
  """

def Number(name, event, x, y, width, height, initial, min, max, realtime = 1,
           tooltip = None):
  """
  Create a new Number Button object.
  @type name: string
  @param name: The string to display on the button.
  @type event: int
  @param event: The event number to pass to the button event function when
      activated.
  @type x: int
  @type y: int
  @param x: The lower left x (horizontal) coordinate of the button.
  @param y: The lower left y (vertical) coordinate of the button.
  @type width: int
  @type height: int
  @param width: The button width.
  @param height: The button height.
  @type initial: int or float
  @type min: int or float
  @type max: int or float
  @param initial:  The initial value.
  @param min:  The minimum value.
  @param max:  The maximum value.
  @type realtime: int
  @param realtime: If non-zero (the default), the slider will emit events as
      it is edited.
  @type tooltip: string
  @param tooltip: The button's tooltip (the string that appears when the mouse
      is kept over the button).
  @rtype: Blender Button
  @return: The Button created.
  """


def String(name, event, x, y, width, height, initial, length, tooltip = None):
  """
  Create a new String Button object.
  @type name: string
  @param name: The string to display on the button.
  @type event: int
  @param event: The event number to pass to the button event function when
      activated.
  @type x: int
  @type y: int
  @param x: The lower left x (horizontal) coordinate of the button.
  @param y: The lower left y (vertical) coordinate of the button.
  @type width: int
  @type height: int
  @param width: The button width.
  @param height: The button height.
  @type initial: string
  @param initial: The string to display initially.
  @type length: int
  @param length: The maximum input length.
  @type tooltip: string
  @param tooltip: The button's tooltip (the string that appears when the mouse
      is kept over the button).
  @rtype: Blender Button
  @return: The Button created.
  """

def GetStringWidth(string, fontsize = 'normal'):
  """
  Get the width in pixels of a string.
  @type string: string
  @param string: A string.
  @type fontsize: string
  @param fontsize: The size of the font: 'normal', 'small' or 'tiny'.
  @rtype: int
  @return: The width of I{string} with the chosen I{fontsize}.
  """

def Text(string, fontsize = 'normal'):
  """
  Draw a string on the screen.
  @type string: string
  @param string: The text string to draw.
  @type fontsize: string
  @param fontsize: The size of the font: 'normal', 'small' or 'tiny'.
  @rtype: int
  @return: The width of I{string} drawn with the chosen I{fontsize}.
  """
