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
 * The Original Code is Copyright (C) (C) 2009 Blender Foundation, Joshua Leung
 * All rights reserved.
 */

/** \file
 * \ingroup editors
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct AnimData;
struct AnimKeylistDrawList;
struct FCurve;
struct MaskLayer;
struct Object;
struct Scene;
struct View2D;
struct bAction;
struct bActionGroup;
struct bAnimContext;
struct bDopeSheet;
struct bGPDlayer;

/* draw simple diamond-shape keyframe */
/* caller should set up vertex format, bind GPU_SHADER_KEYFRAME_SHAPE,
 * immBegin(GPU_PRIM_POINTS, n), then call this n times */
typedef struct KeyframeShaderBindings {
  uint pos_id;
  uint size_id;
  uint color_id;
  uint outline_color_id;
  uint flags_id;
} KeyframeShaderBindings;

void draw_keyframe_shape(float x,
                         float y,
                         float size,
                         bool sel,
                         short key_type,
                         short mode,
                         float alpha,
                         const KeyframeShaderBindings *sh_bindings,
                         short handle_type,
                         short extreme_type);

/* ******************************* Methods ****************************** */

/* Channel Drawing ------------------ */
/* F-Curve */
void draw_fcurve_channel(struct AnimKeylistDrawList *draw_list,
                         struct AnimData *adt,
                         struct FCurve *fcu,
                         float ypos,
                         float yscale_fac,
                         int saction_flag);
/* Action Group Summary */
void draw_agroup_channel(struct AnimKeylistDrawList *draw_list,
                         struct AnimData *adt,
                         struct bActionGroup *agrp,
                         float ypos,
                         float yscale_fac,
                         int saction_flag);
/* Action Summary */
void draw_action_channel(struct AnimKeylistDrawList *draw_list,
                         struct AnimData *adt,
                         struct bAction *act,
                         float ypos,
                         float yscale_fac,
                         int saction_flag);
/* Object Summary */
void draw_object_channel(struct AnimKeylistDrawList *draw_list,
                         struct bDopeSheet *ads,
                         struct Object *ob,
                         float ypos,
                         float yscale_fac,
                         int saction_flag);
/* Scene Summary */
void draw_scene_channel(struct AnimKeylistDrawList *draw_list,
                        struct bDopeSheet *ads,
                        struct Scene *sce,
                        float ypos,
                        float yscale_fac,
                        int saction_flag);
/* DopeSheet Summary */
void draw_summary_channel(struct AnimKeylistDrawList *draw_list,
                          struct bAnimContext *ac,
                          float ypos,
                          float yscale_fac,
                          int saction_flag);
/* Grease Pencil Layer */
void draw_gpl_channel(struct AnimKeylistDrawList *draw_list,
                      struct bDopeSheet *ads,
                      struct bGPDlayer *gpl,
                      float ypos,
                      float yscale_fac,
                      int saction_flag);
/* Mask Layer */
void draw_masklay_channel(struct AnimKeylistDrawList *draw_list,
                          struct bDopeSheet *ads,
                          struct MaskLayer *masklay,
                          float ypos,
                          float yscale_fac,
                          int saction_flag);

struct AnimKeylistDrawList *ED_keylist_draw_list_create(void);
void ED_keylist_draw_list_flush(struct AnimKeylistDrawList *draw_list, struct View2D *v2d);
void ED_keylist_draw_list_free(struct AnimKeylistDrawList *draw_list);

#ifdef __cplusplus
}
#endif
