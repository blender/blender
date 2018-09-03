
OpenGL Wrapper (bgl)
====================

.. module:: bgl

This module wraps OpenGL constants and functions, making them available from
within Blender Python.

The complete list can be retrieved from the module itself, by listing its
contents: dir(bgl).  A simple search on the web can point to more
than enough material to teach OpenGL programming, from books to many
collections of tutorials.

Here is a comprehensive `list of books <https://www.khronos.org/developers/books/>`__ (non free).
The `arcsynthesis tutorials <https://web.archive.org/web/20150225192611/http://www.arcsynthesis.org/gltut/index.html>`__
is one of the best resources to learn modern OpenGL and
`g-truc <http://www.g-truc.net/post-opengl-samples.html#menu>`__
offers a set of extensive examples, including advanced features.


.. note::

   You can use the :class:`Image` type to load and set textures.
   See :class:`Image.gl_load` and :class:`Image.gl_load`,
   for example.


.. function:: glAccum(op, value):

   Operate on the accumulation buffer.

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glAccum.xml>`__

   :type op: Enumerated constant
   :arg op: The accumulation buffer operation.
   :type value: float
   :arg value: a value used in the accumulation buffer operation.


.. function:: glAlphaFunc(func, ref):

   Specify the alpha test function.

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glAlphaFunc.xml>`__

   :type func: Enumerated constant
   :arg func: Specifies the alpha comparison function.
   :type ref: float
   :arg ref: The reference value that incoming alpha values are compared to.
      Clamped between 0 and 1.


.. function:: glAreTexturesResident(n, textures, residences):

   Determine if textures are loaded in texture memory

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glAreTexturesResident.xml>`__

   :type n: int
   :arg n: Specifies the number of textures to be queried.
   :type textures: :class:`bgl.Buffer` object I{type GL_INT}
   :arg textures: Specifies an array containing the names of the textures to be queried
   :type residences: :class:`bgl.Buffer` object I{type GL_INT}(boolean)
   :arg residences: An array in which the texture residence status in returned.
      The residence status of a texture named by an element of textures is
      returned in the corresponding element of residences.


.. function:: glBegin(mode):

   Delimit the vertices of a primitive or a group of like primitives

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glBegin.xml>`__

   :type mode: Enumerated constant
   :arg mode: Specifies the primitive that will be create from vertices between
      glBegin and glEnd.


.. function:: glBindTexture(target, texture):

   Bind a named texture to a texturing target

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glBindTexture.xml>`__

   :type target: Enumerated constant
   :arg target: Specifies the target to which the texture is bound.
   :type texture: unsigned int
   :arg texture: Specifies the name of a texture.


.. function:: glBitmap(width, height, xorig, yorig, xmove, ymove, bitmap):

   Draw a bitmap

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glBitmap.xml>`__

   :type width, height: int
   :arg width, height: Specify the pixel width and height of the bitmap image.
   :type xorig, yorig: float
   :arg xorig, yorig: Specify the location of the origin in the bitmap image. The origin is measured
      from the lower left corner of the bitmap, with right and up being the positive axes.
   :type xmove, ymove: float
   :arg xmove, ymove: Specify the x and y offsets to be added to the current raster position after
      the bitmap is drawn.
   :type bitmap: :class:`bgl.Buffer` object I{type GL_BYTE}
   :arg bitmap: Specifies the address of the bitmap image.


.. function:: glBlendFunc(sfactor, dfactor):

   Specify pixel arithmetic

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glBlendFunc.xml>`__

   :type sfactor: Enumerated constant
   :arg sfactor: Specifies how the red, green, blue, and alpha source blending factors are
      computed.
   :type dfactor: Enumerated constant
   :arg dfactor: Specifies how the red, green, blue, and alpha destination
      blending factors are computed.


.. function:: glCallList(list):

   Execute a display list

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glCallList.xml>`__

   :type list: unsigned int
   :arg list: Specifies the integer name of the display list to be executed.


.. function:: glCallLists(n, type, lists):

   Execute a list of display lists

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glCallLists.xml>`__

   :type n: int
   :arg n: Specifies the number of display lists to be executed.
   :type type: Enumerated constant
   :arg type: Specifies the type of values in lists.
   :type lists: :class:`bgl.Buffer` object
   :arg lists: Specifies the address of an array of name offsets in the display list.
      The pointer type is void because the offsets can be bytes, shorts, ints, or floats,
      depending on the value of type.


.. function:: glClear(mask):

   Clear buffers to preset values

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glClear.xml>`__

   :type mask: Enumerated constant(s)
   :arg mask: Bitwise OR of masks that indicate the buffers to be cleared.


.. function:: glClearAccum(red, green, blue, alpha):

   Specify clear values for the accumulation buffer

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glClearAccum.xml>`__

   :type red, green, blue, alpha: float
   :arg red, green, blue, alpha: Specify the red, green, blue, and alpha values used when the
      accumulation buffer is cleared. The initial values are all 0.


.. function:: glClearColor(red, green, blue, alpha):

   Specify clear values for the color buffers

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glClearColor.xml>`__

   :type red, green, blue, alpha: float
   :arg red, green, blue, alpha: Specify the red, green, blue, and alpha values used when the
      color buffers are cleared. The initial values are all 0.


.. function:: glClearDepth(depth):

   Specify the clear value for the depth buffer

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glClearDepth.xml>`__

   :type depth: int
   :arg depth: Specifies the depth value used when the depth buffer is cleared.
      The initial value is 1.


.. function:: glClearIndex(c):

   Specify the clear value for the color index buffers

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glClearIndex.xml>`__

   :type c: float
   :arg c: Specifies the index used when the color index buffers are cleared.
      The initial value is 0.


.. function:: glClearStencil(s):

   Specify the clear value for the stencil buffer

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glClearStencil.xml>`__

   :type s: int
   :arg s: Specifies the index used when the stencil buffer is cleared. The initial value is 0.


.. function:: glClipPlane (plane, equation):

   Specify a plane against which all geometry is clipped

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glClipPlane.xml>`__

   :type plane: Enumerated constant
   :arg plane: Specifies which clipping plane is being positioned.
   :type equation: :class:`bgl.Buffer` object I{type GL_FLOAT}(double)
   :arg equation: Specifies the address of an array of four double- precision
      floating-point values. These values are interpreted as a plane equation.


.. function:: glColor (red, green, blue, alpha):

   B{glColor3b, glColor3d, glColor3f, glColor3i, glColor3s, glColor3ub, glColor3ui, glColor3us,
   glColor4b, glColor4d, glColor4f, glColor4i, glColor4s, glColor4ub, glColor4ui, glColor4us,
   glColor3bv, glColor3dv, glColor3fv, glColor3iv, glColor3sv, glColor3ubv, glColor3uiv,
   glColor3usv, glColor4bv, glColor4dv, glColor4fv, glColor4iv, glColor4sv, glColor4ubv,
   glColor4uiv, glColor4usv}

   Set a new color.

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glColor.xml>`__

   :type red, green, blue, alpha: Depends on function prototype.
   :arg red, green, blue: Specify new red, green, and blue values for the current color.
   :arg alpha: Specifies a new alpha value for the current color. Included only in the
      four-argument glColor4 commands. (With '4' colors only)


.. function:: glColorMask(red, green, blue, alpha):

   Enable and disable writing of frame buffer color components

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glColorMask.xml>`__

   :type red, green, blue, alpha: int (boolean)
   :arg red, green, blue, alpha: Specify whether red, green, blue, and alpha can or cannot be
      written into the frame buffer. The initial values are all GL_TRUE, indicating that the
      color components can be written.


.. function:: glColorMaterial(face, mode):

   Cause a material color to track the current color

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glColorMaterial.xml>`__

   :type face: Enumerated constant
   :arg face: Specifies whether front, back, or both front and back material parameters should
      track the current color.
   :type mode: Enumerated constant
   :arg mode: Specifies which of several material parameters track the current color.


.. function:: glCopyPixels(x, y, width, height, type):

   Copy pixels in the frame buffer

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glCopyPixels.xml>`__

   :type x, y: int
   :arg x, y: Specify the window coordinates of the lower left corner of the rectangular
      region of pixels to be copied.
   :type width, height: int
   :arg width,height: Specify the dimensions of the rectangular region of pixels to be copied.
      Both must be non-negative.
   :type type: Enumerated constant
   :arg type: Specifies whether color values, depth values, or stencil values are to be copied.


   def glCopyTexImage2D(target, level, internalformat, x, y, width, height, border):

   Copy pixels into a 2D texture image

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glCopyTexImage2D.xml>`__

   :type target: Enumerated constant
   :arg target: Specifies the target texture.
   :type level: int
   :arg level: Specifies the level-of-detail number. Level 0 is the base image level.
      Level n is the nth mipmap reduction image.
   :type internalformat: int
   :arg internalformat: Specifies the number of color components in the texture.
   :type width: int
   :type x, y: int
   :arg x, y: Specify the window coordinates of the first pixel that is copied
      from the frame buffer. This location is the lower left corner of a rectangular
      block of pixels.
   :arg width: Specifies the width of the texture image. Must be 2n+2(border) for
      some integer n. All implementations support texture images that are at least 64
      texels wide.
   :type height: int
   :arg height: Specifies the height of the texture image. Must be 2m+2(border) for
      some integer m. All implementations support texture images that are at least 64
      texels high.
   :type border: int
   :arg border: Specifies the width of the border. Must be either 0 or 1.


.. function:: glCullFace(mode):

   Specify whether front- or back-facing facets can be culled

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glCullFace.xml>`__

   :type mode: Enumerated constant
   :arg mode: Specifies whether front- or back-facing facets are candidates for culling.


.. function:: glDeleteLists(list, range):

   Delete a contiguous group of display lists

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glDeleteLists.xml>`__

   :type list: unsigned int
   :arg list: Specifies the integer name of the first display list to delete
   :type range: int
   :arg range: Specifies the number of display lists to delete


.. function:: glDeleteTextures(n, textures):

   Delete named textures

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glDeleteTextures.xml>`__

   :type n: int
   :arg n: Specifies the number of textures to be deleted
   :type textures: :class:`bgl.Buffer` I{GL_INT}
   :arg textures: Specifies an array of textures to be deleted


.. function:: glDepthFunc(func):

   Specify the value used for depth buffer comparisons

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glDepthFunc.xml>`__

   :type func: Enumerated constant
   :arg func: Specifies the depth comparison function.


.. function:: glDepthMask(flag):

   Enable or disable writing into the depth buffer

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glDepthMask.xml>`__

   :type flag: int (boolean)
   :arg flag: Specifies whether the depth buffer is enabled for writing. If flag is GL_FALSE,
      depth buffer writing is disabled. Otherwise, it is enabled. Initially, depth buffer
      writing is enabled.


.. function:: glDepthRange(zNear, zFar):

   Specify mapping of depth values from normalized device coordinates to window coordinates

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glDepthRange.xml>`__

   :type zNear: int
   :arg zNear: Specifies the mapping of the near clipping plane to window coordinates.
      The initial value is 0.
   :type zFar: int
   :arg zFar: Specifies the mapping of the far clipping plane to window coordinates.
      The initial value is 1.


.. function:: glDisable(cap):

   Disable server-side GL capabilities

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glEnable.xml>`__

   :type cap: Enumerated constant
   :arg cap: Specifies a symbolic constant indicating a GL capability.


.. function:: glDrawBuffer(mode):

   Specify which color buffers are to be drawn into

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glDrawBuffer.xml>`__

   :type mode: Enumerated constant
   :arg mode: Specifies up to four color buffers to be drawn into.


.. function:: glDrawPixels(width, height, format, type, pixels):

   Write a block of pixels to the frame buffer

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glDrawPixels.xml>`__

   :type width, height: int
   :arg width, height: Specify the dimensions of the pixel rectangle to be
      written into the frame buffer.
   :type format: Enumerated constant
   :arg format: Specifies the format of the pixel data.
   :type type: Enumerated constant
   :arg type: Specifies the data type for pixels.
   :type pixels: :class:`bgl.Buffer` object
   :arg pixels: Specifies a pointer to the pixel data.


.. function:: glEdgeFlag (flag):

   B{glEdgeFlag, glEdgeFlagv}

   Flag edges as either boundary or non-boundary

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glEdgeFlag.xml>`__

   :type flag: Depends of function prototype
   :arg flag: Specifies the current edge flag value.The initial value is GL_TRUE.


.. function:: glEnable(cap):

   Enable server-side GL capabilities

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glEnable.xml>`__

   :type cap: Enumerated constant
   :arg cap: Specifies a symbolic constant indicating a GL capability.


.. function:: glEnd():

   Delimit the vertices of a primitive or group of like primitives

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glBegin.xml>`__


.. function:: glEndList():

   Create or replace a display list

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glNewList.xml>`__


.. function:: glEvalCoord (u,v):

   B{glEvalCoord1d, glEvalCoord1f, glEvalCoord2d, glEvalCoord2f, glEvalCoord1dv, glEvalCoord1fv,
   glEvalCoord2dv, glEvalCoord2fv}

   Evaluate enabled one- and two-dimensional maps

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glEvalCoord.xml>`__

   :type u: Depends on function prototype.
   :arg u: Specifies a value that is the domain coordinate u to the basis function defined
      in a previous glMap1 or glMap2 command. If the function prototype ends in 'v' then
      u specifies a pointer to an array containing either one or two domain coordinates. The first
      coordinate is u. The second coordinate is v, which is present only in glEvalCoord2 versions.
   :type v: Depends on function prototype. (only with '2' prototypes)
   :arg v: Specifies a value that is the domain coordinate v to the basis function defined
      in a previous glMap2 command. This argument is not present in a glEvalCoord1 command.


.. function:: glEvalMesh (mode, i1, i2):

   B{glEvalMesh1 or glEvalMesh2}

   Compute a one- or two-dimensional grid of points or lines

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glEvalMesh.xml>`__

   :type mode: Enumerated constant
   :arg mode: In glEvalMesh1, specifies whether to compute a one-dimensional
      mesh of points or lines.
   :type i1, i2: int
   :arg i1, i2: Specify the first and last integer values for the grid domain variable i.


.. function:: glEvalPoint (i, j):

   B{glEvalPoint1 and glEvalPoint2}

   Generate and evaluate a single point in a mesh

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glEvalPoint.xml>`__

   :type i: int
   :arg i: Specifies the integer value for grid domain variable i.
   :type j: int (only with '2' prototypes)
   :arg j: Specifies the integer value for grid domain variable j (glEvalPoint2 only).


.. function:: glFeedbackBuffer (size, type, buffer):

   Controls feedback mode

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glFeedbackBuffer.xml>`__

   :type size: int
   :arg size: Specifies the maximum number of values that can be written into buffer.
   :type type: Enumerated constant
   :arg type: Specifies a symbolic constant that describes the information that
      will be returned for each vertex.
   :type buffer: :class:`bgl.Buffer` object I{GL_FLOAT}
   :arg buffer: Returns the feedback data.


.. function:: glFinish():

   Block until all GL execution is complete

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glFinish.xml>`__


.. function:: glFlush():

   Force Execution of GL commands in finite time

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glFlush.xml>`__


.. function:: glFog (pname, param):

   B{glFogf, glFogi, glFogfv, glFogiv}

   Specify fog parameters

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glFog.xml>`__

   :type pname: Enumerated constant
   :arg pname: Specifies a single-valued fog parameter. If the function prototype
      ends in 'v' specifies a fog parameter.
   :type param: Depends on function prototype.
   :arg param: Specifies the value or values to be assigned to pname. GL_FOG_COLOR
      requires an array of four values. All other parameters accept an array containing
      only a single value.


.. function:: glFrontFace(mode):

   Define front- and back-facing polygons

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glFrontFace.xml>`__

   :type mode: Enumerated constant
   :arg mode: Specifies the orientation of front-facing polygons.


.. function:: glFrustum(left, right, bottom, top, zNear, zFar):

   Multiply the current matrix by a perspective matrix

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glFrustum.xml>`__

   :type left, right: double (float)
   :arg left, right: Specify the coordinates for the left and right vertical
      clipping planes.
   :type top, bottom: double (float)
   :arg top, bottom: Specify the coordinates for the bottom and top horizontal
      clipping planes.
   :type zNear, zFar: double (float)
   :arg zNear, zFar: Specify the distances to the near and far depth clipping planes.
      Both distances must be positive.


.. function:: glGenLists(range):

   Generate a contiguous set of empty display lists

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glGenLists.xml>`__

   :type range: int
   :arg range: Specifies the number of contiguous empty display lists to be generated.


.. function:: glGenTextures(n, textures):

   Generate texture names

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glGenTextures.xml>`__

   :type n: int
   :arg n: Specifies the number of textures name to be generated.
   :type textures: :class:`bgl.Buffer` object I{type GL_INT}
   :arg textures: Specifies an array in which the generated textures names are stored.


.. function:: glGet (pname, param):

   B{glGetBooleanv, glGetfloatv, glGetFloatv, glGetIntegerv}

   Return the value or values of a selected parameter

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glGet.xml>`__

   :type pname: Enumerated constant
   :arg pname: Specifies the parameter value to be returned.
   :type param: Depends on function prototype.
   :arg param: Returns the value or values of the specified parameter.


.. function:: glGetClipPlane(plane, equation):

   Return the coefficients of the specified clipping plane

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glGetClipPlane.xml>`__

   :type plane: Enumerated constant
   :arg plane: Specifies a clipping plane. The number of clipping planes depends on the
      implementation, but at least six clipping planes are supported. They are identified by
      symbolic names of the form GL_CLIP_PLANEi where 0 < i < GL_MAX_CLIP_PLANES.
   :type equation:  :class:`bgl.Buffer` object I{type GL_FLOAT}
   :arg equation:  Returns four float (double)-precision values that are the coefficients of the
      plane equation of plane in eye coordinates. The initial value is (0, 0, 0, 0).


.. function:: glGetError():

   Return error information

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glGetError.xml>`__


.. function:: glGetLight (light, pname, params):

   B{glGetLightfv and glGetLightiv}

   Return light source parameter values

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glGetLight.xml>`__

   :type light: Enumerated constant
   :arg light: Specifies a light source. The number of possible lights depends on the
      implementation, but at least eight lights are supported. They are identified by symbolic
      names of the form GL_LIGHTi where 0 < i < GL_MAX_LIGHTS.
   :type pname: Enumerated constant
   :arg pname: Specifies a light source parameter for light.
   :type params:  :class:`bgl.Buffer` object. Depends on function prototype.
   :arg params: Returns the requested data.


.. function:: glGetMap (target, query, v):

   B{glGetMapdv, glGetMapfv, glGetMapiv}

   Return evaluator parameters

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glGetMap.xml>`_

   :type target: Enumerated constant
   :arg target: Specifies the symbolic name of a map.
   :type query: Enumerated constant
   :arg query: Specifies which parameter to return.
   :type v: :class:`bgl.Buffer` object. Depends on function prototype.
   :arg v: Returns the requested data.


.. function:: glGetMaterial (face, pname, params):

   B{glGetMaterialfv, glGetMaterialiv}

   Return material parameters

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glGetMaterial.xml>`__

   :type face: Enumerated constant
   :arg face: Specifies which of the two materials is being queried.
      representing the front and back materials, respectively.
   :type pname: Enumerated constant
   :arg pname: Specifies the material parameter to return.
   :type params: :class:`bgl.Buffer` object. Depends on function prototype.
   :arg params: Returns the requested data.


.. function:: glGetPixelMap (map, values):

   B{glGetPixelMapfv, glGetPixelMapuiv, glGetPixelMapusv}

   Return the specified pixel map

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glGetPixelMap.xml>`__

   :type map:  Enumerated constant
   :arg map: Specifies the name of the pixel map to return.
   :type values: :class:`bgl.Buffer` object. Depends on function prototype.
   :arg values: Returns the pixel map contents.


.. function:: glGetPolygonStipple(mask):

   Return the polygon stipple pattern

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glGetPolygonStipple.xml>`__

   :type mask: :class:`bgl.Buffer` object I{type GL_BYTE}
   :arg mask: Returns the stipple pattern. The initial value is all 1's.


.. function:: glGetString(name):

   Return a string describing the current GL connection

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glGetString.xml>`__

   :type name: Enumerated constant
   :arg name: Specifies a symbolic constant.



.. function:: glGetTexEnv (target, pname, params):

   B{glGetTexEnvfv, glGetTexEnviv}

   Return texture environment parameters

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glGetTexEnv.xml>`__

   :type target: Enumerated constant
   :arg target: Specifies a texture environment. Must be GL_TEXTURE_ENV.
   :type pname: Enumerated constant
   :arg pname: Specifies the symbolic name of a texture environment parameter.
   :type params: :class:`bgl.Buffer` object. Depends on function prototype.
   :arg params: Returns the requested data.


.. function:: glGetTexGen (coord, pname, params):

   B{glGetTexGendv, glGetTexGenfv, glGetTexGeniv}

   Return texture coordinate generation parameters

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glGetTexGen.xml>`__

   :type coord: Enumerated constant
   :arg coord: Specifies a texture coordinate.
   :type pname: Enumerated constant
   :arg pname: Specifies the symbolic name of the value(s) to be returned.
   :type params: :class:`bgl.Buffer` object. Depends on function prototype.
   :arg params: Returns the requested data.


.. function:: glGetTexImage(target, level, format, type, pixels):

   Return a texture image

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glGetTexImage.xml>`__

   :type target: Enumerated constant
   :arg target: Specifies which texture is to be obtained.
   :type level: int
   :arg level: Specifies the level-of-detail number of the desired image.
      Level 0 is the base image level. Level n is the nth mipmap reduction image.
   :type format: Enumerated constant
   :arg format: Specifies a pixel format for the returned data.
   :type type: Enumerated constant
   :arg type: Specifies a pixel type for the returned data.
   :type pixels: :class:`bgl.Buffer` object.
   :arg pixels: Returns the texture image. Should be a pointer to an array of the
      type specified by type


.. function:: glGetTexLevelParameter (target, level, pname, params):

   B{glGetTexLevelParameterfv, glGetTexLevelParameteriv}

   return texture parameter values for a specific level of detail

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glGetTexLevelParameter.xml>`__

   :type target: Enumerated constant
   :arg target: Specifies the symbolic name of the target texture.
   :type level: int
   :arg level: Specifies the level-of-detail number of the desired image.
      Level 0 is the base image level. Level n is the nth mipmap reduction image.
   :type pname: Enumerated constant
   :arg pname: Specifies the symbolic name of a texture parameter.
   :type params: :class:`bgl.Buffer` object. Depends on function prototype.
   :arg params: Returns the requested data.


.. function:: glGetTexParameter (target, pname, params):

   B{glGetTexParameterfv, glGetTexParameteriv}

   Return texture parameter values

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glGetTexParameter.xml>`__

   :type target: Enumerated constant
   :arg target: Specifies the symbolic name of the target texture.
   :type pname: Enumerated constant
   :arg pname: Specifies the symbolic name the target texture.
   :type params: :class:`bgl.Buffer` object. Depends on function prototype.
   :arg params: Returns the texture parameters.


.. function:: glHint(target, mode):

   Specify implementation-specific hints

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glHint.xml>`__

   :type target: Enumerated constant
   :arg target: Specifies a symbolic constant indicating the behavior to be
      controlled.
   :type mode: Enumerated constant
   :arg mode: Specifies a symbolic constant indicating the desired behavior.


.. function:: glIndex(c):

   B{glIndexd, glIndexf, glIndexi, glIndexs,  glIndexdv, glIndexfv, glIndexiv, glIndexsv}

   Set the current color index

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glIndex.xml>`__

   :type c: :class:`bgl.Buffer` object. Depends on function prototype.
   :arg c: Specifies a pointer to a one element array that contains the new value for
      the current color index.


.. function:: glIndexMask(mask):

   Control the writing of individual bits in the color index buffers

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glIndexMask.xml>`__

   :type mask: int
   :arg mask: Specifies a bit mask to enable and disable the writing of individual bits
      in the color index buffers.
      Initially, the mask is all 1's.


.. function:: glInitNames():

   Initialize the name stack

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glInitNames.xml>`__


.. function:: glIsEnabled(cap):

   Test whether a capability is enabled

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glIsEnabled.xml>`__

   :type cap: Enumerated constant
   :arg cap: Specifies a constant representing a GL capability.


.. function:: glIsList(list):

   Determine if a name corresponds to a display-list

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glIsList.xml>`__

   :type list: unsigned int
   :arg list: Specifies a potential display-list name.


.. function:: glIsTexture(texture):

   Determine if a name corresponds to a texture

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glIsTexture.xml>`__

   :type texture: unsigned int
   :arg texture: Specifies a value that may be the name of a texture.


.. function:: glLight (light, pname, param):

   B{glLightf,glLighti, glLightfv, glLightiv}

   Set the light source parameters

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glLight.xml>`__

   :type light: Enumerated constant
   :arg light: Specifies a light. The number of lights depends on the implementation,
      but at least eight lights are supported. They are identified by symbolic names of the
      form GL_LIGHTi where 0 < i < GL_MAX_LIGHTS.
   :type pname: Enumerated constant
   :arg pname: Specifies a single-valued light source parameter for light.
   :type param: Depends on function prototype.
   :arg param: Specifies the value that parameter pname of light source light will be set to.
      If function prototype ends in 'v' specifies a pointer to the value or values that
      parameter pname of light source light will be set to.


.. function:: glLightModel (pname, param):

   B{glLightModelf, glLightModeli, glLightModelfv, glLightModeliv}

   Set the lighting model parameters

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glLightModel.xml>`__

   :type pname:  Enumerated constant
   :arg pname: Specifies a single-value light model parameter.
   :type param: Depends on function prototype.
   :arg param: Specifies the value that param will be set to. If function prototype ends in 'v'
      specifies a pointer to the value or values that param will be set to.


.. function:: glLineStipple(factor, pattern):

   Specify the line stipple pattern

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glLineStipple.xml>`__

   :type factor: int
   :arg factor: Specifies a multiplier for each bit in the line stipple pattern.
      If factor is 3, for example, each bit in the pattern is used three times before
      the next bit in the pattern is used. factor is clamped to the range [1, 256] and
      defaults to 1.
   :type pattern: unsigned short int
   :arg pattern: Specifies a 16-bit integer whose bit pattern determines which fragments
      of a line will be drawn when the line is rasterized. Bit zero is used first; the default
      pattern is all 1's.


.. function:: glLineWidth(width):

   Specify the width of rasterized lines.

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glLineWidth.xml>`__

   :type width: float
   :arg width: Specifies the width of rasterized lines. The initial value is 1.


.. function:: glListBase(base):

   Set the display-list base for glCallLists

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glListBase.xml>`__

   :type base: unsigned int
   :arg base: Specifies an integer offset that will be added to glCallLists
      offsets to generate display-list names. The initial value is 0.


.. function:: glLoadIdentity():

   Replace the current matrix with the identity matrix

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glLoadIdentity.xml>`__


.. function:: glLoadMatrix (m):

   B{glLoadMatrixd, glLoadMatixf}

   Replace the current matrix with the specified matrix

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glLoadMatrix.xml>`__

   :type m: :class:`bgl.Buffer` object. Depends on function prototype.
   :arg m: Specifies a pointer to 16 consecutive values, which are used as the elements
      of a 4x4 column-major matrix.


.. function:: glLoadName(name):

   Load a name onto the name stack.

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glLoadName.xml>`__

   :type name: unsigned int
   :arg name: Specifies a name that will replace the top value on the name stack.


.. function:: glLogicOp(opcode):

   Specify a logical pixel operation for color index rendering

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glLogicOp.xml>`__

   :type opcode: Enumerated constant
   :arg opcode: Specifies a symbolic constant that selects a logical operation.


.. function:: glMap1 (target, u1, u2, stride, order, points):

   B{glMap1d, glMap1f}

   Define a one-dimensional evaluator

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glMap1.xml>`__

   :type target: Enumerated constant
   :arg target: Specifies the kind of values that are generated by the evaluator.
   :type u1, u2: Depends on function prototype.
   :arg u1,u2: Specify a linear mapping of u, as presented to glEvalCoord1, to ^, t
      he variable that is evaluated by the equations specified by this command.
   :type stride: int
   :arg stride: Specifies the number of floats or float (double)s between the beginning
      of one control point and the beginning of the next one in the data structure
      referenced in points. This allows control points to be embedded in arbitrary data
      structures. The only constraint is that the values for a particular control point must
      occupy contiguous memory locations.
   :type order: int
   :arg order: Specifies the number of control points. Must be positive.
   :type points: :class:`bgl.Buffer` object. Depends on function prototype.
   :arg points: Specifies a pointer to the array of control points.


.. function:: glMap2 (target, u1, u2, ustride, uorder, v1, v2, vstride, vorder, points):

   B{glMap2d, glMap2f}

   Define a two-dimensional evaluator

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glMap2.xml>`__

   :type target: Enumerated constant
   :arg target: Specifies the kind of values that are generated by the evaluator.
   :type u1, u2: Depends on function prototype.
   :arg u1,u2: Specify a linear mapping of u, as presented to glEvalCoord2, to ^, t
      he variable that is evaluated by the equations specified by this command. Initially
      u1 is 0 and u2 is 1.
   :type ustride: int
   :arg ustride: Specifies the number of floats or float (double)s between the beginning
      of control point R and the beginning of control point R ij, where i and j are the u
      and v control point indices, respectively. This allows control points to be embedded
      in arbitrary data structures. The only constraint is that the values for a particular
      control point must occupy contiguous memory locations. The initial value of ustride is 0.
   :type uorder: int
   :arg uorder: Specifies the dimension of the control point array in the u axis.
      Must be positive. The initial value is 1.
   :type v1, v2: Depends on function prototype.
   :arg v1, v2: Specify a linear mapping of v, as presented to glEvalCoord2,
      to ^, one of the two variables that are evaluated by the equations
      specified by this command. Initially, v1 is 0 and v2 is 1.
   :type vstride: int
   :arg vstride: Specifies the number of floats or float (double)s between the
     beginning of control point R and the beginning of control point R ij,
     where i and j are the u and v control point(indices, respectively.
     This allows control points to be embedded in arbitrary data structures.
     The only constraint is that the values for a particular control point must
     occupy contiguous memory locations. The initial value of vstride is 0.
   :type vorder: int
   :arg vorder: Specifies the dimension of the control point array in the v axis.
      Must be positive. The initial value is 1.
   :type points: :class:`bgl.Buffer` object. Depends on function prototype.
   :arg points: Specifies a pointer to the array of control points.


.. function:: glMapGrid (un, u1,u2 ,vn, v1, v2):

   B{glMapGrid1d, glMapGrid1f, glMapGrid2d, glMapGrid2f}

   Define a one- or two-dimensional mesh

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glMapGrid.xml>`__

   :type un: int
   :arg un: Specifies the number of partitions in the grid range interval
      [u1, u2]. Must be positive.
   :type u1, u2: Depends on function prototype.
   :arg u1, u2: Specify the mappings for integer grid domain values i=0 and i=un.
   :type vn: int
   :arg vn: Specifies the number of partitions in the grid range interval
      [v1, v2] (glMapGrid2 only).
   :type v1, v2: Depends on function prototype.
   :arg v1, v2: Specify the mappings for integer grid domain values j=0 and j=vn
      (glMapGrid2 only).


.. function:: glMaterial (face, pname, params):

   Specify material parameters for the lighting model.

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glMaterial.xml>`__

   :type face: Enumerated constant
   :arg face: Specifies which face or faces are being updated. Must be one of:
   :type pname: Enumerated constant
   :arg pname: Specifies the single-valued material parameter of the face
      or faces that is being updated. Must be GL_SHININESS.
   :type params: int
   :arg params: Specifies the value that parameter GL_SHININESS will be set to.
      If function prototype ends in 'v' specifies a pointer to the value or values that
      pname will be set to.


.. function:: glMatrixMode(mode):

   Specify which matrix is the current matrix.

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glMatrixMode.xml>`__

   :type mode: Enumerated constant
   :arg mode: Specifies which matrix stack is the target for subsequent matrix operations.


.. function:: glMultMatrix (m):

   B{glMultMatrixd, glMultMatrixf}

   Multiply the current matrix with the specified matrix

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glMultMatrix.xml>`__

   :type m: :class:`bgl.Buffer` object. Depends on function prototype.
   :arg m: Points to 16 consecutive values that are used as the elements of a 4x4 column
      major matrix.


.. function:: glNewList(list, mode):

   Create or replace a display list

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glNewList.xml>`__

   :type list: unsigned int
   :arg list: Specifies the display list name
   :type mode: Enumerated constant
   :arg mode: Specifies the compilation mode.


.. function:: glNormal3 (nx, ny, nz, v):

   B{Normal3b, Normal3bv, Normal3d, Normal3dv, Normal3f, Normal3fv, Normal3i, Normal3iv,
   Normal3s, Normal3sv}

   Set the current normal vector

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glNormal.xml>`__

   :type nx, ny, nz: Depends on function prototype. (non - 'v' prototypes only)
   :arg nx, ny, nz: Specify the x, y, and z coordinates of the new current normal.
      The initial value of the current normal is the unit vector, (0, 0, 1).
   :type v: :class:`bgl.Buffer` object. Depends on function prototype. ('v' prototypes)
   :arg v: Specifies a pointer to an array of three elements: the x, y, and z coordinates
      of the new current normal.


.. function:: glOrtho(left, right, bottom, top, zNear, zFar):

   Multiply the current matrix with an orthographic matrix

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glOrtho.xml>`__

   :type left, right: double (float)
   :arg left, right: Specify the coordinates for the left and
      right vertical clipping planes.
   :type bottom, top: double (float)
   :arg bottom, top: Specify the coordinates for the bottom and top
      horizontal clipping planes.
   :type zNear, zFar: double (float)
   :arg zNear, zFar: Specify the distances to the nearer and farther
      depth clipping planes. These values are negative if the plane is to be behind the viewer.


.. function:: glPassThrough(token):

   Place a marker in the feedback buffer

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glPassThrough.xml>`__

   :type token: float
   :arg token: Specifies a marker value to be placed in the feedback
      buffer following a GL_PASS_THROUGH_TOKEN.


.. function:: glPixelMap (map, mapsize, values):

   B{glPixelMapfv, glPixelMapuiv, glPixelMapusv}

   Set up pixel transfer maps

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glPixelMap.xml>`__

   :type map: Enumerated constant
   :arg map: Specifies a symbolic map name.
   :type mapsize: int
   :arg mapsize: Specifies the size of the map being defined.
   :type values: :class:`bgl.Buffer` object. Depends on function prototype.
   :arg values: Specifies an array of mapsize values.


.. function:: glPixelStore (pname, param):

   B{glPixelStoref, glPixelStorei}

   Set pixel storage modes

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glPixelStore.xml>`__

   :type pname: Enumerated constant
   :arg pname: Specifies the symbolic name of the parameter to be set.
      Six values affect the packing of pixel data into memory.
      Six more affect the unpacking of pixel data from memory.
   :type param: Depends on function prototype.
   :arg param: Specifies the value that pname is set to.


.. function:: glPixelTransfer (pname, param):

   B{glPixelTransferf, glPixelTransferi}

   Set pixel transfer modes

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glPixelTransfer.xml>`__

   :type pname: Enumerated constant
   :arg pname: Specifies the symbolic name of the pixel transfer parameter to be set.
   :type param: Depends on function prototype.
   :arg param: Specifies the value that pname is set to.


.. function:: glPixelZoom(xfactor, yfactor):

   Specify the pixel zoom factors

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glPixelZoom.xml>`__

   :type xfactor, yfactor: float
   :arg xfactor, yfactor: Specify the x and y zoom factors for pixel write operations.


.. function:: glPointSize(size):

   Specify the diameter of rasterized points

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glPointSize.xml>`__

   :type size: float
   :arg size: Specifies the diameter of rasterized points. The initial value is 1.


.. function:: glPolygonMode(face, mode):

   Select a polygon rasterization mode

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glPolygonMode.xml>`__

   :type face: Enumerated constant
   :arg face: Specifies the polygons that mode applies to.
      Must be GL_FRONT for front-facing polygons, GL_BACK for back- facing
      polygons, or GL_FRONT_AND_BACK for front- and back-facing polygons.
   :type mode: Enumerated constant
   :arg mode: Specifies how polygons will be rasterized.
      The initial value is GL_FILL for both front- and back- facing polygons.


.. function:: glPolygonOffset(factor, units):

   Set the scale and units used to calculate depth values

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glPolygonOffset.xml>`__

   :type factor: float
   :arg factor: Specifies a scale factor that is used to create a variable depth
      offset for each polygon. The initial value is 0.
   :type units:  float
   :arg units: Is multiplied by an implementation-specific value to create a
      constant depth offset. The initial value is 0.


.. function:: glPolygonStipple(mask):

   Set the polygon stippling pattern

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glPolygonStipple.xml>`__

   :type mask: :class:`bgl.Buffer` object I{type GL_BYTE}
   :arg mask: Specifies a pointer to a 32x32 stipple pattern that will be unpacked
      from memory in the same way that glDrawPixels unpacks pixels.


.. function:: glPopAttrib():

   Pop the server attribute stack

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glPopAttrib.xml>`__


.. function:: glPopClientAttrib():

   Pop the client attribute stack

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glPopClientAttrib.xml>`__


.. function:: glPopMatrix():

   Pop the current matrix stack

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glPopMatrix.xml>`__


.. function:: glPopName():

   Pop the name stack

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glPopName.xml>`__


.. function:: glPrioritizeTextures(n, textures, priorities):

   Set texture residence priority

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glPrioritizeTextures.xml>`__

   :type n: int
   :arg n: Specifies the number of textures to be prioritized.
   :type textures: :class:`bgl.Buffer` I{type GL_INT}
   :arg textures: Specifies an array containing the names of the textures to be prioritized.
   :type priorities: :class:`bgl.Buffer` I{type GL_FLOAT}
   :arg priorities: Specifies an array containing the texture priorities.
      A priority given in an element of priorities applies to the texture named
      by the corresponding element of textures.


.. function:: glPushAttrib(mask):

   Push the server attribute stack

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glPushAttrib.xml>`__

   :type mask: Enumerated constant(s)
   :arg mask: Specifies a mask that indicates which attributes to save.


.. function:: glPushClientAttrib(mask):

   Push the client attribute stack

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glPushClientAttrib.xml>`__

   :type mask: Enumerated constant(s)
   :arg mask: Specifies a mask that indicates which attributes to save.


.. function:: glPushMatrix():

   Push the current matrix stack

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glPushMatrix.xml>`__


.. function:: glPushName(name):

   Push the name stack

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glPushName.xml>`__

   :type name: unsigned int
   :arg name: Specifies a name that will be pushed onto the name stack.


.. function:: glRasterPos (x,y,z,w):

   B{glRasterPos2d, glRasterPos2f, glRasterPos2i, glRasterPos2s, glRasterPos3d,
   glRasterPos3f, glRasterPos3i, glRasterPos3s, glRasterPos4d, glRasterPos4f,
   glRasterPos4i, glRasterPos4s, glRasterPos2dv, glRasterPos2fv, glRasterPos2iv,
   glRasterPos2sv, glRasterPos3dv, glRasterPos3fv, glRasterPos3iv, glRasterPos3sv,
   glRasterPos4dv, glRasterPos4fv, glRasterPos4iv, glRasterPos4sv}

   Specify the raster position for pixel operations

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glRasterPos.xml>`__

   :type x, y, z, w: Depends on function prototype. (z and w for '3' and '4' prototypes only)
   :arg x, y, z, w: Specify the x,y,z, and w object coordinates (if present) for the
      raster position.  If function prototype ends in 'v' specifies a pointer to an array of two,
      three, or four elements, specifying x, y, z, and w coordinates, respectively.

   .. note::

      If you are drawing to the 3d view with a Scriptlink of a space handler
      the zoom level of the panels will scale the glRasterPos by the view matrix.
      so a X of 10 will not always offset 10 pixels as you would expect.

      To work around this get the scale value of the view matrix and use it to scale your pixel values.

      .. code-block:: python

        import bgl
        xval, yval= 100, 40
        # Get the scale of the view matrix
        view_matrix = bgl.Buffer(bgl.GL_FLOAT, 16)
        bgl.glGetFloatv(bgl.GL_MODELVIEW_MATRIX, view_matrix)
        f = 1.0 / view_matrix[0]

        # Instead of the usual glRasterPos2i(xval, yval)
        bgl.glRasterPos2f(xval * f, yval * f)


.. function:: glReadBuffer(mode):

   Select a color buffer source for pixels.

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glReadBuffer.xml>`__

   :type mode: Enumerated constant
   :arg mode: Specifies a color buffer.


.. function:: glReadPixels(x, y, width, height, format, type, pixels):

   Read a block of pixels from the frame buffer

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glReadPixels.xml>`__

   :type x, y: int
   :arg x, y: Specify the window coordinates of the first pixel that is read
      from the frame buffer. This location is the lower left corner of a rectangular
      block of pixels.
   :type width, height: int
   :arg width, height: Specify the dimensions of the pixel rectangle. width and
      height of one correspond to a single pixel.
   :type format: Enumerated constant
   :arg format: Specifies the format of the pixel data.
   :type type: Enumerated constant
   :arg type: Specifies the data type of the pixel data.
   :type pixels: :class:`bgl.Buffer` object
   :arg pixels: Returns the pixel data.


.. function:: glRect (x1,y1,x2,y2,v1,v2):

   B{glRectd, glRectf, glRecti, glRects, glRectdv, glRectfv, glRectiv, glRectsv}

   Draw a rectangle

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glRect.xml>`__

   :type x1, y1: Depends on function prototype. (for non 'v' prototypes only)
   :arg x1, y1: Specify one vertex of a rectangle
   :type x2, y2: Depends on function prototype. (for non 'v' prototypes only)
   :arg x2, y2: Specify the opposite vertex of the rectangle
   :type v1, v2: Depends on function prototype. (for 'v' prototypes only)
   :arg v1, v2: Specifies a pointer to one vertex of a rectangle and the pointer
      to the opposite vertex of the rectangle


.. function:: glRenderMode(mode):

   Set rasterization mode

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glRenderMode.xml>`__

   :type mode: Enumerated constant
   :arg mode: Specifies the rasterization mode.


.. function:: glRotate (angle, x, y, z):

   B{glRotated, glRotatef}

   Multiply the current matrix by a rotation matrix

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glRotate.xml>`__

   :type angle:  Depends on function prototype.
   :arg angle:  Specifies the angle of rotation in degrees.
   :type x, y, z:  Depends on function prototype.
   :arg x, y, z:  Specify the x, y, and z coordinates of a vector respectively.


.. function:: glScale (x,y,z):

   B{glScaled, glScalef}

   Multiply the current matrix by a general scaling matrix

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glScale.xml>`__

   :type x, y, z: Depends on function prototype.
   :arg x, y, z: Specify scale factors along the x, y, and z axes, respectively.


.. function:: glScissor(x,y,width,height):

   Define the scissor box

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glScissor.xml>`__

   :type x, y: int
   :arg x, y: Specify the lower left corner of the scissor box. Initially (0, 0).
   :type width, height: int
   :arg width height: Specify the width and height of the scissor box. When a
      GL context is first attached to a window, width and height are set to the
      dimensions of that window.


.. function:: glSelectBuffer(size, buffer):

   Establish a buffer for selection mode values

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glSelectBuffer.xml>`__

   :type size: int
   :arg size: Specifies the size of buffer
   :type buffer: :class:`bgl.Buffer` I{type GL_INT}
   :arg buffer: Returns the selection data


.. function:: glShadeModel(mode):

   Select flat or smooth shading

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glShadeModel.xml>`__

   :type mode: Enumerated constant
   :arg mode: Specifies a symbolic value representing a shading technique.


.. function:: glStencilFunc(func, ref, mask):

   Set function and reference value for stencil testing

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man/docbook4/xhtml/glStencilFunc.xml>`__

   :type func: Enumerated constant
   :arg func: Specifies the test function.
   :type ref: int
   :arg ref: Specifies the reference value for the stencil test. ref is clamped
      to the range [0,2n-1], where n is the number of bitplanes in the stencil
      buffer. The initial value is 0.
   :type mask: unsigned int
   :arg mask: Specifies a mask that is ANDed with both the reference value and
      the stored stencil value when the test is done. The initial value is all 1's.


.. function:: glStencilMask(mask):

   Control the writing of individual bits in the stencil planes

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glStencilMask.xml>`__

   :type mask: unsigned int
   :arg mask: Specifies a bit mask to enable and disable writing of individual bits
      in the stencil planes. Initially, the mask is all 1's.


.. function:: glStencilOp(fail, zfail, zpass):

   Set stencil test actions

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glStencilOp.xml>`__

   :type fail: Enumerated constant
   :arg fail: Specifies the action to take when the stencil test fails.
      The initial value is GL_KEEP.
   :type zfail: Enumerated constant
   :arg zfail: Specifies the stencil action when the stencil test passes, but the
      depth test fails. zfail accepts the same symbolic constants as fail.
      The initial value is GL_KEEP.
   :type zpass: Enumerated constant
   :arg zpass: Specifies the stencil action when both the stencil test and the
      depth test pass, or when the stencil test passes and either there is no
      depth buffer or depth testing is not enabled. zpass accepts the same
      symbolic constants
      as fail. The initial value is GL_KEEP.


.. function:: glTexCoord (s,t,r,q,v):

   B{glTexCoord1d, glTexCoord1f, glTexCoord1i, glTexCoord1s, glTexCoord2d, glTexCoord2f,
   glTexCoord2i, glTexCoord2s, glTexCoord3d, glTexCoord3f, glTexCoord3i, glTexCoord3s,
   glTexCoord4d, glTexCoord4f, glTexCoord4i, glTexCoord4s, glTexCoord1dv, glTexCoord1fv,
   glTexCoord1iv, glTexCoord1sv, glTexCoord2dv, glTexCoord2fv, glTexCoord2iv,
   glTexCoord2sv, glTexCoord3dv, glTexCoord3fv, glTexCoord3iv, glTexCoord3sv,
   glTexCoord4dv, glTexCoord4fv, glTexCoord4iv, glTexCoord4sv}

   Set the current texture coordinates

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glTexCoord.xml>`__

   :type s, t, r, q: Depends on function prototype. (r and q for '3' and '4' prototypes only)
   :arg s, t, r, q: Specify s, t, r, and q texture coordinates. Not all parameters are
      present in all forms of the command.
   :type v: :class:`bgl.Buffer` object. Depends on function prototype. (for 'v' prototypes only)
   :arg v: Specifies a pointer to an array of one, two, three, or four elements,
      which in turn specify the s, t, r, and q texture coordinates.


.. function:: glTexEnv  (target, pname, param):

   B{glTextEnvf, glTextEnvi, glTextEnvfv, glTextEnviv}

   Set texture environment parameters

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glTexEnv.xml>`__

   :type target: Enumerated constant
   :arg target: Specifies a texture environment. Must be GL_TEXTURE_ENV.
   :type pname: Enumerated constant
   :arg pname: Specifies the symbolic name of a single-valued texture environment
      parameter. Must be GL_TEXTURE_ENV_MODE.
   :type param: Depends on function prototype.
   :arg param: Specifies a single symbolic constant. If function prototype ends in 'v'
      specifies a pointer to a parameter array that contains either a single
      symbolic constant or an RGBA color


.. function:: glTexGen (coord, pname, param):

   B{glTexGend, glTexGenf, glTexGeni, glTexGendv, glTexGenfv, glTexGeniv}

   Control the generation of texture coordinates

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glTexGen.xml>`__

   :type coord: Enumerated constant
   :arg coord: Specifies a texture coordinate.
   :type pname: Enumerated constant
   :arg pname: Specifies the symbolic name of the texture- coordinate generation function.
   :type param: Depends on function prototype.
   :arg param: Specifies a single-valued texture generation parameter.
      If function prototype ends in 'v' specifies a pointer to an array of texture
      generation parameters. If pname is GL_TEXTURE_GEN_MODE, then the array must
      contain a single symbolic constant. Otherwise, params holds the coefficients
      for the texture-coordinate generation function specified by pname.


.. function:: glTexImage1D(target, level, internalformat, width, border, format, type, pixels):

   Specify a one-dimensional texture image

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glTexImage1D.xml>`__

   :type target: Enumerated constant
   :arg target: Specifies the target texture.
   :type level: int
   :arg level: Specifies the level-of-detail number. Level 0 is the base image level.
      Level n is the nth mipmap reduction image.
   :type internalformat: int
   :arg internalformat: Specifies the number of color components in the texture.
   :type width: int
   :arg width: Specifies the width of the texture image. Must be 2n+2(border)
      for some integer n. All implementations support texture images that are
      at least 64 texels wide. The height of the 1D texture image is 1.
   :type border: int
   :arg border: Specifies the width of the border. Must be either 0 or 1.
   :type format: Enumerated constant
   :arg format: Specifies the format of the pixel data.
   :type type: Enumerated constant
   :arg type: Specifies the data type of the pixel data.
   :type pixels: :class:`bgl.Buffer` object.
   :arg pixels: Specifies a pointer to the image data in memory.


.. function:: glTexImage2D(target, level, internalformat, width, height, border, format, type, pixels):

   Specify a two-dimensional texture image

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glTexImage2D.xml>`__

   :type target: Enumerated constant
   :arg target: Specifies the target texture.
   :type level: int
   :arg level: Specifies the level-of-detail number. Level 0 is the base image level.
      Level n is the nth mipmap reduction image.
   :type internalformat: int
   :arg internalformat: Specifies the number of color components in the texture.
   :type width: int
   :arg width: Specifies the width of the texture image. Must be 2n+2(border)
      for some integer n. All implementations support texture images that are at
      least 64 texels wide.
   :type height: int
   :arg height: Specifies the height of the texture image. Must be 2m+2(border) for
      some integer m. All implementations support texture images that are at
      least 64 texels high.
   :type border: int
   :arg border: Specifies the width of the border. Must be either 0 or 1.
   :type format: Enumerated constant
   :arg format: Specifies the format of the pixel data.
   :type type: Enumerated constant
   :arg type: Specifies the data type of the pixel data.
   :type pixels: :class:`bgl.Buffer` object.
   :arg pixels: Specifies a pointer to the image data in memory.


.. function:: glTexParameter (target, pname, param):

   B{glTexParameterf, glTexParameteri, glTexParameterfv, glTexParameteriv}

   Set texture parameters

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glTexParameter.xml>`__

   :type target: Enumerated constant
   :arg target: Specifies the target texture.
   :type pname: Enumerated constant
   :arg pname: Specifies the symbolic name of a single-valued texture parameter.
   :type param: Depends on function prototype.
   :arg param: Specifies the value of pname. If function prototype ends in 'v' specifies
      a pointer to an array where the value or values of pname are stored.


.. function:: glTranslate (x, y, z):

   B{glTranslatef, glTranslated}

   Multiply the current matrix by a translation matrix

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glTranslate.xml>`__

   :type x, y, z: Depends on function prototype.
   :arg x, y, z: Specify the x, y, and z coordinates of a translation vector.


.. function:: glVertex (x,y,z,w,v):

   B{glVertex2d, glVertex2f, glVertex2i, glVertex2s, glVertex3d, glVertex3f, glVertex3i,
   glVertex3s, glVertex4d, glVertex4f, glVertex4i, glVertex4s, glVertex2dv, glVertex2fv,
   glVertex2iv, glVertex2sv, glVertex3dv, glVertex3fv, glVertex3iv, glVertex3sv, glVertex4dv,
   glVertex4fv, glVertex4iv, glVertex4sv}

   Specify a vertex

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glVertex.xml>`__

   :type x, y, z, w: Depends on function prototype (z and w for '3' and '4' prototypes only)
   :arg x, y, z, w: Specify x, y, z, and w coordinates of a vertex. Not all parameters
      are present in all forms of the command.
   :type v: :class:`bgl.Buffer` object. Depends of function prototype (for 'v'
      prototypes only)
   :arg v: Specifies a pointer to an array of two, three, or four elements. The
      elements of a two-element array are x and y; of a three-element array,
      x, y, and z; and of a four-element array, x, y, z, and w.


.. function:: glViewport(x,y,width,height):

   Set the viewport

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glViewport.xml>`__

   :type x, y: int
   :arg x, y: Specify the lower left corner of the viewport rectangle,
      in pixels. The initial value is (0,0).
   :type width, height: int
   :arg width, height: Specify the width and height of the viewport. When a GL
      context is first attached to a window, width and height are set to the
      dimensions of that window.


.. function:: gluPerspective(fovY, aspect, zNear, zFar):

   Set up a perspective projection matrix.

   .. seealso:: https://www.opengl.org/sdk/docs/man2/xhtml/gluPerspective.xml

   :type fovY: double
   :arg fovY: Specifies the field of view angle, in degrees, in the y direction.
   :type aspect: double
   :arg aspect: Specifies the aspect ratio that determines the field of view in the x direction.
    The aspect ratio is the ratio of x (width) to y (height).
   :type zNear: double
   :arg zNear: Specifies the distance from the viewer to the near clipping plane (always positive).
   :type zFar: double
   :arg zFar: Specifies the distance from the viewer to the far clipping plane (always positive).


.. function:: gluLookAt(eyex, eyey, eyez, centerx, centery, centerz, upx, upy, upz):

   Define a viewing transformation.

   .. seealso:: https://www.opengl.org/sdk/docs/man2/xhtml/gluLookAt.xml

   :type eyex, eyey, eyez: double
   :arg eyex, eyey, eyez: Specifies the position of the eye point.
   :type centerx, centery, centerz: double
   :arg centerx, centery, centerz: Specifies the position of the reference point.
   :type upx, upy, upz: double
   :arg upx, upy, upz: Specifies the direction of the up vector.


.. function:: gluOrtho2D(left, right, bottom, top):

   Define a 2-D orthographic projection matrix.

   .. seealso:: https://www.opengl.org/sdk/docs/man2/xhtml/gluOrtho2D.xml

   :type left, right: double
   :arg left, right: Specify the coordinates for the left and right vertical clipping planes.
   :type bottom, top: double
   :arg bottom, top: Specify the coordinates for the bottom and top horizontal clipping planes.


.. function:: gluPickMatrix(x, y, width, height, viewport):

   Define a picking region.

   .. seealso:: https://www.opengl.org/sdk/docs/man2/xhtml/gluPickMatrix.xml

   :type x, y: double
   :arg x, y: Specify the center of a picking region in window coordinates.
   :type width, height: double
   :arg width, height: Specify the width and height, respectively, of the picking region in window coordinates.
   :type viewport: :class:`bgl.Buffer` object. [int]
   :arg viewport: Specifies the current viewport.


.. function:: gluProject(objx, objy, objz, modelMatrix, projMatrix, viewport, winx, winy, winz):

   Map object coordinates to window coordinates.

   .. seealso:: https://www.opengl.org/sdk/docs/man2/xhtml/gluProject.xml

   :type objx, objy, objz: double
   :arg objx, objy, objz: Specify the object coordinates.
   :type modelMatrix: :class:`bgl.Buffer` object. [double]
   :arg modelMatrix: Specifies the current modelview matrix (as from a glGetDoublev call).
   :type projMatrix: :class:`bgl.Buffer` object. [double]
   :arg projMatrix: Specifies the current projection matrix (as from a glGetDoublev call).
   :type viewport: :class:`bgl.Buffer` object. [int]
   :arg viewport: Specifies the current viewport (as from a glGetIntegerv call).
   :type winx, winy, winz: :class:`bgl.Buffer` object. [double]
   :arg winx, winy, winz: Return the computed window coordinates.


.. function:: gluUnProject(winx, winy, winz, modelMatrix, projMatrix, viewport, objx, objy, objz):

   Map object coordinates to window coordinates.

   .. seealso:: https://www.opengl.org/sdk/docs/man2/xhtml/gluUnProject.xml

   :type winx, winy, winz: double
   :arg winx, winy, winz: Specify the window coordinates to be mapped.
   :type modelMatrix: :class:`bgl.Buffer` object. [double]
   :arg modelMatrix: Specifies the current modelview matrix (as from a glGetDoublev call).
   :type projMatrix: :class:`bgl.Buffer` object. [double]
   :arg projMatrix: Specifies the current projection matrix (as from a glGetDoublev call).
   :type viewport: :class:`bgl.Buffer` object. [int]
   :arg viewport: Specifies the current viewport (as from a glGetIntegerv call).
   :type objx, objy, objz: :class:`bgl.Buffer` object. [double]
   :arg objx, objy, objz: Return the computed object coordinates.


.. function:: glUseProgram(program):

   Installs a program object as part of current rendering state

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glUseProgram.xml>`__

   :type program: int
   :arg program: Specifies the handle of the program object whose executables are to be used as part of current rendering state.


.. function:: glValidateProgram(program):

   Validates a program object

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glValidateProgram.xml>`__

   :type program: int
   :arg program: Specifies the handle of the program object to be validated.


.. function:: glLinkProgram(program):

   Links a program object.

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glLinkProgram.xml>`__

   :type program: int
   :arg program: Specifies the handle of the program object to be linked.


.. function:: glActiveTexture(texture):

   Select active texture unit.

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glActiveTexture.xml>`__

   :type texture: int
   :arg texture: Constant in ``GL_TEXTURE0`` 0 - 8


.. function:: glAttachShader(program, shader):

   Attaches a shader object to a program object.

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glAttachShader.xml>`__

   :type program: int
   :arg program: Specifies the program object to which a shader object will be attached.
   :type shader: int
   :arg shader: Specifies the shader object that is to be attached.


.. function:: glCompileShader(shader):

   Compiles a shader object.

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glCompileShader.xml>`__

   :type shader: int
   :arg shader: Specifies the shader object to be compiled.


.. function:: glCreateProgram():

   Creates a program object

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glCreateProgram.xml>`__

   :rtype: int
   :return: The new program or zero if an error occurs.


.. function:: glCreateShader(shaderType):

   Creates a shader object.

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glCreateShader.xml>`__

   :type shaderType: Specifies the type of shader to be created.
      Must be one of ``GL_VERTEX_SHADER``,
      ``GL_TESS_CONTROL_SHADER``,
      ``GL_TESS_EVALUATION_SHADER``,
      ``GL_GEOMETRY_SHADER``,
      or ``GL_FRAGMENT_SHADER``.
   :arg shaderType:
   :rtype: int
   :return: 0 if an error occurs.


.. function:: glDeleteProgram(program):

   Deletes a program object.

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glDeleteProgram.xml>`__

   :type program: int
   :arg program: Specifies the program object to be deleted.


.. function:: glDeleteShader(shader):

   Deletes a shader object.

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glDeleteShader.xml>`__

   :type shader: int
   :arg shader: Specifies the shader object to be deleted.


.. function:: glDetachShader(program, shader):

   Detaches a shader object from a program object to which it is attached.

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glDetachShader.xml>`__

   :type program: int
   :arg program: Specifies the program object from which to detach the shader object.
   :type shader: int
   :arg shader: pecifies the program object from which to detach the shader object.


.. function:: glGetAttachedShaders(program, maxCount, count, shaders):

   Returns the handles of the shader objects attached to a program object.

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glGetAttachedShaders.xml>`__

   :type program: int
   :arg program: Specifies the program object to be queried.
   :type maxCount: int
   :arg maxCount: Specifies the size of the array for storing the returned object names.
   :type count: :class:`bgl.Buffer` int buffer.
   :arg count: Returns the number of names actually returned in objects.
   :type shaders: :class:`bgl.Buffer` int buffer.
   :arg shaders: Specifies an array that is used to return the names of attached shader objects.


.. function:: glGetProgramInfoLog(program, maxLength, length, infoLog):

   Returns the information log for a program object.

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glGetProgramInfoLog.xml>`__

   :type program: int
   :arg program: Specifies the program object whose information log is to be queried.
   :type maxLength: int
   :arg maxLength: Specifies the size of the character buffer for storing the returned information log.
   :type length: :class:`bgl.Buffer` int buffer.
   :arg length: Returns the length of the string returned in **infoLog** (excluding the null terminator).
   :type infoLog: :class:`bgl.Buffer` char buffer.
   :arg infoLog: Specifies an array of characters that is used to return the information log.


.. function:: glGetShaderInfoLog(program, maxLength, length, infoLog):

   Returns the information log for a shader object.

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glGetShaderInfoLog.xml>`__

   :type shader: int
   :arg shader: Specifies the shader object whose information log is to be queried.
   :type maxLength: int
   :arg maxLength: Specifies the size of the character buffer for storing the returned information log.
   :type length: :class:`bgl.Buffer` int buffer.
   :arg length: Returns the length of the string returned in **infoLog** (excluding the null terminator).
   :type infoLog: :class:`bgl.Buffer` char buffer.
   :arg infoLog: Specifies an array of characters that is used to return the information log.


.. function:: glGetProgramiv(program, pname, params):

   Returns a parameter from a program object.

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glGetProgram.xml>`__

   :type program: int
   :arg program: Specifies the program object to be queried.
   :type pname: int
   :arg pname: Specifies the object parameter.
   :type params: :class:`bgl.Buffer` int buffer.
   :arg params: Returns the requested object parameter.


.. function:: glIsShader(shader):

   Determines if a name corresponds to a shader object.

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glIsShader.xml>`__

   :type shader: int
   :arg shader: Specifies a potential shader object.


.. function:: glIsProgram(program):

   Determines if a name corresponds to a program object

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glIsProgram.xml>`__

   :type program: int
   :arg program: Specifies a potential program object.


.. function:: glGetShaderSource(shader, bufSize, length, source):

   Returns the source code string from a shader object

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glGetShaderSource.xml>`__

   :type shader: int
   :arg shader: Specifies the shader object to be queried.
   :type bufSize: int
   :arg bufSize: Specifies the size of the character buffer for storing the returned source code string.
   :type length: :class:`bgl.Buffer` int buffer.
   :arg length: Returns the length of the string returned in source (excluding the null terminator).
   :type source: :class:`bgl.Buffer` char.
   :arg source: Specifies an array of characters that is used to return the source code string.


.. function:: glShaderSource(shader, shader_string):

   Replaces the source code in a shader object.

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man/html/glShaderSource.xhtml>`__

   :type shader: int
   :arg shader: Specifies the handle of the shader object whose source code is to be replaced.
   :type shader_string: string
   :arg shader_string: The shader string.


.. class:: Buffer

   The Buffer object is simply a block of memory that is delineated and initialized by the
   user. Many OpenGL functions return data to a C-style pointer, however, because this
   is not possible in python the Buffer object can be used to this end. Wherever pointer
   notation is used in the OpenGL functions the Buffer object can be used in it's bgl
   wrapper. In some instances the Buffer object will need to be initialized with the template
   parameter, while in other instances the user will want to create just a blank buffer
   which will be zeroed by default.

   .. code-block:: python

      import bgl

      myByteBuffer = bgl.Buffer(bgl.GL_BYTE, [32, 32])
      bgl.glGetPolygonStipple(myByteBuffer)

      print(myByteBuffer.dimensions)
      print(myByteBuffer.to_list())

      sliceBuffer = myByteBuffer[0:16]
      print(sliceBuffer)

   .. attribute:: dimensions

      The number of dimensions of the Buffer.

   .. method:: to_list()

      The contents of the Buffer as a python list.

   .. method:: __init__(type, dimensions, template = None):

      This will create a new Buffer object for use with other bgl OpenGL commands.
      Only the type of argument to store in the buffer and the dimensions of the buffer
      are necessary. Buffers are zeroed by default unless a template is supplied, in
      which case the buffer is initialized to the template.

      :type type: int
      :arg type: The format to store data in. The type should be one of
         GL_BYTE, GL_SHORT, GL_INT, or GL_FLOAT.
      :type dimensions: An int or sequence object specifying the dimensions of the buffer.
      :arg dimensions: If the dimensions are specified as an int a linear array will
         be created for the buffer. If a sequence is passed for the dimensions, the buffer
         becomes n-Dimensional, where n is equal to the number of parameters passed in the
         sequence. Example: [256,2] is a two- dimensional buffer while [256,256,4] creates
         a three- dimensional buffer. You can think of each additional dimension as a sub-item
         of the dimension to the left. i.e. [10,2] is a 10 element array each with 2 sub-items.
         [(0,0), (0,1), (1,0), (1,1), (2,0), ...] etc.
      :type template: A python sequence object (optional)
      :arg template: A sequence of matching dimensions which will be used to initialize
         the Buffer. If a template is not passed in all fields will be initialized to 0.
      :rtype: Buffer object
      :return: The newly created buffer as a PyObject.
