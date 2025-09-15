/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

/** NOTE: Keep in sync with eUserPref_GPUBackendType. */
enum GPUBackendType {
  GPU_BACKEND_NONE = 0,
  GPU_BACKEND_OPENGL = 1 << 0,
  GPU_BACKEND_METAL = 1 << 1,
  GPU_BACKEND_VULKAN = 1 << 3,
  GPU_BACKEND_ANY = 0xFFFFFFFFu
};
