# Blender.Camera module and the Camera PyType object

"""
The Blender.Camera submodule.

Camera Data
===========

This module provides access to B{Camera Data} objects in Blender.

Example::

  from Blender import Camera, Object, Scene
  c = Camera.New('ortho').    # create new ortho camera data
  c.lens = 35.0               # set lens value
  cur = Scene.getCurrent().   # get current scene
  ob = Object.New('Camera').  # make camera object
  ob.link(c).                 # link camera data with this object
  cur.link(ob).               # link object into scene
  cur.setCurrentCamera(ob).   # make this camera the active";
"""

def New (type = 'persp', name = 'CamData'):
  """
  Create a new Camera Data object.
  @type type: string
  @param type: The Camera type: 'persp' or 'ortho'.
  @type name: string
  @param name: The Camera Data name.
  @rtype: Blender Camera
  @return: The created Camera Data object.
  """

def Get (name = None):
  """
  Get the Camera Data object(s) from Blender.
  @type name: string
  @param name: The name of the Camera Data.
  @rtype: Blender Camera or a list of Blender Cameras
  @return: It depends on the 'name' parameter:
      - (name): The Camera Data object with the given name;
      - ():     A list with all Camera Data objects in the current scene.
  """

class Camera:
  """
  The Camera Data object
  ======================
    This object gives access to Camera-specific data in Blender.
  @cvar name: The Camera Data name.
  @cvar type: The Camera type: 'persp':0 or 'ortho':1.
  @cvar mode: The mode flags: B{or'ed value}: 'showLimits':1, 'showMist':2.
  @cvar lens: The lens value in [1.0, 250.0].
  @cvar clipStart: The clip start value in [0.0, 100.0].
  @cvar clipEnd: The clip end value in [1.0, 5000.0].
  @cvar drawSize: The draw size value in [0.1, 10.0].
  @warning: Most member variables assume values in some [Min, Max] interval.
      When trying to set them, the given parameter will be clamped to lie in
      that range: if val < Min, then val = Min, if val > Max, then val = Max.
  """

  def getName():
    """
    Get the name of this Camera Data object.
    @rtype: string
    """

  def setName(name):
    """
    Set the name of this Camera Data object.
    @type name: string
    @param name: The new name.
    """

  def getType():
    """
    Get this Camera's type.
    @rtype: int
    @return: 0 for 'persp' or 1 for 'ortho'.
    """

  def setType(type):
    """
    Set this Camera's type.
    @type type: string
    @param type: The Camera type: 'persp' or 'ortho'.
    """

  def getMode():
    """
    Get this Camera's mode flags.
    @rtype: int
    @return: B{OR'ed value}: 'showLimits' is 1, 'showMist' is 2, or
       resp. 01 and 10 in binary.
    """

  def setMode(mode1 = None, mode2 = None):
    """
    Set this Camera's mode flags. Mode strings given are turned 'on'.
    Those not provided are turned 'off', so cam.setMode() -- without 
    arguments -- turns off all mode flags for Camera cam.
    @type mode1: string
    @type mode2: string
    @param mode1: A mode flag: 'showLimits' or 'showMist'.
    @param mode2: A mode flag: 'showLimits' or 'showMist'.
    """

  def getLens():
    """
    Get the lens value.
    @rtype: float
    """

  def setLens(lens):
    """
    Set the lens value.
    @type lens: float
    @param lens: The new lens value. 
    """

  def getClipStart():
    """
    Get the clip start value.
    @rtype: float
    """

  def setClipStart(clipstart):
    """
    Set the clip start value.
    @type clipstart: float
    @param clipstart: The new lens value.
    """

  def getClipEnd():
    """
    Get the clip end value.
    @rtype: float
    """

  def setClipEnd(clipend):
    """
    Set the clip end value.
    @type clipend: float
    @param clipend: The new clip end value.
    """

  def getDrawSize():
    """
    Get the draw size value.
    @rtype: float
    """

  def setDrawSize(drawsize):
    """
    Set the draw size value.
    @type drawsize: float
    @param drawsize: The new draw size value.
    """
