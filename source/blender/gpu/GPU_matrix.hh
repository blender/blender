/* SPDX-FileCopyrightText: 2012 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

namespace blender::gpu {
class Shader;
}  // namespace blender::gpu

/**
 * To Identity transform & empty stack.
 */
void GPU_matrix_reset();

/* ModelView Matrix (2D or 3D) */

/**
 * Push a new matrix unto the stack (copying the previous matrix).
 *
 * \note A function `GPU_matrix_push_identity` could be added
 * instead of calling #GPU_matrix_identity_set after pushing.
 */
void GPU_matrix_push();
void GPU_matrix_pop();

/**
 * Set the model-view to an identity matrix.
 */
void GPU_matrix_identity_set();

/**
 * Scale the model-view matrix.
 */
void GPU_matrix_scale_1f(float factor);

/* 3D ModelView Matrix */

void GPU_matrix_set(const float m[4][4]);
void GPU_matrix_mul(const float m[4][4]);

void GPU_matrix_translate_3f(float x, float y, float z);
void GPU_matrix_translate_3fv(const float vec[3]);
void GPU_matrix_scale_3f(float x, float y, float z);
void GPU_matrix_scale_3fv(const float vec[3]);

/**
 * Axis of rotation should be a unit vector.
 */
void GPU_matrix_rotate_3f(float deg, float x, float y, float z);
/**
 * Axis of rotation should be a unit vector.
 */
void GPU_matrix_rotate_3fv(float deg, const float axis[3]);

void GPU_matrix_rotate_axis(float deg, char axis); /* TODO: enum for axis? */

void GPU_matrix_look_at(float eyeX,
                        float eyeY,
                        float eyeZ,
                        float centerX,
                        float centerY,
                        float centerZ,
                        float upX,
                        float upY,
                        float upZ);
/* TODO: variant that takes eye[3], center[3], up[3] */

/* 2D ModelView Matrix */

void GPU_matrix_translate_2f(float x, float y);
void GPU_matrix_translate_2fv(const float vec[2]);
void GPU_matrix_scale_2f(float x, float y);
void GPU_matrix_scale_2fv(const float vec[2]);
void GPU_matrix_rotate_2d(float deg);

/* Projection Matrix (2D or 3D). */

void GPU_matrix_push_projection();
void GPU_matrix_pop_projection();

/* 3D Projection Matrix. */

void GPU_matrix_identity_projection_set();
void GPU_matrix_projection_set(const float m[4][4]);

void GPU_matrix_ortho_set(float left, float right, float bottom, float top, float near, float far);
void GPU_matrix_ortho_set_z(float near, float far);

void GPU_matrix_frustum_set(
    float left, float right, float bottom, float top, float near, float far);
void GPU_matrix_perspective_set(float fovy, float aspect, float near, float far);

/* 3D Projection between Window and World Space */

struct GPUMatrixUnproject_Precalc {
  float model_inverted[4][4];
  float view[4];
  bool is_persp;
  /**
   * Result of #projmat_dimensions_db.
   * Using double precision here is important as far clipping ranges
   * can cause divide-by-zero when using float, see: #66937.
   */
  struct {
    double xmin, xmax;
    double ymin, ymax;
    double zmin, zmax;
  } dims;
};

bool GPU_matrix_unproject_precalc(GPUMatrixUnproject_Precalc *unproj_precalc,
                                  const float model[4][4],
                                  const float proj[4][4],
                                  const int view[4]);

void GPU_matrix_project_3fv(const float world[3],
                            const float model[4][4],
                            const float proj[4][4],
                            const int view[4],
                            float r_win[3]);

void GPU_matrix_project_2fv(const float world[3],
                            const float model[4][4],
                            const float proj[4][4],
                            const int view[4],
                            float r_win[2]);

bool GPU_matrix_unproject_3fv(const float win[3],
                              const float model_inverted[4][4],
                              const float proj[4][4],
                              const int view[4],
                              float r_world[3]);

/* 2D Projection Matrix. */

void GPU_matrix_ortho_2d_set(float left, float right, float bottom, float top);

/* Functions to get matrix values. */

const float (*GPU_matrix_model_view_get(float m[4][4]))[4];
const float (*GPU_matrix_projection_get(float m[4][4]))[4];
const float (*GPU_matrix_model_view_projection_get(float m[4][4]))[4];

const float (*GPU_matrix_normal_get(float m[3][3]))[3];
const float (*GPU_matrix_normal_inverse_get(float m[3][3]))[3];

/**
 * Set uniform values for currently bound shader.
 */
void GPU_matrix_bind(blender::gpu::Shader *shader);
bool GPU_matrix_dirty_get(); /* since last bind */

/**
 * Own working polygon offset.
 */
float GPU_polygon_offset_calc(const float (*winmat)[4], float viewdist, float dist);
/**
 * \note \a viewdist is only for orthographic projections at the moment.
 */
void GPU_polygon_offset(float viewdist, float dist);

/* Python API needs to be able to inspect the stack so errors raise exceptions
 * instead of crashing. */
#ifdef USE_GPU_PY_MATRIX_API
int GPU_matrix_stack_level_get_model_view();
int GPU_matrix_stack_level_get_projection();
/* static assert ensures this doesn't change! */
#  define GPU_PY_MATRIX_STACK_LEN 31
#endif /* USE_GPU_PY_MATRIX_API */

#ifndef SUPPRESS_GENERIC_MATRIX_API

#  if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
#    define _GPU_MAT3_CONST_CAST(x) \
      (_Generic((x), \
           void *: (const float (*)[3])(x), \
           float *: (const float (*)[3])(x), \
           float (*)[4]: (const float (*)[3])(x), \
           const void *: (const float (*)[3])(x), \
           const float *: (const float (*)[3])(x), \
           const float (*)[3]: (const float (*)[3])(x)))
#    define _GPU_MAT3_CAST(x) \
      (_Generic((x), \
           void *: (float (*)[3])(x), \
           float *: (float (*)[3])(x), \
           float (*)[3]: (float (*)[3])(x)))
#    define _GPU_MAT4_CONST_CAST(x) \
      (_Generic((x), \
           void *: (const float (*)[4])(x), \
           float *: (const float (*)[4])(x), \
           float (*)[4]: (const float (*)[4])(x), \
           const void *: (const float (*)[4])(x), \
           const float *: (const float (*)[4])(x), \
           const float (*)[4]: (const float (*)[4])(x)))
#    define _GPU_MAT4_CAST(x) \
      (_Generic((x), \
           void *: (float (*)[4])(x), \
           float *: (float (*)[4])(x), \
           float (*)[4]: (float (*)[4])(x)))
#  else
#    define _GPU_MAT3_CONST_CAST(x) (const float (*)[3])(x)
#    define _GPU_MAT3_CAST(x) (float (*)[3])(x)
#    define _GPU_MAT4_CONST_CAST(x) (const float (*)[4])(x)
#    define _GPU_MAT4_CAST(x) (float (*)[4])(x)
#  endif /* C11 */

/* make matrix inputs generic, to avoid warnings */
#  define GPU_matrix_mul(x) GPU_matrix_mul(_GPU_MAT4_CONST_CAST(x))
#  define GPU_matrix_set(x) GPU_matrix_set(_GPU_MAT4_CONST_CAST(x))
#  define GPU_matrix_projection_set(x) GPU_matrix_projection_set(_GPU_MAT4_CONST_CAST(x))
#  define GPU_matrix_model_view_get(x) GPU_matrix_model_view_get(_GPU_MAT4_CAST(x))
#  define GPU_matrix_projection_get(x) GPU_matrix_projection_get(_GPU_MAT4_CAST(x))
#  define GPU_matrix_model_view_projection_get(x) \
    GPU_matrix_model_view_projection_get(_GPU_MAT4_CAST(x))
#  define GPU_matrix_normal_get(x) GPU_matrix_normal_get(_GPU_MAT3_CAST(x))
#  define GPU_matrix_normal_inverse_get(x) GPU_matrix_normal_inverse_get(_GPU_MAT3_CAST(x))
#endif /* SUPPRESS_GENERIC_MATRIX_API */

/* Not part of the GPU_matrix API,
 * however we need to check these limits in code that calls into these API's. */
constexpr static int GPU_MATRIX_ORTHO_CLIP_NEAR_DEFAULT = -100;
constexpr static int GPU_MATRIX_ORTHO_CLIP_FAR_DEFAULT = 100;
