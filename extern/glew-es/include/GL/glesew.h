/*
** The OpenGL Extension Wrangler Library
** Copyright (C) 2002-2008, Milan Ikits <milan ikits[]ieee org>
** Copyright (C) 2002-2008, Marcelo E. Magallon <mmagallo[]debian org>
** Copyright (C) 2002, Lev Povalahev
** All rights reserved.
** 
** Redistribution and use in source and binary forms, with or without 
** modification, are permitted provided that the following conditions are met:
** 
** * Redistributions of source code must retain the above copyright notice, 
**   this list of conditions and the following disclaimer.
** * Redistributions in binary form must reproduce the above copyright notice, 
**   this list of conditions and the following disclaimer in the documentation 
**   and/or other materials provided with the distribution.
** * The name of the author may be used to endorse or promote products 
**   derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" 
** AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
** IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
** ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE 
** LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
** CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
** SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
** INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
** CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
** ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
** THE POSSIBILITY OF SUCH DAMAGE.
*/


/*
** Copyright (c) 2008-2009 The Khronos Group Inc.
**
** Permission is hereby granted, free of charge, to any person obtaining a
** copy of this software and/or associated documentation files (the
** "Materials"), to deal in the Materials without restriction, including
** without limitation the rights to use, copy, modify, merge, publish,
** distribute, sublicense, and/or sell copies of the Materials, and to
** permit persons to whom the Materials are furnished to do so, subject to
** the following conditions:
**
** The above copyright notice and this permission notice shall be included
** in all copies or substantial portions of the Materials.
**
** THE MATERIALS ARE PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
** EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
** MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
** IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
** CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
** TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
** MATERIALS OR THE USE OR OTHER DEALINGS IN THE MATERIALS.
*/

/* Copyright © 2011 Linaro Limited
 */

#ifndef __glesew_h__
#define __glesew_h__
#define __GLESEW_H__

#if !defined(__glew_h__) || !defined(__GLEW_H__)
#error glesew.h included instead of glew.h
#endif

// NOTE jwilkins: changing versions from 'ifdef' to 'if' requires setting defaults
#ifndef GL_ES_VERSION_1_0 // XXX
#define GL_ES_VERSION_1_0 1 // XXX
#endif // XXX

// NOTE jwilkins: changing versions from 'ifdef' to 'if' requires setting defaults
#ifndef GL_ES_VERSION_CL_1_1 // XXX
#define GL_ES_VERSION_CL_1_1 1 // XXX
#endif // XXX

// NOTE jwilkins: changing versions from 'ifdef' to 'if' requires setting defaults
#ifndef GL_ES_VERSION_CM_1_1 // XXX
#define GL_ES_VERSION_CM_1_1 1 // XXX
#endif // XXX

// NOTE jwilkins: changing versions from 'ifdef' to 'if' requires setting defaults
#ifndef GL_ES_VERSION_2_0 // XXX
#define GL_ES_VERSION_2_0 1 // XXX
#endif // XXX

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned int GLenum;
typedef unsigned char GLboolean;
typedef unsigned int GLbitfield;
typedef khronos_int8_t   GLbyte;
typedef short GLshort;
typedef int GLint;
typedef int GLsizei;
typedef khronos_uint8_t  GLubyte;
typedef unsigned short GLushort;
typedef unsigned int GLuint;
typedef khronos_float_t  GLfloat;
typedef khronos_float_t  GLclampf;
typedef void GLvoid;
typedef int GLintptrARB;
typedef int GLsizeiptrARB;
typedef khronos_int32_t  GLfixed;
typedef int GLclampx;
/* Internal convenience typedefs */
typedef void (*_GLfuncptr)();

// NOTE jwilkins: had to add these
#if defined(_MSC_VER) && _MSC_VER < 1400
typedef __int64 GLint64EXT;
typedef unsigned __int64 GLuint64EXT;
#elif defined(_MSC_VER) || defined(__BORLANDC__)
typedef signed long long GLint64EXT;
typedef unsigned long long GLuint64EXT;
#else
#  if defined(__MINGW32__) || defined(__CYGWIN__)
#include <inttypes.h>
#  endif
typedef int64_t GLint64EXT;
typedef uint64_t GLuint64EXT;
#endif
typedef GLint64EXT  GLint64;
typedef GLuint64EXT GLuint64;
typedef struct __GLsync *GLsync;



/********************* GL_ES_VERSION_1_0 functions common with OpenGL 1.1 *************************/


/* Extensions */
//#define GL_OES_VERSION_1_0                   1
//#define GL_OES_read_format                 1
//#define GL_OES_compressed_paletted_texture 1

/* ClearBufferMask */
#define GL_DEPTH_BUFFER_BIT               0x00000100
#define GL_STENCIL_BUFFER_BIT             0x00000400
#define GL_COLOR_BUFFER_BIT               0x00004000

/* Boolean */
#define GL_FALSE                          0
#define GL_TRUE                           1

/* BeginMode */
#define GL_POINTS                         0x0000
#define GL_LINES                          0x0001
#define GL_LINE_LOOP                      0x0002
#define GL_LINE_STRIP                     0x0003
#define GL_TRIANGLES                      0x0004
#define GL_TRIANGLE_STRIP                 0x0005
#define GL_TRIANGLE_FAN                   0x0006

/* AlphaFunction */
#define GL_NEVER                          0x0200
#define GL_LESS                           0x0201
#define GL_EQUAL                          0x0202
#define GL_LEQUAL                         0x0203
#define GL_GREATER                        0x0204
#define GL_NOTEQUAL                       0x0205
#define GL_GEQUAL                         0x0206
#define GL_ALWAYS                         0x0207

/* BlendingFactorDest */
#define GL_ZERO                           0
#define GL_ONE                            1
#define GL_SRC_COLOR                      0x0300
#define GL_ONE_MINUS_SRC_COLOR            0x0301
#define GL_SRC_ALPHA                      0x0302
#define GL_ONE_MINUS_SRC_ALPHA            0x0303
#define GL_DST_ALPHA                      0x0304
#define GL_ONE_MINUS_DST_ALPHA            0x0305

/* BlendingFactorSrc */
/*      GL_ZERO */
/*      GL_ONE */
#define GL_DST_COLOR                      0x0306
#define GL_ONE_MINUS_DST_COLOR            0x0307
#define GL_SRC_ALPHA_SATURATE             0x0308
/*      GL_SRC_ALPHA */
/*      GL_ONE_MINUS_SRC_ALPHA */
/*      GL_DST_ALPHA */
/*      GL_ONE_MINUS_DST_ALPHA */

/* ColorMaterialFace */
/*      GL_FRONT_AND_BACK */

/* ColorMaterialParameter */
/*      GL_AMBIENT_AND_DIFFUSE */

/* ColorPointerType */
/*      GL_UNSIGNED_BYTE */
/*      GL_FLOAT */
/*      GL_FIXED */

/* CullFaceMode */
#define GL_FRONT                          0x0404
#define GL_BACK                           0x0405
#define GL_FRONT_AND_BACK                 0x0408

/* DepthFunction */
/*      GL_NEVER */
/*      GL_LESS */
/*      GL_EQUAL */
/*      GL_LEQUAL */
/*      GL_GREATER */
/*      GL_NOTEQUAL */
/*      GL_GEQUAL */
/*      GL_ALWAYS */

/* EnableCap */
#define GL_FOG                            0x0B60
#define GL_LIGHTING                       0x0B50
#define GL_TEXTURE_2D                     0x0DE1
#define GL_CULL_FACE                      0x0B44
#define GL_ALPHA_TEST                     0x0BC0
#define GL_BLEND                          0x0BE2
#define GL_COLOR_LOGIC_OP                 0x0BF2
#define GL_DITHER                         0x0BD0
#define GL_STENCIL_TEST                   0x0B90
#define GL_DEPTH_TEST                     0x0B71
/*      GL_LIGHT0 */
/*      GL_LIGHT1 */
/*      GL_LIGHT2 */
/*      GL_LIGHT3 */
/*      GL_LIGHT4 */
/*      GL_LIGHT5 */
/*      GL_LIGHT6 */
/*      GL_LIGHT7 */
#define GL_POINT_SMOOTH                   0x0B10
#define GL_LINE_SMOOTH                    0x0B20
#define GL_SCISSOR_TEST                   0x0C11
#define GL_COLOR_MATERIAL                 0x0B57
#define GL_NORMALIZE                      0x0BA1
#define GL_RESCALE_NORMAL                 0x803A
#define GL_POLYGON_OFFSET_FILL            0x8037
#define GL_VERTEX_ARRAY                   0x8074
#define GL_NORMAL_ARRAY                   0x8075
#define GL_COLOR_ARRAY                    0x8076
#define GL_TEXTURE_COORD_ARRAY            0x8078
#define GL_MULTISAMPLE                    0x809D
#define GL_SAMPLE_ALPHA_TO_COVERAGE       0x809E
#define GL_SAMPLE_ALPHA_TO_ONE            0x809F
#define GL_SAMPLE_COVERAGE                0x80A0

/* ErrorCode */
#define GL_NO_ERROR                       0
#define GL_INVALID_ENUM                   0x0500
#define GL_INVALID_VALUE                  0x0501
#define GL_INVALID_OPERATION              0x0502
#define GL_STACK_OVERFLOW                 0x0503
#define GL_STACK_UNDERFLOW                0x0504
#define GL_OUT_OF_MEMORY                  0x0505

/* FogMode */
/*      GL_LINEAR */
#define GL_EXP                            0x0800
#define GL_EXP2                           0x0801

/* FogParameter */
#define GL_FOG_DENSITY                    0x0B62
#define GL_FOG_START                      0x0B63
#define GL_FOG_END                        0x0B64
#define GL_FOG_MODE                       0x0B65
#define GL_FOG_COLOR                      0x0B66

/* FrontFaceDirection */
#define GL_CW                             0x0900
#define GL_CCW                            0x0901

/* GetPName */
#define GL_SMOOTH_POINT_SIZE_RANGE        0x0B12
#define GL_SMOOTH_LINE_WIDTH_RANGE        0x0B22
#define GL_ALIASED_POINT_SIZE_RANGE       0x846D
#define GL_ALIASED_LINE_WIDTH_RANGE       0x846E
#define GL_IMPLEMENTATION_COLOR_READ_TYPE_OES 0x8B9A
#define GL_IMPLEMENTATION_COLOR_READ_FORMAT_OES 0x8B9B
#define GL_MAX_LIGHTS                     0x0D31
#define GL_MAX_TEXTURE_SIZE               0x0D33
#define GL_MAX_MODELVIEW_STACK_DEPTH      0x0D36
#define GL_MAX_PROJECTION_STACK_DEPTH     0x0D38
#define GL_MAX_TEXTURE_STACK_DEPTH        0x0D39
#define GL_MAX_VIEWPORT_DIMS              0x0D3A
#define GL_MAX_ELEMENTS_VERTICES          0x80E8
#define GL_MAX_ELEMENTS_INDICES           0x80E9
#define GL_MAX_TEXTURE_UNITS              0x84E2
#define GL_NUM_COMPRESSED_TEXTURE_FORMATS 0x86A2
#define GL_COMPRESSED_TEXTURE_FORMATS     0x86A3
#define GL_SUBPIXEL_BITS                  0x0D50
#define GL_RED_BITS                       0x0D52
#define GL_GREEN_BITS                     0x0D53
#define GL_BLUE_BITS                      0x0D54
#define GL_ALPHA_BITS                     0x0D55
#define GL_DEPTH_BITS                     0x0D56
#define GL_STENCIL_BITS                   0x0D57

/* HintMode */
#define GL_DONT_CARE                      0x1100
#define GL_FASTEST                        0x1101
#define GL_NICEST                         0x1102

/* HintTarget */
#define GL_PERSPECTIVE_CORRECTION_HINT    0x0C50
#define GL_POINT_SMOOTH_HINT              0x0C51
#define GL_LINE_SMOOTH_HINT               0x0C52
#define GL_POLYGON_SMOOTH_HINT            0x0C53
#define GL_FOG_HINT                       0x0C54

/* LightModelParameter */
#define GL_LIGHT_MODEL_AMBIENT            0x0B53
#define GL_LIGHT_MODEL_TWO_SIDE           0x0B52

/* LightParameter */
#define GL_AMBIENT                        0x1200
#define GL_DIFFUSE                        0x1201
#define GL_SPECULAR                       0x1202
#define GL_POSITION                       0x1203
#define GL_SPOT_DIRECTION                 0x1204
#define GL_SPOT_EXPONENT                  0x1205
#define GL_SPOT_CUTOFF                    0x1206
#define GL_CONSTANT_ATTENUATION           0x1207
#define GL_LINEAR_ATTENUATION             0x1208
#define GL_QUADRATIC_ATTENUATION          0x1209

/* DataType */
#define GL_BYTE                           0x1400
#define GL_UNSIGNED_BYTE                  0x1401
#define GL_SHORT                          0x1402
#define GL_UNSIGNED_SHORT                 0x1403
#define GL_FLOAT                          0x1406
#define GL_FIXED                          0x140C

/* LogicOp */
#define GL_CLEAR                          0x1500
#define GL_AND                            0x1501
#define GL_AND_REVERSE                    0x1502
#define GL_COPY                           0x1503
#define GL_AND_INVERTED                   0x1504
#define GL_NOOP                           0x1505
#define GL_XOR                            0x1506
#define GL_OR                             0x1507
#define GL_NOR                            0x1508
#define GL_EQUIV                          0x1509
#define GL_INVERT                         0x150A
#define GL_OR_REVERSE                     0x150B
#define GL_COPY_INVERTED                  0x150C
#define GL_OR_INVERTED                    0x150D
#define GL_NAND                           0x150E
#define GL_SET                            0x150F

/* MaterialFace */
/*      GL_FRONT_AND_BACK */

/* MaterialParameter */
#define GL_EMISSION                       0x1600
#define GL_SHININESS                      0x1601
#define GL_AMBIENT_AND_DIFFUSE            0x1602
/*      GL_AMBIENT */
/*      GL_DIFFUSE */
/*      GL_SPECULAR */

/* MatrixMode */
#define GL_MODELVIEW                      0x1700
#define GL_PROJECTION                     0x1701
#define GL_TEXTURE                        0x1702

/* NormalPointerType */
/*      GL_BYTE */
/*      GL_SHORT */
/*      GL_FLOAT */
/*      GL_FIXED */

/* PixelFormat */
#define GL_ALPHA                          0x1906
#define GL_RGB                            0x1907
#define GL_RGBA                           0x1908
#define GL_LUMINANCE                      0x1909
#define GL_LUMINANCE_ALPHA                0x190A

/* PixelStoreParameter */
#define GL_UNPACK_ALIGNMENT               0x0CF5
#define GL_PACK_ALIGNMENT                 0x0D05

/* PixelType */
/*      GL_UNSIGNED_BYTE */
#define GL_UNSIGNED_SHORT_4_4_4_4         0x8033
#define GL_UNSIGNED_SHORT_5_5_5_1         0x8034
#define GL_UNSIGNED_SHORT_5_6_5           0x8363

/* ShadingModel */
#define GL_FLAT                           0x1D00
#define GL_SMOOTH                         0x1D01

/* StencilFunction */
/*      GL_NEVER */
/*      GL_LESS */
/*      GL_EQUAL */
/*      GL_LEQUAL */
/*      GL_GREATER */
/*      GL_NOTEQUAL */
/*      GL_GEQUAL */
/*      GL_ALWAYS */

/* StencilOp */
/*      GL_ZERO */
#define GL_KEEP                           0x1E00
#define GL_REPLACE                        0x1E01
#define GL_INCR                           0x1E02
#define GL_DECR                           0x1E03
/*      GL_INVERT */

/* StringName */
#define GL_VENDOR                         0x1F00
#define GL_RENDERER                       0x1F01
#define GL_VERSION                        0x1F02
#define GL_EXTENSIONS                     0x1F03

/* TexCoordPointerType */
/*      GL_SHORT */
/*      GL_FLOAT */
/*      GL_FIXED */
/*      GL_BYTE */

/* TextureEnvMode */
#define GL_MODULATE                       0x2100
#define GL_DECAL                          0x2101
/*      GL_BLEND */
#define GL_ADD                            0x0104
/*      GL_REPLACE */

/* TextureEnvParameter */
#define GL_TEXTURE_ENV_MODE               0x2200
#define GL_TEXTURE_ENV_COLOR              0x2201

/* TextureEnvTarget */
#define GL_TEXTURE_ENV                    0x2300

/* TextureMagFilter */
#define GL_NEAREST                        0x2600
#define GL_LINEAR                         0x2601

/* TextureMinFilter */
/*      GL_NEAREST */
/*      GL_LINEAR */
#define GL_NEAREST_MIPMAP_NEAREST         0x2700
#define GL_LINEAR_MIPMAP_NEAREST          0x2701
#define GL_NEAREST_MIPMAP_LINEAR          0x2702
#define GL_LINEAR_MIPMAP_LINEAR           0x2703

/* TextureParameterName */
#define GL_TEXTURE_MAG_FILTER             0x2800
#define GL_TEXTURE_MIN_FILTER             0x2801
#define GL_TEXTURE_WRAP_S                 0x2802
#define GL_TEXTURE_WRAP_T                 0x2803

/* TextureTarget */
/*      GL_TEXTURE_2D */

/* TextureUnit */
#define GL_TEXTURE0                       0x84C0
#define GL_TEXTURE1                       0x84C1
#define GL_TEXTURE2                       0x84C2
#define GL_TEXTURE3                       0x84C3
#define GL_TEXTURE4                       0x84C4
#define GL_TEXTURE5                       0x84C5
#define GL_TEXTURE6                       0x84C6
#define GL_TEXTURE7                       0x84C7
#define GL_TEXTURE8                       0x84C8
#define GL_TEXTURE9                       0x84C9
#define GL_TEXTURE10                      0x84CA
#define GL_TEXTURE11                      0x84CB
#define GL_TEXTURE12                      0x84CC
#define GL_TEXTURE13                      0x84CD
#define GL_TEXTURE14                      0x84CE
#define GL_TEXTURE15                      0x84CF
#define GL_TEXTURE16                      0x84D0
#define GL_TEXTURE17                      0x84D1
#define GL_TEXTURE18                      0x84D2
#define GL_TEXTURE19                      0x84D3
#define GL_TEXTURE20                      0x84D4
#define GL_TEXTURE21                      0x84D5
#define GL_TEXTURE22                      0x84D6
#define GL_TEXTURE23                      0x84D7
#define GL_TEXTURE24                      0x84D8
#define GL_TEXTURE25                      0x84D9
#define GL_TEXTURE26                      0x84DA
#define GL_TEXTURE27                      0x84DB
#define GL_TEXTURE28                      0x84DC
#define GL_TEXTURE29                      0x84DD
#define GL_TEXTURE30                      0x84DE
#define GL_TEXTURE31                      0x84DF

/* TextureWrapMode */
#define GL_REPEAT                         0x2901
#define GL_CLAMP_TO_EDGE                  0x812F

/* PixelInternalFormat */
#define GL_PALETTE4_RGB8_OES              0x8B90
#define GL_PALETTE4_RGBA8_OES             0x8B91
#define GL_PALETTE4_R5_G6_B5_OES          0x8B92
#define GL_PALETTE4_RGBA4_OES             0x8B93
#define GL_PALETTE4_RGB5_A1_OES           0x8B94
#define GL_PALETTE8_RGB8_OES              0x8B95
#define GL_PALETTE8_RGBA8_OES             0x8B96
#define GL_PALETTE8_R5_G6_B5_OES          0x8B97
#define GL_PALETTE8_RGBA4_OES             0x8B98
#define GL_PALETTE8_RGB5_A1_OES           0x8B99

/* VertexPointerType */
/*      GL_SHORT */
/*      GL_FLOAT */
/*      GL_FIXED */
/*      GL_BYTE */

/* LightName */
#define GL_LIGHT0                         0x4000
#define GL_LIGHT1                         0x4001
#define GL_LIGHT2                         0x4002
#define GL_LIGHT3                         0x4003
#define GL_LIGHT4                         0x4004
#define GL_LIGHT5                         0x4005
#define GL_LIGHT6                         0x4006
#define GL_LIGHT7                         0x4007


/*************************************************************/

#if !GL_ES_VERSION_2_0 // NOTE jwilkins: not in all versions of ES
GLAPI void GLAPIENTRY glAlphaFunc (GLenum func, GLclampf ref);
#endif // XXX
GLAPI void GLAPIENTRY glBindTexture (GLenum target, GLuint texture);
GLAPI void GLAPIENTRY glBlendFunc (GLenum sfactor, GLenum dfactor);
GLAPI void GLAPIENTRY glClear (GLbitfield mask);
GLAPI void GLAPIENTRY glClearColor (GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha);
GLAPI void GLAPIENTRY glClearStencil (GLint s);
#if !GL_ES_VERSION_2_0 // NOTE jwilkins: not in all versions of ES
GLAPI void GLAPIENTRY glColor4f (GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
#endif
GLAPI void GLAPIENTRY glColorMask (GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha);
#if !GL_ES_VERSION_2_0 // NOTE jwilkins: not in all versions of ES
GLAPI void GLAPIENTRY glColorPointer (GLint size, GLenum type, GLsizei stride, const GLvoid *pointer);
#endif
GLAPI void GLAPIENTRY glCopyTexImage2D (GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border);
GLAPI void GLAPIENTRY glCopyTexSubImage2D (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height);
GLAPI void GLAPIENTRY glCullFace (GLenum mode);
GLAPI void GLAPIENTRY glDeleteTextures (GLsizei n, const GLuint *textures);
GLAPI void GLAPIENTRY glDepthFunc (GLenum func);
GLAPI void GLAPIENTRY glDepthMask (GLboolean flag);
GLAPI void GLAPIENTRY glDisable (GLenum cap);
#if !GL_ES_VERSION_2_0 // NOTE jwilkins: not in all versions of ES
GLAPI void GLAPIENTRY glDisableClientState (GLenum array);
#endif
GLAPI void GLAPIENTRY glDrawArrays (GLenum mode, GLint first, GLsizei count);
GLAPI void GLAPIENTRY glDrawElements (GLenum mode, GLsizei count, GLenum type, const GLvoid *indices);
GLAPI void GLAPIENTRY glEnable (GLenum cap);
#if !GL_ES_VERSION_2_0 // NOTE jwilkins: not in all versions of ES
GLAPI void GLAPIENTRY glEnableClientState (GLenum array);
#endif 
GLAPI void GLAPIENTRY glFinish (void);
GLAPI void GLAPIENTRY glFlush (void);
#if !GL_ES_VERSION_2_0 // NOTE jwilkins: not in all versions of ES
GLAPI void GLAPIENTRY glFogf (GLenum pname, GLfloat param);
GLAPI void GLAPIENTRY glFogfv (GLenum pname, const GLfloat *params);
#endif
GLAPI void GLAPIENTRY glFrontFace (GLenum mode);
GLAPI void GLAPIENTRY glGenTextures (GLsizei n, GLuint *textures);
GLAPI GLenum GLAPIENTRY glGetError (void);
GLAPI void GLAPIENTRY glGetIntegerv (GLenum pname, GLint *params);
GLAPI const GLubyte * GLAPIENTRY glGetString (GLenum name);
GLAPI void GLAPIENTRY glHint (GLenum target, GLenum mode);
#if !GL_ES_VERSION_2_0 // NOTE jwilkins: not in all versions of ES
GLAPI void GLAPIENTRY glLightModelf (GLenum pname, GLfloat param);
GLAPI void GLAPIENTRY glLightModelfv (GLenum pname, const GLfloat *params);
GLAPI void GLAPIENTRY glLightf (GLenum light, GLenum pname, GLfloat param);
GLAPI void GLAPIENTRY glLightfv (GLenum light, GLenum pname, const GLfloat *params);
#endif
GLAPI void GLAPIENTRY glLineWidth (GLfloat width);
#if !GL_ES_VERSION_2_0 // NOTE jwilkins: not in all versions of ES
GLAPI void GLAPIENTRY glLoadIdentity (void);
GLAPI void GLAPIENTRY glLoadMatrixf (const GLfloat *m);
GLAPI void GLAPIENTRY glLogicOp (GLenum opcode);
GLAPI void GLAPIENTRY glMaterialf (GLenum face, GLenum pname, GLfloat param);
GLAPI void GLAPIENTRY glMaterialfv (GLenum face, GLenum pname, const GLfloat *params);
GLAPI void GLAPIENTRY glMatrixMode (GLenum mode);
GLAPI void GLAPIENTRY glMultMatrixf (const GLfloat *m);
GLAPI void GLAPIENTRY glMultiTexCoord4f (GLenum target, GLfloat s, GLfloat t, GLfloat r, GLfloat q);
GLAPI void GLAPIENTRY glNormal3f (GLfloat nx, GLfloat ny, GLfloat nz);
GLAPI void GLAPIENTRY glNormalPointer (GLenum type, GLsizei stride, const GLvoid *pointer);
#endif
GLAPI void GLAPIENTRY glPixelStorei (GLenum pname, GLint param);
#if !GL_ES_VERSION_2_0 // NOTE jwilkins: not in all versions of ES
GLAPI void GLAPIENTRY glPointSize (GLfloat size);
#endif // XXX
GLAPI void GLAPIENTRY glPolygonOffset (GLfloat factor, GLfloat units);
#if !GL_ES_VERSION_2_0 // NOTE jwilkins: not in all versions of ES
GLAPI void GLAPIENTRY glPopMatrix (void);
GLAPI void GLAPIENTRY glPushMatrix (void);
#endif
GLAPI void GLAPIENTRY glReadPixels (GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLvoid *pixels);
#if !GL_ES_VERSION_2_0 // NOTE jwilkins: not in all versions of ES
GLAPI void GLAPIENTRY glRotatef (GLfloat angle, GLfloat x, GLfloat y, GLfloat z);
GLAPI void GLAPIENTRY glScalef (GLfloat x, GLfloat y, GLfloat z);
#endif
GLAPI void GLAPIENTRY glScissor (GLint x, GLint y, GLsizei width, GLsizei height);
#if !GL_ES_VERSION_2_0 // NOTE jwilkins: not in all versions of ES
GLAPI void GLAPIENTRY glShadeModel (GLenum mode);
#endif // XXX
GLAPI void GLAPIENTRY glStencilFunc (GLenum func, GLint ref, GLuint mask);
GLAPI void GLAPIENTRY glStencilMask (GLuint mask);
GLAPI void GLAPIENTRY glStencilOp (GLenum fail, GLenum zfail, GLenum zpass);
#if !GL_ES_VERSION_2_0 // NOTE jwilkins: not in all versions of ES
GLAPI void GLAPIENTRY glTexCoordPointer (GLint size, GLenum type, GLsizei stride, const GLvoid *pointer);
GLAPI void GLAPIENTRY glTexEnvf (GLenum target, GLenum pname, GLfloat param);
GLAPI void GLAPIENTRY glTexEnvfv (GLenum target, GLenum pname, const GLfloat *params);
#endif
GLAPI void GLAPIENTRY glTexImage2D (GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *pixels);
GLAPI void GLAPIENTRY glTexParameterf (GLenum target, GLenum pname, GLfloat param);
GLAPI void GLAPIENTRY glTexSubImage2D (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels);
#if !GL_ES_VERSION_2_0 // NOTE jwilkins: not in all versions of ES
GLAPI void GLAPIENTRY glTranslatef (GLfloat x, GLfloat y, GLfloat z);
GLAPI void GLAPIENTRY glVertexPointer (GLint size, GLenum type, GLsizei stride, const GLvoid *pointer);
#endif
GLAPI void GLAPIENTRY glViewport (GLint x, GLint y, GLsizei width, GLsizei height);

/* --------------------------- GL_ES_VERSION_1_0 --------------------------- */

#if GL_ES_VERSION_1_0 // NOTE jwilkins: should be if not ifdef
#define GL_ES_VERSION_1_0 1

typedef void (GLAPIENTRY * PFNGLACTIVETEXTUREPROC) (GLenum texture);
typedef void (GLAPIENTRY * PFNGLALPHAFUNCXPROC) (GLenum func, GLclampx ref);
typedef void (GLAPIENTRY * PFNGLCLEARCOLORXPROC) (GLclampx red, GLclampx green, GLclampx blue, GLclampx alpha);
typedef void (GLAPIENTRY * PFNGLCLEARDEPTHFPROC) (GLclampf depth);
typedef void (GLAPIENTRY * PFNGLCLEARDEPTHXPROC) (GLclampx depth);
typedef void (GLAPIENTRY * PFNGLCLIENTACTIVETEXTUREPROC) (GLenum texture);
typedef void (GLAPIENTRY * PFNGLCOLOR4XPROC) (GLfixed red, GLfixed green, GLfixed blue, GLfixed alpha);
typedef void (GLAPIENTRY * PFNGLCOMPRESSEDTEXIMAGE2DPROC) (GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize, const GLvoid *data);
typedef void  (GLAPIENTRY * PFNGLCOMPRESSEDTEXSUBIMAGE2DPROC) (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const GLvoid *data);
typedef void (GLAPIENTRY * PFNGLDEPTHRANGEFPROC) (GLclampf zNear, GLclampf zFar);
typedef void (GLAPIENTRY * PFNGLDEPTHRANGEXPROC) (GLclampx zNear, GLclampx zFar);
typedef void (GLAPIENTRY * PFNGLFOGXPROC) (GLenum pname, GLfixed param);
typedef void (GLAPIENTRY * PFNGLFOGXVPROC) (GLenum pname, const GLfixed *params);
typedef void (GLAPIENTRY * PFNGLFRUSTUMFPROC) (GLfloat left, GLfloat right, GLfloat bottom, GLfloat top, GLfloat zNear, GLfloat zFar);
typedef void (GLAPIENTRY * PFNGLFRUSTUMXPROC) (GLfixed left, GLfixed right, GLfixed bottom, GLfixed top, GLfixed zNear, GLfixed zFar);
typedef void (GLAPIENTRY * PFNGLLIGHTMODELXPROC) (GLenum pname, GLfixed params);
typedef void (GLAPIENTRY * PFNGLLIGHTMODELXVPROC) (GLenum pname, const GLfixed *params);
typedef void (GLAPIENTRY * PFNGLLIGHTXPROC) (GLenum light, GLenum pname, GLfixed param);
typedef void (GLAPIENTRY * PFNGLLIGHTXVPROC) (GLenum light, GLenum pname, const GLfixed *params);
typedef void (GLAPIENTRY * PFNGLLINEWIDTHXPROC) (GLfixed width);
typedef void (GLAPIENTRY * PFNGLLOADMATRIXXPROC) (const GLfixed *m);
typedef void (GLAPIENTRY * PFNGLMATERIALXPROC) (GLenum face, GLenum pname, GLfixed param);
typedef void (GLAPIENTRY * PFNGLMATERIALXVPROC) (GLenum face, GLenum pname, const GLfixed *params);
typedef void (GLAPIENTRY * PFNGLMULTMATRIXXPROC) (const GLfixed *m);
typedef void (GLAPIENTRY * PFNGLMULTITEXCOORD4FPROC) (GLenum target, GLfloat s, GLfloat t, GLfloat r, GLfloat q);
typedef void (GLAPIENTRY * PFNGLMULTITEXCOORD4XPROC) (GLenum target, GLfixed s, GLfixed t, GLfixed r, GLfixed q);
typedef void (GLAPIENTRY * PFNGLNORMAL3XPROC) (GLfixed nx, GLfixed ny, GLfixed nz);
typedef void (GLAPIENTRY * PFNGLORTHOFPROC) (GLfloat left, GLfloat right, GLfloat bottom, GLfloat top, GLfloat zNear, GLfloat zFar);
typedef void (GLAPIENTRY * PFNGLORTHOXPROC) (GLfixed left, GLfixed right, GLfixed bottom, GLfixed top, GLfixed zNear, GLfixed zFar);
typedef void (GLAPIENTRY * PFNGLPOINTSIZEXPROC) (GLfixed size);
typedef void (GLAPIENTRY * PFNGLPOLYGONOFFSETXPROC) (GLfixed factor, GLfixed units);
typedef void (GLAPIENTRY * PFNGLROTATEXPROC) (GLfixed angle, GLfixed x, GLfixed y, GLfixed z);
typedef void (GLAPIENTRY * PFNGLSAMPLECOVERAGEPROC) (GLclampf value, GLboolean invert);
typedef void (GLAPIENTRY * PFNGLSAMPLECOVERAGEXPROC) (GLclampx value, GLboolean invert);
typedef void (GLAPIENTRY * PFNGLSCALEXPROC) (GLfixed x, GLfixed y, GLfixed z);
typedef void (GLAPIENTRY * PFNGLTEXENVXPROC) (GLenum target, GLenum pname, GLfixed param);
typedef void (GLAPIENTRY * PFNGLTEXENVXVPROC) (GLenum target, GLenum pname, const GLfixed *params);
typedef void (GLAPIENTRY * PFNGLTEXPARAMETERXPROC) (GLenum target, GLenum pname, GLfixed param);
typedef void (GLAPIENTRY * PFNGLTRANSLATEXPROC) (GLfixed x, GLfixed y, GLfixed z);

#define glActiveTexture GLEW_GET_FUN(__glewActiveTexture)
#define glAlphaFuncx GLEW_GET_FUN(__glewAlphaFuncx)
#define glClearColorx GLEW_GET_FUN(__glewClearColorx)
#define glClearDepthf GLEW_GET_FUN(__glewClearDepthf)
#define glClearDepthx GLEW_GET_FUN(__glewClearDepthx)
#define glClientActiveTexture GLEW_GET_FUN(__glewClientActiveTexture)
#define glColor4x GLEW_GET_FUN(__glewColor4x)
#define glCompressedTexImage2D GLEW_GET_FUN(__glewCompressedTexImage2D)
#define glCompressedTexSubImage2D GLEW_GET_FUN(__glewCompressedTexSubImage2D)
#define glDepthRangef GLEW_GET_FUN(__glewDepthRangef)
#define glDepthRangex GLEW_GET_FUN(__glewDepthRangex)
#define glFogx GLEW_GET_FUN(__glewFogx)
#define glFogxv GLEW_GET_FUN(__glewFogxv)
#define glFrustumf GLEW_GET_FUN(__glewFrustumf)
#define glFrustumx GLEW_GET_FUN(__glewFrustumx)
#define glLightModelx GLEW_GET_FUN(__glewLightModelx)
#define glLightModelxv GLEW_GET_FUN(__glewLightModelxv)
#define glLightx GLEW_GET_FUN(__glewLightx)
#define glLightxv GLEW_GET_FUN(__glewLightxv)
#define glLineWidthx GLEW_GET_FUN(__glewLineWidthx)
#define glLoadMatrixx GLEW_GET_FUN(__glewLoadMatrixx)
#define glMaterialx GLEW_GET_FUN(__glewMaterialx)
#define glMaterialxv GLEW_GET_FUN(__glewMaterialxv)
#define glMultMatrixx GLEW_GET_FUN(__glewMultMatrixx)
#define glMultiTexCoord4f GLEW_GET_FUN(__glewMultiTexCoord4f)
#define glMultiTexCoord4x GLEW_GET_FUN(__glewMultiTexCoord4x)
#define glNormal3x GLEW_GET_FUN(__glewNormal3x)
#define glOrthof GLEW_GET_FUN(__glewOrthof)
#define glOrthox GLEW_GET_FUN(__glewOrthox)
#define glPointSizex GLEW_GET_FUN(__glewPointSizex)
#define glPolygonOffsetx GLEW_GET_FUN(__glewPolygonOffsetx)
#define glRotatex GLEW_GET_FUN(__glewRotatex)
#define glSampleCoverage GLEW_GET_FUN(__glewSampleCoverage)
#define glSampleCoveragex GLEW_GET_FUN(__glewSampleCoveragex)
#define glScalex GLEW_GET_FUN(__glewScalex)
#define glTexEnvx GLEW_GET_FUN(__glewTexEnvx)
#define glTexEnvxv GLEW_GET_FUN(__glewTexEnvxv)
#define glTexParameterx GLEW_GET_FUN(__glewTexParameterx)
#define glTranslatex GLEW_GET_FUN(__glewTranslatex)

#else // XXX
#define GL_ES_VERSION_1_0 0 // NOTE jwilkins: define version token
#endif /* !GL_ES_VERSION_1_0 */

#define GLEW_ES_VERSION_1_0 GLEW_GET_VAR(__GLEW_ES_VERSION_1_0) // NOTE jwilkins: always needs to be defined

/* -------------------------- GL_ES_VERSION_CL_1_1 ------------------------- */

#if GL_ES_VERSION_CL_1_1 // NOTE jwilkins: should be if not ifdef
#define GL_ES_VERSION_CL_1_1 1

#define GL_VERSION_ES_CL_1_1 0x1
#define GL_VERSION_ES_CL_1_0 0x1
#define GL_CURRENT_COLOR 0x0B00
#define GL_CURRENT_NORMAL 0x0B02
#define GL_CURRENT_TEXTURE_COORDS 0x0B03
#define GL_POINT_SIZE 0x0B11
#define GL_LINE_WIDTH 0x0B21
#define GL_CULL_FACE_MODE 0x0B45
#define GL_FRONT_FACE 0x0B46
#define GL_SHADE_MODEL 0x0B54
#define GL_DEPTH_RANGE 0x0B70
#define GL_DEPTH_WRITEMASK 0x0B72
#define GL_DEPTH_CLEAR_VALUE 0x0B73
#define GL_DEPTH_FUNC 0x0B74
#define GL_STENCIL_CLEAR_VALUE 0x0B91
#define GL_STENCIL_FUNC 0x0B92
#define GL_STENCIL_VALUE_MASK 0x0B93
#define GL_STENCIL_FAIL 0x0B94
#define GL_STENCIL_PASS_DEPTH_FAIL 0x0B95
#define GL_STENCIL_PASS_DEPTH_PASS 0x0B96
#define GL_STENCIL_REF 0x0B97
#define GL_STENCIL_WRITEMASK 0x0B98
#define GL_MATRIX_MODE 0x0BA0
#define GL_VIEWPORT 0x0BA2
#define GL_MODELVIEW_STACK_DEPTH 0x0BA3
#define GL_PROJECTION_STACK_DEPTH 0x0BA4
#define GL_TEXTURE_STACK_DEPTH 0x0BA5
#define GL_MODELVIEW_MATRIX 0x0BA6
#define GL_PROJECTION_MATRIX 0x0BA7
#define GL_TEXTURE_MATRIX 0x0BA8
#define GL_ALPHA_TEST_FUNC 0x0BC1
#define GL_ALPHA_TEST_REF 0x0BC2
#define GL_BLEND_DST 0x0BE0
#define GL_BLEND_SRC 0x0BE1
#define GL_LOGIC_OP_MODE 0x0BF0
#define GL_SCISSOR_BOX 0x0C10
#define GL_COLOR_CLEAR_VALUE 0x0C22
#define GL_COLOR_WRITEMASK 0x0C23
#define GL_ALPHA_SCALE 0x0D1C
#define GL_MAX_CLIP_PLANES 0x0D32
#define GL_POLYGON_OFFSET_UNITS 0x2A00
#define GL_CLIP_PLANE0 0x3000
#define GL_CLIP_PLANE1 0x3001
#define GL_CLIP_PLANE2 0x3002
#define GL_CLIP_PLANE3 0x3003
#define GL_CLIP_PLANE4 0x3004
#define GL_CLIP_PLANE5 0x3005
#define GL_POLYGON_OFFSET_FACTOR 0x8038
#define GL_TEXTURE_BINDING_2D 0x8069
#define GL_VERTEX_ARRAY_SIZE 0x807A
#define GL_VERTEX_ARRAY_TYPE 0x807B
#define GL_VERTEX_ARRAY_STRIDE 0x807C
#define GL_NORMAL_ARRAY_TYPE 0x807E
#define GL_NORMAL_ARRAY_STRIDE 0x807F
#define GL_COLOR_ARRAY_SIZE 0x8081
#define GL_COLOR_ARRAY_TYPE 0x8082
#define GL_COLOR_ARRAY_STRIDE 0x8083
#define GL_TEXTURE_COORD_ARRAY_SIZE 0x8088
#define GL_TEXTURE_COORD_ARRAY_TYPE 0x8089
#define GL_TEXTURE_COORD_ARRAY_STRIDE 0x808A
#define GL_VERTEX_ARRAY_POINTER 0x808E
#define GL_NORMAL_ARRAY_POINTER 0x808F
#define GL_COLOR_ARRAY_POINTER 0x8090
#define GL_TEXTURE_COORD_ARRAY_POINTER 0x8092
#define GL_SAMPLE_BUFFERS 0x80A8
#define GL_SAMPLES 0x80A9
#define GL_SAMPLE_COVERAGE_VALUE 0x80AA
#define GL_SAMPLE_COVERAGE_INVERT 0x80AB
#define GL_POINT_SIZE_MIN 0x8126
#define GL_POINT_SIZE_MAX 0x8127
#define GL_POINT_FADE_THRESHOLD_SIZE 0x8128
#define GL_POINT_DISTANCE_ATTENUATION 0x8129
#define GL_GENERATE_MIPMAP 0x8191
#define GL_GENERATE_MIPMAP_HINT 0x8192
#define GL_ACTIVE_TEXTURE 0x84E0
#define GL_CLIENT_ACTIVE_TEXTURE 0x84E1
#define GL_SUBTRACT 0x84E7
#define GL_COMBINE 0x8570
#define GL_COMBINE_RGB 0x8571
#define GL_COMBINE_ALPHA 0x8572
#define GL_RGB_SCALE 0x8573
#define GL_ADD_SIGNED 0x8574
#define GL_INTERPOLATE 0x8575
#define GL_CONSTANT 0x8576
#define GL_PRIMARY_COLOR 0x8577
#define GL_PREVIOUS 0x8578
#define GL_SRC0_RGB 0x8580
#define GL_SRC1_RGB 0x8581
#define GL_SRC2_RGB 0x8582
#define GL_SRC0_ALPHA 0x8588
#define GL_SRC1_ALPHA 0x8589
#define GL_SRC2_ALPHA 0x858A
#define GL_OPERAND0_RGB 0x8590
#define GL_OPERAND1_RGB 0x8591
#define GL_OPERAND2_RGB 0x8592
#define GL_OPERAND0_ALPHA 0x8598
#define GL_OPERAND1_ALPHA 0x8599
#define GL_OPERAND2_ALPHA 0x859A
#define GL_DOT3_RGB 0x86AE
#define GL_DOT3_RGBA 0x86AF
#define GL_BUFFER_SIZE 0x8764
#define GL_BUFFER_USAGE 0x8765
#define GL_ARRAY_BUFFER 0x8892
#define GL_ELEMENT_ARRAY_BUFFER 0x8893
#define GL_ARRAY_BUFFER_BINDING 0x8894
#define GL_ELEMENT_ARRAY_BUFFER_BINDING 0x8895
#define GL_VERTEX_ARRAY_BUFFER_BINDING 0x8896
#define GL_NORMAL_ARRAY_BUFFER_BINDING 0x8897
#define GL_COLOR_ARRAY_BUFFER_BINDING 0x8898
#define GL_TEXTURE_COORD_ARRAY_BUFFER_BINDING 0x889A
#define GL_STATIC_DRAW 0x88E4
#define GL_DYNAMIC_DRAW 0x88E8

typedef char             GLchar;
typedef khronos_intptr_t GLintptr;
typedef khronos_ssize_t  GLsizeiptr;

typedef void  (GLAPIENTRY * PFNGLBINDBUFFERPROC) (GLenum , GLuint );
typedef void  (GLAPIENTRY * PFNGLBUFFERDATAPROC) (GLenum , GLsizeiptr, const GLvoid *, GLenum );
typedef void  (GLAPIENTRY * PFNGLBUFFERSUBDATAPROC) (GLenum, GLintptr, GLsizeiptr, const GLvoid *);
typedef void  (GLAPIENTRY * PFNGLCLIPPLANEXPROC) (GLenum ,const GLfixed *);
typedef void  (GLAPIENTRY * PFNGLCOLOR4UBPROC) (GLubyte, GLubyte, GLubyte, GLubyte);
typedef void  (GLAPIENTRY * PFNGLDELETEBUFFERSPROC) (GLsizei , const GLuint *);
typedef void  (GLAPIENTRY * PFNGLGENBUFFERSPROC) (GLsizei, GLuint *);
typedef void  (GLAPIENTRY * PFNGLGETBOOLEANVPROC) (GLenum, GLboolean *);
typedef void  (GLAPIENTRY * PFNGLGETBUFFERPARAMETERIVPROC) (GLenum, GLenum, GLint *);
typedef void  (GLAPIENTRY * PFNGLGETCLIPPLANEXPROC) (GLenum ,GLfixed*);
typedef void  (GLAPIENTRY * PFNGLGETFIXEDVPROC) (GLenum, GLfixed *);
typedef void  (GLAPIENTRY * PFNGLGETLIGHTXVPROC) (GLenum, GLenum, GLfixed *);
typedef void  (GLAPIENTRY * PFNGLGETMATERIALXVPROC) (GLenum, GLenum, GLfixed *);
typedef void  (GLAPIENTRY * PFNGLGETPOINTERVPROC) (GLenum , GLvoid **);
typedef void  (GLAPIENTRY * PFNGLGETTEXENVIVPROC) (GLenum, GLenum , GLint *);
typedef void  (GLAPIENTRY * PFNGLGETTEXENVXVPROC) (GLenum, GLenum, GLfixed *);
typedef void  (GLAPIENTRY * PFNGLGETTEXPARAMETERIVPROC) (GLenum, GLenum, GLint *);
typedef void  (GLAPIENTRY * PFNGLGETTEXPARAMETERXVPROC) (GLenum, GLenum, GLfixed *);
typedef GLboolean  (GLAPIENTRY * PFNGLISBUFFERPROC) (GLuint);
typedef GLboolean  (GLAPIENTRY * PFNGLISENABLEDPROC) (GLenum);
typedef GLboolean  (GLAPIENTRY * PFNGLISTEXTUREPROC) (GLuint);
typedef void  (GLAPIENTRY * PFNGLPOINTPARAMETERXPROC) (GLenum, GLfixed);
typedef void  (GLAPIENTRY * PFNGLPOINTPARAMETERXVPROC) (GLenum, const GLfixed *);
typedef void  (GLAPIENTRY * PFNGLTEXENVIPROC) (GLenum, GLenum, GLint);
typedef void  (GLAPIENTRY * PFNGLTEXENVIVPROC) (GLenum, GLenum, const GLint *);
typedef void  (GLAPIENTRY * PFNGLTEXPARAMETERIPROC) (GLenum, GLenum, GLint);
typedef void  (GLAPIENTRY * PFNGLTEXPARAMETERIVPROC) (GLenum, GLenum, const GLint *);
typedef void  (GLAPIENTRY * PFNGLTEXPARAMETERXVPROC) (GLenum, GLenum, const GLfixed *);

#define glBindBuffer GLEW_GET_FUN(__glewBindBuffer)
#define glBufferData GLEW_GET_FUN(__glewBufferData)
#define glBufferSubData GLEW_GET_FUN(__glewBufferSubData)
#define glClipPlanex GLEW_GET_FUN(__glewClipPlanex)
#define glColor4ub GLEW_GET_FUN(__glewColor4ub)
#define glDeleteBuffers GLEW_GET_FUN(__glewDeleteBuffers)
#define glGenBuffers GLEW_GET_FUN(__glewGenBuffers)
#define glGetBooleanv GLEW_GET_FUN(__glewGetBooleanv)
#define glGetBufferParameteriv GLEW_GET_FUN(__glewGetBufferParameteriv)
#define glGetClipPlanex GLEW_GET_FUN(__glewGetClipPlanex)
#define glGetFixedv GLEW_GET_FUN(__glewGetFixedv)
#define glGetLightxv GLEW_GET_FUN(__glewGetLightxv)
#define glGetMaterialxv GLEW_GET_FUN(__glewGetMaterialxv)
#define glGetPointerv GLEW_GET_FUN(__glewGetPointerv)
#define glGetTexEnviv GLEW_GET_FUN(__glewGetTexEnviv)
#define glGetTexEnvxv GLEW_GET_FUN(__glewGetTexEnvxv)
#define glGetTexParameteriv GLEW_GET_FUN(__glewGetTexParameteriv)
#define glGetTexParameterxv GLEW_GET_FUN(__glewGetTexParameterxv)
#define glIsBuffer GLEW_GET_FUN(__glewIsBuffer)
#define glIsEnabled GLEW_GET_FUN(__glewIsEnabled)
#define glIsTexture GLEW_GET_FUN(__glewIsTexture)
#define glPointParameterx GLEW_GET_FUN(__glewPointParameterx)
#define glPointParameterxv GLEW_GET_FUN(__glewPointParameterxv)
#define glTexEnvi GLEW_GET_FUN(__glewTexEnvi)
#define glTexEnviv GLEW_GET_FUN(__glewTexEnviv)
#define glTexParameteri GLEW_GET_FUN(__glewTexParameteri)
#define glTexParameteriv GLEW_GET_FUN(__glewTexParameteriv)
#define glTexParameterxv GLEW_GET_FUN(__glewTexParameterxv)

#else // XXX
#define GL_ES_VERSION_CL_1_1 0  // NOTE jwilkins: define version token
#endif /* !GL_ES_VERSION_CL_1_1 */

#define GLEW_ES_VERSION_CL_1_1 GLEW_GET_VAR(__GLEW_ES_VERSION_CL_1_1) // NOTE jwilkins: always needs to be defined

/* -------------------------- GL_ES_VERSION_CM_1_1 ------------------------- */

#if GL_ES_VERSION_CM_1_1 // NOTE jwilkins: should be if not ifdef
#define GL_ES_VERSION_CM_1_1 1

#define GL_VERSION_ES_CM_1_1 0x1
#define GL_VERSION_ES_CM_1_0 0x1

typedef void  (GLAPIENTRY * PFNGLCLIPPLANEFPROC) (GLenum, const GLfloat *);
typedef void  (GLAPIENTRY * PFNGLGETCLIPPLANEFPROC) (GLenum , GLfloat* );
typedef void  (GLAPIENTRY * PFNGLGETFLOATVPROC) (GLenum , GLfloat *);
typedef void  (GLAPIENTRY * PFNGLGETLIGHTFVPROC) (GLenum , GLenum , GLfloat *);
typedef void  (GLAPIENTRY * PFNGLGETMATERIALFVPROC) (GLenum, GLenum, GLfloat *);
typedef void  (GLAPIENTRY * PFNGLGETTEXENVFVPROC) (GLenum env, GLenum, GLfloat *);
typedef void  (GLAPIENTRY * PFNGLGETTEXPARAMETERFVPROC) (GLenum, GLenum, GLfloat *);
typedef void  (GLAPIENTRY * PFNGLPOINTPARAMETERFPROC) (GLenum , GLfloat );
typedef void  (GLAPIENTRY * PFNGLPOINTPARAMETERFVPROC) (GLenum, const GLfloat *);
typedef void  (GLAPIENTRY * PFNGLTEXPARAMETERFVPROC) (GLenum, GLenum , const GLfloat *);

#define glClipPlanef GLEW_GET_FUN(__glewClipPlanef)
#define glGetClipPlanef GLEW_GET_FUN(__glewGetClipPlanef)
#define glGetFloatv GLEW_GET_FUN(__glewGetFloatv)
#define glGetLightfv GLEW_GET_FUN(__glewGetLightfv)
#define glGetMaterialfv GLEW_GET_FUN(__glewGetMaterialfv)
#define glGetTexEnvfv GLEW_GET_FUN(__glewGetTexEnvfv)
#define glGetTexParameterfv GLEW_GET_FUN(__glewGetTexParameterfv)
#define glPointParameterf GLEW_GET_FUN(__glewPointParameterf)
#define glPointParameterfv GLEW_GET_FUN(__glewPointParameterfv)
#define glTexParameterfv GLEW_GET_FUN(__glewTexParameterfv)

#else // XXX
#define GL_ES_VERSION_CM_1_1 0 // NOTE jwilkins: define version token
#endif /* !GL_ES_VERSION_CM_1_1 */

#define GLEW_ES_VERSION_CM_1_1 GLEW_GET_VAR(__GLEW_ES_VERSION_CM_1_1) // NOTE jwilkins: always needs to be defined

/* --------------------------- GL_ES_VERSION_2_0 --------------------------- */

#if GL_ES_VERSION_2_0 // NOTE jwilkins: should be if not ifdef
#define GL_ES_VERSION_2_0 1

#define GL_NONE 0
#define GL_INVALID_FRAMEBUFFER_OPERATION 0x0506
#define GL_INT 0x1404
#define GL_UNSIGNED_INT 0x1405
#define GL_STENCIL_INDEX 0x1901
#define GL_DEPTH_COMPONENT 0x1902
#define GL_CONSTANT_COLOR 0x8001
#define GL_ONE_MINUS_CONSTANT_COLOR 0x8002
#define GL_CONSTANT_ALPHA 0x8003
#define GL_ONE_MINUS_CONSTANT_ALPHA 0x8004
#define GL_BLEND_COLOR 0x8005
#define GL_FUNC_ADD 0x8006
#define GL_BLEND_EQUATION 0x8009
#define GL_BLEND_EQUATION_RGB 0x8009
#define GL_FUNC_SUBTRACT 0x800A
#define GL_FUNC_REVERSE_SUBTRACT 0x800B
#define GL_RGBA4 0x8056
#define GL_RGB5_A1 0x8057
#define GL_BLEND_DST_RGB 0x80C8
#define GL_BLEND_SRC_RGB 0x80C9
#define GL_BLEND_DST_ALPHA 0x80CA
#define GL_BLEND_SRC_ALPHA 0x80CB
#define GL_DEPTH_COMPONENT16 0x81A5
#define GL_MIRRORED_REPEAT 0x8370
#define GL_MAX_RENDERBUFFER_SIZE 0x84E8
#define GL_INCR_WRAP 0x8507
#define GL_DECR_WRAP 0x8508
#define GL_TEXTURE_CUBE_MAP 0x8513
#define GL_TEXTURE_BINDING_CUBE_MAP 0x8514
#define GL_TEXTURE_CUBE_MAP_POSITIVE_X 0x8515
#define GL_TEXTURE_CUBE_MAP_NEGATIVE_X 0x8516
#define GL_TEXTURE_CUBE_MAP_POSITIVE_Y 0x8517
#define GL_TEXTURE_CUBE_MAP_NEGATIVE_Y 0x8518
#define GL_TEXTURE_CUBE_MAP_POSITIVE_Z 0x8519
#define GL_TEXTURE_CUBE_MAP_NEGATIVE_Z 0x851A
#define GL_MAX_CUBE_MAP_TEXTURE_SIZE 0x851C
#define GL_VERTEX_ATTRIB_ARRAY_ENABLED 0x8622
#define GL_VERTEX_ATTRIB_ARRAY_SIZE 0x8623
#define GL_VERTEX_ATTRIB_ARRAY_STRIDE 0x8624
#define GL_VERTEX_ATTRIB_ARRAY_TYPE 0x8625
#define GL_CURRENT_VERTEX_ATTRIB 0x8626
#define GL_VERTEX_ATTRIB_ARRAY_POINTER 0x8645
#define GL_STENCIL_BACK_FUNC 0x8800
#define GL_STENCIL_BACK_FAIL 0x8801
#define GL_STENCIL_BACK_PASS_DEPTH_FAIL 0x8802
#define GL_STENCIL_BACK_PASS_DEPTH_PASS 0x8803
#define GL_BLEND_EQUATION_ALPHA 0x883D
#define GL_MAX_VERTEX_ATTRIBS 0x8869
#define GL_VERTEX_ATTRIB_ARRAY_NORMALIZED 0x886A
#define GL_MAX_TEXTURE_IMAGE_UNITS 0x8872
#define GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING 0x889F
#define GL_STREAM_DRAW 0x88E0
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_VERTEX_SHADER 0x8B31
#define GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS 0x8B4C
#define GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS 0x8B4D
#define GL_SHADER_TYPE 0x8B4F
#define GL_FLOAT_VEC2 0x8B50
#define GL_FLOAT_VEC3 0x8B51
#define GL_FLOAT_VEC4 0x8B52
#define GL_INT_VEC2 0x8B53
#define GL_INT_VEC3 0x8B54
#define GL_INT_VEC4 0x8B55
#define GL_BOOL 0x8B56
#define GL_BOOL_VEC2 0x8B57
#define GL_BOOL_VEC3 0x8B58
#define GL_BOOL_VEC4 0x8B59
#define GL_FLOAT_MAT2 0x8B5A
#define GL_FLOAT_MAT3 0x8B5B
#define GL_FLOAT_MAT4 0x8B5C
#define GL_SAMPLER_2D 0x8B5E
#define GL_SAMPLER_CUBE 0x8B60
#define GL_DELETE_STATUS 0x8B80
#define GL_COMPILE_STATUS 0x8B81
#define GL_LINK_STATUS 0x8B82
#define GL_VALIDATE_STATUS 0x8B83
#define GL_INFO_LOG_LENGTH 0x8B84
#define GL_ATTACHED_SHADERS 0x8B85
#define GL_ACTIVE_UNIFORMS 0x8B86
#define GL_ACTIVE_UNIFORM_MAX_LENGTH 0x8B87
#define GL_SHADER_SOURCE_LENGTH 0x8B88
#define GL_ACTIVE_ATTRIBUTES 0x8B89
#define GL_ACTIVE_ATTRIBUTE_MAX_LENGTH 0x8B8A
#define GL_SHADING_LANGUAGE_VERSION 0x8B8C
#define GL_CURRENT_PROGRAM 0x8B8D
#define GL_IMPLEMENTATION_COLOR_READ_TYPE 0x8B9A
#define GL_IMPLEMENTATION_COLOR_READ_FORMAT 0x8B9B
#define GL_STENCIL_BACK_REF 0x8CA3
#define GL_STENCIL_BACK_VALUE_MASK 0x8CA4
#define GL_STENCIL_BACK_WRITEMASK 0x8CA5
#define GL_FRAMEBUFFER_BINDING 0x8CA6
#define GL_RENDERBUFFER_BINDING 0x8CA7
#define GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE 0x8CD0
#define GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME 0x8CD1
#define GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL 0x8CD2
#define GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE 0x8CD3
#define GL_FRAMEBUFFER_COMPLETE 0x8CD5
#define GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT 0x8CD6
#define GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT 0x8CD7
#define GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS 0x8CD9
#define GL_FRAMEBUFFER_UNSUPPORTED 0x8CDD
#define GL_COLOR_ATTACHMENT0 0x8CE0
#define GL_DEPTH_ATTACHMENT 0x8D00
#define GL_STENCIL_ATTACHMENT 0x8D20
#define GL_FRAMEBUFFER 0x8D40
#define GL_RENDERBUFFER 0x8D41
#define GL_RENDERBUFFER_WIDTH 0x8D42
#define GL_RENDERBUFFER_HEIGHT 0x8D43
#define GL_RENDERBUFFER_INTERNAL_FORMAT 0x8D44
#define GL_STENCIL_INDEX8 0x8D48
#define GL_RENDERBUFFER_RED_SIZE 0x8D50
#define GL_RENDERBUFFER_GREEN_SIZE 0x8D51
#define GL_RENDERBUFFER_BLUE_SIZE 0x8D52
#define GL_RENDERBUFFER_ALPHA_SIZE 0x8D53
#define GL_RENDERBUFFER_DEPTH_SIZE 0x8D54
#define GL_RENDERBUFFER_STENCIL_SIZE 0x8D55
#define GL_RGB565 0x8D62
#define GL_LOW_FLOAT 0x8DF0
#define GL_MEDIUM_FLOAT 0x8DF1
#define GL_HIGH_FLOAT 0x8DF2
#define GL_LOW_INT 0x8DF3
#define GL_MEDIUM_INT 0x8DF4
#define GL_HIGH_INT 0x8DF5
#define GL_SHADER_BINARY_FORMATS 0x8DF8
#define GL_NUM_SHADER_BINARY_FORMATS 0x8DF9
#define GL_SHADER_COMPILER 0x8DFA
#define GL_MAX_VERTEX_UNIFORM_VECTORS 0x8DFB
#define GL_MAX_VARYING_VECTORS 0x8DFC
#define GL_MAX_FRAGMENT_UNIFORM_VECTORS 0x8DFD

#define GL_VIEWPORT 0x0BA2 // XXX missing enum
#define GL_SCISSOR_BOX 0x0C10 // XXX missing enum
#define GL_GENERATE_MIPMAP_HINT 0x8192 // XXX missing enum
#define GL_ARRAY_BUFFER 0x8892 // XXX missing enum
#define GL_ELEMENT_ARRAY_BUFFER 0x8893 // XXX missing enum
#define GL_ARRAY_BUFFER_BINDING 0x8894 // XXX missing enum
#define GL_ELEMENT_ARRAY_BUFFER_BINDING 0x8895 // XXX missing enum
#define GL_STATIC_DRAW 0x88E4 // XXX missing enum
#define GL_DYNAMIC_DRAW 0x88E8 // XXX missing enum
#define GL_SAMPLE_BUFFERS 0x80A8 // XXX missing enum

typedef char             GLchar; // NOTE jwilkins: this typedef is missing when ES 1.1 is not enabled
typedef khronos_intptr_t GLintptr; // XXX
typedef khronos_ssize_t  GLsizeiptr; // XXX

typedef void  (GLAPIENTRY * PFNGLATTACHSHADERPROC) (GLuint program, GLuint shader);
typedef void  (GLAPIENTRY * PFNGLBINDATTRIBLOCATIONPROC) (GLuint program, GLuint index, const GLchar* name);
typedef void  (GLAPIENTRY * PFNGLBINDFRAMEBUFFERPROC) (GLenum target, GLuint framebuffer);
typedef void  (GLAPIENTRY * PFNGLBINDRENDERBUFFERPROC) (GLenum target, GLuint renderbuffer);
typedef void  (GLAPIENTRY * PFNGLBLENDCOLORPROC) (GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha);
typedef void  (GLAPIENTRY * PFNGLBLENDEQUATIONPROC) ( GLenum mode );
typedef void  (GLAPIENTRY * PFNGLBLENDEQUATIONSEPARATEPROC) (GLenum modeRGB, GLenum modeAlpha);
typedef void  (GLAPIENTRY * PFNGLBLENDFUNCSEPARATEPROC) (GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha);
typedef GLenum  (GLAPIENTRY * PFNGLCHECKFRAMEBUFFERSTATUSPROC) (GLenum target);
typedef void  (GLAPIENTRY * PFNGLCOMPILESHADERPROC) (GLuint shader);
typedef GLuint  (GLAPIENTRY * PFNGLCREATEPROGRAMPROC) (void);
typedef GLuint  (GLAPIENTRY * PFNGLCREATESHADERPROC) (GLenum type);
typedef void  (GLAPIENTRY * PFNGLDELETEFRAMEBUFFERSPROC) (GLsizei n, const GLuint* framebuffers);
typedef void  (GLAPIENTRY * PFNGLDELETEPROGRAMPROC) (GLuint program);
typedef void  (GLAPIENTRY * PFNGLDELETERENDERBUFFERSPROC) (GLsizei n, const GLuint* renderbuffers);
typedef void  (GLAPIENTRY * PFNGLDELETESHADERPROC) (GLuint shader);
typedef void  (GLAPIENTRY * PFNGLDETACHSHADERPROC) (GLuint program, GLuint shader);
typedef void  (GLAPIENTRY * PFNGLDISABLEVERTEXATTRIBARRAYPROC) (GLuint index);
typedef void  (GLAPIENTRY * PFNGLENABLEVERTEXATTRIBARRAYPROC) (GLuint index);
typedef void  (GLAPIENTRY * PFNGLFRAMEBUFFERRENDERBUFFERPROC) (GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer);
typedef void  (GLAPIENTRY * PFNGLFRAMEBUFFERTEXTURE2DPROC) (GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
typedef void  (GLAPIENTRY * PFNGLGENFRAMEBUFFERSPROC) (GLsizei n, GLuint* framebuffers);
typedef void  (GLAPIENTRY * PFNGLGENRENDERBUFFERSPROC) (GLsizei n, GLuint* renderbuffers);
typedef void  (GLAPIENTRY * PFNGLGENERATEMIPMAPPROC) (GLenum target);
typedef void  (GLAPIENTRY * PFNGLGETACTIVEATTRIBPROC) (GLuint program, GLuint index, GLsizei bufsize, GLsizei* length, GLint* size, GLenum* type, GLchar* name);
typedef void  (GLAPIENTRY * PFNGLGETACTIVEUNIFORMPROC) (GLuint program, GLuint index, GLsizei bufsize, GLsizei* length, GLint* size, GLenum* type, GLchar* name);
typedef void  (GLAPIENTRY * PFNGLGETATTACHEDSHADERSPROC) (GLuint program, GLsizei maxcount, GLsizei* count, GLuint* shaders);
typedef int  (GLAPIENTRY * PFNGLGETATTRIBLOCATIONPROC) (GLuint program, const GLchar* name);
typedef void  (GLAPIENTRY * PFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIVPROC) (GLenum target, GLenum attachment, GLenum pname, GLint* params);
typedef void  (GLAPIENTRY * PFNGLGETPROGRAMINFOLOGPROC) (GLuint program, GLsizei bufsize, GLsizei* length, GLchar* infolog);
typedef void  (GLAPIENTRY * PFNGLGETPROGRAMIVPROC) (GLuint program, GLenum pname, GLint* params);
typedef void  (GLAPIENTRY * PFNGLGETRENDERBUFFERPARAMETERIVPROC) (GLenum target, GLenum pname, GLint* params);
typedef void  (GLAPIENTRY * PFNGLGETSHADERINFOLOGPROC) (GLuint shader, GLsizei bufsize, GLsizei* length, GLchar* infolog);
typedef void  (GLAPIENTRY * PFNGLGETSHADERPRECISIONFORMATPROC) (GLenum shadertype, GLenum precisiontype, GLint* range, GLint* precision);
typedef void  (GLAPIENTRY * PFNGLGETSHADERSOURCEPROC) (GLuint shader, GLsizei bufsize, GLsizei* length, GLchar* source);
typedef void  (GLAPIENTRY * PFNGLGETSHADERIVPROC) (GLuint shader, GLenum pname, GLint* params);
typedef int  (GLAPIENTRY * PFNGLGETUNIFORMLOCATIONPROC) (GLuint program, const GLchar* name);
typedef void  (GLAPIENTRY * PFNGLGETUNIFORMFVPROC) (GLuint program, GLint location, GLfloat* params);
typedef void  (GLAPIENTRY * PFNGLGETUNIFORMIVPROC) (GLuint program, GLint location, GLint* params);
typedef void  (GLAPIENTRY * PFNGLGETVERTEXATTRIBPOINTERVPROC) (GLuint index, GLenum pname, GLvoid** pointer);
typedef void  (GLAPIENTRY * PFNGLGETVERTEXATTRIBFVPROC) (GLuint index, GLenum pname, GLfloat* params);
typedef void  (GLAPIENTRY * PFNGLGETVERTEXATTRIBIVPROC) (GLuint index, GLenum pname, GLint* params);
typedef GLboolean  (GLAPIENTRY * PFNGLISFRAMEBUFFERPROC) (GLuint framebuffer);
typedef GLboolean  (GLAPIENTRY * PFNGLISPROGRAMPROC) (GLuint program);
typedef GLboolean  (GLAPIENTRY * PFNGLISRENDERBUFFERPROC) (GLuint renderbuffer);
typedef GLboolean  (GLAPIENTRY * PFNGLISSHADERPROC) (GLuint shader);
typedef void  (GLAPIENTRY * PFNGLLINKPROGRAMPROC) (GLuint program);
typedef void  (GLAPIENTRY * PFNGLRELEASESHADERCOMPILERPROC) (void);
typedef void  (GLAPIENTRY * PFNGLRENDERBUFFERSTORAGEPROC) (GLenum target, GLenum internalformat, GLsizei width, GLsizei height);
typedef void  (GLAPIENTRY * PFNGLSHADERBINARYPROC) (GLsizei n, const GLuint* shaders, GLenum binaryformat, const GLvoid* binary, GLsizei length);
typedef void  (GLAPIENTRY * PFNGLSHADERSOURCEPROC) (GLuint shader, GLsizei count, const GLchar** string, const GLint* length);
typedef void  (GLAPIENTRY * PFNGLSTENCILFUNCSEPARATEPROC) (GLenum face, GLenum func, GLint ref, GLuint mask);
typedef void  (GLAPIENTRY * PFNGLSTENCILMASKSEPARATEPROC) (GLenum face, GLuint mask);
typedef void  (GLAPIENTRY * PFNGLSTENCILOPSEPARATEPROC) (GLenum face, GLenum fail, GLenum zfail, GLenum zpass);
typedef void  (GLAPIENTRY * PFNGLUNIFORM1FPROC) (GLint location, GLfloat x);
typedef void  (GLAPIENTRY * PFNGLUNIFORM1FVPROC) (GLint location, GLsizei count, const GLfloat* v);
typedef void  (GLAPIENTRY * PFNGLUNIFORM1IPROC) (GLint location, GLint x);
typedef void  (GLAPIENTRY * PFNGLUNIFORM1IVPROC) (GLint location, GLsizei count, const GLint* v);
typedef void  (GLAPIENTRY * PFNGLUNIFORM2FPROC) (GLint location, GLfloat x, GLfloat y);
typedef void  (GLAPIENTRY * PFNGLUNIFORM2FVPROC) (GLint location, GLsizei count, const GLfloat* v);
typedef void  (GLAPIENTRY * PFNGLUNIFORM2IPROC) (GLint location, GLint x, GLint y);
typedef void  (GLAPIENTRY * PFNGLUNIFORM2IVPROC) (GLint location, GLsizei count, const GLint* v);
typedef void  (GLAPIENTRY * PFNGLUNIFORM3FPROC) (GLint location, GLfloat x, GLfloat y, GLfloat z);
typedef void  (GLAPIENTRY * PFNGLUNIFORM3FVPROC) (GLint location, GLsizei count, const GLfloat* v);
typedef void  (GLAPIENTRY * PFNGLUNIFORM3IPROC) (GLint location, GLint x, GLint y, GLint z);
typedef void  (GLAPIENTRY * PFNGLUNIFORM3IVPROC) (GLint location, GLsizei count, const GLint* v);
typedef void  (GLAPIENTRY * PFNGLUNIFORM4FPROC) (GLint location, GLfloat x, GLfloat y, GLfloat z, GLfloat w);
typedef void  (GLAPIENTRY * PFNGLUNIFORM4FVPROC) (GLint location, GLsizei count, const GLfloat* v);
typedef void  (GLAPIENTRY * PFNGLUNIFORM4IPROC) (GLint location, GLint x, GLint y, GLint z, GLint w);
typedef void  (GLAPIENTRY * PFNGLUNIFORM4IVPROC) (GLint location, GLsizei count, const GLint* v);
typedef void  (GLAPIENTRY * PFNGLUNIFORMMATRIX2FVPROC) (GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
typedef void  (GLAPIENTRY * PFNGLUNIFORMMATRIX3FVPROC) (GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
typedef void  (GLAPIENTRY * PFNGLUNIFORMMATRIX4FVPROC) (GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
typedef void  (GLAPIENTRY * PFNGLUSEPROGRAMPROC) (GLuint program);
typedef void  (GLAPIENTRY * PFNGLVALIDATEPROGRAMPROC) (GLuint program);
typedef void  (GLAPIENTRY * PFNGLVERTEXATTRIB1FPROC) (GLuint indx, GLfloat x);
typedef void  (GLAPIENTRY * PFNGLVERTEXATTRIB1FVPROC) (GLuint indx, const GLfloat* values);
typedef void  (GLAPIENTRY * PFNGLVERTEXATTRIB2FPROC) (GLuint indx, GLfloat x, GLfloat y);
typedef void  (GLAPIENTRY * PFNGLVERTEXATTRIB2FVPROC) (GLuint indx, const GLfloat* values);
typedef void  (GLAPIENTRY * PFNGLVERTEXATTRIB3FPROC) (GLuint indx, GLfloat x, GLfloat y, GLfloat z);
typedef void  (GLAPIENTRY * PFNGLVERTEXATTRIB3FVPROC) (GLuint indx, const GLfloat* values);
typedef void  (GLAPIENTRY * PFNGLVERTEXATTRIB4FPROC) (GLuint indx, GLfloat x, GLfloat y, GLfloat z, GLfloat w);
typedef void  (GLAPIENTRY * PFNGLVERTEXATTRIB4FVPROC) (GLuint indx, const GLfloat* values);
typedef void  (GLAPIENTRY * PFNGLVERTEXATTRIBPOINTERPROC) (GLuint indx, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid* ptr);

typedef void  (GLAPIENTRY * PFNGLBINDBUFFERPROC) (GLenum , GLuint ); // NOTE jwilkins: missing function
typedef void  (GLAPIENTRY * PFNGLBUFFERDATAPROC) (GLenum , GLsizeiptr, const GLvoid *, GLenum ); // NOTE jwilkins: missing function
typedef void  (GLAPIENTRY * PFNGLBUFFERSUBDATAPROC) (GLenum, GLintptr, GLsizeiptr, const GLvoid *); // NOTE jwilkins: missing function
typedef void  (GLAPIENTRY * PFNGLDELETEBUFFERSPROC) (GLsizei , const GLuint *); // NOTE jwilkins: missing function
typedef void  (GLAPIENTRY * PFNGLGENBUFFERSPROC) (GLsizei, GLuint *); // NOTE jwilkins: missing function
typedef void  (GLAPIENTRY * PFNGLTEXPARAMETERIPROC) (GLenum, GLenum, GLint); // NOTE jwilkins: missing function
typedef GLboolean  (GLAPIENTRY * PFNGLISENABLEDPROC) (GLenum); // NOTE jwilkins: missing function
typedef void  (GLAPIENTRY * PFNGLGETFLOATVPROC) (GLenum , GLfloat *); // NOTE jwilkins: missing function
typedef void (GLAPIENTRY * PFNGLDEPTHRANGEFPROC) (GLclampf zNear, GLclampf zFar); // NOTE jwilkins: missing function
typedef void (GLAPIENTRY * PFNGLACTIVETEXTUREPROC) (GLenum texture); // NOTE jwilkins: missing function
typedef void  (GLAPIENTRY * PFNGLGETBOOLEANVPROC) (GLenum, GLboolean *); // NOTE jwilkins: missing function

#define glAttachShader GLEW_GET_FUN(__glewAttachShader)
#define glBindAttribLocation GLEW_GET_FUN(__glewBindAttribLocation)
#define glBindFramebuffer GLEW_GET_FUN(__glewBindFramebuffer)
#define glBindRenderbuffer GLEW_GET_FUN(__glewBindRenderbuffer)
#define glBlendColor GLEW_GET_FUN(__glewBlendColor)
#define glBlendEquation GLEW_GET_FUN(__glewBlendEquation)
#define glBlendEquationSeparate GLEW_GET_FUN(__glewBlendEquationSeparate)
#define glBlendFuncSeparate GLEW_GET_FUN(__glewBlendFuncSeparate)
#define glCheckFramebufferStatus GLEW_GET_FUN(__glewCheckFramebufferStatus)
#define glCompileShader GLEW_GET_FUN(__glewCompileShader)
#define glCreateProgram GLEW_GET_FUN(__glewCreateProgram)
#define glCreateShader GLEW_GET_FUN(__glewCreateShader)
#define glDeleteFramebuffers GLEW_GET_FUN(__glewDeleteFramebuffers)
#define glDeleteProgram GLEW_GET_FUN(__glewDeleteProgram)
#define glDeleteRenderbuffers GLEW_GET_FUN(__glewDeleteRenderbuffers)
#define glDeleteShader GLEW_GET_FUN(__glewDeleteShader)
#define glDetachShader GLEW_GET_FUN(__glewDetachShader)
#define glDisableVertexAttribArray GLEW_GET_FUN(__glewDisableVertexAttribArray)
#define glEnableVertexAttribArray GLEW_GET_FUN(__glewEnableVertexAttribArray)
#define glFramebufferRenderbuffer GLEW_GET_FUN(__glewFramebufferRenderbuffer)
#define glFramebufferTexture2D GLEW_GET_FUN(__glewFramebufferTexture2D)
#define glGenFramebuffers GLEW_GET_FUN(__glewGenFramebuffers)
#define glGenRenderbuffers GLEW_GET_FUN(__glewGenRenderbuffers)
#define glGenerateMipmap GLEW_GET_FUN(__glewGenerateMipmap)
#define glGetActiveAttrib GLEW_GET_FUN(__glewGetActiveAttrib)
#define glGetActiveUniform GLEW_GET_FUN(__glewGetActiveUniform)
#define glGetAttachedShaders GLEW_GET_FUN(__glewGetAttachedShaders)
#define glGetAttribLocation GLEW_GET_FUN(__glewGetAttribLocation)
#define glGetFramebufferAttachmentParameteriv GLEW_GET_FUN(__glewGetFramebufferAttachmentParameteriv)
#define glGetProgramInfoLog GLEW_GET_FUN(__glewGetProgramInfoLog)
#define glGetProgramiv GLEW_GET_FUN(__glewGetProgramiv)
#define glGetRenderbufferParameteriv GLEW_GET_FUN(__glewGetRenderbufferParameteriv)
#define glGetShaderInfoLog GLEW_GET_FUN(__glewGetShaderInfoLog)
#define glGetShaderPrecisionFormat GLEW_GET_FUN(__glewGetShaderPrecisionFormat)
#define glGetShaderSource GLEW_GET_FUN(__glewGetShaderSource)
#define glGetShaderiv GLEW_GET_FUN(__glewGetShaderiv)
#define glGetUniformLocation GLEW_GET_FUN(__glewGetUniformLocation)
#define glGetUniformfv GLEW_GET_FUN(__glewGetUniformfv)
#define glGetUniformiv GLEW_GET_FUN(__glewGetUniformiv)
#define glGetVertexAttribPointerv GLEW_GET_FUN(__glewGetVertexAttribPointerv)
#define glGetVertexAttribfv GLEW_GET_FUN(__glewGetVertexAttribfv)
#define glGetVertexAttribiv GLEW_GET_FUN(__glewGetVertexAttribiv)
#define glIsFramebuffer GLEW_GET_FUN(__glewIsFramebuffer)
#define glIsProgram GLEW_GET_FUN(__glewIsProgram)
#define glIsRenderbuffer GLEW_GET_FUN(__glewIsRenderbuffer)
#define glIsShader GLEW_GET_FUN(__glewIsShader)
#define glLinkProgram GLEW_GET_FUN(__glewLinkProgram)
#define glReleaseShaderCompiler GLEW_GET_FUN(__glewReleaseShaderCompiler)
#define glRenderbufferStorage GLEW_GET_FUN(__glewRenderbufferStorage)
#define glShaderBinary GLEW_GET_FUN(__glewShaderBinary)
#define glShaderSource GLEW_GET_FUN(__glewShaderSource)
#define glStencilFuncSeparate GLEW_GET_FUN(__glewStencilFuncSeparate)
#define glStencilMaskSeparate GLEW_GET_FUN(__glewStencilMaskSeparate)
#define glStencilOpSeparate GLEW_GET_FUN(__glewStencilOpSeparate)
#define glUniform1f GLEW_GET_FUN(__glewUniform1f)
#define glUniform1fv GLEW_GET_FUN(__glewUniform1fv)
#define glUniform1i GLEW_GET_FUN(__glewUniform1i)
#define glUniform1iv GLEW_GET_FUN(__glewUniform1iv)
#define glUniform2f GLEW_GET_FUN(__glewUniform2f)
#define glUniform2fv GLEW_GET_FUN(__glewUniform2fv)
#define glUniform2i GLEW_GET_FUN(__glewUniform2i)
#define glUniform2iv GLEW_GET_FUN(__glewUniform2iv)
#define glUniform3f GLEW_GET_FUN(__glewUniform3f)
#define glUniform3fv GLEW_GET_FUN(__glewUniform3fv)
#define glUniform3i GLEW_GET_FUN(__glewUniform3i)
#define glUniform3iv GLEW_GET_FUN(__glewUniform3iv)
#define glUniform4f GLEW_GET_FUN(__glewUniform4f)
#define glUniform4fv GLEW_GET_FUN(__glewUniform4fv)
#define glUniform4i GLEW_GET_FUN(__glewUniform4i)
#define glUniform4iv GLEW_GET_FUN(__glewUniform4iv)
#define glUniformMatrix2fv GLEW_GET_FUN(__glewUniformMatrix2fv)
#define glUniformMatrix3fv GLEW_GET_FUN(__glewUniformMatrix3fv)
#define glUniformMatrix4fv GLEW_GET_FUN(__glewUniformMatrix4fv)
#define glUseProgram GLEW_GET_FUN(__glewUseProgram)
#define glValidateProgram GLEW_GET_FUN(__glewValidateProgram)
#define glVertexAttrib1f GLEW_GET_FUN(__glewVertexAttrib1f)
#define glVertexAttrib1fv GLEW_GET_FUN(__glewVertexAttrib1fv)
#define glVertexAttrib2f GLEW_GET_FUN(__glewVertexAttrib2f)
#define glVertexAttrib2fv GLEW_GET_FUN(__glewVertexAttrib2fv)
#define glVertexAttrib3f GLEW_GET_FUN(__glewVertexAttrib3f)
#define glVertexAttrib3fv GLEW_GET_FUN(__glewVertexAttrib3fv)
#define glVertexAttrib4f GLEW_GET_FUN(__glewVertexAttrib4f)
#define glVertexAttrib4fv GLEW_GET_FUN(__glewVertexAttrib4fv)
#define glVertexAttribPointer GLEW_GET_FUN(__glewVertexAttribPointer)

#define glBindBuffer GLEW_GET_FUN(__glewBindBuffer) // NOTE jwilkins: missing function
#define glBufferData GLEW_GET_FUN(__glewBufferData) // NOTE jwilkins: missing function
#define glBufferSubData GLEW_GET_FUN(__glewBufferSubData) // NOTE jwilkins: missing function
#define glDeleteBuffers GLEW_GET_FUN(__glewDeleteBuffers) // NOTE jwilkins: missing function
#define glGenBuffers GLEW_GET_FUN(__glewGenBuffers) // NOTE jwilkins: missing function
#define glTexParameteri GLEW_GET_FUN(__glewTexParameteri) // NOTE jwilkins: missing function
#define glIsEnabled GLEW_GET_FUN(__glewIsEnabled) // NOTE jwilkins: missing function
#define glGetFloatv GLEW_GET_FUN(__glewGetFloatv) // NOTE jwilkins: missing function
#define glDepthRangef GLEW_GET_FUN(__glewDepthRangef) // NOTE jwilkins: missing function
#define glActiveTexture GLEW_GET_FUN(__glewActiveTexture) // NOTE jwilkins: missing function
#define glGetBooleanv GLEW_GET_FUN(__glewGetBooleanv) // NOTE jwilkins: missing function

#define GLEW_ES_VERSION_2_0 GLEW_GET_VAR(__GLEW_ES_VERSION_2_0)

#endif /* !GL_ES_VERSION_2_0 */

/* --------------------- GL_AMD_compressed_3DC_texture --------------------- */

#if !defined(GL_AMD_compressed_3DC_texture) 
#define GL_AMD_compressed_3DC_texture 1

#define GL_3DC_X_AMD 0x87F9
#define GL_3DC_XY_AMD 0x87FA

#define GLEW_AMD_compressed_3DC_texture GLEW_GET_VAR(__GLEW_AMD_compressed_3DC_texture)

#endif /* !GL_AMD_compressed_3DC_texture */

/* --------------------- GL_AMD_compressed_ATC_texture --------------------- */

#if !defined(GL_AMD_compressed_ATC_texture) 
#define GL_AMD_compressed_ATC_texture 1

#define GL_ATC_RGBA_INTERPOLATED_ALPHA_AMD 0x87EE
#define GL_ATC_RGB_AMD 0x8C92
#define GL_ATC_RGBA_EXPLICIT_ALPHA_AMD 0x8C93

#define GLEW_AMD_compressed_ATC_texture GLEW_GET_VAR(__GLEW_AMD_compressed_ATC_texture)

#endif /* !GL_AMD_compressed_ATC_texture */

/* ----------------------- GL_AMD_performance_monitor ---------------------- */

#if !defined(GL_AMD_performance_monitor) 
#define GL_AMD_performance_monitor 1

#define GL_UNSIGNED_INT 0x1405
#define GL_FLOAT 0x1406
#define GL_COUNTER_TYPE_AMD 0x8BC0
#define GL_COUNTER_RANGE_AMD 0x8BC1
#define GL_UNSIGNED_INT64_AMD 0x8BC2
#define GL_PERCENTAGE_AMD 0x8BC3
#define GL_PERFMON_RESULT_AVAILABLE_AMD 0x8BC4
#define GL_PERFMON_RESULT_SIZE_AMD 0x8BC5
#define GL_PERFMON_RESULT_AMD 0x8BC6

typedef void (GLAPIENTRY * PFNGLBEGINPERFMONITORAMDPROC) (GLuint monitor);
typedef void (GLAPIENTRY * PFNGLDELETEPERFMONITORSAMDPROC) (GLsizei n, GLuint* monitors);
typedef void (GLAPIENTRY * PFNGLENDPERFMONITORAMDPROC) (GLuint monitor);
typedef void (GLAPIENTRY * PFNGLGENPERFMONITORSAMDPROC) (GLsizei n, GLuint* monitors);
typedef void (GLAPIENTRY * PFNGLGETPERFMONITORCOUNTERDATAAMDPROC) (GLuint monitor, GLenum pname, GLsizei dataSize, GLuint* data, GLint *bytesWritten);
typedef void (GLAPIENTRY * PFNGLGETPERFMONITORCOUNTERINFOAMDPROC) (GLuint group, GLuint counter, GLenum pname, void* data);
typedef void (GLAPIENTRY * PFNGLGETPERFMONITORCOUNTERSTRINGAMDPROC) (GLuint group, GLuint counter, GLsizei bufSize, GLsizei* length, char *counterString);
typedef void (GLAPIENTRY * PFNGLGETPERFMONITORCOUNTERSAMDPROC) (GLuint group, GLint* numCounters, GLint *maxActiveCounters, GLsizei countersSize, GLuint *counters);
typedef void (GLAPIENTRY * PFNGLGETPERFMONITORGROUPSTRINGAMDPROC) (GLuint group, GLsizei bufSize, GLsizei* length, char *groupString);
typedef void (GLAPIENTRY * PFNGLGETPERFMONITORGROUPSAMDPROC) (GLint* numGroups, GLsizei groupsSize, GLuint *groups);
typedef void (GLAPIENTRY * PFNGLSELECTPERFMONITORCOUNTERSAMDPROC) (GLuint monitor, GLboolean enable, GLuint group, GLint numCounters, GLuint* counterList);

#define glBeginPerfMonitorAMD GLEW_GET_FUN(__glewBeginPerfMonitorAMD)
#define glDeletePerfMonitorsAMD GLEW_GET_FUN(__glewDeletePerfMonitorsAMD)
#define glEndPerfMonitorAMD GLEW_GET_FUN(__glewEndPerfMonitorAMD)
#define glGenPerfMonitorsAMD GLEW_GET_FUN(__glewGenPerfMonitorsAMD)
#define glGetPerfMonitorCounterDataAMD GLEW_GET_FUN(__glewGetPerfMonitorCounterDataAMD)
#define glGetPerfMonitorCounterInfoAMD GLEW_GET_FUN(__glewGetPerfMonitorCounterInfoAMD)
#define glGetPerfMonitorCounterStringAMD GLEW_GET_FUN(__glewGetPerfMonitorCounterStringAMD)
#define glGetPerfMonitorCountersAMD GLEW_GET_FUN(__glewGetPerfMonitorCountersAMD)
#define glGetPerfMonitorGroupStringAMD GLEW_GET_FUN(__glewGetPerfMonitorGroupStringAMD)
#define glGetPerfMonitorGroupsAMD GLEW_GET_FUN(__glewGetPerfMonitorGroupsAMD)
#define glSelectPerfMonitorCountersAMD GLEW_GET_FUN(__glewSelectPerfMonitorCountersAMD)

#define GLEW_AMD_performance_monitor GLEW_GET_VAR(__GLEW_AMD_performance_monitor)

#endif /* !GL_AMD_performance_monitor */

/* ----------------------- GL_AMD_program_binary_Z400 ---------------------- */

#if !defined(GL_AMD_program_binary_Z400) 
#define GL_AMD_program_binary_Z400 1

#define GL_Z400_BINARY_AMD 0x8740

#define GLEW_AMD_program_binary_Z400 GLEW_GET_VAR(__GLEW_AMD_program_binary_Z400)

#endif /* !GL_AMD_program_binary_Z400 */

/* ----------------------- GL_ANGLE_framebuffer_blit ----------------------- */

#if !defined(GL_ANGLE_framebuffer_blit) 
#define GL_ANGLE_framebuffer_blit 1

#define GL_DRAW_FRAMEBUFFER_BINDING_ANGLE 0x8CA6
#define GL_READ_FRAMEBUFFER_ANGLE 0x8CA8
#define GL_DRAW_FRAMEBUFFER_ANGLE 0x8CA9
#define GL_READ_FRAMEBUFFER_BINDING_ANGLE 0x8CAA

typedef void (GLAPIENTRY * PFNGLBLITFRAMEBUFFERANGLEPROC) (GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter);

#define glBlitFramebufferANGLE GLEW_GET_FUN(__glewBlitFramebufferANGLE)

#define GLEW_ANGLE_framebuffer_blit GLEW_GET_VAR(__GLEW_ANGLE_framebuffer_blit)

#endif /* !GL_ANGLE_framebuffer_blit */

/* -------------------- GL_ANGLE_framebuffer_multisample ------------------- */

#if !defined(GL_ANGLE_framebuffer_multisample) 
#define GL_ANGLE_framebuffer_multisample 1

#define GL_RENDERBUFFER_SAMPLES_ANGLE 0x8CAB
#define GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE_ANGLE 0x8D56
#define GL_MAX_SAMPLES_ANGLE 0x8D57

typedef void (GLAPIENTRY * PFNGLRENDERBUFFERSTORAGEMULTISAMPLEANGLEPROC) (GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height);

#define glRenderbufferStorageMultisampleANGLE GLEW_GET_FUN(__glewRenderbufferStorageMultisampleANGLE)

#define GLEW_ANGLE_framebuffer_multisample GLEW_GET_VAR(__GLEW_ANGLE_framebuffer_multisample)

#endif /* !GL_ANGLE_framebuffer_multisample */

/* ----------------------- GL_ANGLE_instanced_arrays ----------------------- */

#if !defined(GL_ANGLE_instanced_arrays) 
#define GL_ANGLE_instanced_arrays 1

#define GL_VERTEX_ATTRIB_ARRAY_DIVISOR_ANGLE 0x88FE

typedef void (GLAPIENTRY * PFNGLDRAWARRAYSINSTANCEDANGLEPROC) (GLenum mode, GLint first, GLsizei count, GLsizei primcount);
typedef void (GLAPIENTRY * PFNGLDRAWELEMENTSINSTANCEDANGLEPROC) (GLenum mode, GLsizei count, GLenum type, const void* indices, GLsizei primcount);
typedef void (GLAPIENTRY * PFNGLVERTEXATTRIBDIVISORANGLEPROC) (GLuint index, GLuint divisor);

#define glDrawArraysInstancedANGLE GLEW_GET_FUN(__glewDrawArraysInstancedANGLE)
#define glDrawElementsInstancedANGLE GLEW_GET_FUN(__glewDrawElementsInstancedANGLE)
#define glVertexAttribDivisorANGLE GLEW_GET_FUN(__glewVertexAttribDivisorANGLE)

#define GLEW_ANGLE_instanced_arrays GLEW_GET_VAR(__GLEW_ANGLE_instanced_arrays)

#endif /* !GL_ANGLE_instanced_arrays */

/* -------------------- GL_ANGLE_pack_reverse_row_order -------------------- */

#if !defined(GL_ANGLE_pack_reverse_row_order) 
#define GL_ANGLE_pack_reverse_row_order 1

#define GL_PACK_REVERSE_ROW_ORDER_ANGLE 0x93A4

#define GLEW_ANGLE_pack_reverse_row_order GLEW_GET_VAR(__GLEW_ANGLE_pack_reverse_row_order)

#endif /* !GL_ANGLE_pack_reverse_row_order */

/* ------------------- GL_ANGLE_texture_compression_dxt3 ------------------- */

#if !defined(GL_ANGLE_texture_compression_dxt3) 
#define GL_ANGLE_texture_compression_dxt3 1

#define GL_COMPRESSED_RGBA_S3TC_DXT3_ANGLE 0x83F2
#define GL_COMPRESSED_RGBA_S3TC_DXT5_ANGLE 0x83F3

#define GLEW_ANGLE_texture_compression_dxt3 GLEW_GET_VAR(__GLEW_ANGLE_texture_compression_dxt3)

#endif /* !GL_ANGLE_texture_compression_dxt3 */

/* ------------------- GL_ANGLE_texture_compression_dxt5 ------------------- */

#if !defined(GL_ANGLE_texture_compression_dxt5) 
#define GL_ANGLE_texture_compression_dxt5 1

#define GL_COMPRESSED_RGBA_S3TC_DXT3_ANGLE 0x83F2
#define GL_COMPRESSED_RGBA_S3TC_DXT5_ANGLE 0x83F3

#define GLEW_ANGLE_texture_compression_dxt5 GLEW_GET_VAR(__GLEW_ANGLE_texture_compression_dxt5)

#endif /* !GL_ANGLE_texture_compression_dxt5 */

/* ------------------------- GL_ANGLE_texture_usage ------------------------ */

#if !defined(GL_ANGLE_texture_usage) 
#define GL_ANGLE_texture_usage 1

#define GL_NONE 0  // NOTE jwilkins: had to change this from 0x0000 so it would match other definition of GL_NONE
#define GL_TEXTURE_USAGE_ANGLE 0x93A2
#define GL_FRAMEBUFFER_ATTACHMENT_ANGLE 0x93A3

#define GLEW_ANGLE_texture_usage GLEW_GET_VAR(__GLEW_ANGLE_texture_usage)

#endif /* !GL_ANGLE_texture_usage */

/* ------------------- GL_ANGLE_translated_shader_source ------------------- */

#if !defined(GL_ANGLE_translated_shader_source) 
#define GL_ANGLE_translated_shader_source 1

#define GL_TRANSLATED_SHADER_SOURCE_LENGTH_ANGLE 0x93A0

typedef void (GLAPIENTRY * PFNGLGETTRANSLATEDSHADERSOURCEANGLEPROC) (GLuint shader, GLsizei bufsize, GLsizei* length, char* source);

#define glGetTranslatedShaderSourceANGLE GLEW_GET_FUN(__glewGetTranslatedShaderSourceANGLE)

#define GLEW_ANGLE_translated_shader_source GLEW_GET_VAR(__GLEW_ANGLE_translated_shader_source)

#endif /* !GL_ANGLE_translated_shader_source */

/* ---------------------- GL_APPLE_copy_texture_levels --------------------- */

#if !defined(GL_APPLE_copy_texture_levels) 
#define GL_APPLE_copy_texture_levels 1

typedef void (GLAPIENTRY * PFNGLCOPYTEXTURELEVELSAPPLEPROC) (GLuint destinationTexture, GLuint sourceTexture, GLint sourceBaseLevel, GLsizei sourceLevelCount);

#define glCopyTextureLevelsAPPLE GLEW_GET_FUN(__glewCopyTextureLevelsAPPLE)

#define GLEW_APPLE_copy_texture_levels GLEW_GET_VAR(__GLEW_APPLE_copy_texture_levels)

#endif /* !GL_APPLE_copy_texture_levels */

/* -------------------- GL_APPLE_framebuffer_multisample ------------------- */

#if !defined(GL_APPLE_framebuffer_multisample) 
#define GL_APPLE_framebuffer_multisample 1

#define GL_DRAW_FRAMEBUFFER_BINDING_APPLE 0x8CA6
#define GL_READ_FRAMEBUFFER_APPLE 0x8CA8
#define GL_DRAW_FRAMEBUFFER_APPLE 0x8CA9
#define GL_READ_FRAMEBUFFER_BINDING_APPLE 0x8CAA
#define GL_RENDERBUFFER_SAMPLES_APPLE 0x8CAB
#define GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE_APPLE 0x8D56
#define GL_MAX_SAMPLES_APPLE 0x8D57

typedef void (GLAPIENTRY * PFNGLRENDERBUFFERSTORAGEMULTISAMPLEAPPLEPROC) (GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height);
typedef void (GLAPIENTRY * PFNGLRESOLVEMULTISAMPLEFRAMEBUFFERAPPLEPROC) (void);

#define glRenderbufferStorageMultisampleAPPLE GLEW_GET_FUN(__glewRenderbufferStorageMultisampleAPPLE)
#define glResolveMultisampleFramebufferAPPLE GLEW_GET_FUN(__glewResolveMultisampleFramebufferAPPLE)

#define GLEW_APPLE_framebuffer_multisample GLEW_GET_VAR(__GLEW_APPLE_framebuffer_multisample)

#endif /* !GL_APPLE_framebuffer_multisample */

/* ---------------------------- GL_APPLE_rgb_422 --------------------------- */

#if !defined(GL_APPLE_rgb_422) 
#define GL_APPLE_rgb_422 1

#define GL_UNSIGNED_SHORT_8_8_APPLE 0x85BA
#define GL_UNSIGNED_SHORT_8_8_REV_APPLE 0x85BB
#define GL_RGB_422_APPLE 0x8A1F

#define GLEW_APPLE_rgb_422 GLEW_GET_VAR(__GLEW_APPLE_rgb_422)

#endif /* !GL_APPLE_rgb_422 */

/* ----------------------------- GL_APPLE_sync ----------------------------- */

#if !defined(GL_APPLE_sync) 
#define GL_APPLE_sync 1

#define GL_SYNC_FLUSH_COMMANDS_BIT_APPLE 0x00000001
#define GL_SYNC_OBJECT_APPLE 0x8A53
#define GL_MAX_SERVER_WAIT_TIMEOUT_APPLE 0x9111
#define GL_OBJECT_TYPE_APPLE 0x9112
#define GL_SYNC_CONDITION_APPLE 0x9113
#define GL_SYNC_STATUS_APPLE 0x9114
#define GL_SYNC_FLAGS_APPLE 0x9115
#define GL_SYNC_FENCE_APPLE 0x9116
#define GL_SYNC_GPU_COMMANDS_COMPLETE_APPLE 0x9117
#define GL_UNSIGNALED_APPLE 0x9118
#define GL_SIGNALED_APPLE 0x9119
#define GL_ALREADY_SIGNALED_APPLE 0x911A
#define GL_TIMEOUT_EXPIRED_APPLE 0x911B
#define GL_CONDITION_SATISFIED_APPLE 0x911C
#define GL_WAIT_FAILED_APPLE 0x911D
#define GL_TIMEOUT_IGNORED_APPLE 0xFFFFFFFFFFFFFFFF

typedef GLenum (GLAPIENTRY * PFNGLCLIENTWAITSYNCAPPLEPROC) (GLsync GLsync, GLbitfield flags, GLuint64 timeout);
typedef void (GLAPIENTRY * PFNGLDELETESYNCAPPLEPROC) (GLsync GLsync);
typedef GLsync (GLAPIENTRY * PFNGLFENCESYNCAPPLEPROC) (GLenum condition, GLbitfield flags);
typedef void (GLAPIENTRY * PFNGLGETINTEGER64VAPPLEPROC) (GLenum pname, GLint64* params);
typedef void (GLAPIENTRY * PFNGLGETSYNCIVAPPLEPROC) (GLsync GLsync, GLenum pname, GLsizei bufSize, GLsizei* length, GLint *values);
typedef GLboolean (GLAPIENTRY * PFNGLISSYNCAPPLEPROC) (GLsync GLsync);
typedef void (GLAPIENTRY * PFNGLWAITSYNCAPPLEPROC) (GLsync GLsync, GLbitfield flags, GLuint64 timeout);

#define glClientWaitSyncAPPLE GLEW_GET_FUN(__glewClientWaitSyncAPPLE)
#define glDeleteSyncAPPLE GLEW_GET_FUN(__glewDeleteSyncAPPLE)
#define glFenceSyncAPPLE GLEW_GET_FUN(__glewFenceSyncAPPLE)
#define glGetInteger64vAPPLE GLEW_GET_FUN(__glewGetInteger64vAPPLE)
#define glGetSyncivAPPLE GLEW_GET_FUN(__glewGetSyncivAPPLE)
#define glIsSyncAPPLE GLEW_GET_FUN(__glewIsSyncAPPLE)
#define glWaitSyncAPPLE GLEW_GET_FUN(__glewWaitSyncAPPLE)

#define GLEW_APPLE_sync GLEW_GET_VAR(__GLEW_APPLE_sync)

#endif /* !GL_APPLE_sync */

/* -------------------- GL_APPLE_texture_2D_limited_npot ------------------- */

#if !defined(GL_APPLE_texture_2D_limited_npot) 
#define GL_APPLE_texture_2D_limited_npot 1

#define GLEW_APPLE_texture_2D_limited_npot GLEW_GET_VAR(__GLEW_APPLE_texture_2D_limited_npot)

#endif /* !GL_APPLE_texture_2D_limited_npot */

/* -------------------- GL_APPLE_texture_format_BGRA8888 ------------------- */

#if !defined(GL_APPLE_texture_format_BGRA8888) 
#define GL_APPLE_texture_format_BGRA8888 1

#define GL_BGRA_EXT 0x80E1

#define GLEW_APPLE_texture_format_BGRA8888 GLEW_GET_VAR(__GLEW_APPLE_texture_format_BGRA8888)

#endif /* !GL_APPLE_texture_format_BGRA8888 */

/* ----------------------- GL_APPLE_texture_max_level ---------------------- */

#if !defined(GL_APPLE_texture_max_level) 
#define GL_APPLE_texture_max_level 1

#define GL_TEXTURE_MAX_LEVEL_APPLE 0x813D

#define GLEW_APPLE_texture_max_level GLEW_GET_VAR(__GLEW_APPLE_texture_max_level)

#endif /* !GL_APPLE_texture_max_level */

/* ----------------------- GL_ARM_mali_program_binary ---------------------- */

#if !defined(GL_ARM_mali_program_binary) 
#define GL_ARM_mali_program_binary 1

#define GL_MALI_PROGRAM_BINARY_ARM 0x8F61

#define GLEW_ARM_mali_program_binary GLEW_GET_VAR(__GLEW_ARM_mali_program_binary)

#endif /* !GL_ARM_mali_program_binary */

/* ----------------------- GL_ARM_mali_shader_binary ----------------------- */

#if !defined(GL_ARM_mali_shader_binary) 
#define GL_ARM_mali_shader_binary 1

#define GL_MALI_SHADER_BINARY_ARM 0x8F60

#define GLEW_ARM_mali_shader_binary GLEW_GET_VAR(__GLEW_ARM_mali_shader_binary)

#endif /* !GL_ARM_mali_shader_binary */

/* ------------------------------ GL_ARM_rgba8 ----------------------------- */

#if !defined(GL_ARM_rgba8) 
#define GL_ARM_rgba8 1

#define GL_RGBA8_OES 0x8058

#define GLEW_ARM_rgba8 GLEW_GET_VAR(__GLEW_ARM_rgba8)

#endif /* !GL_ARM_rgba8 */

/* -------------------------- GL_DMP_shader_binary ------------------------- */

#if !defined(GL_DMP_shader_binary) 
#define GL_DMP_shader_binary 1

#define GL_SHADER_BINARY_DMP 0x9250

#define GLEW_DMP_shader_binary GLEW_GET_VAR(__GLEW_DMP_shader_binary)

#endif /* !GL_DMP_shader_binary */

/* -------------------------- GL_EXT_blend_minmax -------------------------- */

#if !defined(GL_EXT_blend_minmax) 
#define GL_EXT_blend_minmax 1

#define GL_FUNC_ADD_EXT 0x8006
#define GL_MIN_EXT 0x8007
#define GL_MAX_EXT 0x8008
#define GL_BLEND_EQUATION_EXT 0x8009

typedef void (GLAPIENTRY * PFNGLBLENDEQUATIONEXTPROC) (GLenum mode);

#define glBlendEquationEXT GLEW_GET_FUN(__glewBlendEquationEXT)

#define GLEW_EXT_blend_minmax GLEW_GET_VAR(__GLEW_EXT_blend_minmax)

#endif /* !GL_EXT_blend_minmax */

/* --------------------- GL_EXT_color_buffer_half_float -------------------- */

#if !defined(GL_EXT_color_buffer_half_float) 
#define GL_EXT_color_buffer_half_float 1

#define GL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE_EXT 0x8211
#define GL_R16F_EXT 0x822D
#define GL_RG16F_EXT 0x822F
#define GL_RGBA16F_EXT 0x881A
#define GL_RGB16F_EXT 0x881B
#define GL_UNSIGNED_NORMALIZED_EXT 0x8C17

#define GLEW_EXT_color_buffer_half_float GLEW_GET_VAR(__GLEW_EXT_color_buffer_half_float)

#endif /* !GL_EXT_color_buffer_half_float */

/* --------------------------- GL_EXT_debug_label -------------------------- */

#if !defined(GL_EXT_debug_label) 
#define GL_EXT_debug_label 1

#define GL_PROGRAM_PIPELINE_OBJECT_EXT 0x8A4F
#define GL_PROGRAM_OBJECT_EXT 0x8B40
#define GL_SHADER_OBJECT_EXT 0x8B48
#define GL_BUFFER_OBJECT_EXT 0x9151
#define GL_QUERY_OBJECT_EXT 0x9153
#define GL_VERTEX_ARRAY_OBJECT_EXT 0x9154

typedef void (GLAPIENTRY * PFNGLGETOBJECTLABELEXTPROC) (GLenum type, GLuint object, GLsizei bufSize, GLsizei* length, char *label);
typedef void (GLAPIENTRY * PFNGLLABELOBJECTEXTPROC) (GLenum type, GLuint object, GLsizei length, const char* label);

#define glGetObjectLabelEXT GLEW_GET_FUN(__glewGetObjectLabelEXT)
#define glLabelObjectEXT GLEW_GET_FUN(__glewLabelObjectEXT)

#define GLEW_EXT_debug_label GLEW_GET_VAR(__GLEW_EXT_debug_label)

#endif /* !GL_EXT_debug_label */

/* -------------------------- GL_EXT_debug_marker -------------------------- */

#if !defined(GL_EXT_debug_marker) 
#define GL_EXT_debug_marker 1

typedef void (GLAPIENTRY * PFNGLINSERTEVENTMARKEREXTPROC) (GLsizei length, const char* marker);
typedef void (GLAPIENTRY * PFNGLPUSHGROUPMARKEREXTPROC) (GLsizei length, const char* marker);

#define glInsertEventMarkerEXT GLEW_GET_FUN(__glewInsertEventMarkerEXT)
#define glPushGroupMarkerEXT GLEW_GET_FUN(__glewPushGroupMarkerEXT)

#define GLEW_EXT_debug_marker GLEW_GET_VAR(__GLEW_EXT_debug_marker)

#endif /* !GL_EXT_debug_marker */

/* ----------------------- GL_EXT_discard_framebuffer ---------------------- */

#if !defined(GL_EXT_discard_framebuffer) 
#define GL_EXT_discard_framebuffer 1

#define GL_COLOR_EXT 0x1800
#define GL_DEPTH_EXT 0x1801
#define GL_STENCIL_EXT 0x1802

typedef void (GLAPIENTRY * PFNGLDISCARDFRAMEBUFFEREXTPROC) (GLenum target, GLsizei numAttachments, const GLenum* attachments);

#define glDiscardFramebufferEXT GLEW_GET_FUN(__glewDiscardFramebufferEXT)

#define GLEW_EXT_discard_framebuffer GLEW_GET_VAR(__GLEW_EXT_discard_framebuffer)

#endif /* !GL_EXT_discard_framebuffer */

/* --------------------------- GL_EXT_frag_depth --------------------------- */

#if !defined(GL_EXT_frag_depth) 
#define GL_EXT_frag_depth 1

#define GLEW_EXT_frag_depth GLEW_GET_VAR(__GLEW_EXT_frag_depth)

#endif /* !GL_EXT_frag_depth */

/* ------------------------ GL_EXT_map_buffer_range ------------------------ */

#if !defined(GL_EXT_map_buffer_range) 
#define GL_EXT_map_buffer_range 1

#define GL_MAP_READ_BIT_EXT 0x0001
#define GL_MAP_WRITE_BIT_EXT 0x0002
#define GL_MAP_INVALIDATE_RANGE_BIT_EXT 0x0004
#define GL_MAP_INVALIDATE_BUFFER_BIT_EXT 0x0008
#define GL_MAP_FLUSH_EXPLICIT_BIT_EXT 0x0010
#define GL_MAP_UNSYNCHRONIZED_BIT_EXT 0x0020

typedef void (GLAPIENTRY * PFNGLFLUSHMAPPEDBUFFERRANGEEXTPROC) (GLenum target, GLintptr offset, GLsizeiptr length);
typedef GLvoid * (GLAPIENTRY * PFNGLMAPBUFFERRANGEEXTPROC) (GLenum target, GLintptr offset, GLsizeiptr length, GLbitfield access);

#define glFlushMappedBufferRangeEXT GLEW_GET_FUN(__glewFlushMappedBufferRangeEXT)
#define glMapBufferRangeEXT GLEW_GET_FUN(__glewMapBufferRangeEXT)

#define GLEW_EXT_map_buffer_range GLEW_GET_VAR(__GLEW_EXT_map_buffer_range)

#endif /* !GL_EXT_map_buffer_range */

/* ------------------------ GL_EXT_multi_draw_arrays ----------------------- */

#if !defined(GL_EXT_multi_draw_arrays) 
#define GL_EXT_multi_draw_arrays 1

typedef void (GLAPIENTRY * PFNGLMULTIDRAWARRAYSEXTPROC) (GLenum mode, const GLint* first, const GLsizei *count, GLsizei primcount);
typedef void (GLAPIENTRY * PFNGLMULTIDRAWELEMENTSEXTPROC) (GLenum mode, GLsizei* count, GLenum type, const GLvoid **indices, GLsizei primcount);

#define glMultiDrawArraysEXT GLEW_GET_FUN(__glewMultiDrawArraysEXT)
#define glMultiDrawElementsEXT GLEW_GET_FUN(__glewMultiDrawElementsEXT)

#define GLEW_EXT_multi_draw_arrays GLEW_GET_VAR(__GLEW_EXT_multi_draw_arrays)

#endif /* !GL_EXT_multi_draw_arrays */

/* ----------------- GL_EXT_multisampled_render_to_texture ----------------- */

#if !defined(GL_EXT_multisampled_render_to_texture) 
#define GL_EXT_multisampled_render_to_texture 1

#define GL_RENDERBUFFER_SAMPLES_EXT 0x8CAB
#define GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE_EXT 0x8D56
#define GL_MAX_SAMPLES_EXT 0x8D57
#define GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_SAMPLES_EXT 0x8D6C

typedef void (GLAPIENTRY * PFNGLFRAMEBUFFERTEXTURE2DMULTISAMPLEEXTPROC) (GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level, GLsizei samples);
typedef void (GLAPIENTRY * PFNGLRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC) (GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height);

#define glFramebufferTexture2DMultisampleEXT GLEW_GET_FUN(__glewFramebufferTexture2DMultisampleEXT)
#define glRenderbufferStorageMultisampleEXT GLEW_GET_FUN(__glewRenderbufferStorageMultisampleEXT)

#define GLEW_EXT_multisampled_render_to_texture GLEW_GET_VAR(__GLEW_EXT_multisampled_render_to_texture)

#endif /* !GL_EXT_multisampled_render_to_texture */

/* --------------------- GL_EXT_multiview_draw_buffers --------------------- */

#if !defined(GL_EXT_multiview_draw_buffers) 
#define GL_EXT_multiview_draw_buffers 1

#define GL_DRAW_BUFFER_EXT 0x0C01
#define GL_READ_BUFFER_EXT 0x0C02
#define GL_COLOR_ATTACHMENT_EXT 0x90F0
#define GL_MULTIVIEW_EXT 0x90F1
#define GL_MAX_MULTIVIEW_BUFFERS_EXT 0x90F2

typedef void (GLAPIENTRY * PFNGLDRAWBUFFERSINDEXEDEXTPROC) (GLint n, const GLenum* location, const GLint *indices);
typedef void (GLAPIENTRY * PFNGLGETINTEGERI_VEXTPROC) (GLenum target, GLuint index, GLint* data);
typedef void (GLAPIENTRY * PFNGLREADBUFFERINDEXEDEXTPROC) (GLenum src, GLint index);

#define glDrawBuffersIndexedEXT GLEW_GET_FUN(__glewDrawBuffersIndexedEXT)
#define glGetIntegeri_vEXT GLEW_GET_FUN(__glewGetIntegeri_vEXT)
#define glReadBufferIndexedEXT GLEW_GET_FUN(__glewReadBufferIndexedEXT)

#define GLEW_EXT_multiview_draw_buffers GLEW_GET_VAR(__GLEW_EXT_multiview_draw_buffers)

#endif /* !GL_EXT_multiview_draw_buffers */

/* --------------------- GL_EXT_occlusion_query_boolean -------------------- */

#if !defined(GL_EXT_occlusion_query_boolean) 
#define GL_EXT_occlusion_query_boolean 1

#define GL_CURRENT_QUERY_EXT 0x8865
#define GL_QUERY_RESULT_EXT 0x8866
#define GL_QUERY_RESULT_AVAILABLE_EXT 0x8867
#define GL_ANY_SAMPLES_PASSED_EXT 0x8C2F
#define GL_ANY_SAMPLES_PASSED_CONSERVATIVE_EXT 0x8D6A

typedef void (GLAPIENTRY * PFNGLBEGINQUERYEXTPROC) (GLenum target, GLuint id);
typedef void (GLAPIENTRY * PFNGLDELETEQUERIESEXTPROC) (GLsizei n, const GLuint* ids);
typedef void (GLAPIENTRY * PFNGLENDQUERYEXTPROC) (GLenum target);
typedef void (GLAPIENTRY * PFNGLGENQUERIESEXTPROC) (GLsizei n, GLuint* ids);
typedef void (GLAPIENTRY * PFNGLGETQUERYOBJECTUIVEXTPROC) (GLuint id, GLenum pname, GLuint* params);
typedef void (GLAPIENTRY * PFNGLGETQUERYIVEXTPROC) (GLenum target, GLenum pname, GLint* params);
typedef GLboolean (GLAPIENTRY * PFNGLISQUERYEXTPROC) (GLuint id);

#define glBeginQueryEXT GLEW_GET_FUN(__glewBeginQueryEXT)
#define glDeleteQueriesEXT GLEW_GET_FUN(__glewDeleteQueriesEXT)
#define glEndQueryEXT GLEW_GET_FUN(__glewEndQueryEXT)
#define glGenQueriesEXT GLEW_GET_FUN(__glewGenQueriesEXT)
#define glGetQueryObjectuivEXT GLEW_GET_FUN(__glewGetQueryObjectuivEXT)
#define glGetQueryivEXT GLEW_GET_FUN(__glewGetQueryivEXT)
#define glIsQueryEXT GLEW_GET_FUN(__glewIsQueryEXT)

#define GLEW_EXT_occlusion_query_boolean GLEW_GET_VAR(__GLEW_EXT_occlusion_query_boolean)

#endif /* !GL_EXT_occlusion_query_boolean */

/* ------------------------ GL_EXT_read_format_bgra ------------------------ */

#if !defined(GL_EXT_read_format_bgra) 
#define GL_EXT_read_format_bgra 1

#define GL_BGRA_EXT 0x80E1
#define GL_UNSIGNED_SHORT_4_4_4_4_REV_EXT 0x8365
#define GL_UNSIGNED_SHORT_1_5_5_5_REV_EXT 0x8366

#define GLEW_EXT_read_format_bgra GLEW_GET_VAR(__GLEW_EXT_read_format_bgra)

#endif /* !GL_EXT_read_format_bgra */

/* --------------------------- GL_EXT_robustness --------------------------- */

#if !defined(GL_EXT_robustness) 
#define GL_EXT_robustness 1

#define GL_NO_ERROR 0 // NOTE jwilkins: had to change this from 0x0000 so it would math other definition
#define GL_LOSE_CONTEXT_ON_RESET_EXT 0x8252
#define GL_GUILTY_CONTEXT_RESET_EXT 0x8253
#define GL_INNOCENT_CONTEXT_RESET_EXT 0x8254
#define GL_UNKNOWN_CONTEXT_RESET_EXT 0x8255
#define GL_RESET_NOTIFICATION_STRATEGY_EXT 0x8256
#define GL_NO_RESET_NOTIFICATION_EXT 0x8261
#define GL_CONTEXT_ROBUST_ACCESS_EXT 0x90F3

typedef void (GLAPIENTRY * PFNGLGETNUNIFORMFVEXTPROC) (GLuint program, GLint location, GLsizei bufSize, GLfloat* params);
typedef void (GLAPIENTRY * PFNGLGETNUNIFORMIVEXTPROC) (GLuint program, GLint location, GLsizei bufSize, GLint* params);
typedef void (GLAPIENTRY * PFNGLREADNPIXELSEXTPROC) (GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLsizei bufSize, void* data);

#define glGetnUniformfvEXT GLEW_GET_FUN(__glewGetnUniformfvEXT)
#define glGetnUniformivEXT GLEW_GET_FUN(__glewGetnUniformivEXT)
#define glReadnPixelsEXT GLEW_GET_FUN(__glewReadnPixelsEXT)

#define GLEW_EXT_robustness GLEW_GET_VAR(__GLEW_EXT_robustness)

#endif /* !GL_EXT_robustness */

/* ------------------------------ GL_EXT_sRGB ------------------------------ */

#if !defined(GL_EXT_sRGB) 
#define GL_EXT_sRGB 1

#define GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING_EXT 0x8210
#define GL_SRGB_EXT 0x8C40
#define GL_SRGB_ALPHA_EXT 0x8C42
#define GL_SRGB8_ALPHA8_EXT 0x8C43

#define GLEW_EXT_sRGB GLEW_GET_VAR(__GLEW_EXT_sRGB)

#endif /* !GL_EXT_sRGB */

#if 0 // NOTE jwilkins: there is an inconsistency between the ES and Non-ES versions of this extension??
/* --------------------- GL_EXT_separate_shader_objects -------------------- */

#if !defined(GL_EXT_separate_shader_objects) 
#define GL_EXT_separate_shader_objects 1

#define GL_VERTEX_SHADER_BIT_EXT 0x00000001
#define GL_FRAGMENT_SHADER_BIT_EXT 0x00000002
#define GL_PROGRAM_SEPARABLE_EXT 0x8258
#define GL_ACTIVE_PROGRAM_EXT 0x8259
#define GL_PROGRAM_PIPELINE_BINDING_EXT 0x825A
#define GL_ALL_SHADER_BITS_EXT 0xFFFFFFFF

typedef void (GLAPIENTRY * PFNGLACTIVESHADERPROGRAMEXTPROC) (GLuint pipeline, GLuint program);
typedef void (GLAPIENTRY * PFNGLBINDPROGRAMPIPELINEEXTPROC) (GLuint pipeline);
typedef GLuint (GLAPIENTRY * PFNGLCREATESHADERPROGRAMVEXTPROC) (GLenum type, GLsizei count, const char ** strings);
typedef void (GLAPIENTRY * PFNGLDELETEPROGRAMPIPELINESEXTPROC) (GLsizei n, const GLuint* pipelines);
typedef void (GLAPIENTRY * PFNGLGENPROGRAMPIPELINESEXTPROC) (GLsizei n, GLuint* pipelines);
typedef void (GLAPIENTRY * PFNGLGETPROGRAMPIPELINEINFOLOGEXTPROC) (GLuint pipeline, GLsizei bufSize, GLsizei* length, char *infoLog);
typedef void (GLAPIENTRY * PFNGLGETPROGRAMPIPELINEIVEXTPROC) (GLuint pipeline, GLenum pname, GLint* params);
typedef GLboolean (GLAPIENTRY * PFNGLISPROGRAMPIPELINEEXTPROC) (GLuint pipeline);
typedef void (GLAPIENTRY * PFNGLPROGRAMPARAMETERIEXTPROC) (GLuint program, GLenum pname, GLint value);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORM1FEXTPROC) (GLuint program, GLint location, GLfloat x);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORM1FVEXTPROC) (GLuint program, GLint location, GLsizei count, const GLfloat* value);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORM1IEXTPROC) (GLuint program, GLint location, GLint x);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORM1IVEXTPROC) (GLuint program, GLint location, GLsizei count, const GLint* value);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORM2FEXTPROC) (GLuint program, GLint location, GLfloat x, GLfloat y);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORM2FVEXTPROC) (GLuint program, GLint location, GLsizei count, const GLfloat* value);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORM2IEXTPROC) (GLuint program, GLint location, GLint x, GLint y);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORM2IVEXTPROC) (GLuint program, GLint location, GLsizei count, const GLint* value);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORM3FEXTPROC) (GLuint program, GLint location, GLfloat x, GLfloat y, GLfloat z);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORM3FVEXTPROC) (GLuint program, GLint location, GLsizei count, const GLfloat* value);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORM3IEXTPROC) (GLuint program, GLint location, GLint x, GLint y, GLint z);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORM3IVEXTPROC) (GLuint program, GLint location, GLsizei count, const GLint* value);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORM4FEXTPROC) (GLuint program, GLint location, GLfloat x, GLfloat y, GLfloat z, GLfloat w);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORM4FVEXTPROC) (GLuint program, GLint location, GLsizei count, const GLfloat* value);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORM4IEXTPROC) (GLuint program, GLint location, GLint x, GLint y, GLint z, GLint w);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORM4IVEXTPROC) (GLuint program, GLint location, GLsizei count, const GLint* value);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORMMATRIX2FVEXTPROC) (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORMMATRIX3FVEXTPROC) (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
typedef void (GLAPIENTRY * PFNGLPROGRAMUNIFORMMATRIX4FVEXTPROC) (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
typedef void (GLAPIENTRY * PFNGLUSEPROGRAMSTAGESEXTPROC) (GLuint pipeline, GLbitfield stages, GLuint program);
typedef void (GLAPIENTRY * PFNGLVALIDATEPROGRAMPIPELINEEXTPROC) (GLuint pipeline);

#define glActiveShaderProgramEXT GLEW_GET_FUN(__glewActiveShaderProgramEXT)
#define glBindProgramPipelineEXT GLEW_GET_FUN(__glewBindProgramPipelineEXT)
#define glCreateShaderProgramvEXT GLEW_GET_FUN(__glewCreateShaderProgramvEXT)
#define glDeleteProgramPipelinesEXT GLEW_GET_FUN(__glewDeleteProgramPipelinesEXT)
#define glGenProgramPipelinesEXT GLEW_GET_FUN(__glewGenProgramPipelinesEXT)
#define glGetProgramPipelineInfoLogEXT GLEW_GET_FUN(__glewGetProgramPipelineInfoLogEXT)
#define glGetProgramPipelineivEXT GLEW_GET_FUN(__glewGetProgramPipelineivEXT)
#define glIsProgramPipelineEXT GLEW_GET_FUN(__glewIsProgramPipelineEXT)
#define glProgramParameteriEXT GLEW_GET_FUN(__glewProgramParameteriEXT)
#define glProgramUniform1fEXT GLEW_GET_FUN(__glewProgramUniform1fEXT)
#define glProgramUniform1fvEXT GLEW_GET_FUN(__glewProgramUniform1fvEXT)
#define glProgramUniform1iEXT GLEW_GET_FUN(__glewProgramUniform1iEXT)
#define glProgramUniform1ivEXT GLEW_GET_FUN(__glewProgramUniform1ivEXT)
#define glProgramUniform2fEXT GLEW_GET_FUN(__glewProgramUniform2fEXT)
#define glProgramUniform2fvEXT GLEW_GET_FUN(__glewProgramUniform2fvEXT)
#define glProgramUniform2iEXT GLEW_GET_FUN(__glewProgramUniform2iEXT)
#define glProgramUniform2ivEXT GLEW_GET_FUN(__glewProgramUniform2ivEXT)
#define glProgramUniform3fEXT GLEW_GET_FUN(__glewProgramUniform3fEXT)
#define glProgramUniform3fvEXT GLEW_GET_FUN(__glewProgramUniform3fvEXT)
#define glProgramUniform3iEXT GLEW_GET_FUN(__glewProgramUniform3iEXT)
#define glProgramUniform3ivEXT GLEW_GET_FUN(__glewProgramUniform3ivEXT)
#define glProgramUniform4fEXT GLEW_GET_FUN(__glewProgramUniform4fEXT)
#define glProgramUniform4fvEXT GLEW_GET_FUN(__glewProgramUniform4fvEXT)
#define glProgramUniform4iEXT GLEW_GET_FUN(__glewProgramUniform4iEXT)
#define glProgramUniform4ivEXT GLEW_GET_FUN(__glewProgramUniform4ivEXT)
#define glProgramUniformMatrix2fvEXT GLEW_GET_FUN(__glewProgramUniformMatrix2fvEXT)
#define glProgramUniformMatrix3fvEXT GLEW_GET_FUN(__glewProgramUniformMatrix3fvEXT)
#define glProgramUniformMatrix4fvEXT GLEW_GET_FUN(__glewProgramUniformMatrix4fvEXT)
#define glUseProgramStagesEXT GLEW_GET_FUN(__glewUseProgramStagesEXT)
#define glValidateProgramPipelineEXT GLEW_GET_FUN(__glewValidateProgramPipelineEXT)

#define GLEW_EXT_separate_shader_objects GLEW_GET_VAR(__GLEW_EXT_separate_shader_objects)

#endif /* !GL_EXT_separate_shader_objects */
#endif // XXX

/* -------------------- GL_EXT_shader_framebuffer_fetch -------------------- */

#if !defined(GL_EXT_shader_framebuffer_fetch) 
#define GL_EXT_shader_framebuffer_fetch 1

#define GL_FRAGMENT_SHADER_DISCARDS_SAMPLES_EXT 0x8A52

#define GLEW_EXT_shader_framebuffer_fetch GLEW_GET_VAR(__GLEW_EXT_shader_framebuffer_fetch)

#endif /* !GL_EXT_shader_framebuffer_fetch */

/* ----------------------- GL_EXT_shader_texture_lod ----------------------- */

#if !defined(GL_EXT_shader_texture_lod) 
#define GL_EXT_shader_texture_lod 1

#define GLEW_EXT_shader_texture_lod GLEW_GET_VAR(__GLEW_EXT_shader_texture_lod)

#endif /* !GL_EXT_shader_texture_lod */

/* ------------------------- GL_EXT_shadow_samplers ------------------------ */

#if !defined(GL_EXT_shadow_samplers) 
#define GL_EXT_shadow_samplers 1

#define GL_TEXTURE_COMPARE_MODE_EXT 0x884C
#define GL_TEXTURE_COMPARE_FUNC_EXT 0x884D
#define GL_COMPARE_REF_TO_TEXTURE_EXT 0x884E
#define GL_SAMPLER_2D_SHADOW_EXT 0x8B62

#define GLEW_EXT_shadow_samplers GLEW_GET_VAR(__GLEW_EXT_shadow_samplers)

#endif /* !GL_EXT_shadow_samplers */

/* -------------------- GL_EXT_texture_compression_dxt1 -------------------- */

#if !defined(GL_EXT_texture_compression_dxt1) 
#define GL_EXT_texture_compression_dxt1 1

#define GL_COMPRESSED_RGB_S3TC_DXT1_EXT 0x83F0
#define GL_COMPRESSED_RGBA_S3TC_DXT1_EXT 0x83F1

#define GLEW_EXT_texture_compression_dxt1 GLEW_GET_VAR(__GLEW_EXT_texture_compression_dxt1)

#endif /* !GL_EXT_texture_compression_dxt1 */

/* ------------------- GL_EXT_texture_filter_anisotropic ------------------- */

#if !defined(GL_EXT_texture_filter_anisotropic) 
#define GL_EXT_texture_filter_anisotropic 1

#define GL_TEXTURE_MAX_ANISOTROPY_EXT 0x84FE
#define GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT 0x84FF

#define GLEW_EXT_texture_filter_anisotropic GLEW_GET_VAR(__GLEW_EXT_texture_filter_anisotropic)

#endif /* !GL_EXT_texture_filter_anisotropic */

/* --------------------- GL_EXT_texture_format_BGRA8888 -------------------- */

#if !defined(GL_EXT_texture_format_BGRA8888) 
#define GL_EXT_texture_format_BGRA8888 1

#define GL_BGRA_EXT 0x80E1

#define GLEW_EXT_texture_format_BGRA8888 GLEW_GET_VAR(__GLEW_EXT_texture_format_BGRA8888)

#endif /* !GL_EXT_texture_format_BGRA8888 */

/* ------------------------ GL_EXT_texture_lod_bias ------------------------ */

#if !defined(GL_EXT_texture_lod_bias) 
#define GL_EXT_texture_lod_bias 1

#define GL_MAX_TEXTURE_LOD_BIAS_EXT 0x84FD
#define GL_TEXTURE_FILTER_CONTROL_EXT 0x8500
#define GL_TEXTURE_LOD_BIAS_EXT 0x8501

#define GLEW_EXT_texture_lod_bias GLEW_GET_VAR(__GLEW_EXT_texture_lod_bias)

#endif /* !GL_EXT_texture_lod_bias */

/* --------------------------- GL_EXT_texture_rg --------------------------- */

#if !defined(GL_EXT_texture_rg) 
#define GL_EXT_texture_rg 1

#define GL_RED_EXT 0x1903
#define GL_RG_EXT 0x8227
#define GL_R8_EXT 0x8229
#define GL_RG8_EXT 0x822B

#define GLEW_EXT_texture_rg GLEW_GET_VAR(__GLEW_EXT_texture_rg)

#endif /* !GL_EXT_texture_rg */

/* ------------------------- GL_EXT_texture_storage ------------------------ */

#if !defined(GL_EXT_texture_storage) 
#define GL_EXT_texture_storage 1

#define GL_ALPHA8_EXT 0x803C
#define GL_LUMINANCE8_EXT 0x8040
#define GL_LUMINANCE8_ALPHA8_EXT 0x8045
#define GL_RGB10_EXT 0x8052
#define GL_RGB10_A2_EXT 0x8059
#define GL_R8_EXT 0x8229
#define GL_RG8_EXT 0x822B
#define GL_R16F_EXT 0x822D
#define GL_R32F_EXT 0x822E
#define GL_RG16F_EXT 0x822F
#define GL_RG32F_EXT 0x8230
#define GL_RGBA32F_EXT 0x8814
#define GL_RGB32F_EXT 0x8815
#define GL_ALPHA32F_EXT 0x8816
#define GL_LUMINANCE32F_EXT 0x8818
#define GL_LUMINANCE_ALPHA32F_EXT 0x8819
#define GL_RGBA16F_EXT 0x881A
#define GL_RGB16F_EXT 0x881B
#define GL_ALPHA16F_EXT 0x881C
#define GL_LUMINANCE16F_EXT 0x881E
#define GL_LUMINANCE_ALPHA16F_EXT 0x881F
#define GL_RGB_422_APPLE 0x8A1F
#define GL_TEXTURE_IMMUTABLE_FORMAT_EXT 0x912F
#define GL_BGRA8_EXT 0x93A1

typedef void (GLAPIENTRY * PFNGLTEXSTORAGE1DEXTPROC) (GLenum target, GLsizei levels, GLenum internalformat, GLsizei width);
typedef void (GLAPIENTRY * PFNGLTEXSTORAGE2DEXTPROC) (GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height);
typedef void (GLAPIENTRY * PFNGLTEXSTORAGE3DEXTPROC) (GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth);
typedef void (GLAPIENTRY * PFNGLTEXTURESTORAGE1DEXTPROC) (GLuint texture, GLenum target, GLsizei levels, GLenum internalformat, GLsizei width);
typedef void (GLAPIENTRY * PFNGLTEXTURESTORAGE2DEXTPROC) (GLuint texture, GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height);
typedef void (GLAPIENTRY * PFNGLTEXTURESTORAGE3DEXTPROC) (GLuint texture, GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth);

#define glTexStorage1DEXT GLEW_GET_FUN(__glewTexStorage1DEXT)
#define glTexStorage2DEXT GLEW_GET_FUN(__glewTexStorage2DEXT)
#define glTexStorage3DEXT GLEW_GET_FUN(__glewTexStorage3DEXT)
#define glTextureStorage1DEXT GLEW_GET_FUN(__glewTextureStorage1DEXT)
#define glTextureStorage2DEXT GLEW_GET_FUN(__glewTextureStorage2DEXT)
#define glTextureStorage3DEXT GLEW_GET_FUN(__glewTextureStorage3DEXT)

#define GLEW_EXT_texture_storage GLEW_GET_VAR(__GLEW_EXT_texture_storage)

#endif /* !GL_EXT_texture_storage */

/* ------------------- GL_EXT_texture_type_2_10_10_10_REV ------------------ */

#if !defined(GL_EXT_texture_type_2_10_10_10_REV) 
#define GL_EXT_texture_type_2_10_10_10_REV 1

#define GL_UNSIGNED_INT_2_10_10_10_REV_EXT 0x8368

#define GLEW_EXT_texture_type_2_10_10_10_REV GLEW_GET_VAR(__GLEW_EXT_texture_type_2_10_10_10_REV)

#endif /* !GL_EXT_texture_type_2_10_10_10_REV */

/* ------------------------- GL_EXT_unpack_subimage ------------------------ */

#if !defined(GL_EXT_unpack_subimage) 
#define GL_EXT_unpack_subimage 1

#define GL_UNPACK_ROW_LENGTH 0x0CF2
#define GL_UNPACK_SKIP_ROWS 0x0CF3
#define GL_UNPACK_SKIP_PIXELS 0x0CF4

#define GLEW_EXT_unpack_subimage GLEW_GET_VAR(__GLEW_EXT_unpack_subimage)

#endif /* !GL_EXT_unpack_subimage */

/* ----------------------- GL_FJ_shader_binary_GCCSO ----------------------- */

#if !defined(GL_FJ_shader_binary_GCCSO) 
#define GL_FJ_shader_binary_GCCSO 1

#define GL_GCCSO_SHADER_BINARY_FJ 0x9260

#define GLEW_FJ_shader_binary_GCCSO GLEW_GET_VAR(__GLEW_FJ_shader_binary_GCCSO)

#endif /* !GL_FJ_shader_binary_GCCSO */

/* ----------------- GL_IMG_multisampled_render_to_texture ----------------- */

#if !defined(GL_IMG_multisampled_render_to_texture) 
#define GL_IMG_multisampled_render_to_texture 1

#define GL_RENDERBUFFER_SAMPLES_IMG 0x9133
#define GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE_IMG 0x9134
#define GL_MAX_SAMPLES_IMG 0x9135
#define GL_TEXTURE_SAMPLES_IMG 0x9136

typedef void (GLAPIENTRY * PFNGLFRAMEBUFFERTEXTURE2DMULTISAMPLEIMGPROC) (GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level, GLsizei samples);
typedef void (GLAPIENTRY * PFNGLRENDERBUFFERSTORAGEMULTISAMPLEIMGPROC) (GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height);

#define glFramebufferTexture2DMultisampleIMG GLEW_GET_FUN(__glewFramebufferTexture2DMultisampleIMG)
#define glRenderbufferStorageMultisampleIMG GLEW_GET_FUN(__glewRenderbufferStorageMultisampleIMG)

#define GLEW_IMG_multisampled_render_to_texture GLEW_GET_VAR(__GLEW_IMG_multisampled_render_to_texture)

#endif /* !GL_IMG_multisampled_render_to_texture */

/* ------------------------- GL_IMG_program_binary ------------------------- */

#if !defined(GL_IMG_program_binary) 
#define GL_IMG_program_binary 1

#define GL_SGX_PROGRAM_BINARY_IMG 0x9130

#define GLEW_IMG_program_binary GLEW_GET_VAR(__GLEW_IMG_program_binary)

#endif /* !GL_IMG_program_binary */

/* --------------------------- GL_IMG_read_format -------------------------- */

#if !defined(GL_IMG_read_format) 
#define GL_IMG_read_format 1

#define GL_BGRA_IMG 0x80E1
#define GL_UNSIGNED_SHORT_4_4_4_4_REV_IMG 0x8365

#define GLEW_IMG_read_format GLEW_GET_VAR(__GLEW_IMG_read_format)

#endif /* !GL_IMG_read_format */

/* -------------------------- GL_IMG_shader_binary ------------------------- */

#if !defined(GL_IMG_shader_binary) 
#define GL_IMG_shader_binary 1

#define GL_SGX_BINARY_IMG 0x8C0A

#define GLEW_IMG_shader_binary GLEW_GET_VAR(__GLEW_IMG_shader_binary)

#endif /* !GL_IMG_shader_binary */

/* -------------------- GL_IMG_texture_compression_pvrtc ------------------- */

#if !defined(GL_IMG_texture_compression_pvrtc) 
#define GL_IMG_texture_compression_pvrtc 1

#define GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG 0x8C00
#define GL_COMPRESSED_RGB_PVRTC_2BPPV1_IMG 0x8C01
#define GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG 0x8C02
#define GL_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG 0x8C03

#define GLEW_IMG_texture_compression_pvrtc GLEW_GET_VAR(__GLEW_IMG_texture_compression_pvrtc)

#endif /* !GL_IMG_texture_compression_pvrtc */

/* --------------- GL_IMG_texture_env_enhanced_fixed_function -------------- */

#if !defined(GL_IMG_texture_env_enhanced_fixed_function) 
#define GL_IMG_texture_env_enhanced_fixed_function 1

#define GL_DOT3_RGBA_IMG 0x86AF
#define GL_MODULATE_COLOR_IMG 0x8C04
#define GL_RECIP_ADD_SIGNED_ALPHA_IMG 0x8C05
#define GL_TEXTURE_ALPHA_MODULATE_IMG 0x8C06
#define GL_FACTOR_ALPHA_MODULATE_IMG 0x8C07
#define GL_FRAGMENT_ALPHA_MODULATE_IMG 0x8C08
#define GL_ADD_BLEND_IMG 0x8C09

#define GLEW_IMG_texture_env_enhanced_fixed_function GLEW_GET_VAR(__GLEW_IMG_texture_env_enhanced_fixed_function)

#endif /* !GL_IMG_texture_env_enhanced_fixed_function */

/* ------------------------- GL_IMG_user_clip_plane ------------------------ */

#if !defined(GL_IMG_user_clip_plane) 
#define GL_IMG_user_clip_plane 1

#define GL_MAX_CLIP_PLANES_IMG 0x0D32
#define GL_CLIP_PLANE0_IMG 0x3000
#define GL_CLIP_PLANE1_IMG 0x3001
#define GL_CLIP_PLANE2_IMG 0x3002
#define GL_CLIP_PLANE3_IMG 0x3003
#define GL_CLIP_PLANE4_IMG 0x3004
#define GL_CLIP_PLANE5_IMG 0x3005

typedef void (GLAPIENTRY * PFNGLCLIPPLANEFIMGPROC) (GLenum p, GLfloat eqn[4]);

#define glClipPlanefIMG GLEW_GET_FUN(__glewClipPlanefIMG)

#define GLEW_IMG_user_clip_plane GLEW_GET_VAR(__GLEW_IMG_user_clip_plane)

#endif /* !GL_IMG_user_clip_plane */

/* ------------------------------ GL_KHR_debug ----------------------------- */

#if !defined(GL_KHR_debug) 
#define GL_KHR_debug 1

#define GL_CONTEXT_FLAG_DEBUG_BIT 0x00000002
#define GL_STACK_OVERFLOW 0x0503
#define GL_STACK_UNDERFLOW 0x0504
#define GL_DEBUG_OUTPUT_SYNCHRONOUS 0x8242
#define GL_DEBUG_NEXT_LOGGED_MESSAGE_LENGTH 0x8243
#define GL_DEBUG_CALLBACK_FUNCTION 0x8244
#define GL_DEBUG_CALLBACK_USER_PARAM 0x8245
#define GL_DEBUG_SOURCE_API 0x8246
#define GL_DEBUG_SOURCE_WINDOW_SYSTEM 0x8247
#define GL_DEBUG_SOURCE_SHADER_COMPILER 0x8248
#define GL_DEBUG_SOURCE_THIRD_PARTY 0x8249
#define GL_DEBUG_SOURCE_APPLICATION 0x824A
#define GL_DEBUG_SOURCE_OTHER 0x824B
#define GL_DEBUG_TYPE_ERROR 0x824C
#define GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR 0x824D
#define GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR 0x824E
#define GL_DEBUG_TYPE_PORTABILITY 0x824F
#define GL_DEBUG_TYPE_PERFORMANCE 0x8250
#define GL_DEBUG_TYPE_OTHER 0x8251
#define GL_DEBUG_TYPE_MARKER 0x8268
#define GL_DEBUG_TYPE_PUSH_GROUP 0x8269
#define GL_DEBUG_TYPE_POP_GROUP 0x826A
#define GL_DEBUG_SEVERITY_NOTIFICATION 0x826B
#define GL_MAX_DEBUG_GROUP_STACK_DEPTH 0x826C
#define GL_DEBUG_GROUP_STACK_DEPTH 0x826D
#define GL_BUFFER 0x82E0
#define GL_SHADER 0x82E1
#define GL_PROGRAM 0x82E2
#define GL_QUERY 0x82E3
#define GL_PROGRAM_PIPELINE 0x82E4
#define GL_SAMPLER 0x82E6
#define GL_DISPLAY_LIST 0x82E7
#define GL_MAX_LABEL_LENGTH 0x82E8
#define GL_MAX_DEBUG_MESSAGE_LENGTH 0x9143
#define GL_MAX_DEBUG_LOGGED_MESSAGES 0x9144
#define GL_DEBUG_LOGGED_MESSAGES 0x9145
#define GL_DEBUG_SEVERITY_HIGH 0x9146
#define GL_DEBUG_SEVERITY_MEDIUM 0x9147
#define GL_DEBUG_SEVERITY_LOW 0x9148
#define GL_DEBUG_OUTPUT 0x92E0

typedef void (APIENTRY *GLDEBUGPROC)(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, GLvoid* userParam); // NOTE jwilkins: added this typedef

typedef void (GLAPIENTRY * PFNGLDEBUGMESSAGECALLBACKPROC) (GLDEBUGPROC callback, void* userParam); // NOTE jwilkins: had to fix DEBUGPROC
typedef void (GLAPIENTRY * PFNGLDEBUGMESSAGECONTROLPROC) (GLenum source, GLenum type, GLenum severity, GLsizei count, const GLuint* ids, GLboolean enabled);
typedef void (GLAPIENTRY * PFNGLDEBUGMESSAGEINSERTPROC) (GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const char* buf);
typedef GLuint (GLAPIENTRY * PFNGLGETDEBUGMESSAGELOGPROC) (GLuint count, GLsizei bufsize, GLenum* sources, GLenum* types, GLuint* ids, GLenum* severities, GLsizei* lengths, char* messageLog);
typedef void (GLAPIENTRY * PFNGLGETOBJECTLABELPROC) (GLenum identifier, GLuint name, GLsizei bufSize, GLsizei* length, char *label);
typedef void (GLAPIENTRY * PFNGLGETOBJECTPTRLABELPROC) (void* ptr, GLsizei bufSize, GLsizei* length, char *label);
typedef void (GLAPIENTRY * PFNGLGETPOINTERVPROC) (GLenum pname, void** params);
typedef void (GLAPIENTRY * PFNGLOBJECTLABELPROC) (GLenum identifier, GLuint name, GLsizei length, const char* label);
typedef void (GLAPIENTRY * PFNGLOBJECTPTRLABELPROC) (void* ptr, GLsizei length, const char* label);
typedef void (GLAPIENTRY * PFNGLPOPDEBUGGROUPPROC) (void);
typedef void (GLAPIENTRY * PFNGLPUSHDEBUGGROUPPROC) (GLenum source, GLuint id, GLsizei length, const char * message);

#define glDebugMessageCallback GLEW_GET_FUN(__glewDebugMessageCallback)
#define glDebugMessageControl GLEW_GET_FUN(__glewDebugMessageControl)
#define glDebugMessageInsert GLEW_GET_FUN(__glewDebugMessageInsert)
#define glGetDebugMessageLog GLEW_GET_FUN(__glewGetDebugMessageLog)
#define glGetObjectLabel GLEW_GET_FUN(__glewGetObjectLabel)
#define glGetObjectPtrLabel GLEW_GET_FUN(__glewGetObjectPtrLabel)
#define glGetPointerv GLEW_GET_FUN(__glewGetPointerv)
#define glObjectLabel GLEW_GET_FUN(__glewObjectLabel)
#define glObjectPtrLabel GLEW_GET_FUN(__glewObjectPtrLabel)
#define glPopDebugGroup GLEW_GET_FUN(__glewPopDebugGroup)
#define glPushDebugGroup GLEW_GET_FUN(__glewPushDebugGroup)

#define GLEW_KHR_debug GLEW_GET_VAR(__GLEW_KHR_debug)

#endif /* !GL_KHR_debug */

/* ------------------ GL_KHR_texture_compression_astc_ldr ------------------ */

#if !defined(GL_KHR_texture_compression_astc_ldr) 
#define GL_KHR_texture_compression_astc_ldr 1

#define GL_COMPRESSED_RGBA_ASTC_4x4_KHR 0x93B0
#define GL_COMPRESSED_RGBA_ASTC_5x4_KHR 0x93B1
#define GL_COMPRESSED_RGBA_ASTC_5x5_KHR 0x93B2
#define GL_COMPRESSED_RGBA_ASTC_6x5_KHR 0x93B3
#define GL_COMPRESSED_RGBA_ASTC_6x6_KHR 0x93B4
#define GL_COMPRESSED_RGBA_ASTC_8x5_KHR 0x93B5
#define GL_COMPRESSED_RGBA_ASTC_8x6_KHR 0x93B6
#define GL_COMPRESSED_RGBA_ASTC_8x8_KHR 0x93B7
#define GL_COMPRESSED_RGBA_ASTC_10x5_KHR 0x93B8
#define GL_COMPRESSED_RGBA_ASTC_10x6_KHR 0x93B9
#define GL_COMPRESSED_RGBA_ASTC_10x8_KHR 0x93BA
#define GL_COMPRESSED_RGBA_ASTC_10x10_KHR 0x93BB
#define GL_COMPRESSED_RGBA_ASTC_12x10_KHR 0x93BC
#define GL_COMPRESSED_RGBA_ASTC_12x12_KHR 0x93BD
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR 0x93D0
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4_KHR 0x93D1
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5_KHR 0x93D2
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5_KHR 0x93D3
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR 0x93D4
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x5_KHR 0x93D5
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x6_KHR 0x93D6
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR 0x93D7
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x5_KHR 0x93D8
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x6_KHR 0x93D9
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x8_KHR 0x93DA
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x10_KHR 0x93DB
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x10_KHR 0x93DC
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12_KHR 0x93DD

#define GLEW_KHR_texture_compression_astc_ldr GLEW_GET_VAR(__GLEW_KHR_texture_compression_astc_ldr)

#endif /* !GL_KHR_texture_compression_astc_ldr */

/* ------------------------ GL_NV_3dvision_settings ------------------------ */

#if !defined(GL_NV_3dvision_settings) 
#define GL_NV_3dvision_settings 1

#define GL_3DVISION_STEREO_NV 0x90F4
#define GL_STEREO_SEPARATION_NV 0x90F5
#define GL_STEREO_CONVERGENCE_NV 0x90F6
#define GL_STEREO_CUTOFF_NV 0x90F7
#define GL_STEREO_PROJECTION_NV 0x90F8
#define GL_STEREO_PROJECTION_PERSPECTIVE_NV 0x90F9
#define GL_STEREO_PROJECTION_ORTHO_NV 0x90FA

typedef void (GLAPIENTRY * PFNGLSTEREOPARAMETERFNVPROC) (GLenum pname, GLfloat param);
typedef void (GLAPIENTRY * PFNGLSTEREOPARAMETERINVPROC) (GLenum pname, GLint param);

#define glStereoParameterfNV GLEW_GET_FUN(__glewStereoParameterfNV)
#define glStereoParameteriNV GLEW_GET_FUN(__glewStereoParameteriNV)

#define GLEW_NV_3dvision_settings GLEW_GET_VAR(__GLEW_NV_3dvision_settings)

#endif /* !GL_NV_3dvision_settings */

/* ------------------- GL_NV_EGL_stream_consumer_external ------------------ */

#if !defined(GL_NV_EGL_stream_consumer_external) 
#define GL_NV_EGL_stream_consumer_external 1

#define GL_TEXTURE_EXTERNAL_OES 0x8D65
#define GL_SAMPLER_EXTERNAL_OES 0x8D66
#define GL_TEXTURE_BINDING_EXTERNAL_OES 0x8D67
#define GL_REQUIRED_TEXTURE_IMAGE_UNITS_OES 0x8D68

#define GLEW_NV_EGL_stream_consumer_external GLEW_GET_VAR(__GLEW_NV_EGL_stream_consumer_external)

#endif /* !GL_NV_EGL_stream_consumer_external */

/* ------------------------------- GL_NV_bgr ------------------------------- */

#if !defined(GL_NV_bgr) 
#define GL_NV_bgr 1

#define GL_BGR_NV 0x80E0

#define GLEW_NV_bgr GLEW_GET_VAR(__GLEW_NV_bgr)

#endif /* !GL_NV_bgr */

/* ------------------------- GL_NV_coverage_sample ------------------------- */

#if !defined(GL_NV_coverage_sample) 
#define GL_NV_coverage_sample 1

#define GL_COVERAGE_BUFFER_BIT_NV 0x8000
#define GL_COVERAGE_COMPONENT_NV 0x8ED0
#define GL_COVERAGE_COMPONENT4_NV 0x8ED1
#define GL_COVERAGE_ATTACHMENT_NV 0x8ED2
#define GL_COVERAGE_BUFFERS_NV 0x8ED3
#define GL_COVERAGE_ALL_FRAGMENTS_NV 0x8ED5
#define GL_COVERAGE_EDGE_FRAGMENTS_NV 0x8ED6
#define GL_COVERAGE_AUTOMATIC_NV 0x8ED7

typedef void (GLAPIENTRY * PFNGLCOVERAGEMASKNVPROC) (GLboolean mask);
typedef void (GLAPIENTRY * PFNGLCOVERAGEOPERATIONNVPROC) (GLenum operation);

#define glCoverageMaskNV GLEW_GET_FUN(__glewCoverageMaskNV)
#define glCoverageOperationNV GLEW_GET_FUN(__glewCoverageOperationNV)

#define GLEW_NV_coverage_sample GLEW_GET_VAR(__GLEW_NV_coverage_sample)

#endif /* !GL_NV_coverage_sample */

/* ------------------------- GL_NV_depth_nonlinear ------------------------- */

#if !defined(GL_NV_depth_nonlinear) 
#define GL_NV_depth_nonlinear 1

#define GL_DEPTH_COMPONENT16_NONLINEAR_NV 0x8E2C

#define GLEW_NV_depth_nonlinear GLEW_GET_VAR(__GLEW_NV_depth_nonlinear)

#endif /* !GL_NV_depth_nonlinear */

/* --------------------------- GL_NV_draw_buffers -------------------------- */

#if !defined(GL_NV_draw_buffers) 
#define GL_NV_draw_buffers 1

#define GL_MAX_DRAW_BUFFERS_NV 0x8824
#define GL_DRAW_BUFFER0_NV 0x8825
#define GL_DRAW_BUFFER1_NV 0x8826
#define GL_DRAW_BUFFER2_NV 0x8827
#define GL_DRAW_BUFFER3_NV 0x8828
#define GL_DRAW_BUFFER4_NV 0x8829
#define GL_DRAW_BUFFER5_NV 0x882A
#define GL_DRAW_BUFFER6_NV 0x882B
#define GL_DRAW_BUFFER7_NV 0x882C
#define GL_DRAW_BUFFER8_NV 0x882D
#define GL_DRAW_BUFFER9_NV 0x882E
#define GL_DRAW_BUFFER10_NV 0x882F
#define GL_DRAW_BUFFER11_NV 0x8830
#define GL_DRAW_BUFFER12_NV 0x8831
#define GL_DRAW_BUFFER13_NV 0x8832
#define GL_DRAW_BUFFER14_NV 0x8833
#define GL_DRAW_BUFFER15_NV 0x8834
#define GL_COLOR_ATTACHMENT0_NV 0x8CE0
#define GL_COLOR_ATTACHMENT1_NV 0x8CE1
#define GL_COLOR_ATTACHMENT2_NV 0x8CE2
#define GL_COLOR_ATTACHMENT3_NV 0x8CE3
#define GL_COLOR_ATTACHMENT4_NV 0x8CE4
#define GL_COLOR_ATTACHMENT5_NV 0x8CE5
#define GL_COLOR_ATTACHMENT6_NV 0x8CE6
#define GL_COLOR_ATTACHMENT7_NV 0x8CE7
#define GL_COLOR_ATTACHMENT8_NV 0x8CE8
#define GL_COLOR_ATTACHMENT9_NV 0x8CE9
#define GL_COLOR_ATTACHMENT10_NV 0x8CEA
#define GL_COLOR_ATTACHMENT11_NV 0x8CEB
#define GL_COLOR_ATTACHMENT12_NV 0x8CEC
#define GL_COLOR_ATTACHMENT13_NV 0x8CED
#define GL_COLOR_ATTACHMENT14_NV 0x8CEE
#define GL_COLOR_ATTACHMENT15_NV 0x8CEF

typedef void (GLAPIENTRY * PFNGLDRAWBUFFERSNVPROC) (GLsizei n, const GLenum* bufs);

#define glDrawBuffersNV GLEW_GET_FUN(__glewDrawBuffersNV)

#define GLEW_NV_draw_buffers GLEW_GET_VAR(__GLEW_NV_draw_buffers)

#endif /* !GL_NV_draw_buffers */

/* --------------------------- GL_NV_draw_texture -------------------------- */

#if !defined(GL_NV_draw_texture) 
#define GL_NV_draw_texture 1

typedef void (GLAPIENTRY * PFNGLDRAWTEXTURENVPROC) (GLuint texture, GLuint sampler, GLfloat x0, GLfloat y0, GLfloat x1, GLfloat y1, GLfloat z, GLfloat s0, GLfloat t0, GLfloat s1, GLfloat t1);

#define glDrawTextureNV GLEW_GET_FUN(__glewDrawTextureNV)

#define GLEW_NV_draw_texture GLEW_GET_VAR(__GLEW_NV_draw_texture)

#endif /* !GL_NV_draw_texture */

/* ---------------------- GL_NV_fbo_color_attachments ---------------------- */

#if !defined(GL_NV_fbo_color_attachments) 
#define GL_NV_fbo_color_attachments 1

#define GL_MAX_COLOR_ATTACHMENTS_NV 0x8CDF
#define GL_COLOR_ATTACHMENT0_NV 0x8CE0
#define GL_COLOR_ATTACHMENT1_NV 0x8CE1
#define GL_COLOR_ATTACHMENT2_NV 0x8CE2
#define GL_COLOR_ATTACHMENT3_NV 0x8CE3
#define GL_COLOR_ATTACHMENT4_NV 0x8CE4
#define GL_COLOR_ATTACHMENT5_NV 0x8CE5
#define GL_COLOR_ATTACHMENT6_NV 0x8CE6
#define GL_COLOR_ATTACHMENT7_NV 0x8CE7
#define GL_COLOR_ATTACHMENT8_NV 0x8CE8
#define GL_COLOR_ATTACHMENT9_NV 0x8CE9
#define GL_COLOR_ATTACHMENT10_NV 0x8CEA
#define GL_COLOR_ATTACHMENT11_NV 0x8CEB
#define GL_COLOR_ATTACHMENT12_NV 0x8CEC
#define GL_COLOR_ATTACHMENT13_NV 0x8CED
#define GL_COLOR_ATTACHMENT14_NV 0x8CEE
#define GL_COLOR_ATTACHMENT15_NV 0x8CEF

#define GLEW_NV_fbo_color_attachments GLEW_GET_VAR(__GLEW_NV_fbo_color_attachments)

#endif /* !GL_NV_fbo_color_attachments */

/* ------------------------------ GL_NV_fence ------------------------------ */

#if !defined(GL_NV_fence) 
#define GL_NV_fence 1

#define GL_ALL_COMPLETED_NV 0x84F2
#define GL_FENCE_STATUS_NV 0x84F3
#define GL_FENCE_CONDITION_NV 0x84F4

typedef void (GLAPIENTRY * PFNGLDELETEFENCESNVPROC) (GLsizei n, const GLuint* fences);
typedef void (GLAPIENTRY * PFNGLFINISHFENCENVPROC) (GLuint fence);
typedef void (GLAPIENTRY * PFNGLGENFENCESNVPROC) (GLsizei n, GLuint* fences);
typedef void (GLAPIENTRY * PFNGLGETFENCEIVNVPROC) (GLuint fence, GLenum pname, GLint* params);
typedef GLboolean (GLAPIENTRY * PFNGLISFENCENVPROC) (GLuint fence);
typedef void (GLAPIENTRY * PFNGLSETFENCENVPROC) (GLuint fence, GLenum condition);
typedef GLboolean (GLAPIENTRY * PFNGLTESTFENCENVPROC) (GLuint fence);

#define glDeleteFencesNV GLEW_GET_FUN(__glewDeleteFencesNV)
#define glFinishFenceNV GLEW_GET_FUN(__glewFinishFenceNV)
#define glGenFencesNV GLEW_GET_FUN(__glewGenFencesNV)
#define glGetFenceivNV GLEW_GET_FUN(__glewGetFenceivNV)
#define glIsFenceNV GLEW_GET_FUN(__glewIsFenceNV)
#define glSetFenceNV GLEW_GET_FUN(__glewSetFenceNV)
#define glTestFenceNV GLEW_GET_FUN(__glewTestFenceNV)

#define GLEW_NV_fence GLEW_GET_VAR(__GLEW_NV_fence)

#endif /* !GL_NV_fence */

/* -------------------------- GL_NV_pack_subimage -------------------------- */

#if !defined(GL_NV_pack_subimage) 
#define GL_NV_pack_subimage 1

#define GL_PACK_ROW_LENGTH_NV 0x0D02
#define GL_PACK_SKIP_ROWS_NV 0x0D03
#define GL_PACK_SKIP_PIXELS_NV 0x0D04

#define GLEW_NV_pack_subimage GLEW_GET_VAR(__GLEW_NV_pack_subimage)

#endif /* !GL_NV_pack_subimage */

/* --------------------------- GL_NV_packed_float -------------------------- */

#if !defined(GL_NV_packed_float) 
#define GL_NV_packed_float 1

#define GL_R11F_G11F_B10F_NV 0x8C3A
#define GL_UNSIGNED_INT_10F_11F_11F_REV_NV 0x8C3B

#define GLEW_NV_packed_float GLEW_GET_VAR(__GLEW_NV_packed_float)

#endif /* !GL_NV_packed_float */

/* ----------------------- GL_NV_packed_float_linear ----------------------- */

#if !defined(GL_NV_packed_float_linear) 
#define GL_NV_packed_float_linear 1

#define GL_R11F_G11F_B10F_NV 0x8C3A
#define GL_UNSIGNED_INT_10F_11F_11F_REV_NV 0x8C3B

#define GLEW_NV_packed_float_linear GLEW_GET_VAR(__GLEW_NV_packed_float_linear)

#endif /* !GL_NV_packed_float_linear */

/* ----------------------- GL_NV_pixel_buffer_object ----------------------- */

#if !defined(GL_NV_pixel_buffer_object) 
#define GL_NV_pixel_buffer_object 1

#define GL_PIXEL_PACK_BUFFER_NV 0x88EB
#define GL_PIXEL_UNPACK_BUFFER_NV 0x88EC
#define GL_PIXEL_PACK_BUFFER_BINDING_NV 0x88ED
#define GL_PIXEL_UNPACK_BUFFER_BINDING_NV 0x88EF

#define GLEW_NV_pixel_buffer_object GLEW_GET_VAR(__GLEW_NV_pixel_buffer_object)

#endif /* !GL_NV_pixel_buffer_object */

/* ------------------------- GL_NV_platform_binary ------------------------- */

#if !defined(GL_NV_platform_binary) 
#define GL_NV_platform_binary 1

#define GL_NVIDIA_PLATFORM_BINARY_NV 0x890B

#define GLEW_NV_platform_binary GLEW_GET_VAR(__GLEW_NV_platform_binary)

#endif /* !GL_NV_platform_binary */

/* --------------------------- GL_NV_read_buffer --------------------------- */

#if !defined(GL_NV_read_buffer) 
#define GL_NV_read_buffer 1

#define GL_READ_BUFFER_NV 0x0C02

typedef void (GLAPIENTRY * PFNGLREADBUFFERNVPROC) (GLenum mode);

#define glReadBufferNV GLEW_GET_FUN(__glewReadBufferNV)

#define GLEW_NV_read_buffer GLEW_GET_VAR(__GLEW_NV_read_buffer)

#endif /* !GL_NV_read_buffer */

/* ------------------------ GL_NV_read_buffer_front ------------------------ */

#if !defined(GL_NV_read_buffer_front) 
#define GL_NV_read_buffer_front 1

#define GLEW_NV_read_buffer_front GLEW_GET_VAR(__GLEW_NV_read_buffer_front)

#endif /* !GL_NV_read_buffer_front */

/* ---------------------------- GL_NV_read_depth --------------------------- */

#if !defined(GL_NV_read_depth) 
#define GL_NV_read_depth 1

#define GLEW_NV_read_depth GLEW_GET_VAR(__GLEW_NV_read_depth)

#endif /* !GL_NV_read_depth */

/* ------------------------ GL_NV_read_depth_stencil ----------------------- */

#if !defined(GL_NV_read_depth_stencil) 
#define GL_NV_read_depth_stencil 1

#define GLEW_NV_read_depth_stencil GLEW_GET_VAR(__GLEW_NV_read_depth_stencil)

#endif /* !GL_NV_read_depth_stencil */

/* --------------------------- GL_NV_read_stencil -------------------------- */

#if !defined(GL_NV_read_stencil) 
#define GL_NV_read_stencil 1

#define GLEW_NV_read_stencil GLEW_GET_VAR(__GLEW_NV_read_stencil)

#endif /* !GL_NV_read_stencil */

/* -------------------------- GL_NV_texture_array -------------------------- */

#if !defined(GL_NV_texture_array) 
#define GL_NV_texture_array 1

#define GL_UNPACK_SKIP_IMAGES_NV 0x806D
#define GL_UNPACK_IMAGE_HEIGHT_NV 0x806E
#define GL_MAX_ARRAY_TEXTURE_LAYERS_NV 0x88FF
#define GL_TEXTURE_2D_ARRAY_NV 0x8C1A
#define GL_TEXTURE_BINDING_2D_ARRAY_NV 0x8C1D
#define GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LAYER_NV 0x8CD4
#define GL_SAMPLER_2D_ARRAY_NV 0x8DC1

typedef void (GLAPIENTRY * PFNGLCOMPRESSEDTEXIMAGE3DNVPROC) (GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLsizei imageSize, const void* data);
typedef void (GLAPIENTRY * PFNGLCOMPRESSEDTEXSUBIMAGE3DNVPROC) (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLsizei imageSize, const void* data);
typedef void (GLAPIENTRY * PFNGLCOPYTEXSUBIMAGE3DNVPROC) (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height);
typedef void (GLAPIENTRY * PFNGLFRAMEBUFFERTEXTURELAYERNVPROC) (GLenum target, GLenum attachment, GLuint texture, GLint level, GLint layer);
typedef void (GLAPIENTRY * PFNGLTEXIMAGE3DNVPROC) (GLenum target, GLint level, GLenum internalFormat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const void* pixels);
typedef void (GLAPIENTRY * PFNGLTEXSUBIMAGE3DNVPROC) (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const void* pixels);

#define glCompressedTexImage3DNV GLEW_GET_FUN(__glewCompressedTexImage3DNV)
#define glCompressedTexSubImage3DNV GLEW_GET_FUN(__glewCompressedTexSubImage3DNV)
#define glCopyTexSubImage3DNV GLEW_GET_FUN(__glewCopyTexSubImage3DNV)
#define glFramebufferTextureLayerNV GLEW_GET_FUN(__glewFramebufferTextureLayerNV)
#define glTexImage3DNV GLEW_GET_FUN(__glewTexImage3DNV)
#define glTexSubImage3DNV GLEW_GET_FUN(__glewTexSubImage3DNV)

#define GLEW_NV_texture_array GLEW_GET_VAR(__GLEW_NV_texture_array)

#endif /* !GL_NV_texture_array */

/* --------------------- GL_NV_texture_compression_latc -------------------- */

#if !defined(GL_NV_texture_compression_latc) 
#define GL_NV_texture_compression_latc 1

#define GL_COMPRESSED_LUMINANCE_LATC1_NV 0x8C70
#define GL_COMPRESSED_SIGNED_LUMINANCE_LATC1_NV 0x8C71
#define GL_COMPRESSED_LUMINANCE_ALPHA_LATC2_NV 0x8C72
#define GL_COMPRESSED_SIGNED_LUMINANCE_ALPHA_LATC2_NV 0x8C73

#define GLEW_NV_texture_compression_latc GLEW_GET_VAR(__GLEW_NV_texture_compression_latc)

#endif /* !GL_NV_texture_compression_latc */

/* --------------------- GL_NV_texture_compression_s3tc -------------------- */

#if !defined(GL_NV_texture_compression_s3tc) 
#define GL_NV_texture_compression_s3tc 1

#define GL_COMPRESSED_RGB_S3TC_DXT1_NV 0x83F0
#define GL_COMPRESSED_RGBA_S3TC_DXT1_NV 0x83F1
#define GL_COMPRESSED_RGBA_S3TC_DXT3_NV 0x83F2
#define GL_COMPRESSED_RGBA_S3TC_DXT5_NV 0x83F3

#define GLEW_NV_texture_compression_s3tc GLEW_GET_VAR(__GLEW_NV_texture_compression_s3tc)

#endif /* !GL_NV_texture_compression_s3tc */

/* ----------------- GL_NV_texture_compression_s3tc_update ----------------- */

#if !defined(GL_NV_texture_compression_s3tc_update) 
#define GL_NV_texture_compression_s3tc_update 1

#define GLEW_NV_texture_compression_s3tc_update GLEW_GET_VAR(__GLEW_NV_texture_compression_s3tc_update)

#endif /* !GL_NV_texture_compression_s3tc_update */

/* ---------------------- GL_NV_texture_npot_2D_mipmap --------------------- */

#if !defined(GL_NV_texture_npot_2D_mipmap) 
#define GL_NV_texture_npot_2D_mipmap 1

#define GLEW_NV_texture_npot_2D_mipmap GLEW_GET_VAR(__GLEW_NV_texture_npot_2D_mipmap)

#endif /* !GL_NV_texture_npot_2D_mipmap */

/* ---------------------------- GL_OES_EGL_image --------------------------- */

#if !defined(GL_OES_EGL_image) 
#define GL_OES_EGL_image 1

typedef void* GLeglImageOES;

typedef void (GLAPIENTRY * PFNGLEGLIMAGETARGETRENDERBUFFERSTORAGEOESPROC) (GLenum target, GLeglImageOES image);
typedef void (GLAPIENTRY * PFNGLEGLIMAGETARGETTEXTURE2DOESPROC) (GLenum target, GLeglImageOES image);

#define glEGLImageTargetRenderbufferStorageOES GLEW_GET_FUN(__glewEGLImageTargetRenderbufferStorageOES)
#define glEGLImageTargetTexture2DOES GLEW_GET_FUN(__glewEGLImageTargetTexture2DOES)

#define GLEW_OES_EGL_image GLEW_GET_VAR(__GLEW_OES_EGL_image)

#endif /* !GL_OES_EGL_image */

/* ----------------------- GL_OES_EGL_image_external ----------------------- */

#if !defined(GL_OES_EGL_image_external) 
#define GL_OES_EGL_image_external 1

#define GL_TEXTURE_EXTERNAL_OES 0x8D65
#define GL_SAMPLER_EXTERNAL_OES 0x8D66
#define GL_TEXTURE_BINDING_EXTERNAL_OES 0x8D67
#define GL_REQUIRED_TEXTURE_IMAGE_UNITS_OES 0x8D68

#define GLEW_OES_EGL_image_external GLEW_GET_VAR(__GLEW_OES_EGL_image_external)

#endif /* !GL_OES_EGL_image_external */

/* ---------------------------- GL_OES_EGL_sync ---------------------------- */

#if !defined(GL_OES_EGL_sync) 
#define GL_OES_EGL_sync 1

#define GLEW_OES_EGL_sync GLEW_GET_VAR(__GLEW_OES_EGL_sync)

#endif /* !GL_OES_EGL_sync */

/* --------------------- GL_OES_blend_equation_separate -------------------- */

#if !defined(GL_OES_blend_equation_separate) 
#define GL_OES_blend_equation_separate 1

#define GL_BLEND_EQUATION_RGB_OES 0x8009
#define GL_BLEND_EQUATION_ALPHA_OES 0x883D

typedef void (GLAPIENTRY * PFNGLBLENDEQUATIONSEPARATEOESPROC) (GLenum modeRGB, GLenum modeAlpha);

#define glBlendEquationSeparateOES GLEW_GET_FUN(__glewBlendEquationSeparateOES)

#define GLEW_OES_blend_equation_separate GLEW_GET_VAR(__GLEW_OES_blend_equation_separate)

#endif /* !GL_OES_blend_equation_separate */

/* ----------------------- GL_OES_blend_func_separate ---------------------- */

#if !defined(GL_OES_blend_func_separate) 
#define GL_OES_blend_func_separate 1

#define GL_BLEND_DST_RGB_OES 0x80C8
#define GL_BLEND_SRC_RGB_OES 0x80C9
#define GL_BLEND_DST_ALPHA_OES 0x80CA
#define GL_BLEND_SRC_ALPHA_OES 0x80CB

typedef void (GLAPIENTRY * PFNGLBLENDFUNCSEPARATEOESPROC) (GLenum sfactorRGB, GLenum dfactorRGB, GLenum sfactorAlpha, GLenum dfactorAlpha);

#define glBlendFuncSeparateOES GLEW_GET_FUN(__glewBlendFuncSeparateOES)

#define GLEW_OES_blend_func_separate GLEW_GET_VAR(__GLEW_OES_blend_func_separate)

#endif /* !GL_OES_blend_func_separate */

/* ------------------------- GL_OES_blend_subtract ------------------------- */

#if !defined(GL_OES_blend_subtract) 
#define GL_OES_blend_subtract 1

#define GL_FUNC_ADD_OES 0x8006
#define GL_BLEND_EQUATION_OES 0x8009
#define GL_FUNC_SUBTRACT_OES 0x800A
#define GL_FUNC_REVERSE_SUBTRACT_OES 0x800B

typedef void (GLAPIENTRY * PFNGLBLENDEQUATIONOESPROC) (GLenum mode);

#define glBlendEquationOES GLEW_GET_FUN(__glewBlendEquationOES)

#define GLEW_OES_blend_subtract GLEW_GET_VAR(__GLEW_OES_blend_subtract)

#endif /* !GL_OES_blend_subtract */

/* ------------------------ GL_OES_byte_coordinates ------------------------ */

#if !defined(GL_OES_byte_coordinates) 
#define GL_OES_byte_coordinates 1

#define GL_BYTE 0x1400

#define GLEW_OES_byte_coordinates GLEW_GET_VAR(__GLEW_OES_byte_coordinates)

#endif /* !GL_OES_byte_coordinates */

/* ------------------ GL_OES_compressed_ETC1_RGB8_texture ------------------ */

#if !defined(GL_OES_compressed_ETC1_RGB8_texture) 
#define GL_OES_compressed_ETC1_RGB8_texture 1

#define GL_ETC1_RGB8_OES 0x8D64

#define GLEW_OES_compressed_ETC1_RGB8_texture GLEW_GET_VAR(__GLEW_OES_compressed_ETC1_RGB8_texture)

#endif /* !GL_OES_compressed_ETC1_RGB8_texture */

/* ------------------- GL_OES_compressed_paletted_texture ------------------ */

#if !defined(GL_OES_compressed_paletted_texture) 
#define GL_OES_compressed_paletted_texture 1

#define GL_PALETTE4_RGB8_OES 0x8B90
#define GL_PALETTE4_RGBA8_OES 0x8B91
#define GL_PALETTE4_R5_G6_B5_OES 0x8B92
#define GL_PALETTE4_RGBA4_OES 0x8B93
#define GL_PALETTE4_RGB5_A1_OES 0x8B94
#define GL_PALETTE8_RGB8_OES 0x8B95
#define GL_PALETTE8_RGBA8_OES 0x8B96
#define GL_PALETTE8_R5_G6_B5_OES 0x8B97
#define GL_PALETTE8_RGBA4_OES 0x8B98
#define GL_PALETTE8_RGB5_A1_OES 0x8B99

#define GLEW_OES_compressed_paletted_texture GLEW_GET_VAR(__GLEW_OES_compressed_paletted_texture)

#endif /* !GL_OES_compressed_paletted_texture */

/* ----------------------------- GL_OES_depth24 ---------------------------- */

#if !defined(GL_OES_depth24) 
#define GL_OES_depth24 1

#define GL_DEPTH_COMPONENT24_OES 0x81A6

#define GLEW_OES_depth24 GLEW_GET_VAR(__GLEW_OES_depth24)

#endif /* !GL_OES_depth24 */

/* ----------------------------- GL_OES_depth32 ---------------------------- */

#if !defined(GL_OES_depth32) 
#define GL_OES_depth32 1

#define GL_DEPTH_COMPONENT32_OES 0x81A7

#define GLEW_OES_depth32 GLEW_GET_VAR(__GLEW_OES_depth32)

#endif /* !GL_OES_depth32 */

/* -------------------------- GL_OES_depth_texture ------------------------- */

#if !defined(GL_OES_depth_texture) 
#define GL_OES_depth_texture 1

#define GL_UNSIGNED_SHORT 0x1403
#define GL_UNSIGNED_INT 0x1405
#define GL_DEPTH_COMPONENT 0x1902

#define GLEW_OES_depth_texture GLEW_GET_VAR(__GLEW_OES_depth_texture)

#endif /* !GL_OES_depth_texture */

/* --------------------- GL_OES_depth_texture_cube_map --------------------- */

#if !defined(GL_OES_depth_texture_cube_map) 
#define GL_OES_depth_texture_cube_map 1

#define GL_UNSIGNED_SHORT 0x1403
#define GL_UNSIGNED_INT 0x1405
#define GL_DEPTH_COMPONENT 0x1902
#define GL_DEPTH_STENCIL_OES 0x84F9
#define GL_DEPTH24_STENCIL8_OES 0x88F0

#define GLEW_OES_depth_texture_cube_map GLEW_GET_VAR(__GLEW_OES_depth_texture_cube_map)

#endif /* !GL_OES_depth_texture_cube_map */

/* -------------------------- GL_OES_draw_texture -------------------------- */

#if !defined(GL_OES_draw_texture) 
#define GL_OES_draw_texture 1

#define GL_TEXTURE_CROP_RECT_OES 0x8B9D

#define GLEW_OES_draw_texture GLEW_GET_VAR(__GLEW_OES_draw_texture)

#endif /* !GL_OES_draw_texture */

/* ----------------------- GL_OES_element_index_uint ----------------------- */

#if !defined(GL_OES_element_index_uint) 
#define GL_OES_element_index_uint 1

#define GL_UNSIGNED_INT 0x1405

#define GLEW_OES_element_index_uint GLEW_GET_VAR(__GLEW_OES_element_index_uint)

#endif /* !GL_OES_element_index_uint */

/* --------------------- GL_OES_extended_matrix_palette -------------------- */

#if !defined(GL_OES_extended_matrix_palette) 
#define GL_OES_extended_matrix_palette 1

#define GLEW_OES_extended_matrix_palette GLEW_GET_VAR(__GLEW_OES_extended_matrix_palette)

#endif /* !GL_OES_extended_matrix_palette */

/* ------------------------ GL_OES_fbo_render_mipmap ----------------------- */

#if !defined(GL_OES_fbo_render_mipmap) 
#define GL_OES_fbo_render_mipmap 1

#define GLEW_OES_fbo_render_mipmap GLEW_GET_VAR(__GLEW_OES_fbo_render_mipmap)

#endif /* !GL_OES_fbo_render_mipmap */

/* --------------------- GL_OES_fragment_precision_high -------------------- */

#if !defined(GL_OES_fragment_precision_high) 
#define GL_OES_fragment_precision_high 1

#define GLEW_OES_fragment_precision_high GLEW_GET_VAR(__GLEW_OES_fragment_precision_high)

#endif /* !GL_OES_fragment_precision_high */

/* ----------------------- GL_OES_framebuffer_object ----------------------- */

#if !defined(GL_OES_framebuffer_object) 
#define GL_OES_framebuffer_object 1

#define GL_NONE_OES 0
#define GL_INVALID_FRAMEBUFFER_OPERATION_OES 0x0506
#define GL_RGBA4_OES 0x8056
#define GL_RGB5_A1_OES 0x8057
#define GL_DEPTH_COMPONENT16_OES 0x81A5
#define GL_MAX_RENDERBUFFER_SIZE_OES 0x84E8
#define GL_FRAMEBUFFER_BINDING_OES 0x8CA6
#define GL_RENDERBUFFER_BINDING_OES 0x8CA7
#define GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE_OES 0x8CD0
#define GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME_OES 0x8CD1
#define GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL_OES 0x8CD2
#define GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE_OES 0x8CD3
#define GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_3D_ZOFFSET_OES 0x8CD4
#define GL_FRAMEBUFFER_COMPLETE_OES 0x8CD5
#define GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT_OES 0x8CD6
#define GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT_OES 0x8CD7
#define GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS_OES 0x8CD9
#define GL_FRAMEBUFFER_INCOMPLETE_FORMATS_OES 0x8CDA
#define GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER_OES 0x8CDB
#define GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER_OES 0x8CDC
#define GL_FRAMEBUFFER_UNSUPPORTED_OES 0x8CDD
#define GL_COLOR_ATTACHMENT0_OES 0x8CE0
#define GL_DEPTH_ATTACHMENT_OES 0x8D00
#define GL_STENCIL_ATTACHMENT_OES 0x8D20
#define GL_FRAMEBUFFER_OES 0x8D40
#define GL_RENDERBUFFER_OES 0x8D41
#define GL_RENDERBUFFER_WIDTH_OES 0x8D42
#define GL_RENDERBUFFER_HEIGHT_OES 0x8D43
#define GL_RENDERBUFFER_INTERNAL_FORMAT_OES 0x8D44
#define GL_STENCIL_INDEX1_OES 0x8D46
#define GL_STENCIL_INDEX4_OES 0x8D47
#define GL_STENCIL_INDEX8_OES 0x8D48
#define GL_RENDERBUFFER_RED_SIZE_OES 0x8D50
#define GL_RENDERBUFFER_GREEN_SIZE_OES 0x8D51
#define GL_RENDERBUFFER_BLUE_SIZE_OES 0x8D52
#define GL_RENDERBUFFER_ALPHA_SIZE_OES 0x8D53
#define GL_RENDERBUFFER_DEPTH_SIZE_OES 0x8D54
#define GL_RENDERBUFFER_STENCIL_SIZE_OES 0x8D55
#define GL_RGB565_OES 0x8D62

typedef void (GLAPIENTRY * PFNGLBINDFRAMEBUFFEROESPROC) (GLenum target, GLuint framebuffer);
typedef void (GLAPIENTRY * PFNGLBINDRENDERBUFFEROESPROC) (GLenum target, GLuint renderbuffer);
typedef GLenum (GLAPIENTRY * PFNGLCHECKFRAMEBUFFERSTATUSOESPROC) (GLenum target);
typedef void (GLAPIENTRY * PFNGLDELETEFRAMEBUFFERSOESPROC) (GLsizei n, const GLuint* framebuffers);
typedef void (GLAPIENTRY * PFNGLDELETERENDERBUFFERSOESPROC) (GLsizei n, const GLuint* renderbuffers);
typedef void (GLAPIENTRY * PFNGLFRAMEBUFFERRENDERBUFFEROESPROC) (GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer);
typedef void (GLAPIENTRY * PFNGLFRAMEBUFFERTEXTURE2DOESPROC) (GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
typedef void (GLAPIENTRY * PFNGLGENFRAMEBUFFERSOESPROC) (GLsizei n, GLuint* framebuffers);
typedef void (GLAPIENTRY * PFNGLGENRENDERBUFFERSOESPROC) (GLsizei n, GLuint* renderbuffers);
typedef void (GLAPIENTRY * PFNGLGENERATEMIPMAPOESPROC) (GLenum target);
typedef void (GLAPIENTRY * PFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIVOESPROC) (GLenum target, GLenum attachment, GLenum pname, GLint* params);
typedef void (GLAPIENTRY * PFNGLGETRENDERBUFFERPARAMETERIVOESPROC) (GLenum target, GLenum pname, GLint* params);
typedef GLboolean (GLAPIENTRY * PFNGLISFRAMEBUFFEROESPROC) (GLuint framebuffer);
typedef GLboolean (GLAPIENTRY * PFNGLISRENDERBUFFEROESPROC) (GLuint renderbuffer);
typedef void (GLAPIENTRY * PFNGLRENDERBUFFERSTORAGEOESPROC) (GLenum target, GLenum internalformat, GLsizei width, GLsizei height);

#define glBindFramebufferOES GLEW_GET_FUN(__glewBindFramebufferOES)
#define glBindRenderbufferOES GLEW_GET_FUN(__glewBindRenderbufferOES)
#define glCheckFramebufferStatusOES GLEW_GET_FUN(__glewCheckFramebufferStatusOES)
#define glDeleteFramebuffersOES GLEW_GET_FUN(__glewDeleteFramebuffersOES)
#define glDeleteRenderbuffersOES GLEW_GET_FUN(__glewDeleteRenderbuffersOES)
#define glFramebufferRenderbufferOES GLEW_GET_FUN(__glewFramebufferRenderbufferOES)
#define glFramebufferTexture2DOES GLEW_GET_FUN(__glewFramebufferTexture2DOES)
#define glGenFramebuffersOES GLEW_GET_FUN(__glewGenFramebuffersOES)
#define glGenRenderbuffersOES GLEW_GET_FUN(__glewGenRenderbuffersOES)
#define glGenerateMipmapOES GLEW_GET_FUN(__glewGenerateMipmapOES)
#define glGetFramebufferAttachmentParameterivOES GLEW_GET_FUN(__glewGetFramebufferAttachmentParameterivOES)
#define glGetRenderbufferParameterivOES GLEW_GET_FUN(__glewGetRenderbufferParameterivOES)
#define glIsFramebufferOES GLEW_GET_FUN(__glewIsFramebufferOES)
#define glIsRenderbufferOES GLEW_GET_FUN(__glewIsRenderbufferOES)
#define glRenderbufferStorageOES GLEW_GET_FUN(__glewRenderbufferStorageOES)

#define GLEW_OES_framebuffer_object GLEW_GET_VAR(__GLEW_OES_framebuffer_object)

#endif /* !GL_OES_framebuffer_object */

/* ----------------------- GL_OES_get_program_binary ----------------------- */

#if !defined(GL_OES_get_program_binary) 
#define GL_OES_get_program_binary 1

#define GL_PROGRAM_BINARY_LENGTH_OES 0x8741
#define GL_NUM_PROGRAM_BINARY_FORMATS_OES 0x87FE
#define GL_PROGRAM_BINARY_FORMATS_OES 0x87FF

typedef void (GLAPIENTRY * PFNGLGETPROGRAMBINARYOESPROC) (GLuint program, GLsizei bufSize, GLsizei* length, GLenum *binaryFormat, GLvoid*binary);
typedef void (GLAPIENTRY * PFNGLPROGRAMBINARYOESPROC) (GLuint program, GLenum binaryFormat, const void* binary, GLint length);

#define glGetProgramBinaryOES GLEW_GET_FUN(__glewGetProgramBinaryOES)
#define glProgramBinaryOES GLEW_GET_FUN(__glewProgramBinaryOES)

#define GLEW_OES_get_program_binary GLEW_GET_VAR(__GLEW_OES_get_program_binary)

#endif /* !GL_OES_get_program_binary */

/* ---------------------------- GL_OES_mapbuffer --------------------------- */

#if !defined(GL_OES_mapbuffer) 
#define GL_OES_mapbuffer 1

#define GL_WRITE_ONLY_OES 0x88B9
#define GL_BUFFER_ACCESS_OES 0x88BB
#define GL_BUFFER_MAPPED_OES 0x88BC
#define GL_BUFFER_MAP_POINTER_OES 0x88BD

typedef void (GLAPIENTRY * PFNGLGETBUFFERPOINTERVOESPROC) (GLenum target, GLenum pname, void** params);
typedef GLvoid * (GLAPIENTRY * PFNGLMAPBUFFEROESPROC) (GLenum target, GLenum access);
typedef GLboolean (GLAPIENTRY * PFNGLUNMAPBUFFEROESPROC) (GLenum target);

#define glGetBufferPointervOES GLEW_GET_FUN(__glewGetBufferPointervOES)
#define glMapBufferOES GLEW_GET_FUN(__glewMapBufferOES)
#define glUnmapBufferOES GLEW_GET_FUN(__glewUnmapBufferOES)

#define GLEW_OES_mapbuffer GLEW_GET_VAR(__GLEW_OES_mapbuffer)

#endif /* !GL_OES_mapbuffer */

/* --------------------------- GL_OES_matrix_get --------------------------- */

#if !defined(GL_OES_matrix_get) 
#define GL_OES_matrix_get 1

#define GL_PROJECTION_MATRIX_FLOAT_AS_INT_BITS_OES 0x898
#define GL_MODELVIEW_MATRIX_FLOAT_AS_INT_BITS_OES 0x898
#define GL_TEXTURE_MATRIX_FLOAT_AS_INT_BITS_OES 0x898

#define GLEW_OES_matrix_get GLEW_GET_VAR(__GLEW_OES_matrix_get)

#endif /* !GL_OES_matrix_get */

/* ------------------------- GL_OES_matrix_palette ------------------------- */

#if !defined(GL_OES_matrix_palette) 
#define GL_OES_matrix_palette 1

#define GL_MAX_VERTEX_UNITS_OES 0x86A4
#define GL_WEIGHT_ARRAY_TYPE_OES 0x86A9
#define GL_WEIGHT_ARRAY_STRIDE_OES 0x86AA
#define GL_WEIGHT_ARRAY_SIZE_OES 0x86AB
#define GL_WEIGHT_ARRAY_POINTER_OES 0x86AC
#define GL_WEIGHT_ARRAY_OES 0x86AD
#define GL_MATRIX_PALETTE_OES 0x8840
#define GL_MAX_PALETTE_MATRICES_OES 0x8842
#define GL_CURRENT_PALETTE_MATRIX_OES 0x8843
#define GL_MATRIX_INDEX_ARRAY_OES 0x8844
#define GL_MATRIX_INDEX_ARRAY_SIZE_OES 0x8846
#define GL_MATRIX_INDEX_ARRAY_TYPE_OES 0x8847
#define GL_MATRIX_INDEX_ARRAY_STRIDE_OES 0x8848
#define GL_MATRIX_INDEX_ARRAY_POINTER_OES 0x8849
#define GL_WEIGHT_ARRAY_BUFFER_BINDING_OES 0x889E
#define GL_MATRIX_INDEX_ARRAY_BUFFER_BINDING_OES 0x8B9E

typedef void (GLAPIENTRY * PFNGLCURRENTPALETTEMATRIXOESPROC) (GLuint index);
typedef void (GLAPIENTRY * PFNGLMATRIXINDEXPOINTEROESPROC) (GLint size, GLenum type, GLsizei stride, void* pointer);
typedef void (GLAPIENTRY * PFNGLWEIGHTPOINTEROESPROC) (GLint size, GLenum type, GLsizei stride, void* pointer);

#define glCurrentPaletteMatrixOES GLEW_GET_FUN(__glewCurrentPaletteMatrixOES)
#define glMatrixIndexPointerOES GLEW_GET_FUN(__glewMatrixIndexPointerOES)
#define glWeightPointerOES GLEW_GET_FUN(__glewWeightPointerOES)

#define GLEW_OES_matrix_palette GLEW_GET_VAR(__GLEW_OES_matrix_palette)

#endif /* !GL_OES_matrix_palette */

/* ---------------------- GL_OES_packed_depth_stencil ---------------------- */

#if !defined(GL_OES_packed_depth_stencil) 
#define GL_OES_packed_depth_stencil 1

#define GL_DEPTH_STENCIL_OES 0x84F9
#define GL_UNSIGNED_INT_24_8_OES 0x84FA
#define GL_DEPTH24_STENCIL8_OES 0x88F0

#define GLEW_OES_packed_depth_stencil GLEW_GET_VAR(__GLEW_OES_packed_depth_stencil)

#endif /* !GL_OES_packed_depth_stencil */

/* ------------------------ GL_OES_point_size_array ------------------------ */

#if !defined(GL_OES_point_size_array) 
#define GL_OES_point_size_array 1

#define GL_POINT_SIZE_ARRAY_TYPE_OES 0x898A
#define GL_POINT_SIZE_ARRAY_STRIDE_OES 0x898B
#define GL_POINT_SIZE_ARRAY_POINTER_OES 0x898C
#define GL_POINT_SIZE_ARRAY_OES 0x8B9C
#define GL_POINT_SIZE_ARRAY_BUFFER_BINDING_OES 0x8B9F

typedef void (GLAPIENTRY * PFNGLPOINTSIZEPOINTEROESPROC) (GLenum type, GLsizei stride, const void* ptr);

#define glPointSizePointerOES GLEW_GET_FUN(__glewPointSizePointerOES)

#define GLEW_OES_point_size_array GLEW_GET_VAR(__GLEW_OES_point_size_array)

#endif /* !GL_OES_point_size_array */

/* -------------------------- GL_OES_point_sprite -------------------------- */

#if !defined(GL_OES_point_sprite) 
#define GL_OES_point_sprite 1

#define GL_POINT_SPRITE_OES 0x8861
#define GL_COORD_REPLACE_OES 0x8862

#define GLEW_OES_point_sprite GLEW_GET_VAR(__GLEW_OES_point_sprite)

#endif /* !GL_OES_point_sprite */

/* --------------------------- GL_OES_read_format -------------------------- */

#if !defined(GL_OES_read_format) 
#define GL_OES_read_format 1

#define GL_IMPLEMENTATION_COLOR_READ_TYPE_OES 0x8B9A
#define GL_IMPLEMENTATION_COLOR_READ_FORMAT_OES 0x8B9B

#define GLEW_OES_read_format GLEW_GET_VAR(__GLEW_OES_read_format)

#endif /* !GL_OES_read_format */

/* --------------------- GL_OES_required_internalformat -------------------- */

#if !defined(GL_OES_required_internalformat) 
#define GL_OES_required_internalformat 1

#define GL_ALPHA8_OES 0x803C
#define GL_LUMINANCE8_OES 0x8040
#define GL_LUMINANCE4_ALPHA4_OES 0x8043
#define GL_LUMINANCE8_ALPHA8_OES 0x8045
#define GL_RGB8_OES 0x8051
#define GL_RGB10_EXT 0x8052
#define GL_RGBA4_OES 0x8056
#define GL_RGB5_A1_OES 0x8057
#define GL_RGBA8_OES 0x8058
#define GL_RGB10_A2_EXT 0x8059
#define GL_DEPTH_COMPONENT16_OES 0x81A5
#define GL_DEPTH_COMPONENT24_OES 0x81A6
#define GL_DEPTH_COMPONENT32_OES 0x81A7
#define GL_DEPTH24_STENCIL8_OES 0x88F0
#define GL_RGB565_OES 0x8D62

#define GLEW_OES_required_internalformat GLEW_GET_VAR(__GLEW_OES_required_internalformat)

#endif /* !GL_OES_required_internalformat */

/* --------------------------- GL_OES_rgb8_rgba8 --------------------------- */

#if !defined(GL_OES_rgb8_rgba8) 
#define GL_OES_rgb8_rgba8 1

#define GL_RGB8_OES 0x8051
#define GL_RGBA8_OES 0x8058

#define GLEW_OES_rgb8_rgba8 GLEW_GET_VAR(__GLEW_OES_rgb8_rgba8)

#endif /* !GL_OES_rgb8_rgba8 */

/* ------------------------ GL_OES_single_precision ------------------------ */

#if !defined(GL_OES_single_precision) 
#define GL_OES_single_precision 1

typedef double GLclampd;

typedef void (GLAPIENTRY * PFNGLCLEARDEPTHFOESPROC) (GLclampd depth);
typedef void (GLAPIENTRY * PFNGLCLIPPLANEFOESPROC) (GLenum plane, const GLfloat* equation);
typedef void (GLAPIENTRY * PFNGLDEPTHRANGEFOESPROC) (GLclampf n, GLclampf f);
typedef void (GLAPIENTRY * PFNGLFRUSTUMFOESPROC) (GLfloat l, GLfloat r, GLfloat b, GLfloat t, GLfloat n, GLfloat f);
typedef void (GLAPIENTRY * PFNGLGETCLIPPLANEFOESPROC) (GLenum plane, GLfloat* equation);
typedef void (GLAPIENTRY * PFNGLORTHOFOESPROC) (GLfloat l, GLfloat r, GLfloat b, GLfloat t, GLfloat n, GLfloat f);

#define glClearDepthfOES GLEW_GET_FUN(__glewClearDepthfOES)
#define glClipPlanefOES GLEW_GET_FUN(__glewClipPlanefOES)
#define glDepthRangefOES GLEW_GET_FUN(__glewDepthRangefOES)
#define glFrustumfOES GLEW_GET_FUN(__glewFrustumfOES)
#define glGetClipPlanefOES GLEW_GET_FUN(__glewGetClipPlanefOES)
#define glOrthofOES GLEW_GET_FUN(__glewOrthofOES)

#define GLEW_OES_single_precision GLEW_GET_VAR(__GLEW_OES_single_precision)

#endif /* !GL_OES_single_precision */

/* ---------------------- GL_OES_standard_derivatives ---------------------- */

#if !defined(GL_OES_standard_derivatives) 
#define GL_OES_standard_derivatives 1

#define GL_FRAGMENT_SHADER_DERIVATIVE_HINT_OES 0x8B8B

#define GLEW_OES_standard_derivatives GLEW_GET_VAR(__GLEW_OES_standard_derivatives)

#endif /* !GL_OES_standard_derivatives */

/* ---------------------------- GL_OES_stencil1 ---------------------------- */

#if !defined(GL_OES_stencil1) 
#define GL_OES_stencil1 1

#define GL_STENCIL_INDEX1_OES 0x8D46

#define GLEW_OES_stencil1 GLEW_GET_VAR(__GLEW_OES_stencil1)

#endif /* !GL_OES_stencil1 */

/* ---------------------------- GL_OES_stencil4 ---------------------------- */

#if !defined(GL_OES_stencil4) 
#define GL_OES_stencil4 1

#define GL_STENCIL_INDEX4_OES 0x8D47

#define GLEW_OES_stencil4 GLEW_GET_VAR(__GLEW_OES_stencil4)

#endif /* !GL_OES_stencil4 */

/* ---------------------------- GL_OES_stencil8 ---------------------------- */

#if !defined(GL_OES_stencil8) 
#define GL_OES_stencil8 1

#define GL_STENCIL_INDEX8_OES 0x8D48

#define GLEW_OES_stencil8 GLEW_GET_VAR(__GLEW_OES_stencil8)

#endif /* !GL_OES_stencil8 */

/* ----------------------- GL_OES_surfaceless_context ---------------------- */

#if !defined(GL_OES_surfaceless_context) 
#define GL_OES_surfaceless_context 1

#define GL_FRAMEBUFFER_UNDEFINED_OES 0x8219

#define GLEW_OES_surfaceless_context GLEW_GET_VAR(__GLEW_OES_surfaceless_context)

#endif /* !GL_OES_surfaceless_context */

/* --------------------------- GL_OES_texture_3D --------------------------- */

#if !defined(GL_OES_texture_3D) 
#define GL_OES_texture_3D 1

#define GL_TEXTURE_BINDING_3D_OES 0x806A
#define GL_TEXTURE_3D_OES 0x806F
#define GL_TEXTURE_WRAP_R_OES 0x8072
#define GL_MAX_3D_TEXTURE_SIZE_OES 0x8073
#define GL_SAMPLER_3D_OES 0x8B5F

typedef void (GLAPIENTRY * PFNGLCOMPRESSEDTEXIMAGE3DOESPROC) (GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLsizei imageSize, const void* data);
typedef void (GLAPIENTRY * PFNGLCOMPRESSEDTEXSUBIMAGE3DOESPROC) (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLsizei imageSize, const void* data);
typedef void (GLAPIENTRY * PFNGLCOPYTEXSUBIMAGE3DOESPROC) (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height);
typedef void (GLAPIENTRY * PFNGLFRAMEBUFFERTEXTURE3DOESPROC) (GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level, GLint zoffset);
typedef void (GLAPIENTRY * PFNGLTEXIMAGE3DOESPROC) (GLenum target, GLint level, GLenum internalFormat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const void* pixels);
typedef void (GLAPIENTRY * PFNGLTEXSUBIMAGE3DOESPROC) (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const void* pixels);

#define glCompressedTexImage3DOES GLEW_GET_FUN(__glewCompressedTexImage3DOES)
#define glCompressedTexSubImage3DOES GLEW_GET_FUN(__glewCompressedTexSubImage3DOES)
#define glCopyTexSubImage3DOES GLEW_GET_FUN(__glewCopyTexSubImage3DOES)
#define glFramebufferTexture3DOES GLEW_GET_FUN(__glewFramebufferTexture3DOES)
#define glTexImage3DOES GLEW_GET_FUN(__glewTexImage3DOES)
#define glTexSubImage3DOES GLEW_GET_FUN(__glewTexSubImage3DOES)

#define GLEW_OES_texture_3D GLEW_GET_VAR(__GLEW_OES_texture_3D)

#endif /* !GL_OES_texture_3D */

/* ------------------------ GL_OES_texture_cube_map ------------------------ */

#if !defined(GL_OES_texture_cube_map) 
#define GL_OES_texture_cube_map 1

#define GL_TEXTURE_GEN_MODE_OES 0x2500
#define GL_NORMAL_MAP_OES 0x8511
#define GL_REFLECTION_MAP_OES 0x8512
#define GL_TEXTURE_CUBE_MAP_OES 0x8513
#define GL_TEXTURE_BINDING_CUBE_MAP_OES 0x8514
#define GL_TEXTURE_CUBE_MAP_POSITIVE_X_OES 0x8515
#define GL_TEXTURE_CUBE_MAP_NEGATIVE_X_OES 0x8516
#define GL_TEXTURE_CUBE_MAP_POSITIVE_Y_OES 0x8517
#define GL_TEXTURE_CUBE_MAP_NEGATIVE_Y_OES 0x8518
#define GL_TEXTURE_CUBE_MAP_POSITIVE_Z_OES 0x8519
#define GL_TEXTURE_CUBE_MAP_NEGATIVE_Z_OES 0x851A
#define GL_MAX_CUBE_MAP_TEXTURE_SIZE_OES 0x851C
#define GL_TEXTURE_GEN_STR_OES 0x8D60

typedef void (GLAPIENTRY * PFNGLGETTEXGENFVOESPROC) (GLenum coord, GLenum pname, GLfloat* params);
typedef void (GLAPIENTRY * PFNGLGETTEXGENIVOESPROC) (GLenum coord, GLenum pname, GLint* params);
typedef void (GLAPIENTRY * PFNGLGETTEXGENXVOESPROC) (GLenum coord, GLenum pname, GLfixed* params);
typedef void (GLAPIENTRY * PFNGLTEXGENFOESPROC) (GLenum coord, GLenum pname, GLfloat param);
typedef void (GLAPIENTRY * PFNGLTEXGENFVOESPROC) (GLenum coord, GLenum pname, const GLfloat* params);
typedef void (GLAPIENTRY * PFNGLTEXGENIOESPROC) (GLenum coord, GLenum pname, GLint param);
typedef void (GLAPIENTRY * PFNGLTEXGENIVOESPROC) (GLenum coord, GLenum pname, const GLint* params);
typedef void (GLAPIENTRY * PFNGLTEXGENXOESPROC) (GLenum coord, GLenum pname, GLfixed param);
typedef void (GLAPIENTRY * PFNGLTEXGENXVOESPROC) (GLenum coord, GLenum pname, const GLfixed* params);

#define glGetTexGenfvOES GLEW_GET_FUN(__glewGetTexGenfvOES)
#define glGetTexGenivOES GLEW_GET_FUN(__glewGetTexGenivOES)
#define glGetTexGenxvOES GLEW_GET_FUN(__glewGetTexGenxvOES)
#define glTexGenfOES GLEW_GET_FUN(__glewTexGenfOES)
#define glTexGenfvOES GLEW_GET_FUN(__glewTexGenfvOES)
#define glTexGeniOES GLEW_GET_FUN(__glewTexGeniOES)
#define glTexGenivOES GLEW_GET_FUN(__glewTexGenivOES)
#define glTexGenxOES GLEW_GET_FUN(__glewTexGenxOES)
#define glTexGenxvOES GLEW_GET_FUN(__glewTexGenxvOES)

#define GLEW_OES_texture_cube_map GLEW_GET_VAR(__GLEW_OES_texture_cube_map)

#endif /* !GL_OES_texture_cube_map */

/* ---------------------- GL_OES_texture_env_crossbar ---------------------- */

#if !defined(GL_OES_texture_env_crossbar) 
#define GL_OES_texture_env_crossbar 1

#define GLEW_OES_texture_env_crossbar GLEW_GET_VAR(__GLEW_OES_texture_env_crossbar)

#endif /* !GL_OES_texture_env_crossbar */

/* --------------------- GL_OES_texture_mirrored_repeat -------------------- */

#if !defined(GL_OES_texture_mirrored_repeat) 
#define GL_OES_texture_mirrored_repeat 1

#define GL_MIRRORED_REPEAT 0x8370

#define GLEW_OES_texture_mirrored_repeat GLEW_GET_VAR(__GLEW_OES_texture_mirrored_repeat)

#endif /* !GL_OES_texture_mirrored_repeat */

/* -------------------------- GL_OES_texture_npot -------------------------- */

#if !defined(GL_OES_texture_npot) 
#define GL_OES_texture_npot 1

#define GLEW_OES_texture_npot GLEW_GET_VAR(__GLEW_OES_texture_npot)

#endif /* !GL_OES_texture_npot */

/* ----------------------- GL_OES_vertex_array_object ---------------------- */

#if !defined(GL_OES_vertex_array_object) 
#define GL_OES_vertex_array_object 1

#define GL_VERTEX_ARRAY_BINDING_OES 0x85B5

typedef void (GLAPIENTRY * PFNGLBINDVERTEXARRAYOESPROC) (GLuint array);
typedef void (GLAPIENTRY * PFNGLDELETEVERTEXARRAYSOESPROC) (GLsizei n, const GLuint* arrays);
typedef void (GLAPIENTRY * PFNGLGENVERTEXARRAYSOESPROC) (GLsizei n, GLuint* arrays);
typedef GLboolean (GLAPIENTRY * PFNGLISVERTEXARRAYOESPROC) (GLuint array);

#define glBindVertexArrayOES GLEW_GET_FUN(__glewBindVertexArrayOES)
#define glDeleteVertexArraysOES GLEW_GET_FUN(__glewDeleteVertexArraysOES)
#define glGenVertexArraysOES GLEW_GET_FUN(__glewGenVertexArraysOES)
#define glIsVertexArrayOES GLEW_GET_FUN(__glewIsVertexArrayOES)

#define GLEW_OES_vertex_array_object GLEW_GET_VAR(__GLEW_OES_vertex_array_object)

#endif /* !GL_OES_vertex_array_object */

/* ------------------------ GL_OES_vertex_half_float ----------------------- */

#if !defined(GL_OES_vertex_half_float) 
#define GL_OES_vertex_half_float 1

#define GL_HALF_FLOAT_OES 0x8D61

#define GLEW_OES_vertex_half_float GLEW_GET_VAR(__GLEW_OES_vertex_half_float)

#endif /* !GL_OES_vertex_half_float */

/* --------------------- GL_OES_vertex_type_10_10_10_2 --------------------- */

#if !defined(GL_OES_vertex_type_10_10_10_2) 
#define GL_OES_vertex_type_10_10_10_2 1

#define GL_UNSIGNED_INT_10_10_10_2_OES 0x8DF6
#define GL_INT_10_10_10_2_OES 0x8DF7

#define GLEW_OES_vertex_type_10_10_10_2 GLEW_GET_VAR(__GLEW_OES_vertex_type_10_10_10_2)

#endif /* !GL_OES_vertex_type_10_10_10_2 */

/* --------------------------- GL_QCOM_alpha_test -------------------------- */

#if !defined(GL_QCOM_alpha_test) 
#define GL_QCOM_alpha_test 1

#define GL_ALPHA_TEST_QCOM 0x0BC0
#define GL_ALPHA_TEST_FUNC_QCOM 0x0BC1
#define GL_ALPHA_TEST_REF_QCOM 0x0BC2

typedef void (GLAPIENTRY * PFNGLALPHAFUNCQCOMPROC) (GLenum func, GLclampf ref);

#define glAlphaFuncQCOM GLEW_GET_FUN(__glewAlphaFuncQCOM)

#define GLEW_QCOM_alpha_test GLEW_GET_VAR(__GLEW_QCOM_alpha_test)

#endif /* !GL_QCOM_alpha_test */

/* ------------------------ GL_QCOM_binning_control ------------------------ */

#if !defined(GL_QCOM_binning_control) 
#define GL_QCOM_binning_control 1

#define GL_DONT_CARE 0x1100
#define GL_BINNING_CONTROL_HINT_QCOM 0x8FB0
#define GL_CPU_OPTIMIZED_QCOM 0x8FB1
#define GL_GPU_OPTIMIZED_QCOM 0x8FB2
#define GL_RENDER_DIRECT_TO_FRAMEBUFFER_QCOM 0x8FB3

#define GLEW_QCOM_binning_control GLEW_GET_VAR(__GLEW_QCOM_binning_control)

#endif /* !GL_QCOM_binning_control */

/* ------------------------- GL_QCOM_driver_control ------------------------ */

#if !defined(GL_QCOM_driver_control) 
#define GL_QCOM_driver_control 1

typedef void (GLAPIENTRY * PFNGLDISABLEDRIVERCONTROLQCOMPROC) (GLuint driverControl);
typedef void (GLAPIENTRY * PFNGLENABLEDRIVERCONTROLQCOMPROC) (GLuint driverControl);
typedef void (GLAPIENTRY * PFNGLGETDRIVERCONTROLSTRINGQCOMPROC) (GLuint driverControl, GLsizei bufSize, GLsizei* length, char *driverControlString);
typedef void (GLAPIENTRY * PFNGLGETDRIVERCONTROLSQCOMPROC) (GLint* num, GLsizei size, GLuint *driverControls);

#define glDisableDriverControlQCOM GLEW_GET_FUN(__glewDisableDriverControlQCOM)
#define glEnableDriverControlQCOM GLEW_GET_FUN(__glewEnableDriverControlQCOM)
#define glGetDriverControlStringQCOM GLEW_GET_FUN(__glewGetDriverControlStringQCOM)
#define glGetDriverControlsQCOM GLEW_GET_FUN(__glewGetDriverControlsQCOM)

#define GLEW_QCOM_driver_control GLEW_GET_VAR(__GLEW_QCOM_driver_control)

#endif /* !GL_QCOM_driver_control */

/* -------------------------- GL_QCOM_extended_get ------------------------- */

#if !defined(GL_QCOM_extended_get) 
#define GL_QCOM_extended_get 1

#define GL_TEXTURE_WIDTH_QCOM 0x8BD2
#define GL_TEXTURE_HEIGHT_QCOM 0x8BD3
#define GL_TEXTURE_DEPTH_QCOM 0x8BD4
#define GL_TEXTURE_INTERNAL_FORMAT_QCOM 0x8BD5
#define GL_TEXTURE_FORMAT_QCOM 0x8BD6
#define GL_TEXTURE_TYPE_QCOM 0x8BD7
#define GL_TEXTURE_IMAGE_VALID_QCOM 0x8BD8
#define GL_TEXTURE_NUM_LEVELS_QCOM 0x8BD9
#define GL_TEXTURE_TARGET_QCOM 0x8BDA
#define GL_TEXTURE_OBJECT_VALID_QCOM 0x8BDB
#define GL_STATE_RESTORE 0x8BDC

typedef void (GLAPIENTRY * PFNGLEXTGETBUFFERPOINTERVQCOMPROC) (GLenum target, GLvoid** params);
typedef void (GLAPIENTRY * PFNGLEXTGETBUFFERSQCOMPROC) (GLuint* buffers, GLint maxBuffers, GLint* numBuffers);
typedef void (GLAPIENTRY * PFNGLEXTGETFRAMEBUFFERSQCOMPROC) (GLuint* framebuffers, GLint maxFramebuffers, GLint* numFramebuffers);
typedef void (GLAPIENTRY * PFNGLEXTGETRENDERBUFFERSQCOMPROC) (GLuint* renderbuffers, GLint maxRenderbuffers, GLint* numRenderbuffers);
typedef void (GLAPIENTRY * PFNGLEXTGETTEXLEVELPARAMETERIVQCOMPROC) (GLuint texture, GLenum face, GLint level, GLenum pname, GLint* params);
typedef void (GLAPIENTRY * PFNGLEXTGETTEXSUBIMAGEQCOMPROC) (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, void* texels);
typedef void (GLAPIENTRY * PFNGLEXTGETTEXTURESQCOMPROC) (GLuint* textures, GLint maxTextures, GLint* numTextures);
typedef void (GLAPIENTRY * PFNGLEXTTEXOBJECTSTATEOVERRIDEIQCOMPROC) (GLenum target, GLenum pname, GLint param);

#define glExtGetBufferPointervQCOM GLEW_GET_FUN(__glewExtGetBufferPointervQCOM)
#define glExtGetBuffersQCOM GLEW_GET_FUN(__glewExtGetBuffersQCOM)
#define glExtGetFramebuffersQCOM GLEW_GET_FUN(__glewExtGetFramebuffersQCOM)
#define glExtGetRenderbuffersQCOM GLEW_GET_FUN(__glewExtGetRenderbuffersQCOM)
#define glExtGetTexLevelParameterivQCOM GLEW_GET_FUN(__glewExtGetTexLevelParameterivQCOM)
#define glExtGetTexSubImageQCOM GLEW_GET_FUN(__glewExtGetTexSubImageQCOM)
#define glExtGetTexturesQCOM GLEW_GET_FUN(__glewExtGetTexturesQCOM)
#define glExtTexObjectStateOverrideiQCOM GLEW_GET_FUN(__glewExtTexObjectStateOverrideiQCOM)

#define GLEW_QCOM_extended_get GLEW_GET_VAR(__GLEW_QCOM_extended_get)

#endif /* !GL_QCOM_extended_get */

/* ------------------------- GL_QCOM_extended_get2 ------------------------- */

#if !defined(GL_QCOM_extended_get2) 
#define GL_QCOM_extended_get2 1

typedef void (GLAPIENTRY * PFNGLEXTGETPROGRAMBINARYSOURCEQCOMPROC) (GLuint program, GLenum shadertype, char* source, GLint* length);
typedef void (GLAPIENTRY * PFNGLEXTGETPROGRAMSQCOMPROC) (GLuint* programs, GLint maxPrograms, GLint* numPrograms);
typedef void (GLAPIENTRY * PFNGLEXTGETSHADERSQCOMPROC) (GLuint* shaders, GLint maxShaders, GLint* numShaders);
typedef GLboolean (GLAPIENTRY * PFNGLEXTISPROGRAMBINARYQCOMPROC) (GLuint program);

#define glExtGetProgramBinarySourceQCOM GLEW_GET_FUN(__glewExtGetProgramBinarySourceQCOM)
#define glExtGetProgramsQCOM GLEW_GET_FUN(__glewExtGetProgramsQCOM)
#define glExtGetShadersQCOM GLEW_GET_FUN(__glewExtGetShadersQCOM)
#define glExtIsProgramBinaryQCOM GLEW_GET_FUN(__glewExtIsProgramBinaryQCOM)

#define GLEW_QCOM_extended_get2 GLEW_GET_VAR(__GLEW_QCOM_extended_get2)

#endif /* !GL_QCOM_extended_get2 */

/* ---------------------- GL_QCOM_perfmon_global_mode ---------------------- */

#if !defined(GL_QCOM_perfmon_global_mode) 
#define GL_QCOM_perfmon_global_mode 1

#define GL_PERFMON_GLOBAL_MODE_QCOM 0x8FA0

#define GLEW_QCOM_perfmon_global_mode GLEW_GET_VAR(__GLEW_QCOM_perfmon_global_mode)

#endif /* !GL_QCOM_perfmon_global_mode */

/* ------------------------ GL_QCOM_tiled_rendering ------------------------ */

#if !defined(GL_QCOM_tiled_rendering) 
#define GL_QCOM_tiled_rendering 1

#define GL_COLOR_BUFFER_BIT0_QCOM 0x00000001
#define GL_COLOR_BUFFER_BIT1_QCOM 0x00000002
#define GL_COLOR_BUFFER_BIT2_QCOM 0x00000004
#define GL_COLOR_BUFFER_BIT3_QCOM 0x00000008
#define GL_COLOR_BUFFER_BIT4_QCOM 0x00000010
#define GL_COLOR_BUFFER_BIT5_QCOM 0x00000020
#define GL_COLOR_BUFFER_BIT6_QCOM 0x00000040
#define GL_COLOR_BUFFER_BIT7_QCOM 0x00000080
#define GL_DEPTH_BUFFER_BIT0_QCOM 0x00000100
#define GL_DEPTH_BUFFER_BIT1_QCOM 0x00000200
#define GL_DEPTH_BUFFER_BIT2_QCOM 0x00000400
#define GL_DEPTH_BUFFER_BIT3_QCOM 0x00000800
#define GL_DEPTH_BUFFER_BIT4_QCOM 0x00001000
#define GL_DEPTH_BUFFER_BIT5_QCOM 0x00002000
#define GL_DEPTH_BUFFER_BIT6_QCOM 0x00004000
#define GL_DEPTH_BUFFER_BIT7_QCOM 0x00008000
#define GL_STENCIL_BUFFER_BIT0_QCOM 0x00010000
#define GL_STENCIL_BUFFER_BIT1_QCOM 0x00020000
#define GL_STENCIL_BUFFER_BIT2_QCOM 0x00040000
#define GL_STENCIL_BUFFER_BIT3_QCOM 0x00080000
#define GL_STENCIL_BUFFER_BIT4_QCOM 0x00100000
#define GL_STENCIL_BUFFER_BIT5_QCOM 0x00200000
#define GL_STENCIL_BUFFER_BIT6_QCOM 0x00400000
#define GL_STENCIL_BUFFER_BIT7_QCOM 0x00800000
#define GL_MULTISAMPLE_BUFFER_BIT0_QCOM 0x01000000
#define GL_MULTISAMPLE_BUFFER_BIT1_QCOM 0x02000000
#define GL_MULTISAMPLE_BUFFER_BIT2_QCOM 0x04000000
#define GL_MULTISAMPLE_BUFFER_BIT3_QCOM 0x08000000
#define GL_MULTISAMPLE_BUFFER_BIT4_QCOM 0x10000000
#define GL_MULTISAMPLE_BUFFER_BIT5_QCOM 0x20000000
#define GL_MULTISAMPLE_BUFFER_BIT6_QCOM 0x40000000
#define GL_MULTISAMPLE_BUFFER_BIT7_QCOM 0x80000000

typedef void (GLAPIENTRY * PFNGLENDTILINGQCOMPROC) (GLbitfield preserveMask);
typedef void (GLAPIENTRY * PFNGLSTARTTILINGQCOMPROC) (GLuint x, GLuint y, GLuint width, GLuint height, GLbitfield preserveMask);

#define glEndTilingQCOM GLEW_GET_FUN(__glewEndTilingQCOM)
#define glStartTilingQCOM GLEW_GET_FUN(__glewStartTilingQCOM)

#define GLEW_QCOM_tiled_rendering GLEW_GET_VAR(__GLEW_QCOM_tiled_rendering)

#endif /* !GL_QCOM_tiled_rendering */

/* ---------------------- GL_QCOM_writeonly_rendering ---------------------- */

#if !defined(GL_QCOM_writeonly_rendering) 
#define GL_QCOM_writeonly_rendering 1

#define GL_WRITEONLY_RENDERING_QCOM 0x8823

#define GLEW_QCOM_writeonly_rendering GLEW_GET_VAR(__GLEW_QCOM_writeonly_rendering)

#endif /* !GL_QCOM_writeonly_rendering */

/* ------------------------ GL_SUN_multi_draw_arrays ----------------------- */

#if !defined(GL_SUN_multi_draw_arrays) 
#define GL_SUN_multi_draw_arrays 1

typedef void (GLAPIENTRY * PFNGLMULTIDRAWARRAYSSUNPROC) (GLenum mode, const GLint* first, const GLsizei *count, GLsizei primcount);
typedef void (GLAPIENTRY * PFNGLMULTIDRAWELEMENTSSUNPROC) (GLenum mode, GLsizei* count, GLenum type, const GLvoid **indices, GLsizei primcount);

#define glMultiDrawArraysSUN GLEW_GET_FUN(__glewMultiDrawArraysSUN)
#define glMultiDrawElementsSUN GLEW_GET_FUN(__glewMultiDrawElementsSUN)

#define GLEW_SUN_multi_draw_arrays GLEW_GET_VAR(__GLEW_SUN_multi_draw_arrays)

#endif /* !GL_SUN_multi_draw_arrays */

/* --------------------------- GL_VG_KHR_EGL_sync -------------------------- */

#if !defined(GL_VG_KHR_EGL_sync) 
#define GL_VG_KHR_EGL_sync 1

#define GLEW_VG_KHR_EGL_sync GLEW_GET_VAR(__GLEW_VG_KHR_EGL_sync)

#endif /* !GL_VG_KHR_EGL_sync */

/* -------------------------- GL_VIV_shader_binary ------------------------- */

#if !defined(GL_VIV_shader_binary) 
#define GL_VIV_shader_binary 1

#define GL_SHADER_BINARY_VIV 0x8FC4

#define GLEW_VIV_shader_binary GLEW_GET_VAR(__GLEW_VIV_shader_binary)

#endif /* !GL_VIV_shader_binary */

/* ------------------------------------------------------------------------- */

#if defined(GLEW_MX) && defined(_WIN32)
#define GLEW_FUN_EXPORT
#else
#define GLEW_FUN_EXPORT GLEWAPI
#endif /* GLEW_MX */

#if defined(GLEW_MX)
#define GLEW_VAR_EXPORT
#else
#define GLEW_VAR_EXPORT GLEWAPI
#endif /* GLEW_MX */

#if defined(GLEW_MX) && defined(_WIN32)
struct GLEWContextStruct
{
#endif /* GLEW_MX */

#if GL_ES_VERSION_1_0 // NOTE jwilkins: glew doesn't actually seem to be designed to let you use the extension macros
GLEW_FUN_EXPORT PFNGLACTIVETEXTUREPROC __glewActiveTexture;
GLEW_FUN_EXPORT PFNGLALPHAFUNCXPROC __glewAlphaFuncx;
GLEW_FUN_EXPORT PFNGLCLEARCOLORXPROC __glewClearColorx;
GLEW_FUN_EXPORT PFNGLCLEARDEPTHFPROC __glewClearDepthf;
GLEW_FUN_EXPORT PFNGLCLEARDEPTHXPROC __glewClearDepthx;
GLEW_FUN_EXPORT PFNGLCLIENTACTIVETEXTUREPROC __glewClientActiveTexture;
GLEW_FUN_EXPORT PFNGLCOLOR4XPROC __glewColor4x;
GLEW_FUN_EXPORT PFNGLCOMPRESSEDTEXIMAGE2DPROC __glewCompressedTexImage2D;
GLEW_FUN_EXPORT PFNGLCOMPRESSEDTEXSUBIMAGE2DPROC __glewCompressedTexSubImage2D;
GLEW_FUN_EXPORT PFNGLDEPTHRANGEFPROC __glewDepthRangef;
GLEW_FUN_EXPORT PFNGLDEPTHRANGEXPROC __glewDepthRangex;
GLEW_FUN_EXPORT PFNGLFOGXPROC __glewFogx;
GLEW_FUN_EXPORT PFNGLFOGXVPROC __glewFogxv;
GLEW_FUN_EXPORT PFNGLFRUSTUMFPROC __glewFrustumf;
GLEW_FUN_EXPORT PFNGLFRUSTUMXPROC __glewFrustumx;
GLEW_FUN_EXPORT PFNGLLIGHTMODELXPROC __glewLightModelx;
GLEW_FUN_EXPORT PFNGLLIGHTMODELXVPROC __glewLightModelxv;
GLEW_FUN_EXPORT PFNGLLIGHTXPROC __glewLightx;
GLEW_FUN_EXPORT PFNGLLIGHTXVPROC __glewLightxv;
GLEW_FUN_EXPORT PFNGLLINEWIDTHXPROC __glewLineWidthx;
GLEW_FUN_EXPORT PFNGLLOADMATRIXXPROC __glewLoadMatrixx;
GLEW_FUN_EXPORT PFNGLMATERIALXPROC __glewMaterialx;
GLEW_FUN_EXPORT PFNGLMATERIALXVPROC __glewMaterialxv;
GLEW_FUN_EXPORT PFNGLMULTMATRIXXPROC __glewMultMatrixx;
GLEW_FUN_EXPORT PFNGLMULTITEXCOORD4FPROC __glewMultiTexCoord4f;
GLEW_FUN_EXPORT PFNGLMULTITEXCOORD4XPROC __glewMultiTexCoord4x;
GLEW_FUN_EXPORT PFNGLNORMAL3XPROC __glewNormal3x;
GLEW_FUN_EXPORT PFNGLORTHOFPROC __glewOrthof;
GLEW_FUN_EXPORT PFNGLORTHOXPROC __glewOrthox;
GLEW_FUN_EXPORT PFNGLPOINTSIZEXPROC __glewPointSizex;
GLEW_FUN_EXPORT PFNGLPOLYGONOFFSETXPROC __glewPolygonOffsetx;
GLEW_FUN_EXPORT PFNGLROTATEXPROC __glewRotatex;
GLEW_FUN_EXPORT PFNGLSAMPLECOVERAGEPROC __glewSampleCoverage;
GLEW_FUN_EXPORT PFNGLSAMPLECOVERAGEXPROC __glewSampleCoveragex;
GLEW_FUN_EXPORT PFNGLSCALEXPROC __glewScalex;
GLEW_FUN_EXPORT PFNGLTEXENVXPROC __glewTexEnvx;
GLEW_FUN_EXPORT PFNGLTEXENVXVPROC __glewTexEnvxv;
GLEW_FUN_EXPORT PFNGLTEXPARAMETERXPROC __glewTexParameterx;
GLEW_FUN_EXPORT PFNGLTRANSLATEXPROC __glewTranslatex;
#endif // NOTE jwilkins

#if GL_ES_VERSION_CL_1_1 // NOTE jwilkins
GLEW_FUN_EXPORT PFNGLBINDBUFFERPROC __glewBindBuffer;
GLEW_FUN_EXPORT PFNGLBUFFERDATAPROC __glewBufferData;
GLEW_FUN_EXPORT PFNGLBUFFERSUBDATAPROC __glewBufferSubData;
GLEW_FUN_EXPORT PFNGLCLIPPLANEXPROC __glewClipPlanex;
GLEW_FUN_EXPORT PFNGLCOLOR4UBPROC __glewColor4ub;
GLEW_FUN_EXPORT PFNGLDELETEBUFFERSPROC __glewDeleteBuffers;
GLEW_FUN_EXPORT PFNGLGENBUFFERSPROC __glewGenBuffers;
GLEW_FUN_EXPORT PFNGLGETBOOLEANVPROC __glewGetBooleanv;
GLEW_FUN_EXPORT PFNGLGETBUFFERPARAMETERIVPROC __glewGetBufferParameteriv;
GLEW_FUN_EXPORT PFNGLGETCLIPPLANEXPROC __glewGetClipPlanex;
GLEW_FUN_EXPORT PFNGLGETFIXEDVPROC __glewGetFixedv;
GLEW_FUN_EXPORT PFNGLGETLIGHTXVPROC __glewGetLightxv;
GLEW_FUN_EXPORT PFNGLGETMATERIALXVPROC __glewGetMaterialxv;
GLEW_FUN_EXPORT PFNGLGETPOINTERVPROC __glewGetPointerv;
GLEW_FUN_EXPORT PFNGLGETTEXENVIVPROC __glewGetTexEnviv;
GLEW_FUN_EXPORT PFNGLGETTEXENVXVPROC __glewGetTexEnvxv;
GLEW_FUN_EXPORT PFNGLGETTEXPARAMETERIVPROC __glewGetTexParameteriv;
GLEW_FUN_EXPORT PFNGLGETTEXPARAMETERXVPROC __glewGetTexParameterxv;
GLEW_FUN_EXPORT PFNGLISBUFFERPROC __glewIsBuffer;
GLEW_FUN_EXPORT PFNGLISENABLEDPROC __glewIsEnabled;
GLEW_FUN_EXPORT PFNGLISTEXTUREPROC __glewIsTexture;
GLEW_FUN_EXPORT PFNGLPOINTPARAMETERXPROC __glewPointParameterx;
GLEW_FUN_EXPORT PFNGLPOINTPARAMETERXVPROC __glewPointParameterxv;
GLEW_FUN_EXPORT PFNGLTEXENVIPROC __glewTexEnvi;
GLEW_FUN_EXPORT PFNGLTEXENVIVPROC __glewTexEnviv;
GLEW_FUN_EXPORT PFNGLTEXPARAMETERIPROC __glewTexParameteri;
GLEW_FUN_EXPORT PFNGLTEXPARAMETERIVPROC __glewTexParameteriv;
GLEW_FUN_EXPORT PFNGLTEXPARAMETERXVPROC __glewTexParameterxv;
#endif // NOTE jwilkins

#if GL_ES_VERSION_CM_1_1 // XXX
GLEW_FUN_EXPORT PFNGLCLIPPLANEFPROC __glewClipPlanef;
GLEW_FUN_EXPORT PFNGLGETCLIPPLANEFPROC __glewGetClipPlanef;
GLEW_FUN_EXPORT PFNGLGETFLOATVPROC __glewGetFloatv;
GLEW_FUN_EXPORT PFNGLGETLIGHTFVPROC __glewGetLightfv;
GLEW_FUN_EXPORT PFNGLGETMATERIALFVPROC __glewGetMaterialfv;
GLEW_FUN_EXPORT PFNGLGETTEXENVFVPROC __glewGetTexEnvfv;
GLEW_FUN_EXPORT PFNGLGETTEXPARAMETERFVPROC __glewGetTexParameterfv;
GLEW_FUN_EXPORT PFNGLPOINTPARAMETERFPROC __glewPointParameterf;
GLEW_FUN_EXPORT PFNGLPOINTPARAMETERFVPROC __glewPointParameterfv;
GLEW_FUN_EXPORT PFNGLTEXPARAMETERFVPROC __glewTexParameterfv;
#endif // NOTE jwilkins

GLEW_FUN_EXPORT PFNGLATTACHSHADERPROC __glewAttachShader;
GLEW_FUN_EXPORT PFNGLBINDATTRIBLOCATIONPROC __glewBindAttribLocation;
GLEW_FUN_EXPORT PFNGLBINDFRAMEBUFFERPROC __glewBindFramebuffer;
GLEW_FUN_EXPORT PFNGLBINDRENDERBUFFERPROC __glewBindRenderbuffer;
GLEW_FUN_EXPORT PFNGLBLENDCOLORPROC __glewBlendColor;
GLEW_FUN_EXPORT PFNGLBLENDEQUATIONPROC __glewBlendEquation;
GLEW_FUN_EXPORT PFNGLBLENDEQUATIONSEPARATEPROC __glewBlendEquationSeparate;
GLEW_FUN_EXPORT PFNGLBLENDFUNCSEPARATEPROC __glewBlendFuncSeparate;
GLEW_FUN_EXPORT PFNGLCHECKFRAMEBUFFERSTATUSPROC __glewCheckFramebufferStatus;
GLEW_FUN_EXPORT PFNGLCOMPILESHADERPROC __glewCompileShader;
GLEW_FUN_EXPORT PFNGLCREATEPROGRAMPROC __glewCreateProgram;
GLEW_FUN_EXPORT PFNGLCREATESHADERPROC __glewCreateShader;
GLEW_FUN_EXPORT PFNGLDELETEFRAMEBUFFERSPROC __glewDeleteFramebuffers;
GLEW_FUN_EXPORT PFNGLDELETEPROGRAMPROC __glewDeleteProgram;
GLEW_FUN_EXPORT PFNGLDELETERENDERBUFFERSPROC __glewDeleteRenderbuffers;
GLEW_FUN_EXPORT PFNGLDELETESHADERPROC __glewDeleteShader;
GLEW_FUN_EXPORT PFNGLDETACHSHADERPROC __glewDetachShader;
GLEW_FUN_EXPORT PFNGLDISABLEVERTEXATTRIBARRAYPROC __glewDisableVertexAttribArray;
GLEW_FUN_EXPORT PFNGLENABLEVERTEXATTRIBARRAYPROC __glewEnableVertexAttribArray;
GLEW_FUN_EXPORT PFNGLFRAMEBUFFERRENDERBUFFERPROC __glewFramebufferRenderbuffer;
GLEW_FUN_EXPORT PFNGLFRAMEBUFFERTEXTURE2DPROC __glewFramebufferTexture2D;
GLEW_FUN_EXPORT PFNGLGENFRAMEBUFFERSPROC __glewGenFramebuffers;
GLEW_FUN_EXPORT PFNGLGENRENDERBUFFERSPROC __glewGenRenderbuffers;
GLEW_FUN_EXPORT PFNGLGENERATEMIPMAPPROC __glewGenerateMipmap;
GLEW_FUN_EXPORT PFNGLGETACTIVEATTRIBPROC __glewGetActiveAttrib;
GLEW_FUN_EXPORT PFNGLGETACTIVEUNIFORMPROC __glewGetActiveUniform;
GLEW_FUN_EXPORT PFNGLGETATTACHEDSHADERSPROC __glewGetAttachedShaders;
GLEW_FUN_EXPORT PFNGLGETATTRIBLOCATIONPROC __glewGetAttribLocation;
GLEW_FUN_EXPORT PFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIVPROC __glewGetFramebufferAttachmentParameteriv;
GLEW_FUN_EXPORT PFNGLGETPROGRAMINFOLOGPROC __glewGetProgramInfoLog;
GLEW_FUN_EXPORT PFNGLGETPROGRAMIVPROC __glewGetProgramiv;
GLEW_FUN_EXPORT PFNGLGETRENDERBUFFERPARAMETERIVPROC __glewGetRenderbufferParameteriv;
GLEW_FUN_EXPORT PFNGLGETSHADERINFOLOGPROC __glewGetShaderInfoLog;
GLEW_FUN_EXPORT PFNGLGETSHADERPRECISIONFORMATPROC __glewGetShaderPrecisionFormat;
GLEW_FUN_EXPORT PFNGLGETSHADERSOURCEPROC __glewGetShaderSource;
GLEW_FUN_EXPORT PFNGLGETSHADERIVPROC __glewGetShaderiv;
GLEW_FUN_EXPORT PFNGLGETUNIFORMLOCATIONPROC __glewGetUniformLocation;
GLEW_FUN_EXPORT PFNGLGETUNIFORMFVPROC __glewGetUniformfv;
GLEW_FUN_EXPORT PFNGLGETUNIFORMIVPROC __glewGetUniformiv;
GLEW_FUN_EXPORT PFNGLGETVERTEXATTRIBPOINTERVPROC __glewGetVertexAttribPointerv;
GLEW_FUN_EXPORT PFNGLGETVERTEXATTRIBFVPROC __glewGetVertexAttribfv;
GLEW_FUN_EXPORT PFNGLGETVERTEXATTRIBIVPROC __glewGetVertexAttribiv;
GLEW_FUN_EXPORT PFNGLISFRAMEBUFFERPROC __glewIsFramebuffer;
GLEW_FUN_EXPORT PFNGLISPROGRAMPROC __glewIsProgram;
GLEW_FUN_EXPORT PFNGLISRENDERBUFFERPROC __glewIsRenderbuffer;
GLEW_FUN_EXPORT PFNGLISSHADERPROC __glewIsShader;
GLEW_FUN_EXPORT PFNGLLINKPROGRAMPROC __glewLinkProgram;
GLEW_FUN_EXPORT PFNGLRELEASESHADERCOMPILERPROC __glewReleaseShaderCompiler;
GLEW_FUN_EXPORT PFNGLRENDERBUFFERSTORAGEPROC __glewRenderbufferStorage;
GLEW_FUN_EXPORT PFNGLSHADERBINARYPROC __glewShaderBinary;
GLEW_FUN_EXPORT PFNGLSHADERSOURCEPROC __glewShaderSource;
GLEW_FUN_EXPORT PFNGLSTENCILFUNCSEPARATEPROC __glewStencilFuncSeparate;
GLEW_FUN_EXPORT PFNGLSTENCILMASKSEPARATEPROC __glewStencilMaskSeparate;
GLEW_FUN_EXPORT PFNGLSTENCILOPSEPARATEPROC __glewStencilOpSeparate;
GLEW_FUN_EXPORT PFNGLUNIFORM1FPROC __glewUniform1f;
GLEW_FUN_EXPORT PFNGLUNIFORM1FVPROC __glewUniform1fv;
GLEW_FUN_EXPORT PFNGLUNIFORM1IPROC __glewUniform1i;
GLEW_FUN_EXPORT PFNGLUNIFORM1IVPROC __glewUniform1iv;
GLEW_FUN_EXPORT PFNGLUNIFORM2FPROC __glewUniform2f;
GLEW_FUN_EXPORT PFNGLUNIFORM2FVPROC __glewUniform2fv;
GLEW_FUN_EXPORT PFNGLUNIFORM2IPROC __glewUniform2i;
GLEW_FUN_EXPORT PFNGLUNIFORM2IVPROC __glewUniform2iv;
GLEW_FUN_EXPORT PFNGLUNIFORM3FPROC __glewUniform3f;
GLEW_FUN_EXPORT PFNGLUNIFORM3FVPROC __glewUniform3fv;
GLEW_FUN_EXPORT PFNGLUNIFORM3IPROC __glewUniform3i;
GLEW_FUN_EXPORT PFNGLUNIFORM3IVPROC __glewUniform3iv;
GLEW_FUN_EXPORT PFNGLUNIFORM4FPROC __glewUniform4f;
GLEW_FUN_EXPORT PFNGLUNIFORM4FVPROC __glewUniform4fv;
GLEW_FUN_EXPORT PFNGLUNIFORM4IPROC __glewUniform4i;
GLEW_FUN_EXPORT PFNGLUNIFORM4IVPROC __glewUniform4iv;
GLEW_FUN_EXPORT PFNGLUNIFORMMATRIX2FVPROC __glewUniformMatrix2fv;
GLEW_FUN_EXPORT PFNGLUNIFORMMATRIX3FVPROC __glewUniformMatrix3fv;
GLEW_FUN_EXPORT PFNGLUNIFORMMATRIX4FVPROC __glewUniformMatrix4fv;
GLEW_FUN_EXPORT PFNGLUSEPROGRAMPROC __glewUseProgram;
GLEW_FUN_EXPORT PFNGLVALIDATEPROGRAMPROC __glewValidateProgram;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIB1FPROC __glewVertexAttrib1f;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIB1FVPROC __glewVertexAttrib1fv;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIB2FPROC __glewVertexAttrib2f;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIB2FVPROC __glewVertexAttrib2fv;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIB3FPROC __glewVertexAttrib3f;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIB3FVPROC __glewVertexAttrib3fv;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIB4FPROC __glewVertexAttrib4f;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIB4FVPROC __glewVertexAttrib4fv;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIBPOINTERPROC __glewVertexAttribPointer;

#if !GL_ES_VERSION_CL_1_1 // NOTE jwilkins
GLEW_FUN_EXPORT PFNGLBINDBUFFERPROC __glewBindBuffer; // NOTE jwilkins: missing function
GLEW_FUN_EXPORT PFNGLBUFFERDATAPROC __glewBufferData; // NOTE jwilkins: missing function
GLEW_FUN_EXPORT PFNGLBUFFERSUBDATAPROC __glewBufferSubData; // NOTE jwilkins: missing function
GLEW_FUN_EXPORT PFNGLDELETEBUFFERSPROC __glewDeleteBuffers; // NOTE jwilkins: missing function
GLEW_FUN_EXPORT PFNGLGENBUFFERSPROC __glewGenBuffers; // NOTE jwilkins: missing function
GLEW_FUN_EXPORT PFNGLTEXPARAMETERIPROC __glewTexParameteri; // NOTE jwilkins: missing function
GLEW_FUN_EXPORT PFNGLISENABLEDPROC __glewIsEnabled; // NOTE jwilkins: missing function
GLEW_FUN_EXPORT PFNGLGETFLOATVPROC __glewGetFloatv; // NOTE jwilkins: missing function
GLEW_FUN_EXPORT PFNGLDEPTHRANGEFPROC __glewDepthRangef; // NOTE jwilkins: missing function
GLEW_FUN_EXPORT PFNGLACTIVETEXTUREPROC __glewActiveTexture; // NOTE jwilkins: missing function
GLEW_FUN_EXPORT PFNGLGETBOOLEANVPROC __glewGetBooleanv; // NOTE jwilkins: missing function
#endif // XXX

GLEW_FUN_EXPORT PFNGLBEGINPERFMONITORAMDPROC __glewBeginPerfMonitorAMD;
GLEW_FUN_EXPORT PFNGLDELETEPERFMONITORSAMDPROC __glewDeletePerfMonitorsAMD;
GLEW_FUN_EXPORT PFNGLENDPERFMONITORAMDPROC __glewEndPerfMonitorAMD;
GLEW_FUN_EXPORT PFNGLGENPERFMONITORSAMDPROC __glewGenPerfMonitorsAMD;
GLEW_FUN_EXPORT PFNGLGETPERFMONITORCOUNTERDATAAMDPROC __glewGetPerfMonitorCounterDataAMD;
GLEW_FUN_EXPORT PFNGLGETPERFMONITORCOUNTERINFOAMDPROC __glewGetPerfMonitorCounterInfoAMD;
GLEW_FUN_EXPORT PFNGLGETPERFMONITORCOUNTERSTRINGAMDPROC __glewGetPerfMonitorCounterStringAMD;
GLEW_FUN_EXPORT PFNGLGETPERFMONITORCOUNTERSAMDPROC __glewGetPerfMonitorCountersAMD;
GLEW_FUN_EXPORT PFNGLGETPERFMONITORGROUPSTRINGAMDPROC __glewGetPerfMonitorGroupStringAMD;
GLEW_FUN_EXPORT PFNGLGETPERFMONITORGROUPSAMDPROC __glewGetPerfMonitorGroupsAMD;
GLEW_FUN_EXPORT PFNGLSELECTPERFMONITORCOUNTERSAMDPROC __glewSelectPerfMonitorCountersAMD;

GLEW_FUN_EXPORT PFNGLBLITFRAMEBUFFERANGLEPROC __glewBlitFramebufferANGLE;

GLEW_FUN_EXPORT PFNGLRENDERBUFFERSTORAGEMULTISAMPLEANGLEPROC __glewRenderbufferStorageMultisampleANGLE;

GLEW_FUN_EXPORT PFNGLDRAWARRAYSINSTANCEDANGLEPROC __glewDrawArraysInstancedANGLE;
GLEW_FUN_EXPORT PFNGLDRAWELEMENTSINSTANCEDANGLEPROC __glewDrawElementsInstancedANGLE;
GLEW_FUN_EXPORT PFNGLVERTEXATTRIBDIVISORANGLEPROC __glewVertexAttribDivisorANGLE;

GLEW_FUN_EXPORT PFNGLGETTRANSLATEDSHADERSOURCEANGLEPROC __glewGetTranslatedShaderSourceANGLE;

GLEW_FUN_EXPORT PFNGLCOPYTEXTURELEVELSAPPLEPROC __glewCopyTextureLevelsAPPLE;

GLEW_FUN_EXPORT PFNGLRENDERBUFFERSTORAGEMULTISAMPLEAPPLEPROC __glewRenderbufferStorageMultisampleAPPLE;
GLEW_FUN_EXPORT PFNGLRESOLVEMULTISAMPLEFRAMEBUFFERAPPLEPROC __glewResolveMultisampleFramebufferAPPLE;

GLEW_FUN_EXPORT PFNGLCLIENTWAITSYNCAPPLEPROC __glewClientWaitSyncAPPLE;
GLEW_FUN_EXPORT PFNGLDELETESYNCAPPLEPROC __glewDeleteSyncAPPLE;
GLEW_FUN_EXPORT PFNGLFENCESYNCAPPLEPROC __glewFenceSyncAPPLE;
GLEW_FUN_EXPORT PFNGLGETINTEGER64VAPPLEPROC __glewGetInteger64vAPPLE;
GLEW_FUN_EXPORT PFNGLGETSYNCIVAPPLEPROC __glewGetSyncivAPPLE;
GLEW_FUN_EXPORT PFNGLISSYNCAPPLEPROC __glewIsSyncAPPLE;
GLEW_FUN_EXPORT PFNGLWAITSYNCAPPLEPROC __glewWaitSyncAPPLE;

GLEW_FUN_EXPORT PFNGLBLENDEQUATIONEXTPROC __glewBlendEquationEXT;

GLEW_FUN_EXPORT PFNGLGETOBJECTLABELEXTPROC __glewGetObjectLabelEXT;
GLEW_FUN_EXPORT PFNGLLABELOBJECTEXTPROC __glewLabelObjectEXT;

GLEW_FUN_EXPORT PFNGLINSERTEVENTMARKEREXTPROC __glewInsertEventMarkerEXT;
GLEW_FUN_EXPORT PFNGLPUSHGROUPMARKEREXTPROC __glewPushGroupMarkerEXT;

GLEW_FUN_EXPORT PFNGLDISCARDFRAMEBUFFEREXTPROC __glewDiscardFramebufferEXT;

GLEW_FUN_EXPORT PFNGLFLUSHMAPPEDBUFFERRANGEEXTPROC __glewFlushMappedBufferRangeEXT;
GLEW_FUN_EXPORT PFNGLMAPBUFFERRANGEEXTPROC __glewMapBufferRangeEXT;

GLEW_FUN_EXPORT PFNGLMULTIDRAWARRAYSEXTPROC __glewMultiDrawArraysEXT;
GLEW_FUN_EXPORT PFNGLMULTIDRAWELEMENTSEXTPROC __glewMultiDrawElementsEXT;

GLEW_FUN_EXPORT PFNGLFRAMEBUFFERTEXTURE2DMULTISAMPLEEXTPROC __glewFramebufferTexture2DMultisampleEXT;
GLEW_FUN_EXPORT PFNGLRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC __glewRenderbufferStorageMultisampleEXT;

GLEW_FUN_EXPORT PFNGLDRAWBUFFERSINDEXEDEXTPROC __glewDrawBuffersIndexedEXT;
GLEW_FUN_EXPORT PFNGLGETINTEGERI_VEXTPROC __glewGetIntegeri_vEXT;
GLEW_FUN_EXPORT PFNGLREADBUFFERINDEXEDEXTPROC __glewReadBufferIndexedEXT;

GLEW_FUN_EXPORT PFNGLBEGINQUERYEXTPROC __glewBeginQueryEXT;
GLEW_FUN_EXPORT PFNGLDELETEQUERIESEXTPROC __glewDeleteQueriesEXT;
GLEW_FUN_EXPORT PFNGLENDQUERYEXTPROC __glewEndQueryEXT;
GLEW_FUN_EXPORT PFNGLGENQUERIESEXTPROC __glewGenQueriesEXT;
GLEW_FUN_EXPORT PFNGLGETQUERYOBJECTUIVEXTPROC __glewGetQueryObjectuivEXT;
GLEW_FUN_EXPORT PFNGLGETQUERYIVEXTPROC __glewGetQueryivEXT;
GLEW_FUN_EXPORT PFNGLISQUERYEXTPROC __glewIsQueryEXT;

GLEW_FUN_EXPORT PFNGLGETNUNIFORMFVEXTPROC __glewGetnUniformfvEXT;
GLEW_FUN_EXPORT PFNGLGETNUNIFORMIVEXTPROC __glewGetnUniformivEXT;
GLEW_FUN_EXPORT PFNGLREADNPIXELSEXTPROC __glewReadnPixelsEXT;

#if 0 // NOTE jwilkins: there is an inconsistency between the ES and Non-ES versions of this extension??
GLEW_FUN_EXPORT PFNGLACTIVESHADERPROGRAMEXTPROC __glewActiveShaderProgramEXT;
GLEW_FUN_EXPORT PFNGLBINDPROGRAMPIPELINEEXTPROC __glewBindProgramPipelineEXT;
GLEW_FUN_EXPORT PFNGLCREATESHADERPROGRAMVEXTPROC __glewCreateShaderProgramvEXT;
GLEW_FUN_EXPORT PFNGLDELETEPROGRAMPIPELINESEXTPROC __glewDeleteProgramPipelinesEXT;
GLEW_FUN_EXPORT PFNGLGENPROGRAMPIPELINESEXTPROC __glewGenProgramPipelinesEXT;
GLEW_FUN_EXPORT PFNGLGETPROGRAMPIPELINEINFOLOGEXTPROC __glewGetProgramPipelineInfoLogEXT;
GLEW_FUN_EXPORT PFNGLGETPROGRAMPIPELINEIVEXTPROC __glewGetProgramPipelineivEXT;
GLEW_FUN_EXPORT PFNGLISPROGRAMPIPELINEEXTPROC __glewIsProgramPipelineEXT;
GLEW_FUN_EXPORT PFNGLPROGRAMPARAMETERIEXTPROC __glewProgramParameteriEXT;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORM1FEXTPROC __glewProgramUniform1fEXT;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORM1FVEXTPROC __glewProgramUniform1fvEXT;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORM1IEXTPROC __glewProgramUniform1iEXT;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORM1IVEXTPROC __glewProgramUniform1ivEXT;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORM2FEXTPROC __glewProgramUniform2fEXT;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORM2FVEXTPROC __glewProgramUniform2fvEXT;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORM2IEXTPROC __glewProgramUniform2iEXT;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORM2IVEXTPROC __glewProgramUniform2ivEXT;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORM3FEXTPROC __glewProgramUniform3fEXT;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORM3FVEXTPROC __glewProgramUniform3fvEXT;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORM3IEXTPROC __glewProgramUniform3iEXT;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORM3IVEXTPROC __glewProgramUniform3ivEXT;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORM4FEXTPROC __glewProgramUniform4fEXT;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORM4FVEXTPROC __glewProgramUniform4fvEXT;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORM4IEXTPROC __glewProgramUniform4iEXT;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORM4IVEXTPROC __glewProgramUniform4ivEXT;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORMMATRIX2FVEXTPROC __glewProgramUniformMatrix2fvEXT;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORMMATRIX3FVEXTPROC __glewProgramUniformMatrix3fvEXT;
GLEW_FUN_EXPORT PFNGLPROGRAMUNIFORMMATRIX4FVEXTPROC __glewProgramUniformMatrix4fvEXT;
GLEW_FUN_EXPORT PFNGLUSEPROGRAMSTAGESEXTPROC __glewUseProgramStagesEXT;
GLEW_FUN_EXPORT PFNGLVALIDATEPROGRAMPIPELINEEXTPROC __glewValidateProgramPipelineEXT;
#endif // XXX

GLEW_FUN_EXPORT PFNGLTEXSTORAGE1DEXTPROC __glewTexStorage1DEXT;
GLEW_FUN_EXPORT PFNGLTEXSTORAGE2DEXTPROC __glewTexStorage2DEXT;
GLEW_FUN_EXPORT PFNGLTEXSTORAGE3DEXTPROC __glewTexStorage3DEXT;
GLEW_FUN_EXPORT PFNGLTEXTURESTORAGE1DEXTPROC __glewTextureStorage1DEXT;
GLEW_FUN_EXPORT PFNGLTEXTURESTORAGE2DEXTPROC __glewTextureStorage2DEXT;
GLEW_FUN_EXPORT PFNGLTEXTURESTORAGE3DEXTPROC __glewTextureStorage3DEXT;

GLEW_FUN_EXPORT PFNGLFRAMEBUFFERTEXTURE2DMULTISAMPLEIMGPROC __glewFramebufferTexture2DMultisampleIMG;
GLEW_FUN_EXPORT PFNGLRENDERBUFFERSTORAGEMULTISAMPLEIMGPROC __glewRenderbufferStorageMultisampleIMG;

GLEW_FUN_EXPORT PFNGLCLIPPLANEFIMGPROC __glewClipPlanefIMG;

GLEW_FUN_EXPORT PFNGLDEBUGMESSAGECALLBACKPROC __glewDebugMessageCallback;
GLEW_FUN_EXPORT PFNGLDEBUGMESSAGECONTROLPROC __glewDebugMessageControl;
GLEW_FUN_EXPORT PFNGLDEBUGMESSAGEINSERTPROC __glewDebugMessageInsert;
GLEW_FUN_EXPORT PFNGLGETDEBUGMESSAGELOGPROC __glewGetDebugMessageLog;
GLEW_FUN_EXPORT PFNGLGETOBJECTLABELPROC __glewGetObjectLabel;
GLEW_FUN_EXPORT PFNGLGETOBJECTPTRLABELPROC __glewGetObjectPtrLabel;
GLEW_FUN_EXPORT PFNGLGETPOINTERVPROC __glewGetPointerv;
GLEW_FUN_EXPORT PFNGLOBJECTLABELPROC __glewObjectLabel;
GLEW_FUN_EXPORT PFNGLOBJECTPTRLABELPROC __glewObjectPtrLabel;
GLEW_FUN_EXPORT PFNGLPOPDEBUGGROUPPROC __glewPopDebugGroup;
GLEW_FUN_EXPORT PFNGLPUSHDEBUGGROUPPROC __glewPushDebugGroup;

GLEW_FUN_EXPORT PFNGLSTEREOPARAMETERFNVPROC __glewStereoParameterfNV;
GLEW_FUN_EXPORT PFNGLSTEREOPARAMETERINVPROC __glewStereoParameteriNV;

GLEW_FUN_EXPORT PFNGLCOVERAGEMASKNVPROC __glewCoverageMaskNV;
GLEW_FUN_EXPORT PFNGLCOVERAGEOPERATIONNVPROC __glewCoverageOperationNV;

GLEW_FUN_EXPORT PFNGLDRAWBUFFERSNVPROC __glewDrawBuffersNV;

GLEW_FUN_EXPORT PFNGLDRAWTEXTURENVPROC __glewDrawTextureNV;

GLEW_FUN_EXPORT PFNGLDELETEFENCESNVPROC __glewDeleteFencesNV;
GLEW_FUN_EXPORT PFNGLFINISHFENCENVPROC __glewFinishFenceNV;
GLEW_FUN_EXPORT PFNGLGENFENCESNVPROC __glewGenFencesNV;
GLEW_FUN_EXPORT PFNGLGETFENCEIVNVPROC __glewGetFenceivNV;
GLEW_FUN_EXPORT PFNGLISFENCENVPROC __glewIsFenceNV;
GLEW_FUN_EXPORT PFNGLSETFENCENVPROC __glewSetFenceNV;
GLEW_FUN_EXPORT PFNGLTESTFENCENVPROC __glewTestFenceNV;

GLEW_FUN_EXPORT PFNGLREADBUFFERNVPROC __glewReadBufferNV;

GLEW_FUN_EXPORT PFNGLCOMPRESSEDTEXIMAGE3DNVPROC __glewCompressedTexImage3DNV;
GLEW_FUN_EXPORT PFNGLCOMPRESSEDTEXSUBIMAGE3DNVPROC __glewCompressedTexSubImage3DNV;
GLEW_FUN_EXPORT PFNGLCOPYTEXSUBIMAGE3DNVPROC __glewCopyTexSubImage3DNV;
GLEW_FUN_EXPORT PFNGLFRAMEBUFFERTEXTURELAYERNVPROC __glewFramebufferTextureLayerNV;
GLEW_FUN_EXPORT PFNGLTEXIMAGE3DNVPROC __glewTexImage3DNV;
GLEW_FUN_EXPORT PFNGLTEXSUBIMAGE3DNVPROC __glewTexSubImage3DNV;

GLEW_FUN_EXPORT PFNGLEGLIMAGETARGETRENDERBUFFERSTORAGEOESPROC __glewEGLImageTargetRenderbufferStorageOES;
GLEW_FUN_EXPORT PFNGLEGLIMAGETARGETTEXTURE2DOESPROC __glewEGLImageTargetTexture2DOES;

GLEW_FUN_EXPORT PFNGLBLENDEQUATIONSEPARATEOESPROC __glewBlendEquationSeparateOES;

GLEW_FUN_EXPORT PFNGLBLENDFUNCSEPARATEOESPROC __glewBlendFuncSeparateOES;

GLEW_FUN_EXPORT PFNGLBLENDEQUATIONOESPROC __glewBlendEquationOES;

GLEW_FUN_EXPORT PFNGLBINDFRAMEBUFFEROESPROC __glewBindFramebufferOES;
GLEW_FUN_EXPORT PFNGLBINDRENDERBUFFEROESPROC __glewBindRenderbufferOES;
GLEW_FUN_EXPORT PFNGLCHECKFRAMEBUFFERSTATUSOESPROC __glewCheckFramebufferStatusOES;
GLEW_FUN_EXPORT PFNGLDELETEFRAMEBUFFERSOESPROC __glewDeleteFramebuffersOES;
GLEW_FUN_EXPORT PFNGLDELETERENDERBUFFERSOESPROC __glewDeleteRenderbuffersOES;
GLEW_FUN_EXPORT PFNGLFRAMEBUFFERRENDERBUFFEROESPROC __glewFramebufferRenderbufferOES;
GLEW_FUN_EXPORT PFNGLFRAMEBUFFERTEXTURE2DOESPROC __glewFramebufferTexture2DOES;
GLEW_FUN_EXPORT PFNGLGENFRAMEBUFFERSOESPROC __glewGenFramebuffersOES;
GLEW_FUN_EXPORT PFNGLGENRENDERBUFFERSOESPROC __glewGenRenderbuffersOES;
GLEW_FUN_EXPORT PFNGLGENERATEMIPMAPOESPROC __glewGenerateMipmapOES;
GLEW_FUN_EXPORT PFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIVOESPROC __glewGetFramebufferAttachmentParameterivOES;
GLEW_FUN_EXPORT PFNGLGETRENDERBUFFERPARAMETERIVOESPROC __glewGetRenderbufferParameterivOES;
GLEW_FUN_EXPORT PFNGLISFRAMEBUFFEROESPROC __glewIsFramebufferOES;
GLEW_FUN_EXPORT PFNGLISRENDERBUFFEROESPROC __glewIsRenderbufferOES;
GLEW_FUN_EXPORT PFNGLRENDERBUFFERSTORAGEOESPROC __glewRenderbufferStorageOES;

GLEW_FUN_EXPORT PFNGLGETPROGRAMBINARYOESPROC __glewGetProgramBinaryOES;
GLEW_FUN_EXPORT PFNGLPROGRAMBINARYOESPROC __glewProgramBinaryOES;

GLEW_FUN_EXPORT PFNGLGETBUFFERPOINTERVOESPROC __glewGetBufferPointervOES;
GLEW_FUN_EXPORT PFNGLMAPBUFFEROESPROC __glewMapBufferOES;
GLEW_FUN_EXPORT PFNGLUNMAPBUFFEROESPROC __glewUnmapBufferOES;

GLEW_FUN_EXPORT PFNGLCURRENTPALETTEMATRIXOESPROC __glewCurrentPaletteMatrixOES;
GLEW_FUN_EXPORT PFNGLMATRIXINDEXPOINTEROESPROC __glewMatrixIndexPointerOES;
GLEW_FUN_EXPORT PFNGLWEIGHTPOINTEROESPROC __glewWeightPointerOES;

GLEW_FUN_EXPORT PFNGLPOINTSIZEPOINTEROESPROC __glewPointSizePointerOES;

GLEW_FUN_EXPORT PFNGLCLEARDEPTHFOESPROC __glewClearDepthfOES;
GLEW_FUN_EXPORT PFNGLCLIPPLANEFOESPROC __glewClipPlanefOES;
GLEW_FUN_EXPORT PFNGLDEPTHRANGEFOESPROC __glewDepthRangefOES;
GLEW_FUN_EXPORT PFNGLFRUSTUMFOESPROC __glewFrustumfOES;
GLEW_FUN_EXPORT PFNGLGETCLIPPLANEFOESPROC __glewGetClipPlanefOES;
GLEW_FUN_EXPORT PFNGLORTHOFOESPROC __glewOrthofOES;

GLEW_FUN_EXPORT PFNGLCOMPRESSEDTEXIMAGE3DOESPROC __glewCompressedTexImage3DOES;
GLEW_FUN_EXPORT PFNGLCOMPRESSEDTEXSUBIMAGE3DOESPROC __glewCompressedTexSubImage3DOES;
GLEW_FUN_EXPORT PFNGLCOPYTEXSUBIMAGE3DOESPROC __glewCopyTexSubImage3DOES;
GLEW_FUN_EXPORT PFNGLFRAMEBUFFERTEXTURE3DOESPROC __glewFramebufferTexture3DOES;
GLEW_FUN_EXPORT PFNGLTEXIMAGE3DOESPROC __glewTexImage3DOES;
GLEW_FUN_EXPORT PFNGLTEXSUBIMAGE3DOESPROC __glewTexSubImage3DOES;

GLEW_FUN_EXPORT PFNGLGETTEXGENFVOESPROC __glewGetTexGenfvOES;
GLEW_FUN_EXPORT PFNGLGETTEXGENIVOESPROC __glewGetTexGenivOES;
GLEW_FUN_EXPORT PFNGLGETTEXGENXVOESPROC __glewGetTexGenxvOES;
GLEW_FUN_EXPORT PFNGLTEXGENFOESPROC __glewTexGenfOES;
GLEW_FUN_EXPORT PFNGLTEXGENFVOESPROC __glewTexGenfvOES;
GLEW_FUN_EXPORT PFNGLTEXGENIOESPROC __glewTexGeniOES;
GLEW_FUN_EXPORT PFNGLTEXGENIVOESPROC __glewTexGenivOES;
GLEW_FUN_EXPORT PFNGLTEXGENXOESPROC __glewTexGenxOES;
GLEW_FUN_EXPORT PFNGLTEXGENXVOESPROC __glewTexGenxvOES;

GLEW_FUN_EXPORT PFNGLBINDVERTEXARRAYOESPROC __glewBindVertexArrayOES;
GLEW_FUN_EXPORT PFNGLDELETEVERTEXARRAYSOESPROC __glewDeleteVertexArraysOES;
GLEW_FUN_EXPORT PFNGLGENVERTEXARRAYSOESPROC __glewGenVertexArraysOES;
GLEW_FUN_EXPORT PFNGLISVERTEXARRAYOESPROC __glewIsVertexArrayOES;

GLEW_FUN_EXPORT PFNGLALPHAFUNCQCOMPROC __glewAlphaFuncQCOM;

GLEW_FUN_EXPORT PFNGLDISABLEDRIVERCONTROLQCOMPROC __glewDisableDriverControlQCOM;
GLEW_FUN_EXPORT PFNGLENABLEDRIVERCONTROLQCOMPROC __glewEnableDriverControlQCOM;
GLEW_FUN_EXPORT PFNGLGETDRIVERCONTROLSTRINGQCOMPROC __glewGetDriverControlStringQCOM;
GLEW_FUN_EXPORT PFNGLGETDRIVERCONTROLSQCOMPROC __glewGetDriverControlsQCOM;

GLEW_FUN_EXPORT PFNGLEXTGETBUFFERPOINTERVQCOMPROC __glewExtGetBufferPointervQCOM;
GLEW_FUN_EXPORT PFNGLEXTGETBUFFERSQCOMPROC __glewExtGetBuffersQCOM;
GLEW_FUN_EXPORT PFNGLEXTGETFRAMEBUFFERSQCOMPROC __glewExtGetFramebuffersQCOM;
GLEW_FUN_EXPORT PFNGLEXTGETRENDERBUFFERSQCOMPROC __glewExtGetRenderbuffersQCOM;
GLEW_FUN_EXPORT PFNGLEXTGETTEXLEVELPARAMETERIVQCOMPROC __glewExtGetTexLevelParameterivQCOM;
GLEW_FUN_EXPORT PFNGLEXTGETTEXSUBIMAGEQCOMPROC __glewExtGetTexSubImageQCOM;
GLEW_FUN_EXPORT PFNGLEXTGETTEXTURESQCOMPROC __glewExtGetTexturesQCOM;
GLEW_FUN_EXPORT PFNGLEXTTEXOBJECTSTATEOVERRIDEIQCOMPROC __glewExtTexObjectStateOverrideiQCOM;

GLEW_FUN_EXPORT PFNGLEXTGETPROGRAMBINARYSOURCEQCOMPROC __glewExtGetProgramBinarySourceQCOM;
GLEW_FUN_EXPORT PFNGLEXTGETPROGRAMSQCOMPROC __glewExtGetProgramsQCOM;
GLEW_FUN_EXPORT PFNGLEXTGETSHADERSQCOMPROC __glewExtGetShadersQCOM;
GLEW_FUN_EXPORT PFNGLEXTISPROGRAMBINARYQCOMPROC __glewExtIsProgramBinaryQCOM;

GLEW_FUN_EXPORT PFNGLENDTILINGQCOMPROC __glewEndTilingQCOM;
GLEW_FUN_EXPORT PFNGLSTARTTILINGQCOMPROC __glewStartTilingQCOM;

GLEW_FUN_EXPORT PFNGLMULTIDRAWARRAYSSUNPROC __glewMultiDrawArraysSUN;
GLEW_FUN_EXPORT PFNGLMULTIDRAWELEMENTSSUNPROC __glewMultiDrawElementsSUN;

#if defined(GLEW_MX) && !defined(_WIN32)
struct GLEWContextStruct
{
#endif /* GLEW_MX */

GLEW_VAR_EXPORT GLboolean __GLEW_ES_VERSION_1_0;
GLEW_VAR_EXPORT GLboolean __GLEW_ES_VERSION_CL_1_1;
GLEW_VAR_EXPORT GLboolean __GLEW_ES_VERSION_CM_1_1;
GLEW_VAR_EXPORT GLboolean __GLEW_ES_VERSION_2_0;
GLEW_VAR_EXPORT GLboolean __GLEW_AMD_compressed_3DC_texture;
GLEW_VAR_EXPORT GLboolean __GLEW_AMD_compressed_ATC_texture;
GLEW_VAR_EXPORT GLboolean __GLEW_AMD_performance_monitor;
GLEW_VAR_EXPORT GLboolean __GLEW_AMD_program_binary_Z400;
GLEW_VAR_EXPORT GLboolean __GLEW_ANGLE_framebuffer_blit;
GLEW_VAR_EXPORT GLboolean __GLEW_ANGLE_framebuffer_multisample;
GLEW_VAR_EXPORT GLboolean __GLEW_ANGLE_instanced_arrays;
GLEW_VAR_EXPORT GLboolean __GLEW_ANGLE_pack_reverse_row_order;
GLEW_VAR_EXPORT GLboolean __GLEW_ANGLE_texture_compression_dxt3;
GLEW_VAR_EXPORT GLboolean __GLEW_ANGLE_texture_compression_dxt5;
GLEW_VAR_EXPORT GLboolean __GLEW_ANGLE_texture_usage;
GLEW_VAR_EXPORT GLboolean __GLEW_ANGLE_translated_shader_source;
GLEW_VAR_EXPORT GLboolean __GLEW_APPLE_copy_texture_levels;
GLEW_VAR_EXPORT GLboolean __GLEW_APPLE_framebuffer_multisample;
GLEW_VAR_EXPORT GLboolean __GLEW_APPLE_rgb_422;
GLEW_VAR_EXPORT GLboolean __GLEW_APPLE_sync;
GLEW_VAR_EXPORT GLboolean __GLEW_APPLE_texture_2D_limited_npot;
GLEW_VAR_EXPORT GLboolean __GLEW_APPLE_texture_format_BGRA8888;
GLEW_VAR_EXPORT GLboolean __GLEW_APPLE_texture_max_level;
GLEW_VAR_EXPORT GLboolean __GLEW_ARM_mali_program_binary;
GLEW_VAR_EXPORT GLboolean __GLEW_ARM_mali_shader_binary;
GLEW_VAR_EXPORT GLboolean __GLEW_ARM_rgba8;
GLEW_VAR_EXPORT GLboolean __GLEW_DMP_shader_binary;
GLEW_VAR_EXPORT GLboolean __GLEW_EXT_blend_minmax;
GLEW_VAR_EXPORT GLboolean __GLEW_EXT_color_buffer_half_float;
GLEW_VAR_EXPORT GLboolean __GLEW_EXT_debug_label;
GLEW_VAR_EXPORT GLboolean __GLEW_EXT_debug_marker;
GLEW_VAR_EXPORT GLboolean __GLEW_EXT_discard_framebuffer;
GLEW_VAR_EXPORT GLboolean __GLEW_EXT_frag_depth;
GLEW_VAR_EXPORT GLboolean __GLEW_EXT_map_buffer_range;
GLEW_VAR_EXPORT GLboolean __GLEW_EXT_multi_draw_arrays;
GLEW_VAR_EXPORT GLboolean __GLEW_EXT_multisampled_render_to_texture;
GLEW_VAR_EXPORT GLboolean __GLEW_EXT_multiview_draw_buffers;
GLEW_VAR_EXPORT GLboolean __GLEW_EXT_occlusion_query_boolean;
GLEW_VAR_EXPORT GLboolean __GLEW_EXT_read_format_bgra;
GLEW_VAR_EXPORT GLboolean __GLEW_EXT_robustness;
GLEW_VAR_EXPORT GLboolean __GLEW_EXT_sRGB;
GLEW_VAR_EXPORT GLboolean __GLEW_EXT_separate_shader_objects;
GLEW_VAR_EXPORT GLboolean __GLEW_EXT_shader_framebuffer_fetch;
GLEW_VAR_EXPORT GLboolean __GLEW_EXT_shader_texture_lod;
GLEW_VAR_EXPORT GLboolean __GLEW_EXT_shadow_samplers;
GLEW_VAR_EXPORT GLboolean __GLEW_EXT_texture_compression_dxt1;
GLEW_VAR_EXPORT GLboolean __GLEW_EXT_texture_filter_anisotropic;
GLEW_VAR_EXPORT GLboolean __GLEW_EXT_texture_format_BGRA8888;
GLEW_VAR_EXPORT GLboolean __GLEW_EXT_texture_lod_bias;
GLEW_VAR_EXPORT GLboolean __GLEW_EXT_texture_rg;
GLEW_VAR_EXPORT GLboolean __GLEW_EXT_texture_storage;
GLEW_VAR_EXPORT GLboolean __GLEW_EXT_texture_type_2_10_10_10_REV;
GLEW_VAR_EXPORT GLboolean __GLEW_EXT_unpack_subimage;
GLEW_VAR_EXPORT GLboolean __GLEW_FJ_shader_binary_GCCSO;
GLEW_VAR_EXPORT GLboolean __GLEW_IMG_multisampled_render_to_texture;
GLEW_VAR_EXPORT GLboolean __GLEW_IMG_program_binary;
GLEW_VAR_EXPORT GLboolean __GLEW_IMG_read_format;
GLEW_VAR_EXPORT GLboolean __GLEW_IMG_shader_binary;
GLEW_VAR_EXPORT GLboolean __GLEW_IMG_texture_compression_pvrtc;
GLEW_VAR_EXPORT GLboolean __GLEW_IMG_texture_env_enhanced_fixed_function;
GLEW_VAR_EXPORT GLboolean __GLEW_IMG_user_clip_plane;
GLEW_VAR_EXPORT GLboolean __GLEW_KHR_debug;
GLEW_VAR_EXPORT GLboolean __GLEW_KHR_texture_compression_astc_ldr;
GLEW_VAR_EXPORT GLboolean __GLEW_NV_3dvision_settings;
GLEW_VAR_EXPORT GLboolean __GLEW_NV_EGL_stream_consumer_external;
GLEW_VAR_EXPORT GLboolean __GLEW_NV_bgr;
GLEW_VAR_EXPORT GLboolean __GLEW_NV_coverage_sample;
GLEW_VAR_EXPORT GLboolean __GLEW_NV_depth_nonlinear;
GLEW_VAR_EXPORT GLboolean __GLEW_NV_draw_buffers;
GLEW_VAR_EXPORT GLboolean __GLEW_NV_draw_texture;
GLEW_VAR_EXPORT GLboolean __GLEW_NV_fbo_color_attachments;
GLEW_VAR_EXPORT GLboolean __GLEW_NV_fence;
GLEW_VAR_EXPORT GLboolean __GLEW_NV_pack_subimage;
GLEW_VAR_EXPORT GLboolean __GLEW_NV_packed_float;
GLEW_VAR_EXPORT GLboolean __GLEW_NV_packed_float_linear;
GLEW_VAR_EXPORT GLboolean __GLEW_NV_pixel_buffer_object;
GLEW_VAR_EXPORT GLboolean __GLEW_NV_platform_binary;
GLEW_VAR_EXPORT GLboolean __GLEW_NV_read_buffer;
GLEW_VAR_EXPORT GLboolean __GLEW_NV_read_buffer_front;
GLEW_VAR_EXPORT GLboolean __GLEW_NV_read_depth;
GLEW_VAR_EXPORT GLboolean __GLEW_NV_read_depth_stencil;
GLEW_VAR_EXPORT GLboolean __GLEW_NV_read_stencil;
GLEW_VAR_EXPORT GLboolean __GLEW_NV_texture_array;
GLEW_VAR_EXPORT GLboolean __GLEW_NV_texture_compression_latc;
GLEW_VAR_EXPORT GLboolean __GLEW_NV_texture_compression_s3tc;
GLEW_VAR_EXPORT GLboolean __GLEW_NV_texture_compression_s3tc_update;
GLEW_VAR_EXPORT GLboolean __GLEW_NV_texture_npot_2D_mipmap;
GLEW_VAR_EXPORT GLboolean __GLEW_OES_EGL_image;
GLEW_VAR_EXPORT GLboolean __GLEW_OES_EGL_image_external;
GLEW_VAR_EXPORT GLboolean __GLEW_OES_EGL_sync;
GLEW_VAR_EXPORT GLboolean __GLEW_OES_blend_equation_separate;
GLEW_VAR_EXPORT GLboolean __GLEW_OES_blend_func_separate;
GLEW_VAR_EXPORT GLboolean __GLEW_OES_blend_subtract;
GLEW_VAR_EXPORT GLboolean __GLEW_OES_byte_coordinates;
GLEW_VAR_EXPORT GLboolean __GLEW_OES_compressed_ETC1_RGB8_texture;
GLEW_VAR_EXPORT GLboolean __GLEW_OES_compressed_paletted_texture;
GLEW_VAR_EXPORT GLboolean __GLEW_OES_depth24;
GLEW_VAR_EXPORT GLboolean __GLEW_OES_depth32;
GLEW_VAR_EXPORT GLboolean __GLEW_OES_depth_texture;
GLEW_VAR_EXPORT GLboolean __GLEW_OES_depth_texture_cube_map;
GLEW_VAR_EXPORT GLboolean __GLEW_OES_draw_texture;
GLEW_VAR_EXPORT GLboolean __GLEW_OES_element_index_uint;
GLEW_VAR_EXPORT GLboolean __GLEW_OES_extended_matrix_palette;
GLEW_VAR_EXPORT GLboolean __GLEW_OES_fbo_render_mipmap;
GLEW_VAR_EXPORT GLboolean __GLEW_OES_fragment_precision_high;
GLEW_VAR_EXPORT GLboolean __GLEW_OES_framebuffer_object;
GLEW_VAR_EXPORT GLboolean __GLEW_OES_get_program_binary;
GLEW_VAR_EXPORT GLboolean __GLEW_OES_mapbuffer;
GLEW_VAR_EXPORT GLboolean __GLEW_OES_matrix_get;
GLEW_VAR_EXPORT GLboolean __GLEW_OES_matrix_palette;
GLEW_VAR_EXPORT GLboolean __GLEW_OES_packed_depth_stencil;
GLEW_VAR_EXPORT GLboolean __GLEW_OES_point_size_array;
GLEW_VAR_EXPORT GLboolean __GLEW_OES_point_sprite;
GLEW_VAR_EXPORT GLboolean __GLEW_OES_read_format;
GLEW_VAR_EXPORT GLboolean __GLEW_OES_required_internalformat;
GLEW_VAR_EXPORT GLboolean __GLEW_OES_rgb8_rgba8;
GLEW_VAR_EXPORT GLboolean __GLEW_OES_single_precision;
GLEW_VAR_EXPORT GLboolean __GLEW_OES_standard_derivatives;
GLEW_VAR_EXPORT GLboolean __GLEW_OES_stencil1;
GLEW_VAR_EXPORT GLboolean __GLEW_OES_stencil4;
GLEW_VAR_EXPORT GLboolean __GLEW_OES_stencil8;
GLEW_VAR_EXPORT GLboolean __GLEW_OES_surfaceless_context;
GLEW_VAR_EXPORT GLboolean __GLEW_OES_texture_3D;
GLEW_VAR_EXPORT GLboolean __GLEW_OES_texture_cube_map;
GLEW_VAR_EXPORT GLboolean __GLEW_OES_texture_env_crossbar;
GLEW_VAR_EXPORT GLboolean __GLEW_OES_texture_mirrored_repeat;
GLEW_VAR_EXPORT GLboolean __GLEW_OES_texture_npot;
GLEW_VAR_EXPORT GLboolean __GLEW_OES_vertex_array_object;
GLEW_VAR_EXPORT GLboolean __GLEW_OES_vertex_half_float;
GLEW_VAR_EXPORT GLboolean __GLEW_OES_vertex_type_10_10_10_2;
GLEW_VAR_EXPORT GLboolean __GLEW_QCOM_alpha_test;
GLEW_VAR_EXPORT GLboolean __GLEW_QCOM_binning_control;
GLEW_VAR_EXPORT GLboolean __GLEW_QCOM_driver_control;
GLEW_VAR_EXPORT GLboolean __GLEW_QCOM_extended_get;
GLEW_VAR_EXPORT GLboolean __GLEW_QCOM_extended_get2;
GLEW_VAR_EXPORT GLboolean __GLEW_QCOM_perfmon_global_mode;
GLEW_VAR_EXPORT GLboolean __GLEW_QCOM_tiled_rendering;
GLEW_VAR_EXPORT GLboolean __GLEW_QCOM_writeonly_rendering;
GLEW_VAR_EXPORT GLboolean __GLEW_SUN_multi_draw_arrays;
GLEW_VAR_EXPORT GLboolean __GLEW_VG_KHR_EGL_sync;
GLEW_VAR_EXPORT GLboolean __GLEW_VIV_shader_binary;

#ifdef GLEW_MX
}; /* GLEWContextStruct */
#endif /* GLEW_MX */


#endif /* __glesew_h__ */
