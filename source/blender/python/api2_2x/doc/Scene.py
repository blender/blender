# Blender.Scene module and the Scene PyType object

"""
The Blender.Scene submodule.

B{New}: L{Scene.play}; scriptLink methods: L{Scene.getScriptLinks}, etc;
L{Scene.getRadiosityContext}

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
  @cvar name: The Scene name.
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
    @param event: "FrameChanged", "OnLoad" or "Redraw".
    @rtype: list
    @return: a list with Blender L{Text} names (the script links of the given
        'event' type) or None if there are no script links at all.
    """

  def clearScriptLinks ():
    """
    Delete all this Scene's script links.
    @rtype: bool
    @return: 0 if some internal problem occurred or 1 if successful.
    """

  def addScriptLink (text, event):
    """
    Add a new script link to this Scene.
    @type text: string
    @param text: the name of an existing Blender L{Text}.
    @type event: string
    @param event: "FrameChanged", "OnLoad" or "Redraw".
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
