# Blender.Scene.Render module and the RenderData PyType object

"""
The Blender.Scene.Render submodule.

Scene.Render
============

This module provides access to B{Scene Rendering Contexts} in Blender.

Example::
  import Blender
  from Blender import *
  from Blender.Scene import Render
  
  scn = Scene.GetCurrent()
  context = scn.getRenderingContext()
  
  Render.EnableDispWin()
  context.extensions = True
  context.renderPath = "//myRenderdir/"
  context.sizePreset(Render.PC)
  context.imageType = Render.AVIRAW
  context.sFrame = 2
  context.eFrame = 10
  context.renderAnim()
  
  context.imageType = Render.TARGA
  context.fps = 15
  context.sFrame = 15
  context.eFrame = 22
  context.renderAnim()
  
  Render.CloseRenderWindow()
  print context.fps
  print context.cFrame

@type Modes: readonly dictionary
@var Modes: Constant dict used for with L{RenderData.mode} bitfield attribute.  
Values can be ORed together.  Individual bits can also be set/cleared with
boolean attributes.
  - OSA: Oversampling (anti-aliasing) enabled
  - SHADOW: Shadow calculation enabled
  - GAMMA: Gamma correction enabled
  - ENVMAP: Environment map rendering enabled
  - TOONSHADING: Toon edge shading enabled
  - FIELDRENDER: Field rendering enabled
  - FIELDTIME: Time difference in field calculations I{disabled}
  - RADIOSITY: Radiosity rendering enabled
  - BORDER_RENDER: Small cut-out rendering enabled
  - PANORAMA: Panorama rendering enabled
  - CROP: Crop image during border renders
  - ODDFIELD: Odd field first rendering enabled
  - MBLUR: Motion blur enabled
  - UNIFIED: Unified Renderer enabled
  - RAYTRACING: Ray tracing enabled
  - THREADS: Render in two threads enabled (Deprecated, use L{RenderData.threads})

@type SceModes: readonly dictionary
@var SceModes: Constant dict used for with L{RenderData.sceneMode} bitfield attribute.  
Values can be ORed together.  Individual bits can also be set/cleared with
boolean attributes.
  - SEQUENCER: Enables sequencer output rendering.
  - EXTENSION: Adds extensions to the output when rendering animations.
  - SAVE_BUFFERS: Save render tiles to disk to save memory.
  - FREE_IMAGES: Free images used by textures after each render.

@type FramingModes: readonly dictionary
@var FramingModes: Constant dict used for with L{RenderData.gameFrame}
attribute.  One of the following modes can be active:
  - BARS: Show the entire viewport in the display window, using bar
    horizontally or vertically.
  - EXTEND: Show the entire viewport in the display window, viewing more
    horizontally or vertically
  - SCALE: Stretch or squeeze the viewport to fill the display window.

@type BakeModes: readonly dictionary
@var BakeModes: Constant dict used for with L{RenderData.bakeMode}
attribute.  One of the following modes can be active:
  - LIGHT: Bake lighting only.
  - ALL: Bake all render lighting.
  - AO: Bake ambient occlusion.
  - NORMALS: Bake a normal map.
  - TEXTURE: Bake textures.
  - DISPLACEMENT: Bake displacement.

@type BakeNormalSpaceModes: readonly dictionary
@var BakeNormalSpaceModes: Constant dict used for with L{RenderData.bakeNormalSpace}
attribute.  One of the following modes can be active:
  - CAMERA: Bake normals relative to the camera.
  - WORLD: Bake normals in worldspace.
  - OBJECT: Bake normals relative to the object.
  - TANGENT: Bake tangent space normals.

@var INTERNAL: The internal rendering engine. Use with setRenderer()
@var YAFRAY: Yafray rendering engine. Use with setRenderer()

@var AVIRAW: Output format. Use with renderdata.imageType / setImageType()
@var AVIJPEG: Output format. Use with renderdata.imageType / setImageType()
@var AVICODEC: Output format. Use with renderdata.imageType / setImageType()
@var QUICKTIME: Output format. Use with renderdata.imageType / setImageType()
@var TARGA: Output format. Use with renderdata.imageType / setImageType()
@var RAWTGA: Output format. Use with renderdata.imageType / setImageType()
@var HDR: Output format. Use with renderdata.imageType / setImageType()
@var PNG: Output format. Use with renderdata.imageType / setImageType()
@var BMP: Output format. Use with renderdata.imageType / setImageType()
@var JPEG: Output format. Use with renderdata.imageType / setImageType()
@var HAMX: Output format. Use with renderdata.imageType / setImageType()
@var IRIS: Output format. Use with renderdata.imageType / setImageType()
@var IRISZ: Output format. Use with renderdata.imageType / setImageType()
@var FTYPE: Output format. Use with renderdata.imageType / setImageType()
@var OPENEXR: Output format. Use with renderdata.imageType / setImageType()
@var MULTILAYER: Output format. Use with renderdata.imageType / setImageType()
@var TIFF: Output format. Use with renderdata.imageType / setImageType()
@var FFMPEG: Output format. Use with renderdata.imageType / setImageType()
@var CINEON: Output format. Use with renderdata.imageType / setImageType()
@var DPX: Output format. Use with renderdata.imageType / setImageType()

@var PAL: Output format. Use with renderdata.sizePreset()
@var NTSC: Output format. Use with renderdata.sizePreset()
@var DEFAULT: Output format. Use with renderdata.sizePreset()
@var PREVIEW: Output format. Use with renderdata.sizePreset()
@var PC: Output format. Use with renderdata.sizePreset()
@var PAL169: Output format. Use with renderdata.sizePreset()
@var B_PR_FULL: Output format. Use with renderdata.sizePreset()

@var NONE: Yafray GI Quality / Method. Use with renderdata.setYafrayGIQuality()
@var LOW: Yafray GI Quality. Use with renderdata.setYafrayGIQuality()
@var MEDIUM: Yafray GI Quality. Use with renderdata.setYafrayGIQuality()
@var HIGH: Yafray GI Quality. Use with renderdata.setYafrayGIQuality()
@var HIGHER: Yafray GI Quality. Use with renderdata.setYafrayGIQuality()
@var BEST: Yafray GI Quality. Use with renderdata.setYafrayGIQuality()
@var USEAOSETTINGS: Yafray GI Quality. Use with renderdata.setYafrayGIQuality()
@var SKYDOME: Yafray GI Method. Use with renderdata.setYafrayGIMethod()
@var GIFULL: Yafray GI Method. Use with renderdata.setYafrayGIMethod()
"""

def CloseRenderWindow():
  """
  Closes the rendering window.
  """

def EnableDispView():
  """
  Render in the 3d View area.  B{Note} this method is deprecated; 
  use the l{displayMode} attribute instead.
  """

def EnableDispWin():
  """
  Render in Render window.
  B{Note} this method is deprecated; use the l{displayMode} attribute instead.
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

class RenderData:
  """
  The RenderData object
  =====================
  This object gives access to Scene rendering contexts in Blender.
  @ivar unified: Unified Renderer enabled.  
  Also see B{UNIFIED} in L{Modes} constant dict.
  @type unified: boolean
  @ivar renderwinSize: Size of the rendering window. Valid values are 25, 50,
  75, or 100.
  @type renderwinSize: int
  @ivar xParts: Number of horizontal parts for image render.
  Values are clamped to the range [2,512].
  @type xParts: int
  @ivar fieldRendering: Field rendering enabled. 
  Also see B{FIELDRENDER} in L{Modes} constant dict.
  @type fieldRendering: boolean
  @ivar gammaCorrection: Gamma correction enabled. 
  Also see B{GAMMA} in L{Modes} constant dict.
  @type gammaCorrection: boolean
  @ivar eFrame: Ending frame for rendering.
  Values are clamped to the range [1,MAXFRAME].
  @type eFrame: int
  @ivar radiosityRender: Radiosity rendering enabled.
  @type radiosityRender: boolean
  @ivar sizeX: Image width (in pixels).
  Values are clamped to the range [4,10000].
  @type sizeX: int
  @ivar shadow: Shadow calculation enabled. 
  Also see B{SHADOW} in L{Modes} constant dict.
  @type shadow: boolean
  @ivar aspectX: Horizontal aspect ratio.
  Values are clamped to the range [1,200].
  @type aspectX: int
  @ivar mode: Mode bitfield.  See L{Modes} constant dict for values.
  @type mode: bitfield
  @ivar fieldTimeDisable: Time difference in field calculations I{disabled}.
  @type fieldTimeDisable: int
  @ivar cFrame: The current frame for rendering.
  Values are clamped to the range [1,MAXFRAME].
  @type cFrame: int
  @ivar crop: Crop image during border renders. 
  Also see B{CROP} in L{Modes} constant dict.
  @type crop: boolean
  @ivar sFrame: Starting frame for rendering.
  Values are clamped to the range [1,MAXFRAME].
  @type sFrame: int
  @ivar backbuf: Backbuffer image enabled.
  @type backbuf: boolean
  @ivar OSALevel: Oversampling (anti-aliasing) level.  Valid values are
  5, 8, 11, or 16.
  @type OSALevel: int
  @ivar displayMode: Render output in separate window or 3D view.
  Valid values are 0 (display in image editor view), 1 (display in render
  window), or 2 (display full screen).
  @type displayMode: int
  @ivar threads: Number of threads to render, clamed [1-8]
  @type threads: int 
  @ivar backbufPath: Path to a background image (setting loads image).
  @type backbufPath: string
  @ivar toonShading: Toon edge shading enabled. 
  Also see B{TOONSHADING} in L{Modes} constant dict.
  @type toonShading: boolean
  @ivar sceneMode: Scene mode bitfield.  See L{SceModes} constant dict for
  values.
  @type sceneMode: bitfield
  @ivar gameFrameColor: RGB color triplet for bars.  
  Values are clamped in the range [0.0,1.0].
  @type gameFrameColor: list of RGB 3 floats
  @ivar sizeY: Image height (in pixels).
  Values are clamped to the range [4,10000].
  @type sizeY: int
  @ivar renderer: Rendering engine choice.  
  Valid values are 0 (internal) or 1 (Yafray).
  @type renderer: int
  
  @ivar sequencer: Enables sequencer output rendering.
  Also see B{SEQUENCER} in L{SceModes} constant dict.
  @type sequencer: boolean
  @ivar extensions: Add extensions to output (when rendering animations).
  Also see B{EXTENSION} in L{SceModes} constant dict.
  @type extensions: boolean
  @ivar compositor: 'Do Compositor' enabled.
  @type compositor: boolean
  @ivar freeImages: Texture images are freed after render.
  @type freeImages: boolean
  @ivar singleLayer: Only render the active layer.
  @type singleLayer: boolean
  @ivar activeLayer: The active render layer.  Must be in range[0,num render layers-1]
  @type activeLayer: int
  @ivar saveBuffers: Save render buffers to disk while rendering, saves memory.
  @type saveBuffers: boolean
  @ivar compositeFree: Free nodes that are not used while composite.
  @type compositeFree: boolean
  
  @ivar panorama: Panorama rendering enabled. 
  Also see B{PANORAMA} in L{Modes} constant dict.
  @type panorama: boolean
  @ivar rayTracing: Ray tracing enabled. 
  Also see B{RAYTRACING} in L{Modes} constant dict.
  @type rayTracing: boolean
  @ivar renderPath: The path to output the rendered images.
  @type renderPath: string
  @ivar gameFrame: Game framing type.  See L{FramingModes} constant dict.
  @type gameFrame: int
  @ivar aspectY: Vertical aspect ratio.
  Values are clamped to the range [1,200].
  @type aspectY: int
  @ivar imageType: File format for saving images.  See the module's constants
  for values.
  @type imageType: int
  @ivar ftypePath: The path to Ftype file.
  @type ftypePath: string
  @ivar border: The border for border rendering.  The format is
  [xmin,ymin,xmax,ymax].  Values are clamped to [0.0,1.0].
  @type border: list of 4 floats.
  @ivar edgeColor: RGB color triplet for edges in Toon shading (unified
  renderer).
  Values are clamped in the range [0.0,1.0].
  @type edgeColor: list of 3 RGB floats
  @ivar yParts: Number of vertical parts for image render.  
  Values are clamped to the range [2,512].
  @type yParts: int
  @ivar imagePlanes: Image depth in bits.  Valid values are 8, 24, or 32.
  @type imagePlanes: int
  @ivar borderRender: Small cut-out rendering enabled. 
  Also see B{BORDER_RENDER} in L{Modes} constant dict.
  @type borderRender: boolean
  @ivar oversampling: Oversampling (anti-aliasing) enabled. 
  Also see B{OSA} in L{Modes} constant dict.
  @type oversampling: boolean
  @ivar fps: Frames per second.
  Values are clamped to the range [1,120].
  @ivar fpsBase: Frames per second base: used to generate fractional frames
  per second values.  For example, setting fps to 30 and fps_base to 1.001
  will approximate the NTSC frame rate of 29.97 fps.
  Values are clamped to the range [1,120].
  @type fpsBase: float
  @ivar timeCode: Get the current frame in HH:MM:SS:FF format.  Read-only.
  @type timeCode: string
  @ivar environmentMap: Environment map rendering enabled. 
  Also see B{ENVMAP} in L{Modes} constant dict.
  @type environmentMap: boolean
  @ivar motionBlur: Motion blur enabled. 
  Also see B{MBLUR} in L{Modes} constant dict.
  @type motionBlur: boolean
  @ivar oddFieldFirst: Odd field first rendering enabled. 
  Also see B{ODDFIELD} in L{Modes} constant dict.
  @type oddFieldFirst: boolean
  @ivar alphaMode: Setting for sky/background.  Valid values are 0 (fill
  background with sky), 1 (multiply alpha in advance), or 2 (alpha and color
  values remain unchanged).
  @type alphaMode: int
  @ivar gaussFilter: Gauss filter size.
  Values are clamped to the range [0.5,1.5].
  @type gaussFilter: float
  @ivar mblurFactor: Motion blur factor.
  Values are clamped to the range [0.01,5.0].
  @type mblurFactor: float
  @ivar mapOld: Number of frames the Map Old will last
  Values are clamped to the range [1,900].
  @type mapOld: int
  @ivar mapNew: New mapping value (in frames).
  Values are clamped to the range [1,900].
  @type mapNew: int
  @ivar set: The scene linked as a set to this scene.  Values are an existing
  scene or None (setting to None clears the set).  The scene argument cannot
  cause a circular link.
  @type set: BPy_Scene or None
  @ivar yafrayGIMethod: Global Illumination method.
  Valid values are NONE (0), SKYDOME (1) or FULL (2).
  @type yafrayGIMethod: int {NONE (0), SKYDOME (1), GIFULL (2)}
  @ivar yafrayGIQuality: Global Illumination quality.
  @type yafrayGIQuality: int {NONE (0), LOW (1), MEDIUM (2), HIGH (3), HIGHER (4), BEST (5), USEAOSETTINGS (6)}
  @ivar yafrayExportToXML: If true export to an xml file and call yafray instead of plugin.
  @type yafrayExportToXML: boolean
  @ivar yafrayAutoAntiAliasing: Automatic anti-aliasing enabled/disabled.
  @type yafrayAutoAntiAliasing: boolean
  @ivar yafrayClampRGB: Clamp RGB enabled/disabled.
  @type yafrayClampRGB: boolean
  @ivar yafrayAntiAliasingPasses: Number of anti-aliasing passes (0 is no Anti-Aliasing).
  @type yafrayAntiAliasingPasses: int [0, 64]
  @ivar yafrayAntiAliasingSamples: Number of samples per pass.
  @type yafrayAntiAliasingSamples: int [0, 2048]
  @ivar yafrayAntiAliasingPixelSize: Anti-aliasing pixel filter size.
  @type yafrayAntiAliasingPixelSize: float [1.0, 2.0]
  @ivar yafrayAntiAliasingThreshold: Anti-aliasing threshold.
  @type yafrayAntiAliasingThreshold: float [0.05, 1.0]
  @ivar yafrayGICache: Cache occlusion/irradiance samples (faster).
  @type yafrayGICache: boolean
  @ivar yafrayGICacheBumpNormals: Enable/disable bumpnormals for cache.
  @type yafrayGICacheBumpNormals: boolean
  @ivar yafrayGICacheShadowQuality: Shadow quality, keep it under 0.95 :-).
  @type yafrayGICacheShadowQuality: float [0.01, 1.0]
  @ivar yafrayGICachePixelsPerSample: Maximum number of pixels without samples, the lower the better and slower.
  @type yafrayGICachePixelsPerSample: int [1, 50]
  @ivar yafrayGICacheRefinement: Threshold to refine shadows EXPERIMENTAL. 1 = no refinement.
  @type yafrayGICacheRefinement: float [0.001, 1.0]
  @ivar yafrayGIPhotons: Enable/disable use of global photons to help in GI.
  @type yafrayGIPhotons: boolean
  @ivar yafrayGITunePhotons: If true the photonmap is shown directly in the render for tuning.
  @type yafrayGITunePhotons: boolean
  @ivar bakeMode: The method used when baking, see L{BakeModes}.
  @type bakeMode: int
  @ivar bakeNormalSpace: The method used when baking, see L{BakeNormalSpaceModes}.
  @type bakeNormalSpace: int
  @ivar bakeClear: When enabled, baking clears the image first.
  @type bakeClear: bool
  @ivar bakeToActive: When enabled, selected objects are baked onto the active object.
  @type bakeToActive: bool
  @ivar bakeNormalize: Normalize AO and displacement to the range of the distance value.
  @type bakeNormalize: bool  
  @ivar bakeMargin: The pixel distance to extend baked pixels past the boundry (reduces bleeding when mipmapping)
  @type bakeMargin: int
  @ivar bakeDist: The distance in blender units to use when bakeToActive is enabled and geomtry does not overlap.
  @type bakeDist: float
  @ivar bakeBias: The distance in blender units to bias faces further away from the object.
  @type bakeBias: float
  @ivar halfFloat: When enabled use 16bit floats rather then 32bit for OpenEXR files.
  @type halfFloat: bool
  @ivar zbuf: When enabled, save the zbuffer with an OpenEXR file
  @type zbuf: bool
  @ivar preview: When enabled, save a preview jpeg with an OpenEXR file
  @type preview: bool
  @ivar touch: Create an empty file before rendering it.
  @type touch: bool
  @ivar noOverwrite: Skip rendering frames when the file exists.
  @type noOverwrite: bool
  """
  
  def currentFrame(frame = None):
    """
    Get/set the current frame.
    @type frame: int (optional)
    @param frame: must be between 1 - 30000
    @rtype: int (if prototype is empty)
    @return: Current frame for the scene.
    """
 
  def render():
    """
    Render the scene.
    """

  def bake():
    """
    Bake selected objects in the scene.
    """

  def renderAnim():
    """
    Render a series of frames to an output directory.
    """

  def saveRenderedImage(filename, zbuffer=0):
    """
    Saves the image rendered using RenderData.render() to the filename and path
    given in the variable 'filename'.
    
    Make sure the filename you provide makes a valid path when added to the "render path"
    (setRenderPath/getRenderPath) to make up the absolute path.
    If you want to render to a new absolute path just set the renderpath to an
    empty string and use the absolute path as the filename.
    @param filename: The path+filename for the rendered image.
    @type zbuffer: int 
    @param zbuffer: Whether or not to render the zbuffer along with the image.
    @type filename: string 
    @since: 2.40
    @requires: You must have an image currently rendered before calling this method
    @warning: This wont work in background mode. use renderAnim instead.
    """

  def play():
    """
    play animation of rendered images/avi (searches Pics: field).
    """

  def getTimeCode():
    """
    Get the current frame as a string in HH:MM:SS:FF format
    @rtype: string 
    @return: current frame as a string in HH:MM:SS:FF format
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

  def getFrameFilename( frame ):
    """
    Get the filename used for the remdered image.
    @type frame: int
    @param frame: the frame to use in the filename, if no argument given, use the current frame.
    @rtype: string
    @return: Returns the filename that blender would render to, taking into account output path, extension and frame number.
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
    Enable/disable oversampling (anti-aliasing).
    @type toggle: int
    @param toggle: pass 1 for on / 0 for off
    """

  def setOversamplingLevel(level):
    """
    Set the level of over-sampling (anti-aliasing).
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
    Enable alpha and color values remain unchanged.
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

  def getRenderWinSize():
    """
    Get the size of the render window.
    @rtype: int
    @return: window size; can be 25, 50, 75 or 100 (percent).
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
    Enable/disable Gauss sampling filter for anti-aliasing.
    @type toggle: int
    @param toggle: pass 1 for on / 0 for off
    """

  def enableBorderRender(toggle):
    """
    Enable/disable small cut-out rendering.
    @type toggle: int
    @param toggle: pass 1 for on / 0 for off
    """

  def setBorder(left,bottom,right,top):
    """
    Set a border for rendering from cameras in the scene.
    The left,bottom coordinates and right,top coordinates
    define the size of the border. (0,0,1,1) will set the border
    to the whole camera. (0,0) lower left and (1,1) upper right.
    @type left: float
    @param left: float between 0 and 1
    @type right: float
    @param right: float between 0 and 1
    @type bottom: float
    @param bottom: float between 0 and 1
    @type top: float
    @param top: float between 0 and 1
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
    @param frame: must be a valid Blender frame number.
    @rtype: int (if prototype is empty)
    @return: Current starting frame for the scene.
    """

  def endFrame(frame = None):
    """
    Get/set the ending frame for sequence rendering.
    @type frame: int (optional)
    @param frame: must be a valid Blender frame number.
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
    Deprecated: see the L{crop} attribute.
    """

  def setImageType(type):
    """
    Set the type of image to output from the render.
    @type type: enum constant
    @param type: must be one of 13 constants:
        - AVIRAW: Uncompressed AVI files. AVI is a commonly used format on Windows platforms
        - AVIJPEG: AVI movie w/ JPEG images
        - AVICODEC: AVI using win32 codec
        - QUICKTIME: Quicktime movie (if enabled)
        - TARGA: Targa files
        - RAWTGA: Raw Targa files
        - PNG: Png files
        - BMP: Bitmap files
        - JPEG90: JPEG files
        - HAMX: Hamx files
        - IRIS: Iris files
        - IRIZ: Iris + z-buffer files
        - FTYPE: Ftype file
    """

  def quality(qual = None):
    """
    Get/set quality get/setting for JPEG images, AVI JPEG and SGI movies.
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
    Images are saved with black and white (grayscale) data.
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
        - USEAOSETTINGS
    """

  def getYafrayGIQuality():
    """
    Get yafray global Illumination quality.
    @rtype: enum constant
    @return: one of 6 constants:
        - NONE
        - LOW
        - MEDIUM
        - HIGH
        - HIGHER
        - BEST
        - USEAOSETTINGS
    """

  def setYafrayGIMethod(type):
    """
    Set yafray global Illumination method.
    @type type: enum constant
    @param type: must be one of 3 constants:
        - NONE: Do not use GI illumination
        - SKYDOME: Use Skydome method
        - GIFULL: Use Full method
    """

  def getYafrayGIMethod():
    # (dietrich) 2007/06/01
    """
    Get yafray global Illumination method.
    @rtype: enum constant - 
    @return: Current yafray global illumination method:
        - NONE: Do not use GI illumination
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

  def yafrayGIIndirPower(power = None):
    """
    Get/set GI indirect lighting intensity scale.
    @type power: float (optional)
    @param power: must be between 0.01 - 100.0
    @rtype: float (if prototype is empty)
    @return: Current yafray indirect illumination intensity for the scene.
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
    Enable/disable show the photon map directly in the render for tuning.
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

  def enableGameFrameStretch():
    """
    Enable stretch or squeeze the viewport to fill the display window.
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
