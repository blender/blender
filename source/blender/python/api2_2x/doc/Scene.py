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
    @warn: B{Depricated}: use RenderData.imageSizeX() and RenderData.imageSizeY()
    """

  def setWinSize(dimensions):
    """
    @warn: B{Depricated}: use RenderData.imageSizeX() and RenderData.imageSizeY
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
    @warn: B{Depricated}: use RenderData.startFrame()
    """

  def endFrame(frame = None):
    """
    @warn: B{Depricated}: use RenderData.endFrame()
    """

  def currentFrame(frame = None):
    """
    @warn: B{Depricated}: use RenderData.currentFrame
    """

  def frameSettings(start = None, end = None, current = None):
    """
    @warn: B{Depricated}: use RenderData.startFrame(),  RenderData.endFrame, RenderData.currentFrame
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
