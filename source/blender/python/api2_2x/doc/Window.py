# Blender.Window module and the Window PyType object

"""
The Blender.Window submodule.

Window
======

This module provides access to B{Window} functions in Blender.

B{New}: L{EditMode}, L{ViewLayer}, functions related to input events, etc.

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
    - ACTION
    - BUTS
    - FILE
    - IMAGE
    - IMASEL
    - INFO
    - IPO
    - NLA
    - OOPS
    - SCRIPT
    - SEQ
    - SOUND
    - TEXT
    - VIEW3D

@type Qual: readonly dictionary
@var Qual: Qualifier keys (shift, control, alt) bitmasks.
    - LALT: left ALT key
    - RALT: right ALT key
    - ALT: any ALT key, ...
    - LCTRL
    - RCTRL
    - CTRL
    - LSHIFT
    - RSHIFT
    - SHIFT
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

def EditMode(enable = -1):
  """
  Get and optionally set the current edit mode status: in or out.

  Example:: 
    Window.EditMode(0) # MUST leave edit mode before changing an active mesh
    # ...
    # make changes to the mesh
    # ...
    Window.EditMode(1) # be nice to the user and return things to how they were
  @type enable: int
  @param enable: get/set current status:
      - -1: just return current status (default);
      -  0: leave edit mode;
      -  1: enter edit mode.

      It's not an error to try to change to a state that is already the
      current one, the function simply ignores the request. 
  @rtype: int (bool)
  @return: 0 if Blender is not in edit mode right now, 1 otherwise. 
  @warn: this is an important function. NMesh operates on normal Blender
      meshes, not edit mode ones.  If a script changes an active mesh while in
      edit mode, when the user leaves the mode the changes will be lost,
      because the normal mesh will be rebuilt based on its unchanged edit mesh.
  """

def ViewLayer (layers = []):
  """
  Get and optionally set the currently visible layers in all 3d Views.
  @type layers: list of ints
  @param layers: a list with indexes of the layers that will be visible.  Each
      index must be in the range [1, 20].  If not given or equal to [], the
      function simply returns the visible ones without changing anything.
  @rtype: list of ints
  @return: the currently visible layers.
  """

def GetViewQuat ():
  """
  Get the current VIEW3D view quaternion values.
  @rtype: list of floats
  @return: the quaternion as a list of four float values.
  """

def SetViewQuat (quat):
  """
  Set the current VIEW3D view quaternion.
  @type quat: floats or list of floats
  @param quat: four floats or a list of four floats.
  """

def GetViewOffset (ofs):
  """
  Get the current VIEW3D offset values.
  @rtype: list of floats
  @return: a list with three floats: [x,y,z].
  """

def CameraView (camtov3d = 0):
  """
  Set the current VIEW3D view to the active camera's view.  If there's no
  active object or it is not of type 'Camera', the active camera for the
  current scene is used instead.
  @type camtov3d: int (bool)
  @param camtov3d: if nonzero it's the camera that gets positioned at the
      current view, instead of the view being changed to that of the camera.
  """

def QTest ():
  """
  Check if there are pending events in the event queue.
  @rtype: bool
  @return: 1 if there are pending events, 0 otherwise.
  """

def QRead ():
  """
  Get the next pending event from the event queue.

  Example::
   # let's catch all events and move the 3D Cursor when user presses
   # the left mouse button.
   from Blender import Draw, Window
   done = 0
   while not done:  # enter a 'get event' loop
     evt, val = Window.QRead() # catch next event
     if evt in [Draw.ESCKEY, Draw.QKEY]: done = 1 # end loop
     elif evt == Draw.SPACEKEY:
       Draw.PupMenu("Hey!|What did you expect?")
     elif evt == Draw.Redraw: # catch redraw events to handle them
       Window.RedrawAll() # redraw all areas
     elif evt == Draw.LEFTMOUSE and val: # left button pressed
       v3d = Window.ScreenInfo(Window.Types.VIEW3D)
       id = v3d[0]['id'] # get the (first) VIEW3D's id
       Window.QAdd(id, evt, 1) # add the caught mouse event to it
       # actually we should check if the event happened inside that area,
       # using Window.GetMouseCoords() and v3d[0]['vertices'] values.
       Window.QHandle(id) # process the event
       # do something fancy like putting some object where the
       # user positioned the 3d cursor, then:
       Window.Redraw() # show the change in the VIEW3D areas.

  @rtype: list
  @return: [event, val], where:
      - event: int - the key or mouse event (see L{Draw});
      - val: int - 1 for a key press, 0 for a release, new x or y coordinates
          for mouse events.
  """

def QAdd (win, event, val, after = 0):
  """
  Add an event to some window's (actually called areas in Blender) event queue.
  @type win: int
  @param win: the window id, see L{GetScreenInfo}.
  @type event: positive int
  @param event: the event to add, see events in L{Draw}.
  @type val: int
  @param val: 1 for a key press, 0 for a release.
  @type after: int (bool)
  @param after: if nonzero the event is put after the current queue and added
      later.
  """

def QHandle (winId):
  """
  Process immediately all pending events for the given window (area).
  @type winId: int
  @param winId: the window id, see L{GetScreenInfo}.
  @note: see L{QAdd} for how to send events to a particular window.
  """

def GetMouseCoords ():
  """
  Get the current mouse screen coordinates.
  @rtype: list with two ints
  @return: a [x, y] list with the coordinates.
  """

def GetMouseButtons ():
  """
  Get the current mouse button state (compare with events from L{Draw}).
  @rtype: int
  @return: an or'ed flag with the currently pressed buttons.
  """

def GetKeyQualifiers ():
  """
  Get the current qualifier keys state (see / compare against L{Qual}).
  @rtype: int
  @return: an or'ed combination of values in L{Window.Qual}.
  """

def SetKeyQualifiers (qual):
  """
  Fake qualifier keys state.  This is useful because some key events require
  one or more qualifiers to be active (see L{QAdd}).
  @type qual: int
  @param qual: an or'ed combination of values in L{Window.Qual}.
  @rtype: int
  @return: the current state, that should be equal to 'qual'.
  @warn: remember to reset the qual keys to 0 once they are not necessary
     anymore.
  """

def GetAreaID ():
  """
  Get the current area's ID.
  """

def GetAreaSize ():
  """
  Get the current area's size.
  @rtype: list with two ints
  @return: a [width, height] list.
  @note: the returned values are 1 pixel bigger than what L{GetScreenInfo}
     returns for the 'vertices' of the same area.
  """

def GetScreenInfo (type = -1, rect = 'win'):
  """
  Get info about the current screen setup.
  @type type: int
  @param type: the space type (see L{Window.Types}) to restrict the
     results to.  If -1 (the default), info is reported about all available
     areas.
  @type rect: string
  @param rect: the rectangle of interest.  This defines if the corner
      coordinates returned will refer to:
        - the whole area: 'total'
        - only the header: 'header'
        - only the window content part (default): 'win'
  @rtype: list of dictionaries
  @return: a list of dictionaries, one for each area in the screen.  Each
      dictionary has these keys (all values are ints):
        - 'vertices': [xmin, ymin, xmax, ymax] area corners;
        - 'win': window type, see L{Types};
        - 'id': this area's id.
  """
