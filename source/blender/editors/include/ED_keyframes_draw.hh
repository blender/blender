/* SPDX-FileCopyrightText: 2009 Blender Foundation, Joshua Leung. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editors
 */

#pragma once

#include "BLI_sys_types.h"

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
struct GreasePencil;
struct GreasePencilLayer;

/* draw simple diamond-shape keyframe */
/* caller should set up vertex format, bind GPU_SHADER_KEYFRAME_SHAPE,
 * immBegin(GPU_PRIM_POINTS, n), then call this n times */
struct KeyframeShaderBindings {
  uint pos_id;
  uint size_id;
  uint color_id;
  uint outline_color_id;
  uint flags_id;
};

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
void draw_fcurve_channel(AnimKeylistDrawList *draw_list,
                         AnimData *adt,
                         FCurve *fcu,
                         float ypos,
                         float yscale_fac,
                         int saction_flag);
/* Action Group Summary */
void draw_agroup_channel(AnimKeylistDrawList *draw_list,
                         AnimData *adt,
                         bActionGroup *agrp,
                         float ypos,
                         float yscale_fac,
                         int saction_flag);
/* Action Summary */
void draw_action_channel(AnimKeylistDrawList *draw_list,
                         AnimData *adt,
                         bAction *act,
                         float ypos,
                         float yscale_fac,
                         int saction_flag);
/* Object Summary */
void draw_object_channel(AnimKeylistDrawList *draw_list,
                         bDopeSheet *ads,
                         Object *ob,
                         float ypos,
                         float yscale_fac,
                         int saction_flag);
/* Scene Summary */
void draw_scene_channel(AnimKeylistDrawList *draw_list,
                        bDopeSheet *ads,
                        Scene *sce,
                        float ypos,
                        float yscale_fac,
                        int saction_flag);
/* DopeSheet Summary */
void draw_summary_channel(AnimKeylistDrawList *draw_list,
                          bAnimContext *ac,
                          float ypos,
                          float yscale_fac,
                          int saction_flag);

/* Grease Pencil cels channels */
void draw_grease_pencil_cels_channel(AnimKeylistDrawList *draw_list,
                                     bDopeSheet *ads,
                                     const GreasePencilLayer *layer,
                                     float ypos,
                                     float yscale_fac,
                                     int saction_flag);

/* Grease Pencil data channels */
void draw_grease_pencil_datablock_channel(AnimKeylistDrawList *draw_list,
                                          bDopeSheet *ads,
                                          const GreasePencil *grease_pencil,
                                          const float ypos,
                                          const float yscale_fac,
                                          int saction_flag);

/* Grease Pencil Layer */
void draw_gpl_channel(AnimKeylistDrawList *draw_list,
                      bDopeSheet *ads,
                      bGPDlayer *gpl,
                      float ypos,
                      float yscale_fac,
                      int saction_flag);
/* Mask Layer */
void draw_masklay_channel(AnimKeylistDrawList *draw_list,
                          bDopeSheet *ads,
                          MaskLayer *masklay,
                          float ypos,
                          float yscale_fac,
                          int saction_flag);

AnimKeylistDrawList *ED_keylist_draw_list_create();
void ED_keylist_draw_list_flush(AnimKeylistDrawList *draw_list, View2D *v2d);
void ED_keylist_draw_list_free(AnimKeylistDrawList *draw_list);
