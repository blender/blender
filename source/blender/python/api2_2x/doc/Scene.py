# Blender.Scene module and the Scene PyType object

"""
The Blender.Scene submodule.

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
  scene.frameSettings(1, 100 ,1)          # set start, end and current frames
  scene.setWinSize(640, 480)              # set the render window dimensions
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

  def getWinSize():
    """
    Get the current x,y resolution of the render window.  These are the
    dimensions of the image created by the Blender Renderer.
    @rtype: list of two ints
    @return: [width, height].
    """

  def setWinSize(dimensions):
    """
    Set the width and height of the render window.  These are the dimensions
    of the image created by the Blender Renderer.
    @type dimensions: list of two ints
    @param dimensions: The new [width, height] values.
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

  def startFrame(frame = None):
    """
    Get (and optionally set) the start frame value.
    @type frame: int
    @param frame: The start frame.  If None, this method simply returns the
        current start frame.
    @rtype: int
    @return: The start frame value.
    """

  def endFrame(frame = None):
    """
    Get (and optionally set) the end frame value.
    @type frame: int
    @param frame: The end frame.  If None, this method simply returns the
        current end frame.
    @rtype: int
    @return: The end frame value.
    """

  def currentFrame(frame = None):
    """
    Get (and optionally set) the current frame value.
    @type frame: int
    @param frame: The current frame.  If None, this method simply returns the
        current frame value.
    @rtype: int
    @return: The current frame value.
    """

  def frameSettings(start = None, end = None, current = None):
    """
    Get (and optionally set) the start, end and current frame values.
    @type start: int
    @type end: int
    @type current: int
    @param start: The start frame value.
    @param end: The end frame value.
    @param current: The current frame value.
    @rtype: tuple
    @return: The frame values in a tuple: [start, end, current].
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

  def getRenderdir():
    """
    Get the current directory where rendered images are saved.
    @rtype: string
    @return: The path to the current render dir
    """

  def getBackbufdir():
    """
    Get the location of the backbuffer image.
    @rtype: string
    @return: The path to the chosen backbuffer image.
    """

  def getChildren():
    """
    Get all objects linked to this Scene.
    @rtype: list
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
