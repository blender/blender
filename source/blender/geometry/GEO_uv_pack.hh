/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_math_matrix.hh"
#include "BLI_span.hh"

#include "DNA_space_types.h"
#include "DNA_vec_types.h"

#pragma once

/** \file
 * \ingroup geo
 */

struct UnwrapOptions;

enum eUVPackIsland_MarginMethod {
  ED_UVPACK_MARGIN_SCALED = 0, /* Use scale of existing UVs to multiply margin. */
  ED_UVPACK_MARGIN_ADD,        /* Just add the margin, ignoring any UV scale. */
  ED_UVPACK_MARGIN_FRACTION,   /* Specify a precise fraction of final UV output. */
};

namespace blender::geometry {

/** See also #UnwrapOptions. */
class UVPackIsland_Params {
 public:
  /** Reasonable defaults. */
  UVPackIsland_Params();

  void setFromUnwrapOptions(const UnwrapOptions &options);
  void setUDIMOffsetFromSpaceImage(const SpaceImage *sima);

  /** Islands can be rotated to improve packing. */
  bool rotate;
  /** (In UV Editor) only pack islands which have one or more selected UVs. */
  bool only_selected_uvs;
  /** (In 3D Viewport or UV Editor) only pack islands which have selected faces. */
  bool only_selected_faces;
  /** When determining islands, use Seams as boundary edges. */
  bool use_seams;
  /** (In 3D Viewport or UV Editor) use aspect ratio from face. */
  bool correct_aspect;
  /** Ignore islands which have any pinned UVs. */
  bool ignore_pinned;
  /** Treat unselected UVs as if they were pinned. */
  bool pin_unselected;
  /** Additional space to add around each island. */
  float margin;
  /** Which formula to use when scaling island margin. */
  eUVPackIsland_MarginMethod margin_method;
  /** Additional translation for bottom left corner. */
  float udim_base_offset[2];
};

class PackIsland {
 public:
  rctf bounds_rect;
  float2 pre_translate; /* Output. */
  int caller_index;     /* Unchanged by #pack_islands, used by caller. */
};

void pack_islands(const Span<PackIsland *> &islands,
                  const UVPackIsland_Params &params,
                  float r_scale[2]);

}  // namespace blender::geometry
