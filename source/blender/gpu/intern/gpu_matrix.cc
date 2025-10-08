/* SPDX-FileCopyrightText: 2012 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "gpu_context_private.hh"
#include "gpu_matrix_private.hh"

#define SUPPRESS_GENERIC_MATRIX_API
#define USE_GPU_PY_MATRIX_API /* only so values are declared */
#include "GPU_matrix.hh"
#undef USE_GPU_PY_MATRIX_API

#include "BLI_math_matrix.h"
#include "BLI_math_rotation.h"
#include "BLI_math_vector.h"

#include "MEM_guardedalloc.h"

using namespace blender::gpu;

constexpr static int MATRIX_STACK_DEPTH = 32;

using Mat4 = float[4][4];
using Mat3 = float[3][3];

struct MatrixStack {
  Mat4 stack[MATRIX_STACK_DEPTH];
  uint top;
};

struct GPUMatrixState {
  MatrixStack model_view_stack;
  MatrixStack projection_stack;

  bool dirty;

  /* TODO: cache of derived matrices (Normal, MVP, inverse MVP, etc)
   * generate as needed for shaders, invalidate when original matrices change
   *
   * TODO: separate Model from View transform? Batches/objects have model,
   * camera/eye has view & projection
   */
};

#define ModelViewStack Context::get()->matrix_state->model_view_stack
#define ModelView ModelViewStack.stack[ModelViewStack.top]

#define ProjectionStack Context::get()->matrix_state->projection_stack
#define Projection ProjectionStack.stack[ProjectionStack.top]

GPUMatrixState *GPU_matrix_state_create()
{
#define MATRIX_4X4_IDENTITY \
  { \
    {1.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f, 0.0f}, \
        {0.0f, 0.0f, 0.0f, 1.0f} \
  }

  GPUMatrixState *state = (GPUMatrixState *)MEM_mallocN(sizeof(*state), __func__);
  const MatrixStack identity_stack = {{MATRIX_4X4_IDENTITY}, 0};

  state->model_view_stack = state->projection_stack = identity_stack;
  state->dirty = true;

#undef MATRIX_4X4_IDENTITY

  return state;
}

void GPU_matrix_state_discard(GPUMatrixState *state)
{
  MEM_freeN(state);
}

static void gpu_matrix_state_active_set_dirty(bool value)
{
  GPUMatrixState *state = Context::get()->matrix_state;
  state->dirty = value;
}

void GPU_matrix_reset()
{
  GPUMatrixState *state = Context::get()->matrix_state;
  state->model_view_stack.top = 0;
  state->projection_stack.top = 0;
  unit_m4(ModelView);
  unit_m4(Projection);
  gpu_matrix_state_active_set_dirty(true);
}

#ifdef WITH_GPU_SAFETY

/* Check if matrix is numerically good */
static void checkmat(cosnt float *m)
{
  const int n = 16;
  for (int i = 0; i < n; i++) {
#  if _MSC_VER
    BLI_assert(_finite(m[i]));
#  else
    BLI_assert(!isinf(m[i]));
#  endif
  }
}

#  define CHECKMAT(m) checkmat((const float *)m)

#else

#  define CHECKMAT(m)

#endif

void GPU_matrix_push()
{
  BLI_assert(ModelViewStack.top + 1 < MATRIX_STACK_DEPTH);
  ModelViewStack.top++;
  copy_m4_m4(ModelView, ModelViewStack.stack[ModelViewStack.top - 1]);
}

void GPU_matrix_pop()
{
  BLI_assert(ModelViewStack.top > 0);
  ModelViewStack.top--;
  gpu_matrix_state_active_set_dirty(true);
}

void GPU_matrix_push_projection()
{
  BLI_assert(ProjectionStack.top + 1 < MATRIX_STACK_DEPTH);
  ProjectionStack.top++;
  copy_m4_m4(Projection, ProjectionStack.stack[ProjectionStack.top - 1]);
}

void GPU_matrix_pop_projection()
{
  BLI_assert(ProjectionStack.top > 0);
  ProjectionStack.top--;
  gpu_matrix_state_active_set_dirty(true);
}

void GPU_matrix_set(const float m[4][4])
{
  copy_m4_m4(ModelView, m);
  CHECKMAT(ModelView3D);
  gpu_matrix_state_active_set_dirty(true);
}

void GPU_matrix_identity_projection_set()
{
  unit_m4(Projection);
  CHECKMAT(Projection3D);
  gpu_matrix_state_active_set_dirty(true);
}

void GPU_matrix_projection_set(const float m[4][4])
{
  copy_m4_m4(Projection, m);
  CHECKMAT(Projection3D);
  gpu_matrix_state_active_set_dirty(true);
}

void GPU_matrix_identity_set()
{
  unit_m4(ModelView);
  gpu_matrix_state_active_set_dirty(true);
}

void GPU_matrix_translate_2f(float x, float y)
{
  Mat4 m;
  unit_m4(m);
  m[3][0] = x;
  m[3][1] = y;
  GPU_matrix_mul(m);
}

void GPU_matrix_translate_2fv(const float vec[2])
{
  GPU_matrix_translate_2f(vec[0], vec[1]);
}

void GPU_matrix_translate_3f(float x, float y, float z)
{
#if 1
  translate_m4(ModelView, x, y, z);
  CHECKMAT(ModelView);
#else /* above works well in early testing, below is generic version */
  Mat4 m;
  unit_m4(m);
  m[3][0] = x;
  m[3][1] = y;
  m[3][2] = z;
  GPU_matrix_mul(m);
#endif
  gpu_matrix_state_active_set_dirty(true);
}

void GPU_matrix_translate_3fv(const float vec[3])
{
  GPU_matrix_translate_3f(vec[0], vec[1], vec[2]);
}

void GPU_matrix_scale_1f(float factor)
{
  Mat4 m;
  scale_m4_fl(m, factor);
  GPU_matrix_mul(m);
}

void GPU_matrix_scale_2f(float x, float y)
{
  Mat4 m = {{0.0f}};
  m[0][0] = x;
  m[1][1] = y;
  m[2][2] = 1.0f;
  m[3][3] = 1.0f;
  GPU_matrix_mul(m);
}

void GPU_matrix_scale_2fv(const float vec[2])
{
  GPU_matrix_scale_2f(vec[0], vec[1]);
}

void GPU_matrix_scale_3f(float x, float y, float z)
{
  Mat4 m = {{0.0f}};
  m[0][0] = x;
  m[1][1] = y;
  m[2][2] = z;
  m[3][3] = 1.0f;
  GPU_matrix_mul(m);
}

void GPU_matrix_scale_3fv(const float vec[3])
{
  GPU_matrix_scale_3f(vec[0], vec[1], vec[2]);
}

void GPU_matrix_mul(const float m[4][4])
{
  mul_m4_m4_post(ModelView, m);
  CHECKMAT(ModelView);
  gpu_matrix_state_active_set_dirty(true);
}

void GPU_matrix_rotate_2d(float deg)
{
  /* essentially RotateAxis('Z')
   * TODO: simpler math for 2D case
   */
  rotate_m4(ModelView, 'Z', DEG2RADF(deg));
}

void GPU_matrix_rotate_3f(float deg, float x, float y, float z)
{
  const float axis[3] = {x, y, z};
  GPU_matrix_rotate_3fv(deg, axis);
}

void GPU_matrix_rotate_3fv(float deg, const float axis[3])
{
  Mat4 m;
  axis_angle_to_mat4(m, axis, DEG2RADF(deg));
  GPU_matrix_mul(m);
}

void GPU_matrix_rotate_axis(float deg, char axis)
{
  /* rotate_m4 works in place */
  rotate_m4(ModelView, axis, DEG2RADF(deg));
  CHECKMAT(ModelView);
  gpu_matrix_state_active_set_dirty(true);
}

static void mat4_ortho_set(
    float m[4][4], float left, float right, float bottom, float top, float near, float far)
{
  m[0][0] = 2.0f / (right - left);
  m[1][0] = 0.0f;
  m[2][0] = 0.0f;
  m[3][0] = -(right + left) / (right - left);

  m[0][1] = 0.0f;
  m[1][1] = 2.0f / (top - bottom);
  m[2][1] = 0.0f;
  m[3][1] = -(top + bottom) / (top - bottom);

  m[0][2] = 0.0f;
  m[1][2] = 0.0f;
  m[2][2] = -2.0f / (far - near);
  m[3][2] = -(far + near) / (far - near);

  m[0][3] = 0.0f;
  m[1][3] = 0.0f;
  m[2][3] = 0.0f;
  m[3][3] = 1.0f;

  gpu_matrix_state_active_set_dirty(true);
}

static void mat4_frustum_set(
    float m[4][4], float left, float right, float bottom, float top, float near, float far)
{
  m[0][0] = 2.0f * near / (right - left);
  m[1][0] = 0.0f;
  m[2][0] = (right + left) / (right - left);
  m[3][0] = 0.0f;

  m[0][1] = 0.0f;
  m[1][1] = 2.0f * near / (top - bottom);
  m[2][1] = (top + bottom) / (top - bottom);
  m[3][1] = 0.0f;

  m[0][2] = 0.0f;
  m[1][2] = 0.0f;
  m[2][2] = -(far + near) / (far - near);
  m[3][2] = -2.0f * far * near / (far - near);

  m[0][3] = 0.0f;
  m[1][3] = 0.0f;
  m[2][3] = -1.0f;
  m[3][3] = 0.0f;

  gpu_matrix_state_active_set_dirty(true);
}

static void mat4_look_from_origin(float m[4][4], float lookdir[3], float camup[3])
{
  /* This function is loosely based on Mesa implementation.
   *
   * SGI FREE SOFTWARE LICENSE B (Version 2.0, Sept. 18, 2008)
   * Copyright (C) 1991-2000 Silicon Graphics, Inc. All Rights Reserved.
   *
   * Permission is hereby granted, free of charge, to any person obtaining a
   * copy of this software and associated documentation files (the "Software"),
   * to deal in the Software without restriction, including without limitation
   * the rights to use, copy, modify, merge, publish, distribute, sublicense,
   * and/or sell copies of the Software, and to permit persons to whom the
   * Software is furnished to do so, subject to the following conditions:
   *
   * The above copyright notice including the dates of first publication and
   * either this permission notice or a reference to
   * http://oss.sgi.com/projects/FreeB/
   * shall be included in all copies or substantial portions of the Software.
   *
   * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
   * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
   * SILICON GRAPHICS, INC. BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
   * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
   * OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
   * SOFTWARE.
   *
   * Except as contained in this notice, the name of Silicon Graphics, Inc.
   * shall not be used in advertising or otherwise to promote the sale, use or
   * other dealings in this Software without prior written authorization from
   * Silicon Graphics, Inc.
   */
  float side[3];

  normalize_v3(lookdir);

  cross_v3_v3v3(side, lookdir, camup);

  normalize_v3(side);

  cross_v3_v3v3(camup, side, lookdir);

  m[0][0] = side[0];
  m[1][0] = side[1];
  m[2][0] = side[2];
  m[3][0] = 0.0f;

  m[0][1] = camup[0];
  m[1][1] = camup[1];
  m[2][1] = camup[2];
  m[3][1] = 0.0f;

  m[0][2] = -lookdir[0];
  m[1][2] = -lookdir[1];
  m[2][2] = -lookdir[2];
  m[3][2] = 0.0f;

  m[0][3] = 0.0f;
  m[1][3] = 0.0f;
  m[2][3] = 0.0f;
  m[3][3] = 1.0f;

  gpu_matrix_state_active_set_dirty(true);
}

void GPU_matrix_ortho_set(float left, float right, float bottom, float top, float near, float far)
{
  mat4_ortho_set(Projection, left, right, bottom, top, near, far);
  CHECKMAT(Projection);
  gpu_matrix_state_active_set_dirty(true);
}

void GPU_matrix_ortho_set_z(float near, float far)
{
  CHECKMAT(Projection);
  Projection[2][2] = -2.0f / (far - near);
  Projection[3][2] = -(far + near) / (far - near);
  gpu_matrix_state_active_set_dirty(true);
}

void GPU_matrix_ortho_2d_set(float left, float right, float bottom, float top)
{
  Mat4 m;
  mat4_ortho_set(m, left, right, bottom, top, -1.0f, 1.0f);
  CHECKMAT(Projection2D);
  gpu_matrix_state_active_set_dirty(true);
}

void GPU_matrix_frustum_set(
    float left, float right, float bottom, float top, float near, float far)
{
  mat4_frustum_set(Projection, left, right, bottom, top, near, far);
  CHECKMAT(Projection);
  gpu_matrix_state_active_set_dirty(true);
}

void GPU_matrix_perspective_set(float fovy, float aspect, float near, float far)
{
  float half_height = tanf(fovy * float(M_PI / 360.0)) * near;
  float half_width = half_height * aspect;
  GPU_matrix_frustum_set(-half_width, +half_width, -half_height, +half_height, near, far);
}

void GPU_matrix_look_at(float eyeX,
                        float eyeY,
                        float eyeZ,
                        float centerX,
                        float centerY,
                        float centerZ,
                        float upX,
                        float upY,
                        float upZ)
{
  Mat4 cm;
  float lookdir[3];
  float camup[3] = {upX, upY, upZ};

  lookdir[0] = centerX - eyeX;
  lookdir[1] = centerY - eyeY;
  lookdir[2] = centerZ - eyeZ;

  mat4_look_from_origin(cm, lookdir, camup);

  GPU_matrix_mul(cm);
  GPU_matrix_translate_3f(-eyeX, -eyeY, -eyeZ);
}

void GPU_matrix_project_3fv(const float world[3],
                            const float model[4][4],
                            const float proj[4][4],
                            const int view[4],
                            float r_win[3])
{
  float v[4];

  mul_v4_m4v3(v, model, world);
  mul_m4_v4(proj, v);

  if (v[3] != 0.0f) {
    mul_v3_fl(v, 1.0f / v[3]);
  }

  r_win[0] = view[0] + (view[2] * (v[0] + 1)) * 0.5f;
  r_win[1] = view[1] + (view[3] * (v[1] + 1)) * 0.5f;
  r_win[2] = (v[2] + 1) * 0.5f;
}

void GPU_matrix_project_2fv(const float world[3],
                            const float model[4][4],
                            const float proj[4][4],
                            const int view[4],
                            float r_win[2])
{
  float v[4];

  mul_v4_m4v3(v, model, world);
  mul_m4_v4(proj, v);

  if (v[3] != 0.0f) {
    mul_v2_fl(v, 1.0f / v[3]);
  }

  r_win[0] = view[0] + (view[2] * (v[0] + 1)) * 0.5f;
  r_win[1] = view[1] + (view[3] * (v[1] + 1)) * 0.5f;
}

bool GPU_matrix_unproject_3fv(const float win[3],
                              const float model_inverted[4][4],
                              const float proj[4][4],
                              const int view[4],
                              float r_world[3])
{
  zero_v3(r_world);
  const float in[3] = {
      2 * ((win[0] - view[0]) / view[2]) - 1.0f,
      2 * ((win[1] - view[1]) / view[3]) - 1.0f,
      2 * win[2] - 1.0f,
  };

  /**
   * The same result could be obtained as follows:
   *
   * \code{.c}
   * float projinv[4][4];
   * invert_m4_m4(projinv, projview);
   * copy_v3_v3(r_world, in);
   * mul_project_m4_v3(projinv, r_world);
   * \endcode
   *
   * But that solution loses much precision.
   * Therefore, get the same result without inverting the project view matrix.
   */

  float out[3];
  const bool is_persp = proj[3][3] == 0.0f;
  if (is_persp) {
    out[2] = proj[3][2] / (proj[2][2] + in[2]);
    if (isinf(out[2])) {
      out[2] = FLT_MAX;
    }
    out[0] = out[2] * ((proj[2][0] + in[0]) / proj[0][0]);
    out[1] = out[2] * ((proj[2][1] + in[1]) / proj[1][1]);
    out[2] *= -1;
  }
  else {
    out[0] = (-proj[3][0] + in[0]) / proj[0][0];
    out[1] = (-proj[3][1] + in[1]) / proj[1][1];
    out[2] = (-proj[3][2] + in[2]) / proj[2][2];
  }

  if (!is_finite_v3(out)) {
    return false;
  }

  mul_v3_m4v3(r_world, model_inverted, out);
  return true;
}

const float (*GPU_matrix_model_view_get(float m[4][4]))[4]
{
  if (m) {
    copy_m4_m4(m, ModelView);
    return m;
  }

  return ModelView;
}

const float (*GPU_matrix_projection_get(float m[4][4]))[4]
{
  if (m) {
    copy_m4_m4(m, Projection);
    return m;
  }

  return Projection;
}

const float (*GPU_matrix_model_view_projection_get(float m[4][4]))[4]
{
  if (m == nullptr) {
    static Mat4 temp;
    m = temp;
  }

  mul_m4_m4m4(m, Projection, ModelView);
  return m;
}

const float (*GPU_matrix_normal_get(float m[3][3]))[3]
{
  if (m == nullptr) {
    static Mat3 temp3;
    m = temp3;
  }

  copy_m3_m4(m, GPU_matrix_model_view_get(nullptr));

  invert_m3(m);
  transpose_m3(m);

  return m;
}

const float (*GPU_matrix_normal_inverse_get(float m[3][3]))[3]
{
  if (m == nullptr) {
    static Mat3 temp3;
    m = temp3;
  }

  GPU_matrix_normal_get(m);
  invert_m3(m);

  return m;
}

void GPU_matrix_bind(blender::gpu::Shader *shader)
{
  /* set uniform values to matrix stack values
   * call this before a draw call if desired matrices are dirty
   * call glUseProgram before this, as glUniform expects program to be bound
   */
  int32_t MV = GPU_shader_get_builtin_uniform(shader, GPU_UNIFORM_MODELVIEW);
  int32_t P = GPU_shader_get_builtin_uniform(shader, GPU_UNIFORM_PROJECTION);
  int32_t MVP = GPU_shader_get_builtin_uniform(shader, GPU_UNIFORM_MVP);

  int32_t N = GPU_shader_get_builtin_uniform(shader, GPU_UNIFORM_NORMAL);
  int32_t MV_inv = GPU_shader_get_builtin_uniform(shader, GPU_UNIFORM_MODELVIEW_INV);
  int32_t P_inv = GPU_shader_get_builtin_uniform(shader, GPU_UNIFORM_PROJECTION_INV);

  if (MV != -1) {
    GPU_shader_uniform_float_ex(
        shader, MV, 16, 1, (const float *)GPU_matrix_model_view_get(nullptr));
  }
  if (P != -1) {
    GPU_shader_uniform_float_ex(
        shader, P, 16, 1, (const float *)GPU_matrix_projection_get(nullptr));
  }
  if (MVP != -1) {
    GPU_shader_uniform_float_ex(
        shader, MVP, 16, 1, (const float *)GPU_matrix_model_view_projection_get(nullptr));
  }
  if (N != -1) {
    GPU_shader_uniform_float_ex(shader, N, 9, 1, (const float *)GPU_matrix_normal_get(nullptr));
  }
  if (MV_inv != -1) {
    Mat4 m;
    GPU_matrix_model_view_get(m);
    invert_m4(m);
    GPU_shader_uniform_float_ex(shader, MV_inv, 16, 1, (const float *)m);
  }
  if (P_inv != -1) {
    Mat4 m;
    GPU_matrix_projection_get(m);
    invert_m4(m);
    GPU_shader_uniform_float_ex(shader, P_inv, 16, 1, (const float *)m);
  }

  gpu_matrix_state_active_set_dirty(false);
}

bool GPU_matrix_dirty_get()
{
  GPUMatrixState *state = Context::get()->matrix_state;
  return state->dirty;
}

/* -------------------------------------------------------------------- */
/** \name Python API Helpers
 * \{ */

BLI_STATIC_ASSERT(GPU_PY_MATRIX_STACK_LEN + 1 == MATRIX_STACK_DEPTH, "define mismatch");

/* Return int since caller is may subtract. */

int GPU_matrix_stack_level_get_model_view()
{
  GPUMatrixState *state = Context::get()->matrix_state;
  return int(state->model_view_stack.top);
}

int GPU_matrix_stack_level_get_projection()
{
  GPUMatrixState *state = Context::get()->matrix_state;
  return int(state->projection_stack.top);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Polygon Offset Hack
 *
 * Workaround the fact that polygon-offset is implementation dependent.
 * We modify the projection matrix \a winmat in order to change the final depth a tiny amount.
 * \{ */

float GPU_polygon_offset_calc(const float (*winmat)[4], float viewdist, float dist)
{
  /* Seems like we have a factor of 2 more offset than 2.79 for some reason. Correct for this. */
  dist *= 0.5f;

  if (winmat[3][3] > 0.5f) {
#if 1
    return 0.00001f * dist * viewdist;  // ortho tweaking
#else
    static float depth_fac = 0.0f;
    if (depth_fac == 0.0f) {
      /* Hard-code for 24 bit precision. */
      int depthbits = 24;
      depth_fac = 1.0f / float((1 << depthbits) - 1);
    }
    ofs = (-1.0 / winmat[2][2]) * dist * depth_fac;

    UNUSED_VARS(viewdist);
#endif
  }

  /* This adjustment effectively results in reducing the Z value by 0.25%.
   *
   * winmat[4][3] actually evaluates to `-2 * far * near / (far - near)`,
   * is very close to -0.2 with default clip range,
   * and is used as the coefficient multiplied by `w / z`,
   * thus controlling the z dependent part of the depth value.
   */
  return winmat[3][2] * -0.0025f * dist;
}

void GPU_polygon_offset(float viewdist, float dist)
{
  static float winmat[4][4], offset = 0.0f;

  if (dist != 0.0f) {
    /* hack below is to mimic polygon offset */
    GPU_matrix_projection_get(winmat);

    /* dist is from camera to center point */

    float ofs = GPU_polygon_offset_calc(winmat, viewdist, dist);

    winmat[3][2] -= ofs;
    offset += ofs;
  }
  else {
    winmat[3][2] += offset;
    offset = 0.0;
  }

  GPU_matrix_projection_set(winmat);
}

/** \} */
