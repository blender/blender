/* SPDX-FileCopyrightText: 2014 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edgpencil
 */

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_ghash.h"
#include "BLI_hash.h"
#include "BLI_lasso_2d.hh"
#include "BLI_math_color.h"
#include "BLI_math_matrix.h"
#include "BLI_math_vector.hh"
#include "BLI_time.h"
#include "BLI_utildefines.h"

#include "BLT_translation.hh"

#include "DNA_brush_types.h"
#include "DNA_gpencil_legacy_types.h"
#include "DNA_material_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_view3d_types.h"

#include "BKE_action.h"
#include "BKE_brush.hh"
#include "BKE_collection.hh"
#include "BKE_colortools.hh"
#include "BKE_context.hh"
#include "BKE_deform.hh"
#include "BKE_gpencil_curve_legacy.h"
#include "BKE_gpencil_geom_legacy.h"
#include "BKE_gpencil_legacy.h"
#include "BKE_main.hh"
#include "BKE_material.h"
#include "BKE_object.hh"
#include "BKE_paint.hh"
#include "BKE_tracking.h"

#include "WM_api.hh"
#include "WM_toolsystem.hh"
#include "WM_types.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_enum_types.hh"
#include "RNA_prototypes.h"

#include "UI_resources.hh"
#include "UI_view2d.hh"

#include "ED_clip.hh"
#include "ED_gpencil_legacy.hh"
#include "ED_object.hh"
#include "ED_select_utils.hh"
#include "ED_transform_snap_object_context.hh"
#include "ED_view3d.hh"

#include "GPU_immediate.hh"
#include "GPU_immediate_util.hh"
#include "GPU_state.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_query.hh"

#include "gpencil_intern.hh"

/* ******************************************************** */
/* Context Wrangling... */

bGPdata **ED_gpencil_data_get_pointers_direct(ScrArea *area, Object *ob, PointerRNA *r_ptr)
{
  /* if there's an active area, check if the particular editor may
   * have defined any special Grease Pencil context for editing...
   */
  if (area) {
    switch (area->spacetype) {
      case SPACE_PROPERTIES: /* properties */
      case SPACE_INFO:       /* header info */
      case SPACE_TOPBAR:     /* Top-bar */
      case SPACE_VIEW3D:     /* 3D-View */
      {
        if (ob && (ob->type == OB_GPENCIL_LEGACY)) {
          /* GP Object. */
          if (r_ptr) {
            *r_ptr = RNA_id_pointer_create(&ob->id);
          }
          return (bGPdata **)&ob->data;
        }
        return nullptr;
      }
      default: /* Unsupported space. */
        return nullptr;
    }
  }

  return nullptr;
}

bGPdata **ED_annotation_data_get_pointers_direct(ID *screen_id,
                                                 ScrArea *area,
                                                 Scene *scene,
                                                 PointerRNA *r_ptr)
{
  /* If there's an active area, check if the particular editor may
   * have defined any special Grease Pencil context for editing. */
  if (area) {
    SpaceLink *sl = static_cast<SpaceLink *>(area->spacedata.first);

    switch (area->spacetype) {
      case SPACE_INFO: /* header info */
      {
        return nullptr;
      }

      case SPACE_TOPBAR:     /* Top-bar */
      case SPACE_VIEW3D:     /* 3D-View */
      case SPACE_PROPERTIES: /* properties */
      {
        if (r_ptr) {
          *r_ptr = RNA_id_pointer_create(&scene->id);
        }
        return &scene->gpd;
      }
      case SPACE_NODE: /* Nodes Editor */
      {
        SpaceNode *snode = (SpaceNode *)sl;

        /* return the GP data for the active node block/node */
        if (snode && snode->nodetree) {
          /* for now, as long as there's an active node tree,
           * default to using that in the Nodes Editor */
          if (r_ptr) {
            *r_ptr = RNA_id_pointer_create(&snode->nodetree->id);
          }
          return &snode->nodetree->gpd;
        }

        /* Even when there is no node-tree, don't allow this to flow to scene. */
        return nullptr;
      }
      case SPACE_SEQ: /* Sequencer */
      {
        SpaceSeq *sseq = (SpaceSeq *)sl;

        /* For now, Grease Pencil data is associated with the space
         * (actually preview region only). */
        if (r_ptr) {
          *r_ptr = RNA_pointer_create(screen_id, &RNA_SpaceSequenceEditor, sseq);
        }
        return &sseq->gpd;
      }
      case SPACE_IMAGE: /* Image/UV Editor */
      {
        SpaceImage *sima = (SpaceImage *)sl;

        /* For now, Grease Pencil data is associated with the space... */
        if (r_ptr) {
          *r_ptr = RNA_pointer_create(screen_id, &RNA_SpaceImageEditor, sima);
        }
        return &sima->gpd;
      }
      case SPACE_CLIP: /* Nodes Editor */
      {
        SpaceClip *sc = (SpaceClip *)sl;
        MovieClip *clip = ED_space_clip_get_clip(sc);

        if (clip) {
          if (sc->gpencil_src == SC_GPENCIL_SRC_TRACK) {
            const MovieTrackingObject *tracking_object = BKE_tracking_object_get_active(
                &clip->tracking);
            MovieTrackingTrack *track = tracking_object->active_track;

            if (!track) {
              return nullptr;
            }

            if (r_ptr) {
              *r_ptr = RNA_pointer_create(&clip->id, &RNA_MovieTrackingTrack, track);
            }
            return &track->gpd;
          }
          if (r_ptr) {
            *r_ptr = RNA_id_pointer_create(&clip->id);
          }
          return &clip->gpd;
        }
        break;
      }
      default: /* unsupported space */
        return nullptr;
    }
  }

  return nullptr;
}

bGPdata **ED_gpencil_data_get_pointers(const bContext *C, PointerRNA *r_ptr)
{
  ScrArea *area = CTX_wm_area(C);
  Object *ob = CTX_data_active_object(C);

  return ED_gpencil_data_get_pointers_direct(area, ob, r_ptr);
}

bGPdata **ED_annotation_data_get_pointers(const bContext *C, PointerRNA *r_ptr)
{
  ID *screen_id = (ID *)CTX_wm_screen(C);
  Scene *scene = CTX_data_scene(C);
  ScrArea *area = CTX_wm_area(C);

  return ED_annotation_data_get_pointers_direct(screen_id, area, scene, r_ptr);
}
/* -------------------------------------------------------- */

bGPdata *ED_gpencil_data_get_active_direct(ScrArea *area, Object *ob)
{
  bGPdata **gpd_ptr = ED_gpencil_data_get_pointers_direct(area, ob, nullptr);
  return (gpd_ptr) ? *(gpd_ptr) : nullptr;
}

bGPdata *ED_annotation_data_get_active_direct(ID *screen_id, ScrArea *area, Scene *scene)
{
  bGPdata **gpd_ptr = ED_annotation_data_get_pointers_direct(screen_id, area, scene, nullptr);
  return (gpd_ptr) ? *(gpd_ptr) : nullptr;
}

bGPdata *ED_gpencil_data_get_active(const bContext *C)
{
  Object *ob = CTX_data_active_object(C);
  if ((ob == nullptr) || (ob->type != OB_GPENCIL_LEGACY)) {
    return nullptr;
  }
  return static_cast<bGPdata *>(ob->data);
}

bGPdata *ED_annotation_data_get_active(const bContext *C)
{
  bGPdata **gpd_ptr = ED_annotation_data_get_pointers(C, nullptr);
  return (gpd_ptr) ? *(gpd_ptr) : nullptr;
}

bGPdata *ED_gpencil_data_get_active_evaluated(const bContext *C)
{
  ScrArea *area = CTX_wm_area(C);

  const Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  Object *ob = CTX_data_active_object(C);
  Object *ob_eval = DEG_get_evaluated_object(depsgraph, ob);

  return ED_gpencil_data_get_active_direct(area, ob_eval);
}

/* -------------------------------------------------------- */

bool ED_gpencil_data_owner_is_annotation(PointerRNA *owner_ptr)
{
  /* Key Assumption: If the pointer is an object, we're dealing with a GP Object's data.
   * Otherwise, the GP data-block is being used for annotations (i.e. everywhere else). */
  return ((owner_ptr) && (owner_ptr->type != &RNA_Object));
}

/* ******************************************************** */
/* Keyframe Indicator Checks */

bool ED_gpencil_has_keyframe_v3d(Scene * /*scene*/, Object *ob, int cfra)
{
  if (ob && ob->data && (ob->type == OB_GPENCIL_LEGACY)) {
    bGPDlayer *gpl = BKE_gpencil_layer_active_get(static_cast<bGPdata *>(ob->data));
    if (gpl) {
      if (gpl->actframe) {
        /* XXX: assumes that frame has been fetched already */
        return (gpl->actframe->framenum == cfra);
      }
      /* XXX: disabled as could be too much of a penalty */
      // return BKE_gpencil_layer_frame_find(gpl, cfra);
    }
  }

  return false;
}

/* ******************************************************** */
/* Poll Callbacks */

bool gpencil_add_poll(bContext *C)
{
  Object *ob = CTX_data_active_object(C);
  if (ob == nullptr) {
    return false;
  }
  bGPdata *gpd = (bGPdata *)ob->data;

  return (gpd != nullptr);
}

bool gpencil_active_layer_poll(bContext *C)
{
  Object *ob = CTX_data_active_object(C);
  if ((ob == nullptr) || (ob->type != OB_GPENCIL_LEGACY)) {
    return false;
  }
  bGPdata *gpd = (bGPdata *)ob->data;
  bGPDlayer *gpl = BKE_gpencil_layer_active_get(gpd);

  return (gpl != nullptr);
}

bool gpencil_active_brush_poll(bContext *C)
{
  ToolSettings *ts = CTX_data_tool_settings(C);
  Paint *paint = &ts->gp_paint->paint;
  if (paint) {
    return (BKE_paint_brush(paint) != nullptr);
  }
  return false;
}

/* ******************************************************** */
/* Dynamic Enums of GP Layers */
/* NOTE: These include an option to create a new layer and use that... */

const EnumPropertyItem *ED_gpencil_layers_enum_itemf(bContext *C,
                                                     PointerRNA * /*ptr*/,
                                                     PropertyRNA * /*prop*/,
                                                     bool *r_free)
{
  bGPdata *gpd = CTX_data_gpencil_data(C);
  bGPDlayer *gpl;
  EnumPropertyItem *item = nullptr, item_tmp = {0};
  int totitem = 0;
  int i = 0;

  if (ELEM(nullptr, C, gpd)) {
    return rna_enum_dummy_DEFAULT_items;
  }

  /* Existing layers */
  for (gpl = static_cast<bGPDlayer *>(gpd->layers.first); gpl; gpl = gpl->next, i++) {
    item_tmp.identifier = gpl->info;
    item_tmp.name = gpl->info;
    item_tmp.value = i;

    if (gpl->flag & GP_LAYER_ACTIVE) {
      item_tmp.icon = ICON_GREASEPENCIL;
    }
    else {
      item_tmp.icon = ICON_NONE;
    }

    RNA_enum_item_add(&item, &totitem, &item_tmp);
  }

  RNA_enum_item_end(&item, &totitem);
  *r_free = true;

  return item;
}

const EnumPropertyItem *ED_gpencil_layers_with_new_enum_itemf(bContext *C,
                                                              PointerRNA * /*ptr*/,
                                                              PropertyRNA * /*prop*/,
                                                              bool *r_free)
{
  bGPdata *gpd = CTX_data_gpencil_data(C);
  bGPDlayer *gpl;
  EnumPropertyItem *item = nullptr, item_tmp = {0};
  int totitem = 0;
  int i = 0;

  if (ELEM(nullptr, C, gpd)) {
    return rna_enum_dummy_DEFAULT_items;
  }

  /* Create new layer */
  /* TODO: have some way of specifying that we don't want this? */
  {
    /* "New Layer" entry */
    item_tmp.identifier = "__CREATE__";
    item_tmp.name = "New Layer";
    item_tmp.value = -1;
    item_tmp.icon = ICON_ADD;
    RNA_enum_item_add(&item, &totitem, &item_tmp);

    /* separator */
    RNA_enum_item_add_separator(&item, &totitem);
  }

  const int tot = BLI_listbase_count(&gpd->layers);
  /* Existing layers */
  for (gpl = static_cast<bGPDlayer *>(gpd->layers.last), i = 0; gpl; gpl = gpl->prev, i++) {
    item_tmp.identifier = gpl->info;
    item_tmp.name = gpl->info;
    item_tmp.value = tot - i - 1;

    if (gpl->flag & GP_LAYER_ACTIVE) {
      item_tmp.icon = ICON_GREASEPENCIL;
    }
    else {
      item_tmp.icon = ICON_NONE;
    }

    RNA_enum_item_add(&item, &totitem, &item_tmp);
  }

  RNA_enum_item_end(&item, &totitem);
  *r_free = true;

  return item;
}

const EnumPropertyItem *ED_gpencil_material_enum_itemf(bContext *C,
                                                       PointerRNA * /*ptr*/,
                                                       PropertyRNA * /*prop*/,
                                                       bool *r_free)
{
  Object *ob = CTX_data_active_object(C);
  EnumPropertyItem *item = nullptr, item_tmp = {0};
  int totitem = 0;
  int i = 0;

  if (ELEM(nullptr, C, ob)) {
    return rna_enum_dummy_DEFAULT_items;
  }

  /* Existing materials */
  for (i = 1; i <= ob->totcol; i++) {
    Material *ma = BKE_object_material_get(ob, i);
    if (ma) {
      item_tmp.identifier = ma->id.name + 2;
      item_tmp.name = ma->id.name + 2;
      item_tmp.value = i;
      item_tmp.icon = ma->preview ? ma->preview->icon_id : ICON_NONE;

      RNA_enum_item_add(&item, &totitem, &item_tmp);
    }
  }

  RNA_enum_item_end(&item, &totitem);
  *r_free = true;

  return item;
}

/* ******************************************************** */
/* Brush Tool Core */

bool gpencil_stroke_inside_circle(const float mval[2], int rad, int x0, int y0, int x1, int y1)
{
  /* simple within-radius check for now */
  const float screen_co_a[2] = {float(x0), float(y0)};
  const float screen_co_b[2] = {float(x1), float(y1)};

  if (edge_inside_circle(mval, rad, screen_co_a, screen_co_b)) {
    return true;
  }

  /* not inside */
  return false;
}

/* ******************************************************** */
/* Selection Validity Testing */

bool ED_gpencil_frame_has_selected_stroke(const bGPDframe *gpf)
{
  LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
    if (gps->flag & GP_STROKE_SELECT) {
      return true;
    }
  }

  return false;
}

bool ED_gpencil_layer_has_selected_stroke(const bGPDlayer *gpl, const bool is_multiedit)
{
  bGPDframe *init_gpf = static_cast<bGPDframe *>((is_multiedit) ? gpl->frames.first :
                                                                  gpl->actframe);
  for (bGPDframe *gpf = init_gpf; gpf; gpf = gpf->next) {
    if ((gpf == gpl->actframe) || ((gpf->flag & GP_FRAME_SELECT) && (is_multiedit))) {
      if (ED_gpencil_frame_has_selected_stroke(gpf)) {
        return true;
      }
    }
    /* If not multi-edit, exit loop. */
    if (!is_multiedit) {
      break;
    }
  }

  return false;
}

/* ******************************************************** */
/* Stroke Validity Testing */

bool ED_gpencil_stroke_can_use_direct(const ScrArea *area, const bGPDstroke *gps)
{
  /* sanity check */
  if (ELEM(nullptr, area, gps)) {
    return false;
  }

  /* filter stroke types by flags + spacetype */
  if (gps->flag & GP_STROKE_3DSPACE) {
    /* 3D strokes - only in 3D view */
    return ELEM(area->spacetype, SPACE_VIEW3D, SPACE_PROPERTIES);
  }
  if (gps->flag & GP_STROKE_2DIMAGE) {
    /* Special "image" strokes - only in Image Editor */
    return (area->spacetype == SPACE_IMAGE);
  }
  if (gps->flag & GP_STROKE_2DSPACE) {
    /* 2D strokes (data-space) - for any 2D view (i.e. everything other than 3D view). */
    return (area->spacetype != SPACE_VIEW3D);
  }
  /* view aligned - anything goes */
  return true;
}

bool ED_gpencil_stroke_can_use(const bContext *C, const bGPDstroke *gps)
{
  ScrArea *area = CTX_wm_area(C);
  return ED_gpencil_stroke_can_use_direct(area, gps);
}

bool ED_gpencil_stroke_material_editable(Object *ob, const bGPDlayer *gpl, const bGPDstroke *gps)
{
  /* check if the color is editable */
  MaterialGPencilStyle *gp_style = BKE_gpencil_material_settings(ob, gps->mat_nr + 1);

  if (gp_style != nullptr) {
    if (gp_style->flag & GP_MATERIAL_HIDE) {
      return false;
    }
    if (((gpl->flag & GP_LAYER_UNLOCK_COLOR) == 0) && (gp_style->flag & GP_MATERIAL_LOCKED)) {
      return false;
    }
  }

  return true;
}

bool ED_gpencil_stroke_material_visible(Object *ob, const bGPDstroke *gps)
{
  /* check if the color is editable */
  MaterialGPencilStyle *gp_style = BKE_gpencil_material_settings(ob, gps->mat_nr + 1);

  if (gp_style != nullptr) {
    if (gp_style->flag & GP_MATERIAL_HIDE) {
      return false;
    }
  }

  return true;
}

/* ******************************************************** */
/* Space Conversion */

void gpencil_point_conversion_init(bContext *C, GP_SpaceConversion *r_gsc)
{
  ScrArea *area = CTX_wm_area(C);
  ARegion *region = CTX_wm_region(C);

  /* zero out the storage (just in case) */
  memset(r_gsc, 0, sizeof(GP_SpaceConversion));
  unit_m4(r_gsc->mat);

  /* store settings */
  r_gsc->scene = CTX_data_scene(C);
  r_gsc->ob = CTX_data_active_object(C);

  r_gsc->area = area;
  r_gsc->region = region;
  r_gsc->v2d = &region->v2d;

  /* init region-specific stuff */
  if (area->spacetype == SPACE_VIEW3D) {
    wmWindow *win = CTX_wm_window(C);
    Scene *scene = CTX_data_scene(C);
    Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
    View3D *v3d = (View3D *)CTX_wm_space_data(C);
    RegionView3D *rv3d = static_cast<RegionView3D *>(region->regiondata);

    /* init 3d depth buffers */
    view3d_operator_needs_opengl(C);

    view3d_region_operator_needs_opengl(win, region);
    ED_view3d_depth_override(depsgraph, region, v3d, nullptr, V3D_DEPTH_NO_GPENCIL, nullptr);

    /* for camera view set the subrect */
    if (rv3d->persp == RV3D_CAMOB) {
      ED_view3d_calc_camera_border(
          scene, depsgraph, region, v3d, rv3d, &r_gsc->subrect_data, true);
      r_gsc->subrect = &r_gsc->subrect_data;
    }
  }
}

void gpencil_point_to_world_space(const bGPDspoint *pt,
                                  const float diff_mat[4][4],
                                  bGPDspoint *r_pt)
{
  mul_v3_m4v3(&r_pt->x, diff_mat, &pt->x);
}

void gpencil_world_to_object_space(Depsgraph *depsgraph,
                                   Object *obact,
                                   bGPDlayer *gpl,
                                   bGPDstroke *gps)
{
  bGPDspoint *pt;
  int i;

  /* undo matrix */
  float diff_mat[4][4];
  float inverse_diff_mat[4][4];

  BKE_gpencil_layer_transform_matrix_get(depsgraph, obact, gpl, diff_mat);
  zero_axis_bias_m4(diff_mat);
  invert_m4_m4(inverse_diff_mat, diff_mat);

  for (i = 0; i < gps->totpoints; i++) {
    pt = &gps->points[i];
    mul_m4_v3(inverse_diff_mat, &pt->x);
  }
}

void gpencil_world_to_object_space_point(Depsgraph *depsgraph,
                                         Object *obact,
                                         bGPDlayer *gpl,
                                         bGPDspoint *pt)
{
  /* undo matrix */
  float diff_mat[4][4];
  float inverse_diff_mat[4][4];

  BKE_gpencil_layer_transform_matrix_get(depsgraph, obact, gpl, diff_mat);
  zero_axis_bias_m4(diff_mat);
  invert_m4_m4(inverse_diff_mat, diff_mat);

  mul_m4_v3(inverse_diff_mat, &pt->x);
}

void gpencil_point_to_xy(
    const GP_SpaceConversion *gsc, const bGPDstroke *gps, const bGPDspoint *pt, int *r_x, int *r_y)
{
  const ARegion *region = gsc->region;
  const View2D *v2d = gsc->v2d;
  const rctf *subrect = gsc->subrect;
  int xyval[2];

  /* sanity checks */
  BLI_assert(!(gps->flag & GP_STROKE_3DSPACE) || (gsc->area->spacetype == SPACE_VIEW3D));
  BLI_assert(!(gps->flag & GP_STROKE_2DSPACE) || (gsc->area->spacetype != SPACE_VIEW3D));

  if (gps->flag & GP_STROKE_3DSPACE) {
    if (ED_view3d_project_int_global(region, &pt->x, xyval, V3D_PROJ_TEST_NOP) == V3D_PROJ_RET_OK)
    {
      *r_x = xyval[0];
      *r_y = xyval[1];
    }
    else {
      *r_x = V2D_IS_CLIPPED;
      *r_y = V2D_IS_CLIPPED;
    }
  }
  else if (gps->flag & GP_STROKE_2DSPACE) {
    float vec[3] = {pt->x, pt->y, 0.0f};
    mul_m4_v3(gsc->mat, vec);
    UI_view2d_view_to_region_clip(v2d, vec[0], vec[1], r_x, r_y);
  }
  else {
    if (subrect == nullptr) {
      /* normal 3D view (or view space) */
      *r_x = int(pt->x / 100 * region->winx);
      *r_y = int(pt->y / 100 * region->winy);
    }
    else {
      /* camera view, use subrect */
      *r_x = int((pt->x / 100) * BLI_rctf_size_x(subrect)) + subrect->xmin;
      *r_y = int((pt->y / 100) * BLI_rctf_size_y(subrect)) + subrect->ymin;
    }
  }
}

void gpencil_point_to_xy_fl(const GP_SpaceConversion *gsc,
                            const bGPDstroke *gps,
                            const bGPDspoint *pt,
                            float *r_x,
                            float *r_y)
{
  const ARegion *region = gsc->region;
  const View2D *v2d = gsc->v2d;
  const rctf *subrect = gsc->subrect;
  float xyval[2];

  /* sanity checks */
  BLI_assert(!(gps->flag & GP_STROKE_3DSPACE) || (gsc->area->spacetype == SPACE_VIEW3D));
  BLI_assert(!(gps->flag & GP_STROKE_2DSPACE) || (gsc->area->spacetype != SPACE_VIEW3D));

  if (gps->flag & GP_STROKE_3DSPACE) {
    if (ED_view3d_project_float_global(region, &pt->x, xyval, V3D_PROJ_TEST_NOP) ==
        V3D_PROJ_RET_OK)
    {
      *r_x = xyval[0];
      *r_y = xyval[1];
    }
    else {
      *r_x = 0.0f;
      *r_y = 0.0f;
    }
  }
  else if (gps->flag & GP_STROKE_2DSPACE) {
    float vec[3] = {pt->x, pt->y, 0.0f};
    int t_x, t_y;

    mul_m4_v3(gsc->mat, vec);
    UI_view2d_view_to_region_clip(v2d, vec[0], vec[1], &t_x, &t_y);

    if ((t_x == t_y) && (t_x == V2D_IS_CLIPPED)) {
      /* XXX: Or should we just always use the values as-is? */
      *r_x = 0.0f;
      *r_y = 0.0f;
    }
    else {
      *r_x = float(t_x);
      *r_y = float(t_y);
    }
  }
  else {
    if (subrect == nullptr) {
      /* normal 3D view (or view space) */
      *r_x = (pt->x / 100.0f * region->winx);
      *r_y = (pt->y / 100.0f * region->winy);
    }
    else {
      /* camera view, use subrect */
      *r_x = ((pt->x / 100.0f) * BLI_rctf_size_x(subrect)) + subrect->xmin;
      *r_y = ((pt->y / 100.0f) * BLI_rctf_size_y(subrect)) + subrect->ymin;
    }
  }
}

void gpencil_point_3d_to_xy(const GP_SpaceConversion *gsc,
                            const short flag,
                            const float pt[3],
                            float xy[2])
{
  const ARegion *region = gsc->region;
  const View2D *v2d = gsc->v2d;
  const rctf *subrect = gsc->subrect;
  float xyval[2];

  /* sanity checks */
  BLI_assert(gsc->area->spacetype == SPACE_VIEW3D);

  if (flag & GP_STROKE_3DSPACE) {
    if (ED_view3d_project_float_global(region, pt, xyval, V3D_PROJ_TEST_NOP) == V3D_PROJ_RET_OK) {
      xy[0] = xyval[0];
      xy[1] = xyval[1];
    }
    else {
      xy[0] = 0.0f;
      xy[1] = 0.0f;
    }
  }
  else if (flag & GP_STROKE_2DSPACE) {
    float vec[3] = {pt[0], pt[1], 0.0f};
    int t_x, t_y;

    mul_m4_v3(gsc->mat, vec);
    UI_view2d_view_to_region_clip(v2d, vec[0], vec[1], &t_x, &t_y);

    if ((t_x == t_y) && (t_x == V2D_IS_CLIPPED)) {
      /* XXX: Or should we just always use the values as-is? */
      xy[0] = 0.0f;
      xy[1] = 0.0f;
    }
    else {
      xy[0] = float(t_x);
      xy[1] = float(t_y);
    }
  }
  else {
    if (subrect == nullptr) {
      /* normal 3D view (or view space) */
      xy[0] = (pt[0] / 100.0f * region->winx);
      xy[1] = (pt[1] / 100.0f * region->winy);
    }
    else {
      /* camera view, use subrect */
      xy[0] = ((pt[0] / 100.0f) * BLI_rctf_size_x(subrect)) + subrect->xmin;
      xy[1] = ((pt[1] / 100.0f) * BLI_rctf_size_y(subrect)) + subrect->ymin;
    }
  }
}

bool gpencil_point_xy_to_3d(const GP_SpaceConversion *gsc,
                            Scene *scene,
                            const float screen_co[2],
                            float r_out[3])
{
  const RegionView3D *rv3d = static_cast<const RegionView3D *>(gsc->region->regiondata);
  float rvec[3];

  ED_gpencil_drawing_reference_get(scene, gsc->ob, scene->toolsettings->gpencil_v3d_align, rvec);

  float zfac = ED_view3d_calc_zfac(rv3d, rvec);

  float mval_prj[2];

  if (ED_view3d_project_float_global(gsc->region, rvec, mval_prj, V3D_PROJ_TEST_NOP) ==
      V3D_PROJ_RET_OK)
  {
    float dvec[3];
    float xy_delta[2];
    sub_v2_v2v2(xy_delta, mval_prj, screen_co);
    ED_view3d_win_to_delta(gsc->region, xy_delta, zfac, dvec);
    sub_v3_v3v3(r_out, rvec, dvec);

    return true;
  }
  zero_v3(r_out);
  return false;
}

void gpencil_stroke_convertcoords_tpoint(Scene *scene,
                                         ARegion *region,
                                         Object *ob,
                                         const tGPspoint *point2D,
                                         float *depth,
                                         float r_out[3])
{
  ToolSettings *ts = scene->toolsettings;

  if (depth && (*depth == DEPTH_INVALID)) {
    depth = nullptr;
  }

  int mval_i[2];
  round_v2i_v2fl(mval_i, point2D->m_xy);

  if ((depth != nullptr) && ED_view3d_autodist_simple(region, mval_i, r_out, 0, depth)) {
    /* projecting onto 3D-Geometry
     * - nothing more needs to be done here, since view_autodist_simple() has already done it
     */
  }
  else {
    float mval_prj[2];
    float rvec[3];

    /* Current method just converts each point in screen-coordinates to
     * 3D-coordinates using the 3D-cursor as reference.
     */
    ED_gpencil_drawing_reference_get(scene, ob, ts->gpencil_v3d_align, rvec);
    const float zfac = ED_view3d_calc_zfac(static_cast<const RegionView3D *>(region->regiondata),
                                           rvec);

    if (ED_view3d_project_float_global(region, rvec, mval_prj, V3D_PROJ_TEST_NOP) ==
        V3D_PROJ_RET_OK)
    {
      float dvec[3];
      float xy_delta[2];
      sub_v2_v2v2(xy_delta, mval_prj, point2D->m_xy);
      ED_view3d_win_to_delta(region, xy_delta, zfac, dvec);
      sub_v3_v3v3(r_out, rvec, dvec);
    }
    else {
      zero_v3(r_out);
    }
  }
}

void ED_gpencil_drawing_reference_get(const Scene *scene,
                                      const Object *ob,
                                      char align_flag,
                                      float r_vec[3])
{
  const float *fp = scene->cursor.location;

  /* if using a gpencil object at cursor mode, can use the location of the object */
  if (align_flag & GP_PROJECT_VIEWSPACE) {
    if (ob && (ob->type == OB_GPENCIL_LEGACY)) {
      /* fallback (no strokes) - use cursor or object location */
      if (align_flag & GP_PROJECT_CURSOR) {
        /* use 3D-cursor */
        copy_v3_v3(r_vec, fp);
      }
      else {
        /* use object location */
        copy_v3_v3(r_vec, ob->object_to_world().location());
        /* Apply layer offset. */
        bGPdata *gpd = static_cast<bGPdata *>(ob->data);
        bGPDlayer *gpl = BKE_gpencil_layer_active_get(gpd);
        if (gpl != nullptr) {
          add_v3_v3(r_vec, gpl->layer_mat[3]);
        }
      }
    }
  }
  else {
    /* use 3D-cursor */
    copy_v3_v3(r_vec, fp);
  }
}

void ED_gpencil_project_stroke_to_view(bContext *C, bGPDlayer *gpl, bGPDstroke *gps)
{
  Scene *scene = CTX_data_scene(C);
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  Object *ob = CTX_data_active_object(C);
  GP_SpaceConversion gsc = {nullptr};

  bGPDspoint *pt;
  int i;
  float diff_mat[4][4];
  float inverse_diff_mat[4][4];

  /* init space conversion stuff */
  gpencil_point_conversion_init(C, &gsc);

  BKE_gpencil_layer_transform_matrix_get(depsgraph, ob, gpl, diff_mat);
  zero_axis_bias_m4(diff_mat);
  invert_m4_m4(inverse_diff_mat, diff_mat);

  /* Adjust each point */
  for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
    float xy[2];

    bGPDspoint pt2;
    gpencil_point_to_world_space(pt, diff_mat, &pt2);
    gpencil_point_to_xy_fl(&gsc, gps, &pt2, &xy[0], &xy[1]);

    /* Planar - All on same plane parallel to the viewplane */
    gpencil_point_xy_to_3d(&gsc, scene, xy, &pt->x);

    /* Unapply parent corrections */
    mul_m4_v3(inverse_diff_mat, &pt->x);
  }
}

void ED_gpencil_project_stroke_to_plane(const Scene *scene,
                                        const Object *ob,
                                        const RegionView3D *rv3d,
                                        bGPDlayer *gpl,
                                        bGPDstroke *gps,
                                        const float origin[3],
                                        const int axis)
{
  const ToolSettings *ts = scene->toolsettings;
  const View3DCursor *cursor = &scene->cursor;
  float plane_normal[3];
  float vn[3];

  float ray[3];
  float rpoint[3];

  /* Recalculate layer transform matrix. */
  loc_eul_size_to_mat4(gpl->layer_mat, gpl->location, gpl->rotation, gpl->scale);
  invert_m4_m4(gpl->layer_invmat, gpl->layer_mat);

  /* normal vector for a plane locked to axis */
  zero_v3(plane_normal);
  if (axis < 0) {
    /* if the axis is not locked, need a vector to the view direction
     * in order to get the right size of the stroke.
     */
    ED_view3d_global_to_vector(rv3d, origin, plane_normal);
  }
  else if (axis < 3) {
    plane_normal[axis] = 1.0f;
    /* if object, apply object rotation */
    if (ob && (ob->type == OB_GPENCIL_LEGACY)) {
      float mat[4][4];
      copy_m4_m4(mat, ob->object_to_world().ptr());

      /* move origin to cursor */
      if ((ts->gpencil_v3d_align & GP_PROJECT_CURSOR) == 0) {
        if (gpl != nullptr) {
          add_v3_v3(mat[3], gpl->location);
        }
      }
      if (ts->gpencil_v3d_align & GP_PROJECT_CURSOR) {
        copy_v3_v3(mat[3], cursor->location);
      }

      mul_mat3_m4_v3(mat, plane_normal);
    }

    if ((gpl != nullptr) && (ts->gp_sculpt.lock_axis != GP_LOCKAXIS_CURSOR)) {
      mul_mat3_m4_v3(gpl->layer_mat, plane_normal);
    }
  }
  else {
    const float scale[3] = {1.0f, 1.0f, 1.0f};
    plane_normal[2] = 1.0f;
    float mat[4][4];
    loc_eul_size_to_mat4(mat, cursor->location, cursor->rotation_euler, scale);
    mul_mat3_m4_v3(mat, plane_normal);
  }

  /* Reproject the points in the plane */
  for (int i = 0; i < gps->totpoints; i++) {
    bGPDspoint *pt = &gps->points[i];

    /* get a vector from the point with the current view direction of the viewport */
    ED_view3d_global_to_vector(rv3d, &pt->x, vn);

    /* calculate line extreme point to create a ray that cross the plane */
    mul_v3_fl(vn, -50.0f);
    add_v3_v3v3(ray, &pt->x, vn);

    /* if the line never intersect, the point is not changed */
    if (isect_line_plane_v3(rpoint, &pt->x, ray, origin, plane_normal)) {
      copy_v3_v3(&pt->x, rpoint);
    }
  }
}

void ED_gpencil_stroke_reproject(Depsgraph *depsgraph,
                                 const GP_SpaceConversion *gsc,
                                 SnapObjectContext *sctx,
                                 bGPDlayer *gpl,
                                 bGPDframe *gpf,
                                 bGPDstroke *gps,
                                 const eGP_ReprojectModes mode,
                                 const bool keep_original,
                                 const float offset)
{
  ToolSettings *ts = gsc->scene->toolsettings;
  ARegion *region = gsc->region;
  RegionView3D *rv3d = static_cast<RegionView3D *>(region->regiondata);

  /* Recalculate layer transform matrix. */
  loc_eul_size_to_mat4(gpl->layer_mat, gpl->location, gpl->rotation, gpl->scale);
  invert_m4_m4(gpl->layer_invmat, gpl->layer_mat);

  float diff_mat[4][4], inverse_diff_mat[4][4];
  BKE_gpencil_layer_transform_matrix_get(depsgraph, gsc->ob, gpl, diff_mat);
  zero_axis_bias_m4(diff_mat);
  invert_m4_m4(inverse_diff_mat, diff_mat);

  float origin[3];
  if (mode != GP_REPROJECT_CURSOR) {
    ED_gpencil_drawing_reference_get(gsc->scene, gsc->ob, ts->gpencil_v3d_align, origin);
  }
  else {
    copy_v3_v3(origin, gsc->scene->cursor.location);
  }

  bGPDspoint *pt;
  int i;

  /* If keep original, do a copy. */
  bGPDstroke *gps_active = gps;
  /* if duplicate, deselect all points. */
  if (keep_original) {
    gps_active = BKE_gpencil_stroke_duplicate(gps, true, true);
    gps_active->flag &= ~GP_STROKE_SELECT;
    BKE_gpencil_stroke_select_index_reset(gps_active);
    for (i = 0, pt = gps_active->points; i < gps_active->totpoints; i++, pt++) {
      pt->flag &= ~GP_SPOINT_SELECT;
    }
    /* Add to frame. */
    BLI_addtail(&gpf->strokes, gps_active);
  }

  /* Adjust each point */
  for (i = 0, pt = gps_active->points; i < gps_active->totpoints; i++, pt++) {
    float xy[2];

    /* 3D to Screen-space */
    /* NOTE: We can't use gpencil_point_to_xy() here because that uses ints for the screen-space
     * coordinates, resulting in lost precision, which in turn causes stair-stepping
     * artifacts in the final points. */

    bGPDspoint pt2;
    gpencil_point_to_world_space(pt, diff_mat, &pt2);
    gpencil_point_to_xy_fl(gsc, gps_active, &pt2, &xy[0], &xy[1]);

    /* Project stroke in one axis */
    if (ELEM(mode, GP_REPROJECT_FRONT, GP_REPROJECT_SIDE, GP_REPROJECT_TOP, GP_REPROJECT_CURSOR)) {
      int axis = 0;
      switch (mode) {
        case GP_REPROJECT_FRONT: {
          axis = 1;
          break;
        }
        case GP_REPROJECT_SIDE: {
          axis = 0;
          break;
        }
        case GP_REPROJECT_TOP: {
          axis = 2;
          break;
        }
        case GP_REPROJECT_CURSOR: {
          axis = 3;
          break;
        }
        default: {
          axis = 1;
          break;
        }
      }

      ED_gpencil_project_point_to_plane(gsc->scene, gsc->ob, gpl, rv3d, origin, axis, &pt2);

      copy_v3_v3(&pt->x, &pt2.x);

      /* apply parent again */
      gpencil_world_to_object_space_point(depsgraph, gsc->ob, gpl, pt);
    }
    /* Project screen-space back to 3D space (from current perspective)
     * so that all points have been treated the same way. */
    else if (mode == GP_REPROJECT_VIEW) {
      /* Planar - All on same plane parallel to the view-plane. */
      gpencil_point_xy_to_3d(gsc, gsc->scene, xy, &pt->x);
    }
    else {
      /* Geometry - Snap to surfaces of visible geometry */
      float ray_start[3];
      float ray_normal[3];
      /* magic value for initial depth copied from the default
       * value of Python's Scene.ray_cast function
       */
      float depth = 1.70141e+38f;
      float location[3] = {0.0f, 0.0f, 0.0f};
      float normal[3] = {0.0f, 0.0f, 0.0f};

      BLI_assert(gps->flag & GP_STROKE_3DSPACE);
      BLI_assert(gsc->area && gsc->area->spacetype == SPACE_VIEW3D);
      const View3D *v3d = static_cast<const View3D *>(gsc->area->spacedata.first);
      ED_view3d_win_to_ray_clipped(
          depsgraph, region, v3d, xy, &ray_start[0], &ray_normal[0], true);
      SnapObjectParams params{};
      params.snap_target_select = SCE_SNAP_TARGET_ALL;
      if (ED_transform_snap_object_project_ray(sctx,
                                               depsgraph,
                                               v3d,
                                               &params,
                                               &ray_start[0],
                                               &ray_normal[0],
                                               &depth,
                                               &location[0],
                                               &normal[0]))
      {
        /* Apply offset over surface. */
        float normal_vector[3];
        sub_v3_v3v3(normal_vector, ray_start, location);
        normalize_v3(normal_vector);
        mul_v3_fl(normal_vector, offset);

        add_v3_v3v3(&pt->x, location, normal_vector);
      }
      else {
        /* Default to planar */
        gpencil_point_xy_to_3d(gsc, gsc->scene, xy, &pt->x);
      }
    }

    /* Unapply parent corrections */
    if (!ELEM(mode, GP_REPROJECT_FRONT, GP_REPROJECT_SIDE, GP_REPROJECT_TOP)) {
      mul_m4_v3(inverse_diff_mat, &pt->x);
    }
  }
}

void ED_gpencil_project_point_to_plane(const Scene *scene,
                                       const Object *ob,
                                       bGPDlayer *gpl,
                                       const RegionView3D *rv3d,
                                       const float origin[3],
                                       const int axis,
                                       bGPDspoint *pt)
{
  const ToolSettings *ts = scene->toolsettings;
  const View3DCursor *cursor = &scene->cursor;
  float plane_normal[3];
  float vn[3];

  float ray[3];
  float rpoint[3];

  /* normal vector for a plane locked to axis */
  zero_v3(plane_normal);
  if (axis < 0) {
    /* if the axis is not locked, need a vector to the view direction
     * in order to get the right size of the stroke.
     */
    ED_view3d_global_to_vector(rv3d, origin, plane_normal);
  }
  else if (axis < 3) {
    plane_normal[axis] = 1.0f;
    /* if object, apply object rotation */
    if (ob && (ob->type == OB_GPENCIL_LEGACY)) {
      float mat[4][4];
      copy_m4_m4(mat, ob->object_to_world().ptr());
      if ((ts->gpencil_v3d_align & GP_PROJECT_CURSOR) == 0) {
        if (gpl != nullptr) {
          add_v3_v3(mat[3], gpl->location);
        }
      }

      /* move origin to cursor */
      if (ts->gpencil_v3d_align & GP_PROJECT_CURSOR) {
        copy_v3_v3(mat[3], cursor->location);
      }

      mul_mat3_m4_v3(mat, plane_normal);
      /* Apply layer rotation (local transform). */
      if ((gpl != nullptr) && (ts->gp_sculpt.lock_axis != GP_LOCKAXIS_CURSOR)) {
        mul_mat3_m4_v3(gpl->layer_mat, plane_normal);
      }
    }
  }
  else {
    const float scale[3] = {1.0f, 1.0f, 1.0f};
    plane_normal[2] = 1.0f;
    float mat[4][4];
    loc_eul_size_to_mat4(mat, cursor->location, cursor->rotation_euler, scale);

    /* move origin to object */
    if ((ts->gpencil_v3d_align & GP_PROJECT_CURSOR) == 0) {
      copy_v3_v3(mat[3], ob->object_to_world().location());
    }

    mul_mat3_m4_v3(mat, plane_normal);
  }

  /* Reproject the points in the plane */
  /* get a vector from the point with the current view direction of the viewport */
  ED_view3d_global_to_vector(rv3d, &pt->x, vn);

  /* calculate line extreme point to create a ray that cross the plane */
  mul_v3_fl(vn, -50.0f);
  add_v3_v3v3(ray, &pt->x, vn);

  /* if the line never intersect, the point is not changed */
  if (isect_line_plane_v3(rpoint, &pt->x, ray, origin, plane_normal)) {
    copy_v3_v3(&pt->x, rpoint);
  }
}

/* ******************************************************** */
/* Stroke Operations */

/* XXX: Check if these functions duplicate stuff in blenkernel,
 * and/or whether we should just deduplicate. */

void gpencil_subdivide_stroke(bGPdata *gpd, bGPDstroke *gps, const int subdivide)
{
  bGPDspoint *temp_points;
  int totnewpoints, oldtotpoints;
  int i2;

  /* loop as many times as levels */
  for (int s = 0; s < subdivide; s++) {
    totnewpoints = gps->totpoints - 1;
    /* duplicate points in a temp area */
    temp_points = static_cast<bGPDspoint *>(MEM_dupallocN(gps->points));
    oldtotpoints = gps->totpoints;

    /* resize the points arrays */
    gps->totpoints += totnewpoints;
    gps->points = static_cast<bGPDspoint *>(
        MEM_recallocN(gps->points, sizeof(*gps->points) * gps->totpoints));
    if (gps->dvert != nullptr) {
      gps->dvert = static_cast<MDeformVert *>(
          MEM_recallocN(gps->dvert, sizeof(*gps->dvert) * gps->totpoints));
    }

    /* move points from last to first to new place */
    i2 = gps->totpoints - 1;
    for (int i = oldtotpoints - 1; i > 0; i--) {
      bGPDspoint *pt = &temp_points[i];
      bGPDspoint *pt_final = &gps->points[i2];

      copy_v3_v3(&pt_final->x, &pt->x);
      pt_final->pressure = pt->pressure;
      pt_final->strength = pt->strength;
      pt_final->time = pt->time;
      pt_final->flag = pt->flag;
      pt_final->uv_fac = pt->uv_fac;
      pt_final->uv_rot = pt->uv_rot;
      copy_v4_v4(pt_final->vert_color, pt->vert_color);

      if (gps->dvert != nullptr) {
        MDeformVert *dvert = &gps->dvert[i];
        MDeformVert *dvert_final = &gps->dvert[i2];

        dvert_final->totweight = dvert->totweight;
        dvert_final->dw = dvert->dw;
      }

      i2 -= 2;
    }
    /* interpolate mid points */
    i2 = 1;
    for (int i = 0; i < oldtotpoints - 1; i++) {
      bGPDspoint *pt = &temp_points[i];
      bGPDspoint *next = &temp_points[i + 1];
      bGPDspoint *pt_final = &gps->points[i2];

      /* add a half way point */
      interp_v3_v3v3(&pt_final->x, &pt->x, &next->x, 0.5f);
      pt_final->pressure = interpf(pt->pressure, next->pressure, 0.5f);
      pt_final->strength = interpf(pt->strength, next->strength, 0.5f);
      CLAMP(pt_final->strength, GPENCIL_STRENGTH_MIN, 1.0f);
      pt_final->time = interpf(pt->time, next->time, 0.5f);
      pt_final->uv_fac = interpf(pt->uv_fac, next->uv_fac, 0.5f);
      pt_final->uv_rot = interpf(pt->uv_rot, next->uv_rot, 0.5f);
      interp_v4_v4v4(pt_final->vert_color, pt->vert_color, next->vert_color, 0.5f);

      if (gps->dvert != nullptr) {
        MDeformVert *dvert_final = &gps->dvert[i2];
        dvert_final->totweight = 0;
        dvert_final->dw = nullptr;
      }

      i2 += 2;
    }

    MEM_SAFE_FREE(temp_points);

    /* move points to smooth stroke */
    /* duplicate points in a temp area with the new subdivide data */
    temp_points = static_cast<bGPDspoint *>(MEM_dupallocN(gps->points));

    /* extreme points are not changed */
    for (int i = 0; i < gps->totpoints - 2; i++) {
      bGPDspoint *pt = &temp_points[i];
      bGPDspoint *next = &temp_points[i + 1];
      bGPDspoint *pt_final = &gps->points[i + 1];

      /* move point */
      interp_v3_v3v3(&pt_final->x, &pt->x, &next->x, 0.5f);
    }
    /* free temp memory */
    MEM_SAFE_FREE(temp_points);
  }
  /* Calc geometry data. */
  BKE_gpencil_stroke_geometry_update(gpd, gps);
}

void ED_gpencil_reset_layers_parent(Depsgraph *depsgraph, Object *obact, bGPdata *gpd)
{
  bGPDspoint *pt;
  int i;
  float diff_mat[4][4];
  float cur_mat[4][4];
  float gpl_loc[3];
  zero_v3(gpl_loc);

  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    if (gpl->parent != nullptr) {
      /* calculate new matrix */
      if (ELEM(gpl->partype, PAROBJECT, PARSKEL)) {
        invert_m4_m4(cur_mat, gpl->parent->object_to_world().ptr());
        copy_v3_v3(gpl_loc, obact->object_to_world().location());
      }
      else if (gpl->partype == PARBONE) {
        bPoseChannel *pchan = BKE_pose_channel_find_name(gpl->parent->pose, gpl->parsubstr);
        if (pchan) {
          float tmp_mat[4][4];
          mul_m4_m4m4(tmp_mat, gpl->parent->object_to_world().ptr(), pchan->pose_mat);
          invert_m4_m4(cur_mat, tmp_mat);
          copy_v3_v3(gpl_loc, obact->object_to_world().location());
        }
      }

      /* only redo if any change */
      if (!equals_m4m4(gpl->inverse, cur_mat)) {
        /* first apply current transformation to all strokes */
        BKE_gpencil_layer_transform_matrix_get(depsgraph, obact, gpl, diff_mat);
        /* undo local object */
        sub_v3_v3(diff_mat[3], gpl_loc);

        LISTBASE_FOREACH (bGPDframe *, gpf, &gpl->frames) {
          LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
            for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
              mul_m4_v3(diff_mat, &pt->x);
            }
          }
        }
        /* set new parent matrix */
        copy_m4_m4(gpl->inverse, cur_mat);
      }
    }
  }
}
/* ******************************************************** */
/* GP Object Stuff */

Object *ED_gpencil_add_object(bContext *C, const float loc[3], ushort local_view_bits)
{
  const float rot[3] = {0.0f};

  Object *ob = blender::ed::object::add_type(
      C, OB_GPENCIL_LEGACY, nullptr, loc, rot, false, local_view_bits);

  /* create default brushes and colors */
  ED_gpencil_add_defaults(C, ob);

  return ob;
}

void ED_gpencil_add_defaults(bContext *C, Object *ob)
{
  Main *bmain = CTX_data_main(C);
  ToolSettings *ts = CTX_data_tool_settings(C);

  BKE_paint_ensure(bmain, ts, (Paint **)&ts->gp_paint);
  Paint *paint = &ts->gp_paint->paint;
  Brush *brush = BKE_paint_brush(paint);
  /* if not exist, create a new one */
  if ((brush == nullptr) || (brush->gpencil_settings == nullptr)) {
    /* create new brushes */
    BKE_brush_gpencil_paint_presets(bmain, ts, true);
  }

  /* ensure a color exists and is assigned to object */
  BKE_gpencil_object_material_ensure_from_active_input_toolsettings(bmain, ob, ts);

  /* Ensure multi-frame falloff curve. */
  if (ts->gp_sculpt.cur_falloff == nullptr) {
    ts->gp_sculpt.cur_falloff = BKE_curvemapping_add(1, 0.0f, 0.0f, 1.0f, 1.0f);
    CurveMapping *gp_falloff_curve = ts->gp_sculpt.cur_falloff;
    BKE_curvemapping_init(gp_falloff_curve);
    BKE_curvemap_reset(gp_falloff_curve->cm,
                       &gp_falloff_curve->clipr,
                       CURVE_PRESET_GAUSS,
                       CURVEMAP_SLOPE_POSITIVE);
  }
}

/* ******************************************************** */
/* Vertex Groups */

void ED_gpencil_vgroup_assign(bContext *C, Object *ob, float weight)
{
  bGPdata *gpd = (bGPdata *)ob->data;
  const bool is_multiedit = bool(GPENCIL_MULTIEDIT_SESSIONS_ON(gpd));
  const int def_nr = gpd->vertex_group_active_index - 1;
  if (!BLI_findlink(&gpd->vertex_group_names, def_nr)) {
    return;
  }

  CTX_DATA_BEGIN (C, bGPDlayer *, gpl, editable_gpencil_layers) {
    bGPDframe *init_gpf = static_cast<bGPDframe *>((is_multiedit) ? gpl->frames.first :
                                                                    gpl->actframe);
    for (bGPDframe *gpf = init_gpf; gpf; gpf = gpf->next) {
      if ((gpf == gpl->actframe) || ((gpf->flag & GP_FRAME_SELECT) && (is_multiedit))) {
        if (gpf == nullptr) {
          continue;
        }

        LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {

          /* skip strokes that are invalid for current view */
          if (ED_gpencil_stroke_can_use(C, gps) == false) {
            continue;
          }

          if (gps->flag & GP_STROKE_SELECT) {
            /* verify the weight array is created */
            BKE_gpencil_dvert_ensure(gps);

            for (int i = 0; i < gps->totpoints; i++) {
              bGPDspoint *pt = &gps->points[i];
              MDeformVert *dvert = &gps->dvert[i];
              if (pt->flag & GP_SPOINT_SELECT) {
                MDeformWeight *dw = BKE_defvert_ensure_index(dvert, def_nr);
                if (dw) {
                  dw->weight = weight;
                }
              }
            }
          }
        }
      }

      /* If not multi-edit, exit loop. */
      if (!is_multiedit) {
        break;
      }
    }
  }
  CTX_DATA_END;
}

void ED_gpencil_vgroup_remove(bContext *C, Object *ob)
{
  bGPdata *gpd = (bGPdata *)ob->data;
  const bool is_multiedit = bool(GPENCIL_MULTIEDIT_SESSIONS_ON(gpd));
  const int def_nr = gpd->vertex_group_active_index - 1;
  if (!BLI_findlink(&gpd->vertex_group_names, def_nr)) {
    return;
  }

  CTX_DATA_BEGIN (C, bGPDlayer *, gpl, editable_gpencil_layers) {
    bGPDframe *init_gpf = static_cast<bGPDframe *>((is_multiedit) ? gpl->frames.first :
                                                                    gpl->actframe);

    for (bGPDframe *gpf = init_gpf; gpf; gpf = gpf->next) {
      if ((gpf == gpl->actframe) || ((gpf->flag & GP_FRAME_SELECT) && (is_multiedit))) {
        if (gpf == nullptr) {
          continue;
        }

        LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
          /* skip strokes that are invalid for current view */
          if (ED_gpencil_stroke_can_use(C, gps) == false) {
            continue;
          }

          for (int i = 0; i < gps->totpoints; i++) {
            bGPDspoint *pt = &gps->points[i];
            if (gps->dvert == nullptr) {
              continue;
            }
            MDeformVert *dvert = &gps->dvert[i];

            if ((pt->flag & GP_SPOINT_SELECT) && (dvert->totweight > 0)) {
              MDeformWeight *dw = BKE_defvert_find_index(dvert, def_nr);
              if (dw != nullptr) {
                BKE_defvert_remove_group(dvert, dw);
              }
            }
          }
        }
      }

      /* If not multi-edit, exit loop. */
      if (!is_multiedit) {
        break;
      }
    }
  }
  CTX_DATA_END;
}

void ED_gpencil_vgroup_select(bContext *C, Object *ob)
{
  bGPdata *gpd = (bGPdata *)ob->data;
  const bool is_multiedit = bool(GPENCIL_MULTIEDIT_SESSIONS_ON(gpd));
  const int def_nr = gpd->vertex_group_active_index - 1;
  if (!BLI_findlink(&gpd->vertex_group_names, def_nr)) {
    return;
  }

  CTX_DATA_BEGIN (C, bGPDlayer *, gpl, editable_gpencil_layers) {
    bGPDframe *init_gpf = static_cast<bGPDframe *>((is_multiedit) ? gpl->frames.first :
                                                                    gpl->actframe);
    for (bGPDframe *gpf = init_gpf; gpf; gpf = gpf->next) {
      if ((gpf == gpl->actframe) || ((gpf->flag & GP_FRAME_SELECT) && (is_multiedit))) {
        if (gpf == nullptr) {
          continue;
        }

        LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
          /* skip strokes that are invalid for current view */
          if (ED_gpencil_stroke_can_use(C, gps) == false) {
            continue;
          }

          for (int i = 0; i < gps->totpoints; i++) {
            bGPDspoint *pt = &gps->points[i];
            if (gps->dvert == nullptr) {
              continue;
            }
            MDeformVert *dvert = &gps->dvert[i];

            if (BKE_defvert_find_index(dvert, def_nr) != nullptr) {
              pt->flag |= GP_SPOINT_SELECT;
              gps->flag |= GP_STROKE_SELECT;
            }
          }

          if (gps->flag & GP_STROKE_SELECT) {
            BKE_gpencil_stroke_select_index_set(gpd, gps);
          }
        }
      }

      /* If not multi-edit, exit loop. */
      if (!is_multiedit) {
        break;
      }
    }
  }
  CTX_DATA_END;
}

void ED_gpencil_vgroup_deselect(bContext *C, Object *ob)
{
  bGPdata *gpd = (bGPdata *)ob->data;
  const bool is_multiedit = bool(GPENCIL_MULTIEDIT_SESSIONS_ON(gpd));
  const int def_nr = gpd->vertex_group_active_index - 1;
  if (!BLI_findlink(&gpd->vertex_group_names, def_nr)) {
    return;
  }

  CTX_DATA_BEGIN (C, bGPDlayer *, gpl, editable_gpencil_layers) {
    bGPDframe *init_gpf = static_cast<bGPDframe *>((is_multiedit) ? gpl->frames.first :
                                                                    gpl->actframe);
    for (bGPDframe *gpf = init_gpf; gpf; gpf = gpf->next) {
      if ((gpf == gpl->actframe) || ((gpf->flag & GP_FRAME_SELECT) && (is_multiedit))) {
        if (gpf == nullptr) {
          continue;
        }

        LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {

          /* skip strokes that are invalid for current view */
          if (ED_gpencil_stroke_can_use(C, gps) == false) {
            continue;
          }

          for (int i = 0; i < gps->totpoints; i++) {
            bGPDspoint *pt = &gps->points[i];
            if (gps->dvert == nullptr) {
              continue;
            }
            MDeformVert *dvert = &gps->dvert[i];

            if (BKE_defvert_find_index(dvert, def_nr) != nullptr) {
              pt->flag &= ~GP_SPOINT_SELECT;
            }
          }
        }
      }

      /* If not multi-edit, exit loop. */
      if (!is_multiedit) {
        break;
      }
    }
  }
  CTX_DATA_END;
}

/* ******************************************************** */
/* Cursor drawing */

/* check if cursor is in drawing region */
static bool gpencil_check_cursor_region(bContext *C, const int mval_i[2])
{
  ARegion *region = CTX_wm_region(C);
  ScrArea *area = CTX_wm_area(C);
  Object *ob = CTX_data_active_object(C);

  if ((ob == nullptr) || ((ob->mode & OB_MODE_ALL_PAINT_GPENCIL) == 0)) {
    return false;
  }

  /* TODO: add more space-types. */
  if (!ELEM(area->spacetype, SPACE_VIEW3D)) {
    return false;
  }
  if ((region) && (region->regiontype != RGN_TYPE_WINDOW)) {
    return false;
  }
  if (region) {
    return BLI_rcti_isect_pt_v(&region->winrct, mval_i);
  }
  return false;
}

void ED_gpencil_brush_draw_eraser(Brush *brush, int x, int y)
{
  short radius = short(brush->size);

  GPUVertFormat *format = immVertexFormat();
  const uint shdr_pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

  GPU_line_smooth(true);
  GPU_blend(GPU_BLEND_ALPHA);

  immUniformColor4ub(255, 100, 100, 20);
  imm_draw_circle_fill_2d(shdr_pos, x, y, radius, 40);

  immUnbindProgram();

  immBindBuiltinProgram(GPU_SHADER_3D_LINE_DASHED_UNIFORM_COLOR);

  float viewport_size[4];
  GPU_viewport_size_get_f(viewport_size);
  immUniform2f("viewport_size", viewport_size[2], viewport_size[3]);

  immUniformColor4f(1.0f, 0.39f, 0.39f, 0.78f);
  immUniform1i("colors_len", 0); /* "simple" mode */
  immUniform1f("dash_width", 12.0f);
  immUniform1f("udash_factor", 0.5f);

  imm_draw_circle_wire_2d(shdr_pos,
                          x,
                          y,
                          radius,
                          /* XXX Dashed shader gives bad results with sets of small segments
                           * currently, temp hack around the issue. :( */
                          max_ii(8, radius / 2)); /* was fixed 40 */

  immUnbindProgram();

  GPU_blend(GPU_BLEND_NONE);
  GPU_line_smooth(false);
}

static bool gpencil_brush_cursor_poll(bContext *C)
{
  if (WM_toolsystem_active_tool_is_brush(C)) {
    return true;
  }
  return false;
}

float ED_gpencil_cursor_radius(bContext *C, int x, int y)
{
  Scene *scene = CTX_data_scene(C);
  Object *ob = CTX_data_active_object(C);
  ARegion *region = CTX_wm_region(C);
  Brush *brush = BKE_paint_brush(&scene->toolsettings->gp_paint->paint);
  bGPdata *gpd = ED_gpencil_data_get_active(C);

  /* Show brush size. */
  tGPspoint point2D;
  float p1[3];
  float p2[3];
  float distance;
  float radius = 2.0f;

  if (ELEM(nullptr, gpd, brush)) {
    return radius;
  }

  /* Strokes in screen space or world space? */
  if ((gpd->flag & GP_DATA_STROKE_KEEPTHICKNESS) != 0) {
    /* In screen space the cursor radius matches the brush size. */
    radius = float(brush->size) * 0.5f;
  }
  else {
    /* To calculate the brush size in world space, we have to establish the zoom level.
     * For this we take two 2D screen coordinates with a fixed offset,
     * convert them to 3D coordinates and measure the offset distance in 3D.
     * A small distance means a high zoom level. */
    point2D.m_xy[0] = float(x);
    point2D.m_xy[1] = float(y);
    gpencil_stroke_convertcoords_tpoint(scene, region, ob, &point2D, nullptr, p1);
    point2D.m_xy[0] = float(x + 64);
    gpencil_stroke_convertcoords_tpoint(scene, region, ob, &point2D, nullptr, p2);
    /* Clip extreme zoom level (and avoid division by zero). */
    distance = std::max(len_v3v3(p1, p2), 0.001f);

    /* Handle layer thickness change. */
    float brush_size = float(brush->size);
    bGPDlayer *gpl = BKE_gpencil_layer_active_get(gpd);
    if (gpl != nullptr) {
      brush_size = std::max(1.0f, brush_size + gpl->line_change);
    }

    /* Convert the 3D offset distance to a brush radius. */
    radius = (1 / distance) * 2.0f * gpd->pixfactor * (brush_size / 64);
  }
  return radius;
}

float ED_gpencil_radial_control_scale(bContext *C,
                                      Brush *brush,
                                      float initial_value,
                                      const int mval[2])
{
  float scale_fac = 1.0f;
  if ((brush && brush->gpencil_settings) && (brush->ob_mode == OB_MODE_PAINT_GPENCIL_LEGACY) &&
      (brush->gpencil_tool == GPAINT_TOOL_DRAW))
  {
    float cursor_radius = ED_gpencil_cursor_radius(C, mval[0], mval[1]);
    scale_fac = max_ff(cursor_radius, 1.0f) / max_ff(initial_value, 1.0f);
  }

  return scale_fac;
}

/**
 * Helper callback for drawing the cursor itself.
 */
static void gpencil_brush_cursor_draw(bContext *C, int x, int y, void *customdata)
{
  Scene *scene = CTX_data_scene(C);
  ToolSettings *ts = scene->toolsettings;
  Object *ob = CTX_data_active_object(C);
  ARegion *region = CTX_wm_region(C);
  Paint *paint = BKE_paint_get_active_from_context(C);

  bGPdata *gpd = ED_gpencil_data_get_active(C);
  Brush *brush = nullptr;
  Material *ma = nullptr;
  MaterialGPencilStyle *gp_style = nullptr;
  float *last_mouse_position = static_cast<float *>(customdata);

  /* default radius and color */
  float color[3] = {1.0f, 1.0f, 1.0f};
  float darkcolor[3];
  float radius = 3.0f;

  const int mval_i[2] = {x, y};
  /* Check if cursor is in drawing region and has valid data-block. */
  if (!gpencil_check_cursor_region(C, mval_i) || (gpd == nullptr)) {
    return;
  }

  /* for paint use paint brush size and color */
  if (gpd->flag & GP_DATA_STROKE_PAINTMODE) {
    brush = BKE_paint_brush(&scene->toolsettings->gp_paint->paint);
    if ((brush == nullptr) || (brush->gpencil_settings == nullptr)) {
      return;
    }

    /* while drawing hide */
    if ((gpd->runtime.sbuffer_used > 0) &&
        ((brush->gpencil_settings->flag & GP_BRUSH_STABILIZE_MOUSE) == 0) &&
        ((brush->gpencil_settings->flag & GP_BRUSH_STABILIZE_MOUSE_TEMP) == 0))
    {
      return;
    }

    if ((paint->flags & PAINT_SHOW_BRUSH) == 0) {
      return;
    }

    /* eraser has special shape and use a different shader program */
    if (brush->gpencil_tool == GPAINT_TOOL_ERASE) {
      ED_gpencil_brush_draw_eraser(brush, x, y);
      return;
    }

    /* get current drawing color */
    ma = BKE_gpencil_object_material_from_brush_get(ob, brush);

    if (ma) {
      gp_style = ma->gp_style;

      /* Follow user settings for the size of the draw cursor:
       * - Fixed size, or
       * - Brush size (i.e. stroke thickness)
       */
      if ((gp_style) && GPENCIL_PAINT_MODE(gpd) &&
          ((brush->gpencil_settings->flag & GP_BRUSH_STABILIZE_MOUSE) == 0) &&
          ((brush->gpencil_settings->flag & GP_BRUSH_STABILIZE_MOUSE_TEMP) == 0) &&
          (brush->gpencil_tool == GPAINT_TOOL_DRAW))
      {

        const bool is_vertex_stroke =
            (GPENCIL_USE_VERTEX_COLOR_STROKE(ts, brush) &&
             (brush->gpencil_settings->brush_draw_mode != GP_BRUSH_MODE_MATERIAL)) ||
            (!GPENCIL_USE_VERTEX_COLOR_STROKE(ts, brush) &&
             (brush->gpencil_settings->brush_draw_mode == GP_BRUSH_MODE_VERTEXCOLOR));

        /* Strokes in screen space or world space? */
        if ((gpd->flag & GP_DATA_STROKE_KEEPTHICKNESS) != 0) {
          /* In screen space the cursor radius matches the brush size. */
          radius = float(brush->size) * 0.5f;
        }
        else {
          radius = ED_gpencil_cursor_radius(C, x, y);
        }

        copy_v3_v3(color, is_vertex_stroke ? brush->rgb : gp_style->stroke_rgba);
      }
      else {
        /* Only Tint tool must show big cursor. */
        if (brush->gpencil_tool == GPAINT_TOOL_TINT) {
          radius = brush->size;
          copy_v3_v3(color, brush->rgb);
        }
        else {
          radius = 5.0f;
          copy_v3_v3(color, brush->add_col);
        }
      }
    }
  }

  /* Sculpt use sculpt brush size */
  if (GPENCIL_SCULPT_MODE(gpd)) {
    brush = BKE_paint_brush(&scene->toolsettings->gp_sculptpaint->paint);
    if ((brush == nullptr) || (brush->gpencil_settings == nullptr)) {
      return;
    }
    if ((paint->flags & PAINT_SHOW_BRUSH) == 0) {
      return;
    }

    radius = brush->size;
    if (brush->gpencil_settings->sculpt_flag & (GP_SCULPT_FLAG_INVERT | GP_SCULPT_FLAG_TMP_INVERT))
    {
      copy_v3_v3(color, brush->sub_col);
    }
    else {
      copy_v3_v3(color, brush->add_col);
    }
  }

  /* Weight Paint */
  if (GPENCIL_WEIGHT_MODE(gpd)) {
    brush = BKE_paint_brush(&scene->toolsettings->gp_weightpaint->paint);
    if ((brush == nullptr) || (brush->gpencil_settings == nullptr)) {
      return;
    }
    if ((paint->flags & PAINT_SHOW_BRUSH) == 0) {
      return;
    }

    radius = brush->size;
    if (brush->gpencil_settings->sculpt_flag & (GP_SCULPT_FLAG_INVERT | GP_SCULPT_FLAG_TMP_INVERT))
    {
      copy_v3_v3(color, brush->sub_col);
    }
    else {
      copy_v3_v3(color, brush->add_col);
    }
  }

  /* For Vertex Paint use brush size. */
  if (GPENCIL_VERTEX_MODE(gpd)) {
    brush = BKE_paint_brush(&scene->toolsettings->gp_vertexpaint->paint);
    if ((brush == nullptr) || (brush->gpencil_settings == nullptr)) {
      return;
    }
    if ((paint->flags & PAINT_SHOW_BRUSH) == 0) {
      return;
    }

    radius = brush->size;
    copy_v3_v3(color, brush->rgb);
  }

  /* draw icon */
  GPUVertFormat *format = immVertexFormat();
  uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

  GPU_line_smooth(true);
  GPU_blend(GPU_BLEND_ALPHA);

  /* Inner Ring: Color from UI panel */
  immUniformColor4f(color[0], color[1], color[2], 0.8f);
  imm_draw_circle_wire_2d(pos, x, y, radius, 40);

  /* Outer Ring: Dark color for contrast on light backgrounds (e.g. gray on white) */
  mul_v3_v3fl(darkcolor, color, 0.40f);
  immUniformColor4f(darkcolor[0], darkcolor[1], darkcolor[2], 0.8f);
  imm_draw_circle_wire_2d(pos, x, y, radius + 1, 40);

  GPU_blend(GPU_BLEND_NONE);
  GPU_line_smooth(false);

  /* Draw line for lazy mouse */
  if ((last_mouse_position) && (brush->gpencil_settings->flag & GP_BRUSH_STABILIZE_MOUSE_TEMP)) {
    GPU_line_smooth(true);
    GPU_blend(GPU_BLEND_ALPHA);

    copy_v3_v3(color, brush->add_col);
    immUniformColor4f(color[0], color[1], color[2], 0.8f);

    immBegin(GPU_PRIM_LINES, 2);
    immVertex2f(pos, x, y);
    immVertex2f(pos,
                last_mouse_position[0] + region->winrct.xmin,
                last_mouse_position[1] + region->winrct.ymin);
    immEnd();

    GPU_blend(GPU_BLEND_NONE);
    GPU_line_smooth(false);
  }

  immUnbindProgram();
}

void ED_gpencil_toggle_brush_cursor(bContext *C, bool enable, void *customdata)
{
  Scene *scene = CTX_data_scene(C);
  GP_Sculpt_Settings *gset = &scene->toolsettings->gp_sculpt;
  float *lastpost = static_cast<float *>(customdata);

  if (gset->paintcursor && !enable) {
    /* clear cursor */
    WM_paint_cursor_end(static_cast<wmPaintCursor *>(gset->paintcursor));
    gset->paintcursor = nullptr;
  }
  else if (enable) {
    /* in some situations cursor could be duplicated, so it is better disable first if exist */
    if (gset->paintcursor) {
      /* clear cursor */
      WM_paint_cursor_end(static_cast<wmPaintCursor *>(gset->paintcursor));
      gset->paintcursor = nullptr;
    }
    /* enable cursor */
    gset->paintcursor = WM_paint_cursor_activate(SPACE_TYPE_ANY,
                                                 RGN_TYPE_ANY,
                                                 gpencil_brush_cursor_poll,
                                                 gpencil_brush_cursor_draw,
                                                 (lastpost) ? customdata : nullptr);
  }
}

void ED_gpencil_setup_modes(bContext *C, bGPdata *gpd, int newmode)
{
  if (!gpd) {
    return;
  }

  switch (newmode) {
    case OB_MODE_EDIT_GPENCIL_LEGACY:
      gpd->flag |= GP_DATA_STROKE_EDITMODE;
      gpd->flag &= ~GP_DATA_STROKE_PAINTMODE;
      gpd->flag &= ~GP_DATA_STROKE_SCULPTMODE;
      gpd->flag &= ~GP_DATA_STROKE_WEIGHTMODE;
      gpd->flag &= ~GP_DATA_STROKE_VERTEXMODE;
      ED_gpencil_toggle_brush_cursor(C, false, nullptr);
      break;
    case OB_MODE_PAINT_GPENCIL_LEGACY:
      gpd->flag &= ~GP_DATA_STROKE_EDITMODE;
      gpd->flag |= GP_DATA_STROKE_PAINTMODE;
      gpd->flag &= ~GP_DATA_STROKE_SCULPTMODE;
      gpd->flag &= ~GP_DATA_STROKE_WEIGHTMODE;
      gpd->flag &= ~GP_DATA_STROKE_VERTEXMODE;
      ED_gpencil_toggle_brush_cursor(C, true, nullptr);
      break;
    case OB_MODE_SCULPT_GPENCIL_LEGACY:
      gpd->flag &= ~GP_DATA_STROKE_EDITMODE;
      gpd->flag &= ~GP_DATA_STROKE_PAINTMODE;
      gpd->flag |= GP_DATA_STROKE_SCULPTMODE;
      gpd->flag &= ~GP_DATA_STROKE_WEIGHTMODE;
      gpd->flag &= ~GP_DATA_STROKE_VERTEXMODE;
      ED_gpencil_toggle_brush_cursor(C, true, nullptr);
      break;
    case OB_MODE_WEIGHT_GPENCIL_LEGACY:
      gpd->flag &= ~GP_DATA_STROKE_EDITMODE;
      gpd->flag &= ~GP_DATA_STROKE_PAINTMODE;
      gpd->flag &= ~GP_DATA_STROKE_SCULPTMODE;
      gpd->flag |= GP_DATA_STROKE_WEIGHTMODE;
      gpd->flag &= ~GP_DATA_STROKE_VERTEXMODE;
      ED_gpencil_toggle_brush_cursor(C, true, nullptr);
      break;
    case OB_MODE_VERTEX_GPENCIL_LEGACY:
      gpd->flag &= ~GP_DATA_STROKE_EDITMODE;
      gpd->flag &= ~GP_DATA_STROKE_PAINTMODE;
      gpd->flag &= ~GP_DATA_STROKE_SCULPTMODE;
      gpd->flag &= ~GP_DATA_STROKE_WEIGHTMODE;
      gpd->flag |= GP_DATA_STROKE_VERTEXMODE;
      ED_gpencil_toggle_brush_cursor(C, true, nullptr);
      break;
    default:
      gpd->flag &= ~GP_DATA_STROKE_EDITMODE;
      gpd->flag &= ~GP_DATA_STROKE_PAINTMODE;
      gpd->flag &= ~GP_DATA_STROKE_SCULPTMODE;
      gpd->flag &= ~GP_DATA_STROKE_WEIGHTMODE;
      gpd->flag &= ~GP_DATA_STROKE_VERTEXMODE;
      ED_gpencil_toggle_brush_cursor(C, false, nullptr);
      break;
  }
}

/**
 * Helper to convert 2d to 3d for simple drawing buffer.
 */
static void gpencil_stroke_convertcoords(ARegion *region,
                                         const tGPspoint *point2D,
                                         const float origin[3],
                                         float out[3])
{
  float mval_prj[2];
  float rvec[3];

  copy_v3_v3(rvec, origin);

  const float zfac = ED_view3d_calc_zfac(static_cast<const RegionView3D *>(region->regiondata),
                                         rvec);

  if (ED_view3d_project_float_global(region, rvec, mval_prj, V3D_PROJ_TEST_NOP) == V3D_PROJ_RET_OK)
  {
    float dvec[3];
    float xy_delta[2];
    sub_v2_v2v2(xy_delta, mval_prj, point2D->m_xy);
    ED_view3d_win_to_delta(region, xy_delta, zfac, dvec);
    sub_v3_v3v3(out, rvec, dvec);
  }
  else {
    zero_v3(out);
  }
}

void ED_gpencil_tpoint_to_point(ARegion *region,
                                float origin[3],
                                const tGPspoint *tpt,
                                bGPDspoint *pt)
{
  float p3d[3];
  /* conversion to 3d format */
  gpencil_stroke_convertcoords(region, tpt, origin, p3d);
  copy_v3_v3(&pt->x, p3d);
  zero_v4(pt->vert_color);

  pt->pressure = tpt->pressure;
  pt->strength = tpt->strength;
  pt->uv_fac = tpt->uv_fac;
  pt->uv_rot = tpt->uv_rot;
}

void ED_gpencil_update_color_uv(Main *bmain, Material *mat)
{
  Material *gps_ma = nullptr;
  /* Read all strokes. */
  for (Object *ob = static_cast<Object *>(bmain->objects.first); ob;
       ob = static_cast<Object *>(ob->id.next))
  {
    if (ob->type == OB_GPENCIL_LEGACY) {
      bGPdata *gpd = static_cast<bGPdata *>(ob->data);
      if (gpd == nullptr) {
        continue;
      }

      LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
        /* only editable and visible layers are considered */
        if (BKE_gpencil_layer_is_editable(gpl)) {
          LISTBASE_FOREACH (bGPDframe *, gpf, &gpl->frames) {
            LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
              /* check if it is editable */
              if (ED_gpencil_stroke_material_editable(ob, gpl, gps) == false) {
                continue;
              }
              gps_ma = BKE_gpencil_material(ob, gps->mat_nr + 1);
              /* update */
              if ((gps_ma) && (gps_ma == mat)) {
                BKE_gpencil_stroke_uv_update(gps);
              }
            }
          }
        }
      }
      DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
    }
  }
}

static bool gpencil_check_collision(bGPDstroke *gps,
                                    bGPDstroke **gps_array,
                                    GHash *all_2d,
                                    int totstrokes,
                                    const float p2d_a1[2],
                                    const float p2d_a2[2],
                                    float r_hit[2])
{
  bool hit = false;
  /* check segment with all segments of all strokes */
  for (int s = 0; s < totstrokes; s++) {
    bGPDstroke *gps_iter = gps_array[s];
    if (gps_iter->totpoints < 2) {
      continue;
    }
    /* get stroke 2d version */
    float(*points2d)[2] = static_cast<float(*)[2]>(BLI_ghash_lookup(all_2d, gps_iter));

    for (int i2 = 0; i2 < gps_iter->totpoints - 1; i2++) {
      float p2d_b1[2], p2d_b2[2];
      copy_v2_v2(p2d_b1, points2d[i2]);
      copy_v2_v2(p2d_b2, points2d[i2 + 1]);

      /* don't self check */
      if (gps == gps_iter) {
        if (equals_v2v2(p2d_a1, p2d_b1) || equals_v2v2(p2d_a1, p2d_b2)) {
          continue;
        }
        if (equals_v2v2(p2d_a2, p2d_b1) || equals_v2v2(p2d_a2, p2d_b2)) {
          continue;
        }
      }
      /* check collision */
      int check = isect_seg_seg_v2_point(p2d_a1, p2d_a2, p2d_b1, p2d_b2, r_hit);
      if (check > 0) {
        hit = true;
        break;
      }
    }

    if (hit) {
      break;
    }
  }

  if (!hit) {
    zero_v2(r_hit);
  }

  return hit;
}

static void gpencil_copy_points(
    bGPDstroke *gps, bGPDspoint *pt, bGPDspoint *pt_final, int i, int i2)
{
  /* don't copy same point */
  if (i == i2) {
    return;
  }

  copy_v3_v3(&pt_final->x, &pt->x);
  pt_final->pressure = pt->pressure;
  pt_final->strength = pt->strength;
  pt_final->time = pt->time;
  pt_final->flag = pt->flag;
  pt_final->uv_fac = pt->uv_fac;
  pt_final->uv_rot = pt->uv_rot;
  copy_v4_v4(pt_final->vert_color, pt->vert_color);

  if (gps->dvert != nullptr) {
    MDeformVert *dvert = &gps->dvert[i];
    MDeformVert *dvert_final = &gps->dvert[i2];
    MEM_SAFE_FREE(dvert_final->dw);

    dvert_final->totweight = dvert->totweight;
    if (dvert->dw == nullptr) {
      dvert_final->dw = nullptr;
      dvert_final->totweight = 0;
    }
    else {
      dvert_final->dw = static_cast<MDeformWeight *>(MEM_dupallocN(dvert->dw));
    }
  }
}

static void gpencil_insert_point(bGPdata *gpd,
                                 bGPDstroke *gps,
                                 bGPDspoint *a_pt,
                                 bGPDspoint *b_pt,
                                 const float co_a[3],
                                 const float co_b[3])
{
  bGPDspoint *temp_points;
  int totnewpoints, oldtotpoints;

  totnewpoints = gps->totpoints;
  if (a_pt) {
    totnewpoints++;
  }
  if (b_pt) {
    totnewpoints++;
  }

  /* duplicate points in a temp area */
  temp_points = static_cast<bGPDspoint *>(MEM_dupallocN(gps->points));
  oldtotpoints = gps->totpoints;

  /* look index of base points because memory is changed when resize points array */
  int a_idx = -1;
  int b_idx = -1;
  for (int i = 0; i < oldtotpoints; i++) {
    bGPDspoint *pt = &gps->points[i];
    if (pt == a_pt) {
      a_idx = i;
    }
    if (pt == b_pt) {
      b_idx = i;
    }
  }

  /* resize the points arrays */
  gps->totpoints = totnewpoints;
  gps->points = static_cast<bGPDspoint *>(
      MEM_recallocN(gps->points, sizeof(*gps->points) * gps->totpoints));
  if (gps->dvert != nullptr) {
    gps->dvert = static_cast<MDeformVert *>(
        MEM_recallocN(gps->dvert, sizeof(*gps->dvert) * gps->totpoints));
  }

  /* copy all points */
  int i2 = 0;
  for (int i = 0; i < oldtotpoints; i++) {
    bGPDspoint *pt = &temp_points[i];
    bGPDspoint *pt_final = &gps->points[i2];
    gpencil_copy_points(gps, pt, pt_final, i, i2);

    /* create new point duplicating point and copy location */
    if (ELEM(i, a_idx, b_idx)) {
      i2++;
      pt_final = &gps->points[i2];
      gpencil_copy_points(gps, pt, pt_final, i, i2);
      copy_v3_v3(&pt_final->x, (i == a_idx) ? co_a : co_b);

      /* Un-select. */
      pt_final->flag &= ~GP_SPOINT_SELECT;
      /* tag to avoid more checking with this point */
      pt_final->flag |= GP_SPOINT_TAG;
    }

    i2++;
  }
  /* Calc geometry data. */
  BKE_gpencil_stroke_geometry_update(gpd, gps);

  MEM_SAFE_FREE(temp_points);
}

static float gpencil_calc_factor(const float p2d_a1[2],
                                 const float p2d_a2[2],
                                 const float r_hit2d[2])
{
  float dist1 = len_squared_v2v2(p2d_a1, p2d_a2);
  float dist2 = len_squared_v2v2(p2d_a1, r_hit2d);
  float f = dist1 > 0.0f ? dist2 / dist1 : 0.0f;

  /* apply a correction factor */
  float v1[2];
  interp_v2_v2v2(v1, p2d_a1, p2d_a2, f);
  float dist3 = len_squared_v2v2(p2d_a1, v1);
  float f1 = dist1 > 0.0f ? dist3 / dist1 : 0.0f;
  f = f + (f - f1);

  return f;
}

int ED_gpencil_select_stroke_segment(bGPdata *gpd,
                                     bGPDlayer *gpl,
                                     bGPDstroke *gps,
                                     bGPDspoint *pt,
                                     bool select,
                                     bool insert,
                                     const float scale,
                                     float r_hita[3],
                                     float r_hitb[3])
{
  if (gps->totpoints < 2) {
    return 0;
  }
  const float min_factor = 0.0015f;
  bGPDspoint *pta1 = nullptr;
  bGPDspoint *pta2 = nullptr;
  float f = 0.0f;
  int i2 = 0;

  bGPDlayer *gpl_orig = (gpl->runtime.gpl_orig) ? gpl->runtime.gpl_orig : gpl;
  bGPDframe *gpf = gpl_orig->actframe;
  if (gpf == nullptr) {
    return 0;
  }

  int memsize = BLI_listbase_count(&gpf->strokes);
  bGPDstroke **gps_array = static_cast<bGPDstroke **>(
      MEM_callocN(sizeof(bGPDstroke *) * memsize, __func__));

  /* save points */
  bGPDspoint *oldpoints = static_cast<bGPDspoint *>(MEM_dupallocN(gps->points));

  /* Save list of strokes to check */
  int totstrokes = 0;
  LISTBASE_FOREACH (bGPDstroke *, gps_iter, &gpf->strokes) {
    if (gps_iter->totpoints < 2) {
      continue;
    }
    gps_array[totstrokes] = gps_iter;
    totstrokes++;
  }

  if (totstrokes == 0) {
    return 0;
  }

  /* look for index of the current point */
  int cur_idx = -1;
  for (int i = 0; i < gps->totpoints; i++) {
    pta1 = &gps->points[i];
    if (pta1 == pt) {
      cur_idx = i;
      break;
    }
  }
  if (cur_idx < 0) {
    return 0;
  }

  /* Convert all gps points to 2d and save in a hash to avoid recalculation. */
  int direction = 0;
  float(*points2d)[2] = static_cast<float(*)[2]>(
      MEM_mallocN(sizeof(*points2d) * gps->totpoints, "GP Stroke temp 2d points"));
  BKE_gpencil_stroke_2d_flat_ref(
      gps->points, gps->totpoints, gps->points, gps->totpoints, points2d, scale, &direction);

  GHash *all_2d = BLI_ghash_ptr_new(__func__);

  for (int s = 0; s < totstrokes; s++) {
    bGPDstroke *gps_iter = gps_array[s];
    float(*points2d_iter)[2] = static_cast<float(*)[2]>(
        MEM_mallocN(sizeof(*points2d_iter) * gps_iter->totpoints, __func__));

    /* the extremes of the stroke are scaled to improve collision detection
     * for near lines */
    BKE_gpencil_stroke_2d_flat_ref(gps->points,
                                   gps->totpoints,
                                   gps_iter->points,
                                   gps_iter->totpoints,
                                   points2d_iter,
                                   scale,
                                   &direction);
    BLI_ghash_insert(all_2d, gps_iter, points2d_iter);
  }

  bool hit_a = false;
  bool hit_b = false;
  float p2d_a1[2] = {0.0f, 0.0f};
  float p2d_a2[2] = {0.0f, 0.0f};
  float r_hit2d[2];
  bGPDspoint *hit_pointa = nullptr;
  bGPDspoint *hit_pointb = nullptr;

  /* analyze points before current */
  if (cur_idx > 0) {
    for (int i = cur_idx; i >= 0; i--) {
      pta1 = &gps->points[i];
      copy_v2_v2(p2d_a1, points2d[i]);

      i2 = i - 1;
      CLAMP_MIN(i2, 0);
      pta2 = &gps->points[i2];
      copy_v2_v2(p2d_a2, points2d[i2]);

      hit_a = gpencil_check_collision(gps, gps_array, all_2d, totstrokes, p2d_a1, p2d_a2, r_hit2d);

      if (select) {
        pta1->flag |= GP_SPOINT_SELECT;
      }
      else {
        pta1->flag &= ~GP_SPOINT_SELECT;
      }

      if (hit_a) {
        f = gpencil_calc_factor(p2d_a1, p2d_a2, r_hit2d);
        interp_v3_v3v3(r_hita, &pta1->x, &pta2->x, f);
        if (f > min_factor) {
          hit_pointa = pta2; /* first point is second (inverted loop) */
        }
        else {
          pta1->flag &= ~GP_SPOINT_SELECT;
        }
        break;
      }
    }
  }

  /* analyze points after current */
  for (int i = cur_idx; i < gps->totpoints; i++) {
    pta1 = &gps->points[i];
    copy_v2_v2(p2d_a1, points2d[i]);

    i2 = i + 1;
    CLAMP_MAX(i2, gps->totpoints - 1);
    pta2 = &gps->points[i2];
    copy_v2_v2(p2d_a2, points2d[i2]);

    hit_b = gpencil_check_collision(gps, gps_array, all_2d, totstrokes, p2d_a1, p2d_a2, r_hit2d);

    if (select) {
      pta1->flag |= GP_SPOINT_SELECT;
    }
    else {
      pta1->flag &= ~GP_SPOINT_SELECT;
    }

    if (hit_b) {
      f = gpencil_calc_factor(p2d_a1, p2d_a2, r_hit2d);
      interp_v3_v3v3(r_hitb, &pta1->x, &pta2->x, f);
      if (f > min_factor) {
        hit_pointb = pta1;
      }
      else {
        pta1->flag &= ~GP_SPOINT_SELECT;
      }
      break;
    }
  }

  /* insert new point in the collision points */
  if (insert) {
    gpencil_insert_point(gpd, gps, hit_pointa, hit_pointb, r_hita, r_hitb);
  }

  /* free memory */
  if (all_2d) {
    GHashIterator gh_iter;
    GHASH_ITER (gh_iter, all_2d) {
      float(*p2d)[2] = static_cast<float(*)[2]>(BLI_ghashIterator_getValue(&gh_iter));
      MEM_SAFE_FREE(p2d);
    }
    BLI_ghash_free(all_2d, nullptr, nullptr);
  }

  /* if no hit, reset selection flag */
  if ((!hit_a) && (!hit_b)) {
    for (int i = 0; i < gps->totpoints; i++) {
      pta1 = &gps->points[i];
      pta2 = &oldpoints[i];
      pta1->flag = pta2->flag;
    }
  }

  MEM_SAFE_FREE(points2d);
  MEM_SAFE_FREE(gps_array);
  MEM_SAFE_FREE(oldpoints);

  /* return type of hit */
  if ((hit_a) && (hit_b)) {
    return 3;
  }
  if (hit_a) {
    return 1;
  }
  if (hit_b) {
    return 2;
  }
  return 0;
}

void ED_gpencil_select_toggle_all(bContext *C, int action)
{
  Object *ob = CTX_data_active_object(C);
  bGPdata *gpd = static_cast<bGPdata *>(ob->data);

  /* for "toggle", test for existing selected strokes */
  if (action == SEL_TOGGLE) {
    action = SEL_SELECT;

    CTX_DATA_BEGIN (C, bGPDstroke *, gps, editable_gpencil_strokes) {
      if (gps->flag & GP_STROKE_SELECT) {
        action = SEL_DESELECT;
        break; /* XXX: this only gets out of the inner loop. */
      }
    }
    CTX_DATA_END;
  }

  /* if deselecting, we need to deselect strokes across all frames
   * - Currently, an exception is only given for deselection
   *   Selecting and toggling should only affect what's visible,
   *   while deselecting helps clean up unintended/forgotten
   *   stuff on other frames
   */
  if (action == SEL_DESELECT) {
    /* deselect strokes across editable layers
     * NOTE: we limit ourselves to editable layers, since once a layer is "locked/hidden
     *       nothing should be able to touch it
     */
    /* Set selection index to 0. */
    gpd->select_last_index = 0;

    CTX_DATA_BEGIN (C, bGPDlayer *, gpl, editable_gpencil_layers) {

      /* deselect all strokes on all frames */
      LISTBASE_FOREACH (bGPDframe *, gpf, &gpl->frames) {
        LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
          bGPDspoint *pt;
          int i;

          /* only edit strokes that are valid in this view... */
          if (ED_gpencil_stroke_can_use(C, gps)) {
            for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
              pt->flag &= ~GP_SPOINT_SELECT;
            }

            gps->flag &= ~GP_STROKE_SELECT;
            BKE_gpencil_stroke_select_index_reset(gps);
          }
        }
      }
    }
    CTX_DATA_END;
  }
  else {
    /* select or deselect all strokes */
    CTX_DATA_BEGIN (C, bGPDstroke *, gps, editable_gpencil_strokes) {
      bGPDspoint *pt;
      int i;
      bool selected = false;

      /* Change selection status of all points, then make the stroke match */
      for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
        switch (action) {
          case SEL_SELECT:
            pt->flag |= GP_SPOINT_SELECT;
            break;
#if 0
          case SEL_DESELECT:
            pt->flag &= ~GP_SPOINT_SELECT;
            break;
#endif
          case SEL_INVERT:
            pt->flag ^= GP_SPOINT_SELECT;
            break;
        }

        if (pt->flag & GP_SPOINT_SELECT) {
          selected = true;
        }
      }

      /* Change status of stroke */
      if (selected) {
        gps->flag |= GP_STROKE_SELECT;
        BKE_gpencil_stroke_select_index_set(gpd, gps);
      }
      else {
        gps->flag &= ~GP_STROKE_SELECT;
        BKE_gpencil_stroke_select_index_reset(gps);
      }
    }
    CTX_DATA_END;
  }
}

void ED_gpencil_select_curve_toggle_all(bContext *C, int action)
{
  /* if toggle, check if we need to select or deselect */
  if (action == SEL_TOGGLE) {
    action = SEL_SELECT;
    GP_EDITABLE_CURVES_BEGIN(gps_iter, C, gpl, gps, gpc)
    {
      if (gpc->flag & GP_CURVE_SELECT) {
        action = SEL_DESELECT;
      }
    }
    GP_EDITABLE_CURVES_END(gps_iter);
  }

  if (action == SEL_DESELECT) {
    /* Set selection index to 0. */
    Object *ob = CTX_data_active_object(C);
    bGPdata *gpd = static_cast<bGPdata *>(ob->data);
    gpd->select_last_index = 0;

    GP_EDITABLE_CURVES_BEGIN(gps_iter, C, gpl, gps, gpc)
    {
      for (int i = 0; i < gpc->tot_curve_points; i++) {
        bGPDcurve_point *gpc_pt = &gpc->curve_points[i];
        BezTriple *bezt = &gpc_pt->bezt;
        gpc_pt->flag &= ~GP_CURVE_POINT_SELECT;
        BEZT_DESEL_ALL(bezt);
      }
      gpc->flag &= ~GP_CURVE_SELECT;
      gps->flag &= ~GP_STROKE_SELECT;
      BKE_gpencil_stroke_select_index_reset(gps);
    }
    GP_EDITABLE_CURVES_END(gps_iter);
  }
  else {
    GP_EDITABLE_STROKES_BEGIN (gps_iter, C, gpl, gps) {
      Object *ob = CTX_data_active_object(C);
      bGPdata *gpd = static_cast<bGPdata *>(ob->data);
      bool selected = false;

      /* Make sure stroke has an editcurve */
      if (gps->editcurve == nullptr) {
        BKE_gpencil_stroke_editcurve_update(gpd, gpl, gps);
        gps->flag |= GP_STROKE_NEEDS_CURVE_UPDATE;
        BKE_gpencil_stroke_geometry_update(gpd, gps);
      }

      bGPDcurve *gpc = gps->editcurve;
      for (int i = 0; i < gpc->tot_curve_points; i++) {
        bGPDcurve_point *gpc_pt = &gpc->curve_points[i];
        BezTriple *bezt = &gpc_pt->bezt;
        switch (action) {
          case SEL_SELECT:
            gpc_pt->flag |= GP_CURVE_POINT_SELECT;
            BEZT_SEL_ALL(bezt);
            break;
          case SEL_INVERT:
            gpc_pt->flag ^= GP_CURVE_POINT_SELECT;
            if (gpc_pt->flag & GP_CURVE_POINT_SELECT) {
              BEZT_SEL_ALL(bezt);
            }
            else {
              BEZT_DESEL_ALL(bezt);
            }
            break;
          default:
            break;
        }

        if (gpc_pt->flag & GP_CURVE_POINT_SELECT) {
          selected = true;
        }
      }

      if (selected) {
        gpc->flag |= GP_CURVE_SELECT;
        gps->flag |= GP_STROKE_SELECT;
        BKE_gpencil_stroke_select_index_set(gpd, gps);
      }
      else {
        gpc->flag &= ~GP_CURVE_SELECT;
        gps->flag &= ~GP_STROKE_SELECT;
        BKE_gpencil_stroke_select_index_reset(gps);
      }
    }
    GP_EDITABLE_STROKES_END(gps_iter);
  }
}

tGPspoint *ED_gpencil_sbuffer_ensure(tGPspoint *buffer_array,
                                     int *buffer_size,
                                     int *buffer_used,
                                     const bool clear)
{
  tGPspoint *p = nullptr;

  /* By default a buffer is created with one block with a predefined number of free points,
   * if the size is not enough, the cache is reallocated adding a new block of free points.
   * This is done in order to keep cache small and improve speed. */
  if (*buffer_used + 1 > *buffer_size) {
    if ((*buffer_size == 0) || (buffer_array == nullptr)) {
      p = static_cast<tGPspoint *>(
          MEM_callocN(sizeof(tGPspoint) * GP_STROKE_BUFFER_CHUNK, "GPencil Sbuffer"));
      *buffer_size = GP_STROKE_BUFFER_CHUNK;
    }
    else {
      *buffer_size += GP_STROKE_BUFFER_CHUNK;
      p = static_cast<tGPspoint *>(MEM_recallocN(buffer_array, sizeof(tGPspoint) * *buffer_size));
    }

    if (p == nullptr) {
      *buffer_size = *buffer_used = 0;
    }

    buffer_array = p;
  }

  /* clear old data */
  if (clear) {
    *buffer_used = 0;
    if (buffer_array != nullptr) {
      memset(buffer_array, 0, sizeof(tGPspoint) * *buffer_size);
    }
  }

  return buffer_array;
}

void ED_gpencil_sbuffer_update_eval(bGPdata *gpd, Object *ob_eval)
{
  bGPdata *gpd_eval = (bGPdata *)ob_eval->data;

  gpd_eval->runtime.sbuffer = gpd->runtime.sbuffer;
  gpd_eval->runtime.sbuffer_sflag = gpd->runtime.sbuffer_sflag;
  gpd_eval->runtime.sbuffer_used = gpd->runtime.sbuffer_used;
  gpd_eval->runtime.sbuffer_size = gpd->runtime.sbuffer_size;
  gpd_eval->runtime.tot_cp_points = gpd->runtime.tot_cp_points;
  gpd_eval->runtime.cp_points = gpd->runtime.cp_points;
}

void ED_gpencil_tag_scene_gpencil(Scene *scene)
{
  /* Mark all grease pencil data-blocks of the scene. */
  FOREACH_SCENE_COLLECTION_BEGIN (scene, collection) {
    FOREACH_COLLECTION_OBJECT_RECURSIVE_BEGIN (collection, ob) {
      if (ob->type == OB_GPENCIL_LEGACY) {
        bGPdata *gpd = (bGPdata *)ob->data;
        gpd->flag |= GP_DATA_CACHE_IS_DIRTY;
        DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
      }
    }
    FOREACH_COLLECTION_OBJECT_RECURSIVE_END;
  }
  FOREACH_SCENE_COLLECTION_END;

  DEG_id_tag_update(&scene->id, ID_RECALC_SYNC_TO_EVAL);

  WM_main_add_notifier(NC_GPENCIL | NA_EDITED, nullptr);
}

void ED_gpencil_fill_vertex_color_set(ToolSettings *ts, Brush *brush, bGPDstroke *gps)
{
  const bool is_vertex = (GPENCIL_USE_VERTEX_COLOR_FILL(ts, brush) &&
                          (brush->gpencil_settings->brush_draw_mode != GP_BRUSH_MODE_MATERIAL)) ||
                         (!GPENCIL_USE_VERTEX_COLOR_FILL(ts, brush) &&
                          (brush->gpencil_settings->brush_draw_mode == GP_BRUSH_MODE_VERTEXCOLOR));
  if (is_vertex) {
    copy_v3_v3(gps->vert_color_fill, brush->rgb);
    gps->vert_color_fill[3] = brush->gpencil_settings->vertex_factor;
    srgb_to_linearrgb_v4(gps->vert_color_fill, gps->vert_color_fill);
  }
  else {
    zero_v4(gps->vert_color_fill);
  }
}

void ED_gpencil_point_vertex_color_set(ToolSettings *ts,
                                       Brush *brush,
                                       bGPDspoint *pt,
                                       tGPspoint *tpt)
{
  const bool is_vertex = (GPENCIL_USE_VERTEX_COLOR_STROKE(ts, brush) &&
                          (brush->gpencil_settings->brush_draw_mode != GP_BRUSH_MODE_MATERIAL)) ||
                         (!GPENCIL_USE_VERTEX_COLOR_STROKE(ts, brush) &&
                          (brush->gpencil_settings->brush_draw_mode == GP_BRUSH_MODE_VERTEXCOLOR));

  if (is_vertex) {
    if (tpt == nullptr) {
      copy_v3_v3(pt->vert_color, brush->rgb);
      pt->vert_color[3] = brush->gpencil_settings->vertex_factor;
      srgb_to_linearrgb_v4(pt->vert_color, pt->vert_color);
    }
    else {
      copy_v3_v3(pt->vert_color, tpt->vert_color);
      pt->vert_color[3] = brush->gpencil_settings->vertex_factor;
    }
  }
  else {
    zero_v4(pt->vert_color);
  }
}

void ED_gpencil_init_random_settings(Brush *brush,
                                     const int mval[2],
                                     GpRandomSettings *random_settings)
{
  int seed = (uint(ceil(BLI_time_now_seconds())) + 1) % 128;
  /* Use mouse position to get randomness. */
  int ix = mval[0] * seed;
  int iy = mval[1] * seed;
  int iz = ix + iy * seed;
  zero_v3(random_settings->hsv);

  BrushGpencilSettings *brush_settings = brush->gpencil_settings;
  /* Random to Hue. */
  if (brush_settings->random_hue > 0.0f) {
    float rand = BLI_hash_int_01(BLI_hash_int_2d(ix, iy)) * 2.0f - 1.0f;
    random_settings->hsv[0] = rand * brush_settings->random_hue * 0.5f;
  }
  /* Random to Saturation. */
  if (brush_settings->random_saturation > 0.0f) {
    float rand = BLI_hash_int_01(BLI_hash_int_2d(iy, ix)) * 2.0f - 1.0f;
    random_settings->hsv[1] = rand * brush_settings->random_saturation;
  }
  /* Random to Value. */
  if (brush_settings->random_value > 0.0f) {
    float rand = BLI_hash_int_01(BLI_hash_int_2d(ix * iz, iy * iz)) * 2.0f - 1.0f;
    random_settings->hsv[2] = rand * brush_settings->random_value;
  }

  /* Random to pressure. */
  if (brush_settings->draw_random_press > 0.0f) {
    random_settings->pressure = BLI_hash_int_01(BLI_hash_int_2d(ix + iz, iy + iz)) * 2.0f - 1.0f;
  }

  /* Random to color strength. */
  if (brush_settings->draw_random_strength) {
    random_settings->strength = BLI_hash_int_01(BLI_hash_int_2d(ix + iy, iy + iz + ix)) * 2.0f -
                                1.0f;
  }

  /* Random to uv texture rotation. */
  if (brush_settings->uv_random > 0.0f) {
    random_settings->uv = BLI_hash_int_01(BLI_hash_int_2d(iy + iz, ix * iz)) * 2.0f - 1.0f;
  }
}

static void gpencil_sbuffer_vertex_color_random(
    bGPdata *gpd, Brush *brush, tGPspoint *tpt, const float random_color[3], float pen_pressure)
{
  BrushGpencilSettings *brush_settings = brush->gpencil_settings;
  if (brush_settings->flag & GP_BRUSH_GROUP_RANDOM) {
    int seed = (uint(ceil(BLI_time_now_seconds())) + 1) % 128;

    int ix = int(tpt->m_xy[0] * seed);
    int iy = int(tpt->m_xy[1] * seed);
    int iz = ix + iy * seed;
    blender::float3 hsv;
    float factor_value[3];
    zero_v3(factor_value);

    /* Apply randomness to Hue. */
    if (brush_settings->random_hue > 0.0f) {
      if ((brush_settings->flag2 & GP_BRUSH_USE_HUE_AT_STROKE) == 0) {

        float rand = BLI_hash_int_01(BLI_hash_int_2d(ix, gpd->runtime.sbuffer_used)) * 2.0f - 1.0f;
        factor_value[0] = rand * brush_settings->random_hue * 0.5f;
      }
      else {
        factor_value[0] = random_color[0];
      }

      /* Apply random curve. */
      if (brush_settings->flag2 & GP_BRUSH_USE_HUE_RAND_PRESS) {
        factor_value[0] *= BKE_curvemapping_evaluateF(
            brush_settings->curve_rand_hue, 0, pen_pressure);
      }
    }

    /* Apply randomness to Saturation. */
    if (brush_settings->random_saturation > 0.0f) {
      if ((brush_settings->flag2 & GP_BRUSH_USE_SAT_AT_STROKE) == 0) {
        float rand = BLI_hash_int_01(BLI_hash_int_2d(iy, gpd->runtime.sbuffer_used)) * 2.0f - 1.0f;
        factor_value[1] = rand * brush_settings->random_saturation;
      }
      else {
        factor_value[1] = random_color[1];
      }

      /* Apply random curve. */
      if (brush_settings->flag2 & GP_BRUSH_USE_SAT_RAND_PRESS) {
        factor_value[1] *= BKE_curvemapping_evaluateF(
            brush_settings->curve_rand_saturation, 0, pen_pressure);
      }
    }

    /* Apply randomness to Value. */
    if (brush_settings->random_value > 0.0f) {
      if ((brush_settings->flag2 & GP_BRUSH_USE_VAL_AT_STROKE) == 0) {
        float rand = BLI_hash_int_01(BLI_hash_int_2d(iz, gpd->runtime.sbuffer_used)) * 2.0f - 1.0f;
        factor_value[2] = rand * brush_settings->random_value;
      }
      else {
        factor_value[2] = random_color[2];
      }

      /* Apply random curve. */
      if (brush_settings->flag2 & GP_BRUSH_USE_VAL_RAND_PRESS) {
        factor_value[2] *= BKE_curvemapping_evaluateF(
            brush_settings->curve_rand_value, 0, pen_pressure);
      }
    }

    rgb_to_hsv_v(tpt->vert_color, hsv);
    add_v3_v3(hsv, factor_value);
    /* For Hue need to cover all range, but for Saturation and Value
     * is not logic because the effect is too hard, so the value is just clamped. */
    if (hsv[0] < 0.0f) {
      hsv[0] += 1.0f;
    }
    else if (hsv[0] > 1.0f) {
      hsv[0] -= 1.0f;
    }

    hsv = blender::math::clamp(hsv, 0.0f, 1.0f);
    hsv_to_rgb_v(hsv, tpt->vert_color);
  }
}

void ED_gpencil_sbuffer_vertex_color_set(Depsgraph *depsgraph,
                                         Object *ob,
                                         ToolSettings *ts,
                                         Brush *brush,
                                         Material *material,
                                         float random_color[3],
                                         float pen_pressure)
{
  bGPdata *gpd = (bGPdata *)ob->data;
  Object *ob_eval = (Object *)DEG_get_evaluated_id(depsgraph, &ob->id);
  bGPdata *gpd_eval = (bGPdata *)ob_eval->data;
  MaterialGPencilStyle *gp_style = material->gp_style;

  const bool is_vertex_fill =
      (GPENCIL_USE_VERTEX_COLOR_FILL(ts, brush) &&
       (brush->gpencil_settings->brush_draw_mode != GP_BRUSH_MODE_MATERIAL)) ||
      (!GPENCIL_USE_VERTEX_COLOR_FILL(ts, brush) &&
       (brush->gpencil_settings->brush_draw_mode == GP_BRUSH_MODE_VERTEXCOLOR));

  const bool is_vertex_stroke =
      (GPENCIL_USE_VERTEX_COLOR_STROKE(ts, brush) &&
       (brush->gpencil_settings->brush_draw_mode != GP_BRUSH_MODE_MATERIAL)) ||
      (!GPENCIL_USE_VERTEX_COLOR_STROKE(ts, brush) &&
       (brush->gpencil_settings->brush_draw_mode == GP_BRUSH_MODE_VERTEXCOLOR));

  int idx = gpd->runtime.sbuffer_used;
  tGPspoint *tpt = (tGPspoint *)gpd->runtime.sbuffer + idx;

  float vertex_color[4];
  copy_v3_v3(vertex_color, brush->rgb);
  vertex_color[3] = brush->gpencil_settings->vertex_factor;
  srgb_to_linearrgb_v4(vertex_color, vertex_color);

  /* Copy fill vertex color. */
  if (is_vertex_fill) {
    copy_v4_v4(gpd->runtime.vert_color_fill, vertex_color);
  }
  else {
    copy_v4_v4(gpd->runtime.vert_color_fill, gp_style->fill_rgba);
  }
  /* Copy stroke vertex color. */
  if (is_vertex_stroke) {
    copy_v4_v4(tpt->vert_color, vertex_color);
  }
  else {
    copy_v4_v4(tpt->vert_color, gp_style->stroke_rgba);
  }

  /* Random Color. */
  gpencil_sbuffer_vertex_color_random(gpd, brush, tpt, random_color, pen_pressure);

  /* Copy to evaluate data because paint operators don't tag refresh until end for speedup
   * painting. */
  if (gpd_eval != nullptr) {
    copy_v4_v4(gpd_eval->runtime.vert_color_fill, gpd->runtime.vert_color_fill);
    gpd_eval->runtime.matid = gpd->runtime.matid;
    gpd_eval->runtime.fill_opacity_fac = gpd->runtime.fill_opacity_fac;
  }
}

void ED_gpencil_projected_2d_bound_box(const GP_SpaceConversion *gsc,
                                       const bGPDstroke *gps,
                                       const float diff_mat[4][4],
                                       float r_min[2],
                                       float r_max[2])
{
  float bounds[8][2];
  BoundBox bb;
  BKE_boundbox_init_from_minmax(&bb, gps->boundbox_min, gps->boundbox_max);

  /* Project 8 vertices in 2D. */
  for (int i = 0; i < 8; i++) {
    bGPDspoint pt_dummy, pt_dummy_ps;
    copy_v3_v3(&pt_dummy.x, bb.vec[i]);
    gpencil_point_to_world_space(&pt_dummy, diff_mat, &pt_dummy_ps);
    gpencil_point_to_xy_fl(gsc, gps, &pt_dummy_ps, &bounds[i][0], &bounds[i][1]);
  }

  /* Take extremes. */
  INIT_MINMAX2(r_min, r_max);
  for (int i = 0; i < 8; i++) {
    minmax_v2v2_v2(r_min, r_max, bounds[i]);
  }

  /* Ensure the bounding box is oriented to axis. */
  if (r_max[0] < r_min[0]) {
    std::swap(r_min[0], r_max[0]);
  }
  if (r_max[1] < r_min[1]) {
    std::swap(r_min[1], r_max[1]);
  }
}

bool ED_gpencil_stroke_check_collision(const GP_SpaceConversion *gsc,
                                       bGPDstroke *gps,
                                       const float mval[2],
                                       const int radius,
                                       const float diff_mat[4][4])
{
  const int offset = int(ceil(sqrt((radius * radius) * 2)));
  float boundbox_min[2];
  float boundbox_max[2];

  /* Check we have something to use (only for old files). */
  if (is_zero_v3(gps->boundbox_min)) {
    BKE_gpencil_stroke_boundingbox_calc(gps);
  }

  ED_gpencil_projected_2d_bound_box(gsc, gps, diff_mat, boundbox_min, boundbox_max);

  rcti rect_stroke = {
      int(boundbox_min[0]), int(boundbox_max[0]), int(boundbox_min[1]), int(boundbox_max[1])};

  /* For mouse, add a small offset to avoid false negative in corners. */
  rcti rect_mouse = {
      int(mval[0]) - offset, int(mval[0]) + offset, int(mval[1]) - offset, int(mval[1]) + offset};

  /* Check collision between both rectangles. */
  return BLI_rcti_isect(&rect_stroke, &rect_mouse, nullptr);
}

bool ED_gpencil_stroke_point_is_inside(const bGPDstroke *gps,
                                       const GP_SpaceConversion *gsc,
                                       const int mval[2],
                                       const float diff_mat[4][4])
{
  bool hit = false;
  if (gps->totpoints == 0) {
    return hit;
  }

  int len = gps->totpoints;
  blender::Array<blender::int2> mcoords(len);

  /* Convert stroke to 2D array of points. */
  const bGPDspoint *pt;
  int i;
  for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
    bGPDspoint pt2;
    gpencil_point_to_world_space(pt, diff_mat, &pt2);
    gpencil_point_to_xy(gsc, gps, &pt2, &mcoords[i][0], &mcoords[i][1]);
  }

  /* Compute bound-box of lasso (for faster testing later). */
  rcti rect;
  BLI_lasso_boundbox(&rect, mcoords);

  /* Test if point inside stroke. */
  hit = (!ELEM(V2D_IS_CLIPPED, mval[0], mval[1]) && BLI_rcti_isect_pt(&rect, mval[0], mval[1]) &&
         BLI_lasso_is_point_inside(mcoords, mval[0], mval[1], INT_MAX));

  return hit;
}

void ED_gpencil_stroke_extremes_to2d(const GP_SpaceConversion *gsc,
                                     const float diff_mat[4][4],
                                     bGPDstroke *gps,
                                     float r_ctrl1[2],
                                     float r_ctrl2[2])
{
  bGPDspoint pt_dummy_ps;

  gpencil_point_to_world_space(&gps->points[0], diff_mat, &pt_dummy_ps);
  gpencil_point_to_xy_fl(gsc, gps, &pt_dummy_ps, &r_ctrl1[0], &r_ctrl1[1]);
  gpencil_point_to_world_space(&gps->points[gps->totpoints - 1], diff_mat, &pt_dummy_ps);
  gpencil_point_to_xy_fl(gsc, gps, &pt_dummy_ps, &r_ctrl2[0], &r_ctrl2[1]);
}

bGPDstroke *ED_gpencil_stroke_nearest_to_ends(bContext *C,
                                              const GP_SpaceConversion *gsc,
                                              bGPDlayer *gpl,
                                              bGPDframe *gpf,
                                              bGPDstroke *gps,
                                              const float ctrl1[2],
                                              const float ctrl2[2],
                                              const float radius,
                                              int *r_index)
{
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  Object *ob = CTX_data_active_object(C);
  bGPDstroke *gps_rtn = nullptr;
  const float radius_sqr = radius * radius;

  /* calculate difference matrix object */
  float diff_mat[4][4];
  BKE_gpencil_layer_transform_matrix_get(depsgraph, ob, gpl, diff_mat);

  /* Calculate the extremes of the stroke in 2D. */
  bGPDspoint pt_parent;
  float pt2d_start[2], pt2d_end[2];

  bGPDspoint *pt = &gps->points[0];
  gpencil_point_to_world_space(pt, diff_mat, &pt_parent);
  gpencil_point_to_xy_fl(gsc, gps, &pt_parent, &pt2d_start[0], &pt2d_start[1]);

  pt = &gps->points[gps->totpoints - 1];
  gpencil_point_to_world_space(pt, diff_mat, &pt_parent);
  gpencil_point_to_xy_fl(gsc, gps, &pt_parent, &pt2d_end[0], &pt2d_end[1]);

  /* Loop all strokes of the active frame. */
  float dist_min = FLT_MAX;
  LISTBASE_FOREACH (bGPDstroke *, gps_target, &gpf->strokes) {
    /* Check if the color is editable. */
    if ((gps_target == gps) || (ED_gpencil_stroke_material_editable(ob, gpl, gps) == false)) {
      continue;
    }

    /* Check that stroke is not closed. Closed strokes must not be included in the merge. */
    if (gps_target->flag & GP_STROKE_CYCLIC) {
      continue;
    }

    /* Check if one of the ends is inside target stroke bounding box. */
    if (!ED_gpencil_stroke_check_collision(gsc, gps_target, pt2d_start, radius, diff_mat) &&
        !ED_gpencil_stroke_check_collision(gsc, gps_target, pt2d_end, radius, diff_mat))
    {
      continue;
    }
    /* Check the distance of the ends with the ends of target stroke to avoid middle contact.
     * All is done in 2D plane. */
    float pt2d_target_start[2], pt2d_target_end[2];

    pt = &gps_target->points[0];
    gpencil_point_to_world_space(pt, diff_mat, &pt_parent);
    gpencil_point_to_xy_fl(gsc, gps, &pt_parent, &pt2d_target_start[0], &pt2d_target_start[1]);

    pt = &gps_target->points[gps_target->totpoints - 1];
    gpencil_point_to_world_space(pt, diff_mat, &pt_parent);
    gpencil_point_to_xy_fl(gsc, gps, &pt_parent, &pt2d_target_end[0], &pt2d_target_end[1]);

    /* If the distance to the original stroke extremes is too big, the stroke must not be joined.
     */
    if ((len_squared_v2v2(ctrl1, pt2d_target_start) > radius_sqr) &&
        (len_squared_v2v2(ctrl1, pt2d_target_end) > radius_sqr) &&
        (len_squared_v2v2(ctrl2, pt2d_target_start) > radius_sqr) &&
        (len_squared_v2v2(ctrl2, pt2d_target_end) > radius_sqr))
    {
      continue;
    }

    if ((len_squared_v2v2(pt2d_start, pt2d_target_start) > radius_sqr) &&
        (len_squared_v2v2(pt2d_start, pt2d_target_end) > radius_sqr) &&
        (len_squared_v2v2(pt2d_end, pt2d_target_start) > radius_sqr) &&
        (len_squared_v2v2(pt2d_end, pt2d_target_end) > radius_sqr))
    {
      continue;
    }

    /* Loop all points and check what is the nearest point. */
    int i;
    for (i = 0, pt = gps_target->points; i < gps_target->totpoints; i++, pt++) {
      /* Convert point to 2D. */
      float pt2d[2];
      gpencil_point_to_world_space(pt, diff_mat, &pt_parent);
      gpencil_point_to_xy_fl(gsc, gps, &pt_parent, &pt2d[0], &pt2d[1]);

      /* Check with Start point. */
      float dist = len_squared_v2v2(pt2d, pt2d_start);
      if ((dist <= radius_sqr) && (dist < dist_min)) {
        *r_index = i;
        dist_min = dist;
        gps_rtn = gps_target;
      }
      /* Check with End point. */
      dist = len_squared_v2v2(pt2d, pt2d_end);
      if ((dist <= radius_sqr) && (dist < dist_min)) {
        *r_index = i;
        dist_min = dist;
        gps_rtn = gps_target;
      }
    }
  }

  return gps_rtn;
}

bGPDstroke *ED_gpencil_stroke_join_and_trim(
    bGPdata *gpd, bGPDframe *gpf, bGPDstroke *gps, bGPDstroke *gps_dst, const int pt_index)
{
  if ((gps->totpoints < 1) || (gps_dst->totpoints < 1)) {
    return nullptr;
  }
  BLI_assert(pt_index >= 0 && pt_index < gps_dst->totpoints);

  bGPDspoint *pt = nullptr;

  /* Cannot be cyclic. */
  gps->flag &= ~GP_STROKE_CYCLIC;
  gps_dst->flag &= ~GP_STROKE_CYCLIC;

  /* Trim stroke. */
  bGPDstroke *gps_final = gps_dst;
  if ((pt_index > 0) && (pt_index < gps_dst->totpoints - 2)) {
    /* Untag any pending operation. */
    gps_dst->flag &= ~GP_STROKE_TAG;
    for (int i = 0; i < gps_dst->totpoints; i++) {
      gps_dst->points[i].flag &= ~GP_SPOINT_TAG;
    }

    /* Delete points of the shorter extreme */
    pt = &gps_dst->points[0];
    float dist_to_start = BKE_gpencil_stroke_segment_length(gps_dst, 0, pt_index, true);
    pt = &gps_dst->points[gps_dst->totpoints - 1];
    float dist_to_end = BKE_gpencil_stroke_segment_length(
        gps_dst, pt_index, gps_dst->totpoints - 1, true);

    if (dist_to_start < dist_to_end) {
      for (int i = 0; i < pt_index; i++) {
        gps_dst->points[i].flag |= GP_SPOINT_TAG;
      }
    }
    else {
      for (int i = pt_index + 1; i < gps_dst->totpoints; i++) {
        gps_dst->points[i].flag |= GP_SPOINT_TAG;
      }
    }
    /* Remove tagged points to trim stroke. */
    gps_final = BKE_gpencil_stroke_delete_tagged_points(
        gpd, gpf, gps_dst, gps_dst->next, GP_SPOINT_TAG, false, false, 0);
  }

  /* Join both strokes. */
  int totpoint = gps_final->totpoints;
  BKE_gpencil_stroke_join(gps_final, gps, false, true, true, true);

  /* Select the join points and merge if the distance is very small. */
  pt = &gps_final->points[totpoint - 1];
  pt->flag |= GP_SPOINT_SELECT;

  pt = &gps_final->points[totpoint];
  pt->flag |= GP_SPOINT_SELECT;
  BKE_gpencil_stroke_merge_distance(gpd, gpf, gps_final, 0.01f, false);

  /* Unselect all points. */
  for (int i = 0; i < gps_final->totpoints; i++) {
    gps_final->points[i].flag &= ~GP_SPOINT_SELECT;
  }

  /* Delete old stroke. */
  BLI_remlink(&gpf->strokes, gps);
  BKE_gpencil_free_stroke(gps);

  return gps_final;
}

void ED_gpencil_stroke_close_by_distance(bGPDstroke *gps, const float threshold)
{
  if (gps == nullptr) {
    return;
  }
  bGPDspoint *pt_start = &gps->points[0];
  bGPDspoint *pt_end = &gps->points[gps->totpoints - 1];

  const float threshold_sqr = threshold * threshold;
  float dist_to_close = len_squared_v3v3(&pt_start->x, &pt_end->x);
  if (dist_to_close < threshold_sqr) {
    gps->flag |= GP_STROKE_CYCLIC;
    BKE_gpencil_stroke_close(gps);
  }
}

void ED_gpencil_layer_merge(bGPdata *gpd,
                            bGPDlayer *gpl_src,
                            bGPDlayer *gpl_dst,
                            const bool reverse)
{
  /* Collect frames of gpl_dst in hash table to avoid O(n^2) lookups. */
  GHash *gh_frames_dst = BLI_ghash_int_new_ex(__func__, 64);
  LISTBASE_FOREACH (bGPDframe *, gpf_dst, &gpl_dst->frames) {
    BLI_ghash_insert(gh_frames_dst, POINTER_FROM_INT(gpf_dst->framenum), gpf_dst);
  }

  /* Read all frames from merge layer and add any missing in destination layer,
   * copying all previous strokes to keep the image equals.
   * Need to do it in a separated loop to avoid strokes accumulation. */
  LISTBASE_FOREACH (bGPDframe *, gpf_src, &gpl_src->frames) {
    /* Try to find frame in destination layer hash table. */
    bGPDframe *gpf_dst = static_cast<bGPDframe *>(
        BLI_ghash_lookup(gh_frames_dst, POINTER_FROM_INT(gpf_src->framenum)));
    if (!gpf_dst) {
      gpf_dst = BKE_gpencil_layer_frame_get(gpl_dst, gpf_src->framenum, GP_GETFRAME_ADD_COPY);
      /* Use same frame type. */
      gpf_dst->key_type = gpf_src->key_type;
      BLI_ghash_insert(gh_frames_dst, POINTER_FROM_INT(gpf_src->framenum), gpf_dst);
    }

    /* Copy current source frame to further frames
     * that are keyframes in destination layer and not in source layer
     * to keep the image equals. */
    if (gpf_dst->next && (!gpf_src->next || (gpf_dst->next->framenum < gpf_src->next->framenum))) {
      gpf_dst = gpf_dst->next;
      BKE_gpencil_layer_frame_get(gpl_src, gpf_dst->framenum, GP_GETFRAME_ADD_COPY);
    }
  }

  /* Read all frames from merge layer and add strokes. */
  LISTBASE_FOREACH (bGPDframe *, gpf_src, &gpl_src->frames) {
    /* Try to find frame in destination layer hash table. */
    bGPDframe *gpf_dst = static_cast<bGPDframe *>(
        BLI_ghash_lookup(gh_frames_dst, POINTER_FROM_INT(gpf_src->framenum)));
    /* Add to tail all strokes. */
    if (gpf_dst) {
      if (reverse) {
        BLI_movelisttolist_reverse(&gpf_dst->strokes, &gpf_src->strokes);
      }
      else {
        BLI_movelisttolist(&gpf_dst->strokes, &gpf_src->strokes);
      }
    }
  }

  /* Add Masks to destination layer. */
  LISTBASE_FOREACH (bGPDlayer_Mask *, mask, &gpl_src->mask_layers) {
    /* Don't add merged layers or missing layer names. */
    if (!BKE_gpencil_layer_named_get(gpd, mask->name) || STREQ(mask->name, gpl_src->info) ||
        STREQ(mask->name, gpl_dst->info))
    {
      continue;
    }
    if (!BKE_gpencil_layer_mask_named_get(gpl_dst, mask->name)) {
      bGPDlayer_Mask *mask_new = static_cast<bGPDlayer_Mask *>(MEM_dupallocN(mask));
      BLI_addtail(&gpl_dst->mask_layers, mask_new);
      gpl_dst->act_mask++;
    }
  }

  /* Set destination layer as active. */
  BKE_gpencil_layer_active_set(gpd, gpl_dst);

  /* Now delete merged layer. */
  BKE_gpencil_layer_delete(gpd, gpl_src);
  BLI_ghash_free(gh_frames_dst, nullptr, nullptr);

  /* Reorder masking. */
  if (gpl_dst->mask_layers.first) {
    BKE_gpencil_layer_mask_sort(gpd, gpl_dst);
  }
}

static void gpencil_layer_new_name_get(bGPdata *gpd, char *r_name, size_t name_maxncpy)
{
  int index = 0;
  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    if (strstr(gpl->info, "GP_Layer")) {
      index++;
    }
  }

  if (index == 0) {
    BLI_strncpy(r_name, "GP_Layer", name_maxncpy);
    return;
  }
  BLI_snprintf(r_name, name_maxncpy, "GP_Layer.%03d", index);
}

int ED_gpencil_new_layer_dialog(bContext *C, wmOperator *op)
{
  Object *ob = CTX_data_active_object(C);
  PropertyRNA *prop;
  if (RNA_int_get(op->ptr, "layer") == -1) {
    prop = RNA_struct_find_property(op->ptr, "new_layer_name");
    if (!RNA_property_is_set(op->ptr, prop)) {
      char name[MAX_NAME];
      bGPdata *gpd = static_cast<bGPdata *>(ob->data);
      gpencil_layer_new_name_get(gpd, name, sizeof(name));
      RNA_property_string_set(op->ptr, prop, name);
      return WM_operator_props_dialog_popup(C, op, 200, IFACE_("Add New Layer"), IFACE_("Add"));
    }
  }
  return 0;
}
