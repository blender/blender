# Blender.Window module and the Window PyType object

"""
The Blender.Window submodule.

Window
======

This module provides access to B{Window} functions in Blender.

B{New}: file and image selectors accept a filename now.

Example:
--------

FileSelector::

  import Blender
  from Blender import Window
  #
  def my_callback(filename):                # callback for the FileSelector
    print "You chose the file:", filename   # do something with the chosen file
  #
  Window.FileSelector (my_callback, "Choose one!")

Example:
--------

DrawProgressBar::

  import Blender
  from Blender.Window import DrawProgressBar
  #
  # substitute the bogus_*() function calls for your own, of course.
  #
  DrawProgressBar (0.0, "Importing data ...")
  bogus_importData()
  DrawProgressBar (0.3, "Building something")
  bogus_build()
  DrawProgressBar (0.8, "Updating Blender")
  bogus_update()
  DrawProgressBar (1.0, "Finished") 
  #
  # another example:
  #
  number = 1
  while number < 20:
    file = filename + "00%d" % number
    DrawProgressBar (number / 20.0, "Loading texture: %s" % file)
    Blender.Image.Load(file)
    number += 1

  DrawProgressBar (1.0, "Finished loading")


@type Types: readonly dictionary
@var Types: The available Window Types.
    - VIEW3D
    - IPO
    - OOPS
    - BUTS
    - FILE
    - IMAGE
    - INFO
    - SEQ
    - IMASEL
    - SOUND
    - ACTION
    - TEXT
    - NLA
"""

def Redraw ():
  """
  Force a redraw of a specific Window Type (see L{Types}).
  """

def RedrawAll ():
  """
  Redraw all windows.
  """

def QRedrawAll ():
  """
  Redraw all windows by queue event.
  """

def FileSelector (callback, title = 'SELECT FILE', filename = '<default>'):
  """
  Open the file selector window in Blender.  After the user selects a filename,
  it is passed as parameter to the function callback given to FileSelector().
  Example::
    import Blender
    #
    def my_function(filename):
      print 'The selected file was:', filename
    #
    Blender.Window.FileSelector (my_function, 'SAVE FILE')
  @type callback: function that accepts a string: f(str)
  @param callback: The function that must be provided to FileSelector() and
      will receive the selected filename as parameter.
  @type title: string
  @param title: The string that appears in the button to confirm the selection
      and return from the file selection window.
  @type filename: string
  @param filename: A filename.  This defaults to Blender.Get('filename').
  """

def ImageSelector (callback, title = 'SELECT IMAGE', filename = '<default>'):
  """
  Open the image selector window in Blender.  After the user selects a filename,
  it is passed as parameter to the function callback given to ImageSelector().
  Example::
    import Blender
    #
    def my_function(imagename):
      print 'The selected image was:', imagename
    #
    Blender.Window.ImageSelector (my_function, 'LOAD IMAGE')
  @type callback: function that accepts a string: f(str)
  @param callback: The function that must be provided to ImageSelector() and
      will receive the selected filename as parameter.
  @type title: string
  @param title: The string that appears in the button to confirm the selection
      and return from the image selection window.
  @type filename: string
  @param filename: A filename.  This defaults to Blender.Get('filename').
  """

def DrawProgressBar (done, text):
  """
  Draw a progress bar in the upper right corner of the screen. To cancel it
  prematurely, users can press the "Esc" key.  Start it with done = 0 and end
  it with done = 1.
  @type done: float
  @param done: A float in [0.0, 1.0] that tells the advance in the progress
      bar.
  @type text: string
  @param text: Info about what is currently being done "behind the scenes".
  """

def GetCursorPos ():
  """
  Get the current 3d cursor position.
  @rtype: list of three floats
  @return: the current position: [x, y, z].
  """
def SetCursorPos (coords):
  """
  Change the 3d cursor position.  Note: if visible, the 3d window must be
  redrawn to display the change.  This can be done with
  L{Redraw}(L{Types}['VIEW3D']), for example.
  @type coords: 3 floats or a list of 3 floats
  @param coords: The new x, y, z coordinates.
  """

def GetViewVector ():
  """
  Get the current 3d view vector.
  @rtype: list of three floats
  @return: the current vector: [x, y, z].
  """

def GetViewMatrix ():
  """
  Get the current 3d view matrix.
  @rtype: 4x4 float matrix
  @return: the current matrix.
  """
