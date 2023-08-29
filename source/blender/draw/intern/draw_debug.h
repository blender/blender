/* SPDX-FileCopyrightText: 2018 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 *
 * \brief Simple API to draw debug shapes in the viewport.
 * IMPORTANT: This is the legacy API for C. Use draw_debug.hh instead in new C++ code.
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef struct DRWDebugModule DRWDebugModule;

struct BoundBox;

void DRW_debug_modelmat_reset(void);
void DRW_debug_modelmat(const float modelmat[4][4]);

/**
 * IMPORTANT: For now there is a limit of DRW_DEBUG_DRAW_VERT_MAX that can be drawn
 * using all the draw functions.
 */
void DRW_debug_line_v3v3(const float v1[3], const float v2[3], const float color[4]);
void DRW_debug_polygon_v3(const float (*v)[3], int vert_len, const float color[4]);
/**
 * \note g_modelmat is still applied on top.
 */
void DRW_debug_m4(const float m[4][4]);
void DRW_debug_m4_as_bbox(const float m[4][4], bool invert, const float color[4]);
void DRW_debug_bbox(const BoundBox *bbox, const float color[4]);
void DRW_debug_sphere(const float center[3], float radius, const float color[4]);

#ifdef __cplusplus
}
#endif
