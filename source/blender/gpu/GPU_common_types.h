/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef enum eGPULoadOp {
  GPU_LOADACTION_CLEAR = 0,
  GPU_LOADACTION_LOAD,
  GPU_LOADACTION_DONT_CARE
} eGPULoadOp;

typedef enum eGPUStoreOp { GPU_STOREACTION_STORE = 0, GPU_STOREACTION_DONT_CARE } eGPUStoreOp;

typedef enum eGPUFrontFace {
  GPU_CLOCKWISE,
  GPU_COUNTERCLOCKWISE,
} eGPUFrontFace;

#ifdef __cplusplus
}
#endif
