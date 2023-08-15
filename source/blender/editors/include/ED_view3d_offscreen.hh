/* SPDX-FileCopyrightText: 2008 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editors
 */

#pragma once

#include "DNA_object_enums.h"
#include "DNA_view3d_types.h"

#include "IMB_imbuf_types.h"

/* ********* exports for space_view3d/ module for offscreen rendering ********** */
struct ARegion;
struct Depsgraph;
struct GPUOffScreen;
struct GPUViewport;
struct Scene;
struct View3D;
struct View3DShading;

void ED_view3d_draw_offscreen(Depsgraph *depsgraph,
                              const Scene *scene,
                              eDrawType drawtype,
                              View3D *v3d,
                              ARegion *region,
                              int winx,
                              int winy,
                              const float viewmat[4][4],
                              const float winmat[4][4],
                              bool is_image_render,
                              bool draw_background,
                              const char *viewname,
                              bool do_color_management,
                              bool restore_rv3d_mats,
                              GPUOffScreen *ofs,
                              GPUViewport *viewport);
/**
 * Creates own fake 3d views (wrapping #ED_view3d_draw_offscreen). Similar too
 * #ED_view_draw_offscreen_imbuf_simple, but takes view/projection matrices as arguments.
 */
void ED_view3d_draw_offscreen_simple(Depsgraph *depsgraph,
                                     Scene *scene,
                                     View3DShading *shading_override,
                                     eDrawType drawtype,
                                     int object_type_exclude_viewport_override,
                                     int object_type_exclude_select_override,
                                     int winx,
                                     int winy,
                                     unsigned int draw_flags,
                                     const float viewmat[4][4],
                                     const float winmat[4][4],
                                     float clip_start,
                                     float clip_end,
                                     bool is_xr_surface,
                                     bool is_image_render,
                                     bool draw_background,
                                     const char *viewname,
                                     bool do_color_management,
                                     GPUOffScreen *ofs,
                                     GPUViewport *viewport);

/**
 * Utility func for ED_view3d_draw_offscreen
 *
 * \param ofs: Optional off-screen buffer, can be NULL.
 * (avoids re-creating when doing multiple GL renders).
 */
ImBuf *ED_view3d_draw_offscreen_imbuf(Depsgraph *depsgraph,
                                      Scene *scene,
                                      eDrawType drawtype,
                                      View3D *v3d,
                                      ARegion *region,
                                      int sizex,
                                      int sizey,
                                      eImBufFlags imbuf_flag,
                                      int alpha_mode,
                                      const char *viewname,
                                      bool restore_rv3d_mats,
                                      GPUOffScreen *ofs,
                                      char err_out[256]);
/**
 * Creates own fake 3d views (wrapping #ED_view3d_draw_offscreen_imbuf)
 *
 * \param ofs: Optional off-screen buffer can be NULL.
 * (avoids re-creating when doing multiple GL renders).
 *
 * \note used by the sequencer
 */
ImBuf *ED_view3d_draw_offscreen_imbuf_simple(Depsgraph *depsgraph,
                                             Scene *scene,
                                             View3DShading *shading_override,
                                             eDrawType drawtype,
                                             Object *camera,
                                             int width,
                                             int height,
                                             eImBufFlags imbuf_flags,
                                             eV3DOffscreenDrawFlag draw_flags,
                                             int alpha_mode,
                                             const char *viewname,
                                             GPUOffScreen *ofs,
                                             char err_out[256]);
