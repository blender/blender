/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2014 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Jason Wilkins
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file intern/gl-deprecated.h
 *  \ingroup glew-mx
 *  Utility used to check for use of deprecated functions.
 */

#ifndef __GL_DEPRECATED_H__
#define __GL_DEPRECATED_H__

// GL Version 1.0
#undef glAccum
#define glAccum DO_NOT_USE_glAccum
#undef glAlphaFunc
#define glAlphaFunc DO_NOT_USE_glAlphaFunc
#undef glBegin
#define glBegin DO_NOT_USE_glBegin
#undef glBitmap
#define glBitmap DO_NOT_USE_glBitmap
#undef glCallList
#define glCallList DO_NOT_USE_glCallList
#undef glCallLists
#define glCallLists DO_NOT_USE_glCallLists
#undef glClearAccum
#define glClearAccum DO_NOT_USE_glClearAccum
#undef glClearIndex
#define glClearIndex DO_NOT_USE_glClearIndex
#undef glClipPlane
#define glClipPlane DO_NOT_USE_glClipPlane
#undef glColor3b
#define glColor3b DO_NOT_USE_glColor3b
#undef glColor3bv
#define glColor3bv DO_NOT_USE_glColor3bv
#undef glColor3d
#define glColor3d DO_NOT_USE_glColor3d
#undef glColor3dv
#define glColor3dv DO_NOT_USE_glColor3dv
#undef glColor3f
#define glColor3f DO_NOT_USE_glColor3f
#undef glColor3fv
#define glColor3fv DO_NOT_USE_glColor3fv
#undef glColor3i
#define glColor3i DO_NOT_USE_glColor3i
#undef glColor3iv
#define glColor3iv DO_NOT_USE_glColor3iv
#undef glColor3s
#define glColor3s DO_NOT_USE_glColor3s
#undef glColor3sv
#define glColor3sv DO_NOT_USE_glColor3sv
#undef glColor3ub
#define glColor3ub DO_NOT_USE_glColor3ub
#undef glColor3ubv
#define glColor3ubv DO_NOT_USE_glColor3ubv
#undef glColor3ui
#define glColor3ui DO_NOT_USE_glColor3ui
#undef glColor3uiv
#define glColor3uiv DO_NOT_USE_glColor3uiv
#undef glColor3us
#define glColor3us DO_NOT_USE_glColor3us
#undef glColor3usv
#define glColor3usv DO_NOT_USE_glColor3usv
#undef glColor4b
#define glColor4b DO_NOT_USE_glColor4b
#undef glColor4bv
#define glColor4bv DO_NOT_USE_glColor4bv
#undef glColor4d
#define glColor4d DO_NOT_USE_glColor4d
#undef glColor4dv
#define glColor4dv DO_NOT_USE_glColor4dv
#undef glColor4f
#define glColor4f DO_NOT_USE_glColor4f
#undef glColor4fv
#define glColor4fv DO_NOT_USE_glColor4fv
#undef glColor4i
#define glColor4i DO_NOT_USE_glColor4i
#undef glColor4iv
#define glColor4iv DO_NOT_USE_glColor4iv
#undef glColor4s
#define glColor4s DO_NOT_USE_glColor4s
#undef glColor4sv
#define glColor4sv DO_NOT_USE_glColor4sv
#undef glColor4ub
#define glColor4ub DO_NOT_USE_glColor4ub
#undef glColor4ubv
#define glColor4ubv DO_NOT_USE_glColor4ubv
#undef glColor4ui
#define glColor4ui DO_NOT_USE_glColor4ui
#undef glColor4uiv
#define glColor4uiv DO_NOT_USE_glColor4uiv
#undef glColor4us
#define glColor4us DO_NOT_USE_glColor4us
#undef glColor4usv
#define glColor4usv DO_NOT_USE_glColor4usv
#undef glColorMaterial
#define glColorMaterial DO_NOT_USE_glColorMaterial
#undef glCopyPixels
#define glCopyPixels DO_NOT_USE_glCopyPixels
#undef glDeleteLists
#define glDeleteLists DO_NOT_USE_glDeleteLists
#undef glDrawPixels
#define glDrawPixels DO_NOT_USE_glDrawPixels
#undef glEdgeFlag
#define glEdgeFlag DO_NOT_USE_glEdgeFlag
#undef glEdgeFlagv
#define glEdgeFlagv DO_NOT_USE_glEdgeFlagv
#undef glEnd
#define glEnd DO_NOT_USE_glEnd
#undef glEndList
#define glEndList DO_NOT_USE_glEndList
#undef glEvalCoord1d
#define glEvalCoord1d DO_NOT_USE_glEvalCoord1d
#undef glEvalCoord1dv
#define glEvalCoord1dv DO_NOT_USE_glEvalCoord1dv
#undef glEvalCoord1f
#define glEvalCoord1f DO_NOT_USE_glEvalCoord1f
#undef glEvalCoord1fv
#define glEvalCoord1fv DO_NOT_USE_glEvalCoord1fv
#undef glEvalCoord2d
#define glEvalCoord2d DO_NOT_USE_glEvalCoord2d
#undef glEvalCoord2dv
#define glEvalCoord2dv DO_NOT_USE_glEvalCoord2dv
#undef glEvalCoord2f
#define glEvalCoord2f DO_NOT_USE_glEvalCoord2f
#undef glEvalCoord2fv
#define glEvalCoord2fv DO_NOT_USE_glEvalCoord2fv
#undef glEvalMesh1
#define glEvalMesh1 DO_NOT_USE_glEvalMesh1
#undef glEvalMesh2
#define glEvalMesh2 DO_NOT_USE_glEvalMesh2
#undef glEvalPoint1
#define glEvalPoint1 DO_NOT_USE_glEvalPoint1
#undef glEvalPoint2
#define glEvalPoint2 DO_NOT_USE_glEvalPoint2
#undef glFeedbackBuffer
#define glFeedbackBuffer DO_NOT_USE_glFeedbackBuffer
#undef glFogf
#define glFogf DO_NOT_USE_glFogf
#undef glFogfv
#define glFogfv DO_NOT_USE_glFogfv
#undef glFogi
#define glFogi DO_NOT_USE_glFogi
#undef glFogiv
#define glFogiv DO_NOT_USE_glFogiv
#undef glFrustum
#define glFrustum DO_NOT_USE_glFrustum
#undef glGenLists
#define glGenLists DO_NOT_USE_glGenLists
#undef glGetClipPlane
#define glGetClipPlane DO_NOT_USE_glGetClipPlane
#undef glGetLightfv
#define glGetLightfv DO_NOT_USE_glGetLightfv
#undef glGetLightiv
#define glGetLightiv DO_NOT_USE_glGetLightiv
#undef glGetMapdv
#define glGetMapdv DO_NOT_USE_glGetMapdv
#undef glGetMapfv
#define glGetMapfv DO_NOT_USE_glGetMapfv
#undef glGetMapiv
#define glGetMapiv DO_NOT_USE_glGetMapiv
#undef glGetMaterialfv
#define glGetMaterialfv DO_NOT_USE_glGetMaterialfv
#undef glGetMaterialiv
#define glGetMaterialiv DO_NOT_USE_glGetMaterialiv
#undef glGetPixelMapfv
#define glGetPixelMapfv DO_NOT_USE_glGetPixelMapfv
#undef glGetPixelMapuiv
#define glGetPixelMapuiv DO_NOT_USE_glGetPixelMapuiv
#undef glGetPixelMapusv
#define glGetPixelMapusv DO_NOT_USE_glGetPixelMapusv
#undef glGetPolygonStipple
#define glGetPolygonStipple DO_NOT_USE_glGetPolygonStipple
#undef glGetTexEnvfv
#define glGetTexEnvfv DO_NOT_USE_glGetTexEnvfv
#undef glGetTexEnviv
#define glGetTexEnviv DO_NOT_USE_glGetTexEnviv
#undef glGetTexGendv
#define glGetTexGendv DO_NOT_USE_glGetTexGendv
#undef glGetTexGenfv
#define glGetTexGenfv DO_NOT_USE_glGetTexGenfv
#undef glGetTexGeniv
#define glGetTexGeniv DO_NOT_USE_glGetTexGeniv
#undef glIndexMask
#define glIndexMask DO_NOT_USE_glIndexMask
#undef glIndexd
#define glIndexd DO_NOT_USE_glIndexd
#undef glIndexdv
#define glIndexdv DO_NOT_USE_glIndexdv
#undef glIndexf
#define glIndexf DO_NOT_USE_glIndexf
#undef glIndexfv
#define glIndexfv DO_NOT_USE_glIndexfv
#undef glIndexi
#define glIndexi DO_NOT_USE_glIndexi
#undef glIndexiv
#define glIndexiv DO_NOT_USE_glIndexiv
#undef glIndexs
#define glIndexs DO_NOT_USE_glIndexs
#undef glIndexsv
#define glIndexsv DO_NOT_USE_glIndexsv
#undef glInitNames
#define glInitNames DO_NOT_USE_glInitNames
#undef glIsList
#define glIsList DO_NOT_USE_glIsList
#undef glLightModelf
#define glLightModelf DO_NOT_USE_glLightModelf
#undef glLightModelfv
#define glLightModelfv DO_NOT_USE_glLightModelfv
#undef glLightModeli
#define glLightModeli DO_NOT_USE_glLightModeli
#undef glLightModeliv
#define glLightModeliv DO_NOT_USE_glLightModeliv
#undef glLightf
#define glLightf DO_NOT_USE_glLightf
#undef glLightfv
#define glLightfv DO_NOT_USE_glLightfv
#undef glLighti
#define glLighti DO_NOT_USE_glLighti
#undef glLightiv
#define glLightiv DO_NOT_USE_glLightiv
#undef glLineStipple
#define glLineStipple DO_NOT_USE_glLineStipple
#undef glListBase
#define glListBase DO_NOT_USE_glListBase
#undef glLoadIdentity
#define glLoadIdentity DO_NOT_USE_glLoadIdentity
#undef glLoadMatrixd
#define glLoadMatrixd DO_NOT_USE_glLoadMatrixd
#undef glLoadMatrixf
#define glLoadMatrixf DO_NOT_USE_glLoadMatrixf
#undef glLoadName
#define glLoadName DO_NOT_USE_glLoadName
#undef glMap1d
#define glMap1d DO_NOT_USE_glMap1d
#undef glMap1f
#define glMap1f DO_NOT_USE_glMap1f
#undef glMap2d
#define glMap2d DO_NOT_USE_glMap2d
#undef glMap2f
#define glMap2f DO_NOT_USE_glMap2f
#undef glMapGrid1d
#define glMapGrid1d DO_NOT_USE_glMapGrid1d
#undef glMapGrid1f
#define glMapGrid1f DO_NOT_USE_glMapGrid1f
#undef glMapGrid2d
#define glMapGrid2d DO_NOT_USE_glMapGrid2d
#undef glMapGrid2f
#define glMapGrid2f DO_NOT_USE_glMapGrid2f
#undef glMaterialf
#define glMaterialf DO_NOT_USE_glMaterialf
#undef glMaterialfv
#define glMaterialfv DO_NOT_USE_glMaterialfv
#undef glMateriali
#define glMateriali DO_NOT_USE_glMateriali
#undef glMaterialiv
#define glMaterialiv DO_NOT_USE_glMaterialiv
#undef glMatrixMode
#define glMatrixMode DO_NOT_USE_glMatrixMode
#undef glMultMatrixd
#define glMultMatrixd DO_NOT_USE_glMultMatrixd
#undef glMultMatrixf
#define glMultMatrixf DO_NOT_USE_glMultMatrixf
#undef glNewList
#define glNewList DO_NOT_USE_glNewList
#undef glNormal3b
#define glNormal3b DO_NOT_USE_glNormal3b
#undef glNormal3bv
#define glNormal3bv DO_NOT_USE_glNormal3bv
#undef glNormal3d
#define glNormal3d DO_NOT_USE_glNormal3d
#undef glNormal3dv
#define glNormal3dv DO_NOT_USE_glNormal3dv
#undef glNormal3f
#define glNormal3f DO_NOT_USE_glNormal3f
#undef glNormal3fv
#define glNormal3fv DO_NOT_USE_glNormal3fv
#undef glNormal3i
#define glNormal3i DO_NOT_USE_glNormal3i
#undef glNormal3iv
#define glNormal3iv DO_NOT_USE_glNormal3iv
#undef glNormal3s
#define glNormal3s DO_NOT_USE_glNormal3s
#undef glNormal3sv
#define glNormal3sv DO_NOT_USE_glNormal3sv
#undef glOrtho
#define glOrtho DO_NOT_USE_glOrtho
#undef glPassThrough
#define glPassThrough DO_NOT_USE_glPassThrough
#undef glPixelMapfv
#define glPixelMapfv DO_NOT_USE_glPixelMapfv
#undef glPixelMapuiv
#define glPixelMapuiv DO_NOT_USE_glPixelMapuiv
#undef glPixelMapusv
#define glPixelMapusv DO_NOT_USE_glPixelMapusv
#undef glPixelTransferf
#define glPixelTransferf DO_NOT_USE_glPixelTransferf
#undef glPixelTransferi
#define glPixelTransferi DO_NOT_USE_glPixelTransferi
#undef glPixelZoom
#define glPixelZoom DO_NOT_USE_glPixelZoom
#undef glPolygonStipple
#define glPolygonStipple DO_NOT_USE_glPolygonStipple
#undef glPopAttrib
#define glPopAttrib DO_NOT_USE_glPopAttrib
#undef glPopMatrix
#define glPopMatrix DO_NOT_USE_glPopMatrix
#undef glPopName
#define glPopName DO_NOT_USE_glPopName
#undef glPushAttrib
#define glPushAttrib DO_NOT_USE_glPushAttrib
#undef glPushMatrix
#define glPushMatrix DO_NOT_USE_glPushMatrix
#undef glPushName
#define glPushName DO_NOT_USE_glPushName
#undef glRasterPos2d
#define glRasterPos2d DO_NOT_USE_glRasterPos2d
#undef glRasterPos2dv
#define glRasterPos2dv DO_NOT_USE_glRasterPos2dv
#undef glRasterPos2f
#define glRasterPos2f DO_NOT_USE_glRasterPos2f
#undef glRasterPos2fv
#define glRasterPos2fv DO_NOT_USE_glRasterPos2fv
#undef glRasterPos2i
#define glRasterPos2i DO_NOT_USE_glRasterPos2i
#undef glRasterPos2iv
#define glRasterPos2iv DO_NOT_USE_glRasterPos2iv
#undef glRasterPos2s
#define glRasterPos2s DO_NOT_USE_glRasterPos2s
#undef glRasterPos2sv
#define glRasterPos2sv DO_NOT_USE_glRasterPos2sv
#undef glRasterPos3d
#define glRasterPos3d DO_NOT_USE_glRasterPos3d
#undef glRasterPos3dv
#define glRasterPos3dv DO_NOT_USE_glRasterPos3dv
#undef glRasterPos3f
#define glRasterPos3f DO_NOT_USE_glRasterPos3f
#undef glRasterPos3fv
#define glRasterPos3fv DO_NOT_USE_glRasterPos3fv
#undef glRasterPos3i
#define glRasterPos3i DO_NOT_USE_glRasterPos3i
#undef glRasterPos3iv
#define glRasterPos3iv DO_NOT_USE_glRasterPos3iv
#undef glRasterPos3s
#define glRasterPos3s DO_NOT_USE_glRasterPos3s
#undef glRasterPos3sv
#define glRasterPos3sv DO_NOT_USE_glRasterPos3sv
#undef glRasterPos4d
#define glRasterPos4d DO_NOT_USE_glRasterPos4d
#undef glRasterPos4dv
#define glRasterPos4dv DO_NOT_USE_glRasterPos4dv
#undef glRasterPos4f
#define glRasterPos4f DO_NOT_USE_glRasterPos4f
#undef glRasterPos4fv
#define glRasterPos4fv DO_NOT_USE_glRasterPos4fv
#undef glRasterPos4i
#define glRasterPos4i DO_NOT_USE_glRasterPos4i
#undef glRasterPos4iv
#define glRasterPos4iv DO_NOT_USE_glRasterPos4iv
#undef glRasterPos4s
#define glRasterPos4s DO_NOT_USE_glRasterPos4s
#undef glRasterPos4sv
#define glRasterPos4sv DO_NOT_USE_glRasterPos4sv
#undef glRectd
#define glRectd DO_NOT_USE_glRectd
#undef glRectdv
#define glRectdv DO_NOT_USE_glRectdv
#undef glRectf
#define glRectf DO_NOT_USE_glRectf
#undef glRectfv
#define glRectfv DO_NOT_USE_glRectfv
#undef glRecti
#define glRecti DO_NOT_USE_glRecti
#undef glRectiv
#define glRectiv DO_NOT_USE_glRectiv
#undef glRects
#define glRects DO_NOT_USE_glRects
#undef glRectsv
#define glRectsv DO_NOT_USE_glRectsv
#undef glRenderMode
#define glRenderMode DO_NOT_USE_glRenderMode
#undef glRotated
#define glRotated DO_NOT_USE_glRotated
#undef glRotatef
#define glRotatef DO_NOT_USE_glRotatef
#undef glScaled
#define glScaled DO_NOT_USE_glScaled
#undef glScalef
#define glScalef DO_NOT_USE_glScalef
#undef glSelectBuffer
#define glSelectBuffer DO_NOT_USE_glSelectBuffer
#undef glShadeModel
#define glShadeModel DO_NOT_USE_glShadeModel
#undef glTexCoord1d
#define glTexCoord1d DO_NOT_USE_glTexCoord1d
#undef glTexCoord1dv
#define glTexCoord1dv DO_NOT_USE_glTexCoord1dv
#undef glTexCoord1f
#define glTexCoord1f DO_NOT_USE_glTexCoord1f
#undef glTexCoord1fv
#define glTexCoord1fv DO_NOT_USE_glTexCoord1fv
#undef glTexCoord1i
#define glTexCoord1i DO_NOT_USE_glTexCoord1i
#undef glTexCoord1iv
#define glTexCoord1iv DO_NOT_USE_glTexCoord1iv
#undef glTexCoord1s
#define glTexCoord1s DO_NOT_USE_glTexCoord1s
#undef glTexCoord1sv
#define glTexCoord1sv DO_NOT_USE_glTexCoord1sv
#undef glTexCoord2d
#define glTexCoord2d DO_NOT_USE_glTexCoord2d
#undef glTexCoord2dv
#define glTexCoord2dv DO_NOT_USE_glTexCoord2dv
#undef glTexCoord2f
#define glTexCoord2f DO_NOT_USE_glTexCoord2f
#undef glTexCoord2fv
#define glTexCoord2fv DO_NOT_USE_glTexCoord2fv
#undef glTexCoord2i
#define glTexCoord2i DO_NOT_USE_glTexCoord2i
#undef glTexCoord2iv
#define glTexCoord2iv DO_NOT_USE_glTexCoord2iv
#undef glTexCoord2s
#define glTexCoord2s DO_NOT_USE_glTexCoord2s
#undef glTexCoord2sv
#define glTexCoord2sv DO_NOT_USE_glTexCoord2sv
#undef glTexCoord3d
#define glTexCoord3d DO_NOT_USE_glTexCoord3d
#undef glTexCoord3dv
#define glTexCoord3dv DO_NOT_USE_glTexCoord3dv
#undef glTexCoord3f
#define glTexCoord3f DO_NOT_USE_glTexCoord3f
#undef glTexCoord3fv
#define glTexCoord3fv DO_NOT_USE_glTexCoord3fv
#undef glTexCoord3i
#define glTexCoord3i DO_NOT_USE_glTexCoord3i
#undef glTexCoord3iv
#define glTexCoord3iv DO_NOT_USE_glTexCoord3iv
#undef glTexCoord3s
#define glTexCoord3s DO_NOT_USE_glTexCoord3s
#undef glTexCoord3sv
#define glTexCoord3sv DO_NOT_USE_glTexCoord3sv
#undef glTexCoord4d
#define glTexCoord4d DO_NOT_USE_glTexCoord4d
#undef glTexCoord4dv
#define glTexCoord4dv DO_NOT_USE_glTexCoord4dv
#undef glTexCoord4f
#define glTexCoord4f DO_NOT_USE_glTexCoord4f
#undef glTexCoord4fv
#define glTexCoord4fv DO_NOT_USE_glTexCoord4fv
#undef glTexCoord4i
#define glTexCoord4i DO_NOT_USE_glTexCoord4i
#undef glTexCoord4iv
#define glTexCoord4iv DO_NOT_USE_glTexCoord4iv
#undef glTexCoord4s
#define glTexCoord4s DO_NOT_USE_glTexCoord4s
#undef glTexCoord4sv
#define glTexCoord4sv DO_NOT_USE_glTexCoord4sv
#undef glTexEnvf
#define glTexEnvf DO_NOT_USE_glTexEnvf
#undef glTexEnvfv
#define glTexEnvfv DO_NOT_USE_glTexEnvfv
#undef glTexEnvi
#define glTexEnvi DO_NOT_USE_glTexEnvi
#undef glTexEnviv
#define glTexEnviv DO_NOT_USE_glTexEnviv
#undef glTexGend
#define glTexGend DO_NOT_USE_glTexGend
#undef glTexGendv
#define glTexGendv DO_NOT_USE_glTexGendv
#undef glTexGenf
#define glTexGenf DO_NOT_USE_glTexGenf
#undef glTexGenfv
#define glTexGenfv DO_NOT_USE_glTexGenfv
#undef glTexGeni
#define glTexGeni DO_NOT_USE_glTexGeni
#undef glTexGeniv
#define glTexGeniv DO_NOT_USE_glTexGeniv
#undef glTranslated
#define glTranslated DO_NOT_USE_glTranslated
#undef glTranslatef
#define glTranslatef DO_NOT_USE_glTranslatef
#undef glVertex2d
#define glVertex2d DO_NOT_USE_glVertex2d
#undef glVertex2dv
#define glVertex2dv DO_NOT_USE_glVertex2dv
#undef glVertex2f
#define glVertex2f DO_NOT_USE_glVertex2f
#undef glVertex2fv
#define glVertex2fv DO_NOT_USE_glVertex2fv
#undef glVertex2i
#define glVertex2i DO_NOT_USE_glVertex2i
#undef glVertex2iv
#define glVertex2iv DO_NOT_USE_glVertex2iv
#undef glVertex2s
#define glVertex2s DO_NOT_USE_glVertex2s
#undef glVertex2sv
#define glVertex2sv DO_NOT_USE_glVertex2sv
#undef glVertex3d
#define glVertex3d DO_NOT_USE_glVertex3d
#undef glVertex3dv
#define glVertex3dv DO_NOT_USE_glVertex3dv
#undef glVertex3f
#define glVertex3f DO_NOT_USE_glVertex3f
#undef glVertex3fv
#define glVertex3fv DO_NOT_USE_glVertex3fv
#undef glVertex3i
#define glVertex3i DO_NOT_USE_glVertex3i
#undef glVertex3iv
#define glVertex3iv DO_NOT_USE_glVertex3iv
#undef glVertex3s
#define glVertex3s DO_NOT_USE_glVertex3s
#undef glVertex3sv
#define glVertex3sv DO_NOT_USE_glVertex3sv
#undef glVertex4d
#define glVertex4d DO_NOT_USE_glVertex4d
#undef glVertex4dv
#define glVertex4dv DO_NOT_USE_glVertex4dv
#undef glVertex4f
#define glVertex4f DO_NOT_USE_glVertex4f
#undef glVertex4fv
#define glVertex4fv DO_NOT_USE_glVertex4fv
#undef glVertex4i
#define glVertex4i DO_NOT_USE_glVertex4i
#undef glVertex4iv
#define glVertex4iv DO_NOT_USE_glVertex4iv
#undef glVertex4s
#define glVertex4s DO_NOT_USE_glVertex4s
#undef glVertex4sv
#define glVertex4sv DO_NOT_USE_glVertex4sv

// GL Version 1.1
#undef glAreTexturesResident
#define glAreTexturesResident DO_NOT_USE_glAreTexturesResident
#undef glArrayElement
#define glArrayElement DO_NOT_USE_glArrayElement
#undef glColorPointer
#define glColorPointer DO_NOT_USE_glColorPointer
#undef glDisableClientState
#define glDisableClientState DO_NOT_USE_glDisableClientState
#undef glEdgeFlagPointer
#define glEdgeFlagPointer DO_NOT_USE_glEdgeFlagPointer
#undef glEnableClientState
#define glEnableClientState DO_NOT_USE_glEnableClientState
#undef glIndexPointer
#define glIndexPointer DO_NOT_USE_glIndexPointer
#undef glIndexub
#define glIndexub DO_NOT_USE_glIndexub
#undef glIndexubv
#define glIndexubv DO_NOT_USE_glIndexubv
#undef glInterleavedArrays
#define glInterleavedArrays DO_NOT_USE_glInterleavedArrays
#undef glNormalPointer
#define glNormalPointer DO_NOT_USE_glNormalPointer
#undef glPopClientAttrib
#define glPopClientAttrib DO_NOT_USE_glPopClientAttrib
#undef glPrioritizeTextures
#define glPrioritizeTextures DO_NOT_USE_glPrioritizeTextures
#undef glPushClientAttrib
#define glPushClientAttrib DO_NOT_USE_glPushClientAttrib
#undef glTexCoordPointer
#define glTexCoordPointer DO_NOT_USE_glTexCoordPointer
#undef glVertexPointer
#define glVertexPointer DO_NOT_USE_glVertexPointer

// GL Version1.2
#undef glColorSubTable
#define glColorSubTable DO_NOT_USE_glColorSubTable
#undef glColorTable
#define glColorTable DO_NOT_USE_glColorTable
#undef glColorTableParameterfv
#define glColorTableParameterfv DO_NOT_USE_glColorTableParameterfv
#undef glColorTableParameteriv
#define glColorTableParameteriv DO_NOT_USE_glColorTableParameteriv
#undef glConvolutionFilter1D
#define glConvolutionFilter1D DO_NOT_USE_glConvolutionFilter1D
#undef glConvolutionFilter2D
#define glConvolutionFilter2D DO_NOT_USE_glConvolutionFilter2D
#undef glConvolutionParameterf
#define glConvolutionParameterf DO_NOT_USE_glConvolutionParameterf
#undef glConvolutionParameterfv
#define glConvolutionParameterfv DO_NOT_USE_glConvolutionParameterfv
#undef glConvolutionParameteri
#define glConvolutionParameteri DO_NOT_USE_glConvolutionParameteri
#undef glConvolutionParameteriv
#define glConvolutionParameteriv DO_NOT_USE_glConvolutionParameteriv
#undef glCopyColorSubTable
#define glCopyColorSubTable DO_NOT_USE_glCopyColorSubTable
#undef glCopyColorTable
#define glCopyColorTable DO_NOT_USE_glCopyColorTable
#undef glCopyConvolutionFilter1D
#define glCopyConvolutionFilter1D DO_NOT_USE_glCopyConvolutionFilter1D
#undef glCopyConvolutionFilter2D
#define glCopyConvolutionFilter2D DO_NOT_USE_glCopyConvolutionFilter2D
#undef glGetColorTable
#define glGetColorTable DO_NOT_USE_glGetColorTable
#undef glGetColorTableParameterfv
#define glGetColorTableParameterfv DO_NOT_USE_glGetColorTableParameterfv
#undef glGetColorTableParameteriv
#define glGetColorTableParameteriv DO_NOT_USE_glGetColorTableParameteriv
#undef glGetConvolutionFilter
#define glGetConvolutionFilter DO_NOT_USE_glGetConvolutionFilter
#undef glGetConvolutionParameterfv
#define glGetConvolutionParameterfv DO_NOT_USE_glGetConvolutionParameterfv
#undef glGetConvolutionParameteriv
#define glGetConvolutionParameteriv DO_NOT_USE_glGetConvolutionParameteriv
#undef glGetHistogram
#define glGetHistogram DO_NOT_USE_glGetHistogram
#undef glGetHistogramParameterfv
#define glGetHistogramParameterfv DO_NOT_USE_glGetHistogramParameterfv
#undef glGetHistogramParameteriv
#define glGetHistogramParameteriv DO_NOT_USE_glGetHistogramParameteriv
#undef glGetMinmax
#define glGetMinmax DO_NOT_USE_glGetMinmax
#undef glGetMinmaxParameterfv
#define glGetMinmaxParameterfv DO_NOT_USE_glGetMinmaxParameterfv
#undef glGetMinmaxParameteriv
#define glGetMinmaxParameteriv DO_NOT_USE_glGetMinmaxParameteriv
#undef glGetSeparableFilter
#define glGetSeparableFilter DO_NOT_USE_glGetSeparableFilter
#undef glHistogram
#define glHistogram DO_NOT_USE_glHistogram
#undef glMinmax
#define glMinmax DO_NOT_USE_glMinmax
#undef glResetHistogram
#define glResetHistogram DO_NOT_USE_glResetHistogram
#undef glResetMinmax
#define glResetMinmax DO_NOT_USE_glResetMinmax
#undef glSeparableFilter2D
#define glSeparableFilter2D DO_NOT_USE_glSeparableFilter2D

// GL Version1.3
#undef glClientActiveTexture
#define glClientActiveTexture DO_NOT_USE_glClientActiveTexture
#undef glLoadTransposeMatrixd
#define glLoadTransposeMatrixd DO_NOT_USE_glLoadTransposeMatrixd
#undef glLoadTransposeMatrixf
#define glLoadTransposeMatrixf DO_NOT_USE_glLoadTransposeMatrixf
#undef glMultTransposeMatrixd
#define glMultTransposeMatrixd DO_NOT_USE_glMultTransposeMatrixd
#undef glMultTransposeMatrixf
#define glMultTransposeMatrixf DO_NOT_USE_glMultTransposeMatrixf
#undef glMultiTexCoord1d
#define glMultiTexCoord1d DO_NOT_USE_glMultiTexCoord1d
#undef glMultiTexCoord1dv
#define glMultiTexCoord1dv DO_NOT_USE_glMultiTexCoord1dv
#undef glMultiTexCoord1f
#define glMultiTexCoord1f DO_NOT_USE_glMultiTexCoord1f
#undef glMultiTexCoord1fv
#define glMultiTexCoord1fv DO_NOT_USE_glMultiTexCoord1fv
#undef glMultiTexCoord1i
#define glMultiTexCoord1i DO_NOT_USE_glMultiTexCoord1i
#undef glMultiTexCoord1iv
#define glMultiTexCoord1iv DO_NOT_USE_glMultiTexCoord1iv
#undef glMultiTexCoord1s
#define glMultiTexCoord1s DO_NOT_USE_glMultiTexCoord1s
#undef glMultiTexCoord1sv
#define glMultiTexCoord1sv DO_NOT_USE_glMultiTexCoord1sv
#undef glMultiTexCoord2d
#define glMultiTexCoord2d DO_NOT_USE_glMultiTexCoord2d
#undef glMultiTexCoord2dv
#define glMultiTexCoord2dv DO_NOT_USE_glMultiTexCoord2dv
#undef glMultiTexCoord2f
#define glMultiTexCoord2f DO_NOT_USE_glMultiTexCoord2f
#undef glMultiTexCoord2fv
#define glMultiTexCoord2fv DO_NOT_USE_glMultiTexCoord2fv
#undef glMultiTexCoord2i
#define glMultiTexCoord2i DO_NOT_USE_glMultiTexCoord2i
#undef glMultiTexCoord2iv
#define glMultiTexCoord2iv DO_NOT_USE_glMultiTexCoord2iv
#undef glMultiTexCoord2s
#define glMultiTexCoord2s DO_NOT_USE_glMultiTexCoord2s
#undef glMultiTexCoord2sv
#define glMultiTexCoord2sv DO_NOT_USE_glMultiTexCoord2sv
#undef glMultiTexCoord3d
#define glMultiTexCoord3d DO_NOT_USE_glMultiTexCoord3d
#undef glMultiTexCoord3dv
#define glMultiTexCoord3dv DO_NOT_USE_glMultiTexCoord3dv
#undef glMultiTexCoord3f
#define glMultiTexCoord3f DO_NOT_USE_glMultiTexCoord3f
#undef glMultiTexCoord3fv
#define glMultiTexCoord3fv DO_NOT_USE_glMultiTexCoord3fv
#undef glMultiTexCoord3i
#define glMultiTexCoord3i DO_NOT_USE_glMultiTexCoord3i
#undef glMultiTexCoord3iv
#define glMultiTexCoord3iv DO_NOT_USE_glMultiTexCoord3iv
#undef glMultiTexCoord3s
#define glMultiTexCoord3s DO_NOT_USE_glMultiTexCoord3s
#undef glMultiTexCoord3sv
#define glMultiTexCoord3sv DO_NOT_USE_glMultiTexCoord3sv
#undef glMultiTexCoord4d
#define glMultiTexCoord4d DO_NOT_USE_glMultiTexCoord4d
#undef glMultiTexCoord4dv
#define glMultiTexCoord4dv DO_NOT_USE_glMultiTexCoord4dv
#undef glMultiTexCoord4f
#define glMultiTexCoord4f DO_NOT_USE_glMultiTexCoord4f
#undef glMultiTexCoord4fv
#define glMultiTexCoord4fv DO_NOT_USE_glMultiTexCoord4fv
#undef glMultiTexCoord4i
#define glMultiTexCoord4i DO_NOT_USE_glMultiTexCoord4i
#undef glMultiTexCoord4iv
#define glMultiTexCoord4iv DO_NOT_USE_glMultiTexCoord4iv
#undef glMultiTexCoord4s
#define glMultiTexCoord4s DO_NOT_USE_glMultiTexCoord4s
#undef glMultiTexCoord4sv
#define glMultiTexCoord4sv DO_NOT_USE_glMultiTexCoord4sv

// GL Version 1.4
#undef glFogCoordPointer
#define glFogCoordPointer DO_NOT_USE_glFogCoordPointer
#undef glFogCoordd
#define glFogCoordd DO_NOT_USE_glFogCoordd
#undef glFogCoorddv
#define glFogCoorddv DO_NOT_USE_glFogCoorddv
#undef glFogCoordf
#define glFogCoordf DO_NOT_USE_glFogCoordf
#undef glFogCoordfv
#define glFogCoordfv DO_NOT_USE_glFogCoordfv
#undef glSecondaryColor3b
#define glSecondaryColor3b DO_NOT_USE_glSecondaryColor3b
#undef glSecondaryColor3bv
#define glSecondaryColor3bv DO_NOT_USE_glSecondaryColor3bv
#undef glSecondaryColor3d
#define glSecondaryColor3d DO_NOT_USE_glSecondaryColor3d
#undef glSecondaryColor3dv
#define glSecondaryColor3dv DO_NOT_USE_glSecondaryColor3dv
#undef glSecondaryColor3f
#define glSecondaryColor3f DO_NOT_USE_glSecondaryColor3f
#undef glSecondaryColor3fv
#define glSecondaryColor3fv DO_NOT_USE_glSecondaryColor3fv
#undef glSecondaryColor3i
#define glSecondaryColor3i DO_NOT_USE_glSecondaryColor3i
#undef glSecondaryColor3iv
#define glSecondaryColor3iv DO_NOT_USE_glSecondaryColor3iv
#undef glSecondaryColor3s
#define glSecondaryColor3s DO_NOT_USE_glSecondaryColor3s
#undef glSecondaryColor3sv
#define glSecondaryColor3sv DO_NOT_USE_glSecondaryColor3sv
#undef glSecondaryColor3ub
#define glSecondaryColor3ub DO_NOT_USE_glSecondaryColor3ub
#undef glSecondaryColor3ubv
#define glSecondaryColor3ubv DO_NOT_USE_glSecondaryColor3ubv
#undef glSecondaryColor3ui
#define glSecondaryColor3ui DO_NOT_USE_glSecondaryColor3ui
#undef glSecondaryColor3uiv
#define glSecondaryColor3uiv DO_NOT_USE_glSecondaryColor3uiv
#undef glSecondaryColor3us
#define glSecondaryColor3us DO_NOT_USE_glSecondaryColor3us
#undef glSecondaryColor3usv
#define glSecondaryColor3usv DO_NOT_USE_glSecondaryColor3usv
#undef glSecondaryColorPointer
#define glSecondaryColorPointer DO_NOT_USE_glSecondaryColorPointer
#undef glWindowPos2d
#define glWindowPos2d DO_NOT_USE_glWindowPos2d
#undef glWindowPos2dv
#define glWindowPos2dv DO_NOT_USE_glWindowPos2dv
#undef glWindowPos2f
#define glWindowPos2f DO_NOT_USE_glWindowPos2f
#undef glWindowPos2fv
#define glWindowPos2fv DO_NOT_USE_glWindowPos2fv
#undef glWindowPos2i
#define glWindowPos2i DO_NOT_USE_glWindowPos2i
#undef glWindowPos2iv
#define glWindowPos2iv DO_NOT_USE_glWindowPos2iv
#undef glWindowPos2s
#define glWindowPos2s DO_NOT_USE_glWindowPos2s
#undef glWindowPos2sv
#define glWindowPos2sv DO_NOT_USE_glWindowPos2sv
#undef glWindowPos3d
#define glWindowPos3d DO_NOT_USE_glWindowPos3d
#undef glWindowPos3dv
#define glWindowPos3dv DO_NOT_USE_glWindowPos3dv
#undef glWindowPos3f
#define glWindowPos3f DO_NOT_USE_glWindowPos3f
#undef glWindowPos3fv
#define glWindowPos3fv DO_NOT_USE_glWindowPos3fv
#undef glWindowPos3i
#define glWindowPos3i DO_NOT_USE_glWindowPos3i
#undef glWindowPos3iv
#define glWindowPos3iv DO_NOT_USE_glWindowPos3iv
#undef glWindowPos3s
#define glWindowPos3s DO_NOT_USE_glWindowPos3s
#undef glWindowPos3sv
#define glWindowPos3sv DO_NOT_USE_glWindowPos3sv

// Old Token Names 1.2
#undef GL_POINT_SIZE_RANGE
#define GL_POINT_SIZE_RANGE DO_NOT_USE_GL_POINT_SIZE_RANGE
#undef GL_POINT_SIZE_GRANULARITY
#define GL_POINT_SIZE_GRANULARITY DO_NOT_USE_GL_POINT_SIZE_GRANULARITY

// Old Token Names 1.5
#undef GL_CURRENT_FOG_COORDINATE
#define GL_CURRENT_FOG_COORDINATE DO_NOT_USE_GL_CURRENT_FOG_COORDINATE
#undef GL_FOG_COORDINATE
#define GL_FOG_COORDINATE DO_NOT_USE_GL_FOG_COORDINATE
#undef GL_FOG_COORDINATE_ARRAY
#define GL_FOG_COORDINATE_ARRAY DO_NOT_USE_GL_FOG_COORDINATE_ARRAY
#undef GL_FOG_COORDINATE_ARRAY_BUFFER_BINDING
#define GL_FOG_COORDINATE_ARRAY_BUFFER_BINDING DO_NOT_USE_GL_FOG_COORDINATE_ARRAY_BUFFER_BINDING
#undef GL_FOG_COORDINATE_ARRAY_POINTER
#define GL_FOG_COORDINATE_ARRAY_POINTER DO_NOT_USE_GL_FOG_COORDINATE_ARRAY_POINTER
#undef GL_FOG_COORDINATE_ARRAY_STRIDE
#define GL_FOG_COORDINATE_ARRAY_STRIDE DO_NOT_USE_GL_FOG_COORDINATE_ARRAY_STRIDE
#undef GL_FOG_COORDINATE_ARRAY_TYPE
#define GL_FOG_COORDINATE_ARRAY_TYPE DO_NOT_USE_GL_FOG_COORDINATE_ARRAY_TYPE
#undef GL_FOG_COORDINATE_SOURCE
#define GL_FOG_COORDINATE_SOURCE DO_NOT_USE_GL_FOG_COORDINATE_SOURCE
#undef GL_SOURCE0_ALPHA
#define GL_SOURCE0_ALPHA DO_NOT_USE_GL_SOURCE0_ALPHA
#undef GL_SOURCE0_RGB
#define GL_SOURCE0_RGB DO_NOT_USE_GL_SOURCE0_RGB
#undef GL_SOURCE1_ALPHA
#define GL_SOURCE1_ALPHA DO_NOT_USE_GL_SOURCE1_ALPHA
#undef GL_SOURCE1_RGB
#define GL_SOURCE1_RGB DO_NOT_USE_GL_SOURCE1_RGB
#undef GL_SOURCE2_ALPHA
#define GL_SOURCE2_ALPHA DO_NOT_USE_GL_SOURCE2_ALPHA
#undef GL_SOURCE2_RGB
#define GL_SOURCE2_RGB DO_NOT_USE_GL_SOURCE2_RGB

// Old Token Names 3.0
#undef GL_CLIP_PLANE0
#define GL_CLIP_PLANE0 USE_GL_CLIP_DISTANCE0
#undef GL_CLIP_PLANE1
#define GL_CLIP_PLANE1 USE_GL_CLIP_DISTANCE1
#undef GL_CLIP_PLANE2
#define GL_CLIP_PLANE2 USE_GL_CLIP_DISTANCE2
#undef GL_CLIP_PLANE3
#define GL_CLIP_PLANE3 USE_GL_CLIP_DISTANCE3
#undef GL_CLIP_PLANE4
#define GL_CLIP_PLANE4 USE_GL_CLIP_DISTANCE4
#undef GL_CLIP_PLANE5
#define GL_CLIP_PLANE5 USE_GL_CLIP_DISTANCE5
#undef GL_COMPARE_R_TO_TEXTURE
#define GL_COMPARE_R_TO_TEXTURE USE_GL_COMPARE_REF_TO_TEXTURE
#undef GL_MAX_CLIP_PLANES
#define GL_MAX_CLIP_PLANES USE_GL_MAX_CLIP_DISTANCES
#undef GL_MAX_VARYING_FLOATS
#define GL_MAX_VARYING_FLOATS USE__MAX_VARYING_COMPONENTS

// Old Token Names 3.2
#undef GL_VERTEX_PROGRAM_POINT_SIZE
#define GL_VERTEX_PROGRAM_POINT_SIZE USE_GL_PROGRAM_POINT_SIZE

// Old Token Names 4.1
#undef GL_CURRENT_PROGRAM
#define GL_CURRENT_PROGRAM DO_NOT_USE_GL_CURRENT_PROGRAM

#endif /* __GL_DEPRECATED_H__ */
