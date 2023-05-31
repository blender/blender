/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2023 Blender Foundation. */

/** \file
 * \ingroup editors
 */

#pragma once

struct Main;
struct Object;

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus

namespace blender::ed::greasepencil {

void create_blank(Main &bmain, Object &object, int frame_numer);
void create_stroke(Main &bmain, Object &object, float4x4 matrix, int frame_numer);

}  // namespace blender::ed::greasepencil
#endif
