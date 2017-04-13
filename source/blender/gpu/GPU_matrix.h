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
#define SUPPORT_LEGACY_MATRIX 1

/* implement 2D parts with 4x4 matrices, even though 3x3 feels better
 * this is a compromise to get core profile up & running sooner
 * external API stays (almost) the same
 */
#define MATRIX_2D_4x4 1


void gpuMatrixInit(void); /* called by system -- make private? */


/* MatrixMode is conceptually different from GL_MATRIX_MODE */

typedef enum {
	MATRIX_MODE_INACTIVE,
	MATRIX_MODE_2D,
	MATRIX_MODE_3D
} MatrixMode;

MatrixMode gpuMatrixMode(void);

void gpuMatrixBegin2D(void);
void gpuMatrixBegin3D(void);
void gpuMatrixEnd(void);
/* TODO: gpuMatrixResume2D & gpuMatrixResume3D to switch modes but not reset stack */


/* ModelView Matrix (2D or 3D) */

void gpuPushMatrix(void); /* TODO: PushCopy vs PushIdentity? */
void gpuPopMatrix(void);

void gpuLoadIdentity(void);

void gpuScaleUniform(float factor);


/* 3D ModelView Matrix */

void gpuLoadMatrix3D(const float m[4][4]);
void gpuMultMatrix3D(const float m[4][4]);
//const float *gpuGetMatrix3D(float m[4][4]);

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

#if MATRIX_2D_4x4
void gpuMultMatrix2D(const float m[4][4]);
#else
void gpuLoadMatrix2D(const float m[3][3]);
void gpuMultMatrix2D(const float m[3][3]);
#endif

void gpuTranslate2f(float x, float y);
void gpuTranslate2fv(const float vec[2]);
void gpuScale2f(float x, float y);
void gpuScale2fv(const float vec[2]);
void gpuRotate2D(float deg);


/* 3D Projection Matrix */

void gpuLoadProjectionMatrix3D(const float m[4][4]);

void gpuOrtho(float left, float right, float bottom, float top, float near, float far);
void gpuFrustum(float left, float right, float bottom, float top, float near, float far);
void gpuPerspective(float fovy, float aspect, float near, float far);

/* 3D Projection between Window and World Space */

void gpuProject(const float world[3], const float model[4][4], const float proj[4][4], const int view[4], float win[3]);
bool gpuUnProject(const float win[3], const float model[4][4], const float proj[4][4], const int view[4], float world[3]);

/* 2D Projection Matrix */

void gpuOrtho2D(float left, float right, float bottom, float top);


/* functions to get matrix values */
const float *gpuGetModelViewMatrix3D(float m[4][4]);
const float *gpuGetProjectionMatrix3D(float m[4][4]);
const float *gpuGetModelViewProjectionMatrix3D(float m[4][4]);

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
#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
#  define gpuMultMatrix3D(x)  \
	gpuMultMatrix3D(_Generic((x), \
	        float *:      (const float (*)[4])(x), \
	        float [16]:   (const float (*)[4])(x), \
	        float (*)[4]: (const float (*)[4])(x), \
	        float [4][4]: (const float (*)[4])(x), \
	        const float *:      (const float (*)[4])(x), \
	        const float [16]:   (const float (*)[4])(x), \
	        const float (*)[4]: (const float (*)[4])(x), \
	        const float [4][4]: (const float (*)[4])(x)) \
)
#  define gpuLoadMatrix3D(x)  \
	gpuLoadMatrix3D(_Generic((x), \
	        float *:      (const float (*)[4])(x), \
	        float [16]:   (const float (*)[4])(x), \
	        float (*)[4]: (const float (*)[4])(x), \
	        float [4][4]: (const float (*)[4])(x), \
	        const float *:      (const float (*)[4])(x), \
	        const float [16]:   (const float (*)[4])(x), \
	        const float (*)[4]: (const float (*)[4])(x), \
	        const float [4][4]: (const float (*)[4])(x)) \
)
/* TODO: finish this in a simpler way --^ */
#else
#  define gpuMultMatrix3D(x)  gpuMultMatrix3D((const float (*)[4])(x))
#  define gpuLoadMatrix3D(x)  gpuLoadMatrix3D((const float (*)[4])(x))

#  define gpuLoadProjectionMatrix3D(x)  gpuLoadProjectionMatrix3D((const float (*)[4])(x))

# if MATRIX_2D_4x4
#  define gpuMultMatrix2D(x)  gpuMultMatrix2D((const float (*)[4])(x))
# else
#  define gpuMultMatrix2D(x)  gpuMultMatrix2D((const float (*)[3])(x))
#  define gpuLoadMatrix2D(x)  gpuLoadMatrix2D((const float (*)[3])(x))
# endif

#  define gpuGetModelViewMatrix3D(x)  gpuGetModelViewMatrix3D((float (*)[4])(x))
#  define gpuGetProjectionMatrix3D(x)  gpuGetProjectionMatrix3D((float (*)[4])(x))
#  define gpuGetModelViewProjectionMatrix3D(x)  gpuGetModelViewProjectionMatrix3D((float (*)[4])(x))
#  define gpuGetNormalMatrix(x)  gpuGetNormalMatrix((float (*)[3])(x))
#  define gpuGetNormalMatrixInverse(x)  gpuGetNormalMatrixInverse((float (*)[3])(x))
#endif /* C11 */
#endif /* SUPPRESS_GENERIC_MATRIX_API */
#endif /* GPU_MATRIX_H */
