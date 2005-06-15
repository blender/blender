# Blender.Scene module and the Scene PyType object

"""
The Blender.Scene submodule.

B{New}:
  - L{Scene.clearScriptLinks} accepts a parameter now.
  - L{Scene.getLayers}, L{Scene.setLayers} and the L{layers<Scene.layers>} and
    L{Layers<Scene.Layers>} Scene attributes. 
  - L{Scene.getActiveObject} method.

Scene
=====

This module provides access to B{Scenes} in Blender.

Example::
  import Blender
  from Blender import Scene, Object, Camera
  #
  camdata = Camera.New('ortho')           # create new camera data
  camdata.setName('newCam')
  camdata.setLens(16.0)
  scene = Scene.New('NewScene')           # create a new scene
  camobj = Object.New('Camera')           # create a new camera object
  camobj.link(camdata)                    # (*) link data to object first
  scene.link(camobj)                      # and then link object to scene
  scene.makeCurrent()                     # make this the current scene

@warn: as done in the example (*), it's recommended to first link object data to
    objects and only after that link objects to scene.  This is because if
    there is no object data linked to an object ob, scene.link(ob) will
    automatically create the missing data.  This is ok on its own, but I{if
    after that} this object is linked to obdata, the automatically created one
    will be discarded -- as expected -- but will stay in Blender's memory
    space until the program is exited, since Blender doesn't really get rid of
    most kinds of data.  So first linking obdata to object, then object to
    scene is a tiny tiny bit faster than the other way around and also saves
    some realtime memory (if many objects are created from scripts, the
    savings become important).
"""

def New (name = 'Scene'):
  """
  Create a new Scene in Blender.
  @type name: string
  @param name: The Scene name.
  @rtype: Blender Scene
  @return: The created Scene.
  """

def Get (name = None):
  """
  Get the Scene(s) from Blender.
  @type name: string
  @param name: The name of a Scene.
  @rtype: Blender Scene or a list of Blender Scenes
  @return: It depends on the I{name} parameter:
      - (name): The Scene with the given I{name};
      - ():     A list with all Scenes currently in Blender.
  """

def GetCurrent():
  """
  Get the currently active Scene in Blender.
  @rtype: Blender Scene
  @return: The currently active Scene.
  """

def Unlink(scene):
  """
  Unlink (delete) a Scene from Blender.
  @type scene: Blender Scene
  @param scene: The Scene to be unlinked.
  """

class Scene:
  """
  The Scene object
  ================
    This object gives access to Scene data in Blender.
  @type name: string
  @ivar name: The Scene name.
  @type Layers: integer (bitmask)
  @ivar Layers: The Scene layers (check also the easier to use
        L{layers<Scene.Scene.layers>}).  This value is a bitmask with at least
        one position set for the 20 possible layers starting from the low order
        bit.  The easiest way to deal with these values in in hexadecimal 
        notation.
        Example::
          scene.Layers = 0x04 # sets layer 3 ( bit pattern 0100 )
          scene.Layers |= 0x01
          print scene.Layers # will print: 5 ( meaning bit pattern 0101)
        After setting the Layers value, the interface (at least the 3d View and
        the Buttons window) needs to be redrawn to show the changes.
  @type layers: list of integers
  @ivar layers: The Scene layers (check also L{Layers<Scene.Scene.Layers>}).
        This attribute accepts and returns a list of integer values in the
        range [1, 20].
        Example::
          scene.layers = [3] # set layer 3
          scene.layers = scene.layers.append(1)
          print scene.layers # will print: [1, 3]
  """

  def getName():
    """
    Get the name of this Scene.
    @rtype: string
    """

  def setName(name):
    """
    Set the name of this Scene.
    @type name: string
    @param name: The new name.
    """

  def getLayers():
    """
    Get the layers set for this Scene.
    @rtype: list of integers
    @return: a list where each number means the layer with that number is
       set.
    """

  def setLayers(layers):
    """
    Set the visible layers for this scene.
    @type layers: list of integers
    @param layers: a list of integers in the range [1, 20], where each available
       index makes the layer with that number visible.
    @note: if this Scene is the current one, the 3D View layers are also
       updated, but the screen needs to be redrawn (at least 3D Views and
       Buttons windows) for the changes to be seen.
    """

  def copy(duplicate_objects = 1):
    """
    Make a copy of this Scene.
    @type duplicate_objects: int
    @param duplicate_objects: Defines how the Scene children are duplicated:
        - 0: Link Objects;
        - 1: Link Object Data;
        - 2: Full copy.
    @rtype: Scene
    @return: The copied Blender Scene.
    """

  def makeCurrent():
    """
    Make this Scene the currently active one in Blender.
    """

  def update(full = 0):
    """
    Update this Scene in Blender.
    @type full: int
    @param full: A bool to control the level of updating:
        - 0: sort the base list of objects.
        - 1: sort and also regroup, do ipos, ikas, keys, script links, etc.
    @warn: When in doubt, try with I{full = 0} first, since it is faster.
        The "full" update is a recent addition to this method.
    """

  def getRenderingContext():
    """
    Get the rendering context for this scene, see L{Render}.
    @rtype: RenderData
    @return: the render data object for this scene.
    """

  def getRadiosityContext():
    """
    Get the radiosity context for this scene, see L{Radio}.
    @rtype: Blender Radiosity
    @return: the radiosity object for this scene.
    @note: only the current scene can return a radiosity context.
    """

  def getChildren():
    """
    Get all objects linked to this Scene.
    @rtype: list of Blender Objects
    @return: A list with all Blender Objects linked to this Scene.
    @note: L{Object.Get} will return all objects currently in Blender, which
       means all objects from all available scenes.  In most cases (exporter
       scripts, for example), it's probably better to use this
       scene.GetChildren instead, since it will only access objects from this
       particular scene.
    """

  def getActiveObject():
    """
    Get this scene's active object.
    @note: the active object, if selected, can also be retrieved with
      L{Object.GetSelected} -- it is the first item in the returned
      list.  But even when no object is selected in Blender, there can be
      an active one (if the user enters editmode, for example, this is the
      object that should become available for edition).  So what makes this
      scene method different from C{Object.GetSelected()[0]} is that it can
      return the active object even when no objects are selected.
    @rtype: Blender Object or None
    @return: the active object or None if not available.
    """

  def getCurrentCamera():
    """
    Get the currently active Camera for this Scene.
    @rtype: Blender Camera
    @return: The currently active Camera.
    """

  def setCurrentCamera(camera):
    """
    Set the currently active Camera in this Scene.
    @type camera: Blender Camera
    @param camera: The new active Camera.
    """

  def link(object):
    """
    Link an Object to this Scene.
    @type object: Blender Object
    @param object: A Blender Object.
    """

  def unlink(object):
    """
    Unlink an Object from this Scene.
    @type object: Blender Object
    @param object: A Blender Object.
    """

  def getScriptLinks (event):
    """
    Get a list with this Scene's script links of type 'event'.
    @type event: string
    @param event: "FrameChanged", "OnLoad", "OnSave", "Redraw" or "Render".
    @rtype: list
    @return: a list with Blender L{Text} names (the script links of the given
        'event' type) or None if there are no script links at all.
    """

  def clearScriptLinks (links = None):
    """
    Delete script links from this Scene.  If no list is specified, all
    script links are deleted.
    @type links: list of strings
    @param links: None (default) or a list of Blender L{Text} names.
    """

  def addScriptLink (text, event):
    """
    Add a new script link to this Scene.
    @type text: string
    @param text: the name of an existing Blender L{Text}.
    @type event: string
    @param event: "FrameChanged", "OnLoad", "OnSave", "Redraw" or "Render".
    """

  def play (mode = 0, win = '<VIEW3D>'):
    """
    Play a realtime animation.  This is the "Play Back Animation" function in
    Blender, different from playing a sequence of rendered images (for that
    check L{Render.RenderData.play}).
    @type mode: int
    @param mode: controls playing:
        - 0: keep playing in the biggest 'win' window;
        - 1: keep playing in all 'win', VIEW3D and SEQ windows;
        - 2: play once in the biggest VIEW3D;
        - 3: play once in all 'win', VIEW3D and SEQ windows.
    @type win: int
    @param win: window type, see L{Window.Types}.  Only some of them are
        meaningful here: VIEW3D, SEQ, IPO, ACTION, NLA, SOUND.  But the others
        are also accepted, since this function can be used simply as an
        interruptible timer.  If 'win' is not visible or invalid, VIEW3D is
        tried, then any bigger visible window.
    @rtype: bool
    @return: 0 on normal exit or 1 when play back is canceled by user input.
    """
