#ifndef _GPU_MATRIX_H_
#define _GPU_MATRIX_H_

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
 * The Original Code is Copyright (C) 2012 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Alexandr Kuznetsov, Jason Wilkins
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file source/blender/gpu/GPU_matrix.h
 *  \ingroup gpu
 */

#include "GPU_glew.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum eGPUMatrixMode {
	GPU_MODELVIEW_MATRIX = 0,
	GPU_PROJECTION_MATRIX = 1,
	GPU_TEXTURE_MATRIX = 2
} eGPUMatrixMode;

void gpuPushMatrix(eGPUMatrixMode stack);
void gpuPopMatrix(eGPUMatrixMode stack);

void gpuLoadMatrix(eGPUMatrixMode stack, const float m[16]);
void gpuLoadMatrixd(eGPUMatrixMode stack, const double m[16]);
const float *gpuGetMatrix(eGPUMatrixMode stack, float m[16]);
void gpuGetMatrixd(eGPUMatrixMode stack, double m[16]);

void gpuLoadIdentity(eGPUMatrixMode stack);

void gpuMultMatrix(eGPUMatrixMode stack, const float m[16]);
void gpuMultMatrixd(eGPUMatrixMode stack, const double m[16]);

void gpuTranslate(eGPUMatrixMode stack, float x, float y, float z);
void gpuScale(eGPUMatrixMode stack, GLfloat x, GLfloat y, GLfloat z);
void gpuRotateVector(eGPUMatrixMode stack, GLfloat deg, GLfloat vector[3]);
void gpuRotateAxis(eGPUMatrixMode stack, GLfloat deg, char axis);
void gpuRotateRight(eGPUMatrixMode stack, char type);

void gpuOrtho(eGPUMatrixMode stack, GLfloat left, GLfloat right, GLfloat bottom, GLfloat top, GLfloat nearVal, GLfloat farVal);
void gpuFrustum(eGPUMatrixMode stack, GLfloat left, GLfloat right, GLfloat bottom, GLfloat top, GLfloat nearVal, GLfloat farVal);

void gpuLoadOrtho(eGPUMatrixMode stack, GLfloat left, GLfloat right, GLfloat bottom, GLfloat top, GLfloat nearVal, GLfloat farVal);
void gpuLoadFrustum(eGPUMatrixMode stack, GLfloat left, GLfloat right, GLfloat bottom, GLfloat top, GLfloat nearVal, GLfloat farVal);

void gpuLookAt(eGPUMatrixMode stack, GLfloat eyeX, GLfloat eyeY, GLfloat eyeZ, GLfloat centerX, GLfloat centerY, GLfloat centerZ, GLfloat upX, GLfloat upY, GLfloat upZ);

void gpuProject(const GLfloat obj[3], const GLfloat model[16], const GLfloat proj[16], const GLint view[4], GLfloat win[3]);
GLboolean gpuUnProject(const GLfloat win[3], const GLfloat model[16], const GLfloat proj[16], const GLint view[4], GLfloat obj[3]);

void gpu_commit_matrix(void);

void GPU_feedback_vertex_3fv(GLenum type, GLfloat x, GLfloat y, GLfloat z,            GLfloat out[3]);
void GPU_feedback_vertex_4fv(GLenum type, GLfloat x, GLfloat y, GLfloat z, GLfloat w, GLfloat out[4]);
void GPU_feedback_vertex_4dv(GLenum type, GLdouble x, GLdouble y, GLdouble z, GLdouble w, GLdouble out[4]);

#if defined(GLEW_ES_ONLY)

/* ES 2.0 doesn't define these symbolic constants, but the matrix stack replacement library emulates them
 * (GL core has deprecated matrix stacks, but it should still be in the header) */

#ifndef GL_MODELVIEW_MATRIX
#define GL_MODELVIEW_MATRIX 0x0BA6
#endif

#ifndef GL_PROJECTION_MATRIX
#define GL_PROJECTION_MATRIX 0x0BA7
#endif

#ifndef GL_TEXTURE_MATRIX
#define GL_TEXTURE_MATRIX 0x0BA8
#endif

#endif

#ifdef __cplusplus
}
#endif

#endif /* GPU_MATRIX_H */
