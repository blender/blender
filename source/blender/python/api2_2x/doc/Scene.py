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

  Scene.enableDispWin()
  scene.enableExtensions(1)
  scene.setRenderPath("C:/myRenderdir/")
  scene.sizePreset(Scene.PC)
  scene.setImageType(Scene.AVIRAW)
  scene.startFrame(2)
  scene.endFrame(10)
  scene.renderAnim()

  scene.setImageType(Scene.TARGA)
  scene.framesPerSec(15)
  scene.startFrame(15)
  scene.endFrame(22)
  scene.renderAnim()

  Scene.CloseRenderWindow()
  print scene.framesPerSec()
  

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

def CloseRenderWindow():
  """
  Closes the rendering window.
  """

def EnableDispView():
  """
  Render in the 3d View area.
  """

def EnableDispWin():
  """
  Render in Render window.
  """

def SetRenderWinPos(locationList):
  """
  Set the position of the Render window on the screen.
  Possible values are:
    -  S = south
    -  N = north
    -  W = west
    -  E = east
    -  C = center
    -  ne = northeast
    -  nw = northwest
    -  se = southeast
    -  sw = southwest
  @type locationList: PyList of strings
  @param locationList: a list of strings that together define
  the location of the Render window on the screen.
  """

def EnableEdgeShift():
  """
  Globally with the unified renderer enabled the outlines of the render
  are shifted a bit.
  """

def EnableEdgeAll():
  """
  Globally consider transparent faces for edge-rendering with the unified renderer.
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

  def render():
    """
    Render the scene.
    """

  def renderAnim():
    """
    Render a series of frames to an output directory.
    """

  def play():
    """
    play animation of rendered images/avi (searches Pics: field).
    """

  def setRenderPath(path):
    """
    Set the path to where the renderer will write to.
    @type path: string
    @param path: A directory for that the renderer searches for
    both playback and output from the renderAnim function.
    """

  def getRenderPath():
    """
    Get the path to where the renderer will write to.
    @rtype: string
    @return: Returns the directory that is used to playback and store rendered
    sequences.
    """

  def setBackbufPath(path):
    """
    Set the path to a background image and load it.
    @type path: string
    @param path: The path to a background image for loading.
    """

  def getBackbufPath():
    """
    Get the path to the background image.
    @rtype: string
    @return: The path to a background image.
    """

  def enableBackbuf(toggle):
    """
    Enable/disable the backbuf image.
    @type toggle: int
    @param toggle: pass 1 for on / 0 for off
    """

  def setFtypePath(path):
    """
    Set the path to Ftype file.
    @type path: string
    @param path: Path to Ftype Image type.
    """

  def getFtypePath():
    """
    Get the path to the Ftype file
    @rtype: string
    @return: Path to FtypeImage type.
    """

  def enableExtensions(toggle):
    """
    Enable/disable windows extensions for output files.
    @type toggle: int
    @param toggle: pass 1 for on / 0 for off
    """

  def enableSequencer(toggle):
    """
    Enable/disable Do Sequence.
    @type toggle: int
    @param toggle: pass 1 for on / 0 for off
    """

  def enableRenderDaemon(toggle):
    """
    Enable/disable Scene daemon.
    @type toggle: int
    @param toggle: pass 1 for on / 0 for off
    """

  def enableToonShading(toggle):
    """
    Enable/disable Edge rendering.
    @type toggle: int
    @param toggle: pass 1 for on / 0 for off
    """

  def edgeIntensity(intensity = None):
    """
    Get/set edge intensity for toon shading.
    @type intensity: int (optional)
    @param intensity: must be between 0 - 255
    @rtype: int (if prototype is empty)
    @return: Current edge intensity for the scene.
    """

  def setEdgeColor(red, green, blue):
    """
    Set the edge color for toon shading.
    @type red: float
    @param red: must be between 0 - 1.0
    @type green: float
    @param green: must be between 0 - 1.0
    @type blue: float
    @param blue: must be between 0 - 1.0
    """

  def getEdgeColor():
    """
    Get the edge color for toon shading.
    @rtype: string
    @return: A string representing the edge color.
    """

  def edgeAntiShift(intensity = None):
    """
    With the unified renderer, reduce intensity on boundaries.
    @type intensity: int (optional)
    @param intensity: must be between 0 - 255
    @rtype: int (if prototype is empty)
    @return: Current edge antishift for the scene.
    """

  def enableOversampling(toggle):
    """
    Enable/disable oversampling (anit-aliasing).
    @type toggle: int
    @param toggle: pass 1 for on / 0 for off
    """

  def setOversamplingLevel(level):
    """
    Set the edge color for toon shading.
    @type level: int
    @param level: can be either 5, 8, 11, or 16
    """

  def enableMotionBlur(toggle):
    """
    Enable/disable MBlur.
    @type toggle: int
    @param toggle: pass 1 for on / 0 for off
    """

  def motionBlurLevel(level = None):
    """
    Get/set the length of shutter time for motion blur.
    @type level: float (optional)
    @param level: must be between 0.01 - 5.0
    @rtype: float (if prototype is empty)
    @return: Current MBlur for the scene.
    """

  def partsX(parts = None):
    """
    Get/set the number of parts to divide the render in the X direction.
    @type parts: int (optional)
    @param parts: must be between 1 - 64
    @rtype: int (if prototype is empty)
    @return: Current number of parts in the X for the scene.
    """

  def partsY(parts = None):
    """
    Get/set the number of parts to divide the render in the Y direction.
    @type parts: int (optional)
    @param parts: must be between 1 - 64
    @rtype: int (if prototype is empty)
    @return: Current number of parts in the Y for the scene.
    """

  def enableSky():
    """
    Enable render background with sky.
    """

  def enablePremultiply():
    """
    Enable premultiply alpha.
    """

  def enableKey():
    """
    Enable alpha and colour values remain unchanged.
    """

  def enableShadow(toggle):
    """
    Enable/disable shadow calculation.
    @type toggle: int
    @param toggle: pass 1 for on / 0 for off
    """

  def enableEnvironmentMap(toggle):
    """
    Enable/disable environment map rendering.
    @type toggle: int
    @param toggle: pass 1 for on / 0 for off
    """

  def enableRayTracing(toggle):
    """
    Enable/disable ray tracing.
    @type toggle: int
    @param toggle: pass 1 for on / 0 for off
    """

  def enableRadiosityRender(toggle):
    """
    Enable/disable radiosity rendering.
    @type toggle: int
    @param toggle: pass 1 for on / 0 for off
    """

  def enablePanorama(toggle):
    """
    Enable/disable panorama rendering (output width is multiplied by Xparts).
    @type toggle: int
    @param toggle: pass 1 for on / 0 for off
    """

  def setRenderWinSize(size):
    """
    Set the size of the render window.
    @type size: int
    @param size: can be 25, 50, 75 or 100 (percent).
    """

  def enableFieldRendering(toggle):
    """
    Enable/disable field rendering
    @type toggle: int
    @param toggle: pass 1 for on / 0 for off
    """

  def enableOddFieldFirst(toggle):
    """
    Enable/disable Odd field first rendering (Default: Even field).
    @type toggle: int
    @param toggle: pass 1 for on / 0 for off
    """

  def enableFieldTimeDisable(toggle):
    """
    Enable/disable time difference in field calculations.
    @type toggle: int
    @param toggle: pass 1 for on / 0 for off
    """

  def enableGaussFilter(toggle):
    """
    Enable/disable Gauss sampling filter for antialiasing.
    @type toggle: int
    @param toggle: pass 1 for on / 0 for off
    """

  def enableBorderRender(toggle):
    """
    Enable/disable small cut-out rendering.
    @type toggle: int
    @param toggle: pass 1 for on / 0 for off
    """

  def enableGammaCorrection(toggle):
    """
    Enable/disable gamma correction.
    @type toggle: int
    @param toggle: pass 1 for on / 0 for off
    """

  def gaussFilterSize(size = None):
    """
    Get/sets the Gauss filter size.
    @type size: float (optional)
    @param size: must be between 0.5 - 1.5
    @rtype: float (if prototype is empty)
    @return: Current gauss filter size for the scene.
    """

  def startFrame(frame = None):
    """
    Get/set the starting frame for sequence rendering.
    @type frame: int (optional)
    @param frame: must be between 1 - 18000
    @rtype: int (if prototype is empty)
    @return: Current starting frame for the scene.
    """

  def endFrame(frame = None):
    """
    Get/set the ending frame for sequence rendering.
    @type frame: int (optional)
    @param frame: must be between 1 - 18000
    @rtype: int (if prototype is empty)
    @return: Current ending frame for the scene.
    """

  def imageSizeX(size = None):
    """
    Get/set the image width in pixels.
    @type size: int (optional)
    @param size: must be between 4 - 10000
    @rtype: int (if prototype is empty)
    @return: Current image width for the scene.
    """

  def imageSizeY(size = None):
    """
    Get/set the image height in pixels.
    @type size: int (optional)
    @param size: must be between 4 - 10000
    @rtype: int (if prototype is empty)
    @return: Current image height for the scene.
    """

  def aspectRatioX(ratio = None):
    """
    Get/set the horizontal aspect ratio.
    @type ratio: int (optional)
    @param ratio: must be between 1 - 200
    @rtype: int (if prototype is empty)
    @return: Current horizontal aspect ratio for the scene.
    """

  def aspectRatioY(ratio = None):
    """
    Get/set the vertical aspect ratio.
    @type ratio: int (optional)
    @param ratio: must be between 1 - 200
    @rtype: int (if prototype is empty)
    @return: Current vertical aspect ratio for the scene.
    """

  def setRenderer(type):
    """
    Get/set which renderer to render the output.
    @type type: enum constant
    @param type: must be one of 2 constants:
        - INTERN: Blender's internal renderer
        - YAFRAY: Yafray renderer
    """

  def enableCropping(toggle):
    """
    Enable/disable exclusion of border rendering from total image.
    @type toggle: int
    @param toggle: pass 1 for on / 0 for off
    """

  def setImageType(type):
    """
    Set the type of image to output from the render.
    @type type: enum constant
    @param type: must be one of 13 constants:
        - AVIRAW: Uncompressed AVI files. AVI is a commonly used format on Windows plattforms
        - AVIJPEG: AVI movie w/ Jpeg images
        - AVICODEC: AVI using win32 codec
        - QUICKTIME: Quicktime movie (if enabled)
        - TARGA: Targa files
        - RAWTGA: Raw Targa files
        - PNG: Png files
        - BMP: Bitmap files
        - JPEG90: Jpeg files
        - HAMX: Hamx files
        - IRIS: Iris files
        - IRIZ: Iris + z-buffer files
        - FTYPE: Ftype file
    """

  def quality(qual = None):
    """
    Get/set quality get/setting for JPEG images, AVI Jpeg and SGI movies.
    @type qual: int (optional)
    @param qual: must be between 10 - 100
    @rtype: int (if prototype is empty)
    @return: Current image quality for the scene.
    """

  def framesPerSec(qual = None):
    """
    Get/set frames per second.
    @type qual: int (optional)
    @param qual: must be between 1 - 120
    @rtype: int (if prototype is empty)
    @return: Current frames per second for the scene.
    """

  def enableGrayscale():
    """
    Images are saved with BW (grayscale) data.
    """

  def enableRGBColor():
    """
    Images are saved with RGB (color) data.
    """

  def enableRGBAColor():
    """
    Images are saved with RGB and Alpha data (if supported).
    """

  def sizePreset(type):
    """
    Set the renderer to one of a few presets.
    @type type: enum constant
    @param type: must be one of 8 constants:
        - PAL: The European video standard: 720 x 576 pixels, 54 x 51 aspect.
        - FULL: For large screens: 1280 x 1024 pixels. 
        - PREVIEW: For preview rendering: 320 x 256 pixels.
        - PAL169: Wide-screen PAL.
        - DEFAULT: Like "PAL", but here the render settings are also set.
        - PANO: Panorama render.
        - NTSC: For TV playback.
        - PC: For standard PC graphics: 640 x 480 pixels.
    """

  def enableUnifiedRenderer(toggle):
    """
    Use the unified renderer.
    @type toggle: int
    @param toggle: pass 1 for on / 0 for off
    """

  def setYafrayGIQuality(type):
    """
    Set yafray global Illumination quality.
    @type type: enum constant
    @param type: must be one of 6 constants:
        - NONE
        - LOW
        - MEDIUM
        - HIGH
        - HIGHER
        - BEST
    """

  def setYafrayGIMethod(type):
    """
    Set yafray global Illumination method.
    @type type: enum constant
    @param type: must be one of 3 constants:
        - NONE: Dont use GI illumination
        - SKYDOME: Use Skydome method
        - GIFULL: Use Full method
    """

  def yafrayGIPower(power = None):
    """
    Get/set GI lighting intensity scale.
    YafrayMethod must be either SKYDOME or GIFULL.
    @type power: float (optional)
    @param power: must be between 0.01 - 100.0
    @rtype: float (if prototype is empty)
    @return: Current yafray global illumination intensity for the scene.
    """

  def yafrayGIDepth(depth = None):
    """
    Get/set number of bounces of the indirect light.
    YafrayMethod must be GIFULL.
    @type depth: int (optional)
    @param depth: must be between 1 - 8
    @rtype: int (if prototype is empty)
    @return: Current yafray global illumination light bounces for the scene.
    """

  def yafrayGICDepth(depth = None):
    """
    Get/set number of bounces inside objects (for caustics).
    YafrayMethod must be GIFULL.
    @type depth: int (optional)
    @param depth: must be between 1 - 8
    @rtype: int (if prototype is empty)
    @return: Current yafray global illumination inside light bounces for the scene.
    """

  def enableYafrayGICache(toggle):
    """
    Enable/disable cache irradiance samples (faster).
    YafrayMethod must be GIFULL.
    @type toggle: int
    @param toggle: pass 1 for on / 0 for off
    """

  def enableYafrayGIPhotons(toggle):
    """
    Enable/disable use of global photons to help in GI.
    YafrayMethod must be GIFULL.
    @type toggle: int
    @param toggle: pass 1 for on / 0 for off
    """

  def yafrayGIPhotonCount(count = None):
    """
    Get/set number of photons to shoot.
    YafrayMethod must be GIFULL and Photons enabled.
    @type count: int (optional)
    @param count: must be between 0 - 10000000
    @rtype: int (if prototype is empty)
    @return: Current number of photons to shoot for the scene.
    """


  def yafrayGIPhotonRadius(radius = None):
    """
    Get/set radius to search for photons to mix (blur).
    YafrayMethod must be GIFULL and Photons enabled.
    @type radius: float (optional)
    @param radius: must be between 0.00001 - 100.0
    @rtype: float (if prototype is empty)
    @return: Current photon search radius for the scene.
    """


  def yafrayGIPhotonMixCount(count = None):
    """
    Get/set number of photons to keep inside radius.
    YafrayMethod must be GIFULL and Photons enabled.
    @type count: int (optional)
    @param count: must be between 0 - 1000
    @rtype: int (if prototype is empty)
    @return: Current number of photons to keep inside radius for the scene.
    """

  def enableYafrayGITunePhotons(toggle):
    """
    Enable/disable show the photonmap directly in the render for tuning.
    YafrayMethod must be GIFULL and Photons enabled.
    @type toggle: int
    @param toggle: pass 1 for on / 0 for off
    """

  def yafrayGIShadowQuality(qual = None):
    """
    Get/set the shadow quality, keep it under 0.95.
    YafrayMethod must be GIFULL and Cache enabled.
    @type qual: float (optional)
    @param qual: must be between 0.01 - 1.0
    @rtype: float (if prototype is empty)
    @return: Current shadow quality for the scene.
    """

  def yafrayGIPixelsPerSample(pixels = None):
    """
    Get/set maximum number of pixels without samples, the lower the better and slower.
    YafrayMethod must be GIFULL and Cache enabled.
    @type pixels: int (optional)
    @param pixels: must be between 1 - 50
    @rtype: int (if prototype is empty)
    @return: Current number of pixels without samples for the scene.
    """

  def enableYafrayGIGradient(toggle):
    """
    Enable/disable try to smooth lighting using a gradient.
    YafrayMethod must be GIFULL and Cache enabled.
    @type toggle: int
    @param toggle: pass 1 for on / 0 for off
    """ 

  def yafrayGIRefinement(refine = None):
    """
    Get/set threshold to refine shadows EXPERIMENTAL. 1 = no refinement.
    YafrayMethod must be GIFULL and Cache enabled.
    @type refine: float (optional)
    @param refine: must be between 0.001 - 1.0
    @rtype: float (if prototype is empty)
    @return: Current threshold to refine shadows for the scene.
    """

  def yafrayRayBias(bias = None):
    """
    Get/set shadow ray bias to avoid self shadowing.
    @type bias: float (optional)
    @param bias: must be between 0 - 10.0
    @rtype: float (if prototype is empty)
    @return: Current ray bias for the scene.
    """

  def yafrayRayDepth(depth = None):
    """
    Get/set maximum render ray depth from the camera.
    @type depth: int (optional)
    @param depth: must be between 1 - 80
    @rtype: int (if prototype is empty)
    @return: Current ray depth for the scene.
    """

  def yafrayGamma(gamma = None):
    """
    Get/set gamma correction, 1 is off.
    @type gamma: float (optional)
    @param gamma: must be between 0.001 - 5.0
    @rtype: float (if prototype is empty)
    @return: Current gamma correction for the scene.
    """

  def yafrayExposure(expose = None):
    """
    Get/set exposure adjustment, 0 is off.
    @type expose: float (optional)
    @param expose: must be between 0 - 10.0
    @rtype: float (if prototype is empty)
    @return: Current exposure adjustment for the scene.
    """

  def yafrayProcessorCount(count = None):
    """
    Get/set number of processors to use.
    @type count: int (optional)
    @param count: must be between 1 - 8
    @rtype: int (if prototype is empty)
    @return: Current number of processors for the scene.
    """

  def enableGameFrameStretch():
    """
    Enble stretch or squeeze the viewport to fill the display window.
    """

  def enableGameFrameExpose():
    """
    Enable show the entire viewport in the display window, viewing more 
    horizontally or vertically.
    """

  def enableGameFrameBars():
    """
    Enable show the entire viewport in the display window, using bar 
    horizontally or vertically.
    """

  def setGameFrameColor(red, green, blue):
    """
    Set the red, green, blue component of the bars.
    @type red: float
    @param red: must be between 0 - 1.0
    @type green: float
    @param green: must be between 0 - 1.0
    @type blue: float
    @param blue: must be between 0 - 1.0
    """

  def getGameFrameColor():
    """
    Set the red, green, blue component of the bars.
    @rtype: string
    @return: A string representing the color component of the bars.
    """

  def gammaLevel(level = None):
    """
    Get/set the gamma value for blending oversampled images (1.0 = no correction).
    Unified renderer must be enabled.
    @type level: float (optional)
    @param level: must be between 0.2 - 5.0
    @rtype: float (if prototype is empty)
    @return: Current gamma value for the scene.
    """

  def postProcessAdd(add = None):
    """
    Get/set post processing add.
    Unified renderer must be enabled.
    @type add: float (optional)
    @param add: must be between -1.0 - 1.0
    @rtype: float (if prototype is empty)
    @return: Current processing add value for the scene.
    """

  def postProcessMultiply(mult = None):
    """
    Get/set post processing multiply.
    Unified renderer must be enabled.
    @type mult: float (optional)
    @param mult: must be between 0.01 - 4.0
    @rtype: float (if prototype is empty)
    @return: Current processing multiply value for the scene.
    """

  def postProcessGamma(gamma = None):
    """
    Get/set post processing gamma.
    Unified renderer must be enabled.
    @type gamma: float (optional)
    @param gamma: must be between 0.2 - 2.0
    @rtype: float (if prototype is empty)
    @return: Current processing gamma value for the scene.
    """

  def SGIMaxsize(size = None):
    """
    Get/set maximum size per frame to save in an SGI movie.
    SGI must be defined on your machine.
    @type size: int (optional)
    @param size: must be between 0 - 500
    @rtype: int (if prototype is empty)
    @return: Current SGI maximum size per frame for the scene.
    """

  def enableSGICosmo(toggle):
    """
    Enable/disable attempt to save SGI movies using Cosmo hardware
    SGI must be defined on your machine.
    @type toggle: int
    @param toggle: pass 1 for on / 0 for off
    """

  def oldMapValue(value = None):
    """
    Get/set specify old map value in frames.
    @type value: int (optional)
    @param value: must be between 1 - 900
    @rtype: int (if prototype is empty)
    @return: Current old map value for the scene.
    """

  def newMapValue(value = None):
    """
    Get/set specify new map value in frames.
    @type value: int (optional)
    @param value: must be between 1 - 900
    @rtype: int (if prototype is empty)
    @return: Current new map value for the scene.
    """
