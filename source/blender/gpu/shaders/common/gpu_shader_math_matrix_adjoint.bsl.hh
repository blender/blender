/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_shader_compat.hh"

/* -------------------------------------------------------------------- */
/** \name Normalize
 * \{ */

/**
 * Returns the adjoint of the matrix (also known as adjugate matrix).
 */
float2x2 adjoint(float2x2 mat)
{
  float2x2 adj = float2x2(0.0f);
  for (int c = 0; c < 2; c++) {
    for (int r = 0; r < 2; r++) {
      /* Copy other cells except the "cross" to compute the determinant. */
      float tmp = 0.0f;
      for (int m_c = 0; m_c < 2; m_c++) {
        for (int m_r = 0; m_r < 2; m_r++) {
          if (m_c != c && m_r != r) {
            tmp = mat[m_c][m_r];
          }
        }
      }
      float minor = tmp;
      /* Transpose directly to get the adjugate. Swap destination row and col. */
      adj[r][c] = (((c + r) & 1) != 0) ? -minor : minor;
    }
  }
  return adj;
}

/**
 * Returns the adjoint of the matrix (also known as adjugate matrix).
 */
float3x3 adjoint(float3x3 mat)
{
  float3x3 adj = float3x3(0.0f);
  for (int c = 0; c < 3; c++) {
    for (int r = 0; r < 3; r++) {
      /* Copy other cells except the "cross" to compute the determinant. */
      float2x2 tmp = float2x2(0.0f);
      for (int m_c = 0; m_c < 3; m_c++) {
        for (int m_r = 0; m_r < 3; m_r++) {
          if (m_c != c && m_r != r) {
            int d_c = (m_c < c) ? m_c : (m_c - 1);
            int d_r = (m_r < r) ? m_r : (m_r - 1);
            tmp[d_c][d_r] = mat[m_c][m_r];
          }
        }
      }
      float minor = determinant(tmp);
      /* Transpose directly to get the adjugate. Swap destination row and col. */
      adj[r][c] = (((c + r) & 1) != 0) ? -minor : minor;
    }
  }
  return adj;
}

/**
 * Returns the adjoint of the matrix (also known as adjugate matrix).
 */
float4x4 adjoint(float4x4 mat)
{
  float4x4 adj = float4x4(0.0f);
  for (int c = 0; c < 4; c++) {
    for (int r = 0; r < 4; r++) {
      /* Copy other cells except the "cross" to compute the determinant. */
      float3x3 tmp = float3x3(0.0f);
      for (int m_c = 0; m_c < 4; m_c++) {
        for (int m_r = 0; m_r < 4; m_r++) {
          if (m_c != c && m_r != r) {
            int d_c = (m_c < c) ? m_c : (m_c - 1);
            int d_r = (m_r < r) ? m_r : (m_r - 1);
            tmp[d_c][d_r] = mat[m_c][m_r];
          }
        }
      }
      float minor = determinant(tmp);
      /* Transpose directly to get the adjugate. Swap destination row and col. */
      adj[r][c] = (((c + r) & 1) != 0) ? -minor : minor;
    }
  }
  return adj;
}

/** \} */
