/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2023 Blender Foundation. */

/** \file
 * \ingroup editors
 */

#pragma once

struct bContext;

struct Main;
struct Object;

struct wmKeyConfig;

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------- */
/** \name C Wrappers
 * \{ */

void ED_operatortypes_grease_pencil(void);
void ED_keymap_grease_pencil(struct wmKeyConfig *keyconf);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus

namespace blender::ed::greasepencil {

bool editable_grease_pencil_poll(bContext *C);

void create_blank(Main &bmain, Object &object, int frame_number);
void create_stroke(Main &bmain, Object &object, float4x4 matrix, int frame_number);
void create_suzanne(Main &bmain, Object &object, float4x4 matrix, const int frame_number);

}  // namespace blender::ed::greasepencil
#endif
