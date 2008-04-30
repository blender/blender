/* ----------------------------------------------------------------------------
Copyright (c) 2001-2002, Lev Povalahev
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, 
are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice, 
      this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright notice, 
      this list of conditions and the following disclaimer in the documentation 
      and/or other materials provided with the distribution.
    * The name of the author may be used to endorse or promote products 
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED 
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. 
IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, 
INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, 
BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, 
OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, 
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF 
THE POSSIBILITY OF SUCH DAMAGE.
------------------------------------------------------------------------------*/
/* 
    Lev Povalahev

    levp@gmx.net

    http://www.uni-karlsruhe.de/~uli2/

*/                         
      
#include "extgl.h"
#include <stdio.h>
#include <string.h>

#ifndef _WIN32
#include <GL/glx.h>
#endif /* _WIN32 */

/* turn off the warning for the borland compiler*/
#ifdef __BORLANDC__
#pragma warn -8064
#pragma warn -8065
#endif /* __BORLANDC__	*/

/* function variables */

/*-------------------------------------*/
/* WGL stuff */
/*-------------------------------------*/

#ifdef _WIN32

/* WGL_EXT_etxension_string */

#ifdef WGL_EXT_extensions_string
wglGetExtensionsStringEXTPROC wglGetExtensionsStringEXT = NULL;
#endif /* WGL_EXT_extension_string */

/* WGL_ARB_buffer_region */

#ifdef WGL_ARB_buffer_region
wglCreateBufferRegionARBPROC wglCreateBufferRegionARB = NULL;
wglDeleteBufferRegionARBPROC wglDeleteBufferRegionARB = NULL;
wglSaveBufferRegionARBPROC wglSaveBufferRegionARB = NULL;
wglRestoreBufferRegionARBPROC wglRestoreBufferRegionARB = NULL;
#endif /* WGL_ARB_buffer_region */

/* WGL_ARB_extension_string */


#ifdef WGL_ARB_extensions_string
wglGetExtensionsStringARBPROC wglGetExtensionsStringARB = NULL;
#endif /* WGL_ARB_extension_string */

/* WGL_ARB_pbuffer */

#ifdef WGL_ARB_pbuffer
wglCreatePbufferARBPROC wglCreatePbufferARB = NULL;
wglGetPbufferDCARBPROC wglGetPbufferDCARB = NULL;
wglReleasePbufferDCARBPROC wglReleasePbufferDCARB = NULL;
wglDestroyPbufferARBPROC wglDestroyPbufferARB = NULL;
wglQueryPbufferARBPROC wglQueryPbufferARB = NULL;
#endif /* WGL_ARB_pbuffer */

/* WGL_ARB_pixel_format */

#ifdef WGL_ARB_pixel_format
wglGetPixelFormatAttribivARBPROC wglGetPixelFormatAttribivARB = NULL;
wglGetPixelFormatAttribfvARBPROC wglGetPixelFormatAttribfvARB = NULL;
wglChoosePixelFormatARBPROC wglChoosePixelFormatARB = NULL;
#endif /* WGL_ARB_pixel_format */

/* WGL_ARB_render_texture */

#ifdef WGL_ARB_render_texture
wglBindTexImageARBPROC wglBindTexImageARB = NULL;
wglReleaseTexImageARBPROC wglReleaseTexImageARB = NULL;
wglSetPbufferAttribARBPROC wglSetPbufferAttribARB = NULL;
#endif /* WGL_ARB_render_texture */

/* WGL_EXT_swap_control */

#ifdef WGL_EXT_swap_control
wglSwapIntervalEXTPROC wglSwapIntervalEXT = NULL;
wglGetSwapIntervalEXTPROC wglGetSwapIntervalEXT = NULL;
#endif /* WGL_EXT_swap_control */

/* WGL_ARB_make_current_read */

#ifdef WGL_ARB_make_current_read
wglMakeContextCurrentARBPROC wglMakeContextCurrentARB = NULL;
wglGetCurrentReadDCARBPROC wglGetCurrentReadDCARB = NULL;
#endif /* WGL_ARB_make_current_read*/ 

/* VAR */

#endif /* WIN32 */

/*-------------------------------------*/
/*---WGL STUFF END---------------------*/
/*-------------------------------------*/

#ifdef _WIN32

#ifdef GL_VERSION_1_2
glDrawRangeElementsPROC glDrawRangeElements = NULL;
glTexImage3DPROC glTexImage3D = NULL;
glTexSubImage3DPROC glTexSubImage3D = NULL;
glCopyTexSubImage3DPROC glCopyTexSubImage3D = NULL;
#endif /* GL_VERSION_1_2 */

#ifdef GL_ARB_imaging
glBlendColorPROC glBlendColor = NULL;
LIB_RENDERING_EXPORT
glBlendEquationPROC glBlendEquation = NULL;
glColorTablePROC glColorTable = NULL;
glColorTableParameterfvPROC glColorTableParameterfv = NULL;
glColorTableParameterivPROC glColorTableParameteriv = NULL;
glCopyColorTablePROC glCopyColorTable = NULL;
glGetColorTablePROC glGetColorTable = NULL;
glGetColorTableParameterfvPROC glGetColorTableParameterfv = NULL;
glGetColorTableParameterivPROC glGetColorTableParameteriv = NULL;
glColorSubTablePROC glColorSubTable = NULL;
glCopyColorSubTablePROC glCopyColorSubTable = NULL;
glConvolutionFilter1DPROC glConvolutionFilter1D = NULL;
glConvolutionFilter2DPROC glConvolutionFilter2D = NULL;
glConvolutionParameterfPROC glConvolutionParameterf = NULL;
glConvolutionParameterfvPROC glConvolutionParameterfv = NULL;
glConvolutionParameteriPROC glConvolutionParameteri = NULL;
glConvolutionParameterivPROC glConvolutionParameteriv = NULL;
glCopyConvolutionFilter1DPROC glCopyConvolutionFilter1D = NULL;
glCopyConvolutionFilter2DPROC glCopyConvolutionFilter2D = NULL;
glGetConvolutionFilterPROC glGetConvolutionFilter = NULL;
glGetConvolutionParameterfvPROC glGetConvolutionParameterfv = NULL;
glGetConvolutionParameterivPROC glGetConvolutionParameteriv = NULL;
glGetSeparableFilterPROC glGetSeparableFilter = NULL;
glSeparableFilter2DPROC glSeparableFilter2D = NULL;
glGetHistogramPROC glGetHistogram = NULL;
glGetHistogramParameterfvPROC glGetHistogramParameterfv = NULL;
glGetHistogramParameterivPROC glGetHistogramParameteriv = NULL;
glGetMinmaxPROC glGetMinmax = NULL;
glGetMinmaxParameterfvPROC glGetMinmaxParameterfv = NULL;
glGetMinmaxParameterivPROC glGetMinmaxParameteriv = NULL;
glHistogramPROC glHistogram = NULL;
glMinmaxPROC glMinmax = NULL;
glResetHistogramPROC glResetHistogram = NULL;
glResetMinmaxPROC glResetMinmax = NULL;
#endif /* GL_ARB_imaging */

/* 1.3 */

#ifdef GL_VERSION_1_3
glActiveTexturePROC glActiveTexture = NULL;
glClientActiveTexturePROC glClientActiveTexture = NULL;
glMultiTexCoord1dPROC glMultiTexCoord1d = NULL;
glMultiTexCoord1dvPROC glMultiTexCoord1dv = NULL;
glMultiTexCoord1fPROC glMultiTexCoord1f = NULL;
glMultiTexCoord1fvPROC glMultiTexCoord1fv = NULL;
glMultiTexCoord1iPROC glMultiTexCoord1i = NULL;
glMultiTexCoord1ivPROC glMultiTexCoord1iv = NULL;
glMultiTexCoord1sPROC glMultiTexCoord1s = NULL;
glMultiTexCoord1svPROC glMultiTexCoord1sv = NULL;
glMultiTexCoord2dPROC glMultiTexCoord2d = NULL;
glMultiTexCoord2dvPROC glMultiTexCoord2dv = NULL;
glMultiTexCoord2fPROC glMultiTexCoord2f = NULL;
glMultiTexCoord2fvPROC glMultiTexCoord2fv = NULL;
glMultiTexCoord2iPROC glMultiTexCoord2i = NULL;
glMultiTexCoord2ivPROC glMultiTexCoord2iv = NULL;
glMultiTexCoord2sPROC glMultiTexCoord2s = NULL;
glMultiTexCoord2svPROC glMultiTexCoord2sv = NULL;
glMultiTexCoord3dPROC glMultiTexCoord3d = NULL;
glMultiTexCoord3dvPROC glMultiTexCoord3dv = NULL;
glMultiTexCoord3fPROC glMultiTexCoord3f = NULL;
glMultiTexCoord3fvPROC glMultiTexCoord3fv = NULL;
glMultiTexCoord3iPROC glMultiTexCoord3i = NULL;
glMultiTexCoord3ivPROC glMultiTexCoord3iv = NULL;
glMultiTexCoord3sPROC glMultiTexCoord3s = NULL;
glMultiTexCoord3svPROC glMultiTexCoord3sv = NULL;
glMultiTexCoord4dPROC glMultiTexCoord4d = NULL;
glMultiTexCoord4dvPROC glMultiTexCoord4dv = NULL;
glMultiTexCoord4fPROC glMultiTexCoord4f = NULL;
glMultiTexCoord4fvPROC glMultiTexCoord4fv = NULL;
glMultiTexCoord4iPROC glMultiTexCoord4i = NULL;
glMultiTexCoord4ivPROC glMultiTexCoord4iv = NULL;
glMultiTexCoord4sPROC glMultiTexCoord4s = NULL;
glMultiTexCoord4svPROC glMultiTexCoord4sv = NULL;
glLoadTransposeMatrixfPROC glLoadTransposeMatrixf = NULL;
glLoadTransposeMatrixdPROC glLoadTransposeMatrixd = NULL;
glMultTransposeMatrixfPROC glMultTransposeMatrixf = NULL;
glMultTransposeMatrixdPROC glMultTransposeMatrixd = NULL;
glCompressedTexImage3DPROC glCompressedTexImage3D = NULL;
glCompressedTexImage2DPROC glCompressedTexImage2D = NULL;
glCompressedTexImage1DPROC glCompressedTexImage1D = NULL;
glCompressedTexSubImage3DPROC glCompressedTexSubImage3D = NULL;
glCompressedTexSubImage2DPROC glCompressedTexSubImage2D = NULL;
glCompressedTexSubImage1DPROC glCompressedTexSubImage1D = NULL;
glGetCompressedTexImagePROC glGetCompressedTexImage = NULL;
glSampleCoveragePROC glSampleCoverage = NULL;
#endif /* GL_VERSION_1_3 */

/* ARB_multitexture */

#ifdef GL_ARB_multitexture
glActiveTextureARBPROC glActiveTextureARB = NULL;
glClientActiveTextureARBPROC glClientActiveTextureARB = NULL;
glMultiTexCoord1dARBPROC glMultiTexCoord1dARB = NULL;
glMultiTexCoord1dvARBPROC glMultiTexCoord1dvARB = NULL;
glMultiTexCoord1fARBPROC glMultiTexCoord1fARB = NULL;
glMultiTexCoord1fvARBPROC glMultiTexCoord1fvARB = NULL;
glMultiTexCoord1iARBPROC glMultiTexCoord1iARB = NULL;
glMultiTexCoord1ivARBPROC glMultiTexCoord1ivARB = NULL;
glMultiTexCoord1sARBPROC glMultiTexCoord1sARB = NULL;
glMultiTexCoord1svARBPROC glMultiTexCoord1svARB = NULL;
glMultiTexCoord2dARBPROC glMultiTexCoord2dARB = NULL;
glMultiTexCoord2dvARBPROC glMultiTexCoord2dvARB = NULL;
glMultiTexCoord2fARBPROC glMultiTexCoord2fARB = NULL;
glMultiTexCoord2fvARBPROC glMultiTexCoord2fvARB = NULL;
glMultiTexCoord2iARBPROC glMultiTexCoord2iARB = NULL;
glMultiTexCoord2ivARBPROC glMultiTexCoord2ivARB = NULL;
glMultiTexCoord2sARBPROC glMultiTexCoord2sARB = NULL;
glMultiTexCoord2svARBPROC glMultiTexCoord2svARB = NULL;
glMultiTexCoord3dARBPROC glMultiTexCoord3dARB = NULL;
glMultiTexCoord3dvARBPROC glMultiTexCoord3dvARB = NULL;
glMultiTexCoord3fARBPROC glMultiTexCoord3fARB = NULL;
glMultiTexCoord3fvARBPROC glMultiTexCoord3fvARB = NULL;
glMultiTexCoord3iARBPROC glMultiTexCoord3iARB = NULL;
glMultiTexCoord3ivARBPROC glMultiTexCoord3ivARB = NULL;
glMultiTexCoord3sARBPROC glMultiTexCoord3sARB = NULL;
glMultiTexCoord3svARBPROC glMultiTexCoord3svARB = NULL;
glMultiTexCoord4dARBPROC glMultiTexCoord4dARB = NULL;
glMultiTexCoord4dvARBPROC glMultiTexCoord4dvARB = NULL;
glMultiTexCoord4fARBPROC glMultiTexCoord4fARB = NULL;
glMultiTexCoord4fvARBPROC glMultiTexCoord4fvARB = NULL;
glMultiTexCoord4iARBPROC glMultiTexCoord4iARB = NULL;
glMultiTexCoord4ivARBPROC glMultiTexCoord4ivARB = NULL;
glMultiTexCoord4sARBPROC glMultiTexCoord4sARB = NULL;
glMultiTexCoord4svARBPROC glMultiTexCoord4svARB = NULL;
#endif /* GL_ARB_multitexture */

#endif /* WIN32 */

/* ARB_transpose_matrix */

#ifdef GL_ARB_transpose_matrix
glLoadTransposeMatrixfARBPROC glLoadTransposeMatrixfARB = NULL;
glLoadTransposeMatrixdARBPROC glLoadTransposeMatrixdARB = NULL;
glMultTransposeMatrixfARBPROC glMultTransposeMatrixfARB = NULL;
glMultTransposeMatrixdARBPROC glMultTransposeMatrixdARB = NULL;
#endif /* GL_ARB_transpose_matrix */

/* ARB_texture_compression */

#ifdef GL_ARB_texture_compression 
glCompressedTexImage3DARBPROC glCompressedTexImage3DARB = NULL;
glCompressedTexImage2DARBPROC glCompressedTexImage2DARB = NULL;
glCompressedTexImage1DARBPROC glCompressedTexImage1DARB = NULL; 
glCompressedTexSubImage3DARBPROC glCompressedTexSubImage3DARB = NULL;
glCompressedTexSubImage2DARBPROC glCompressedTexSubImage2DARB = NULL;
glCompressedTexSubImage1DARBPROC glCompressedTexSubImage1DARB = NULL;
glGetCompressedTexImageARBPROC glGetCompressedTexImageARB = NULL;
#endif /* GL_ARB_texture_compression */

/* EXT_secondary_color */

#ifdef GL_EXT_secondary_color
glSecondaryColor3bEXTPROC glSecondaryColor3bEXT = NULL;
glSecondaryColor3bvEXTPROC glSecondaryColor3bvEXT = NULL;
glSecondaryColor3dEXTPROC glSecondaryColor3dEXT = NULL;
glSecondaryColor3dvEXTPROC glSecondaryColor3dvEXT = NULL;
glSecondaryColor3fEXTPROC glSecondaryColor3fEXT = NULL;
glSecondaryColor3fvEXTPROC glSecondaryColor3fvEXT = NULL;
glSecondaryColor3iEXTPROC glSecondaryColor3iEXT = NULL;
glSecondaryColor3ivEXTPROC glSecondaryColor3ivEXT = NULL;
glSecondaryColor3sEXTPROC glSecondaryColor3sEXT = NULL;
glSecondaryColor3svEXTPROC glSecondaryColor3svEXT = NULL;
glSecondaryColor3ubEXTPROC glSecondaryColor3ubEXT = NULL;
glSecondaryColor3ubvEXTPROC glSecondaryColor3ubvEXT = NULL;
glSecondaryColor3uiEXTPROC glSecondaryColor3uiEXT = NULL;
glSecondaryColor3uivEXTPROC glSecondaryColor3uivEXT = NULL;
glSecondaryColor3usEXTPROC glSecondaryColor3usEXT = NULL;
glSecondaryColor3usvEXTPROC glSecondaryColor3usvEXT = NULL;
glSecondaryColorPointerEXTPROC glSecondaryColorPointerEXT = NULL;
#endif /* GL_EXT_secondary_color */

/* EXT_compiled_vertex_array */

#ifdef GL_EXT_compiled_vertex_array 
glLockArraysEXTPROC glLockArraysEXT = NULL;
glUnlockArraysEXTPROC glUnlockArraysEXT = NULL;
#endif /* GL_EXT_compiled_vertex_array */

/* EXT_fog_coord */

#ifdef GL_EXT_fog_coord
glFogCoordfEXTPROC glFogCoordfEXT = NULL;
glFogCoordfvEXTPROC glFogCoordfvEXT = NULL;
glFogCoorddEXTPROC glFogCoorddEXT = NULL;
glFogCoorddvEXTPROC glFogCoorddvEXT = NULL;
glFogCoordPointerEXTPROC glFogCoordPointerEXT = NULL;
#endif /* GL_EXT_for_color */

/* NV_vertex_array_range */

#ifdef GL_NV_vertex_array_range
glFlushVertexArrayRangeNVPROC glFlushVertexArrayRangeNV = NULL;
glVertexArrayRangeNVPROC glVertexArrayRangeNV = NULL;

#ifdef _WIN32
wglAllocateMemoryNVPROC wglAllocateMemoryNV = NULL;
wglFreeMemoryNVPROC wglFreeMemoryNV = NULL;
#else
glXAllocateMemoryNVPROC glXAllocateMemoryNV = NULL;
glXFreeMemoryNVPROC glXFreeMemoryNV = NULL;
#endif /* WIN32 */

#endif /* GL_NV_vertex_array_range */

/* EXT_point_parameters */

#ifdef GL_EXT_point_parameters
glPointParameterfEXTPROC glPointParameterfEXT = NULL;
glPointParameterfvEXTPROC glPointParameterfvEXT = NULL;
#endif /* GL_EXT_point_parameters */

/* NV_register_combiners */

#ifdef GL_NV_register_combiners
glCombinerParameterfvNVPROC glCombinerParameterfvNV = NULL;
glCombinerParameterfNVPROC  glCombinerParameterfNV = NULL;
glCombinerParameterivNVPROC glCombinerParameterivNV = NULL;
glCombinerParameteriNVPROC glCombinerParameteriNV = NULL;
glCombinerInputNVPROC glCombinerInputNV = NULL;
glCombinerOutputNVPROC glCombinerOutputNV = NULL;
glFinalCombinerInputNVPROC glFinalCombinerInputNV = NULL;
glGetCombinerInputParameterfvNVPROC glGetCombinerInputParameterfvNV = NULL;
glGetCombinerInputParameterivNVPROC glGetCombinerInputParameterivNV = NULL;
glGetCombinerOutputParameterfvNVPROC glGetCombinerOutputParameterfvNV = NULL;
glGetCombinerOutputParameterivNVPROC glGetCombinerOutputParameterivNV = NULL;
glGetFinalCombinerInputParameterfvNVPROC glGetFinalCombinerInputParameterfvNV = NULL;
glGetFinalCombinerInputParameterivNVPROC glGetFinalCombinerInputParameterivNV = NULL;
#endif /* GL_NV_register_combiners */

/* ARB_multisample */

#ifdef GL_ARB_multisample
glSampleCoverageARBPROC glSampleCoverageARB = NULL;
#endif /* GL_ARB_multisample */

/* EXT_vertex_weighting */

#ifdef GL_EXT_vertex_weighting
glVertexWeightfEXTPROC glVertexWeightfEXT = NULL;
glVertexWeightfvEXTPROC glVertexWeightfvEXT = NULL;
glVertexWeightPointerEXTPROC glVertexWeightPointerEXT = NULL;
#endif /* GL_EXT_vertex_weighting */

/* NV_vertex_program */

#ifdef GL_NV_vertex_program
glBindProgramNVPROC glBindProgramNV = NULL;
glDeleteProgramsNVPROC glDeleteProgramsNV = NULL;
glExecuteProgramNVPROC glExecuteProgramNV = NULL;
glGenProgramsNVPROC glGenProgramsNV = NULL;
glAreProgramsResidentNVPROC glAreProgramsResidentNV = NULL;
glRequestResidentProgramsNVPROC glRequestResidentProgramsNV = NULL;
glGetProgramParameterfvNVPROC glGetProgramParameterfvNV = NULL;
glGetProgramParameterdvNVPROC glGetProgramParameterdvNV = NULL;
glGetProgramivNVPROC glGetProgramivNV = NULL;
glGetProgramStringNVPROC glGetProgramStringNV = NULL;
glGetTrackMatrixivNVPROC glGetTrackMatrixivNV = NULL;
glGetVertexAttribdvNVPROC glGetVertexAttribdvNV = NULL;
glGetVertexAttribfvNVPROC glGetVertexAttribfvNV = NULL;
glGetVertexAttribivNVPROC glGetVertexAttribivNV = NULL;
glGetVertexAttribPointervNVPROC glGetVertexAttribPointervNV = NULL;
glIsProgramNVPROC glIsProgramNV = NULL;
glLoadProgramNVPROC glLoadProgramNV = NULL;
glProgramParameter4fNVPROC glProgramParameter4fNV = NULL;
glProgramParameter4dNVPROC glProgramParameter4dNV = NULL;
glProgramParameter4dvNVPROC glProgramParameter4dvNV = NULL;
glProgramParameter4fvNVPROC glProgramParameter4fvNV = NULL;
glProgramParameters4dvNVPROC glProgramParameters4dvNV = NULL;
glProgramParameters4fvNVPROC glProgramParameters4fvNV = NULL;
glTrackMatrixNVPROC glTrackMatrixNV = NULL;
glVertexAttribPointerNVPROC glVertexAttribPointerNV = NULL;
glVertexAttrib1sNVPROC glVertexAttrib1sNV = NULL;
glVertexAttrib1fNVPROC glVertexAttrib1fNV = NULL;
glVertexAttrib1dNVPROC glVertexAttrib1dNV = NULL;
glVertexAttrib2sNVPROC glVertexAttrib2sNV = NULL;
glVertexAttrib2fNVPROC glVertexAttrib2fNV = NULL;
glVertexAttrib2dNVPROC glVertexAttrib2dNV = NULL;
glVertexAttrib3sNVPROC glVertexAttrib3sNV = NULL;
glVertexAttrib3fNVPROC glVertexAttrib3fNV = NULL;
glVertexAttrib3dNVPROC glVertexAttrib3dNV = NULL;
glVertexAttrib4sNVPROC glVertexAttrib4sNV = NULL;
glVertexAttrib4fNVPROC glVertexAttrib4fNV = NULL;
glVertexAttrib4dNVPROC glVertexAttrib4dNV = NULL;
glVertexAttrib4ubNVPROC glVertexAttrib4ubNV = NULL;
glVertexAttrib1svNVPROC glVertexAttrib1svNV = NULL;
glVertexAttrib1fvNVPROC glVertexAttrib1fvNV = NULL;
glVertexAttrib1dvNVPROC glVertexAttrib1dvNV = NULL;
glVertexAttrib2svNVPROC glVertexAttrib2svNV = NULL;
glVertexAttrib2fvNVPROC glVertexAttrib2fvNV = NULL;
glVertexAttrib2dvNVPROC glVertexAttrib2dvNV = NULL;
glVertexAttrib3svNVPROC glVertexAttrib3svNV = NULL;
glVertexAttrib3fvNVPROC glVertexAttrib3fvNV = NULL;
glVertexAttrib3dvNVPROC glVertexAttrib3dvNV = NULL;
glVertexAttrib4svNVPROC glVertexAttrib4svNV = NULL;
glVertexAttrib4fvNVPROC glVertexAttrib4fvNV = NULL;
glVertexAttrib4dvNVPROC glVertexAttrib4dvNV = NULL;
glVertexAttrib4ubvNVPROC glVertexAttrib4ubvNV = NULL;
glVertexAttribs1svNVPROC glVertexAttribs1svNV = NULL;
glVertexAttribs1fvNVPROC glVertexAttribs1fvNV = NULL;
glVertexAttribs1dvNVPROC glVertexAttribs1dvNV = NULL;
glVertexAttribs2svNVPROC glVertexAttribs2svNV = NULL;
glVertexAttribs2fvNVPROC glVertexAttribs2fvNV = NULL;
glVertexAttribs2dvNVPROC glVertexAttribs2dvNV = NULL;
glVertexAttribs3svNVPROC glVertexAttribs3svNV = NULL;
glVertexAttribs3fvNVPROC glVertexAttribs3fvNV = NULL;
glVertexAttribs3dvNVPROC glVertexAttribs3dvNV = NULL;
glVertexAttribs4svNVPROC glVertexAttribs4svNV = NULL;
glVertexAttribs4fvNVPROC glVertexAttribs4fvNV = NULL;
glVertexAttribs4dvNVPROC glVertexAttribs4dvNV = NULL;
glVertexAttribs4ubvNVPROC glVertexAttribs4ubvNV = NULL;
#endif /* GL_NV_vertex_program */

/* NV_fence */

#ifdef GL_NV_fence
glGenFencesNVPROC glGenFencesNV = NULL;
glDeleteFencesNVPROC glDeleteFencesNV = NULL;
glSetFenceNVPROC glSetFenceNV = NULL;
glTestFenceNVPROC glTestFenceNV = NULL;
glFinishFenceNVPROC glFinishFenceNV = NULL;
glIsFenceNVPROC glIsFenceNV = NULL;
glGetFenceivNVPROC glGetFenceivNV = NULL;
#endif /* GL_NV_fence */

/* NV_register_combiners2 */

#ifdef GL_NV_register_combiners2
glCombinerStageParameterfvNVPROC glCombinerStageParameterfvNV = NULL;
glGetCombinerStageParameterfvNVPROC glGetCombinerStageParameterfvNV = NULL;
#endif /* GL_NV_register_combiners2 */

/* NV_evaluators */

#ifdef GL_NV_evaluators
glMapControlPointsNVPROC glMapControlPointsNV = NULL;
glMapParameterivNVPROC glMapParameterivNV = NULL;
glMapParameterfvNVPROC glMapParameterfvNV = NULL;
glGetMapControlPointsNVPROC glGetMapControlPointsNV = NULL;
glGetMapParameterivNVPROC glGetMapParameterivNV = NULL;
glGetMapParameterfvNVPROC glGetMapParameterfvNV = NULL;
glGetMapAttribParameterivNVPROC glGetMapAttribParameterivNV = NULL;
glGetMapAttribParameterfvNVPROC glGetMapAttribParameterfvNV = NULL;
glEvalMapsNVPROC glEvalMapsNV = NULL;
#endif /* GL_NV_evaluators */

/* ATI_pn_triangles */

#ifdef GL_ATI_pn_triangles 
glPNTrianglesiATIPROC glPNTrianglesiATI = NULL;
glPNTrianglesfATIPROC glPNTrianglesfATI = NULL;
#endif /* GL_ATI_pn_triangles */

/* ARB_point_parameters */

#ifdef GL_ARB_point_parameters
glPointParameterfARBPROC glPointParameterfARB = NULL;
glPointParameterfvARBPROC glPointParameterfvARB = NULL;
#endif /* GL_ABR_point_parameters */

/* ARB_vertex_blend */

#ifdef GL_ARB_vertex_blend
glWeightbvARBPROC glWeightbvARB = NULL;
glWeightsvARBPROC glWeightsvARB = NULL;
glWeightivARBPROC glWeightivARB = NULL;
glWeightfvARBPROC glWeightfvARB = NULL;
glWeightdvARBPROC glWeightdvARB = NULL;
glWeightubvARBPROC glWeightubvARB = NULL;
glWeightusvARBPROC glWeightusvARB = NULL;
glWeightuivARBPROC glWeightuivARB = NULL;
glWeightPointerARBPROC glWeightPointerARB = NULL;
glVertexBlendARBPROC glVertexBlendARB = NULL;
#endif /* GL_ARB_vertex_blend */

/* EXT_multi_draw_arrays */

#ifdef GL_EXT_multi_draw_arrays
glMultiDrawArraysEXTPROC glMultiDrawArraysEXT = NULL;
glMultiDrawElementsEXTPROC glMultiDrawElementsEXT = NULL;
#endif /* GL_EXT_multi_draw_arrays */

/* ARB_matrix_palette */

#ifdef GL_ARB_matrix_palette
glCurrentPaletteMatrixARBPROC glCurrentPaletteMatrixARB = NULL;
glMatrixIndexubvARBPROC glMatrixIndexubvARB = NULL;
glMatrixIndexusvARBPROC glMatrixIndexusvARB = NULL;
glMatrixIndexuivARBPROC glMatrixIndexuivARB = NULL;
glMatrixIndexPointerARBPROC glMatrixIndexPointerARB = NULL;
#endif /* GL_ARB_matrix_palette */

/* EXT_vertex_shader */

#ifdef GL_EXT_vertex_shader
glBeginVertexShaderEXTPROC glBeginVertexShaderEXT = NULL;
glEndVertexShaderEXTPROC glEndVertexShaderEXT = NULL;
glBindVertexShaderEXTPROC glBindVertexShaderEXT = NULL;
glGenVertexShadersEXTPROC glGenVertexShadersEXT = NULL;
glDeleteVertexShaderEXTPROC glDeleteVertexShaderEXT = NULL;
glShaderOp1EXTPROC glShaderOp1EXT = NULL;
glShaderOp2EXTPROC glShaderOp2EXT = NULL;
glShaderOp3EXTPROC glShaderOp3EXT = NULL;
glSwizzleEXTPROC glSwizzleEXT = NULL;
glWriteMaskEXTPROC glWriteMaskEXT = NULL;
glInsertComponentEXTPROC glInsertComponentEXT = NULL;
glExtractComponentEXTPROC glExtractComponentEXT = NULL;
glGenSymbolsEXTPROC glGenSymbolsEXT = NULL;
glSetInvariantEXTPROC glSetInvariantEXT = NULL;
glSetLocalConstantEXTPROC glSetLocalConstantEXT = NULL;
glVariantbvEXTPROC glVariantbvEXT = NULL;
glVariantsvEXTPROC glVariantsvEXT = NULL;
glVariantivEXTPROC glVariantivEXT = NULL;
glVariantfvEXTPROC glVariantfvEXT = NULL;
glVariantdvEXTPROC glVariantdvEXT = NULL;
glVariantubvEXTPROC glVariantubvEXT = NULL;
glVariantusvEXTPROC glVariantusvEXT = NULL;
glVariantuivEXTPROC glVariantuivEXT = NULL;
glVariantPointerEXTPROC glVariantPointerEXT = NULL;
glEnableVariantClientStateEXTPROC glEnableVariantClientStateEXT = NULL;
glDisableVariantClientStateEXTPROC glDisableVariantClientStateEXT = NULL;
glBindLightParameterEXTPROC glBindLightParameterEXT = NULL;
glBindMaterialParameterEXTPROC glBindMaterialParameterEXT = NULL;
glBindTexGenParameterEXTPROC glBindTexGenParameterEXT = NULL;
glBindTextureUnitParameterEXTPROC glBindTextureUnitParameterEXT = NULL;
glBindParameterEXTPROC glBindParameterEXT = NULL;
glIsVariantEnabledEXTPROC glIsVariantEnabledEXT = NULL;
glGetVariantBooleanvEXTPROC glGetVariantBooleanvEXT = NULL;
glGetVariantIntegervEXTPROC glGetVariantIntegervEXT = NULL;
glGetVariantFloatvEXTPROC glGetVariantFloatvEXT = NULL;
glGetVariantPointervEXTPROC glGetVariantPointervEXT = NULL;
glGetInvariantBooleanvEXTPROC glGetInvariantBooleanvEXT = NULL;
glGetInvariantIntegervEXTPROC glGetInvariantIntegervEXT = NULL;
glGetInvariantFloatvEXTPROC glGetInvariantFloatvEXT = NULL;
glGetLocalConstantBooleanvEXTPROC glGetLocalConstantBooleanvEXT = NULL;
glGetLocalConstantIntegervEXTPROC glGetLocalConstantIntegervEXT = NULL;
glGetLocalConstantFloatvEXTPROC glGetLocalConstantFloatvEXT = NULL;
#endif /* GL_EXT_vertex_shader */

/* ATI_envmap_bumpmap */

#ifdef GL_ATI_envmap_bumpmap
glTexBumpParameterivATIPROC glTexBumpParameterivATI = NULL;
glTexBumpParameterfvATIPROC glTexBumpParameterfvATI = NULL;
glGetTexBumpParameterivATIPROC glGetTexBumpParameterivATI = NULL;
glGetTexBumpParameterfvATIPROC glGetTexBumpParameterfvATI = NULL;
#endif /* GL_ATI_envmap_bumpmap */

/* ATI_fragment_shader */

#ifdef GL_ATI_fragment_shader
glGenFragmentShadersATIPROC glGenFragmentShadersATI = NULL;
glBindFragmentShaderATIPROC glBindFragmentShaderATI = NULL;
glDeleteFragmentShaderATIPROC glDeleteFragmentShaderATI = NULL;
glBeginFragmentShaderATIPROC glBeginFragmentShaderATI = NULL;
glEndFragmentShaderATIPROC glEndFragmentShaderATI = NULL;
glPassTexCoordATIPROC glPassTexCoordATI = NULL;
glSampleMapATIPROC glSampleMapATI = NULL;
glColorFragmentOp1ATIPROC glColorFragmentOp1ATI = NULL;
glColorFragmentOp2ATIPROC glColorFragmentOp2ATI = NULL;
glColorFragmentOp3ATIPROC glColorFragmentOp3ATI = NULL;
glAlphaFragmentOp1ATIPROC glAlphaFragmentOp1ATI = NULL;
glAlphaFragmentOp2ATIPROC glAlphaFragmentOp2ATI = NULL;
glAlphaFragmentOp3ATIPROC glAlphaFragmentOp3ATI = NULL;
glSetFragmentShaderConstantATIPROC glSetFragmentShaderConstantATI = NULL;
#endif /* GL_ATI_fragment_shader */

/* ATI_element_array */

#ifdef GL_ATI_element_array 
glElementPointerATIPROC glElementPointerATI = NULL;
glDrawElementArrayATIPROC glDrawElementArrayATI = NULL;
glDrawRangeElementArrayATIPROC glDrawRangeElementArrayATI = NULL;
#endif /* GL_ATI_element_array */

/* ATI_vertex_streams */

#ifdef GL_ATI_vertex_streams
glClientActiveVertexStreamATIPROC glClientActiveVertexStreamATI = NULL;
glVertexBlendEnviATIPROC glVertexBlendEnviATI = NULL;
glVertexBlendEnvfATIPROC glVertexBlendEnvfATI = NULL;
glVertexStream2sATIPROC glVertexStream2sATI = NULL;
glVertexStream2svATIPROC glVertexStream2svATI = NULL;
glVertexStream2iATIPROC glVertexStream2iATI = NULL;
glVertexStream2ivATIPROC glVertexStream2ivATI = NULL;
glVertexStream2fATIPROC glVertexStream2fATI = NULL;
glVertexStream2fvATIPROC glVertexStream2fvATI = NULL;
glVertexStream2dATIPROC glVertexStream2dATI = NULL;
glVertexStream2dvATIPROC glVertexStream2dvATI = NULL;
glVertexStream3sATIPROC glVertexStream3sATI = NULL;
glVertexStream3svATIPROC glVertexStream3svATI = NULL;
glVertexStream3iATIPROC glVertexStream3iATI = NULL;
glVertexStream3ivATIPROC glVertexStream3ivATI = NULL;
glVertexStream3fATIPROC glVertexStream3fATI = NULL;
glVertexStream3fvATIPROC glVertexStream3fvATI = NULL;
glVertexStream3dATIPROC glVertexStream3dATI = NULL;
glVertexStream3dvATIPROC glVertexStream3dvATI = NULL;
glVertexStream4sATIPROC glVertexStream4sATI = NULL;
glVertexStream4svATIPROC glVertexStream4svATI = NULL;
glVertexStream4iATIPROC glVertexStream4iATI = NULL;
glVertexStream4ivATIPROC glVertexStream4ivATI = NULL;
glVertexStream4fATIPROC glVertexStream4fATI = NULL;
glVertexStream4fvATIPROC glVertexStream4fvATI = NULL;
glVertexStream4dATIPROC glVertexStream4dATI = NULL;
glVertexStream4dvATIPROC glVertexStream4dvATI = NULL;
glNormalStream3bATIPROC glNormalStream3bATI = NULL;
glNormalStream3bvATIPROC glNormalStream3bvATI = NULL;
glNormalStream3sATIPROC glNormalStream3sATI = NULL;
glNormalStream3svATIPROC glNormalStream3svATI = NULL;
glNormalStream3iATIPROC glNormalStream3iATI = NULL;
glNormalStream3ivATIPROC glNormalStream3ivATI = NULL;
glNormalStream3fATIPROC glNormalStream3fATI = NULL;
glNormalStream3fvATIPROC glNormalStream3fvATI = NULL;
glNormalStream3dATIPROC glNormalStream3dATI = NULL;
glNormalStream3dvATIPROC glNormalStream3dvATI = NULL;
#endif /* GL_ATI_vertex_streams */

/* ATI_vertex_array_object */

#ifdef GL_ATI_vertex_array_object
glNewObjectBufferATIPROC glNewObjectBufferATI = NULL;
glIsObjectBufferATIPROC glIsObjectBufferATI = NULL;
glUpdateObjectBufferATIPROC glUpdateObjectBufferATI = NULL;
glGetObjectBufferfvATIPROC glGetObjectBufferfvATI = NULL;
glGetObjectBufferivATIPROC glGetObjectBufferivATI = NULL;
glFreeObjectBufferATIPROC glFreeObjectBufferATI = NULL;
glArrayObjectATIPROC glArrayObjectATI = NULL;
glGetArrayObjectfvATIPROC glGetArrayObjectfvATI = NULL;
glGetArrayObjectivATIPROC glGetArrayObjectivATI = NULL;
glVariantArrayObjectATIPROC glVariantArrayObjectATI = NULL;
glGetVariantArrayObjectfvATIPROC glGetVariantArrayObjectfvATI = NULL;
glGetVariantArrayObjectivATIPROC glGetVariantArrayObjectivATI = NULL;
#endif /* GL_ATI_vertex_array_object */

/* NV_occlusion_query */

#ifdef GL_NV_occlusion_query
glGenOcclusionQueriesNVPROC glGenOcclusionQueriesNV = NULL;
glDeleteOcclusionQueriesNVPROC glDeleteOcclusionQueriesNV = NULL;
glIsOcclusionQueryNVPROC glIsOcclusionQueryNV = NULL;
glBeginOcclusionQueryNVPROC glBeginOcclusionQueryNV = NULL;
glEndOcclusionQueryNVPROC glEndOcclusionQueryNV = NULL;
glGetOcclusionQueryivNVPROC glGetOcclusionQueryivNV = NULL;
glGetOcclusionQueryuivNVPROC glGetOcclusionQueryuivNV = NULL;
#endif /* GL_NV_occlusion_query */

/* NV_point_sprite */

#ifdef GL_NV_point_sprite
glPointParameteriNVPROC glPointParameteriNV = NULL;
glPointParameterivNVPROC glPointParameterivNV = NULL;
#endif /* GL_NV_point_sprite */

/* ARB_window_pos */

#ifdef GL_ARB_window_pos
glWindowPos2dARBPROC glWindowPos2dARB = NULL;
glWindowPos2fARBPROC glWindowPos2fARB = NULL;
glWindowPos2iARBPROC glWindowPos2iARB = NULL;
glWindowPos2sARBPROC glWindowPos2sARB = NULL;
glWindowPos2dvARBPROC glWindowPos2dvARB = NULL;
glWindowPos2fvARBPROC glWindowPos2fvARB = NULL;
glWindowPos2ivARBPROC glWindowPos2ivARB = NULL;
glWindowPos2svARBPROC glWindowPos2svARB = NULL;
glWindowPos3dARBPROC glWindowPos3dARB = NULL;
glWindowPos3fARBPROC glWindowPos3fARB = NULL;
glWindowPos3iARBPROC glWindowPos3iARB = NULL;
glWindowPos3sARBPROC glWindowPos3sARB = NULL;
glWindowPos3dvARBPROC glWindowPos3dvARB = NULL;
glWindowPos3fvARBPROC glWindowPos3fvARB = NULL;
glWindowPos3ivARBPROC glWindowPos3ivARB = NULL;
glWindowPos3svARBPROC glWindowPos3svARB = NULL;
#endif /* GL_ARB_window_pos */

/* EXT_draw_range_elements */

#ifdef GL_EXT_draw_range_elements
glDrawRangeElementsEXTPROC glDrawRangeElementsEXT = NULL;
#endif /* GL_EXT_draw_range_elements  */

/* EXT_stencil_two_side */

#ifdef GL_EXT_stencil_two_side
glActiveStencilFaceEXTPROC glActiveStencilFaceEXT = NULL;
#endif /* GL_EXT_stencil_two_side */

/* ARB_vertex_program */

#ifdef GL_ARB_vertex_program
glVertexAttrib1sARBPROC glVertexAttrib1sARB = NULL;
glVertexAttrib1fARBPROC glVertexAttrib1fARB = NULL;
glVertexAttrib1dARBPROC glVertexAttrib1dARB = NULL;
glVertexAttrib2sARBPROC glVertexAttrib2sARB = NULL;
glVertexAttrib2fARBPROC glVertexAttrib2fARB = NULL;
glVertexAttrib2dARBPROC glVertexAttrib2dARB = NULL;
glVertexAttrib3sARBPROC glVertexAttrib3sARB = NULL;
glVertexAttrib3fARBPROC glVertexAttrib3fARB = NULL;
glVertexAttrib3dARBPROC glVertexAttrib3dARB = NULL;
glVertexAttrib4sARBPROC glVertexAttrib4sARB = NULL;
glVertexAttrib4fARBPROC glVertexAttrib4fARB = NULL;
glVertexAttrib4dARBPROC glVertexAttrib4dARB = NULL;
glVertexAttrib4NubARBPROC glVertexAttrib4NubARB = NULL;
glVertexAttrib1svARBPROC glVertexAttrib1svARB = NULL;
glVertexAttrib1fvARBPROC glVertexAttrib1fvARB = NULL;
glVertexAttrib1dvARBPROC glVertexAttrib1dvARB = NULL;
glVertexAttrib2svARBPROC glVertexAttrib2svARB = NULL;
glVertexAttrib2fvARBPROC glVertexAttrib2fvARB = NULL;
glVertexAttrib2dvARBPROC glVertexAttrib2dvARB = NULL;
glVertexAttrib3svARBPROC glVertexAttrib3svARB = NULL;
glVertexAttrib3fvARBPROC glVertexAttrib3fvARB = NULL;
glVertexAttrib3dvARBPROC glVertexAttrib3dvARB = NULL;
glVertexAttrib4bvARBPROC glVertexAttrib4bvARB = NULL;
glVertexAttrib4svARBPROC glVertexAttrib4svARB = NULL;
glVertexAttrib4ivARBPROC glVertexAttrib4ivARB = NULL;
glVertexAttrib4ubvARBPROC glVertexAttrib4ubvARB = NULL;
glVertexAttrib4usvARBPROC glVertexAttrib4usvARB = NULL;
glVertexAttrib4uivARBPROC glVertexAttrib4uivARB = NULL;
glVertexAttrib4fvARBPROC glVertexAttrib4fvARB = NULL;
glVertexAttrib4dvARBPROC glVertexAttrib4dvARB = NULL;
glVertexAttrib4NbvARBPROC glVertexAttrib4NbvARB = NULL;
glVertexAttrib4NsvARBPROC glVertexAttrib4NsvARB = NULL;
glVertexAttrib4NivARBPROC glVertexAttrib4NivARB = NULL;
glVertexAttrib4NubvARBPROC glVertexAttrib4NubvARB = NULL;
glVertexAttrib4NusvARBPROC glVertexAttrib4NusvARB = NULL;
glVertexAttrib4NuivARBPROC glVertexAttrib4NuivARB = NULL;
glVertexAttribPointerARBPROC glVertexAttribPointerARB = NULL;
glEnableVertexAttribArrayARBPROC glEnableVertexAttribArrayARB = NULL;
glDisableVertexAttribArrayARBPROC glDisableVertexAttribArrayARB = NULL;
glProgramStringARBPROC glProgramStringARB = NULL;
glBindProgramARBPROC glBindProgramARB = NULL;
glDeleteProgramsARBPROC glDeleteProgramsARB = NULL;
glGenProgramsARBPROC glGenProgramsARB = NULL;
glProgramEnvParameter4dARBPROC glProgramEnvParameter4dARB = NULL;
glProgramEnvParameter4dvARBPROC glProgramEnvParameter4dvARB = NULL;
glProgramEnvParameter4fARBPROC glProgramEnvParameter4fARB = NULL;
glProgramEnvParameter4fvARBPROC glProgramEnvParameter4fvARB = NULL;
glProgramLocalParameter4dARBPROC glProgramLocalParameter4dARB = NULL;
glProgramLocalParameter4dvARBPROC glProgramLocalParameter4dvARB = NULL;
glProgramLocalParameter4fARBPROC glProgramLocalParameter4fARB = NULL;
glProgramLocalParameter4fvARBPROC glProgramLocalParameter4fvARB = NULL;
glGetProgramEnvParameterdvARBPROC glGetProgramEnvParameterdvARB = NULL;
glGetProgramEnvParameterfvARBPROC glGetProgramEnvParameterfvARB = NULL;
glGetProgramLocalParameterdvARBPROC glGetProgramLocalParameterdvARB = NULL;
glGetProgramLocalParameterfvARBPROC glGetProgramLocalParameterfvARB = NULL;
glGetProgramivARBPROC glGetProgramivARB = NULL;
glGetProgramStringARBPROC glGetProgramStringARB = NULL;
glGetVertexAttribdvARBPROC glGetVertexAttribdvARB = NULL;
glGetVertexAttribfvARBPROC glGetVertexAttribfvARB = NULL;
glGetVertexAttribivARBPROC glGetVertexAttribivARB = NULL;
glGetVertexAttribPointervARBPROC glGetVertexAttribPointervARB = NULL;
glIsProgramARBPROC glIsProgramARB = NULL;
#endif /* GL_ARB_vertex_program */

/* EXT_cull_vertex */

#ifdef GL_EXT_cull_vertex
glCullParameterfvEXTPROC glCullParameterfvEXT = NULL;
glCullParameterdvEXTPROC glCullParameterdvEXT = NULL;
#endif /* GL_EXT_cull_vertex */

#ifdef GL_EXT_blend_function_sepatate
glBlendFuncSeparateEXTPROC glBlendFuncSeparateEXT = NULL;
glBlendFuncSeparateINGRPROC glBlendFuncSeparateINGR = NULL;
#endif /* GL_EXT_blend_func_separate */

#ifdef _WIN32
#ifdef GL_VERSION_1_4
/*#ifndef GL_VERSION_1_2
glBlendColorPROC glBlendColor = NULL;
glBlendEquationPROC glBlendEquation = NULL;
#endif *//* GL_VERSION_1_2 */
glFogCoordfPROC glFogCoordf = NULL;
glFogCoordfvPROC glFogCoordfv = NULL;
glFogCoorddPROC glFogCoordd = NULL;
glFogCoorddvPROC glFogCoorddv = NULL;
glFogCoordPointerPROC glFogCoordPointer = NULL;
glMultiDrawArraysPROC glMultiDrawArrays = NULL;
glMultiDrawElementsPROC glMultiDrawElements = NULL;
glPointParameterfPROC glPointParameterf = NULL;
glPointParameterfvPROC glPointParameterfv = NULL;
glSecondaryColor3bPROC glSecondaryColor3b = NULL;
glSecondaryColor3bvPROC glSecondaryColor3bv = NULL;
glSecondaryColor3dPROC glSecondaryColor3d = NULL;
glSecondaryColor3dvPROC glSecondaryColor3dv = NULL;
glSecondaryColor3fPROC glSecondaryColor3f = NULL;
glSecondaryColor3fvPROC glSecondaryColor3fv = NULL;
glSecondaryColor3iPROC glSecondaryColor3i = NULL;
glSecondaryColor3ivPROC glSecondaryColor3iv = NULL;
glSecondaryColor3sPROC glSecondaryColor3s = NULL;
glSecondaryColor3svPROC glSecondaryColor3sv = NULL;
glSecondaryColor3ubPROC glSecondaryColor3ub = NULL;
glSecondaryColor3ubvPROC glSecondaryColor3ubv = NULL;
glSecondaryColor3uiPROC glSecondaryColor3ui = NULL;
glSecondaryColor3uivPROC glSecondaryColor3uiv = NULL;
glSecondaryColor3usPROC glSecondaryColor3us = NULL;
glSecondaryColor3usvPROC glSecondaryColor3usv = NULL;
glSecondaryColorPointerPROC glSecondaryColorPointer = NULL;
glBlendFuncSeparatePROC glBlendFuncSeparate = NULL;
glWindowPos2dPROC glWindowPos2d = NULL;
glWindowPos2fPROC glWindowPos2f = NULL;
glWindowPos2iPROC glWindowPos2i = NULL;
glWindowPos2sPROC glWindowPos2s = NULL;
glWindowPos2dvPROC glWindowPos2dv = NULL;
glWindowPos2fvPROC glWindowPos2fv = NULL;
glWindowPos2ivPROC glWindowPos2iv = NULL;
glWindowPos2svPROC glWindowPos2sv = NULL;
glWindowPos3dPROC glWindowPos3d = NULL;
glWindowPos3fPROC glWindowPos3f = NULL;
glWindowPos3iPROC glWindowPos3i = NULL;
glWindowPos3sPROC glWindowPos3s = NULL;
glWindowPos3dvPROC glWindowPos3dv = NULL;
glWindowPos3fvPROC glWindowPos3fv = NULL;
glWindowPos3ivPROC glWindowPos3iv = NULL;
glWindowPos3svPROC glWindowPos3sv = NULL;
#endif /* GL_VERSION_1_4 */
#endif /* WIN32 */

#ifdef GL_EXT_blend_func_separate
glBlendFuncSeparateEXTPROC glBlendFuncSeparateEXT = NULL;
#endif /* GL_EXT_blend_func_separate */


#ifdef GL_NV_element_array
glElementPointerNVPROC glElementPointerNV = NULL;
glDrawElementArrayNVPROC glDrawElementArrayNV = NULL;
glDrawRangeElementArrayNVPROC glDrawRangeElementArrayNV = NULL;
glMultiDrawElementArrayNVPROC glMultiDrawElementArrayNV = NULL;
glMultiDrawRangeElementArrayNVPROC glMultiDrawRangeElementArrayNV = NULL;
#endif /* GL_NV_element_array */

#ifdef GL_NV_fragment_program
glProgramNamedParameter4fNVPROC glProgramNamedParameter4fNV = NULL;
glProgramNamedParameter4dNVPROC glProgramNamedParameter4dNV = NULL;
glProgramNamedParameter4fvNVPROC glProgramNamedParameter4fvNV = NULL;
glProgramNamedParameter4dvNVPROC glProgramNamedParameter4dvNV = NULL;
glGetProgramNamedParameterfvNVPROC glGetProgramNamedParameterfvNV = NULL;
glGetProgramNamedParameterdvNVPROC glGetProgramNamedParameterdvNV = NULL;
#ifndef GL_ARB_vertex_program
glProgramLocalParameter4dARBPROC glProgramLocalParameter4dARB = NULL;
glProgramLocalParameter4dvARBPROC glProgramLocalParameter4dvARB = NULL;
glProgramLocalParameter4fARBPROC glProgramLocalParameter4fARB = NULL;
glProgramLocalParameter4fvARBPROC glProgramLocalParameter4fvARB = NULL;
glGetProgramLocalParameterdvARBPROC glGetProgramLocalParameterdvARB = NULL;
glGetProgramLocalParameterfvARBPROC glGetProgramLocalParameterfvARB = NULL;
#endif /* GL_ARB_vertex_program */
#endif /* GL_NV_fragment_program */


#ifdef GL_NV_primitive_restart
glPrimitiveRestartNVPROC glPrimitiveRestartNV = NULL;
glPrimitiveRestartIndexNVPROC glPrimitiveRestartIndexNV = NULL;
#endif /* GL_NV_primitive_restart */

// added -ec
#ifdef GL_ATI_draw_buffers
PFNGLDRAWBUFFERS glDrawBuffersATI;
#endif

static int extgl_error = 0;

struct ExtensionTypes extgl_Extensions;

struct ExtensionTypes SupportedExtensions; /* deprecated, please do not use */


/* getProcAddress */

void *extgl_GetProcAddress(const char *name)
{
#ifdef _WIN32
    void *t = wglGetProcAddress(name);
    if (t == NULL)
    {
        extgl_error = 1;  
    }
    return t;
#else
    void *t = (void*)glXGetProcAddressARB((GLubyte *)name);
    if (t == NULL)
    {
        extgl_error = 1;
    }
    return t;
#endif
}

/*-----------------------------------------------------*/
/* WGL stuff */
/*-----------------------------------------------------*/

#ifdef _WIN32

/** returns true if the extention is available */
int QueryWGLExtension(const char *name)
{
    const GLubyte *extensions;
    const GLubyte *start;
    GLubyte *where, *terminator;

    /* Extension names should not have spaces. */
    where = (GLubyte *) strchr(name, ' ');
    if (where || *name == '\0')
        return 0;
    if (wglGetExtensionsStringARB == NULL)
        if (wglGetExtensionsStringEXT == NULL)
            return 0;
        else
            extensions = (GLubyte*)wglGetExtensionsStringEXT();
    else
        extensions = (GLubyte*)wglGetExtensionsStringARB(wglGetCurrentDC());
    /* It takes a bit of care to be fool-proof about parsing the
         OpenGL extensions string. Don't be fooled by sub-strings,
        etc. */
    start = extensions;
    for (;;) 
    {
        where = (GLubyte *) strstr((const char *) start, name);
        if (!where)
            break;
        terminator = where + strlen(name);
        if (where == start || *(where - 1) == ' ')
            if (*terminator == ' ' || *terminator == '\0')
                return 1;
        start = terminator;
    }
    return 0;
}

void extgl_InitWGLARBBufferRegion()
{
#ifdef WGL_ARB_buffer_region    
    if (!extgl_Extensions.wgl.ARB_buffer_region)
        return;
    wglCreateBufferRegionARB = (wglCreateBufferRegionARBPROC) extgl_GetProcAddress("wglCreateBufferRegionARB");
    wglDeleteBufferRegionARB = (wglDeleteBufferRegionARBPROC) extgl_GetProcAddress("wglDeleteBufferRegionARB");
    wglSaveBufferRegionARB = (wglSaveBufferRegionARBPROC) extgl_GetProcAddress("wglSaveBufferRegionARB");
    wglRestoreBufferRegionARB = (wglRestoreBufferRegionARBPROC) extgl_GetProcAddress("wglRestoreBufferRegionARB");
#endif
}

void extgl_InitWGLARBPbuffer()
{
#ifdef WGL_ARB_pbuffer
    if (!extgl_Extensions.wgl.ARB_pbuffer)
        return;
    wglCreatePbufferARB = (wglCreatePbufferARBPROC) extgl_GetProcAddress("wglCreatePbufferARB");
    wglGetPbufferDCARB = (wglGetPbufferDCARBPROC) extgl_GetProcAddress("wglGetPbufferDCARB");
    wglReleasePbufferDCARB = (wglReleasePbufferDCARBPROC) extgl_GetProcAddress("wglReleasePbufferDCARB");
    wglDestroyPbufferARB = (wglDestroyPbufferARBPROC) extgl_GetProcAddress("wglDestroyPbufferARB");
    wglQueryPbufferARB = (wglQueryPbufferARBPROC) extgl_GetProcAddress("wglQueryPbufferARB");
#endif
}

void extgl_InitWGLARBPixelFormat()
{
#ifdef WGL_ARB_pixel_format
    if (!extgl_Extensions.wgl.ARB_pixel_format)
        return;
    wglGetPixelFormatAttribivARB = (wglGetPixelFormatAttribivARBPROC) extgl_GetProcAddress("wglGetPixelFormatAttribivARB");
    wglGetPixelFormatAttribfvARB = (wglGetPixelFormatAttribfvARBPROC) extgl_GetProcAddress("wglGetPixelFormatAttribfvARB");
    wglChoosePixelFormatARB = (wglChoosePixelFormatARBPROC) extgl_GetProcAddress("wglChoosePixelFormatARB");
#endif
}

void extgl_InitWGLARBRenderTexture()
{
#ifdef WGL_ARB_render_texture
    if (!extgl_Extensions.wgl.ARB_render_texture)
        return;
    wglBindTexImageARB = (wglBindTexImageARBPROC) extgl_GetProcAddress("wglBindTexImageARB");
    wglReleaseTexImageARB = (wglReleaseTexImageARBPROC) extgl_GetProcAddress("wglReleaseTexImageARB");
    wglSetPbufferAttribARB = (wglSetPbufferAttribARBPROC) extgl_GetProcAddress("wglSetPbufferAttribARB");
#endif
}

void extgl_InitWGLEXTSwapControl()
{
#ifdef WGL_EXT_swap_control
    if (!extgl_Extensions.wgl.EXT_swap_control)
        return;
    wglSwapIntervalEXT = (wglSwapIntervalEXTPROC) extgl_GetProcAddress("wglSwapIntervalEXT");
    wglGetSwapIntervalEXT = (wglGetSwapIntervalEXTPROC) extgl_GetProcAddress("wglGetSwapIntervalEXT");
#endif
}

void extgl_InitWGLARBMakeCurrentRead()
{
#ifdef WGL_ARB_make_current_read
    if (!extgl_Extensions.wgl.ARB_make_current_read)
        return;
    wglMakeContextCurrentARB = (wglMakeContextCurrentARBPROC) extgl_GetProcAddress("wglMakeContextCurrentARB");
    wglGetCurrentReadDCARB = (wglGetCurrentReadDCARBPROC) extgl_GetProcAddress("wglGetCurrentReadDCARB");
#endif
}

void extgl_InitSupportedWGLExtensions()
{
    extgl_Extensions.wgl.ARB_buffer_region = QueryWGLExtension("WGL_ARB_buffer_region");
    extgl_Extensions.wgl.ARB_make_current_read = QueryWGLExtension("WGL_ARB_make_current_read");
    extgl_Extensions.wgl.ARB_multisample = QueryWGLExtension("WGL_ARB_multisample");
    extgl_Extensions.wgl.ARB_pbuffer = QueryWGLExtension("WGL_ARB_pbuffer");
    extgl_Extensions.wgl.ARB_pixel_format = QueryWGLExtension("WGL_ARB_pixel_format");
    extgl_Extensions.wgl.ARB_render_texture = QueryWGLExtension("WGL_ARB_render_texture");
    extgl_Extensions.wgl.EXT_swap_control = QueryWGLExtension("WGL_EXT_swap_control");
    extgl_Extensions.wgl.NV_render_depth_texture = QueryWGLExtension("WGL_NV_render_depth_texture");
    extgl_Extensions.wgl.NV_render_texture_rectangle = QueryWGLExtension("WGL_NV_render_texture_rectangle");
    extgl_Extensions.wgl.ATI_pixel_format_float = QueryWGLExtension("WGL_ATI_pixel_format_float"); // added -ec
}

int extgl_InitializeWGL()
{
    extgl_error = 0;
    wglGetExtensionsStringARB = (wglGetExtensionsStringARBPROC) extgl_GetProcAddress("wglGetExtensionsStringARB");
    wglGetExtensionsStringEXT = (wglGetExtensionsStringEXTPROC) extgl_GetProcAddress("wglGetExtensionsStringEXT");
    extgl_Extensions.wgl.ARB_extensions_string = wglGetExtensionsStringARB != NULL;
    extgl_Extensions.wgl.EXT_extensions_string = wglGetExtensionsStringEXT != NULL;
    extgl_error = 0;

    extgl_InitSupportedWGLExtensions();
   

    extgl_InitWGLARBMakeCurrentRead();
    extgl_InitWGLEXTSwapControl();
    extgl_InitWGLARBRenderTexture();
    extgl_InitWGLARBPixelFormat();
    extgl_InitWGLARBPbuffer();
    extgl_InitWGLARBBufferRegion();
    
    return extgl_error;
}

#endif /* WIN32 */

/*-----------------------------------------------------*/
/* WGL stuff END*/
/*-----------------------------------------------------*/

/** returns true if the extention is available */
int QueryExtension(const char *name)
{
    const GLubyte *extensions;
    const GLubyte *start;
    GLubyte *where, *terminator;

    /* Extension names should not have spaces. */
    where = (GLubyte *) strchr(name, ' ');
    if (where || *name == '\0')
        return 0;
    extensions = glGetString(GL_EXTENSIONS);
    /* It takes a bit of care to be fool-proof about parsing the
         OpenGL extensions string. Don't be fooled by sub-strings,
        etc. */
    start = extensions;
    for (;;) 
    {
        where = (GLubyte *) strstr((const char *) start, name);
        if (!where)
            break;
        terminator = where + strlen(name);
        if (where == start || *(where - 1) == ' ')
            if (*terminator == ' ' || *terminator == '\0')
                return 1;
        start = terminator;
    }
    return 0;
}

// added -ec
/* ATI_draw_buffers */
void extgl_InitATIDrawBuffers()
{
#ifdef GL_ATI_draw_buffers
    if (!extgl_Extensions.ATI_draw_buffers)
        return;
    glDrawBuffersATI = (PFNGLDRAWBUFFERS) extgl_GetProcAddress("glDrawBuffersATI");
#endif
}

void extgl_InitARBFragmentProgram()
{
#ifdef GL_ARB_fragment_program
    if (!extgl_Extensions.ARB_fragment_program)
        return;
    glProgramStringARB = (glProgramStringARBPROC) extgl_GetProcAddress("glProgramStringARB");
    glBindProgramARB = (glBindProgramARBPROC) extgl_GetProcAddress("glBindProgramARB");
    glDeleteProgramsARB = (glDeleteProgramsARBPROC) extgl_GetProcAddress("glDeleteProgramsARB");
    glGenProgramsARB = (glGenProgramsARBPROC) extgl_GetProcAddress("glGenProgramsARB");
    glProgramEnvParameter4dARB = (glProgramEnvParameter4dARBPROC) extgl_GetProcAddress("glProgramEnvParameter4dARB");
    glProgramEnvParameter4dvARB = (glProgramEnvParameter4dvARBPROC) extgl_GetProcAddress("glProgramEnvParameter4dvARB");
    glProgramEnvParameter4fARB = (glProgramEnvParameter4fARBPROC) extgl_GetProcAddress("glProgramEnvParameter4fARB");
    glProgramEnvParameter4fvARB = (glProgramEnvParameter4fvARBPROC) extgl_GetProcAddress("glProgramEnvParameter4fvARB");
    glProgramLocalParameter4dARB = (glProgramLocalParameter4dARBPROC) extgl_GetProcAddress("glProgramLocalParameter4dARB");
    glProgramLocalParameter4dvARB = (glProgramLocalParameter4dvARBPROC) extgl_GetProcAddress("glProgramLocalParameter4dvARB");
    glProgramLocalParameter4fARB = (glProgramLocalParameter4fARBPROC) extgl_GetProcAddress("glProgramLocalParameter4fARB");
    glProgramLocalParameter4fvARB = (glProgramLocalParameter4fvARBPROC) extgl_GetProcAddress("glProgramLocalParameter4fvARB");
    glGetProgramEnvParameterdvARB = (glGetProgramEnvParameterdvARBPROC) extgl_GetProcAddress("glGetProgramEnvParameterdvARB");
    glGetProgramEnvParameterfvARB = (glGetProgramEnvParameterfvARBPROC) extgl_GetProcAddress("glGetProgramEnvParameterfvARB");
    glGetProgramLocalParameterdvARB = (glGetProgramLocalParameterdvARBPROC) extgl_GetProcAddress("glGetProgramLocalParameterdvARB");
    glGetProgramLocalParameterfvARB = (glGetProgramLocalParameterfvARBPROC) extgl_GetProcAddress("glGetProgramLocalParameterfvARB");
    glGetProgramivARB = (glGetProgramivARBPROC) extgl_GetProcAddress("glGetProgramivARB");
    glGetProgramStringARB = (glGetProgramStringARBPROC) extgl_GetProcAddress("glGetProgramStringARB");
    glIsProgramARB = (glIsProgramARBPROC) extgl_GetProcAddress("glIsProgramARB");
#endif
}

void extgl_InitNVPrimitiveRestart()
{
#ifdef GL_NV_primitive_restart
    if (!extgl_Extensions.NV_primitive_restart)
        return;
    glPrimitiveRestartNV = (glPrimitiveRestartNVPROC) extgl_GetProcAddress("glPrimitiveRestartNV");
    glPrimitiveRestartIndexNV = (glPrimitiveRestartIndexNVPROC) extgl_GetProcAddress("glPrimitiveRestartIndexNV");
#endif /* GL_NV_primitive_restart */
}

void extgl_InitNVFragmentProgram()
{
#ifdef GL_NV_fragment_program
    if (!extgl_Extensions.NV_fragment_program)
        return;
    glProgramNamedParameter4fNV = (glProgramNamedParameter4fNVPROC) extgl_GetProcAddress("glProgramNamedParameter4fNV");
    glProgramNamedParameter4dNV = (glProgramNamedParameter4dNVPROC) extgl_GetProcAddress("glProgramNamedParameter4dNV");
    glProgramNamedParameter4fvNV = (glProgramNamedParameter4fvNVPROC) extgl_GetProcAddress("glProgramNamedParameter4fvNV");
    glProgramNamedParameter4dvNV = (glProgramNamedParameter4dvNVPROC) extgl_GetProcAddress("glProgramNamedParameter4dvNV");
    glGetProgramNamedParameterfvNV = (glGetProgramNamedParameterfvNVPROC) extgl_GetProcAddress("glGetProgramNamedParameterfvNV");
    glGetProgramNamedParameterdvNV = (glGetProgramNamedParameterdvNVPROC) extgl_GetProcAddress("glGetProgramNamedParameterdvNV");
#ifndef GL_ARB_vertex_program
    glProgramLocalParameter4dARB = (glProgramLocalParameter4dARBPROC) extgl_GetProcAddress("glProgramLocalParameter4dARB");
    glProgramLocalParameter4dvARB = (glProgramLocalParameter4dvARBPROC) extgl_GetProcAddress("glProgramLocalParameter4dvARB");
    glProgramLocalParameter4fARB = (glProgramLocalParameter4fARBPROC) extgl_GetProcAddress("glProgramLocalParameter4fARB");
    glProgramLocalParameter4fvARB = (glProgramLocalParameter4fvARBPROC) extgl_GetProcAddress("glProgramLocalParameter4fvARB");
    glGetProgramLocalParameterdvARB = (glGetProgramLocalParameterdvARBPROC) extgl_GetProcAddress("glGetProgramLocalParameterdvARB");
    glGetProgramLocalParameterfvARB = (glGetProgramLocalParameterfvARBPROC) extgl_GetProcAddress("glGetProgramLocalParameterfvARB");
#endif /* GL_ARB_vertex_program */
#endif /* GL_NV_fragment_program */
}

void extgl_InitNVElementArray()
{
#ifdef GL_NV_element_array
    if (!extgl_Extensions.NV_element_array)
        return;
    glElementPointerNV = (glElementPointerNVPROC) extgl_GetProcAddress("glElementPointerNV");
    glDrawElementArrayNV = (glDrawElementArrayNVPROC) extgl_GetProcAddress("glDrawElementArrayNV");
    glDrawRangeElementArrayNV = (glDrawRangeElementArrayNVPROC) extgl_GetProcAddress("glDrawRangeElementArrayNV");
    glMultiDrawElementArrayNV = (glMultiDrawElementArrayNVPROC) extgl_GetProcAddress("glMultiDrawElementArrayNV");
    glMultiDrawRangeElementArrayNV = (glMultiDrawRangeElementArrayNVPROC) extgl_GetProcAddress("glMultiDrawRangeElementArrayNV");
#endif
}


void extgl_InitEXTBlendFuncSeparate()
{
#ifdef GL_EXT_blend_func_separate
    if (!extgl_Extensions.EXT_blend_func_separate)
        return;
    glBlendFuncSeparateEXT = (glBlendFuncSeparateEXTPROC) extgl_GetProcAddress("glBlendFuncSeparateEXT");
#endif
}

void extgl_InitEXTCullVertex()
{
#ifdef GL_EXT_cull_vertex
    if (!extgl_Extensions.EXT_cull_vertex)
        return;
    glCullParameterfvEXT = (glCullParameterfvEXTPROC) extgl_GetProcAddress("glCullParameterfvEXT");
    glCullParameterdvEXT = (glCullParameterdvEXTPROC) extgl_GetProcAddress("glCullParameterdvEXT");
#endif
}

void extgl_InitARBVertexProgram()
{
#ifdef GL_ARB_vertex_program
    if (!extgl_Extensions.ARB_vertex_program)
        return;
    glVertexAttrib1sARB = (glVertexAttrib1sARBPROC) extgl_GetProcAddress("glVertexAttrib1sARB");
    glVertexAttrib1fARB = (glVertexAttrib1fARBPROC) extgl_GetProcAddress("glVertexAttrib1fARB");
    glVertexAttrib1dARB = (glVertexAttrib1dARBPROC) extgl_GetProcAddress("glVertexAttrib1dARB");
    glVertexAttrib2sARB = (glVertexAttrib2sARBPROC) extgl_GetProcAddress("glVertexAttrib2sARB");
    glVertexAttrib2fARB = (glVertexAttrib2fARBPROC) extgl_GetProcAddress("glVertexAttrib2fARB");
    glVertexAttrib2dARB = (glVertexAttrib2dARBPROC) extgl_GetProcAddress("glVertexAttrib2dARB");
    glVertexAttrib3sARB = (glVertexAttrib3sARBPROC) extgl_GetProcAddress("glVertexAttrib3sARB");
    glVertexAttrib3fARB = (glVertexAttrib3fARBPROC) extgl_GetProcAddress("glVertexAttrib3fARB");
    glVertexAttrib3dARB = (glVertexAttrib3dARBPROC) extgl_GetProcAddress("glVertexAttrib3dARB");
    glVertexAttrib4sARB = (glVertexAttrib4sARBPROC) extgl_GetProcAddress("glVertexAttrib4sARB");
    glVertexAttrib4fARB = (glVertexAttrib4fARBPROC) extgl_GetProcAddress("glVertexAttrib4fARB");
    glVertexAttrib4dARB = (glVertexAttrib4dARBPROC) extgl_GetProcAddress("glVertexAttrib4dARB");
    glVertexAttrib4NubARB = (glVertexAttrib4NubARBPROC) extgl_GetProcAddress("glVertexAttrib4NubARB");
    glVertexAttrib1svARB = (glVertexAttrib1svARBPROC) extgl_GetProcAddress("glVertexAttrib1svARB");
    glVertexAttrib1fvARB = (glVertexAttrib1fvARBPROC) extgl_GetProcAddress("glVertexAttrib1fvARB");
    glVertexAttrib1dvARB = (glVertexAttrib1dvARBPROC) extgl_GetProcAddress("glVertexAttrib1dvARB");
    glVertexAttrib2svARB = (glVertexAttrib2svARBPROC) extgl_GetProcAddress("glVertexAttrib2svARB");
    glVertexAttrib2fvARB = (glVertexAttrib2fvARBPROC) extgl_GetProcAddress("glVertexAttrib2fvARB");
    glVertexAttrib2dvARB = (glVertexAttrib2dvARBPROC) extgl_GetProcAddress("glVertexAttrib2dvARB");
    glVertexAttrib3svARB = (glVertexAttrib3svARBPROC) extgl_GetProcAddress("glVertexAttrib3svARB");
    glVertexAttrib3fvARB = (glVertexAttrib3fvARBPROC) extgl_GetProcAddress("glVertexAttrib3fvARB");
    glVertexAttrib3dvARB = (glVertexAttrib3dvARBPROC) extgl_GetProcAddress("glVertexAttrib3dvARB");
    glVertexAttrib4bvARB = (glVertexAttrib4bvARBPROC) extgl_GetProcAddress("glVertexAttrib4bvARB");
    glVertexAttrib4svARB = (glVertexAttrib4svARBPROC) extgl_GetProcAddress("glVertexAttrib4svARB");
    glVertexAttrib4ivARB = (glVertexAttrib4ivARBPROC) extgl_GetProcAddress("glVertexAttrib4ivARB");
    glVertexAttrib4ubvARB = (glVertexAttrib4ubvARBPROC) extgl_GetProcAddress("glVertexAttrib4ubvARB");
    glVertexAttrib4usvARB = (glVertexAttrib4usvARBPROC) extgl_GetProcAddress("glVertexAttrib4usvARB");
    glVertexAttrib4uivARB = (glVertexAttrib4uivARBPROC) extgl_GetProcAddress("glVertexAttrib4uivARB");
    glVertexAttrib4fvARB = (glVertexAttrib4fvARBPROC) extgl_GetProcAddress("glVertexAttrib4fvARB");
    glVertexAttrib4dvARB = (glVertexAttrib4dvARBPROC) extgl_GetProcAddress("glVertexAttrib4dvARB");
    glVertexAttrib4NbvARB = (glVertexAttrib4NbvARBPROC) extgl_GetProcAddress("glVertexAttrib4NbvARB");
    glVertexAttrib4NsvARB = (glVertexAttrib4NsvARBPROC) extgl_GetProcAddress("glVertexAttrib4NsvARB");
    glVertexAttrib4NivARB = (glVertexAttrib4NivARBPROC) extgl_GetProcAddress("glVertexAttrib4NivARB");
    glVertexAttrib4NubvARB = (glVertexAttrib4NubvARBPROC) extgl_GetProcAddress("glVertexAttrib4NubvARB");
    glVertexAttrib4NusvARB = (glVertexAttrib4NusvARBPROC) extgl_GetProcAddress("glVertexAttrib4NusvARB");
    glVertexAttrib4NuivARB = (glVertexAttrib4NuivARBPROC) extgl_GetProcAddress("glVertexAttrib4NuivARB");
    glVertexAttribPointerARB = (glVertexAttribPointerARBPROC) extgl_GetProcAddress("glVertexAttribPointerARB");
    glEnableVertexAttribArrayARB = (glEnableVertexAttribArrayARBPROC) extgl_GetProcAddress("glEnableVertexAttribArrayARB");
    glDisableVertexAttribArrayARB = (glDisableVertexAttribArrayARBPROC) extgl_GetProcAddress("glDisableVertexAttribArrayARB");
    glProgramStringARB = (glProgramStringARBPROC) extgl_GetProcAddress("glProgramStringARB");
    glBindProgramARB = (glBindProgramARBPROC) extgl_GetProcAddress("glBindProgramARB");
    glDeleteProgramsARB = (glDeleteProgramsARBPROC) extgl_GetProcAddress("glDeleteProgramsARB");
    glGenProgramsARB = (glGenProgramsARBPROC) extgl_GetProcAddress("glGenProgramsARB");
    glProgramEnvParameter4dARB = (glProgramEnvParameter4dARBPROC) extgl_GetProcAddress("glProgramEnvParameter4dARB");
    glProgramEnvParameter4dvARB = (glProgramEnvParameter4dvARBPROC) extgl_GetProcAddress("glProgramEnvParameter4dvARB");
    glProgramEnvParameter4fARB = (glProgramEnvParameter4fARBPROC) extgl_GetProcAddress("glProgramEnvParameter4fARB");
    glProgramEnvParameter4fvARB = (glProgramEnvParameter4fvARBPROC) extgl_GetProcAddress("glProgramEnvParameter4fvARB");
    glProgramLocalParameter4dARB = (glProgramLocalParameter4dARBPROC) extgl_GetProcAddress("glProgramLocalParameter4dARB");
    glProgramLocalParameter4dvARB = (glProgramLocalParameter4dvARBPROC) extgl_GetProcAddress("glProgramLocalParameter4dvARB");
    glProgramLocalParameter4fARB = (glProgramLocalParameter4fARBPROC) extgl_GetProcAddress("glProgramLocalParameter4fARB");
    glProgramLocalParameter4fvARB = (glProgramLocalParameter4fvARBPROC) extgl_GetProcAddress("glProgramLocalParameter4fvARB");
    glGetProgramEnvParameterdvARB = (glGetProgramEnvParameterdvARBPROC) extgl_GetProcAddress("glGetProgramEnvParameterdvARB");
    glGetProgramEnvParameterfvARB = (glGetProgramEnvParameterfvARBPROC) extgl_GetProcAddress("glGetProgramEnvParameterfvARB");
    glGetProgramLocalParameterdvARB = (glGetProgramLocalParameterdvARBPROC) extgl_GetProcAddress("glGetProgramLocalParameterdvARB");
    glGetProgramLocalParameterfvARB = (glGetProgramLocalParameterfvARBPROC) extgl_GetProcAddress("glGetProgramLocalParameterfvARB");
    glGetProgramivARB = (glGetProgramivARBPROC) extgl_GetProcAddress("glGetProgramivARB");
    glGetProgramStringARB = (glGetProgramStringARBPROC) extgl_GetProcAddress("glGetProgramStringARB");
    glGetVertexAttribdvARB = (glGetVertexAttribdvARBPROC) extgl_GetProcAddress("glGetVertexAttribdvARB");
    glGetVertexAttribfvARB = (glGetVertexAttribfvARBPROC) extgl_GetProcAddress("glGetVertexAttribfvARB");
    glGetVertexAttribivARB = (glGetVertexAttribivARBPROC) extgl_GetProcAddress("glGetVertexAttribivARB");
    glGetVertexAttribPointervARB = (glGetVertexAttribPointervARBPROC) extgl_GetProcAddress("glGetVertexAttribPointervARB");
    glIsProgramARB = (glIsProgramARBPROC) extgl_GetProcAddress("glIsProgramARB");
#endif
}

void extgl_InitEXTStencilTwoSide()
{
#ifdef GL_EXT_stencil_two_side
    if (!extgl_Extensions.EXT_stencil_two_side)
        return;
    glActiveStencilFaceEXT = (glActiveStencilFaceEXTPROC) extgl_GetProcAddress("glActiveStencilFaceEXT");
#endif
}

void extgl_InitARBWindowPos()
{
#ifdef GL_ARB_window_pos
    if (!extgl_Extensions.ARB_window_pos)
        return;
    glWindowPos2dARB = (glWindowPos2dARBPROC) extgl_GetProcAddress("glWindowPos2dARB");
    glWindowPos2fARB = (glWindowPos2fARBPROC) extgl_GetProcAddress("glWindowPos2fARB");
    glWindowPos2iARB = (glWindowPos2iARBPROC) extgl_GetProcAddress("glWindowPos2iARB");
    glWindowPos2sARB = (glWindowPos2sARBPROC) extgl_GetProcAddress("glWindowPos2sARB");
    glWindowPos2dvARB = (glWindowPos2dvARBPROC) extgl_GetProcAddress("glWindowPos2dvARB");
    glWindowPos2fvARB = (glWindowPos2fvARBPROC) extgl_GetProcAddress("glWindowPos2fvARB");
    glWindowPos2ivARB = (glWindowPos2ivARBPROC) extgl_GetProcAddress("glWindowPos2ivARB");
    glWindowPos2svARB = (glWindowPos2svARBPROC) extgl_GetProcAddress("glWindowPos2svARB");
    glWindowPos3dARB = (glWindowPos3dARBPROC) extgl_GetProcAddress("glWindowPos3dARB");
    glWindowPos3fARB = (glWindowPos3fARBPROC) extgl_GetProcAddress("glWindowPos3fARB");
    glWindowPos3iARB = (glWindowPos3iARBPROC) extgl_GetProcAddress("glWindowPos3iARB");
    glWindowPos3sARB = (glWindowPos3sARBPROC) extgl_GetProcAddress("glWindowPos3sARB");
    glWindowPos3dvARB = (glWindowPos3dvARBPROC) extgl_GetProcAddress("glWindowPos3dvARB");
    glWindowPos3fvARB = (glWindowPos3fvARBPROC) extgl_GetProcAddress("glWindowPos3fvARB");
    glWindowPos3ivARB = (glWindowPos3ivARBPROC) extgl_GetProcAddress("glWindowPos3ivARB");
    glWindowPos3svARB = (glWindowPos3svARBPROC) extgl_GetProcAddress("glWindowPos3svARB");
#endif 
}

void extgl_InitARBTextureCompression()
{
#ifdef GL_ARB_texture_compression
    if (!extgl_Extensions.ARB_texture_compression)
        return;
    glCompressedTexImage3DARB = (glCompressedTexImage3DARBPROC) extgl_GetProcAddress("glCompressedTexImage3DARB");
    glCompressedTexImage2DARB = (glCompressedTexImage2DARBPROC) extgl_GetProcAddress("glCompressedTexImage2DARB");
    glCompressedTexImage1DARB = (glCompressedTexImage1DARBPROC) extgl_GetProcAddress("glCompressedTexImage1DARB");
    glCompressedTexSubImage3DARB = (glCompressedTexSubImage3DARBPROC) extgl_GetProcAddress("glCompressedTexSubImage3DARB");
    glCompressedTexSubImage2DARB = (glCompressedTexSubImage2DARBPROC) extgl_GetProcAddress("glCompressedTexSubImage2DARB");
    glCompressedTexSubImage1DARB = (glCompressedTexSubImage1DARBPROC) extgl_GetProcAddress("glCompressedTexSubImage1DARB");
    glGetCompressedTexImageARB = (glGetCompressedTexImageARBPROC) extgl_GetProcAddress("glGetCompressedTexImageARB");
#endif
}

void extgl_InitNVPointSprite()
{
#ifdef GL_NV_point_sprite
    if (!extgl_Extensions.NV_point_sprite)
        return;
    glPointParameteriNV = (glPointParameteriNVPROC) extgl_GetProcAddress("glPointParameteriNV");
    glPointParameterivNV = (glPointParameterivNVPROC) extgl_GetProcAddress("glPointParameterivNV");
#endif
}

void extgl_InitNVOcclusionQuery()
{
#ifdef GL_NV_occlusion_query
    if (!extgl_Extensions.NV_occlusion_query)
        return;
    glGenOcclusionQueriesNV = (glGenOcclusionQueriesNVPROC) extgl_GetProcAddress("glGenOcclusionQueriesNV");
    glDeleteOcclusionQueriesNV = (glDeleteOcclusionQueriesNVPROC) extgl_GetProcAddress("glDeleteOcclusionQueriesNV");
    glIsOcclusionQueryNV = (glIsOcclusionQueryNVPROC) extgl_GetProcAddress("glIsOcclusionQueryNV");
    glBeginOcclusionQueryNV = (glBeginOcclusionQueryNVPROC) extgl_GetProcAddress("glBeginOcclusionQueryNV");
    glEndOcclusionQueryNV = (glEndOcclusionQueryNVPROC) extgl_GetProcAddress("glEndOcclusionQueryNV");
    glGetOcclusionQueryivNV = (glGetOcclusionQueryivNVPROC) extgl_GetProcAddress("glGetOcclusionQueryivNV");
    glGetOcclusionQueryuivNV = (glGetOcclusionQueryuivNVPROC) extgl_GetProcAddress("glGetOcclusionQueryuivNV");
#endif
}

void extgl_InitATIVertexArrayObject()
{
#ifdef GL_ATI_vertex_array_object
    if (!extgl_Extensions.ATI_vertex_array_object)
        return;
    glNewObjectBufferATI = (glNewObjectBufferATIPROC) extgl_GetProcAddress("glNewObjectBufferATI");
    glIsObjectBufferATI = (glIsObjectBufferATIPROC) extgl_GetProcAddress("glIsObjectBufferATI");
    glUpdateObjectBufferATI = (glUpdateObjectBufferATIPROC) extgl_GetProcAddress("glUpdateObjectBufferATI");
    glGetObjectBufferfvATI = (glGetObjectBufferfvATIPROC) extgl_GetProcAddress("glGetObjectBufferfvATI");
    glGetObjectBufferivATI = (glGetObjectBufferivATIPROC) extgl_GetProcAddress("glGetObjectBufferivATI");
    glFreeObjectBufferATI = (glFreeObjectBufferATIPROC) extgl_GetProcAddress("glFreeObjectBufferATI");
    glArrayObjectATI = (glArrayObjectATIPROC) extgl_GetProcAddress("glArrayObjectATI");
    glGetArrayObjectfvATI = (glGetArrayObjectfvATIPROC) extgl_GetProcAddress("glGetArrayObjectfvATI");
    glGetArrayObjectivATI = (glGetArrayObjectivATIPROC) extgl_GetProcAddress("glGetArrayObjectivATI");
    glVariantArrayObjectATI = (glVariantArrayObjectATIPROC) extgl_GetProcAddress("glVariantArrayObjectATI");
    glGetVariantArrayObjectfvATI = (glGetVariantArrayObjectfvATIPROC) extgl_GetProcAddress("glGetVariantArrayObjectfvATI");
    glGetVariantArrayObjectivATI = (glGetVariantArrayObjectivATIPROC) extgl_GetProcAddress("glGetVariantArrayObjectivATI");
#endif
}

void extgl_InitATIVertexStreams()
{
#ifdef GL_ATI_vertex_streams
    if (!extgl_Extensions.ATI_vertex_streams)
        return;
    glClientActiveVertexStreamATI = (glClientActiveVertexStreamATIPROC) extgl_GetProcAddress("glClientActiveVertexStreamATI");
    glVertexBlendEnviATI = (glVertexBlendEnviATIPROC) extgl_GetProcAddress("glVertexBlendEnviATI");
    glVertexBlendEnvfATI = (glVertexBlendEnvfATIPROC) extgl_GetProcAddress("glVertexBlendEnvfATI");
    glVertexStream2sATI = (glVertexStream2sATIPROC) extgl_GetProcAddress("glVertexStream2sATI");
    glVertexStream2svATI = (glVertexStream2svATIPROC) extgl_GetProcAddress("glVertexStream2svATI");
    glVertexStream2iATI = (glVertexStream2iATIPROC) extgl_GetProcAddress("glVertexStream2iATI");
    glVertexStream2ivATI = (glVertexStream2ivATIPROC) extgl_GetProcAddress("glVertexStream2ivATI");
    glVertexStream2fATI = (glVertexStream2fATIPROC) extgl_GetProcAddress("glVertexStream2fATI");
    glVertexStream2fvATI = (glVertexStream2fvATIPROC) extgl_GetProcAddress("glVertexStream2fvATI");
    glVertexStream2dATI = (glVertexStream2dATIPROC) extgl_GetProcAddress("glVertexStream2dATI");
    glVertexStream2dvATI = (glVertexStream2dvATIPROC) extgl_GetProcAddress("glVertexStream2dvATI");
    glVertexStream3sATI = (glVertexStream3sATIPROC) extgl_GetProcAddress("glVertexStream3sATI");
    glVertexStream3svATI = (glVertexStream3svATIPROC) extgl_GetProcAddress("glVertexStream3svATI");
    glVertexStream3iATI = (glVertexStream3iATIPROC) extgl_GetProcAddress("glVertexStream3iATI");
    glVertexStream3ivATI = (glVertexStream3ivATIPROC) extgl_GetProcAddress("glVertexStream3ivATI");
    glVertexStream3fATI = (glVertexStream3fATIPROC) extgl_GetProcAddress("glVertexStream3fATI");
    glVertexStream3fvATI = (glVertexStream3fvATIPROC) extgl_GetProcAddress("glVertexStream3fvATI");
    glVertexStream3dATI = (glVertexStream3dATIPROC) extgl_GetProcAddress("glVertexStream3dATI");
    glVertexStream3dvATI = (glVertexStream3dvATIPROC) extgl_GetProcAddress("glVertexStream3dvATI");
    glVertexStream4sATI = (glVertexStream4sATIPROC) extgl_GetProcAddress("glVertexStream4sATI");
    glVertexStream4svATI = (glVertexStream4svATIPROC) extgl_GetProcAddress("glVertexStream4svATI");
    glVertexStream4iATI = (glVertexStream4iATIPROC) extgl_GetProcAddress("glVertexStream4iATI");
    glVertexStream4ivATI = (glVertexStream4ivATIPROC) extgl_GetProcAddress("glVertexStream4ivATI");
    glVertexStream4fATI = (glVertexStream4fATIPROC) extgl_GetProcAddress("glVertexStream4fATI");
    glVertexStream4fvATI = (glVertexStream4fvATIPROC) extgl_GetProcAddress("glVertexStream4fvATI");
    glVertexStream4dATI = (glVertexStream4dATIPROC) extgl_GetProcAddress("glVertexStream4dATI");
    glVertexStream4dvATI = (glVertexStream4dvATIPROC) extgl_GetProcAddress("glVertexStream4dvATI");
    glNormalStream3bATI = (glNormalStream3bATIPROC) extgl_GetProcAddress("glNormalStream3bATI");
    glNormalStream3bvATI = (glNormalStream3bvATIPROC) extgl_GetProcAddress("glNormalStream3bvATI");
    glNormalStream3sATI = (glNormalStream3sATIPROC) extgl_GetProcAddress("glNormalStream3sATI");
    glNormalStream3svATI = (glNormalStream3svATIPROC) extgl_GetProcAddress("glNormalStream3svATI");
    glNormalStream3iATI = (glNormalStream3iATIPROC) extgl_GetProcAddress("glNormalStream3iATI");
    glNormalStream3ivATI = (glNormalStream3ivATIPROC) extgl_GetProcAddress("glNormalStream3ivATI");
    glNormalStream3fATI = (glNormalStream3fATIPROC) extgl_GetProcAddress("glNormalStream3fATI");
    glNormalStream3fvATI = (glNormalStream3fvATIPROC) extgl_GetProcAddress("glNormalStream3fvATI");
    glNormalStream3dATI = (glNormalStream3dATIPROC) extgl_GetProcAddress("glNormalStream3dATI");
    glNormalStream3dvATI = (glNormalStream3dvATIPROC) extgl_GetProcAddress("glNormalStream3dvATI");
#endif
}

void extgl_InitATIElementArray()
{
#ifdef GL_ATI_element_array
    if (!extgl_Extensions.ATI_element_array)
        return;
    glElementPointerATI = (glElementPointerATIPROC) extgl_GetProcAddress("glElementPointerATI");
    glDrawElementArrayATI = (glDrawElementArrayATIPROC) extgl_GetProcAddress("glDrawElementArrayATI");
    glDrawRangeElementArrayATI = (glDrawRangeElementArrayATIPROC) extgl_GetProcAddress("glDrawRangeElementArrayATI");
#endif
}

void extgl_InitATIFragmentShader()
{
#ifdef GL_ATI_fragment_shader
    if (!extgl_Extensions.ATI_fragment_shader)
        return;
    glGenFragmentShadersATI = (glGenFragmentShadersATIPROC) extgl_GetProcAddress("glGenFragmentShadersATI");
    glBindFragmentShaderATI = (glBindFragmentShaderATIPROC) extgl_GetProcAddress("glBindFragmentShaderATI");
    glDeleteFragmentShaderATI = (glDeleteFragmentShaderATIPROC) extgl_GetProcAddress("glDeleteFragmentShaderATI");
    glBeginFragmentShaderATI = (glBeginFragmentShaderATIPROC) extgl_GetProcAddress("glBeginFragmentShaderATI");
    glEndFragmentShaderATI = (glEndFragmentShaderATIPROC) extgl_GetProcAddress("glEndFragmentShaderATI");
    glPassTexCoordATI = (glPassTexCoordATIPROC) extgl_GetProcAddress("glPassTexCoordATI");
    glSampleMapATI = (glSampleMapATIPROC) extgl_GetProcAddress("glSampleMapATI");
    glColorFragmentOp1ATI = (glColorFragmentOp1ATIPROC) extgl_GetProcAddress("glColorFragmentOp1ATI");
    glColorFragmentOp2ATI = (glColorFragmentOp2ATIPROC) extgl_GetProcAddress("glColorFragmentOp2ATI");
    glColorFragmentOp3ATI = (glColorFragmentOp3ATIPROC) extgl_GetProcAddress("glColorFragmentOp3ATI");
    glAlphaFragmentOp1ATI = (glAlphaFragmentOp1ATIPROC) extgl_GetProcAddress("glAlphaFragmentOp1ATI");
    glAlphaFragmentOp2ATI = (glAlphaFragmentOp2ATIPROC) extgl_GetProcAddress("glAlphaFragmentOp2ATI");
    glAlphaFragmentOp3ATI = (glAlphaFragmentOp3ATIPROC) extgl_GetProcAddress("glAlphaFragmentOp3ATI");
    glSetFragmentShaderConstantATI = (glSetFragmentShaderConstantATIPROC) extgl_GetProcAddress("glSetFragmentShaderConstantATI");
#endif
}


void extgl_InitATIEnvmapBumpmap()
{
#ifdef GL_ATI_envmap_bumpmap
    if (!extgl_Extensions.ATI_envmap_bumpmap)
        return;
    glTexBumpParameterivATI = (glTexBumpParameterivATIPROC) extgl_GetProcAddress("glTexBumpParameterivATI");
    glTexBumpParameterfvATI = (glTexBumpParameterfvATIPROC) extgl_GetProcAddress("glTexBumpParameterfvATI");
    glGetTexBumpParameterivATI = (glGetTexBumpParameterivATIPROC) extgl_GetProcAddress("glGetTexBumpParameterivATI");
    glGetTexBumpParameterfvATI = (glGetTexBumpParameterfvATIPROC) extgl_GetProcAddress("glGetTexBumpParameterfvATI");
#endif
}

void extgl_InitEXTVertexShader()
{
#ifdef GL_EXT_vertex_shader
    if (!extgl_Extensions.EXT_vertex_shader)
        return;
    glBeginVertexShaderEXT = (glBeginVertexShaderEXTPROC) extgl_GetProcAddress("glBeginVertexShaderEXT");
    glEndVertexShaderEXT = (glEndVertexShaderEXTPROC) extgl_GetProcAddress("glEndVertexShaderEXT");
    glBindVertexShaderEXT = (glBindVertexShaderEXTPROC) extgl_GetProcAddress("glBindVertexShaderEXT");
    glGenVertexShadersEXT = (glGenVertexShadersEXTPROC) extgl_GetProcAddress("glGenVertexShadersEXT");
    glDeleteVertexShaderEXT = (glDeleteVertexShaderEXTPROC) extgl_GetProcAddress("glDeleteVertexShaderEXT");
    glShaderOp1EXT = (glShaderOp1EXTPROC) extgl_GetProcAddress("glShaderOp1EXT");
    glShaderOp2EXT = (glShaderOp2EXTPROC) extgl_GetProcAddress("glShaderOp2EXT");
    glShaderOp3EXT = (glShaderOp3EXTPROC) extgl_GetProcAddress("glShaderOp3EXT");
    glSwizzleEXT = (glSwizzleEXTPROC) extgl_GetProcAddress("glSwizzleEXT");
    glWriteMaskEXT = (glWriteMaskEXTPROC) extgl_GetProcAddress("glWriteMaskEXT");
    glInsertComponentEXT = (glInsertComponentEXTPROC) extgl_GetProcAddress("glInsertComponentEXT");
    glExtractComponentEXT = (glExtractComponentEXTPROC) extgl_GetProcAddress("glExtractComponentEXT");
    glGenSymbolsEXT = (glGenSymbolsEXTPROC) extgl_GetProcAddress("glGenSymbolsEXT");
    glSetInvariantEXT = (glSetInvariantEXTPROC) extgl_GetProcAddress("glSetInvarianceEXT");
    glSetLocalConstantEXT = (glSetLocalConstantEXTPROC) extgl_GetProcAddress("glSetLocalConstantEXT");
    glVariantbvEXT = (glVariantbvEXTPROC) extgl_GetProcAddress("glVariantbvEXT");
    glVariantsvEXT = (glVariantsvEXTPROC) extgl_GetProcAddress("glVariantsvEXT");
    glVariantivEXT = (glVariantivEXTPROC) extgl_GetProcAddress("glVariantivEXT");
    glVariantfvEXT = (glVariantfvEXTPROC) extgl_GetProcAddress("glVariantfvEXT");
    glVariantdvEXT = (glVariantdvEXTPROC) extgl_GetProcAddress("glVariantdvEXT");
    glVariantubvEXT = (glVariantubvEXTPROC) extgl_GetProcAddress("glVariantubvEXT");
    glVariantusvEXT = (glVariantusvEXTPROC) extgl_GetProcAddress("glVariantusvEXT");
    glVariantuivEXT = (glVariantuivEXTPROC) extgl_GetProcAddress("glVariantuivEXT");
    glVariantPointerEXT = (glVariantPointerEXTPROC) extgl_GetProcAddress("glVariantPointerEXT");
    glEnableVariantClientStateEXT = (glEnableVariantClientStateEXTPROC) extgl_GetProcAddress("glEnableVariantClientStateEXT");
    glDisableVariantClientStateEXT = (glDisableVariantClientStateEXTPROC) extgl_GetProcAddress("glDisableVariantClientStateEXT");
    glBindLightParameterEXT = (glBindLightParameterEXTPROC) extgl_GetProcAddress("glBindLightParameterEXT");
    glBindMaterialParameterEXT = (glBindMaterialParameterEXTPROC) extgl_GetProcAddress("glBindMaterialParameterEXT");
    glBindTexGenParameterEXT = (glBindTexGenParameterEXTPROC) extgl_GetProcAddress("glBindTexGenParameterEXT");
    glBindTextureUnitParameterEXT = (glBindTextureUnitParameterEXTPROC) extgl_GetProcAddress("glBindTextureUnitParameterEXT");
    glBindParameterEXT = (glBindParameterEXTPROC) extgl_GetProcAddress("glBindParameterEXT");
    glIsVariantEnabledEXT = (glIsVariantEnabledEXTPROC) extgl_GetProcAddress("glIsVariantEnabledEXT");
    glGetVariantBooleanvEXT = (glGetVariantBooleanvEXTPROC) extgl_GetProcAddress("glGetVariantBooleanvEXT");
    glGetVariantIntegervEXT = (glGetVariantIntegervEXTPROC) extgl_GetProcAddress("glGetVariantIntegervEXT");
    glGetVariantFloatvEXT = (glGetVariantFloatvEXTPROC) extgl_GetProcAddress("glGetVariantFloatvEXT");
    glGetVariantPointervEXT = (glGetVariantPointervEXTPROC) extgl_GetProcAddress("glGetVariantPointervEXT");
    glGetInvariantBooleanvEXT = (glGetInvariantBooleanvEXTPROC) extgl_GetProcAddress("glGetInvariantBooleanvEXT");
    glGetInvariantIntegervEXT = (glGetInvariantIntegervEXTPROC) extgl_GetProcAddress("glGetInvariantIntegervEXT");
    glGetInvariantFloatvEXT = (glGetInvariantFloatvEXTPROC) extgl_GetProcAddress("glGetInvariantFloatvEXT");
    glGetLocalConstantBooleanvEXT = (glGetLocalConstantBooleanvEXTPROC) extgl_GetProcAddress("glGetLocalConstantBooleanvEXT");
    glGetLocalConstantIntegervEXT = (glGetLocalConstantIntegervEXTPROC) extgl_GetProcAddress("glGetLocalConstantIntegervEXT");
    glGetLocalConstantFloatvEXT = (glGetLocalConstantFloatvEXTPROC) extgl_GetProcAddress("glGetLocalConstantFloatvEXT");
#endif
}

void extgl_InitARBMatrixPalette()
{
#ifdef GL_ARB_matrix_palette
    if (!extgl_Extensions.ARB_matrix_palette)
        return;
    glCurrentPaletteMatrixARB = (glCurrentPaletteMatrixARBPROC) extgl_GetProcAddress("glCurrentPaletteMatrixARB");
    glMatrixIndexubvARB = (glMatrixIndexubvARBPROC) extgl_GetProcAddress("glMatrixIndexubvARB");
    glMatrixIndexusvARB = (glMatrixIndexusvARBPROC) extgl_GetProcAddress("glMatrixIndexusvARB");
    glMatrixIndexuivARB = (glMatrixIndexuivARBPROC) extgl_GetProcAddress("glMatrixIndexuivARB");
    glMatrixIndexPointerARB = (glMatrixIndexPointerARBPROC) extgl_GetProcAddress("glMatrixIndexPointerARB");
#endif
}

void extgl_InitEXTMultiDrawArrays()
{
#ifdef GL_EXT_multi_draw_arrays
    if (!extgl_Extensions.EXT_multi_draw_arrays)
        return;
    glMultiDrawArraysEXT = (glMultiDrawArraysEXTPROC) extgl_GetProcAddress("glMultiDrawArraysEXT");
    glMultiDrawElementsEXT = (glMultiDrawElementsEXTPROC) extgl_GetProcAddress("glMultiDrawElementsEXT");
#endif
}

void extgl_InitARBVertexBlend()
{
#ifdef GL_ARB_vertex_blend
    if (!extgl_Extensions.ARB_vertex_blend)
        return;
    glWeightbvARB = (glWeightbvARBPROC) extgl_GetProcAddress("glWeightbvARB");
    glWeightsvARB = (glWeightsvARBPROC) extgl_GetProcAddress("glWeightsvARB");
    glWeightivARB = (glWeightivARBPROC) extgl_GetProcAddress("glWeightivARB");
    glWeightfvARB = (glWeightfvARBPROC) extgl_GetProcAddress("glWeightfvARB");
    glWeightdvARB = (glWeightdvARBPROC) extgl_GetProcAddress("glWeightdvARB");
    glWeightubvARB = (glWeightubvARBPROC) extgl_GetProcAddress("glWeightubvARB");
    glWeightusvARB = (glWeightusvARBPROC) extgl_GetProcAddress("glWeightusvARB");
    glWeightuivARB = (glWeightuivARBPROC) extgl_GetProcAddress("glWeightuivARB");
    glWeightPointerARB = (glWeightPointerARBPROC) extgl_GetProcAddress("glWeightPointerARB");
    glVertexBlendARB = (glVertexBlendARBPROC) extgl_GetProcAddress("glVertexBlendARB");
#endif
}

void extgl_InitARBPointParameters()
{
#ifdef GL_ARB_point_parameters
    if (!extgl_Extensions.ARB_point_parameters)
        return;
    glPointParameterfARB = (glPointParameterfARBPROC) extgl_GetProcAddress("glPointParameterfARB");
    glPointParameterfvARB = (glPointParameterfvARBPROC) extgl_GetProcAddress("glPointParameterfvARB");
#endif
}

void extgl_InitATIPNTriangles()
{
#ifdef GL_ATI_pn_triangles
    if (!extgl_Extensions.ATI_pn_triangles)
        return;
    glPNTrianglesiATI = (glPNTrianglesiATIPROC) extgl_GetProcAddress("glPNTrianglesiATI");
    glPNTrianglesfATI = (glPNTrianglesfATIPROC) extgl_GetProcAddress("glPNTrianglesfATI");
#endif
}

void extgl_InitNVEvaluators()
{
#ifdef GL_NV_evaluators
    if (!extgl_Extensions.NV_evaluators)
        return;
    glMapControlPointsNV = (glMapControlPointsNVPROC) extgl_GetProcAddress("glMapControlPointsNV");
    glMapParameterivNV = (glMapParameterivNVPROC) extgl_GetProcAddress("glMapParameterivNV");
    glMapParameterfvNV = (glMapParameterfvNVPROC) extgl_GetProcAddress("glMapParameterfvNV");
    glGetMapControlPointsNV = (glGetMapControlPointsNVPROC) extgl_GetProcAddress("glGetMapControlPointsNV");
    glGetMapParameterivNV = (glGetMapParameterivNVPROC) extgl_GetProcAddress("glGetMapParameterivNV");
    glGetMapParameterfvNV = (glGetMapParameterfvNVPROC) extgl_GetProcAddress("glGetMapParameterfvNV");
    glGetMapAttribParameterivNV = (glGetMapAttribParameterivNVPROC) extgl_GetProcAddress("glGetMapAttribParameterivNV");
    glGetMapAttribParameterfvNV = (glGetMapAttribParameterfvNVPROC) extgl_GetProcAddress("glGetMapAttribParameterfvNV");
    glEvalMapsNV = (glEvalMapsNVPROC) extgl_GetProcAddress("glEvalMapsNV");
#endif
}

void extgl_InitNVRegisterCombiners2()
{
#ifdef GL_NV_register_combiners
    if (!extgl_Extensions.NV_register_combiners2)
        return;
    glCombinerStageParameterfvNV = (glCombinerStageParameterfvNVPROC) extgl_GetProcAddress("glCombinerStageParameterfvNV");
    glGetCombinerStageParameterfvNV = (glGetCombinerStageParameterfvNVPROC) extgl_GetProcAddress("glGetCombinerStageParameterfvNV");
#endif
}

void extgl_InitNVFence()
{
#ifdef GL_NV_fence
    if (!extgl_Extensions.NV_fence)
        return;
    glGenFencesNV = (glGenFencesNVPROC) extgl_GetProcAddress("glGenFencesNV");
    glDeleteFencesNV = (glDeleteFencesNVPROC) extgl_GetProcAddress("glDeleteFencesNV");
    glSetFenceNV = (glSetFenceNVPROC) extgl_GetProcAddress("glSetFenceNV");
    glTestFenceNV = (glTestFenceNVPROC) extgl_GetProcAddress("glTestFenceNV");
    glFinishFenceNV = (glFinishFenceNVPROC) extgl_GetProcAddress("glFinishFenceNV");
    glIsFenceNV = (glIsFenceNVPROC) extgl_GetProcAddress("glIsFenceNV");
    glGetFenceivNV = (glGetFenceivNVPROC) extgl_GetProcAddress("glGetFenceivNV");
#endif
}

void extgl_InitNVVertexProgram()
{
#ifdef GL_NV_vertex_program
    if (!extgl_Extensions.NV_vertex_program)
        return;
    glBindProgramNV = (glBindProgramNVPROC) extgl_GetProcAddress("glBindProgramNV");
    glDeleteProgramsNV = (glDeleteProgramsNVPROC) extgl_GetProcAddress("glDeleteProgramsNV");
    glExecuteProgramNV = (glExecuteProgramNVPROC) extgl_GetProcAddress("glExecuteProgramNV");
    glGenProgramsNV = (glGenProgramsNVPROC) extgl_GetProcAddress("glGenProgramsNV");
    glAreProgramsResidentNV = (glAreProgramsResidentNVPROC) extgl_GetProcAddress("glAreProgramsResidentNV");
    glRequestResidentProgramsNV = (glRequestResidentProgramsNVPROC) extgl_GetProcAddress("glRequestResidentProgramsNV");
    glGetProgramParameterfvNV = (glGetProgramParameterfvNVPROC) extgl_GetProcAddress("glGetProgramParameterfvNV");
    glGetProgramParameterdvNV = (glGetProgramParameterdvNVPROC) extgl_GetProcAddress("glGetProgramParameterdvNV");
    glGetProgramivNV = (glGetProgramivNVPROC) extgl_GetProcAddress("glGetProgramivNV");
    glGetProgramStringNV = (glGetProgramStringNVPROC) extgl_GetProcAddress("glGetProgramStringNV");
    glGetTrackMatrixivNV = (glGetTrackMatrixivNVPROC) extgl_GetProcAddress("glGetTrackMatrixivNV");
    glGetVertexAttribdvNV = (glGetVertexAttribdvNVPROC) extgl_GetProcAddress("glGetVertexAttribdvNV");
    glGetVertexAttribfvNV = (glGetVertexAttribfvNVPROC) extgl_GetProcAddress("glGetVertexAttribfvNV");
    glGetVertexAttribivNV = (glGetVertexAttribivNVPROC) extgl_GetProcAddress("glGetVertexAttribivNV");
    glGetVertexAttribPointervNV = (glGetVertexAttribPointervNVPROC) extgl_GetProcAddress("glGetVertexAttribPointervNV");
    glIsProgramNV = (glIsProgramNVPROC) extgl_GetProcAddress("glIsProgramNV");
    glLoadProgramNV = (glLoadProgramNVPROC) extgl_GetProcAddress("glLoadProgramNV");
    glProgramParameter4fNV = (glProgramParameter4fNVPROC) extgl_GetProcAddress("glProgramParameter4fNV");
    glProgramParameter4dNV = (glProgramParameter4dNVPROC) extgl_GetProcAddress("glProgramParameter4dNV");
    glProgramParameter4dvNV = (glProgramParameter4dvNVPROC) extgl_GetProcAddress("glProgramParameter4dvNV");
    glProgramParameter4fvNV = (glProgramParameter4fvNVPROC) extgl_GetProcAddress("glProgramParameter4fvNV");
    glProgramParameters4dvNV = (glProgramParameters4dvNVPROC) extgl_GetProcAddress("glProgramParameters4dvNV");
    glProgramParameters4fvNV = (glProgramParameters4fvNVPROC) extgl_GetProcAddress("glProgramParameters4fvNV");
    glTrackMatrixNV = (glTrackMatrixNVPROC) extgl_GetProcAddress("glTrackMatrixNV");
    glVertexAttribPointerNV = (glVertexAttribPointerNVPROC) extgl_GetProcAddress("glVertexAttribPointerNV");
    glVertexAttrib1sNV = (glVertexAttrib1sNVPROC) extgl_GetProcAddress("glVertexAttrib1sNV");
    glVertexAttrib1fNV = (glVertexAttrib1fNVPROC) extgl_GetProcAddress("glVertexAttrib1fNV");
    glVertexAttrib1dNV = (glVertexAttrib1dNVPROC) extgl_GetProcAddress("glVertexAttrib1dNV");
    glVertexAttrib2sNV = (glVertexAttrib2sNVPROC) extgl_GetProcAddress("glVertexAttrib2sNV");
    glVertexAttrib2fNV = (glVertexAttrib2fNVPROC) extgl_GetProcAddress("glVertexAttrib2fNV");
    glVertexAttrib2dNV = (glVertexAttrib2dNVPROC) extgl_GetProcAddress("glVertexAttrib2dNV");
    glVertexAttrib3sNV = (glVertexAttrib3sNVPROC) extgl_GetProcAddress("glVertexAttrib3sNV");
    glVertexAttrib3fNV = (glVertexAttrib3fNVPROC) extgl_GetProcAddress("glVertexAttrib3fNV");
    glVertexAttrib3dNV = (glVertexAttrib3dNVPROC) extgl_GetProcAddress("glVertexAttrib3dNV");
    glVertexAttrib4sNV = (glVertexAttrib4sNVPROC) extgl_GetProcAddress("glVertexAttrib4sNV");
    glVertexAttrib4fNV = (glVertexAttrib4fNVPROC) extgl_GetProcAddress("glVertexAttrib4fNV");
    glVertexAttrib4dNV = (glVertexAttrib4dNVPROC) extgl_GetProcAddress("glVertexAttrib4dNV");
    glVertexAttrib4ubNV = (glVertexAttrib4ubNVPROC) extgl_GetProcAddress("glVertexAttrib4ubNV");
    glVertexAttrib1svNV = (glVertexAttrib1svNVPROC) extgl_GetProcAddress("glVertexAttrib1svNV");
    glVertexAttrib1fvNV = (glVertexAttrib1fvNVPROC) extgl_GetProcAddress("glVertexAttrib1fvNV");
    glVertexAttrib1dvNV = (glVertexAttrib1dvNVPROC) extgl_GetProcAddress("glVertexAttrib1dvNV");
    glVertexAttrib2svNV = (glVertexAttrib2svNVPROC) extgl_GetProcAddress("glVertexAttrib2svNV");
    glVertexAttrib2fvNV = (glVertexAttrib2fvNVPROC) extgl_GetProcAddress("glVertexAttrib2fvNV");
    glVertexAttrib2dvNV = (glVertexAttrib2dvNVPROC) extgl_GetProcAddress("glVertexAttrib2dvNV");
    glVertexAttrib3svNV = (glVertexAttrib3svNVPROC) extgl_GetProcAddress("glVertexAttrib3svNV");
    glVertexAttrib3fvNV = (glVertexAttrib3fvNVPROC) extgl_GetProcAddress("glVertexAttrib3fvNV");
    glVertexAttrib3dvNV = (glVertexAttrib3dvNVPROC) extgl_GetProcAddress("glVertexAttrib3dvNV");
    glVertexAttrib4svNV = (glVertexAttrib4svNVPROC) extgl_GetProcAddress("glVertexAttrib4svNV");
    glVertexAttrib4fvNV = (glVertexAttrib4fvNVPROC) extgl_GetProcAddress("glVertexAttrib4fvNV");
    glVertexAttrib4dvNV = (glVertexAttrib4dvNVPROC) extgl_GetProcAddress("glVertexAttrib4dvNV");
    glVertexAttrib4ubvNV = (glVertexAttrib4ubvNVPROC) extgl_GetProcAddress("glVertexAttrib4ubvNV");
    glVertexAttribs1svNV = (glVertexAttribs1svNVPROC) extgl_GetProcAddress("glVertexAttribs1svNV");
    glVertexAttribs1fvNV = (glVertexAttribs1fvNVPROC) extgl_GetProcAddress("glVertexAttribs1fvNV");
    glVertexAttribs1dvNV = (glVertexAttribs1dvNVPROC) extgl_GetProcAddress("glVertexAttribs1dvNV");
    glVertexAttribs2svNV = (glVertexAttribs2svNVPROC) extgl_GetProcAddress("glVertexAttribs2svNV");
    glVertexAttribs2fvNV = (glVertexAttribs2fvNVPROC) extgl_GetProcAddress("glVertexAttribs2fvNV");
    glVertexAttribs2dvNV = (glVertexAttribs2dvNVPROC) extgl_GetProcAddress("glVertexAttribs2dvNV");
    glVertexAttribs3svNV = (glVertexAttribs3svNVPROC) extgl_GetProcAddress("glVertexAttribs3svNV");
    glVertexAttribs3fvNV = (glVertexAttribs3fvNVPROC) extgl_GetProcAddress("glVertexAttribs3fvNV");
    glVertexAttribs3dvNV = (glVertexAttribs3dvNVPROC) extgl_GetProcAddress("glVertexAttribs3dvNV");
    glVertexAttribs4svNV = (glVertexAttribs4svNVPROC) extgl_GetProcAddress("glVertexAttribs4svNV");
    glVertexAttribs4fvNV = (glVertexAttribs4fvNVPROC) extgl_GetProcAddress("glVertexAttribs4fvNV");
    glVertexAttribs4dvNV = (glVertexAttribs4dvNVPROC) extgl_GetProcAddress("glVertexAttribs4dvNV");
    glVertexAttribs4ubvNV = (glVertexAttribs4ubvNVPROC) extgl_GetProcAddress("glVertexAttribs4ubvNV");
#endif
}

void extgl_InitEXTVertexWeighting()
{
#ifdef GL_EXT_vertex_weighting
    if (!extgl_Extensions.EXT_vertex_weighting)
        return;
    glVertexWeightfEXT = (glVertexWeightfEXTPROC) extgl_GetProcAddress("glVertexWeightfEXT");
    glVertexWeightfvEXT = (glVertexWeightfvEXTPROC) extgl_GetProcAddress("glVertexWeightfvEXT");
    glVertexWeightPointerEXT = (glVertexWeightPointerEXTPROC) extgl_GetProcAddress("glVertexWeightPointerEXT");
#endif
}

void extgl_InitARBMultisample()
{
#ifdef GL_ARB_multisample
    if (!extgl_Extensions.ARB_multisample)
        return;
    glSampleCoverageARB = (glSampleCoverageARBPROC) extgl_GetProcAddress("glSampleCoverageARB");
#endif
}

void extgl_InitNVRegisterCombiners()
{
#ifdef GL_NV_register_combiners
    if (!extgl_Extensions.NV_register_combiners)
        return;
    glCombinerParameterfvNV = (glCombinerParameterfvNVPROC) extgl_GetProcAddress("glCombinerParameterfvNV");
    glCombinerParameterfNV = (glCombinerParameterfNVPROC) extgl_GetProcAddress("glCombinerParameterfNV");
    glCombinerParameterivNV = (glCombinerParameterivNVPROC) extgl_GetProcAddress("glCombinerParameterivNV");
    glCombinerParameteriNV = (glCombinerParameteriNVPROC) extgl_GetProcAddress("glCombinerParameteriNV");
    glCombinerInputNV = (glCombinerInputNVPROC) extgl_GetProcAddress("glCombinerInputNV");
    glCombinerOutputNV = (glCombinerOutputNVPROC) extgl_GetProcAddress("glCombinerOutputNV");
    glFinalCombinerInputNV = (glFinalCombinerInputNVPROC) extgl_GetProcAddress("glFinalCombinerInputNV");
    glGetCombinerInputParameterfvNV = (glGetCombinerInputParameterfvNVPROC) extgl_GetProcAddress("glGetCombinerInputParameterfvNV");
    glGetCombinerInputParameterivNV = (glGetCombinerInputParameterivNVPROC) extgl_GetProcAddress("glGetCombinerInputParameterivNV");
    glGetCombinerOutputParameterfvNV = (glGetCombinerOutputParameterfvNVPROC) extgl_GetProcAddress("glGetCombinerOutputParameterfvNV");
    glGetCombinerOutputParameterivNV = (glGetCombinerOutputParameterivNVPROC) extgl_GetProcAddress("glGetCombinerOutputParameterivNV");
    glGetFinalCombinerInputParameterfvNV = (glGetFinalCombinerInputParameterfvNVPROC) extgl_GetProcAddress("glGetFinalCombinerInputParameterfvNV");
    glGetFinalCombinerInputParameterivNV = (glGetFinalCombinerInputParameterivNVPROC) extgl_GetProcAddress("glGetFinalCombinerInputParameterivNV");
#endif
}

void extgl_InitEXTPointParameters()
{
#ifdef GL_EXT_point_parameters
    if (!extgl_Extensions.EXT_point_parameters)
        return;
    glPointParameterfEXT = (glPointParameterfEXTPROC) extgl_GetProcAddress("glPointParameterfEXT");
    glPointParameterfvEXT = (glPointParameterfvEXTPROC) extgl_GetProcAddress("glPointParameterfvEXT");
#endif
}

void extgl_InitNVVertexArrayRange()
{
#ifdef GL_NV_vertex_array_range
    if (!extgl_Extensions.NV_vertex_array_range)
        return;
    glFlushVertexArrayRangeNV = (glFlushVertexArrayRangeNVPROC) extgl_GetProcAddress("glFlushVertexArrayRangeNV");
    glVertexArrayRangeNV = (glVertexArrayRangeNVPROC) extgl_GetProcAddress("glVertexArrayRangeNV");
#ifdef _WIN32
    wglAllocateMemoryNV = (wglAllocateMemoryNVPROC) extgl_GetProcAddress("wglAllocateMemoryNV");
    wglFreeMemoryNV = (wglFreeMemoryNVPROC) extgl_GetProcAddress("wglFreeMemoryNV");
#else
    glXAllocateMemoryNV = (glXAllocateMemoryNVPROC) extgl_GetProcAddress("glXAllocateMemoryNV");
    glXFreeMemoryNV = (glXFreeMemoryNVPROC) extgl_GetProcAddress("glXFreeMemoryNV");
#endif /* WIN32 */
#endif
}
 
void extgl_InitEXTFogCoord()
{
#ifdef GL_EXT_fog_coord
    if (!extgl_Extensions.EXT_fog_coord)
        return;
    glFogCoordfEXT = (glFogCoordfEXTPROC) extgl_GetProcAddress("glFogCoordfEXT");
    glFogCoordfvEXT = (glFogCoordfvEXTPROC) extgl_GetProcAddress("glFogCoordfvEXT");
    glFogCoorddEXT = (glFogCoorddEXTPROC) extgl_GetProcAddress("glFogCoorddEXT");
    glFogCoorddvEXT = (glFogCoorddvEXTPROC) extgl_GetProcAddress("glFogCoorddvEXT");
    glFogCoordPointerEXT = (glFogCoordPointerEXTPROC) extgl_GetProcAddress("glFogCoordPointerEXT");
#endif
}

void extgl_InitEXTSecondaryColor()
{
#ifdef GL_EXT_secondary_color
    if (!extgl_Extensions.EXT_secondary_color)
        return;
    glSecondaryColor3bEXT = (glSecondaryColor3bEXTPROC) extgl_GetProcAddress("glSecondaryColor3bEXT");
    glSecondaryColor3bvEXT = (glSecondaryColor3bvEXTPROC) extgl_GetProcAddress("glSecondaryColor3bvEXT");
    glSecondaryColor3dEXT = (glSecondaryColor3dEXTPROC) extgl_GetProcAddress("glSecondaryColor3dEXT");
    glSecondaryColor3dvEXT = (glSecondaryColor3dvEXTPROC) extgl_GetProcAddress("glSecondaryColor3dvEXT");
    glSecondaryColor3fEXT = (glSecondaryColor3fEXTPROC) extgl_GetProcAddress("glSecondaryColor3fEXT");
    glSecondaryColor3fvEXT = (glSecondaryColor3fvEXTPROC) extgl_GetProcAddress("glSecondaryColor3fvEXT");
    glSecondaryColor3iEXT = (glSecondaryColor3iEXTPROC) extgl_GetProcAddress("glSecondaryColor3iEXT");
    glSecondaryColor3ivEXT = (glSecondaryColor3ivEXTPROC) extgl_GetProcAddress("glSecondaryColor3ivEXT");
    glSecondaryColor3sEXT = (glSecondaryColor3sEXTPROC) extgl_GetProcAddress("glSecondaryColor3sEXT");
    glSecondaryColor3svEXT = (glSecondaryColor3svEXTPROC) extgl_GetProcAddress("glSecondaryColor3svEXT");
    glSecondaryColor3ubEXT = (glSecondaryColor3ubEXTPROC) extgl_GetProcAddress("glSecondaryColor3ubEXT");
    glSecondaryColor3ubvEXT = (glSecondaryColor3ubvEXTPROC) extgl_GetProcAddress("glSecondaryColor3ubvEXT");
    glSecondaryColor3uiEXT = (glSecondaryColor3uiEXTPROC) extgl_GetProcAddress("glSecondaryColor3uiEXT");
    glSecondaryColor3uivEXT = (glSecondaryColor3uivEXTPROC) extgl_GetProcAddress("glSecondaryColor3uivEXT");
    glSecondaryColor3usEXT = (glSecondaryColor3usEXTPROC) extgl_GetProcAddress("glSecondaryColor3usEXT");
    glSecondaryColor3usvEXT = (glSecondaryColor3usvEXTPROC) extgl_GetProcAddress("glSecondaryColor3usvEXT");
    glSecondaryColorPointerEXT = (glSecondaryColorPointerEXTPROC) extgl_GetProcAddress("glSecondaryColorPointerEXT");
#endif
}

void extgl_InitEXTCompiledVertexArray()
{
#ifdef GL_EXT_compiled_vertex_array
    if (!extgl_Extensions.EXT_compiled_vertex_array)
        return;
    glLockArraysEXT = (glLockArraysEXTPROC) extgl_GetProcAddress("glLockArraysEXT");
    glUnlockArraysEXT = (glUnlockArraysEXTPROC) extgl_GetProcAddress("glUnlockArraysEXT");
#endif
}

void extgl_InitARBTransposeMatrix()
{
#ifdef GL_ARB_transpose_matrix
    if (!extgl_Extensions.ARB_transpose_matrix)
        return;
    glLoadTransposeMatrixfARB = (glLoadTransposeMatrixfARBPROC) extgl_GetProcAddress("glLoadTransposeMatrixfARB");
    glLoadTransposeMatrixdARB = (glLoadTransposeMatrixdARBPROC) extgl_GetProcAddress("glLoadTransposeMatrixdARB");
    glMultTransposeMatrixfARB = (glMultTransposeMatrixfARBPROC) extgl_GetProcAddress("glMultTransposeMatrixfARB");
    glMultTransposeMatrixdARB = (glMultTransposeMatrixdARBPROC) extgl_GetProcAddress("glMultTransposeMatrixdARB");
#endif
}

void extgl_InitEXTDrawRangeElements()
{
#ifdef GL_EXT_draw_range_elements
    if (!extgl_Extensions.EXT_draw_range_elements)
        return;
    glDrawRangeElementsEXT = (glDrawRangeElementsEXTPROC) extgl_GetProcAddress("glDrawRangeElementsEXT");
#endif
}

void extgl_InitARBMultitexture()
{
#ifdef _WIN32
#ifdef GL_ARB_multitexture
    if (!extgl_Extensions.ARB_multitexture)
        return;
    glActiveTextureARB = (glActiveTextureARBPROC) extgl_GetProcAddress("glActiveTextureARB");
    glClientActiveTextureARB = (glClientActiveTextureARBPROC) extgl_GetProcAddress("glClientActiveTextureARB");

    glMultiTexCoord1dARB = (glMultiTexCoord1dARBPROC) extgl_GetProcAddress("glMultiTexCoord1dARB");
    glMultiTexCoord1dvARB = (glMultiTexCoord1dvARBPROC) extgl_GetProcAddress("glMultiTexCoord1dvARB");
    glMultiTexCoord1fARB = (glMultiTexCoord1fARBPROC) extgl_GetProcAddress("glMultiTexCoord1fARB");
    glMultiTexCoord1fvARB = (glMultiTexCoord1fvARBPROC) extgl_GetProcAddress("glMultiTexCoord1fvARB");
    glMultiTexCoord1iARB = (glMultiTexCoord1iARBPROC) extgl_GetProcAddress("glMultiTexCoord1iARB");
    glMultiTexCoord1ivARB = (glMultiTexCoord1ivARBPROC) extgl_GetProcAddress("glMultiTexCoord1ivARB");
    glMultiTexCoord1sARB = (glMultiTexCoord1sARBPROC) extgl_GetProcAddress("glMultiTexCoord1sARB");
    glMultiTexCoord1svARB = (glMultiTexCoord1svARBPROC) extgl_GetProcAddress("glMultiTexCoord1svARB");

    glMultiTexCoord2dARB = (glMultiTexCoord2dARBPROC) extgl_GetProcAddress("glMultiTexCoord2dARB");
    glMultiTexCoord2dvARB = (glMultiTexCoord2dvARBPROC) extgl_GetProcAddress("glMultiTexCoord2dvARB");
    glMultiTexCoord2fARB = (glMultiTexCoord2fARBPROC) extgl_GetProcAddress("glMultiTexCoord2fARB");
    glMultiTexCoord2fvARB = (glMultiTexCoord2fvARBPROC) extgl_GetProcAddress("glMultiTexCoord2fvARB");
    glMultiTexCoord2iARB = (glMultiTexCoord2iARBPROC) extgl_GetProcAddress("glMultiTexCoord2iARB");
    glMultiTexCoord2ivARB = (glMultiTexCoord2ivARBPROC) extgl_GetProcAddress("glMultiTexCoord2ivARB");
    glMultiTexCoord2sARB = (glMultiTexCoord2sARBPROC) extgl_GetProcAddress("glMultiTexCoord2sARB");
    glMultiTexCoord2svARB = (glMultiTexCoord2svARBPROC) extgl_GetProcAddress("glMultiTexCoord2svARB");

    glMultiTexCoord3dARB = (glMultiTexCoord3dARBPROC) extgl_GetProcAddress("glMultiTexCoord3dARB");
    glMultiTexCoord3dvARB = (glMultiTexCoord3dvARBPROC) extgl_GetProcAddress("glMultiTexCoord3dvARB");
    glMultiTexCoord3fARB = (glMultiTexCoord3fARBPROC) extgl_GetProcAddress("glMultiTexCoord3fARB");
    glMultiTexCoord3fvARB = (glMultiTexCoord3fvARBPROC) extgl_GetProcAddress("glMultiTexCoord3fvARB");
    glMultiTexCoord3iARB = (glMultiTexCoord3iARBPROC) extgl_GetProcAddress("glMultiTexCoord3iARB");
    glMultiTexCoord3ivARB = (glMultiTexCoord3ivARBPROC) extgl_GetProcAddress("glMultiTexCoord3ivARB");
    glMultiTexCoord3sARB = (glMultiTexCoord3sARBPROC) extgl_GetProcAddress("glMultiTexCoord3sARB");
    glMultiTexCoord3svARB = (glMultiTexCoord3svARBPROC) extgl_GetProcAddress("glMultiTexCoord3svARB");

    glMultiTexCoord4dARB = (glMultiTexCoord4dARBPROC) extgl_GetProcAddress("glMultiTexCoord4dARB");
    glMultiTexCoord4dvARB = (glMultiTexCoord4dvARBPROC) extgl_GetProcAddress("glMultiTexCoord4dvARB");
    glMultiTexCoord4fARB = (glMultiTexCoord4fARBPROC) extgl_GetProcAddress("glMultiTexCoord4fARB");
    glMultiTexCoord4fvARB = (glMultiTexCoord4fvARBPROC) extgl_GetProcAddress("glMultiTexCoord4fvARB");
    glMultiTexCoord4iARB = (glMultiTexCoord4iARBPROC) extgl_GetProcAddress("glMultiTexCoord4iARB");
    glMultiTexCoord4ivARB = (glMultiTexCoord4ivARBPROC) extgl_GetProcAddress("glMultiTexCoord4ivARB");
    glMultiTexCoord4sARB = (glMultiTexCoord4sARBPROC) extgl_GetProcAddress("glMultiTexCoord4sARB");
    glMultiTexCoord4svARB = (glMultiTexCoord4svARBPROC) extgl_GetProcAddress("glMultiTexCoord4svARB");
#endif /* GL_ARB_multitexture */
#endif /* WIN32 */
}

void extgl_InitOpenGL1_2()
{
#ifdef _WIN32
#ifdef GL_VERSION_1_2
    if (!extgl_Extensions.OpenGL12)
        return;
    glTexImage3D = (glTexImage3DPROC) extgl_GetProcAddress("glTexImage3D");
    glTexSubImage3D = (glTexSubImage3DPROC) extgl_GetProcAddress("glTexSubImage3D");
    glCopyTexSubImage3D = (glCopyTexSubImage3DPROC) extgl_GetProcAddress("glCopyTexSubImage3D");
    glDrawRangeElements = (glDrawRangeElementsPROC) extgl_GetProcAddress("glDrawRangeElements");
#endif /* GL_VERSION_1_2 */
#endif /* WIN32 */
}

void extgl_InitARBImaging()
{
#ifdef _WIN32
#ifdef GL_ARB_imaging
    if (!extgl_Extensions.ARB_imaging)
        return;
    glBlendColor = (glBlendColorPROC) extgl_GetProcAddress("glBlendColor");
    glBlendEquation = (glBlendEquationPROC) extgl_GetProcAddress("glBlendEquation");
    glColorTable = (glColorTablePROC) extgl_GetProcAddress("glColorTable");
    glColorTableParameterfv = (glColorTableParameterfvPROC) extgl_GetProcAddress("glColorTableParameterfv");
    glColorTableParameteriv = (glColorTableParameterivPROC) extgl_GetProcAddress("glColorTableParameteriv");
    glCopyColorTable = (glCopyColorTablePROC) extgl_GetProcAddress("glCopyColorTable");
    glGetColorTable = (glGetColorTablePROC) extgl_GetProcAddress("glGetColorTable");
    glGetColorTableParameterfv = (glGetColorTableParameterfvPROC) extgl_GetProcAddress("glGetColorTableParameterfv");
    glGetColorTableParameteriv = (glGetColorTableParameterivPROC) extgl_GetProcAddress("glGetColorTableParameteriv");
    glColorSubTable = (glColorSubTablePROC) extgl_GetProcAddress("glColorSubTable");
    glCopyColorSubTable = (glCopyColorSubTablePROC) extgl_GetProcAddress("glCopyColorSubTable");
    glConvolutionFilter1D = (glConvolutionFilter1DPROC) extgl_GetProcAddress("glConvolutionFilter1D");
    glConvolutionFilter2D = (glConvolutionFilter2DPROC) extgl_GetProcAddress("glConvolutionFilter2D");
    glConvolutionParameterf = (glConvolutionParameterfPROC) extgl_GetProcAddress("glConvolutionParameterf");
    glConvolutionParameterfv = (glConvolutionParameterfvPROC) extgl_GetProcAddress("glConvolutionParameterfv");
    glConvolutionParameteri = (glConvolutionParameteriPROC) extgl_GetProcAddress("glConvolutionParameteri");
    glConvolutionParameteriv = (glConvolutionParameterivPROC) extgl_GetProcAddress("glConvolutionParameteriv");
    glCopyConvolutionFilter1D = (glCopyConvolutionFilter1DPROC) extgl_GetProcAddress("glCopyConvolutionFilter1D");
    glCopyConvolutionFilter2D = (glCopyConvolutionFilter2DPROC) extgl_GetProcAddress("glCopyConvolutionFilter2D");
    glGetConvolutionFilter = (glGetConvolutionFilterPROC) extgl_GetProcAddress("glGetConvolutionFilter");
    glGetConvolutionParameterfv = (glGetConvolutionParameterfvPROC) extgl_GetProcAddress("glGetConvolutionParameterfv");
    glGetConvolutionParameteriv = (glGetConvolutionParameterivPROC) extgl_GetProcAddress("glGetConvolutionParameteriv");
    glGetSeparableFilter = (glGetSeparableFilterPROC) extgl_GetProcAddress("glGetSeparableFilter");
    glSeparableFilter2D = (glSeparableFilter2DPROC) extgl_GetProcAddress("glSeparableFilter2D");
    glGetHistogram = (glGetHistogramPROC) extgl_GetProcAddress("glGetHistogram");
    glGetHistogramParameterfv = (glGetHistogramParameterfvPROC) extgl_GetProcAddress("glGetHistogramParameterfv");
    glGetHistogramParameteriv = (glGetHistogramParameterivPROC) extgl_GetProcAddress("glGetHistogramParameteriv");
    glGetMinmax = (glGetMinmaxPROC) extgl_GetProcAddress("glGetMinmax");
    glGetMinmaxParameterfv = (glGetMinmaxParameterfvPROC) extgl_GetProcAddress("glGetMinmaxParameterfv");
    glGetMinmaxParameteriv = (glGetMinmaxParameterivPROC) extgl_GetProcAddress("glGetMinmaxParameteriv");
    glHistogram = (glHistogramPROC) extgl_GetProcAddress("glHistogram");
    glMinmax = (glMinmaxPROC) extgl_GetProcAddress("glMinmax");
    glResetHistogram = (glResetHistogramPROC) extgl_GetProcAddress("glResetHistogram");
    glResetMinmax = (glResetMinmaxPROC) extgl_GetProcAddress("glResetMinmax");
#endif /* GL_ARB_imaging */
#endif /* WIN32 */
}

void extgl_InitOpenGL1_3()
{
#ifdef _WIN32
#ifdef GL_VERSION_1_3
    if (!extgl_Extensions.OpenGL13)
        return;
    glActiveTexture = (glActiveTexturePROC) extgl_GetProcAddress("glActiveTexture");
    glClientActiveTexture = (glClientActiveTexturePROC) extgl_GetProcAddress("glClientActiveTexture");

    glMultiTexCoord1d = (glMultiTexCoord1dPROC) extgl_GetProcAddress("glMultiTexCoord1d");
    glMultiTexCoord1dv = (glMultiTexCoord1dvPROC) extgl_GetProcAddress("glMultiTexCoord1dv");
    glMultiTexCoord1f = (glMultiTexCoord1fPROC) extgl_GetProcAddress("glMultiTexCoord1f");
    glMultiTexCoord1fv = (glMultiTexCoord1fvPROC) extgl_GetProcAddress("glMultiTexCoord1fv");
    glMultiTexCoord1i = (glMultiTexCoord1iPROC) extgl_GetProcAddress("glMultiTexCoord1i");
    glMultiTexCoord1iv = (glMultiTexCoord1ivPROC) extgl_GetProcAddress("glMultiTexCoord1iv");
    glMultiTexCoord1s = (glMultiTexCoord1sPROC) extgl_GetProcAddress("glMultiTexCoord1s");
    glMultiTexCoord1sv = (glMultiTexCoord1svPROC) extgl_GetProcAddress("glMultiTexCoord1sv");

    glMultiTexCoord2d = (glMultiTexCoord2dPROC) extgl_GetProcAddress("glMultiTexCoord2d");
    glMultiTexCoord2dv = (glMultiTexCoord2dvPROC) extgl_GetProcAddress("glMultiTexCoord2dv");
    glMultiTexCoord2f = (glMultiTexCoord2fPROC) extgl_GetProcAddress("glMultiTexCoord2f");
    glMultiTexCoord2fv = (glMultiTexCoord2fvPROC) extgl_GetProcAddress("glMultiTexCoord2fv");
    glMultiTexCoord2i = (glMultiTexCoord2iPROC) extgl_GetProcAddress("glMultiTexCoord2i");
    glMultiTexCoord2iv = (glMultiTexCoord2ivPROC) extgl_GetProcAddress("glMultiTexCoord2iv");
    glMultiTexCoord2s = (glMultiTexCoord2sPROC) extgl_GetProcAddress("glMultiTexCoord2s");
    glMultiTexCoord2sv = (glMultiTexCoord2svPROC) extgl_GetProcAddress("glMultiTexCoord2sv");

    glMultiTexCoord3d = (glMultiTexCoord3dPROC) extgl_GetProcAddress("glMultiTexCoord3d");
    glMultiTexCoord3dv = (glMultiTexCoord3dvPROC) extgl_GetProcAddress("glMultiTexCoord3dv");
    glMultiTexCoord3f = (glMultiTexCoord3fPROC) extgl_GetProcAddress("glMultiTexCoord3f");
    glMultiTexCoord3fv = (glMultiTexCoord3fvPROC) extgl_GetProcAddress("glMultiTexCoord3fv");
    glMultiTexCoord3i = (glMultiTexCoord3iPROC) extgl_GetProcAddress("glMultiTexCoord3i");
    glMultiTexCoord3iv = (glMultiTexCoord3ivPROC) extgl_GetProcAddress("glMultiTexCoord3iv");
    glMultiTexCoord3s = (glMultiTexCoord3sPROC) extgl_GetProcAddress("glMultiTexCoord3s");
    glMultiTexCoord3sv = (glMultiTexCoord3svPROC) extgl_GetProcAddress("glMultiTexCoord3sv");

    glMultiTexCoord4d = (glMultiTexCoord4dPROC) extgl_GetProcAddress("glMultiTexCoord4d");
    glMultiTexCoord4dv = (glMultiTexCoord4dvPROC) extgl_GetProcAddress("glMultiTexCoord4dv");
    glMultiTexCoord4f = (glMultiTexCoord4fPROC) extgl_GetProcAddress("glMultiTexCoord4f");
    glMultiTexCoord4fv = (glMultiTexCoord4fvPROC) extgl_GetProcAddress("glMultiTexCoord4fv");
    glMultiTexCoord4i = (glMultiTexCoord4iPROC) extgl_GetProcAddress("glMultiTexCoord4i");
    glMultiTexCoord4iv = (glMultiTexCoord4ivPROC) extgl_GetProcAddress("glMultiTexCoord4iv");
    glMultiTexCoord4s = (glMultiTexCoord4sPROC) extgl_GetProcAddress("glMultiTexCoord4s");
    glMultiTexCoord4sv = (glMultiTexCoord4svPROC) extgl_GetProcAddress("glMultiTexCoord4sv");

    glLoadTransposeMatrixf = (glLoadTransposeMatrixfPROC) extgl_GetProcAddress("glLoadTransposeMatrixf");
    glLoadTransposeMatrixd = (glLoadTransposeMatrixdPROC) extgl_GetProcAddress("glLoadTransposeMatrixd");
    glMultTransposeMatrixf = (glMultTransposeMatrixfPROC) extgl_GetProcAddress("glMultTransposeMatrixf");
    glMultTransposeMatrixd = (glMultTransposeMatrixdPROC) extgl_GetProcAddress("glMultTransposeMatrixd");
    glCompressedTexImage3D = (glCompressedTexImage3DPROC) extgl_GetProcAddress("glCompressedTexImage3D");
    glCompressedTexImage2D = (glCompressedTexImage2DPROC) extgl_GetProcAddress("glCompressedTexImage2D");
    glCompressedTexImage1D = (glCompressedTexImage1DPROC) extgl_GetProcAddress("glCompressedTexImage1D");
    glCompressedTexSubImage3D = (glCompressedTexSubImage3DPROC) extgl_GetProcAddress("glCompressedTexSubImage3D");
    glCompressedTexSubImage2D = (glCompressedTexSubImage2DPROC) extgl_GetProcAddress("glCompressedTexSubImage2D");
    glCompressedTexSubImage1D = (glCompressedTexSubImage1DPROC) extgl_GetProcAddress("glCompressedTexSubImage1D");
    glGetCompressedTexImage = (glGetCompressedTexImagePROC) extgl_GetProcAddress("glGetCompressedTexImage");

    glSampleCoverage = (glSampleCoveragePROC) extgl_GetProcAddress("glSampleCoverage");
#endif /* GL_VERSION_1_3 */
#endif /* WIN32 */
}

void extgl_InitOpenGL1_4()
{
#ifdef _WIN32
#ifdef GL_VERSION_1_4
    if (!extgl_Extensions.OpenGL14)
        return;
    glBlendColor = (glBlendColorPROC) extgl_GetProcAddress("glBlendColor");
    glBlendEquation = (glBlendEquationPROC) extgl_GetProcAddress("glBlendEquation");
    glFogCoordf = (glFogCoordfPROC) extgl_GetProcAddress("glFogCoordf");
    glFogCoordfv = (glFogCoordfvPROC) extgl_GetProcAddress("glFogCoordfv");
    glFogCoordd = (glFogCoorddPROC) extgl_GetProcAddress("glFogCoordd");
    glFogCoorddv = (glFogCoorddvPROC) extgl_GetProcAddress("glFogCoorddv");
    glFogCoordPointer = (glFogCoordPointerPROC) extgl_GetProcAddress("glFogCoordPointer");
    glMultiDrawArrays = (glMultiDrawArraysPROC) extgl_GetProcAddress("glMultiDrawArrays");
    glMultiDrawElements = (glMultiDrawElementsPROC) extgl_GetProcAddress("glMultiDrawElements");
    glPointParameterf = (glPointParameterfPROC) extgl_GetProcAddress("glPointParameterf");
    glPointParameterfv = (glPointParameterfvPROC) extgl_GetProcAddress("glPointParameterfv");
    glSecondaryColor3b = (glSecondaryColor3bPROC) extgl_GetProcAddress("glSecondaryColor3b");
    glSecondaryColor3bv = (glSecondaryColor3bvPROC) extgl_GetProcAddress("glSecondaryColor3bv");
    glSecondaryColor3d = (glSecondaryColor3dPROC) extgl_GetProcAddress("glSecondaryColor3d");
    glSecondaryColor3dv = (glSecondaryColor3dvPROC) extgl_GetProcAddress("glSecondaryColor3dv");
    glSecondaryColor3f = (glSecondaryColor3fPROC) extgl_GetProcAddress("glSecondaryColor3f");
    glSecondaryColor3fv = (glSecondaryColor3fvPROC) extgl_GetProcAddress("glSecondaryColor3fv");
    glSecondaryColor3i = (glSecondaryColor3iPROC) extgl_GetProcAddress("glSecondaryColor3i");
    glSecondaryColor3iv = (glSecondaryColor3ivPROC) extgl_GetProcAddress("glSecondaryColor3iv");
    glSecondaryColor3s = (glSecondaryColor3sPROC) extgl_GetProcAddress("glSecondaryColor3s");
    glSecondaryColor3sv = (glSecondaryColor3svPROC) extgl_GetProcAddress("glSecondaryColor3sv");
    glSecondaryColor3ub = (glSecondaryColor3ubPROC) extgl_GetProcAddress("glSecondaryColor3ub");
    glSecondaryColor3ubv = (glSecondaryColor3ubvPROC) extgl_GetProcAddress("glSecondaryColor3ubv");
    glSecondaryColor3ui = (glSecondaryColor3uiPROC) extgl_GetProcAddress("glSecondaryColor3ui");
    glSecondaryColor3uiv = (glSecondaryColor3uivPROC) extgl_GetProcAddress("glSecondaryColor3uiv");
    glSecondaryColor3us = (glSecondaryColor3usPROC) extgl_GetProcAddress("glSecondaryColor3us");
    glSecondaryColor3usv = (glSecondaryColor3usvPROC) extgl_GetProcAddress("glSecondaryColor3usv");
    glSecondaryColorPointer = (glSecondaryColorPointerPROC) extgl_GetProcAddress("glSecondaryColorPointer");
    glBlendFuncSeparate = (glBlendFuncSeparatePROC) extgl_GetProcAddress("glBlendFuncSeparate");
    glWindowPos2d = (glWindowPos2dPROC) extgl_GetProcAddress("glWindowPos2d");
    glWindowPos2f = (glWindowPos2fPROC) extgl_GetProcAddress("glWindowPos2f");
    glWindowPos2i = (glWindowPos2iPROC) extgl_GetProcAddress("glWindowPos2i");
    glWindowPos2s = (glWindowPos2sPROC) extgl_GetProcAddress("glWindowPos2s");
    glWindowPos2dv = (glWindowPos2dvPROC) extgl_GetProcAddress("glWindowPos2dv");
    glWindowPos2fv = (glWindowPos2fvPROC) extgl_GetProcAddress("glWindowPos2fv");
    glWindowPos2iv = (glWindowPos2ivPROC) extgl_GetProcAddress("glWindowPos2iv");
    glWindowPos2sv = (glWindowPos2svPROC) extgl_GetProcAddress("glWindowPos2sv");
    glWindowPos3d = (glWindowPos3dPROC) extgl_GetProcAddress("glWindowPos3d");
    glWindowPos3f = (glWindowPos3fPROC) extgl_GetProcAddress("glWindowPos3f");
    glWindowPos3i = (glWindowPos3iPROC) extgl_GetProcAddress("glWindowPos3i");
    glWindowPos3s = (glWindowPos3sPROC) extgl_GetProcAddress("glWindowPos3s");
    glWindowPos3dv = (glWindowPos3dvPROC) extgl_GetProcAddress("glWindowPos3dv");
    glWindowPos3fv = (glWindowPos3fvPROC) extgl_GetProcAddress("glWindowPos3fv");
    glWindowPos3iv = (glWindowPos3ivPROC) extgl_GetProcAddress("glWindowPos3iv");
    glWindowPos3sv = (glWindowPos3svPROC) extgl_GetProcAddress("glWindowPos3sv");
#endif /* GL_VERSION_1_4 */
#endif /* WIN32 */
}

void extgl_InitSupportedExtensions()
{
    char *s = (char*) glGetString(GL_VERSION);
    if (!s)
        return;
    const char v[2]={s[0],'\0'};
    int major = atoi(v);
    if(atoi(v) >= 2){
      extgl_Extensions.OpenGL12 = 1;
      extgl_Extensions.OpenGL13 = 1;
      extgl_Extensions.OpenGL14 = 1;
    }else{
    s = strstr(s, "1.");
    }
    if (s == NULL)
    {
        extgl_Extensions.OpenGL12 = 0;    
        extgl_Extensions.OpenGL13 = 0;    
        extgl_Extensions.OpenGL14 = 0;
    }
    else
    {
        extgl_Extensions.OpenGL12 = 0;
        extgl_Extensions.OpenGL13 = 0;
        extgl_Extensions.OpenGL14 = 0;

        if( s[2] >= '4' )
        {
            extgl_Extensions.OpenGL12 = 1;
            extgl_Extensions.OpenGL13 = 1;
            extgl_Extensions.OpenGL14 = 1;
        }
        if( s[2] == '3' )
        {
            extgl_Extensions.OpenGL12 = 1;
            extgl_Extensions.OpenGL13 = 1;
        }
        if( s[2] == '2' )
        {
            extgl_Extensions.OpenGL12 = 1;
        }
    }
    extgl_Extensions.ARB_depth_texture = QueryExtension("GL_ARB_depth_texture");
    extgl_Extensions.ARB_fragment_program = QueryExtension("GL_ARB_fragment_program");
    extgl_Extensions.ARB_imaging = QueryExtension("GL_ARB_imaging");
    extgl_Extensions.ARB_matrix_palette = QueryExtension("GL_ARB_matrix_palette");
    extgl_Extensions.ARB_multisample = QueryExtension("GL_ARB_multisample");
    extgl_Extensions.ARB_multitexture = QueryExtension("GL_ARB_multitexture");
    extgl_Extensions.ARB_point_parameters = QueryExtension("GL_ARB_point_parameters");
    extgl_Extensions.ARB_shadow = QueryExtension("GL_ARB_shadow");
    extgl_Extensions.ARB_shadow_ambient = QueryExtension("GL_ARB_shadow_ambient");
    extgl_Extensions.ARB_texture_border_clamp = QueryExtension("GL_ARB_texture_border_clamp");
    extgl_Extensions.ARB_texture_compression = QueryExtension("GL_ARB_texture_compression");
    extgl_Extensions.ARB_texture_cube_map = QueryExtension("GL_ARB_texture_cube_map");
    extgl_Extensions.ARB_texture_env_add = QueryExtension("GL_ARB_texture_env_add");
    extgl_Extensions.ARB_texture_env_combine = QueryExtension("GL_ARB_texture_env_combine");
    extgl_Extensions.ARB_texture_env_crossbar = QueryExtension("GL_ARB_texture_env_crossbar");
    extgl_Extensions.ARB_texture_env_dot3 = QueryExtension("GL_ARB_texture_env_dot3");
    extgl_Extensions.ARB_texture_mirrored_repeat = QueryExtension("GL_ARB_texture_mirrored_repeat");
    extgl_Extensions.ARB_transpose_matrix = QueryExtension("GL_ARB_transpose_matrix");
    extgl_Extensions.ARB_vertex_blend = QueryExtension("GL_ARB_vertex_blend");
    extgl_Extensions.ARB_vertex_program = QueryExtension("GL_ARB_vertex_program");
    extgl_Extensions.ARB_window_pos = QueryExtension("GL_ARB_window_pos");
    extgl_Extensions.EXT_abgr = QueryExtension("GL_EXT_abgr");
    extgl_Extensions.EXT_bgra = QueryExtension("GL_EXT_bgra");
    extgl_Extensions.EXT_blend_func_separate = QueryExtension("GL_EXT_blend_function_separate");
    extgl_Extensions.EXT_compiled_vertex_array = QueryExtension("GL_EXT_compiled_vertex_array");
    extgl_Extensions.EXT_cull_vertex = QueryExtension("GL_EXT_cull_vertex");
    extgl_Extensions.EXT_draw_range_elements = QueryExtension("GL_EXT_draw_range_elements");
    extgl_Extensions.EXT_fog_coord = QueryExtension("GL_EXT_fog_coord");
    extgl_Extensions.EXT_multi_draw_arrays = QueryExtension("GL_EXT_multi_draw_arrays");
    extgl_Extensions.EXT_point_parameters = QueryExtension("GL_EXT_point_parameters");
    extgl_Extensions.EXT_secondary_color = QueryExtension("GL_EXT_secondary_color");
    extgl_Extensions.EXT_separate_specular_color = QueryExtension("GL_EXT_separate_specular_color");
    extgl_Extensions.EXT_shadow_funcs = QueryExtension("GL_EXT_shadow_funcs");
    extgl_Extensions.EXT_stencil_two_side = QueryExtension("GL_EXT_stencil_two_side");
    extgl_Extensions.EXT_stencil_wrap = QueryExtension("GL_EXT_stencil_wrap");
    extgl_Extensions.EXT_texture_compression_s3tc = QueryExtension("GL_EXT_texture_compression_s3tc");
    extgl_Extensions.EXT_texture_env_combine = QueryExtension("GL_EXT_texture_env_combine");
    extgl_Extensions.EXT_texture_filter_anisotropic = QueryExtension("GL_EXT_texture_filter_anisotropic");
    extgl_Extensions.EXT_texture_lod_bias = QueryExtension("GL_EXT_texture_lod_bias");
    extgl_Extensions.EXT_texture_rectangle = QueryExtension("GL_EXT_texture_rectangle"); // added -ec
    extgl_Extensions.EXT_vertex_shader = QueryExtension("GL_EXT_vertex_shader");
    extgl_Extensions.EXT_vertex_weighting = QueryExtension("GL_EXT_vertex_weighting");
    extgl_Extensions.ATI_draw_buffers = QueryExtension("GL_ATI_draw_buffers"); // added -ec    
    extgl_Extensions.ATI_element_array = QueryExtension("GL_ATI_element_array");
    extgl_Extensions.ATI_envmap_bumpmap = QueryExtension("GL_ATI_envmap_bumpmap");
    extgl_Extensions.ATI_fragment_shader = QueryExtension("GL_ATI_fragment_shader");
    extgl_Extensions.ATI_pn_triangles = QueryExtension("GL_ATI_pn_triangles");
    extgl_Extensions.ATI_point_cull_mode = QueryExtension("GL_ATI_point_cull_mode");
    extgl_Extensions.ATI_text_fragment_shader = QueryExtension("GL_ATI_text_fragment_shader");
    extgl_Extensions.ATI_texture_float = QueryExtension("GL_ATI_texture_float"); // added -ec
    extgl_Extensions.ATI_texture_mirror_once = QueryExtension("GL_ATI_texture_mirror_once");
    extgl_Extensions.ATI_vertex_array_object = QueryExtension("GL_ATI_vertex_array_object");
    extgl_Extensions.ATI_vertex_streams = QueryExtension("GL_ATI_vertex_streams");
    extgl_Extensions.ATIX_point_sprites = QueryExtension("GL_ATIX_point_sprites");
    extgl_Extensions.ATIX_texture_env_route = QueryExtension("GL_ATIX_texture_env_route");
    extgl_Extensions.HP_occlusion_test = QueryExtension("GL_HP_occlusion_test");
    extgl_Extensions.NV_blend_square = QueryExtension("GL_NV_blend_square");
    extgl_Extensions.NV_copy_depth_to_color = QueryExtension("GL_NV_copy_depth_to_color");
    extgl_Extensions.NV_depth_clamp = QueryExtension("GL_NV_depth_clamp");
    extgl_Extensions.NV_element_array = QueryExtension("GL_NV_element_array");
    extgl_Extensions.NV_evaluators = QueryExtension("GL_NV_evaluators");
    extgl_Extensions.NV_fence = QueryExtension("GL_NV_fence");
    extgl_Extensions.NV_float_buffer = QueryExtension("GL_NV_float_buffer");
    extgl_Extensions.NV_fog_distance = QueryExtension("GL_NV_fog_distance");
    extgl_Extensions.NV_fragment_program = QueryExtension("GL_NV_fragment_program");
    extgl_Extensions.NV_light_max_exponent = QueryExtension("GL_NV_light_max_exponent");
    extgl_Extensions.NV_occlusion_query = QueryExtension("GL_NV_occlusion_query");
    extgl_Extensions.NV_packed_depth_stencil = QueryExtension("GL_NV_packed_depth_stencil");
    extgl_Extensions.NV_point_sprite = QueryExtension("GL_NV_point_sprite");
    extgl_Extensions.NV_primitive_restart = QueryExtension("GL_NV_primitive_restart");
    extgl_Extensions.NV_register_combiners = QueryExtension("GL_NV_register_combiners");
    extgl_Extensions.NV_register_combiners2 = QueryExtension("GL_NV_register_combiners2");
    extgl_Extensions.NV_texgen_reflection = QueryExtension("GL_NV_texgen_reflection");
    extgl_Extensions.NV_texture_env_combine4 = QueryExtension("GL_NV_texture_env_combine4");
    extgl_Extensions.NV_texture_rectangle = QueryExtension("GL_NV_texture_rectangle");
    extgl_Extensions.NV_texture_shader = QueryExtension("GL_NV_texture_shader");
    extgl_Extensions.NV_texture_shader2 = QueryExtension("GL_NV_texture_shader2");
    extgl_Extensions.NV_texture_shader3 = QueryExtension("GL_NV_texture_shader3");
    extgl_Extensions.NV_vertex_array_range = QueryExtension("GL_NV_vertex_array_range");
    extgl_Extensions.NV_vertex_array_range2 = QueryExtension("GL_NV_vertex_array_range2");
    extgl_Extensions.NV_vertex_program = QueryExtension("GL_NV_vertex_program");
    extgl_Extensions.NV_vertex_program1_1 = QueryExtension("GL_NV_vertex_program1_1");
    extgl_Extensions.NV_vertex_program2 = QueryExtension("GL_NV_vertex_program2");
    extgl_Extensions.SGIS_generate_mipmap = QueryExtension("GL_SGIS_generate_mipmap");
    extgl_Extensions.SGIX_depth_texture = QueryExtension("GL_SGIX_depth_texture");
    extgl_Extensions.SGIX_shadow = QueryExtension("GL_SGIX_shadow");
}


/* extgl_Init the extensions and load all the functions */
int extgl_Initialize()
{
    extgl_error = 0;
    extgl_InitSupportedExtensions();
    
    /* first load the etensions */
    extgl_InitARBTransposeMatrix();
    extgl_InitARBMultisample();
    extgl_InitEXTCompiledVertexArray();
    extgl_InitEXTSecondaryColor();
    extgl_InitEXTFogCoord();
    extgl_InitNVVertexArrayRange();
    extgl_InitEXTPointParameters();
    extgl_InitNVRegisterCombiners();
    extgl_InitEXTVertexWeighting();
    extgl_InitNVVertexProgram();
    extgl_InitNVFence();
    extgl_InitNVRegisterCombiners2();
    extgl_InitATIPNTriangles();
    extgl_InitARBPointParameters();
    extgl_InitARBVertexBlend();
    extgl_InitEXTMultiDrawArrays();
    extgl_InitARBMatrixPalette();
    extgl_InitEXTVertexShader();
    extgl_InitATIEnvmapBumpmap();
    extgl_InitATIFragmentShader();
    extgl_InitATIElementArray();
    extgl_InitATIVertexStreams();
    extgl_InitATIVertexArrayObject();
    extgl_InitNVOcclusionQuery();
    extgl_InitNVPointSprite();
    extgl_InitARBWindowPos();
    extgl_InitARBTextureCompression();
    extgl_InitEXTDrawRangeElements();
    extgl_InitEXTStencilTwoSide();
    extgl_InitARBVertexProgram();
    extgl_InitEXTCullVertex();
    extgl_InitEXTBlendFuncSeparate();
    extgl_InitARBImaging();
    extgl_InitARBMultitexture();
    extgl_InitNVElementArray();
    extgl_InitNVFragmentProgram();
    extgl_InitNVPrimitiveRestart();
    extgl_InitARBFragmentProgram();
    extgl_InitATIDrawBuffers();
    
   /* now load core opengl */
    extgl_InitOpenGL1_2();
    extgl_InitOpenGL1_3();
    extgl_InitOpenGL1_4();

    /* load WGL extensions */
#ifdef _WIN32
    extgl_InitializeWGL();
#endif

    SupportedExtensions = extgl_Extensions;
    return extgl_error;
}

/* deprecated function please do not use it, use extgl_Initialize() instead */
int glInitialize()
{
    return extgl_Initialize();
}

/* turn on the warning for the borland compiler*/
#ifdef __BORLANDC__
#pragma warn .8064
#pragma warn .8065
#endif /* __BORLANDC__	*/
