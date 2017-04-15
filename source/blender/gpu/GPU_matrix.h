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
 * Contributor(s): Alexandr Kuznetsov, Jason Wilkins, Mike Erwin
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file source/blender/gpu/GPU_matrix.h
 *  \ingroup gpu
 */

#ifndef _GPU_MATRIX_H_
#define _GPU_MATRIX_H_

#include "BLI_sys_types.h"
#include "GPU_glew.h"
#include "../../../intern/gawain/gawain/shader_interface.h"

#ifdef __cplusplus
extern "C" {
#endif

/* For now we support the legacy matrix stack in gpuGetMatrix functions.
 * Will remove this after switching to core profile, which can happen after
 * we convert all code to use the API in this file. */
#ifdef WITH_GL_PROFILE_CORE
	#define SUPPORT_LEGACY_MATRIX 0
#else
	#define SUPPORT_LEGACY_MATRIX 1
#endif


void gpuMatrixReset(void); /* to Identity transform & empty stack */

/* ModelView Matrix (2D or 3D) */

void gpuPushMatrix(void); /* TODO: PushCopy vs PushIdentity? */
void gpuPopMatrix(void);

void gpuLoadIdentity(void);

void gpuScaleUniform(float factor);


/* 3D ModelView Matrix */

void gpuLoadMatrix(const float m[4][4]);
void gpuMultMatrix(const float m[4][4]);

void gpuTranslate3f(float x, float y, float z);
void gpuTranslate3fv(const float vec[3]);
void gpuScale3f(float x, float y, float z);
void gpuScale3fv(const float vec[3]);
void gpuRotate3f(float deg, float x, float y, float z); /* axis of rotation should be a unit vector */
void gpuRotate3fv(float deg, const float axis[3]); /* axis of rotation should be a unit vector */
void gpuRotateAxis(float deg, char axis); /* TODO: enum for axis? */

void gpuLookAt(float eyeX, float eyeY, float eyeZ, float centerX, float centerY, float centerZ, float upX, float upY, float upZ);
/* TODO: variant that takes eye[3], center[3], up[3] */


/* 2D ModelView Matrix */

void gpuTranslate2f(float x, float y);
void gpuTranslate2fv(const float vec[2]);
void gpuScale2f(float x, float y);
void gpuScale2fv(const float vec[2]);
void gpuRotate2D(float deg);


/* 3D Projection Matrix */

void gpuLoadProjectionMatrix(const float m[4][4]);

void gpuOrtho(float left, float right, float bottom, float top, float near, float far);
void gpuFrustum(float left, float right, float bottom, float top, float near, float far);
void gpuPerspective(float fovy, float aspect, float near, float far);

/* 3D Projection between Window and World Space */

void gpuProject(const float world[3], const float model[4][4], const float proj[4][4], const int view[4], float win[3]);
bool gpuUnProject(const float win[3], const float model[4][4], const float proj[4][4], const int view[4], float world[3]);

/* 2D Projection Matrix */

void gpuOrtho2D(float left, float right, float bottom, float top);


/* functions to get matrix values */
const float *gpuGetModelViewMatrix(float m[4][4]);
const float *gpuGetProjectionMatrix(float m[4][4]);
const float *gpuGetModelViewProjectionMatrix(float m[4][4]);

const float *gpuGetNormalMatrix(float m[3][3]);
const float *gpuGetNormalMatrixInverse(float m[3][3]);


/* set uniform values for currently bound shader */
void gpuBindMatrices(const ShaderInterface*);
bool gpuMatricesDirty(void); /* since last bind */

#ifdef __cplusplus
}
#endif


#ifndef SUPPRESS_GENERIC_MATRIX_API
/* make matrix inputs generic, to avoid warnings */
#  define gpuMultMatrix(x)  gpuMultMatrix((const float (*)[4])(x))
#  define gpuLoadMatrix(x)  gpuLoadMatrix((const float (*)[4])(x))
#  define gpuLoadProjectionMatrix(x)  gpuLoadProjectionMatrix((const float (*)[4])(x))
#  define gpuGetModelViewMatrix(x)  gpuGetModelViewMatrix((float (*)[4])(x))
#  define gpuGetProjectionMatrix(x)  gpuGetProjectionMatrix((float (*)[4])(x))
#  define gpuGetModelViewProjectionMatrix(x)  gpuGetModelViewProjectionMatrix((float (*)[4])(x))
#  define gpuGetNormalMatrix(x)  gpuGetNormalMatrix((float (*)[3])(x))
#  define gpuGetNormalMatrixInverse(x)  gpuGetNormalMatrixInverse((float (*)[3])(x))
#endif /* SUPPRESS_GENERIC_MATRIX_API */
#endif /* GPU_MATRIX_H */
