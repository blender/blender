# Blender.Draw module and the Button PyType object

"""
The Blender.Draw submodule.

Draw
====

B{New}:
 - access to ASCII values in L{events<Register>} callbacks;
 - 'large' fonts for L{Text} and L{GetStringWidth}.
 - Pop-up blocks with L{PupBlock}
 - Color Picker button with L{ColorPicker}

This module provides access to a B{windowing interface} in Blender.  Its widgets
include many kinds of buttons: push, toggle, menu, number, string, slider,
scrollbar, plus support for text drawing.  It also includes keyboard keys and
mouse button code values in its dictionary, see a list after this example.

Example::
 import Blender
 from Blender import Draw, BGL

 mystring = ""
 mymsg = ""
 toggle = 0

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
   else: return # no need to redraw if nothing changed

   Draw.Redraw(1)

 def button_event(evt):  # the function to handle Draw Button events
   global mymsg, toggle
   if evt == 1:
     mymsg = "You pressed the toggle button."
     toggle = 1 - toggle
     Draw.Redraw(1)

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

 Draw.Register(gui, event, button_event)  # registering the 3 callbacks

All available events:
  - ACCENTGRAVEKEY
  - AKEY
  - BACKSLASHKEY
  - BACKSPACEKEY
  - BKEY
  - CAPSLOCKKEY
  - CKEY
  - COMMAKEY
  - DELKEY
  - DKEY
  - DOWNARROWKEY
  - EIGHTKEY
  - EKEY
  - ENDKEY
  - EQUALKEY
  - ESCKEY
  - F10KEY
  - F11KEY
  - F12KEY
  - F1KEY
  - F2KEY
  - F3KEY
  - F4KEY
  - F5KEY
  - F6KEY
  - F7KEY
  - F8KEY
  - F9KEY
  - FIVEKEY
  - FKEY
  - FOURKEY
  - GKEY
  - HKEY
  - HOMEKEY
  - IKEY
  - INPUTCHANGE
  - INSERTKEY
  - JKEY
  - KEYBD
  - KKEY
  - LEFTALTKEY
  - LEFTARROWKEY
  - LEFTBRACKETKEY
  - LEFTCTRLKEY
  - LEFTMOUSE
  - LEFTSHIFTKEY
  - LINEFEEDKEY
  - LKEY
  - MIDDLEMOUSE
  - MINUSKEY
  - MKEY
  - MOUSEX
  - MOUSEY
  - NINEKEY
  - NKEY
  - OKEY
  - ONEKEY
  - PAD0
  - PAD1
  - PAD2
  - PAD3
  - PAD4
  - PAD5
  - PAD6
  - PAD7
  - PAD8
  - PAD9
  - PADASTERKEY
  - PADENTER
  - PADMINUS
  - PADPERIOD
  - PADPLUSKEY
  - PADSLASHKEY
  - PAGEDOWNKEY
  - PAGEUPKEY
  - PAUSEKEY
  - PERIODKEY
  - PKEY
  - QFULL
  - QKEY
  - QUOTEKEY
  - Q_FIRSTTIME
  - RAWKEYBD
  - REDRAW
  - RETKEY
  - RIGHTALTKEY
  - RIGHTARROWKEY
  - RIGHTBRACKETKEY
  - RIGHTCTRLKEY
  - RIGHTMOUSE
  - RIGHTSHIFTKEY
  - RKEY
  - SEMICOLONKEY
  - SEVENKEY
  - SIXKEY
  - SKEY
  - SLASHKEY
  - SPACEKEY
  - TABKEY
  - THREEKEY
  - TIMER0
  - TIMER1
  - TIMER2
  - TIMER3
  - TKEY
  - TWOKEY
  - UKEY
  - UPARROWKEY
  - VKEY
  - WHEELDOWNMOUSE
  - WHEELUPMOUSE
  - WINCLOSE
  - WINFREEZE
  - WINQUIT
  - WINTHAW
  - WKEY
  - XKEY
  - YKEY
  - ZEROKEY
  - ZKEY

@note: function Button has an alias: L{PushButton}.

@warn: B{very important}: if using your script causes "Error totblock"
messages when Blender exits (meaning that memory has been leaked), this may
have been caused by an ignored return value from one of the button types.  To
avoid this, assign created buttons return values to B{global} variables,
instead of ignoring them.  Examples::

	# avoid this, it can cause memory leaks:
	Draw.Toggle(...)
	Draw.Number(...)
	Draw.String(...)
	# this is correct -- assuming the variables are globals:
	my_toggle_button = Draw.Toggle(...)
	my_int_button = Draw.Number(...)
	my_str_button = Draw.String(...)


@warn: Inside the windowing loop (after Draw.Register() has been executed and
before Draw.Exit() is called), don't use the redraw functions from other
modules (Blender and Window).  The Draw submodule has its own Draw.Redraw() and
Draw.Draw() functions that can be used inside the windowing loop.
"""

def Exit():
	"""
	Exit the windowing interface.
	"""

def BeginAlign():
	"""
	Buttons after this function will draw aligned (button layout only).
	"""

def EndAlign():
	"""
	Use after BeginAlign() to stop aligning the buttons (button layout only).
	"""

def UIBlock(draw):
	"""
	This function creates a popup area where buttons, labels, sliders etc can be drawn.
	
	@type draw: function
	@param draw: A function to draw to the popup area, taking no arguments: draw().
	
	@note: The size of the popup will expand to fit the bounds of the buttons created in the draw function.
	@note: Be sure to use the mouse coordinates to position the buttons under the mouse,
		so the popup dosn't exit as soon as it opens.
		The coordinates for buttons start 0,0 at the bottom left hand side of the screen.
	@note: Within this popup, Redraw events and the registered button callback will not work.
		For buttons to run events, use per button callbacks.
	@note: OpenGL drawing functions wont work within this popup, for text use L{Label} rather then L{Text}
	@warning: L{Menu} will not work properly within a UIBlock, this is a limitation with blenders user interface internals.
	"""

def Register(draw = None, event = None, button = None):
	"""
	Register callbacks for windowing.
	@type draw: function
	@type event: function
	@type button: function
	@param draw: A function to draw the screen, taking no arguments: draw().
	@param event: A function to handle keyboard and mouse input events, taking
		two arguments: f(evt, val), where:
			- 'evt' (int) is the event number;
			- 'val' (int) is the value modifier.  If val = 0, the event refers to a
			key or mouse button being released.  Otherwise it's a key/button press.
	@param button: A function to handle Draw Button events, taking one argument:
		f(evt), where:
			- 'evt' is the button number (see the I{event} parameter in L{Button}).
	@note: note that in the example at the beginning of this page Draw.Register
		is called only once.  It's not necessary to re-register the callbacks,
		they will stay until Draw.Exit is called.  It's enough to redraw the
		screen, when a relevant event is caught.
	@note: only during the B{event} callback: the L{Blender}.ascii variable holds
		the ASCII integer value (if it exists and is valid) of the current event.
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
	@type value: int, float, string or 3 floats
	@param value: The value to store in the button.
	@rtype: Blender Button
	@return: The Button created.
	@note: String values must have less then 400 characters.
	"""

def PushButton(name, event, x, y, width, height, tooltip = None, callback = None):
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
	@type callback: function
	@param callback: an optional argument so this button can have its own
		callback function. the function will run whenever this button is pressed.
		This function must accept 2 arguments (event, val).
	@note: This function used to be called only "Button".  We added an
		alternative alias to avoid a name clash with the L{Button} class/type that
		caused trouble in this documentation's generation.  The old name shouldn't
		be deprecated, use Button or PushButton (better) at your choice.
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

def PupTreeMenu( menu ):
	"""
	Create a popup menu tree.
	
	Each item in the list is: a menu item - (str, event); a separator - None;
	or submenu - (str, [...]).
	
	Submenus list uses the same syntax as the menu list. To add a title to the
	main menu, end the first entry str with '%t' - the event is ignored.

	Example::
		result = Draw.PupTreeMenu( [ ("Title%t", 0), ("Menu Item 1", 10), ("Menu Item 2", 12), ("SubMenu", [("Menu Item 3", 100), ("MenuItem4", 101) ]  ) ] )
	
	@type menu: string
	@param menu: A menu list
	@rtype: int
	@return: the chosen entry number or -1 if none was chosen.
	"""

def PupIntInput(text, default, min, max):
	"""
	Create an integer number input pop-up.

	This allows python to use Blender's integer number pop-up input.

	Example::
		default = 50
		min = 0
		max = 100

		msg = "Set this value between 0 and 100"
		result = Draw.PupIntInput(msg, default, min, max)
		if result != None:
			print result
		else:
			print 'no user input'

	@type text: string
	@param text: The text that is displayed in the pop-up.
	@type default: int
	@param default: The value that the pop-up is set to initially.
	@type min: int
	@param min: The lowest value the pop-up will allow.
	@type max: int
	@param max: The highest value the pop-up will allow.
	@rtype: int
	@return: the number chosen or None if none was chosen.
	"""

def PupFloatInput(text, default, min, max, clickStep, floatLen):
	"""
	Create a floating point number input pop-up.

	This allows python to use Blender's floating point pop-up input.

	Example::
		default = 50
		min = 0.0
		max = 10.0
		clickStep = 100
		floatLen = 3

		msg = "Set this value between 0 and 100"
		result = Draw.PupFloatInput(msg, default, min, max, clickStep, floatLen)
		if result != None:
			print result
		else:
			print 'no user input'
	
	@type text: string
	@param text: The text that is displayed in the pop-up.
	@type default: float
	@param default: The value that the pop-up is set to initially.
	@type min: float
	@param min: The lowest value the pop-up will allow.
	@type max: float
	@param max: The highest value the pop-up will allow.
	@type clickStep: int
	@param clickStep: How much is incremented per user click, 100 will increment 1.0, 10 will increment 0.1 etc.
	@type floatLen: int
	@param floatLen: The number of decimal places to display, between 2 and 4.
	@rtype: float
	@return: the number chosen or None if none was chosen.
	"""

def PupStrInput(text, default, max = 20):
	"""
	Create a string input pop-up.

	This allows python to use Blender's string pop-up input.

	Example::
		Blender.Draw.PupStrInput("Name:", "untitled", 25)
	
	@type text: string
	@param text: The text that is displayed in the pop-up.
	@type default: string
	@param default: The value that the pop-up is set to initially.  If it's longer
		then 'max', it's truncated.
	@type max: int
	@param max: The most characters the pop-up input will allow.  If not given
		it defaults to 20 chars.  It should be in the range [1, 100].
	@rtype: string
	@return: The text entered by the user or None if none was chosen.
	"""

def PupBlock(title, sequence):
	"""
	Display a pop-up block.
	
	Possible formats for the items in the sequence parameter.
	(Value are objects created with L{Create})
		- string:	Defines a label
		- (string, Value, string): Defines a toggle button. The first string is the text on the button, the optional second string is the tooltip.
		- (string, Value, min, max, string): Defines a numeric or string button, depending on the content of Value.  The first string is the text on the button, the optional second string is the tooltip. I{For string, max is the maximum length of the string and min is unused.}
		
	Example::
		import Blender
		
		text = Blender.Draw.Create("short text")
		f = Blender.Draw.Create(1.0)
		i = Blender.Draw.Create(2)
		tog = Blender.Draw.Create(0)
		
		block = []
		
		block.append(("Name: ", text, 0, 30, "this is some tool tip"))
		block.append("Some Label")
		block.append(("Value: ", f, 0.0, 100.0))
		block.append(("Value: ", i, 0, 100))
		block.append(("Option", tog, "another tooltip"))
		
		retval = Blender.Draw.PupBlock("PupBlock test", block)
		
		print "PupBlock returned", retval
		
		print "text\\t", text
		print "float\\t", f
		print "int\\t", i
		print "toggle\\t", tog

	@warning: On cancel, the Value objects are brought back to there initial values except for string values which will still contain the modified values.
	@type title: string
	@param title: The title of the block.
	@param sequence: A sequence defining what the block contains.
		The order of the list is the order of appearance, from top down.
	@rtype: int
	@return: 1 if the pop-up is confirmed, 0 otherwise
	"""

def Menu(name, event, x, y, width, height, default, tooltip = None, callback = None):
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
	@type callback: function
	@param callback: an optional argument so this button can have its own
		callback function. the function will run whenever this button is pressed.
		This function must accept 2 arguments (event, val).
	@rtype: Blender Button
	@return: The Button created.
	"""

def Toggle(name, event, x, y, width, height, default, tooltip = None, callback = None):
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
	@type callback: function
	@param callback: an optional argument so this button can have its own
		callback function. the function will run whenever this button is pressed.
		This function must accept 2 arguments (event, val).
	@rtype: Blender Button
	@return: The Button created.
	"""

def Slider(name, event, x, y, width, height, initial, min, max, realtime = 1,
			tooltip = None, callback = None):
	"""
	Create a new Slider Button object.
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
		
	@type callback: function
	@param callback: an optional argument so this button can have its own
		callback function. the function will run whenever this button is pressed.
		This function must accept 2 arguments (event, val).
	@rtype: Blender Button
	@return: The Button created.
	@note: slider callbacks will not work if the realtime setting is enabled.
	"""

#def Scrollbar(event, x, y, width, height, initial, min, max, realtime = 1,
#           tooltip = None):
#  """
#  Create a new Scrollbar Button object.
#  @type event: int
#  @param event: The event number to pass to the button event function when
#      activated.
#  @type x: int
#  @type y: int
#  @param x: The lower left x (horizontal) coordinate of the button.
#  @param y: The lower left y (vertical) coordinate of the button.
#  @type width: int
#  @type height: int
#  @param width: The button width.
#  @param height: The button height.
#  @type initial: int or float
#  @type min: int or float
#  @type max: int or float
#  @param initial:  The initial value.
#  @param min:  The minimum value.
#  @param max:  The maximum value.
#  @type realtime: int
#  @param realtime: If non-zero (the default), the slider will emit events as
#      it is edited.
#  @type tooltip: string
#  @param tooltip: The button's tooltip (the string that appears when the mouse
#      is kept over the button).
#  @rtype: Blender Button
#  @return: The Button created.
#  """

def ColorPicker(event, x, y, width, height, initial, tooltip = None, callback = None):
	"""
	Create a new Color Picker Button object.
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
	@type initial: 3-float tuple
	@param initial:  The initial color value. All values must be between 0 and 1
	@type tooltip: string
	@param tooltip: The button's tooltip (the string that appears when the mouse
		is kept over the button).
	@type callback: function
	@param callback: an optional argument so this button can have its own
		callback function. the function will run whenever this button is pressed.
		This function must accept 2 arguments (event, val).
	@rtype: Blender Button
	@return: The Button created.
	@note: The color picker will not work if the Register's event function is None.
	@note: Using the same button variable with more then 1 button at a time will corrupt memory.
	"""

def Normal(event, x, y, width, height, initial, tooltip = None, callback = None):
	"""
	Create a new Normal button, this allows you to set a 3d vector by rotating a sphere.
	@type event: int
	@param event: The event number to pass to the button event function when
		activated.
	@type x: int
	@type y: int
	@param x: The lower left x (horizontal) coordinate of the button.
	@param y: The lower left y (vertical) coordinate of the button.
	@type width: int
	@type height: int
	@param width: The button width - non square normal buttons .
	@param height: The button height.
	@type initial: 3-float tuple
	@param initial:  The initial vector value.
	@type tooltip: string
	@param tooltip: The button's tooltip (the string that appears when the mouse
		is kept over the button).
	@type callback: function
	@param callback: an optional argument so this button can have its own
		callback function. the function will run whenever this button is pressed.
		This function must accept 2 arguments (event, val).
	@rtype: Blender Button
	@return: The Button created.
	@note: The normal button will not work if the Register's event function is None.
	@note: Using the same button variable with more then 1 button at a time will corrupt memory.
	"""

def Number(name, event, x, y, width, height, initial, min, max, tooltip = None, callback = None):
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
	@type tooltip: string
	@param tooltip: The button's tooltip (the string that appears when the mouse
		is kept over the button).
	@type callback: function
	@param callback: an optional argument so this button can have its own
		callback function. the function will run whenever this button is pressed.
		This function must accept 2 arguments (event, val).
	@rtype: Blender Button
	@return: The Button created.

	I{B{Example:}}

	This example draws a single floating point value::
		from Blender import Draw
		b= Draw.Create(0.0) # Data for floating point button
		def bevent(evt):
			print 'My Button event:', evt
		def gui():
			global b
			b= Draw.Number('value: ', 1000, 0,0, 200, 20, b.val, 0,10, 'some text tip')

		Draw.Register(gui, None, bevent) # we are not going to worry about keyboard and mouse events
	"""


def String(name, event, x, y, width, height, initial, length, tooltip = None, callback = None):
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
	@type callback: function
	@param callback: an optional argument so this button can have its own
		callback function. the function will run whenever this button is pressed.
		This function must accept 2 arguments (event, val).
	@rtype: Blender Button
	@return: The Button created.
	"""

def GetStringWidth(string, fontsize = 'normal'):
	"""
	Get the width in pixels of a string.
	@type string: string
	@param string: A string.
	@type fontsize: string
	@param fontsize: The size of the font: 'large', 'normal', 'normalfix', 'small' or 'tiny'.
	@rtype: int
	@return: The width of I{string} with the chosen I{fontsize}.
	"""

def Text(string, fontsize = 'normal'):
	"""
	Draw a string on the screen.

	Text location is set using the OpenGL raster location functions L{BGL.glRasterPos} before the text is drawn.
	This sets the text location from the lower left corner of the current window.

	Text color is set using the OpenGL color functions L{BGL.glColor} before the text is drawn.

	@type string: string
	@param string: The text string to draw.
	@type fontsize: string
	@param fontsize: The size of the font: 'large', 'normal', 'normalfix', 'small' or 'tiny'.
	@rtype: int
	@return: The width of I{string} drawn with the chosen I{fontsize}.
	@note: For drawing text in the 3d view see the workaround in L{BGL.glRasterPos}
	"""

def Label(string, x, y, w, h):
	"""
	Draw a text lable on the screen.

	@type string: string
	@param string: The text string to draw.
	@rtype: None
	@return: None
	"""

def Image(image, x, y, zoomx=1.0, zoomy=1.0, clipx=0, clipy=0, clipw=-1, cliph=-1):
	"""
	Draw an image on the screen.

	The image is drawn at the location specified by the coordinates (x,y).  A
	pair of optional zoom factors (in horizontal and vertical directions) can
	be applied to the image as it is drawn, and an additional clipping rectangle
	can be applied to extract a particular sub-region of the image to draw.

	Note that the clipping rectangle is given in image space coordinates.  In
	image space, the origin is located at the bottom left, with x coordinates 
	increasing to the right and y coordinates increasing upwards.  No matter 
	where the clipping rectangle is placed in image space, the lower-left pixel 
	drawn on the screen is always placed at the coordinates (x,y).  The
	clipping rectangle is itself clipped to the dimensions of the image.  If
	either the width or the height of the clipping rectangle are negative then
	the corresponding dimension (width or height) is set to include as much of 
	the image as possible.

	For drawing images with alpha blending with the background you will need to enable blending as shown in the example.
	
	Example::
		import Blender
		from Blender import BGL, Image, Draw
		
		myimage = Image.Load('myimage.png')
		
		def gui():
			BGL.glEnable( BGL.GL_BLEND ) # Only needed for alpha blending images with background.
			BGL.glBlendFunc(BGL.GL_SRC_ALPHA, BGL.GL_ONE_MINUS_SRC_ALPHA) 
		
			Draw.Image(myimage, 50, 50)
		
			BGL.glDisable( BGL.GL_BLEND )
		def event(evt, val):
			if evt == Draw.ESCKEY:
				Draw.Exit()
		
		Draw.Register(gui, event, None)

	@type image: Blender.Image
	@param image: The image to draw.
	@type x: int
	@param x: The lower left x (horizontal) position of the origin of the image.
	@type y: int
	@param y: The lower left y (vertical) position of the origin of the image.
	@type zoomx: float
	@param zoomx: The x (horizontal) zoom factor to use when drawing the image.
	@type zoomy: float
	@param zoomy: The y (vertical) zoom factor to use when drawing the image.
	@type clipx: int
	@param clipx: The lower left x (horizontal) origin of the clipping rectangle
				  within the image.  A value of 0 indicates the left of the
				  image.
	@type clipy: int
	@param clipy: The lower left y (vertical) origin of the clipping rectangle
				  within the image.  A value of 0 indicates the bottom of the
				  image.
	@type clipw: int
	@param clipw: The width of the clipping rectangle within the image. If this
				  value is negative then the clipping rectangle includes as much
				  of the image as possible in the x (horizontal) direction.
	@type cliph: int
	@param cliph: The height of the clipping rectangle within the image. If this
				  value is negative then the clipping rectangle includes as much
				  of the image as possible in the y (vertical) direction.
	"""

class Button:
	"""
	The Button object
	=================
		This object represents a button in Blender's GUI.
	@type val: int or float, string or 3-float tuple (depends on button type).
	@ivar val: The button's value.
	"""
