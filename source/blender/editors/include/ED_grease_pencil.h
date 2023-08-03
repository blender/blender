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

struct KeyframeEditData;

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

#  include "BKE_grease_pencil.hh"

#  include "BLI_generic_span.hh"
#  include "BLI_math_matrix_types.hh"

#  include "ED_keyframes_edit.h"

namespace blender::ed::greasepencil {

bool remove_all_selected_frames(GreasePencil &grease_pencil, bke::greasepencil::Layer &layer);

void select_layer_channel(GreasePencil &grease_pencil, bke::greasepencil::Layer *layer);

/**
 * Sets the selection flag, according to \a selection_mode to the frame at \a frame_number in the
 * \a layer if such frame exists. Returns false if no such frame exists.
 */
bool select_frame_at(bke::greasepencil::Layer &layer,
                     const int frame_number,
                     const short select_mode);

void select_all_frames(bke::greasepencil::Layer &layer, const short select_mode);

void select_frames_region(struct KeyframeEditData *ked,
                          bke::greasepencil::Layer &layer,
                          const short tool,
                          const short select_mode);

/**
 * Returns true if any frame of the \a layer is selected.
 */
bool has_any_frame_selected(const bke::greasepencil::Layer &layer);

void create_keyframe_edit_data_selected_frames_list(KeyframeEditData *ked,
                                                    const bke::greasepencil::Layer &layer);

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

int64_t ramer_douglas_peucker_simplify(IndexRange range,
                                       float epsilon,
                                       FunctionRef<float(IndexRange, int64_t)> dist_function,
                                       MutableSpan<bool> dst);

}  // namespace blender::ed::greasepencil
#endif
