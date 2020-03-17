/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup editors
 */

#ifndef __ED_VIEW3D_OFFSCREEN_H__
#define __ED_VIEW3D_OFFSCREEN_H__

#include "DNA_view3d_types.h"
#include "DNA_object_enums.h"

#include "IMB_imbuf_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ********* exports for space_view3d/ module for offscreen rendering ********** */
struct ARegion;
struct Depsgraph;
struct GPUOffScreen;
struct GPUViewport;
struct Scene;
struct View3D;
struct View3DShading;

void ED_view3d_draw_offscreen(struct Depsgraph *depsgraph,
                              const struct Scene *scene,
                              eDrawType drawtype,
                              struct View3D *v3d,
                              struct ARegion *region,
                              int winx,
                              int winy,
                              float viewmat[4][4],
                              float winmat[4][4],
                              bool is_image_render,
                              bool do_sky,
                              bool is_persp,
                              const char *viewname,
                              const bool do_color_management,
                              struct GPUOffScreen *ofs,
                              struct GPUViewport *viewport);
void ED_view3d_draw_offscreen_simple(struct Depsgraph *depsgraph,
                                     struct Scene *scene,
                                     struct View3DShading *shading_override,
                                     int drawtype,
                                     int winx,
                                     int winy,
                                     unsigned int draw_flags,
                                     float viewmat[4][4],
                                     float winmat[4][4],
                                     float clip_start,
                                     float clip_end,
                                     bool is_image_render,
                                     bool do_sky,
                                     bool is_persp,
                                     const char *viewname,
                                     const bool do_color_management,
                                     struct GPUOffScreen *ofs,
                                     struct GPUViewport *viewport);

struct ImBuf *ED_view3d_draw_offscreen_imbuf(struct Depsgraph *depsgraph,
                                             struct Scene *scene,
                                             eDrawType drawtype,
                                             struct View3D *v3d,
                                             struct ARegion *region,
                                             int sizex,
                                             int sizey,
                                             eImBufFlags imbuf_flag,
                                             int alpha_mode,
                                             const char *viewname,
                                             struct GPUOffScreen *ofs,
                                             char err_out[256]);
struct ImBuf *ED_view3d_draw_offscreen_imbuf_simple(struct Depsgraph *depsgraph,
                                                    struct Scene *scene,
                                                    struct View3DShading *shading_override,
                                                    eDrawType drawtype,
                                                    struct Object *camera,
                                                    int width,
                                                    int height,
                                                    eImBufFlags imbuf_flags,
                                                    eV3DOffscreenDrawFlag draw_flags,
                                                    int alpha_mode,
                                                    const char *viewname,
                                                    struct GPUOffScreen *ofs,
                                                    char err_out[256]);

#ifdef __cplusplus
}
#endif

#endif /* __ED_VIEW3D_H__ */
