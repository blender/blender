/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 */

#pragma once

#include "BKE_global.hh"

#include "BLI_math_matrix.hh"

#include "DRW_gpu_wrapper.hh"
#include "DRW_render.hh"

#include "UI_resources.hh"

#include "draw_handle.hh"

#include "overlay_shader_shared.h"

/* Needed for eSpaceImage_UVDT_Stretch and eMaskOverlayMode */
#include "DNA_mask_types.h"
#include "DNA_space_types.h"
/* Forward declarations */

enum OVERLAY_UVLineStyle {
  OVERLAY_UV_LINE_STYLE_OUTLINE = 0,
  OVERLAY_UV_LINE_STYLE_DASH = 1,
  OVERLAY_UV_LINE_STYLE_BLACK = 2,
  OVERLAY_UV_LINE_STYLE_WHITE = 3,
  OVERLAY_UV_LINE_STYLE_SHADOW = 4,
};

struct OVERLAY_Data {
  void *engine_type;
  DRWViewportEmptyList *fbl;
  DRWViewportEmptyList *txl;
  DRWViewportEmptyList *psl;
  DRWViewportEmptyList *stl;

  void *instance;
};
