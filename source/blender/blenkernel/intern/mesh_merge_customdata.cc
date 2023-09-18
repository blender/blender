/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include "MEM_guardedalloc.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BLI_math_vector_types.hh"
#include "BLI_task.hh"
#include "BLI_utildefines.h"

#include "BKE_customdata.h"
#include "BKE_mesh.hh"
#include "BKE_mesh_mapping.hh"
#include "BLI_memarena.h"

#include "BLI_strict_flags.h"

using namespace blender;

enum {
  CMP_CLOSE = 0,
  CMP_EQUAL = 1,
  CMP_APART = 2,
};

static int compare_v2_classify(const float uv_a[2], const float uv_b[2])
{
  if (uv_a[0] == uv_b[0] && uv_a[1] == uv_b[1]) {
    return CMP_EQUAL;
  }
  /* NOTE(@ideasman42): that the ULP value is the primary value used to compare relative
   * values as the absolute value doesn't account for float precision at difference scales.
   * - For subdivision-surface ULP of 3 is sufficient,
   *   although this value is extremely small.
   * - For bevel the ULP of 12 is sufficient to merge UVs that appear to be connected
   *   with bevel on Suzanne beveled 15% with 6 segments.
   *
   * These values could be tweaked but should be kept on the small side to prevent
   * unintentional joining of intentionally disconnected UVs.
   *
   * Before v2.91 the threshold was either (`1e-4` or `0.05 / image_size` for selection picking).
   * So picking used a threshold of `1e-4` for a 500x500 image and `1e-5` for a 5000x5000 image.
   * Given this value worked reasonably well for a long time, the absolute difference should
   * never exceed `1e-4` (#STD_UV_CONNECT_LIMIT which is still used in a few areas). */
  const float diff_abs = 1e-12f;
  const int diff_ulp = 12;

  if (compare_ff_relative(uv_a[0], uv_b[0], diff_abs, diff_ulp) &&
      compare_ff_relative(uv_a[1], uv_b[1], diff_abs, diff_ulp))
  {
    return CMP_CLOSE;
  }
  return CMP_APART;
}

static void merge_uvs_for_vertex(const Span<int> loops_for_vert, Span<float2 *> mloopuv_layers)
{
  if (loops_for_vert.size() <= 1) {
    return;
  }
  /* Manipulate a copy of the loop indices, de-duplicating UVs per layer. */
  Vector<int, 32> loops_merge;
  loops_merge.reserve(loops_for_vert.size());
  for (float2 *mloopuv : mloopuv_layers) {
    BLI_assert(loops_merge.is_empty());
    loops_merge.extend_unchecked(loops_for_vert);
    while (loops_merge.size() > 1) {
      uint i_last = uint(loops_merge.size()) - 1;
      const float *uv_src = mloopuv[loops_merge[0]];
      for (uint i = 1; i <= i_last;) {
        float *uv_dst = mloopuv[loops_merge[i]];
        switch (compare_v2_classify(uv_src, uv_dst)) {
          case CMP_CLOSE: {
            uv_dst[0] = uv_src[0];
            uv_dst[1] = uv_src[1];
            ATTR_FALLTHROUGH;
          }
          case CMP_EQUAL: {
            loops_merge[i] = loops_merge[i_last--];
            break;
          }
          case CMP_APART: {
            /* Doesn't match, check the next UV. */
            i++;
            break;
          }
          default: {
            BLI_assert_unreachable();
          }
        }
      }
      /* Finished de-duplicating with the first index, throw it away. */
      loops_merge[0] = loops_merge[i_last];
      loops_merge.resize(i_last);
    }
    loops_merge.clear();
  }
}

void BKE_mesh_merge_customdata_for_apply_modifier(Mesh *me)
{
  if (me->totloop == 0) {
    return;
  }
  const int mloopuv_layers_num = CustomData_number_of_layers(&me->loop_data, CD_PROP_FLOAT2);
  if (mloopuv_layers_num == 0) {
    return;
  }

  const GroupedSpan<int> vert_to_loop = me->vert_to_corner_map();

  Vector<float2 *> mloopuv_layers;
  mloopuv_layers.reserve(mloopuv_layers_num);
  for (int a = 0; a < mloopuv_layers_num; a++) {
    float2 *mloopuv = static_cast<float2 *>(
        CustomData_get_layer_n_for_write(&me->loop_data, CD_PROP_FLOAT2, a, me->totloop));
    mloopuv_layers.append_unchecked(mloopuv);
  }

  Span<float2 *> mloopuv_layers_as_span = mloopuv_layers.as_span();

  threading::parallel_for(IndexRange(me->totvert), 1024, [&](IndexRange range) {
    for (const int64_t v_index : range) {
      merge_uvs_for_vertex(vert_to_loop[v_index], mloopuv_layers_as_span);
    }
  });
}
