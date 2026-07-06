/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_math_matrix_construct_lib.glsl"
#include "gpu_shader_math_vector_reduce_lib.glsl"
#include "gpu_shader_utildefines_lib.glsl"

/* Apply left Jacobi rotation on 3x3 matrix : matrix = rotation * matrix.
 * Rotates rows p and q. */
float3x3 jacobi_rotate_left_3x3(float3x3 matrix, int p, int q, float2x2 rotation)
{
  float3x3 result = matrix;
  const float3 row_p = float3(matrix[0][p], matrix[1][p], matrix[2][p]);
  const float3 row_q = float3(matrix[0][q], matrix[1][q], matrix[2][q]);

  const float3 new_row_p = rotation[0][0] * row_p + rotation[1][0] * row_q;
  const float3 new_row_q = rotation[0][1] * row_p + rotation[1][1] * row_q;

  result[0][p] = new_row_p[0];
  result[1][p] = new_row_p[1];
  result[2][p] = new_row_p[2];
  result[0][q] = new_row_q[0];
  result[1][q] = new_row_q[1];
  result[2][q] = new_row_q[2];
  return result;
}

/* Apply right Jacobi rotation on 3x3 matrix : matrix = matrix * rotation.
 * Rotates columns p and q. */
float3x3 jacobi_rotate_right_3x3(float3x3 matrix, int p, int q, float2x2 rotation)
{
  float3x3 result = matrix;
  const float3 col_p = matrix[p];
  const float3 col_q = matrix[q];

  result[p] = rotation[0][0] * col_p + rotation[0][1] * col_q;
  result[q] = rotation[1][0] * col_p + rotation[1][1] * col_q;
  return result;
}

/* Compute Jacobi rotation for symmetric 2x2 matrix [[x, y], [y, z]]. */
float2x2 construct_jacobi_rotation(float x, float y, float z)
{
  /* Check if off-diagonal is negligible; no rotation needed. */
  const float off_diagonal = 2.0f * abs(y);
  if (off_diagonal < FLT_MIN) {
    return float2x2(1.0f);
  }

  const float tau = (x - z) / off_diagonal;
  const float w = sqrt(tau * tau + 1.0f);
  const float t = 1.0f / (tau > 0.0f ? (tau + w) : (tau - w));

  const float sign_t = t > 0.0f ? 1.0f : -1.0f;
  const float c = 1.0f / sqrt(t * t + 1.0f);
  const float s = -sign_t * sign(y) * abs(t) * c;
  return float2x2(c, -s, s, c);
}

/* Compute left and right Jacobi rotations for the 2x2 block at indices (p, q)
 * (i.e. block obtained from intersecting lines p & q with columns p & q). */
void jacobi_svd_2x2(float3x3 matrix, int p, int q, out float2x2 j_left, out float2x2 j_right)
{
  float2x2 block;
  block[0] = float2(matrix[p][p], matrix[p][q]);
  block[1] = float2(matrix[q][p], matrix[q][q]);

  const float t = block[0][0] + block[1][1];
  const float d = block[0][1] - block[1][0];
  float2x2 rotation;
  if (abs(d) < FLT_MIN) {
    rotation = float2x2(1.0f);
  }
  else {
    const float u = t / d;
    const float tmp = sqrt(1.0f + u * u);
    rotation = float2x2(u / tmp, -(1.0f / tmp), 1.0f / tmp, u / tmp);
  }

  block = rotation * block;
  j_right = construct_jacobi_rotation(block[0][0], block[0][1], block[1][1]);
  j_left = rotation * transpose(j_right);
}

void jacobi_svd_3x3(float3x3 A, out float3x3 U, out float3 S, out float3x3 V)
{
  constexpr float convergence_epsilon = 2.0f * FLT_EPSILON;
  const float zero_threshold = FLT_MIN;
  /* Cap on Jacobi sweeps: most inputs converge in ~11 sweeps, but 80 is the
   * smallest cap at which abs max error reaches ~6.6e-6. */
  constexpr int max_sweeps = 80;

  /* Normalize matrix for numerical stability. */
  float scale = reduce_max(
      float3(reduce_max(abs(A[0])), reduce_max(abs(A[1])), reduce_max(abs(A[2]))));
  if (scale == 0.0f) {
    scale = 1.0f;
  }

  A = A * (1.0f / scale);
  U = mat3x3_identity();
  V = mat3x3_identity();
  float max_diag_entry = max(max(abs(A[0][0]), abs(A[1][1])), abs(A[2][2]));

  /* The main Jacobi SVD iteration. Sweep until converged or max_sweeps is reached. */
  for (int i = 0; i < max_sweeps; i++) {
    bool finished = true;

    for (int p = 1; p < 3; p++) {
      for (int q = 0; q < p; q++) {
        const float threshold = max(zero_threshold, convergence_epsilon * max_diag_entry);

        /* Skip pairs already converged. */
        if (abs(A[q][p]) > threshold || abs(A[p][q]) > threshold) {
          finished = false;

          /* SVD of the 2x2 block at indices (p, q). */
          float2x2 j_left;
          float2x2 j_right;
          jacobi_svd_2x2(A, p, q, j_left, j_right);

          /* Accumulate rotations. */
          A = jacobi_rotate_left_3x3(A, p, q, j_left);
          U = jacobi_rotate_right_3x3(U, p, q, transpose(j_left));

          A = jacobi_rotate_right_3x3(A, p, q, j_right);
          V = jacobi_rotate_right_3x3(V, p, q, j_right);

          max_diag_entry = max(max(abs(A[0][0]), abs(A[1][1])), abs(A[2][2]));
        }
      }
    }

    if (finished) {
      break;
    }
  }

  /* A is now diagonal. Extract singular values, force S >= 0, update sign into U. */
  S = float3(0.0f);
  for (int i = 0; i < 3; i++) {
    const float a = A[i][i];
    S[i] = abs(a);
    if (a < 0.0f) {
      U[i] = -U[i];
    }
  }

  /* Revert to original scale. */
  S = S * scale;

  /* Sort singular values descending & update U,V columns accordingly. */
  for (int i = 0; i < 3; i++) {
    float max_val = 0.0f;
    int pos = i;
    for (int j = i; j < 3; j++) {
      const float val = S[j];
      if (val > max_val) {
        max_val = val;
        pos = j;
      }
    }
    if (max_val == 0.0f) {
      break;
    }
    if (pos != i) {
      const float tmp_s = S[i];
      S[i] = S[pos];
      S[pos] = tmp_s;

      const float3 tmp_u = U[i];
      U[i] = U[pos];
      U[pos] = tmp_u;

      const float3 tmp_v = V[i];
      V[i] = V[pos];
      V[pos] = tmp_v;
    }
  }
}

[[node]]
void matrix_svd(float4x4 matrix, out float4x4 U, out float3 S, out float4x4 V)
{
  const float3x3 A = to_float3x3(matrix);
  float3x3 U3;
  float3x3 V3;
  jacobi_svd_3x3(A, U3, S, V3);
  U = to_float4x4(U3);
  V = to_float4x4(V3);
}
