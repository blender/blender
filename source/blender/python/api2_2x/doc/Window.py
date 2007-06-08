# Blender.Window module and the Window PyType object

"""
The Blender.Window submodule.

B{New}: renamed ViewLayer to L{ViewLayers} (actually added an alias, so both
forms will work).

Window
======

This module provides access to B{Window} functions in Blender.

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

@type MButs: readonly dictionary
@var MButs: Mouse buttons.
    - L: left mouse button
    - M: middle mouse button
    - R: right mouse button

@warn: The event system in Blender needs a rewrite, though we don't know when that will happen.  Until then, event related functions here (L{QAdd}, L{QRead},
L{QHandle}, etc.) can be used, but they are actually experimental and can be
substituted for a better method when the rewrite happens.  In other words, use
them at your own risk, because though they should work well and allow many
interesting and powerful possibilities, they can be deprecated in some future
version of Blender / Blender Python.
"""

def Redraw (spacetype = '<Types.VIEW3D>'):
  """
  Force a redraw of a specific space type.
  @type spacetype: int
  @param spacetype: the space type, see L{Types}.  By default the 3d Views are
      redrawn.  If spacetype < 0, all currently visible spaces are redrawn.
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
  @warn: script links are not allowed to call the File / Image Selectors.  This
     is because script links global dictionaries are removed when they finish
     execution and the File Selector needs the passed callback to stay around.
     An alternative is calling the File Selector from another script (see
     L{Blender.Run}).
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
  @param filename: A filename.  This defaults to L{Blender.Get}('filename').
  @warn: script links are not allowed to call the File / Image Selectors.  This
     is because script links global dictionaries are removed when they finish
     execution and the File Selector needs the passed callback to stay around.
     An alternative is calling the File Selector from another script (see
     L{Blender.Run}).
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

def GetActiveLayer ():
	"""
	Get the bitmask for the active layer.
	@note: if there is no 3d view it will return zero.
	@rtype: int
	@return: layer bitmask
	"""

def SetActiveLayer(layermask):
	"""
	Set the bitmask for the active layer.
	@type layermask: int
	@param layermask: An integer bitmask, to use humanly readable values do (1<<0) for the first layer, (1<<19) for the last layer.
	"""

def SetCursorPos (coords):
	"""
	Change the 3d cursor position.
	@type coords: 3 floats or a list of 3 floats
	@param coords: The new x, y, z coordinates.
	@note: if visible, the 3d View must be redrawn to display the change.  This
		can be done with L{Redraw}.
	"""

def WaitCursor (bool):
  """
  Set cursor to wait or back to normal mode.

  Example::
    Blender.Window.WaitCursor(1)
    Blender.sys.sleep(2000) # do something that takes some time
    Blender.Window.WaitCursor(0) # back

  @type bool: int (bool)
  @param bool: if nonzero the cursor is set to wait mode, otherwise to normal
      mode.
  @note: when the script finishes execution, the cursor is set to normal by
      Blender itself.
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
  @rtype: 4x4 float matrix (WRAPPED DATA)
  @return: the current matrix.
  """

def GetPerspMatrix ():
  """
  Get the current 3d perspective matrix.
  @rtype: 4x4 float matrix (WRAPPED DATA)
  @return: the current matrix.
  """

def EditMode(enable = -1, undo_msg = 'From script', undo = 1):
  """
  Get and optionally set the current edit mode status: in or out.

  Example:: 
    in_editmode = Window.EditMode()
    # MUST leave edit mode before changing an active mesh:
    if in_editmode: Window.EditMode(0)
    # ...
    # make changes to the mesh
    # ...
    # be nice to the user and return things to how they were:
    if in_editmode: Window.EditMode(1)
  @type enable: int
  @param enable: get/set current status:
      - -1: just return current status (default);
      -  0: leave edit mode;
      -  1: enter edit mode.

      It's not an error to try to change to a state that is already the
      current one, the function simply ignores the request.
  @type undo_msg: string
  @param undo_msg: only needed when exiting edit mode (EditMode(0)).  This
      string is used as the undo message in the Mesh->Undo History submenu in
      the 3d view header.  Max length is 63, strings longer than that get
      clamped.
  @param undo: don't save Undo information (only needed when exiting edit
  mode).
  @type undo: int
  @rtype: int (bool)
  @return: 0 if Blender is not in edit mode right now, 1 otherwise. 
  @warn: this is an important function. NMesh operates on normal Blender
      meshes, not edit mode ones.  If a script changes an active mesh while in
      edit mode, when the user leaves the mode the changes will be lost,
      because the normal mesh will be rebuilt based on its unchanged edit mesh.
  """

def ViewLayers (layers = [], winid = None):
  """
  Get and optionally set the currently visible layers in all 3d Views.
  @type layers: list of ints
  @param layers: a list with indexes of the layers that will be visible.  Each
      index must be in the range [1, 20].  If not given or equal to [], the
      function simply returns the visible ones without changing anything.
  @type winid: window id from as redurned by GetScreenInfo
  @param winid: An optional argument to set the layer of a window
      rather then setting the scene layers. For this to display in the 3d view
      the layer lock must be disabled (unlocked).
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

def GetViewOffset ():
  """
  Get the current VIEW3D offset values.
  @rtype: list of floats
  @return: a list with three floats: [x,y,z].
  @note: The 3 values returned are flipped in comparison object locations.
  """

def SetViewOffset (ofs):
  """
  Set the current VIEW3D offset values.
  @type ofs: 3 floats or list of 3 floats
  @param ofs: the new view offset values.
  @note: The value you give flipped in comparison object locations.
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
  @rtype: int
  @return: 0 if there are no pending events, non-zero otherwise.
  """

def QRead ():
  """
  Get the next pending event from the event queue.

  Example::
   # let's catch all events and move the 3D Cursor when user presses
   # the left mouse button.
   from Blender import Draw, Window

   v3d = Window.GetScreenInfo(Window.Types.VIEW3D)
   id = v3d[0]['id'] # get the (first) VIEW3D's id

   done = 0

   while not done:  # enter a 'get event' loop
     evt, val = Window.QRead() # catch next event
     if evt in [Draw.MOUSEX, Draw.MOUSEY]:
       continue # speeds things up, ignores mouse movement
     elif evt in [Draw.ESCKEY, Draw.QKEY]: done = 1 # end loop
     elif evt == Draw.SPACEKEY:
       Draw.PupMenu("Hey!|What did you expect?")
     elif evt == Draw.Redraw: # catch redraw events to handle them
       Window.RedrawAll() # redraw all areas
     elif evt == Draw.LEFTMOUSE: # left button pressed
       Window.QAdd(id, evt, 1) # add the caught mouse event to our v3d
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
          for mouse movement events.
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
  Get mouse's current screen coordinates.
  @rtype: list with two ints
  @return: a [x, y] list with the coordinates.
  """

def SetMouseCoords (coords):
  """
  Set mouse's current screen coordinates.
  @type coords: (list of) two ints
  @param coords: can be passed as x, y or [x, y] and are clamped to stay inside
      the screen.  If not given they default to the coordinates of the middle
      of the screen.
  """

def GetMouseButtons ():
  """
  Get the current mouse button state (see / compare against L{MButs}).
  @rtype: int
  @return: an OR'ed flag with the currently pressed buttons.
  """

def GetKeyQualifiers ():
  """
  Get the current qualifier keys state (see / compare against L{Qual}).
  @rtype: int
  @return: an OR'ed combination of values in L{Qual}.
  """

def SetKeyQualifiers (qual):
  """
  Fake qualifier keys state.  This is useful because some key events require
  one or more qualifiers to be active (see L{QAdd}).
  @type qual: int
  @param qual: an OR'ed combination of values in L{Qual}.
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

def GetScreenSize ():
  """
  Get Blender's screen size.
  @rtype: list with two ints
  @return: a [width, height] list.
  """

def GetScreens ():
  """
  Get the names of all available screens.
  @rtype: list of strings
  @return: a list of names that can be passed to L{SetScreen}.
  """

def SetScreen (name):
  """
  Set as current screen the one with the given name.
  @type name: string
  @param name: the name of an existing screen.  Use L{GetScreens} to get
      a list with all screen names.
  """

def GetScreenInfo (type = -1, rect = 'win', screen = ''):
  """
  Get info about the current screen setup.
  @type type: int
  @param type: the space type (see L{Types}) to restrict the
     results to.  If -1 (the default), info is reported about all available
     areas.
  @type rect: string
  @param rect: the rectangle of interest.  This defines if the corner
      coordinates returned will refer to:
        - the whole area: 'total'
        - only the header: 'header'
        - only the window content part (default): 'win'
  @type screen: string
  @param screen: the name of an available screen.  The current one is used by
      default.
  @rtype: list of dictionaries
  @return: a list of dictionaries, one for each area in the screen.  Each
      dictionary has these keys (all values are ints):
        - 'vertices': [xmin, ymin, xmax, ymax] area corners;
        - 'win': window type, see L{Types};
        - 'id': this area's id.
  """
