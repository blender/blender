# Blender.Scene module and the Scene PyType object

"""
The Blender.Scene submodule.

Scene
=====

This module provides access to B{Scenes} in Blender.

Example::

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
    @rtype: list
    @return: [width, height].
    """

  def setWinSize(dimensions):
    """
    Set the width and height of the render window.  These are the dimensions
    of the image created by the Blender Renderer.
    @type dimensions: list
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
