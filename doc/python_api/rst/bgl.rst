
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
   See :class:`Image.gl_load` and :class:`Image.gl_free`,
   for example.


.. function:: glBindTexture(target, texture):

   Bind a named texture to a texturing target

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glBindTexture.xml>`__

   :type target: Enumerated constant
   :arg target: Specifies the target to which the texture is bound.
   :type texture: unsigned int
   :arg texture: Specifies the name of a texture.


.. function:: glBlendFunc(sfactor, dfactor):

   Specify pixel arithmetic

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glBlendFunc.xml>`__

   :type sfactor: Enumerated constant
   :arg sfactor: Specifies how the red, green, blue, and alpha source blending factors are
      computed.
   :type dfactor: Enumerated constant
   :arg dfactor: Specifies how the red, green, blue, and alpha destination
      blending factors are computed.


.. function:: glClear(mask):

   Clear buffers to preset values

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glClear.xml>`__

   :type mask: Enumerated constant(s)
   :arg mask: Bitwise OR of masks that indicate the buffers to be cleared.


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


.. function:: glCopyTexImage2D(target, level, internalformat, x, y, width, height, border):

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

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glGetMap.xml>`__

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


.. function:: glIsEnabled(cap):

   Test whether a capability is enabled

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glIsEnabled.xml>`__

   :type cap: Enumerated constant
   :arg cap: Specifies a constant representing a GL capability.


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


.. function:: glLineWidth(width):

   Specify the width of rasterized lines.

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glLineWidth.xml>`__

   :type width: float
   :arg width: Specifies the width of rasterized lines. The initial value is 1.


.. function:: glLoadMatrix (m):

   B{glLoadMatrixd, glLoadMatixf}

   Replace the current matrix with the specified matrix

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glLoadMatrix.xml>`__

   :type m: :class:`bgl.Buffer` object. Depends on function prototype.
   :arg m: Specifies a pointer to 16 consecutive values, which are used as the elements
      of a 4x4 column-major matrix.


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


.. function:: glMultMatrix (m):

   B{glMultMatrixd, glMultMatrixf}

   Multiply the current matrix with the specified matrix

   .. seealso:: `OpenGL Docs <https://www.opengl.org/sdk/docs/man2/xhtml/glMultMatrix.xml>`__

   :type m: :class:`bgl.Buffer` object. Depends on function prototype.
   :arg m: Points to 16 consecutive values that are used as the elements of a 4x4 column
      major matrix.


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
