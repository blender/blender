# Blender.Camera module and the Camera PyType object

"""
The Blender.Camera submodule

This module provides access to B{Camera Data} objects in Blender.

Example::

  from Blender import Camera, Object, Scene
  c = Camera.New('ortho').    # create new ortho camera data
  c.lens = 35.0                # set lens value
  cur = Scene.getCurrent().   # get current Scene
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
  @rtype: Camera
  @return: The created Camera Data object.
  """

def Get (name = None):
  """
  Get the Camera Data object(s).rom Blender.
  @type name: string
  @param name: The name of the Camera Data.
  @rtype: Camera or a list of Cameras
  @return: It depends on the 'name' parameter:
      - (name).The Camera Data object with the given name;
      - ().    A list with all Camera Data objects in the current Scene.
  """

class Camera:
  """
  The Camera Data object
  ======================
    This object gives access to Camera-specific data in Blender.
  @cvar name: The Camera Data name.
  @cvar type: The type: 'persp':0 or 'ortho':1.
  @cvar mode: The mode flags: B{or'ed value}: 'showLimits':1, 'showMist':2.
  @cvar lens: The lens value in [1.0, 250.0].
  @cvar clipStart: The clip start value in [0.0, 100.0].
  @cvar clipEnd: The clip end value in [1.0, 5000.0].
  @cvar drawSize: The draw size value in [0.1, 10.0].
  """

  def getName(self):
    """
    Get the name of this Camera Data object.
    @rtype: string
    """

  def setName(self, name):
    """
    Set the name of this Camera Data object.
    @type name: string
    @param name: The new name.
    """

  def getType(self):
    """
    Get this Camera's type.
    @rtype: int
    @return: 0 for 'persp' or 1 for 'ortho'.
    """

  def setType(self, type):
    """
    Set this Camera's type.
    @type type: string
    @param type: The Camera type: 'persp' or 'ortho'.
    """

  def getMode(self):
    """
    Get this Camera's mode flags.
    @rtype: int
    @return: B{OR'ed value}: 'showLimits' is 1, 'showMist' is 2, or
       resp. 01 and 10 in binary.
    """

  def setMode(self, mode1 = None, mode2 = None):
    """
    Set this Camera's mode flags. Mode strings given are turned 'on'.
    Those not provided are turned 'off', so cam.setMode().- without 
    arguments -- turns off all mode flags for Camera cam.
    @type mode1: string
    @type mode2: string
    @param mode1: A mode flag: 'showLimits' or 'showMist'.
    @param mode2: A mode flag: 'showLimits' or 'showMist'.
    """

  def getLens(self):
    """
    Get the lens value.
    @rtype: float
    """

  def setLens(self, lens):
    """
    Set the lens value.
    @type lens: float
    @param lens: The new lens value. 
    @warning: The value will be clamped to the min/max limits of this variable.
    """

  def getClipStart(self):
    """
    Get the clip start value.
    @rtype: float
    """

  def setClipStart(self, clipStart):
    """
    Set the clip start value.
    @type clipStart: float
    @param clipStart: The new lens value.
    @warning: The value will be clamped to the min/max limits of this variable.
    """

  def getClipEnd(self):
    """
    Get the clip end value.
    @rtype: float
    """

  def setClipEnd(self, clipEnd):
    """
    Set the clip end value.
    @type clipEnd: float
    @param clipEnd: The new clip end value.
    @warning: The value will be clamped to the min/max limits of this variable.
    """

  def getDrawSize(self):
    """
    Get the draw size value.
    @rtype: float
    """

  def setDrawSize(self, drawSize):
    """
    Set the draw size value.
    @type drawSize: float
    @param drawSize: The new draw size value.
    @warning: The value will be clamped to the min/max limits of this variable.
    """
