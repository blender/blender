/* SPDX-FileCopyrightText: 2016 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#include "DRW_pbvh.hh"

#include "draw_attributes.hh"
#include "draw_manager_c.hh"

#include "BKE_attribute.hh"
#include "BKE_curve.hh"
#include "BKE_customdata.hh"
#include "BKE_duplilist.hh"
#include "BKE_global.hh"
#include "BKE_image.hh"
#include "BKE_mesh.hh"
#include "BKE_object.hh"
#include "BKE_paint.hh"
#include "BKE_volume.hh"

/* For debug cursor position. */
#include "WM_api.hh"
#include "wm_window.hh"

#include "DNA_curve_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meta_types.h"
#include "DNA_screen_types.h"

#include "BLI_array.hh"
#include "BLI_hash.h"
#include "BLI_link_utils.h"
#include "BLI_listbase.h"
#include "BLI_math_bits.h"
#include "BLI_memblock.h"
#include "BLI_mempool.h"

#ifdef DRW_DEBUG_CULLING
#  include "BLI_math_bits.h"
#endif

#include "GPU_capabilities.hh"
#include "GPU_material.hh"
#include "GPU_uniform_buffer.hh"

#include "intern/gpu_codegen.hh"

#include "draw_view.hh"

/* -------------------------------------------------------------------- */
/** \name Draw Call (DRW_calls)
 * \{ */

eDRWCommandType command_type_get(const uint64_t *command_type_bits, int index)
{
  return eDRWCommandType((command_type_bits[index / 16] >> ((index % 16) * 4)) & 0xF);
}
