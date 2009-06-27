# Blender.BGL module (OpenGL wrapper)

"""
The Blender.BGL submodule (the OpenGL wrapper).

B{New}: some GLU functions: L{gluLookAt}, etc.

The Blender.BGL submodule
=========================
(when accessing it from the Game Engine use BGL instead of Blender.BGL)

This module wraps OpenGL constants and functions, making them available from
within Blender Python.

The complete list can be retrieved from the module itself, by listing its
contents: dir(Blender.BGL).  A simple search on the net can point to more 
than enough material to teach OpenGL programming, from books to many 
collections of tutorials.

The "red book": "I{OpenGL Programming Guide: The Official Guide to Learning
OpenGL}" and the online NeHe tutorials are two of the best resources.

Example::
  import Blender
  from Blender.BGL import *
  from Blender import Draw
  R = G = B = 0
  A = 1
  title = "Testing BGL  + Draw"
  instructions = "Use mouse buttons or wheel to change the background color."
  quitting = " Press ESC or q to quit."
  len1 = Draw.GetStringWidth(title)
  len2 = Draw.GetStringWidth(instructions + quitting)
  #
  def show_win():
    glClearColor(R,G,B,A)                # define color used to clear buffers 
    glClear(GL_COLOR_BUFFER_BIT)         # use it to clear the color buffer
    glColor3f(0.35,0.18,0.92)            # define default color
    glBegin(GL_POLYGON)                  # begin a vertex data list
    glVertex2i(165, 158)
    glVertex2i(252, 55)
    glVertex2i(104, 128)
    glEnd()
    glColor3f(0.4,0.4,0.4)               # change default color
    glRecti(40, 96, 60+len1, 113)
    glColor3f(1,1,1)
    glRasterPos2i(50,100)                # move cursor to x = 50, y = 100
    Draw.Text(title)                     # draw this text there
    glRasterPos2i(350,40)                # move cursor again
    Draw.Text(instructions + quitting)   # draw another msg
    glBegin(GL_LINE_LOOP)                # begin a vertex-data list
    glVertex2i(46,92)
    glVertex2i(120,92)
    glVertex2i(120,115)
    glVertex2i(46,115)
    glEnd()                              # close this list
  #
  def ev(evt, val):                      # event callback for Draw.Register()
    global R,G,B,A                       # ... it handles input events
    if evt == Draw.ESCKEY or evt == Draw.QKEY:
      Draw.Exit()                        # this quits the script
    elif not val: return
    elif evt == Draw.LEFTMOUSE: R = 1 - R
    elif evt == Draw.MIDDLEMOUSE: G = 1 - G
    elif evt == Draw.RIGHTMOUSE: B = 1 - B
    elif evt == Draw.WHEELUPMOUSE:
      R += 0.1
      if R > 1: R = 1
    elif evt == Draw.WHEELDOWNMOUSE:
      R -= 0.1
      if R < 0: R = 0
    else:
      return                             # don't redraw if nothing changed
    Draw.Redraw(1)                       # make changes visible.
  #
  Draw.Register(show_win, ev, None)      # start the main loop

@note: you can use the L{Image} module and L{Image.Image} BPy object to load
    and set textures.  See L{Image.Image.glLoad} and L{Image.Image.glFree},
    for example.
@see: U{www.opengl.org}
@see: U{nehe.gamedev.net}
"""

def glAccum(op, value):
  """
  Operate on the accumulation buffer
  @see: U{www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/accum.html}

  @type op: Enumerated constant
  @param op: The accumulation buffer operation. 
  @type value: float
  @param value: a value used in the accumulation buffer operation.
  """

def glAlphaFunc(func, ref):
  """
  Specify the alpha test function
  @see: U{www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/alphafunc.html}
  
  @type func: Enumerated constant
  @param func: Specifies the alpha comparison function. 
  @type ref: float
  @param ref: The reference value that incoming alpha values are compared to. 
  Clamped between 0 and 1.
  """

def glAreTexturesResident(n, textures, residences):
  """
  Determine if textures are loaded in texture memory
  @see: U{www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/aretexturesresident.html}

  @type n: int
  @param n: Specifies the number of textures to be queried.
  @type textures: Buffer object I{type GL_INT}
  @param textures: Specifies an array containing the names of the textures to be queried 
  @type residences: Buffer object I{type GL_INT}(boolean)
  @param residences: An array in which the texture residence status in returned.The residence status of a
  texture named by an element of textures is returned in the corresponding element of residences.
  """

def glBegin(mode):
  """
  Delimit the vertices of a primitive or a group of like primatives
  @see: U{www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/begin.html}

  @type mode: Enumerated constant
  @param mode: Specifies the primitive that will be create from vertices between glBegin and
  glEnd. 
  """

def glBindTexture(target, texture):
  """
  Bind a named texture to a texturing target
  @see: U{www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/bindtexture.html}

  @type target: Enumerated constant
  @param target: Specifies the target to which the texture is bound. 
  @type texture: unsigned int
  @param texture: Specifies the name of a texture.
  """

def glBitmap(width, height, xorig, yorig, xmove, ymove, bitmap):
  """
  Draw a bitmap
  @see: U{www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/bitmap.html}

  @type width, height: int
  @param width, height: Specify the pixel width and height of the bitmap image.
  @type xorig, yorig: float
  @param xorig, yorig: Specify the location of the origin in the bitmap image. The origin is measured
  from the lower left corner of the bitmap, with right and up being the positive axes.
  @type xmove, ymove: float
  @param xmove, ymove: Specify the x and y offsets to be added to the current raster position after 
  the bitmap is drawn. 
  @type bitmap: Buffer object I{type GL_BYTE}
  @param bitmap: Specifies the address of the bitmap image. 
  """

def glBlendFunc(sfactor, dfactor):
  """
  Specify pixel arithmetic
  @see: U{www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/blendfunc.html}

  @type sfactor: Enumerated constant
  @param sfactor: Specifies how the red, green, blue, and alpha source blending factors are 
  computed. 
  @type dfactor: Enumerated constant
  @param dfactor: Specifies how the red, green, blue, and alpha destination blending factors are 
  computed. 
  """

def glCallList(list):
  """
  Execute a display list
  @see: U{www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/calllist.html}

  @type list: unsigned int
  @param list: Specifies the integer name of the display list to be executed.
  """

def glCallLists(n, type, lists):
  """
  Execute a list of display lists
  @see: U{www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/calllists.html}

  @type n: int
  @param n: Specifies the number of display lists to be executed. 
  @type type: Enumerated constant
  @param type: Specifies the type of values in lists. 
  @type lists: Buffer object
  @param lists: Specifies the address of an array of name offsets in the display list. 
  The pointer type is void because the offsets can be bytes, shorts, ints, or floats, 
  depending on the value of type.
  """

def glClear(mask):
  """
  Clear buffers to preset values
  @see: U{www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/clear.html}

  @type mask: Enumerated constant(s)
  @param mask: Bitwise OR of masks that indicate the buffers to be cleared. 
  """

def glClearAccum(red, green, blue, alpha):
  """
  Specify clear values for the accumulation buffer
  @see: U{www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/clearaccum.html}

  @type red, green, blue, alpha: float
  @param red, green, blue, alpha: Specify the red, green, blue, and alpha values used when the 
  accumulation buffer is cleared. The initial values are all 0. 
  """

def glClearColor(red, green, blue, alpha):
  """
  Specify clear values for the color buffers
  @see: U{www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/clearcolor.html}

  @type red, green, blue, alpha: float
  @param red, green, blue, alpha: Specify the red, green, blue, and alpha values used when the 
  color buffers are cleared. The initial values are all 0. 
  """

def glClearDepth(depth):
  """
  Specify the clear value for the depth buffer
  @see: U{www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/cleardepth.html}

  @type depth: int
  @param depth: Specifies the depth value used when the depth buffer is cleared. 
  The initial value is 1.  
  """

def glClearIndex(c):
  """
  Specify the clear value for the color index buffers
  @see: U{www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/clearindex.html}

  @type c: float
  @param c: Specifies the index used when the color index buffers are cleared. 
  The initial value is 0. 
  """

def glClearStencil(s):
  """
  Specify the clear value for the stencil buffer
  @see: U{www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/clearstencil.html}

  @type s: int
  @param s: Specifies the index used when the stencil buffer is cleared. The initial value is 0. 
  """

def glClipPlane (plane, equation):
  """
  Specify a plane against which all geometry is clipped
  @see: U{www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/clipplane.html}

  @type plane: Enumerated constant
  @param plane: Specifies which clipping plane is being positioned. 
  @type equation: Buffer object I{type GL_FLOAT}(double)
  @param equation: Specifies the address of an array of four double- precision floating-point 
  values. These values are interpreted as a plane equation.
  """

def glColor (red, green, blue, alpha):
  """
  B{glColor3b, glColor3d, glColor3f, glColor3i, glColor3s, glColor3ub, glColor3ui, glColor3us, 
  glColor4b, glColor4d, glColor4f, glColor4i, glColor4s, glColor4ub, glColor4ui, glColor4us, 
  glColor3bv, glColor3dv, glColor3fv, glColor3iv, glColor3sv, glColor3ubv, glColor3uiv, 
  glColor3usv, glColor4bv, glColor4dv, glColor4fv, glColor4iv, glColor4sv, glColor4ubv, 
  glColor4uiv, glColor4usv}

  Set a new color.
  @see: U{www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/color.html}

  @type red, green, blue, alpha: Depends on function prototype. 
  @param red, green, blue: Specify new red, green, and blue values for the current color. 
  @param alpha: Specifies a new alpha value for the current color. Included only in the 
  four-argument glColor4 commands. (With '4' colors only)
  """

def glColorMask(red, green, blue, alpha):
  """
  Enable and disable writing of frame buffer color components
  @see: U{www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/colormask.html}

  @type red, green, blue, alpha: int (boolean)
  @param red, green, blue, alpha: Specify whether red, green, blue, and alpha can or cannot be 
  written into the frame buffer. The initial values are all GL_TRUE, indicating that the 
  color components can be written. 
  """

def glColorMaterial(face, mode):
  """
  Cause a material color to track the current color 
  @see: U{www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/colormaterial.html}

  @type face: Enumerated constant
  @param face: Specifies whether front, back, or both front and back material parameters should 
  track the current color. 
  @type mode: Enumerated constant
  @param mode: Specifies which of several material parameters track the current color. 
  """

def glCopyPixels(x, y, width, height, type):
  """
  Copy pixels in the frame buffer
  @see: U{www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/copypixels.html}

  @type x, y: int
  @param x, y: Specify the window coordinates of the lower left corner of the rectangular 
  region of pixels to be copied. 
  @type width, height: int
  @param width,height: Specify the dimensions of the rectangular region of pixels to be copied. 
  Both must be non-negative. 
  @type type: Enumerated constant
  @param type: Specifies whether color values, depth values, or stencil values are to be copied. 
  """

def glCullFace(mode):
  """
  Specify whether front- or back-facing facets can be culled 
  @see: U{www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/cullface.html}

  @type mode: Enumerated constant
  @param mode: Specifies whether front- or back-facing facets are candidates for culling. 
  """

def glDeleteLists(list, range):
  """
  Delete a contiguous group of display lists
  @see: U{www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/deletelists.html}

  @type list: unsigned int
  @param list: Specifies the integer name of the first display list to delete
  @type range: int
  @param range: Specifies the number of display lists to delete
  """

def glDeleteTextures(n, textures):
  """
  Delete named textures
  @see: U{www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/deletetextures.html}

  @type n: int
  @param n: Specifies the number of textures to be deleted
  @type textures: Buffer I{GL_INT}
  @param textures: Specifies an array of textures to be deleted
  """

def glDepthFunc(func):
  """
  Specify the value used for depth buffer comparisons 
  @see: U{www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/depthfunc.html}

  @type func: Enumerated constant
  @param func: Specifies the depth comparison function. 
  """

def glDepthMask(flag):
  """
  Enable or disable writing into the depth buffer
  @see: U{www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/depthmask.html}

  @type flag: int (boolean)
  @param flag: Specifies whether the depth buffer is enabled for writing. If flag is GL_FALSE,
  depth buffer writing is disabled. Otherwise, it is enabled. Initially, depth buffer 
  writing is enabled. 
  """

def glDepthRange(zNear, zFar):
  """
  Specify mapping of depth values from normalized device coordinates to window coordinates 
  @see: U{www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/depthrange.html}

  @type zNear: int
  @param zNear: Specifies the mapping of the near clipping plane to window coordinates. 
  The initial value is 0. 
  @type zFar: int
  @param zFar: Specifies the mapping of the far clipping plane to window coordinates. 
  The initial value is 1. 
  """

def glDisable(cap):
  """
  Disable server-side GL capabilities
  @see: U{www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/enable.html}

  @type cap: Enumerated constant
  @param cap: Specifies a symbolic constant indicating a GL capability.
  """

def glDrawBuffer(mode):
  """
  Specify which color buffers are to be drawn into
  @see: U{www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/drawbuffer.html}

  @type mode: Enumerated constant
  @param mode: Specifies up to four color buffers to be drawn into. 
  """

def glDrawPixels(width, height, format, type, pixels):
  """
  Write a block of pixels to the frame buffer
  @see: U{www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/drawpixels.html}

  @type width, height: int
  @param width, height: Specify the dimensions of the pixel rectangle to be 
  written into the frame buffer. 
  @type format: Enumerated constant
  @param format: Specifies the format of the pixel data. 
  @type type: Enumerated constant
  @param type: Specifies the data type for pixels. 
  @type pixels: Buffer object 
  @param pixels: Specifies a pointer to the pixel data. 
  """

def glEdgeFlag (flag):
  """
  B{glEdgeFlag, glEdgeFlagv}

  Flag edges as either boundary or non-boundary
  @see: U{www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/edgeflag.html}

  @type flag: Depends of function prototype
  @param flag: Specifies the current edge flag value.The initial value is GL_TRUE. 
  """

def glEnable(cap):
  """
  Enable server-side GL capabilities
  @see: U{www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/enable.html}

  @type cap: Enumerated constant
  @param cap: Specifies a symbolic constant indicating a GL capability.
  """

def glEnd():
  """
  Delimit the vertices of a primitive or group of like primitives
  @see: U{www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/begin.html}
  """

def glEndList():
  """
  Create or replace a display list
  @see: U{www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/newlist.html}
  """

def glEvalCoord (u,v):
  """
  B{glEvalCoord1d, glEvalCoord1f, glEvalCoord2d, glEvalCoord2f, glEvalCoord1dv, glEvalCoord1fv, 
  glEvalCoord2dv, glEvalCoord2fv}

  Evaluate enabled one- and two-dimensional maps
  @see: U{www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/evalcoord.html}

  @type u: Depends on function prototype.
  @param u: Specifies a value that is the domain coordinate u to the basis function defined 
  in a previous glMap1 or glMap2 command. If the function prototype ends in 'v' then
  u specifies a pointer to an array containing either one or two domain coordinates. The first 
  coordinate is u. The second coordinate is v, which is present only in glEvalCoord2 versions. 
  @type v: Depends on function prototype. (only with '2' prototypes)
  @param v: Specifies a value that is the domain coordinate v to the basis function defined 
  in a previous glMap2 command. This argument is not present in a glEvalCoord1 command. 
  """

def glEvalMesh (mode, i1, i2):
  """
  B{glEvalMesh1 or glEvalMesh2}

  Compute a one- or two-dimensional grid of points or lines
  @see: U{www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/evalmesh.html}

  @type mode: Enumerated constant
  @param mode: In glEvalMesh1, specifies whether to compute a one-dimensional 
  mesh of points or lines.
  @type i1, i2: int
  @param i1, i2: Specify the first and last integer values for the grid domain variable i.
  """

def glEvalPoint (i, j):
  """
  B{glEvalPoint1 and glEvalPoint2}

  Generate and evaluate a single point in a mesh
  @see: U{www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/evalpoint.html}

  @type i: int
  @param i: Specifies the integer value for grid domain variable i.
  @type j: int (only with '2' prototypes)
  @param j: Specifies the integer value for grid domain variable j (glEvalPoint2 only).
  """

def glFeedbackBuffer (size, type, buffer):
  """
  Controls feedback mode
  @see: U{www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/feedbackbuffer.html}

  @type size: int
  @param size:Specifies the maximum number of values that can be written into buffer. 
  @type type: Enumerated constant
  @param type:Specifies a symbolic constant that describes the information that 
  will be returned for each vertex. 
  @type buffer: Buffer object I{GL_FLOAT}
  @param buffer: Returns the feedback data. 
  """

def glFinish():
  """
  Block until all GL execution is complete
  @see: U{www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/finish.html}
  """

def glFlush():
  """
  Force Execution of GL commands in finite time
  @see: U{www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/flush.html}
  """

def glFog (pname, param):
  """
  B{glFogf, glFogi, glFogfv, glFogiv}

  Specify fog parameters
  @see: U{www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/fog.html}

  @type pname: Enumerated constant
  @param pname: Specifies a single-valued fog parameter. If the function prototype
  ends in 'v' specifies a fog parameter.
  @type param: Depends on function prototype.
  @param param: Specifies the value or values to be assigned to pname. GL_FOG_COLOR 
  requires an array of four values. All other parameters accept an array containing 
  only a single value. 
  """

def glFrontFace(mode):
  """
  Define front- and back-facing polygons
  @see: U{www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/frontface.html}

  @type mode: Enumerated constant
  @param mode: Specifies the orientation of front-facing polygons.
  """

def glFrustum(left, right, bottom, top, zNear, zFar):
  """
  Multiply the current matrix by a perspective matrix
  @see: U{www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/frustum.html}

  @type left, right: double (float)
  @param left, right: Specify the coordinates for the left and right vertical 
  clipping planes. 
  @type top, bottom: double (float)
  @param top, bottom: Specify the coordinates for the bottom and top horizontal 
  clipping planes. 
  @type zNear, zFar: double (float)
  @param zNear, zFar: Specify the distances to the near and far depth clipping planes. 
  Both distances must be positive. 
  """

def glGenLists(range):
  """
  Generate a contiguous set of empty display lists
  @see: U{www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/genlists.html}

  @type range: int
  @param range: Specifies the number of contiguous empty display lists to be generated. 
  """

def glGenTextures(n, textures):
  """
  Generate texture names
  @see: U{www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/gentextures.html}

  @type n: int
  @param n: Specifies the number of textures name to be generated.
  @type textures: Buffer object I{type GL_INT}
  @param textures: Specifies an array in which the generated textures names are stored.
  """

def glGet (pname, param):
  """
  B{glGetBooleanv, glGetfloatv, glGetFloatv, glGetIntegerv}

  Return the value or values of a selected parameter
  @see: U{www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/get.html}

  @type pname: Enumerated constant
  @param pname: Specifies the parameter value to be returned. 
  @type param: Depends on function prototype.
  @param param: Returns the value or values of the specified parameter. 
  """

def glGetClipPlane(plane, equation):
  """
  Return the coefficients of the specified clipping plane 
  @see: U{www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/getclipplane.html}
 
  @type plane: Enumerated constant
  @param plane: Specifies a clipping plane. The number of clipping planes depends on the 
  implementation, but at least six clipping planes are supported. They are identified by 
  symbolic names of the form GL_CLIP_PLANEi where 0 < i < GL_MAX_CLIP_PLANES. 
  @type equation:  Buffer object I{type GL_FLOAT}
  @param equation:  Returns four float (double)-precision values that are the coefficients of the
  plane equation of plane in eye coordinates. The initial value is (0, 0, 0, 0). 
  """

def glGetError():
  """
  Return error information
  @see: U{www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/geterror.html}
  """

def glGetLight (light, pname, params):
  """
  B{glGetLightfv and glGetLightiv}

  Return light source parameter values
  @see: U{www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/getlight.html}
 
  @type light: Enumerated constant
  @param light: Specifies a light source. The number of possible lights depends on the 
  implementation, but at least eight lights are supported. They are identified by symbolic 
  names of the form GL_LIGHTi where 0 < i < GL_MAX_LIGHTS. 
  @type pname: Enumerated constant
  @param pname: Specifies a light source parameter for light. 
  @type params:  Buffer object. Depends on function prototype.
  @param params: Returns the requested data. 
  """

def glGetMap (target, query, v):
  """
  B{glGetMapdv, glGetMapfv, glGetMapiv}

  Return evaluator parameters
  @see: U{www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/getmap.html}

  @type target: Enumerated constant
  @param target: Specifies the symbolic name of a map. 
  @type query: Enumerated constant
  @param query: Specifies which parameter to return. 
  @type v: Buffer object. Depends on function prototype.
  @param v: Returns the requested data. 
  """

def glGetMaterial (face, pname, params):
  """
  B{glGetMaterialfv, glGetMaterialiv}

  Return material parameters
  @see: U{www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/getmaterial.html}

  @type face: Enumerated constant
  @param face: Specifies which of the two materials is being queried.  
  representing the front and back materials, respectively. 
  @type pname: Enumerated constant
  @param pname: Specifies the material parameter to return. 
  @type params: Buffer object. Depends on function prototype.
  @param params: Returns the requested data. 
  """

def glGetPixelMap (map, values):
  """
  B{glGetPixelMapfv, glGetPixelMapuiv, glGetPixelMapusv}

  Return the specified pixel map
  @see: U{www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/getpixelmap.html}

  @type map:  Enumerated constant
  @param map: Specifies the name of the pixel map to return. 
  @type values: Buffer object. Depends on function prototype.
  @param values: Returns the pixel map contents. 
  """

def glGetPolygonStipple(mask):
  """
  Return the polygon stipple pattern
  @see: U{www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/getpolygonstipple.html}

  @type mask: Buffer object I{type GL_BYTE}
  @param mask: Returns the stipple pattern. The initial value is all 1's.
  """

def glGetString(name):
  """
  Return a string describing the current GL connection
  @see: U{www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/getstring.html}

  @type name: Enumerated constant
  @param name: Specifies a symbolic constant. 

  """

def glGetTexEnv (target, pname, params):
  """
  B{glGetTexEnvfv, glGetTexEnviv}

  Return texture environment parameters
  @see: U{www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/gettexenv.html}

  @type target: Enumerated constant
  @param target: Specifies a texture environment. Must be GL_TEXTURE_ENV. 
  @type pname: Enumerated constant
  @param pname: Specifies the symbolic name of a texture environment parameter. 
  @type params: Buffer object. Depends on function prototype.
  @param params: Returns the requested data. 
  """

def glGetTexGen (coord, pname, params):
  """
  B{glGetTexGendv, glGetTexGenfv, glGetTexGeniv}
 
  Return texture coordinate generation parameters
  @see: U{www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/gettexgen.html}

  @type coord: Enumerated constant
  @param coord: Specifies a texture coordinate. 
  @type pname: Enumerated constant
  @param pname: Specifies the symbolic name of the value(s) to be returned. 
  @type params: Buffer object. Depends on function prototype.
  @param params: Returns the requested data. 
  """

def glGetTexImage(target, level, format, type, pixels):
  """
  Return a texture image
  @see: U{www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/getteximage.html}

  @type target: Enumerated constant
  @param target: Specifies which texture is to be obtained. 
  @type level: int
  @param level: Specifies the level-of-detail number of the desired image. 
  Level 0 is the base image level. Level n is the nth mipmap reduction image. 
  @type format: Enumerated constant
  @param format: Specifies a pixel format for the returned data. 
  @type type: Enumerated constant
  @param type: Specifies a pixel type for the returned data. 
  @type pixels: Buffer object.
  @param pixels: Returns the texture image. Should be a pointer to an array of the 
  type specified by type
  """

def glGetTexLevelParameter (target, level, pname, params):
  """
  B{glGetTexLevelParameterfv, glGetTexLevelParameteriv}

  return texture parameter values for a specific level of detail 
  @see: U{opengl.org/developers/documentation/man_pages/hardcopy/GL/html/gl/gettexlevelparameter.html}

  @type target: Enumerated constant
  @param target: Specifies the symbolic name of the target texture. 
  @type level: int
  @param level: Specifies the level-of-detail number of the desired image. 
  Level 0 is the base image level. Level n is the nth mipmap reduction image. 
  @type pname: Enumerated constant
  @param pname: Specifies the symbolic name of a texture parameter. 
  @type params: Buffer object. Depends on function prototype.
  @param params: Returns the requested data.
  """

def glGetTexParameter (target, pname, params):
  """
  B{glGetTexParameterfv, glGetTexParameteriv}

  Return texture parameter values 
  @see: U{www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/gettexparameter.html}

  @type target: Enumerated constant
  @param target: Specifies the symbolic name of the target texture. 
  @type pname: Enumerated constant
  @param pname: Specifies the symbolic name the target texture. 
  @type params: Buffer object. Depends on function prototype.
  @param params: Returns the texture parameters.
  """

def glHint(target, mode):
  """
  Specify implementation-specific hints
  @see: U{www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/hint.html}

  @type target: Enumerated constant
  @param target: Specifies a symbolic constant indicating the behavior to be 
  controlled. 
  @type mode: Enumerated constant
  @param mode: Specifies a symbolic constant indicating the desired behavior. 
  """

def glIndex (c):
  """
  B{glIndexd, glIndexf, glIndexi, glIndexs,  glIndexdv, glIndexfv, glIndexiv, glIndexsv}

  Set the current color index
  @see: U{www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/index_.html}

  @type c: Buffer object. Depends on function prototype.
  @param c: Specifies a pointer to a one element array that contains the new value for
  the current color index.
  """

def glInitNames():
  """
  Initialize the name stack
  @see: U{www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/initnames.html}
  """

def glIsEnabled(cap):
  """
  Test whether a capability is enabled
  @see: U{www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/isenabled.html}

  @type cap: Enumerated constant
  @param cap: Specifies a constant representing a GL capability.
  """

def glIsList(list):
  """
  Determine if a name corresponds to a display-list
  @see: U{www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/islist.html}

  @type list: unsigned int
  @param list: Specifies a potential display-list name.
  """

def glIsTexture(texture):
  """
  Determine if a name corresponds to a texture
  @see: U{www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/istexture.html}

  @type texture: unsigned int
  @param texture: Specifies a value that may be the name of a texture.
  """

def glLight (light, pname, param):
  """
  B{glLightf,glLighti, glLightfv, glLightiv}

  Set the light source parameters
  @see: U{www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/light.html}

  @type light: Enumerated constant
  @param light: Specifies a light. The number of lights depends on the implementation, 
  but at least eight lights are supported. They are identified by symbolic names of the 
  form GL_LIGHTi where 0 < i < GL_MAX_LIGHTS. 
  @type pname: Enumerated constant
  @param pname: Specifies a single-valued light source parameter for light. 
  @type param: Depends on function prototype.
  @param param: Specifies the value that parameter pname of light source light will be set to.  
  If function prototype ends in 'v' specifies a pointer to the value or values that 
  parameter pname of light source light will be set to. 
  """

def glLightModel (pname, param):
  """
  B{glLightModelf, glLightModeli, glLightModelfv, glLightModeliv}

  Set the lighting model parameters
  @see: U{www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/lightmodel.html}

  @type pname:  Enumerated constant
  @param pname: Specifies a single-value light model parameter. 
  @type param: Depends on function prototype.
  @param param: Specifies the value that param will be set to. If function prototype ends in 'v'
  specifies a pointer to the value or values that param will be set to.
  """

def glLineStipple(factor, pattern):
  """
  Specify the line stipple pattern
  @see: U{www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/linestipple.html}

  @type factor: int
  @param factor: Specifies a multiplier for each bit in the line stipple pattern. 
  If factor is 3, for example, each bit in the pattern is used three times before 
  the next bit in the pattern is used. factor is clamped to the range [1, 256] and 
  defaults to 1. 
  @type pattern: unsigned short int
  @param pattern: Specifies a 16-bit integer whose bit pattern determines which fragments 
  of a line will be drawn when the line is rasterized. Bit zero is used first; the default 
  pattern is all 1's. 
  """

def glLineWidth(width):
  """
  Specify the width of rasterized lines.
  @see: U{www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/linewidth.html}

  @type width: float
  @param width: Specifies the width of rasterized lines. The initial value is 1. 
  """

def glListBase(base):
  """
  Set the display-list base for glCallLists 
  @see: U{www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/listbase.html}

  @type base: unsigned int
  @param base: Specifies an integer offset that will be added to glCallLists 
  offsets to generate display-list names. The initial value is 0.
  """

def glLoadIdentity():
  """
  Replace the current matrix with the identity matrix 
  @see: U{www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/loadidentity.html}
  """

def glLoadMatrix (m):
  """
  B{glLoadMatrixd, glLoadMatixf}

  Replace the current matrix with the specified matrix 
  @see: U{www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/loadmatrix.html}

  @type m: Buffer object. Depends on function prototype.
  @param m: Specifies a pointer to 16 consecutive values, which are used as the elements 
  of a 4x4 column-major matrix. 
  """

def glLoadName(name):
  """
  Load a name onto the name stack.
  @see: U{www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/loadname.html}

  @type name: unsigned int
  @param name: Specifies a name that will replace the top value on the name stack. 
  """

def glLogicOp(opcode):
  """
  Specify a logical pixel operation for color index rendering 
  @see: U{www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/logicop.html}

  @type opcode: Enumerated constant
  @param opcode: Specifies a symbolic constant that selects a logical operation. 
  """

def glMap1 (target, u1, u2, stride, order, points):
  """
  B{glMap1d, glMap1f}

  Define a one-dimensional evaluator
  @see: U{www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/map1.html}

  @type target: Enumerated constant
  @param target: Specifies the kind of values that are generated by the evaluator. 
  @type u1, u2: Depends on function prototype.
  @param u1,u2: Specify a linear mapping of u, as presented to glEvalCoord1, to ^, t
  he variable that is evaluated by the equations specified by this command. 
  @type stride: int
  @param stride: Specifies the number of floats or float (double)s between the beginning 
  of one control point and the beginning of the next one in the data structure 
  referenced in points. This allows control points to be embedded in arbitrary data 
  structures. The only constraint is that the values for a particular control point must 
  occupy contiguous memory locations. 
  @type order: int
  @param order: Specifies the number of control points. Must be positive. 
  @type points: Buffer object. Depends on function prototype.
  @param points: Specifies a pointer to the array of control points. 
  """

def glMap2 (target, u1, u2, ustride, uorder, v1, v2, vstride, vorder, points):
  """
  B{glMap2d, glMap2f}

  Define a two-dimensional evaluator
  @see: U{www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/map2.html}

  @type target: Enumerated constant
  @param target: Specifies the kind of values that are generated by the evaluator. 
  @type u1, u2: Depends on function prototype.
  @param u1,u2: Specify a linear mapping of u, as presented to glEvalCoord2, to ^, t
  he variable that is evaluated by the equations specified by this command. Initially
  u1 is 0 and u2 is 1.
  @type ustride: int
  @param ustride: Specifies the number of floats or float (double)s between the beginning 
  of control point R and the beginning of control point R ij, where i and j are the u 
  and v control point indices, respectively. This allows control points to be embedded 
  in arbitrary data structures. The only constraint is that the values for a particular 
  control point must occupy contiguous memory locations. The initial value of ustride is 0. 
  @type uorder: int
  @param uorder: Specifies the dimension of the control point array in the u axis. 
  Must be positive. The initial value is 1. 
  @type v1, v2: Depends on function prototype.
  @param v1, v2: Specify a linear mapping of v, as presented to glEvalCoord2, to ^, 
  one of the two variables that are evaluated by the equations specified by this command. 
  Initially, v1 is 0 and v2 is 1. 
  @type vstride: int
  @param vstride: Specifies the number of floats or float (double)s between the beginning of control 
  point R and the beginning of control point R ij, where i and j are the u and v control 
  point(indices, respectively. This allows control points to be embedded in arbitrary data
  structures. The only constraint is that the values for a particular control point must 
  occupy contiguous memory locations. The initial value of vstride is 0. 
  @type vorder: int
  @param vorder: Specifies the dimension of the control point array in the v axis. 
  Must be positive. The initial value is 1. 
  @type points: Buffer object. Depends on function prototype.
  @param points: Specifies a pointer to the array of control points. 
  """

def glMapGrid (un, u1,u2 ,vn, v1, v2):
  """
  B{glMapGrid1d, glMapGrid1f, glMapGrid2d, glMapGrid2f}

  Define a one- or two-dimensional mesh
  @see: U{www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/mapgrid.html}

  @type un: int
  @param un: Specifies the number of partitions in the grid range interval 
  [u1, u2]. Must be positive. 
  @type u1, u2: Depends on function prototype.
  @param u1, u2: Specify the mappings for integer grid domain values i=0 and i=un. 
  @type vn: int
  @param vn: Specifies the number of partitions in the grid range interval [v1, v2] 
  (glMapGrid2 only). 
  @type v1, v2: Depends on function prototype.
  @param v1, v2: Specify the mappings for integer grid domain values j=0 and j=vn 
  (glMapGrid2 only). 
  """

def glMaterial (face, pname, params):
  """
  Specify material parameters for the lighting model.
  @see: U{www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/material.html}
  
  @type face: Enumerated constant
  @param face: Specifies which face or faces are being updated. Must be one of:
  @type pname: Enumerated constant
  @param pname: Specifies the single-valued material parameter of the face 
  or faces that is being updated. Must be GL_SHININESS. 
  @type params: int
  @param params: Specifies the value that parameter GL_SHININESS will be set to. 
  If function prototype ends in 'v' specifies a pointer to the value or values that 
  pname will be set to. 
  """

def glMatrixMode(mode):
  """
  Specify which matrix is the current matrix.
  @see: U{www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/matrixmode.html}

  @type mode: Enumerated constant
  @param mode: Specifies which matrix stack is the target for subsequent matrix operations. 
  """

def glMultMatrix (m):
  """
  B{glMultMatrixd, glMultMatrixf}

  Multiply the current matrix with the specified matrix
  @see: U{www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/multmatrix.html}

  @type m: Buffer object. Depends on function prototype.
  @param m: Points to 16 consecutive values that are used as the elements of a 4x4 column
  major matrix.
  """

def glNewList(list, mode):
  """
  Create or replace a display list
  @see: U{www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/newlist.html}
  
  @type list: unsigned int
  @param list: Specifies the display list name
  @type mode: Enumerated constant
  @param mode: Specifies the compilation mode.
  """

def glNormal3 (nx, ny, nz, v):
  """
  B{Normal3b, Normal3bv, Normal3d, Normal3dv, Normal3f, Normal3fv, Normal3i, Normal3iv,
  Normal3s, Normal3sv}

  Set the current normal vector
  @see: U{www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/normal.html}
  
  @type nx, ny, nz: Depends on function prototype. (non - 'v' prototypes only)
  @param nx, ny, nz: Specify the x, y, and z coordinates of the new current normal. 
  The initial value of the current normal is the unit vector, (0, 0, 1). 
  @type v: Buffer object. Depends on function prototype. ('v' prototypes)
  @param v: Specifies a pointer to an array of three elements: the x, y, and z coordinates
  of the new current normal.
  """
  
def glOrtho(left, right, bottom, top, zNear, zFar):
  """
  Multiply the current matrix with an orthographic matrix
  @see: U{www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/ortho.html}
  
  @type left, right: double (float)
  @param left, right: Specify the coordinates for the left and 
  right vertical clipping planes. 
  @type bottom, top: double (float)
  @param bottom, top: Specify the coordinates for the bottom and top 
  horizontal clipping planes. 
  @type zNear, zFar: double (float)
  @param zNear, zFar: Specify the distances to the nearer and farther 
  depth clipping planes. These values are negative if the plane is to be behind the viewer. 
  """

def glPassThrough(token):
  """
  Place a marker in the feedback buffer
  @see: U{www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/passthrough.html}

  @type token: float
  @param token: Specifies a marker value to be placed in the feedback 
  buffer following a GL_PASS_THROUGH_TOKEN. 
  """

def glPixelMap (map, mapsize, values):
  """
  B{glPixelMapfv, glPixelMapuiv, glPixelMapusv}

  Set up pixel transfer maps
  @see:  U{www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/pixelmap.html}

  @type map: Enumerated constant
  @param map: Specifies a symbolic map name.
  @type mapsize: int
  @param mapsize: Specifies the size of the map being defined. 
  @type values: Buffer object. Depends on function prototype.
  @param values: Specifies an array of mapsize values. 
  """

def glPixelStore (pname, param):
  """
  B{glPixelStoref, glPixelStorei}

  Set pixel storage modes
  @see: U{www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/pixelstore.html}
  
  @type pname: Enumerated constant
  @param pname: Specifies the symbolic name of the parameter to be set. 
  Six values affect the packing of pixel data into memory.
  Six more affect the unpacking of pixel data from memory. 
  @type param: Depends on function prototype.
  @param param: Specifies the value that pname is set to. 
  """

def glPixelTransfer (pname, param):
  """
  B{glPixelTransferf, glPixelTransferi}

  Set pixel transfer modes
  @see: U{www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/pixeltransfer.html}
 
  @type pname: Enumerated constant
  @param pname: Specifies the symbolic name of the pixel transfer parameter to be set. 
  @type param: Depends on function prototype.
  @param param: Specifies the value that pname is set to. 
  """

def glPixelZoom(xfactor, yfactor):
  """
  Specify the pixel zoom factors
  @see: U{www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/pixelzoom.html}
  
  @type xfactor, yfactor: float
  @param xfactor, yfactor: Specify the x and y zoom factors for pixel write operations.
  """

def glPointSize(size):
  """
  Specify the diameter of rasterized points
  @see: U{www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/pointsize.html}
  
  @type size: float
  @param size: Specifies the diameter of rasterized points. The initial value is 1.
  """

def glPolygonMode(face, mode):
  """
  Select a polygon rasterization mode
  @see: U{www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/polygonmode.html}
 
  @type face: Enumerated constant
  @param face: Specifies the polygons that mode applies to. 
  Must be GL_FRONT for front-facing polygons, GL_BACK for back- facing polygons, 
  or GL_FRONT_AND_BACK for front- and back-facing polygons. 
  @type mode: Enumerated constant
  @param mode: Specifies how polygons will be rasterized. 
  The initial value is GL_FILL for both front- and back- facing polygons. 
  """

def glPolygonOffset(factor, units):
  """
  Set the scale and units used to calculate depth values
  @see: U{www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/polygonoffset.html}
  
  @type factor: float
  @param factor: Specifies a scale factor that is used to create a variable depth 
  offset for each polygon. The initial value is 0. 
  @type units:  float
  @param units: Is multiplied by an implementation-specific value to create a constant
  depth offset. The initial value is 0. 
  """

def glPolygonStipple(mask):
  """
  Set the polygon stippling pattern
  @see: U{www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/polygonstipple.html}
  
  @type mask: Buffer object I{type GL_BYTE}
  @param mask: Specifies a pointer to a 32x32 stipple pattern that will be unpacked 
  from memory in the same way that glDrawPixels unpacks pixels. 
  """

def glPopAttrib():
  """
  Pop the server attribute stack
  @see: U{www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/pushattrib.html}
  """

def glPopClientAttrib():
  """
  Pop the client attribute stack
  @see: U{www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/pushclientattrib.html}
  """

def glPopMatrix():
  """
  Pop the current matrix stack
  @see: U{www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/pushmatrix.html}
  """

def glPopName():
  """
  Pop the name stack
  @see: U{www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/pushname.html}
  """

def glPrioritizeTextures(n, textures, priorities):
  """
  Set texture residence priority
  @see: U{www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/prioritizetextures.html}
  
  @type n: int
  @param n:Specifies the number of textures to be prioritized. 
  @type textures: Buffer I{type GL_INT}
  @param textures: Specifies an array containing the names of the textures to be prioritized. 
  @type priorities: Buffer I{type GL_FLOAT}
  @param priorities: Specifies an array containing the texture priorities. A priority given 
  in an element of priorities applies to the texture named by the corresponding element of textures. 
  """

def glPushAttrib(mask):
  """
  Push the server attribute stack
  @see: U{www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/pushattrib.html}

  @type mask: Enumerated constant(s)
  @param mask: Specifies a mask that indicates which attributes to save.
  """

def glPushClientAttrib(mask):
  """
  Push the client attribute stack
  @see: U{www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/pushclientattrib.html}

  @type mask: Enumerated constant(s)
  @param mask: Specifies a mask that indicates which attributes to save.
  """

def glPushMatrix():
  """
  Push the current matrix stack
  @see: U{www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/pushmatrix.html}
  """

def glPushName(name):
  """
  Push the name stack
  @see: U{www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/pushname.html}

  @type name: unsigned int
  @param name: Specifies a name that will be pushed onto the name stack.
  """

def glRasterPos (x,y,z,w):
  """
  B{glRasterPos2d, glRasterPos2f, glRasterPos2i, glRasterPos2s, glRasterPos3d, 
  glRasterPos3f, glRasterPos3i, glRasterPos3s, glRasterPos4d, glRasterPos4f, 
  glRasterPos4i, glRasterPos4s, glRasterPos2dv, glRasterPos2fv, glRasterPos2iv, 
  glRasterPos2sv, glRasterPos3dv, glRasterPos3fv, glRasterPos3iv, glRasterPos3sv, 
  glRasterPos4dv, glRasterPos4fv, glRasterPos4iv, glRasterPos4sv}

  Specify the raster position for pixel operations
  @see: U{www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/rasterpos.html}

  @type x, y, z, w: Depends on function prototype. (z and w for '3' and '4' prototypes only)
  @param x, y, z, w: Specify the x,y,z, and w object coordinates (if present) for the 
  raster position.  If function prototype ends in 'v' specifies a pointer to an array of two, 
  three, or four elements, specifying x, y, z, and w coordinates, respectively.
  @note:
    If you are drawing to the 3d view with a Scriptlink of a space handler
    the zoom level of the panels will scale the glRasterPos by the view matrix.
    so a X of 10 will not always offset 10 pixels as you would expect.

    To work around this get the scale value of the view matrix and use it to scale your pixel values.

    Workaround::

      import Blender
      from Blender.BGL import *
      xval, yval= 100, 40
      # Get the scale of the view matrix
      viewMatrix = Buffer(GL_FLOAT, 16)
      glGetFloatv(GL_MODELVIEW_MATRIX, viewMatrix)
      f = 1/viewMatrix[0]
      glRasterPos2f(xval*f, yval*f) # Instead of the usual glRasterPos2i(xval, yval)
  """

def glReadBuffer(mode):
  """
  Select a color buffer source for pixels.
  @see: U{www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/readbuffer.html}
  
  @type mode: Enumerated constant
  @param mode: Specifies a color buffer. 
  """

def glReadPixels(x, y, width, height, format, type, pixels):
  """
  Read a block of pixels from the frame buffer
  @see: U{www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/readpixels.html}
  
  @type x, y: int
  @param x, y:Specify the window coordinates of the first pixel that is read 
  from the frame buffer. This location is the lower left corner of a rectangular
  block of pixels. 
  @type width, height: int
  @param width, height: Specify the dimensions of the pixel rectangle. width and 
  height of one correspond to a single pixel. 
  @type format: Enumerated constant
  @param format: Specifies the format of the pixel data. 
  @type type: Enumerated constant
  @param type: Specifies the data type of the pixel data. 
  @type pixels: Buffer object
  @param pixels: Returns the pixel data. 
  """

def glRect (x1,y1,x2,y2,v1,v2):
  """
  B{glRectd, glRectf, glRecti, glRects, glRectdv, glRectfv, glRectiv, glRectsv}

  Draw a rectangle
  @see: U{www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/rect.html}
 
  @type x1, y1: Depends on function prototype. (for non 'v' prototypes only)
  @param x1, y1: Specify one vertex of a rectangle
  @type x2, y2: Depends on function prototype. (for non 'v' prototypes only)
  @param x2, y2: Specify the opposite vertex of the rectangle
  @type v1, v2: Depends on function prototype. (for 'v' prototypes only)
  @param v1, v2: Specifies a pointer to one vertex of a rectangle and the pointer
  to the opposite vertex of the rectangle
  """

def glRenderMode(mode):
  """
  Set rasterization mode
  @see: U{www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/rendermode.html}
  
  @type mode: Enumerated constant
  @param mode: Specifies the rasterization mode. 
  """

def glRotate (angle, x, y, z):
  """
  B{glRotated, glRotatef}

  Multiply the current matrix by a rotation matrix
  @see: U{www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/rotate.html}

  @type angle:  Depends on function prototype.
  @param angle:  Specifies the angle of rotation in degrees.
  @type x, y, z:  Depends on function prototype.
  @param x, y, z:  Specify the x, y, and z coordinates of a vector respectively.
  """

def glScale (x,y,z):
  """
  B{glScaled, glScalef}

  Multiply the current matrix by a general scaling matrix
  @see: U{www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/scale.html}

  @type x, y, z: Depends on function prototype.
  @param x, y, z: Specify scale factors along the x, y, and z axes, respectively.
  """

def glScissor(x,y,width,height):
  """
  Define the scissor box
  @see: U{www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/scissor.html}
 
  @type x, y: int
  @param x, y: Specify the lower left corner of the scissor box. Initially (0, 0). 
  @type width, height: int
  @param width height: Specify the width and height of the scissor box. When a 
  GL context is first attached to a window, width and height are set to the 
  dimensions of that window. 
  """

def glSelectBuffer(size, buffer):
  """
  Establish a buffer for selection mode values
  @see: U{www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/selectbuffer.html}

  @type size: int
  @param size: Specifies the size of buffer
  @type buffer: Buffer I{type GL_INT}
  @param buffer: Returns the selection data
  """

def glShadeModel(mode):
  """
  Select flat or smooth shading
  @see: U{www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/shademodel.html}
  
  @type mode: Enumerated constant
  @param mode: Specifies a symbolic value representing a shading technique.  
  """

def glStencilFuc(func, ref, mask):
  """
  Set function and reference value for stencil testing
  @see: U{www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/stencilfunc.html}

  @type func: Enumerated constant
  @param func:Specifies the test function. 
  @type ref: int
  @param ref:Specifies the reference value for the stencil test. ref is clamped to 
  the range [0,2n-1], where n is the number of bitplanes in the stencil buffer. 
  The initial value is 0.
  @type mask: unsigned int
  @param mask:Specifies a mask that is ANDed with both the reference value and 
  the stored stencil value when the test is done. The initial value is all 1's. 
  """

def glStencilMask(mask):
  """
  Control the writing of individual bits in the stencil planes
  @see: U{www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/stencilmask.html}
  
  @type mask: unsigned int
  @param mask: Specifies a bit mask to enable and disable writing of individual bits 
  in the stencil planes. Initially, the mask is all 1's. 
  """

def glStencilOp(fail, zfail, zpass):
  """
  Set stencil test actions
  @see: U{www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/stencilop.html}
  
  @type fail: Enumerated constant
  @param fail: Specifies the action to take when the stencil test fails. 
  The initial value is GL_KEEP. 
  @type zfail: Enumerated constant
  @param zfail: Specifies the stencil action when the stencil test passes, but the 
  depth test fails. zfail accepts the same symbolic constants as fail. 
  The initial value is GL_KEEP. 
  @type zpass: Enumerated constant
  @param zpass: Specifies the stencil action when both the stencil test and the 
  depth test pass, or when the stencil test passes and either there is no depth 
  buffer or depth testing is not enabled. zpass accepts the same symbolic constants 
  as fail. The initial value is GL_KEEP.
  """

def glTexCoord (s,t,r,q,v): 
  """
  B{glTexCoord1d, glTexCoord1f, glTexCoord1i, glTexCoord1s, glTexCoord2d, glTexCoord2f, 
  glTexCoord2i, glTexCoord2s, glTexCoord3d, glTexCoord3f, glTexCoord3i, glTexCoord3s, 
  glTexCoord4d, glTexCoord4f, glTexCoord4i, glTexCoord4s, glTexCoord1dv, glTexCoord1fv, 
  glTexCoord1iv, glTexCoord1sv, glTexCoord2dv, glTexCoord2fv, glTexCoord2iv, 
  glTexCoord2sv, glTexCoord3dv, glTexCoord3fv, glTexCoord3iv, glTexCoord3sv, 
  glTexCoord4dv, glTexCoord4fv, glTexCoord4iv, glTexCoord4sv}

  Set the current texture coordinates
  @see: U{www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/texcoord.html}
  
  @type s, t, r, q: Depends on function prototype. (r and q for '3' and '4' prototypes only)
  @param s, t, r, q: Specify s, t, r, and q texture coordinates. Not all parameters are 
  present in all forms of the command. 
  @type v: Buffer object. Depends on function prototype. (for 'v' prototypes only)
  @param v: Specifies a pointer to an array of one, two, three, or four elements, 
  which in turn specify the s, t, r, and q texture coordinates. 
  """

def glTexEnv  (target, pname, param):
  """
  B{glTextEnvf, glTextEnvi, glTextEnvfv, glTextEnviv}

  Set texture environment parameters
  @see: U{www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/texenv.html} 

  @type target: Enumerated constant
  @param target: Specifies a texture environment. Must be GL_TEXTURE_ENV.
  @type pname: Enumerated constant
  @param pname: Specifies the symbolic name of a single-valued texture environment 
  parameter. Must be GL_TEXTURE_ENV_MODE. 
  @type param: Depends on function prototype.
  @param param: Specifies a single symbolic constant. If function prototype ends in 'v'
  specifies a pointer to a parameter array that contains either a single symbolic 
  constant or an RGBA color
  """

def glTexGen (coord, pname, param):
  """
  B{glTexGend, glTexGenf, glTexGeni, glTexGendv, glTexGenfv, glTexGeniv}

  Control the generation of texture coordinates
  @see: U{www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/texgen.html}
  
  @type coord: Enumerated constant
  @param coord: Specifies a texture coordinate. 
  @type pname: Enumerated constant
  @param pname: Specifies the symbolic name of the texture- coordinate generation function. 
  @type param: Depends on function prototype.
  @param param: Specifies a single-valued texture generation parameter. 
  If function prototype ends in 'v' specifies a pointer to an array of texture 
  generation parameters. If pname is GL_TEXTURE_GEN_MODE, then the array must 
  contain a single symbolic constant. Otherwise, params holds the coefficients 
  for the texture-coordinate generation function specified by pname. 
  """

def glTexImage1D(target, level, internalformat, width, border, format, type, pixels):
  """
  Specify a one-dimensional texture image
  @see: U{www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/teximage1d.html}

  @type target: Enumerated constant
  @param target: Specifies the target texture. 
  @type level: int
  @param level: Specifies the level-of-detail number. Level 0 is the base image level. 
  Level n is the nth mipmap reduction image. 
  @type internalformat: int
  @param internalformat: Specifies the number of color components in the texture. 
  @type width: int
  @param width: Specifies the width of the texture image. Must be 2n+2(border) for 
  some integer n. All implementations support texture images that are at least 64 
  texels wide. The height of the 1D texture image is 1. 
  @type border: int
  @param border: Specifies the width of the border. Must be either 0 or 1. 
  @type format: Enumerated constant
  @param format: Specifies the format of the pixel data. 
  @type type: Enumerated constant
  @param type: Specifies the data type of the pixel data. 
  @type pixels: Buffer object.
  @param pixels: Specifies a pointer to the image data in memory. 
  """

def glTexImage2D(target, level, internalformat, width, height, border, format, type, pixels):
  """
  Specify a two-dimensional texture image
  @see: U{www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/teximage2d.html}

  @type target: Enumerated constant
  @param target: Specifies the target texture. 
  @type level: int
  @param level: Specifies the level-of-detail number. Level 0 is the base image level. 
  Level n is the nth mipmap reduction image. 
  @type internalformat: int
  @param internalformat: Specifies the number of color components in the texture. 
  @type width: int
  @param width: Specifies the width of the texture image. Must be 2n+2(border) for 
  some integer n. All implementations support texture images that are at least 64 
  texels wide. 
  @type height: int
  @param height: Specifies the height of the texture image. Must be 2m+2(border) for 
  some integer m. All implementations support texture images that are at least 64 
  texels high. 
  @type border: int
  @param border: Specifies the width of the border. Must be either 0 or 1. 
  @type format: Enumerated constant
  @param format: Specifies the format of the pixel data. 
  @type type: Enumerated constant
  @param type: Specifies the data type of the pixel data. 
  @type pixels: Buffer object.
  @param pixels: Specifies a pointer to the image data in memory. 
  """

def glTexParameter (target, pname, param):
  """
  B{glTexParameterf, glTexParameteri, glTexParameterfv, glTexParameteriv}

  Set texture parameters
  @see: U{www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/texparameter.html}

  @type target: Enumerated constant
  @param target: Specifies the target texture.
  @type pname: Enumerated constant
  @param pname: Specifies the symbolic name of a single-valued texture parameter. 
  @type param: Depends on function prototype.
  @param param: Specifies the value of pname. If function prototype ends in 'v' specifies 
  a pointer to an array where the value or values of pname are stored. 
  """

def glTranslate (x, y, z):
  """
  B{glTranslatef, glTranslated}

  Multiply the current matrix by a translation matrix
  @see: U{www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/translate.html}

  @type x, y, z: Depends on function prototype.
  @param x, y, z: Specify the x, y, and z coordinates of a translation vector. 
  """

def glVertex (x,y,z,w,v):
  """
  B{glVertex2d, glVertex2f, glVertex2i, glVertex2s, glVertex3d, glVertex3f, glVertex3i, 
  glVertex3s, glVertex4d, glVertex4f, glVertex4i, glVertex4s, glVertex2dv, glVertex2fv, 
  glVertex2iv, glVertex2sv, glVertex3dv, glVertex3fv, glVertex3iv, glVertex3sv, glVertex4dv, 
  glVertex4fv, glVertex4iv, glVertex4sv}

  Specify a vertex
  @see: U{www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/vertex.html}
  
  @type x, y, z, w: Depends on function prototype (z and w for '3' and '4' prototypes only)
  @param x, y, z, w: Specify x, y, z, and w coordinates of a vertex. Not all parameters 
  are present in all forms of the command. 
  @type v: Buffer object. Depends of function prototype (for 'v' prototypes only)
  @param v: Specifies a pointer to an array of two, three, or four elements. The 
  elements of a two-element array are x and y; of a three-element array, x, y, and z; 
  and of a four-element array, x, y, z, and w. 
  """

def glViewport(x,y,width,height):
  """
  Set the viewport
  @see: U{www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/viewport.html}

  @type x, y: int
  @param x, y: Specify the lower left corner of the viewport rectangle, 
  in pixels. The initial value is (0,0). 
  @type width, height: int
  @param width, height: Specify the width and height of the viewport. When a GL context 
  is first attached to a window, width and height are set to the dimensions of that window. 
  """

def gluPerspective(fovY, aspect, zNear, zFar):
  """
  Set up a perspective projection matrix.
  @see: U{http://biology.ncsa.uiuc.edu/cgi-bin/infosrch.cgi?cmd=getdoc&coll=0650&db=bks&fname=/SGI_Developer/OpenGL_RM/ch06.html#id5577288}

  @type fovY: double
  @param fovY: Specifies the field of view angle, in degrees, in the y direction.
  @type aspect: double
  @param aspect: Specifies the aspect ratio that determines the field of view in the x direction. 
   The aspect ratio is the ratio of x (width) to y (height).
  @type zNear: double
  @param zNear: Specifies the distance from the viewer to the near clipping plane (always positive).
  @type zFar: double
  @param zFar: Specifies the distance from the viewer to the far clipping plane (always positive).
  """

def gluLookAt(eyex, eyey, eyez, centerx, centery, centerz, upx, upy, upz):
  """
  Define a viewing transformation
  @see: U{http://biology.ncsa.uiuc.edu/cgi-bin/infosrch.cgi?cmd=getdoc&coll=0650&db=bks&fname=/SGI_Developer/OpenGL_RM/ch06.html#id5573042}

  @type eyex, eyey, eyez: double
  @param eyex, eyey, eyez: Specifies the position of the eye point.  
  @type centerx, centery, centerz: double
  @param centerx, centery, centerz: Specifies the position of the reference point.
  @type upx, upy, upz: double
  @param upx, upy, upz: Specifies the direction of the up vector.
  """

def gluOrtho2D(left, right, bottom, top):
  """
  Define a 2-D orthographic projection matrix
  @see: U{http://biology.ncsa.uiuc.edu/cgi-bin/infosrch.cgi?cmd=getdoc&coll=0650&db=bks&fname=/SGI_Developer/OpenGL_RM/ch06.html#id5578074}

  @type left, right: double
  @param left, right: Specify the coordinates for the left and right vertical clipping planes.
  @type bottom, top: double
  @param bottom, top: Specify the coordinates for the bottom and top horizontal clipping planes.
  """

def gluPickMatrix(x, y, width, height, viewport):
  """
  Define a picking region
  @see: U{http://biology.ncsa.uiuc.edu/cgi-bin/infosrch.cgi?cmd=getdoc&coll=0650&db=bks&fname=/SGI_Developer/OpenGL_RM/ch06.html#id5578074}

  @type x, y: double
  @param x, y: Specify the center of a picking region in window coordinates.
  @type width, height: double
  @param width, height: Specify the width and height, respectively, of the picking region in window coordinates.
  @type viewport: Buffer object. [int]
  @param viewport: Specifies the current viewport.
  """

def gluProject(objx, objy, objz, modelMatrix, projMatrix, viewport, winx, winy, winz):
  """
  Map object coordinates to window coordinates.
  @see: U{http://biology.ncsa.uiuc.edu/cgi-bin/infosrch.cgi?cmd=getdoc&coll=0650&db=bks&fname=/SGI_Developer/OpenGL_RM/ch06.html#id5578074}
  
  @type objx, objy, objz: double
  @param objx, objy, objz: Specify the object coordinates.
  @type modelMatrix: Buffer object. [double]
  @param modelMatrix: Specifies the current modelview matrix (as from a glGetDoublev call).
  @type projMatrix: Buffer object. [double]
  @param projMatrix: Specifies the current projection matrix (as from a glGetDoublev call).
  @type viewport: Buffer object. [int]
  @param viewport: Specifies the current viewport (as from a glGetIntegerv call).
  @type winx, winy, winz: Buffer object. [double]
  @param winx, winy, winz: Return the computed window coordinates. 
  """

def gluUnProject(winx, winy, winz, modelMatrix, projMatrix, viewport, objx, objy, objz):
  """
  Map object coordinates to window
  coordinates.
  @see: U{http://biology.ncsa.uiuc.edu/cgi-bin/infosrch.cgi?cmd=getdoc&coll=0650&db=bks&fname=/SGI_Developer/OpenGL_RM/ch06.html#id5582204}

  @type winx, winy, winz: double
  @param winx, winy, winz: Specify the window coordinates to be mapped.
  @type modelMatrix: Buffer object. [double]
  @param modelMatrix: Specifies the current modelview matrix (as from a glGetDoublev call).
  @type projMatrix: Buffer object. [double]
  @param projMatrix: Specifies the current projection matrix (as from a glGetDoublev call).
  @type viewport: Buffer object. [int]
  @param viewport: Specifies the current viewport (as from a glGetIntegerv call).
  @type objx, objy, objz: Buffer object. [double]
  @param objx, objy, objz: Return the computed object coordinates.
  """

class Buffer:
  """
  The Buffer object is simply a block of memory that is delineated and initialized by the
  user. Many OpenGL functions return data to a C-style pointer, however, because this
  is not possible in python the Buffer object can be used to this end. Wherever pointer
  notation is used in the OpenGL functions the Buffer object can be used in it's BGL 
  wrapper. In some instances the Buffer object will need to be initialized with the template 
  parameter, while in other instances the user will want to create just a blank buffer 
  which will be zeroed by default.

  Example with Buffer::
    import Blender
    from Blender import BGL
    myByteBuffer = BGL.Buffer(BGL.GL_BYTE, [32,32])
    BGL.glGetPolygonStipple(myByteBuffer)
    print myByteBuffer.dimensions
    print myByteBuffer.list
    sliceBuffer = myByteBuffer[0:16]
    print sliceBuffer 

  @ivar list: The contents of the Buffer.
  @ivar dimensions: The size of the Buffer.
  """

  def __init__(type, dimensions, template = None):
    """
    This will create a new Buffer object for use with other BGL OpenGL commands.
    Only the type of argument to store in the buffer and the dimensions of the buffer
    are necessary. Buffers are zeroed by default unless a template is supplied, in 
    which case the buffer is initialized to the template.

    @type type: int
    @param type: The format to store data in. The type should be one of 
    GL_BYTE, GL_SHORT, GL_INT, or GL_FLOAT.
    @type dimensions: An int or sequence object specifying the dimensions of the buffer.
    @param dimensions: If the dimensions are specified as an int a linear array will 
    be created for the buffer. If a sequence is passed for the dimensions, the buffer 
    becomes n-Dimensional, where n is equal to the number of parameters passed in the 
    sequence. Example: [256,2] is a two- dimensional buffer while [256,256,4] creates 
    a three- dimensional buffer. You can think of each additional dimension as a sub-item 
    of the dimension to the left. i.e. [10,2] is a 10 element array each with 2 sub-items.  
    [(0,0), (0,1), (1,0), (1,1), (2,0), ...] etc.
    @type template: A python sequence object (optional)
    @param template: A sequence of matching dimensions which will be used to initialize
    the Buffer. If a template is not passed in all fields will be initialized to 0.
    @rtype: Buffer object
    @return: The newly created buffer as a PyObject.
    """
