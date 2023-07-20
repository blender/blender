/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editors
 */

#pragma once

#include "BKE_attribute.h"

struct bContext;

struct Main;
struct Object;

struct wmKeyConfig;

#ifdef __cplusplus
extern "C" {
#endif

enum {
  LAYER_REORDER_ABOVE,
  LAYER_REORDER_BELOW,
};

/* -------------------------------------------------------------------- */
/** \name C Wrappers
 * \{ */

void ED_operatortypes_grease_pencil(void);
void ED_operatortypes_grease_pencil_draw(void);
void ED_operatortypes_grease_pencil_frames(void);
void ED_operatortypes_grease_pencil_layers(void);
void ED_operatortypes_grease_pencil_select(void);
void ED_operatortypes_grease_pencil_edit(void);
void ED_keymap_grease_pencil(struct wmKeyConfig *keyconf);
/**
 * Get the selection mode for Grease Pencil selection operators: point, stroke, segment.
 */
eAttrDomain ED_grease_pencil_selection_domain_get(struct bContext *C);

/** \} */

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus

#  include "BLI_generic_span.hh"
#  include "BLI_math_matrix_types.hh"

namespace blender::ed::greasepencil {

bool active_grease_pencil_poll(bContext *C);
bool editable_grease_pencil_poll(bContext *C);
bool editable_grease_pencil_point_selection_poll(bContext *C);
bool grease_pencil_painting_poll(bContext *C);

void create_blank(Main &bmain, Object &object, int frame_number);
void create_stroke(Main &bmain, Object &object, float4x4 matrix, int frame_number);
void create_suzanne(Main &bmain, Object &object, float4x4 matrix, const int frame_number);

void gaussian_blur_1D(const GSpan src,
                      int64_t iterations,
                      float influence,
                      bool smooth_ends,
                      bool keep_shape,
                      bool is_cyclic,
                      GMutableSpan dst);

void ramer_douglas_peucker_simplify(const Span<float3> src, float epsilon, MutableSpan<bool> dst);

}  // namespace blender::ed::greasepencil
#endif
