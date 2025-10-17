/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#pragma once

#include "BLI_enum_flags.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_vector.hh"

struct GPUMaterial;
struct Object;
namespace blender::gpu {
class Batch;
}

namespace blender::draw {

#define SCULPT_DEBUG_DRAW (G.debug_value == 889)

struct SculptBatch {
  gpu::Batch *batch;
  int material_slot;
  int debug_index;
  float3 debug_color();
};

enum SculptBatchFeature {
  SCULPT_BATCH_DEFAULT = 0,
  SCULPT_BATCH_WIREFRAME = 1 << 0,
  SCULPT_BATCH_MASK = 1 << 1,
  SCULPT_BATCH_FACE_SET = 1 << 2,
  SCULPT_BATCH_VERTEX_COLOR = 1 << 3,
  SCULPT_BATCH_UV = 1 << 4
};
ENUM_OPERATORS(SculptBatchFeature);

/** Used by engines that don't use GPUMaterials, like the Workbench and Overlay engines. */
Vector<SculptBatch> sculpt_batches_get(const Object *ob, SculptBatchFeature features);

/** Used by EEVEE. */
Vector<SculptBatch> sculpt_batches_per_material_get(const Object *ob,
                                                    Span<const GPUMaterial *> materials);

}  // namespace blender::draw
