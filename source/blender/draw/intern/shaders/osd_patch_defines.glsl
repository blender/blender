/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * File that contains dynamic defines based on the actual used GPU backend.
 *
 * NOTE: file must be included before `osd_patch_basis.glsl`.
 */

#pragma once

#include "gpu_shader_compat.hh"

/* Ensure the basis code has access to proper backend specification define: it is not guaranteed
 * that the code provided by OpenSubdiv specifies it. For example, it doesn't for GLSL but it
 * does for Metal. Additionally, for Metal OpenSubdiv defines OSD_PATCH_BASIS_METAL as 1, so do
 * the same here to avoid possible warning about value being re-defined. */
#ifdef GPU_METAL
#  define OSD_PATCH_BASIS_METAL 1
#else
#  define OSD_PATCH_BASIS_GLSL
#endif
