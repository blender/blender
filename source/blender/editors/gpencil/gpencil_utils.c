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
 * The Original Code is Copyright (C) 2014, Blender Foundation
 */

/** \file
 * \ingroup edgpencil
 */

#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_ghash.h"
#include "BLI_hash.h"
#include "BLI_math.h"
#include "BLI_rand.h"
#include "BLI_utildefines.h"
#include "BLT_translation.h"

#include "PIL_time.h"

#include "DNA_brush_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_view3d_types.h"

#include "BKE_action.h"
#include "BKE_brush.h"
#include "BKE_collection.h"
#include "BKE_colortools.h"
#include "BKE_context.h"
#include "BKE_deform.h"
#include "BKE_gpencil.h"
#include "BKE_gpencil_geom.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_object.h"
#include "BKE_paint.h"
#include "BKE_tracking.h"

#include "WM_api.h"
#include "WM_toolsystem.h"
#include "WM_types.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "UI_resources.h"
#include "UI_view2d.h"

#include "ED_clip.h"
#include "ED_gpencil.h"
#include "ED_object.h"
#include "ED_screen.h"
#include "ED_select_utils.h"
#include "ED_transform_snap_object_context.h"
#include "ED_view3d.h"

#include "GPU_immediate.h"
#include "GPU_immediate_util.h"
#include "GPU_state.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "gpencil_intern.h"

/* ******************************************************** */
/* Context Wrangling... */

/**
 * Get pointer to active Grease Pencil datablock,
 * and an RNA-pointer to trace back to whatever owns it,
 * when context info is not available.
 */
bGPdata **ED_gpencil_data_get_pointers_direct(ScrArea *area, Object *ob, PointerRNA *r_ptr)
{
  /* if there's an active area, check if the particular editor may
   * have defined any special Grease Pencil context for editing...
   */
  if (area) {
    switch (area->spacetype) {
      case SPACE_PROPERTIES: /* properties */
      case SPACE_INFO:       /* header info */
      case SPACE_TOPBAR:     /* Topbar */
      case SPACE_VIEW3D:     /* 3D-View */
      {
        if (ob && (ob->type == OB_GPENCIL)) {
          /* GP Object. */
          if (r_ptr) {
            RNA_id_pointer_create(&ob->id, r_ptr);
          }
          return (bGPdata **)&ob->data;
        }
        else {
          return NULL;
        }

        break;
      }
      default: /* Unsupported space. */
        return NULL;
    }
  }

  return NULL;
}

/**
 * Get pointer to active Grease Pencil datablock for annotations,
 * and an RNA-pointer to trace back to whatever owns it,
 * when context info is not available.
 */
bGPdata **ED_annotation_data_get_pointers_direct(ID *screen_id,
                                                 ScrArea *area,
                                                 Scene *scene,
                                                 PointerRNA *r_ptr)
{
  /* If there's an active area, check if the particular editor may
   * have defined any special Grease Pencil context for editing. */
  if (area) {
    SpaceLink *sl = area->spacedata.first;

    switch (area->spacetype) {
      case SPACE_PROPERTIES: /* properties */
      case SPACE_INFO:       /* header info */
      {
        return NULL;
        break;
      }

      case SPACE_TOPBAR: /* Topbar */
      case SPACE_VIEW3D: /* 3D-View */
      {
        if (r_ptr) {
          RNA_id_pointer_create(&scene->id, r_ptr);
        }
        return &scene->gpd;

        break;
      }
      case SPACE_NODE: /* Nodes Editor */
      {
        SpaceNode *snode = (SpaceNode *)sl;

        /* return the GP data for the active node block/node */
        if (snode && snode->nodetree) {
          /* for now, as long as there's an active node tree,
           * default to using that in the Nodes Editor */
          if (r_ptr) {
            RNA_id_pointer_create(&snode->nodetree->id, r_ptr);
          }
          return &snode->nodetree->gpd;
        }

        /* Even when there is no node-tree, don't allow this to flow to scene. */
        return NULL;
      }
      case SPACE_SEQ: /* Sequencer */
      {
        SpaceSeq *sseq = (SpaceSeq *)sl;

        /* For now, Grease Pencil data is associated with the space
         * (actually preview region only). */
        if (r_ptr) {
          RNA_pointer_create(screen_id, &RNA_SpaceSequenceEditor, sseq, r_ptr);
        }
        return &sseq->gpd;
      }
      case SPACE_IMAGE: /* Image/UV Editor */
      {
        SpaceImage *sima = (SpaceImage *)sl;

        /* For now, Grease Pencil data is associated with the space... */
        if (r_ptr) {
          RNA_pointer_create(screen_id, &RNA_SpaceImageEditor, sima, r_ptr);
        }
        return &sima->gpd;
      }
      case SPACE_CLIP: /* Nodes Editor */
      {
        SpaceClip *sc = (SpaceClip *)sl;
        MovieClip *clip = ED_space_clip_get_clip(sc);

        if (clip) {
          if (sc->gpencil_src == SC_GPENCIL_SRC_TRACK) {
            MovieTrackingTrack *track = BKE_tracking_track_get_active(&clip->tracking);

            if (!track) {
              return NULL;
            }

            if (r_ptr) {
              RNA_pointer_create(&clip->id, &RNA_MovieTrackingTrack, track, r_ptr);
            }
            return &track->gpd;
          }
          else {
            if (r_ptr) {
              RNA_id_pointer_create(&clip->id, r_ptr);
            }
            return &clip->gpd;
          }
        }
        break;
      }
      default: /* unsupported space */
        return NULL;
    }
  }

  return NULL;
}

/* Get pointer to active Grease Pencil datablock,
 * and an RNA-pointer to trace back to whatever owns it. */
bGPdata **ED_gpencil_data_get_pointers(const bContext *C, PointerRNA *r_ptr)
{
  ScrArea *area = CTX_wm_area(C);
  Object *ob = CTX_data_active_object(C);

  return ED_gpencil_data_get_pointers_direct(area, ob, r_ptr);
}

/* Get pointer to active Grease Pencil datablock,
 * and an RNA-pointer to trace back to whatever owns it. */
bGPdata **ED_annotation_data_get_pointers(const bContext *C, PointerRNA *r_ptr)
{
  ID *screen_id = (ID *)CTX_wm_screen(C);
  Scene *scene = CTX_data_scene(C);
  ScrArea *area = CTX_wm_area(C);

  return ED_annotation_data_get_pointers_direct(screen_id, area, scene, r_ptr);
}
/* -------------------------------------------------------- */

/* Get the active Grease Pencil datablock, when context is not available */
bGPdata *ED_gpencil_data_get_active_direct(ScrArea *area, Object *ob)
{
  bGPdata **gpd_ptr = ED_gpencil_data_get_pointers_direct(area, ob, NULL);
  return (gpd_ptr) ? *(gpd_ptr) : NULL;
}

/* Get the active Grease Pencil datablock, when context is not available */
bGPdata *ED_annotation_data_get_active_direct(ID *screen_id, ScrArea *area, Scene *scene)
{
  bGPdata **gpd_ptr = ED_annotation_data_get_pointers_direct(screen_id, area, scene, NULL);
  return (gpd_ptr) ? *(gpd_ptr) : NULL;
}

/**
 * Get the active Grease Pencil datablock
 */
bGPdata *ED_gpencil_data_get_active(const bContext *C)
{
  Object *ob = CTX_data_active_object(C);
  if ((ob == NULL) || (ob->type != OB_GPENCIL)) {
    return NULL;
  }
  return ob->data;
}

/* Get the active Grease Pencil datablock
 * \note This is the original (bmain) copy of the datablock, stored in files.
 * Do not use for reading evaluated copies of GP Objects data
 */
bGPdata *ED_annotation_data_get_active(const bContext *C)
{
  bGPdata **gpd_ptr = ED_annotation_data_get_pointers(C, NULL);
  return (gpd_ptr) ? *(gpd_ptr) : NULL;
}
/**
 * Get the evaluated copy of the active Grease Pencil datablock (where applicable)
 * - For the 3D View (i.e. "GP Objects"), this gives the evaluated copy of the GP datablock
 *   (i.e. a copy of the active GP datablock for the active object, where modifiers have been
 *   applied). This is needed to correctly work with "Copy-on-Write".
 * - For all other editors (i.e. "GP Annotations"), this just gives the active datablock
 *   like for #ED_gpencil_data_get_active()
 */
bGPdata *ED_gpencil_data_get_active_evaluated(const bContext *C)
{
  ScrArea *area = CTX_wm_area(C);

  const Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  Object *ob = CTX_data_active_object(C);
  Object *ob_eval = DEG_get_evaluated_object(depsgraph, ob);

  return ED_gpencil_data_get_active_direct(area, ob_eval);
}

/* -------------------------------------------------------- */

/**
 * Utility to check whether the r_ptr output of ED_gpencil_data_get_pointers()
 * is for annotation usage.
 */
bool ED_gpencil_data_owner_is_annotation(PointerRNA *owner_ptr)
{
  /* Key Assumption: If the pointer is an object, we're dealing with a GP Object's data.
   * Otherwise, the GP datablock is being used for annotations (i.e. everywhere else)
   */
  return ((owner_ptr) && (owner_ptr->type != &RNA_Object));
}

/* ******************************************************** */
/* Keyframe Indicator Checks */

/* Check whether there's an active GP keyframe on the current frame */
bool ED_gpencil_has_keyframe_v3d(Scene *UNUSED(scene), Object *ob, int cfra)
{
  if (ob && ob->data && (ob->type == OB_GPENCIL)) {
    bGPDlayer *gpl = BKE_gpencil_layer_active_get(ob->data);
    if (gpl) {
      if (gpl->actframe) {
        // XXX: assumes that frame has been fetched already
        return (gpl->actframe->framenum == cfra);
      }
      else {
        /* XXX: disabled as could be too much of a penalty */
        /* return BKE_gpencil_layer_frame_find(gpl, cfra); */
      }
    }
  }

  return false;
}

/* ******************************************************** */
/* Poll Callbacks */

/* poll callback for adding data/layers - special */
bool gp_add_poll(bContext *C)
{
  Object *ob = CTX_data_active_object(C);
  if (ob == NULL) {
    return false;
  }
  bGPdata *gpd = (bGPdata *)ob->data;

  return (gpd != NULL);
}

/* poll callback for checking if there is an active layer */
bool gp_active_layer_poll(bContext *C)
{
  Object *ob = CTX_data_active_object(C);
  if ((ob == NULL) || (ob->type != OB_GPENCIL)) {
    return false;
  }
  bGPdata *gpd = (bGPdata *)ob->data;
  bGPDlayer *gpl = BKE_gpencil_layer_active_get(gpd);

  return (gpl != NULL);
}

/* poll callback for checking if there is an active brush */
bool gp_active_brush_poll(bContext *C)
{
  ToolSettings *ts = CTX_data_tool_settings(C);
  Paint *paint = &ts->gp_paint->paint;
  if (paint) {
    return (paint->brush != NULL);
  }
  else {
    return false;
  }
}

/* ******************************************************** */
/* Dynamic Enums of GP Layers */
/* NOTE: These include an option to create a new layer and use that... */

/* Just existing layers */
const EnumPropertyItem *ED_gpencil_layers_enum_itemf(bContext *C,
                                                     PointerRNA *UNUSED(ptr),
                                                     PropertyRNA *UNUSED(prop),
                                                     bool *r_free)
{
  bGPdata *gpd = CTX_data_gpencil_data(C);
  bGPDlayer *gpl;
  EnumPropertyItem *item = NULL, item_tmp = {0};
  int totitem = 0;
  int i = 0;

  if (ELEM(NULL, C, gpd)) {
    return DummyRNA_DEFAULT_items;
  }

  /* Existing layers */
  for (gpl = gpd->layers.first; gpl; gpl = gpl->next, i++) {
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

/* Existing + Option to add/use new layer */
const EnumPropertyItem *ED_gpencil_layers_with_new_enum_itemf(bContext *C,
                                                              PointerRNA *UNUSED(ptr),
                                                              PropertyRNA *UNUSED(prop),
                                                              bool *r_free)
{
  bGPdata *gpd = CTX_data_gpencil_data(C);
  bGPDlayer *gpl;
  EnumPropertyItem *item = NULL, item_tmp = {0};
  int totitem = 0;
  int i = 0;

  if (ELEM(NULL, C, gpd)) {
    return DummyRNA_DEFAULT_items;
  }

  /* Create new layer */
  /* TODO: have some way of specifying that we don't want this? */

  const int tot = BLI_listbase_count(&gpd->layers);
  /* Existing layers */
  for (gpl = gpd->layers.last, i = 0; gpl; gpl = gpl->prev, i++) {
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
  {
    /* separator */
    RNA_enum_item_add_separator(&item, &totitem);

    /* "New Layer" entry */
    item_tmp.identifier = "__CREATE__";
    item_tmp.name = "New Layer";
    item_tmp.value = -1;
    item_tmp.icon = ICON_ADD;
    RNA_enum_item_add(&item, &totitem, &item_tmp);
  }

  RNA_enum_item_end(&item, &totitem);
  *r_free = true;

  return item;
}

/* Just existing Materials */
const EnumPropertyItem *ED_gpencil_material_enum_itemf(bContext *C,
                                                       PointerRNA *UNUSED(ptr),
                                                       PropertyRNA *UNUSED(prop),
                                                       bool *r_free)
{
  Object *ob = CTX_data_active_object(C);
  EnumPropertyItem *item = NULL, item_tmp = {0};
  int totitem = 0;
  int i = 0;

  if (ELEM(NULL, C, ob)) {
    return DummyRNA_DEFAULT_items;
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

/**
 * Check whether a given stroke segment is inside a circular brush
 *
 * \param mval: The current screen-space coordinates (midpoint) of the brush
 * \param mvalo: The previous screen-space coordinates (midpoint) of the brush (NOT CURRENTLY USED)
 * \param rad: The radius of the brush
 *
 * \param x0, y0: The screen-space x and y coordinates of the start of the stroke segment
 * \param x1, y1: The screen-space x and y coordinates of the end of the stroke segment
 */
bool gp_stroke_inside_circle(const float mval[2], int rad, int x0, int y0, int x1, int y1)
{
  /* simple within-radius check for now */
  const float screen_co_a[2] = {x0, y0};
  const float screen_co_b[2] = {x1, y1};

  if (edge_inside_circle(mval, rad, screen_co_a, screen_co_b)) {
    return true;
  }

  /* not inside */
  return false;
}

/* ******************************************************** */
/* Stroke Validity Testing */

/* Check whether given stroke can be edited given the supplied context */
/* TODO: do we need additional flags for screenspace vs dataspace? */
bool ED_gpencil_stroke_can_use_direct(const ScrArea *area, const bGPDstroke *gps)
{
  /* sanity check */
  if (ELEM(NULL, area, gps)) {
    return false;
  }

  /* filter stroke types by flags + spacetype */
  if (gps->flag & GP_STROKE_3DSPACE) {
    /* 3D strokes - only in 3D view */
    return ((area->spacetype == SPACE_VIEW3D) || (area->spacetype == SPACE_PROPERTIES));
  }
  else if (gps->flag & GP_STROKE_2DIMAGE) {
    /* Special "image" strokes - only in Image Editor */
    return (area->spacetype == SPACE_IMAGE);
  }
  else if (gps->flag & GP_STROKE_2DSPACE) {
    /* 2D strokes (dataspace) - for any 2D view (i.e. everything other than 3D view) */
    return (area->spacetype != SPACE_VIEW3D);
  }
  else {
    /* view aligned - anything goes */
    return true;
  }
}

/* Check whether given stroke can be edited in the current context */
bool ED_gpencil_stroke_can_use(const bContext *C, const bGPDstroke *gps)
{
  ScrArea *area = CTX_wm_area(C);
  return ED_gpencil_stroke_can_use_direct(area, gps);
}

/* Check whether given stroke can be edited for the current color */
bool ED_gpencil_stroke_color_use(Object *ob, const bGPDlayer *gpl, const bGPDstroke *gps)
{
  /* check if the color is editable */
  MaterialGPencilStyle *gp_style = BKE_gpencil_material_settings(ob, gps->mat_nr + 1);

  if (gp_style != NULL) {
    if (gp_style->flag & GP_MATERIAL_HIDE) {
      return false;
    }
    if (((gpl->flag & GP_LAYER_UNLOCK_COLOR) == 0) && (gp_style->flag & GP_MATERIAL_LOCKED)) {
      return false;
    }
  }

  return true;
}

/* ******************************************************** */
/* Space Conversion */

/**
 * Init settings for stroke point space conversions
 *
 * \param r_gsc: [out] The space conversion settings struct, populated with necessary params
 */
void gp_point_conversion_init(bContext *C, GP_SpaceConversion *r_gsc)
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
    struct Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
    View3D *v3d = (View3D *)CTX_wm_space_data(C);
    RegionView3D *rv3d = region->regiondata;

    /* init 3d depth buffers */
    view3d_operator_needs_opengl(C);

    view3d_region_operator_needs_opengl(win, region);
    ED_view3d_autodist_init(depsgraph, region, v3d, 0);

    /* for camera view set the subrect */
    if (rv3d->persp == RV3D_CAMOB) {
      ED_view3d_calc_camera_border(
          scene, depsgraph, region, v3d, rv3d, &r_gsc->subrect_data, true);
      r_gsc->subrect = &r_gsc->subrect_data;
    }
  }
}

/**
 * Convert point to parent space
 *
 * \param pt: Original point
 * \param diff_mat: Matrix with the difference between original parent matrix
 * \param[out] r_pt: Pointer to new point after apply matrix
 */
void gp_point_to_parent_space(const bGPDspoint *pt, const float diff_mat[4][4], bGPDspoint *r_pt)
{
  float fpt[3];

  mul_v3_m4v3(fpt, diff_mat, &pt->x);
  copy_v3_v3(&r_pt->x, fpt);
}

/**
 * Change position relative to parent object
 */
void gp_apply_parent(Depsgraph *depsgraph, Object *obact, bGPDlayer *gpl, bGPDstroke *gps)
{
  bGPDspoint *pt;
  int i;

  /* undo matrix */
  float diff_mat[4][4];
  float inverse_diff_mat[4][4];
  float fpt[3];

  BKE_gpencil_parent_matrix_get(depsgraph, obact, gpl, diff_mat);
  invert_m4_m4(inverse_diff_mat, diff_mat);

  for (i = 0; i < gps->totpoints; i++) {
    pt = &gps->points[i];
    mul_v3_m4v3(fpt, inverse_diff_mat, &pt->x);
    copy_v3_v3(&pt->x, fpt);
  }
}

/**
 * Change point position relative to parent object
 */
void gp_apply_parent_point(Depsgraph *depsgraph, Object *obact, bGPDlayer *gpl, bGPDspoint *pt)
{
  /* undo matrix */
  float diff_mat[4][4];
  float inverse_diff_mat[4][4];
  float fpt[3];

  BKE_gpencil_parent_matrix_get(depsgraph, obact, gpl, diff_mat);
  invert_m4_m4(inverse_diff_mat, diff_mat);

  mul_v3_m4v3(fpt, inverse_diff_mat, &pt->x);
  copy_v3_v3(&pt->x, fpt);
}

/**
 * Convert a Grease Pencil coordinate (i.e. can be 2D or 3D) to screenspace (2D)
 *
 * \param[out] r_x: The screen-space x-coordinate of the point
 * \param[out] r_y: The screen-space y-coordinate of the point
 *
 * \warning This assumes that the caller has already checked
 * whether the stroke in question can be drawn.
 */
void gp_point_to_xy(
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
    if (ED_view3d_project_int_global(region, &pt->x, xyval, V3D_PROJ_TEST_NOP) ==
        V3D_PROJ_RET_OK) {
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
    if (subrect == NULL) {
      /* normal 3D view (or view space) */
      *r_x = (int)(pt->x / 100 * region->winx);
      *r_y = (int)(pt->y / 100 * region->winy);
    }
    else {
      /* camera view, use subrect */
      *r_x = (int)((pt->x / 100) * BLI_rctf_size_x(subrect)) + subrect->xmin;
      *r_y = (int)((pt->y / 100) * BLI_rctf_size_y(subrect)) + subrect->ymin;
    }
  }
}

/**
 * Convert a Grease Pencil coordinate (i.e. can be 2D or 3D) to screenspace (2D).
 *
 * Just like #gp_point_to_xy(), except the resulting coordinates are floats not ints.
 * Use this version to solve "stair-step" artifacts which may arise when
 * roundtripping the calculations.
 *
 * \param r_x[out]: The screen-space x-coordinate of the point.
 * \param r_y[out]: The screen-space y-coordinate of the point.
 *
 * \warning This assumes that the caller has already checked
 * whether the stroke in question can be drawn.
 */
void gp_point_to_xy_fl(const GP_SpaceConversion *gsc,
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
        V3D_PROJ_RET_OK) {
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
      *r_x = (float)t_x;
      *r_y = (float)t_y;
    }
  }
  else {
    if (subrect == NULL) {
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

/**
 * generic based on gp_point_to_xy_fl
 */
void gp_point_3d_to_xy(const GP_SpaceConversion *gsc,
                       const short flag,
                       const float pt[3],
                       float xy[2])
{
  const ARegion *region = gsc->region;
  const View2D *v2d = gsc->v2d;
  const rctf *subrect = gsc->subrect;
  float xyval[2];

  /* sanity checks */
  BLI_assert((gsc->area->spacetype == SPACE_VIEW3D));

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
      xy[0] = (float)t_x;
      xy[1] = (float)t_y;
    }
  }
  else {
    if (subrect == NULL) {
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

/**
 * Project screenspace coordinates to 3D-space
 *
 * For use with editing tools where it is easier to perform the operations in 2D,
 * and then later convert the transformed points back to 3D.
 *
 * \param screen_co: The screenspace 2D coordinates to convert to
 * \param r_out: The resulting 3D coordinates of the input point
 *
 * \note We include this as a utility function, since the standard method
 * involves quite a few steps, which are invariably always the same
 * for all GPencil operations. So, it's nicer to just centralize these.
 *
 * \warning Assumes that it is getting called in a 3D view only.
 */
bool gp_point_xy_to_3d(const GP_SpaceConversion *gsc,
                       Scene *scene,
                       const float screen_co[2],
                       float r_out[3])
{
  const RegionView3D *rv3d = gsc->region->regiondata;
  float rvec[3];

  ED_gpencil_drawing_reference_get(scene, gsc->ob, scene->toolsettings->gpencil_v3d_align, rvec);

  float zfac = ED_view3d_calc_zfac(rv3d, rvec, NULL);

  float mval_f[2], mval_prj[2];
  float dvec[3];

  copy_v2_v2(mval_f, screen_co);

  if (ED_view3d_project_float_global(gsc->region, rvec, mval_prj, V3D_PROJ_TEST_NOP) ==
      V3D_PROJ_RET_OK) {
    sub_v2_v2v2(mval_f, mval_prj, mval_f);
    ED_view3d_win_to_delta(gsc->region, mval_f, dvec, zfac);
    sub_v3_v3v3(r_out, rvec, dvec);

    return true;
  }
  else {
    zero_v3(r_out);

    return false;
  }
}

/**
 * Convert tGPspoint (temporary 2D/screenspace point data used by GP modal operators)
 * to 3D coordinates.
 *
 * \param point2D: The screenspace 2D point data to convert.
 * \param depth: Depth array (via #ED_view3d_autodist_depth()).
 * \param[out] r_out: The resulting 2D point data.
 */
void gp_stroke_convertcoords_tpoint(Scene *scene,
                                    ARegion *region,
                                    Object *ob,
                                    const tGPspoint *point2D,
                                    float *depth,
                                    float r_out[3])
{
  ToolSettings *ts = scene->toolsettings;

  int mval_i[2];
  round_v2i_v2fl(mval_i, &point2D->x);

  if ((depth != NULL) && (ED_view3d_autodist_simple(region, mval_i, r_out, 0, depth))) {
    /* projecting onto 3D-Geometry
     * - nothing more needs to be done here, since view_autodist_simple() has already done it
     */
  }
  else {
    float mval_f[2] = {point2D->x, point2D->y};
    float mval_prj[2];
    float rvec[3], dvec[3];
    float zfac;

    /* Current method just converts each point in screen-coordinates to
     * 3D-coordinates using the 3D-cursor as reference.
     */
    ED_gpencil_drawing_reference_get(scene, ob, ts->gpencil_v3d_align, rvec);
    zfac = ED_view3d_calc_zfac(region->regiondata, rvec, NULL);

    if (ED_view3d_project_float_global(region, rvec, mval_prj, V3D_PROJ_TEST_NOP) ==
        V3D_PROJ_RET_OK) {
      sub_v2_v2v2(mval_f, mval_prj, mval_f);
      ED_view3d_win_to_delta(region, mval_f, dvec, zfac);
      sub_v3_v3v3(r_out, rvec, dvec);
    }
    else {
      zero_v3(r_out);
    }
  }
}

/**
 * Get drawing reference point for conversion or projection of the stroke
 * \param[out] r_vec : Reference point found
 */
void ED_gpencil_drawing_reference_get(const Scene *scene,
                                      const Object *ob,
                                      char align_flag,
                                      float r_vec[3])
{
  const float *fp = scene->cursor.location;

  /* if using a gpencil object at cursor mode, can use the location of the object */
  if (align_flag & GP_PROJECT_VIEWSPACE) {
    if (ob && (ob->type == OB_GPENCIL)) {
      /* fallback (no strokes) - use cursor or object location */
      if (align_flag & GP_PROJECT_CURSOR) {
        /* use 3D-cursor */
        copy_v3_v3(r_vec, fp);
      }
      else {
        /* use object location */
        copy_v3_v3(r_vec, ob->obmat[3]);
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
  GP_SpaceConversion gsc = {NULL};

  bGPDspoint *pt;
  int i;
  float diff_mat[4][4];
  float inverse_diff_mat[4][4];

  /* init space conversion stuff */
  gp_point_conversion_init(C, &gsc);

  BKE_gpencil_parent_matrix_get(depsgraph, ob, gpl, diff_mat);
  invert_m4_m4(inverse_diff_mat, diff_mat);

  /* Adjust each point */
  for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
    float xy[2];

    bGPDspoint pt2;
    gp_point_to_parent_space(pt, diff_mat, &pt2);
    gp_point_to_xy_fl(&gsc, gps, &pt2, &xy[0], &xy[1]);

    /* Planar - All on same plane parallel to the viewplane */
    gp_point_xy_to_3d(&gsc, scene, xy, &pt->x);

    /* Unapply parent corrections */
    mul_m4_v3(inverse_diff_mat, &pt->x);
  }
}

/**
 * Reproject all points of the stroke to a plane locked to axis to avoid stroke offset
 */
void ED_gp_project_stroke_to_plane(const Scene *scene,
                                   const Object *ob,
                                   const RegionView3D *rv3d,
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
    if (ob && (ob->type == OB_GPENCIL)) {
      float mat[4][4];
      copy_m4_m4(mat, ob->obmat);

      /* move origin to cursor */
      if (ts->gpencil_v3d_align & GP_PROJECT_CURSOR) {
        copy_v3_v3(mat[3], cursor->location);
      }

      mul_mat3_m4_v3(mat, plane_normal);
    }
  }
  else {
    float scale[3] = {1.0f, 1.0f, 1.0f};
    plane_normal[2] = 1.0f;
    float mat[4][4];
    loc_eul_size_to_mat4(mat, cursor->location, cursor->rotation_euler, scale);

    /* move origin to object */
    if ((ts->gpencil_v3d_align & GP_PROJECT_CURSOR) == 0) {
      copy_v3_v3(mat[3], ob->obmat[3]);
    }

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

/**
 * Reproject given point to a plane locked to axis to avoid stroke offset
 * \param[in,out] pt: Point to affect
 */
void ED_gp_project_point_to_plane(const Scene *scene,
                                  const Object *ob,
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
    if (ob && (ob->type == OB_GPENCIL)) {
      float mat[4][4];
      copy_m4_m4(mat, ob->obmat);

      /* move origin to cursor */
      if (ts->gpencil_v3d_align & GP_PROJECT_CURSOR) {
        copy_v3_v3(mat[3], cursor->location);
      }

      mul_mat3_m4_v3(mat, plane_normal);
    }
  }
  else {
    float scale[3] = {1.0f, 1.0f, 1.0f};
    plane_normal[2] = 1.0f;
    float mat[4][4];
    loc_eul_size_to_mat4(mat, cursor->location, cursor->rotation_euler, scale);

    /* move origin to object */
    if ((ts->gpencil_v3d_align & GP_PROJECT_CURSOR) == 0) {
      copy_v3_v3(mat[3], ob->obmat[3]);
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

/**
 * Subdivide a stroke once, by adding a point half way between each pair of existing points
 * \param gps: Stroke data
 * \param subdivide: Number of times to subdivide
 */
void gp_subdivide_stroke(bGPDstroke *gps, const int subdivide)
{
  bGPDspoint *temp_points;
  int totnewpoints, oldtotpoints;
  int i2;

  /* loop as many times as levels */
  for (int s = 0; s < subdivide; s++) {
    totnewpoints = gps->totpoints - 1;
    /* duplicate points in a temp area */
    temp_points = MEM_dupallocN(gps->points);
    oldtotpoints = gps->totpoints;

    /* resize the points arrays */
    gps->totpoints += totnewpoints;
    gps->points = MEM_recallocN(gps->points, sizeof(*gps->points) * gps->totpoints);
    if (gps->dvert != NULL) {
      gps->dvert = MEM_recallocN(gps->dvert, sizeof(*gps->dvert) * gps->totpoints);
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

      if (gps->dvert != NULL) {
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

      if (gps->dvert != NULL) {
        MDeformVert *dvert_final = &gps->dvert[i2];
        dvert_final->totweight = 0;
        dvert_final->dw = NULL;
      }

      i2 += 2;
    }

    MEM_SAFE_FREE(temp_points);

    /* move points to smooth stroke */
    /* duplicate points in a temp area with the new subdivide data */
    temp_points = MEM_dupallocN(gps->points);

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
  BKE_gpencil_stroke_geometry_update(gps);
}

/* Reset parent matrix for all layers. */
void ED_gpencil_reset_layers_parent(Depsgraph *depsgraph, Object *obact, bGPdata *gpd)
{
  bGPDspoint *pt;
  int i;
  float diff_mat[4][4];
  float cur_mat[4][4];
  float gpl_loc[3];
  zero_v3(gpl_loc);

  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    if (gpl->parent != NULL) {
      /* calculate new matrix */
      if ((gpl->partype == PAROBJECT) || (gpl->partype == PARSKEL)) {
        invert_m4_m4(cur_mat, gpl->parent->obmat);
        copy_v3_v3(gpl_loc, obact->obmat[3]);
      }
      else if (gpl->partype == PARBONE) {
        bPoseChannel *pchan = BKE_pose_channel_find_name(gpl->parent->pose, gpl->parsubstr);
        if (pchan) {
          float tmp_mat[4][4];
          mul_m4_m4m4(tmp_mat, gpl->parent->obmat, pchan->pose_mat);
          invert_m4_m4(cur_mat, tmp_mat);
          copy_v3_v3(gpl_loc, obact->obmat[3]);
        }
      }

      /* only redo if any change */
      if (!equals_m4m4(gpl->inverse, cur_mat)) {
        /* first apply current transformation to all strokes */
        BKE_gpencil_parent_matrix_get(depsgraph, obact, gpl, diff_mat);
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

/* Helper function to create new OB_GPENCIL Object */
Object *ED_gpencil_add_object(bContext *C, const float loc[3], ushort local_view_bits)
{
  float rot[3] = {0.0f};

  Object *ob = ED_object_add_type(C, OB_GPENCIL, NULL, loc, rot, false, local_view_bits);

  /* create default brushes and colors */
  ED_gpencil_add_defaults(C, ob);

  return ob;
}

/* Helper function to create default colors and drawing brushes */
void ED_gpencil_add_defaults(bContext *C, Object *ob)
{
  Main *bmain = CTX_data_main(C);
  ToolSettings *ts = CTX_data_tool_settings(C);

  BKE_paint_ensure(ts, (Paint **)&ts->gp_paint);
  Paint *paint = &ts->gp_paint->paint;
  /* if not exist, create a new one */
  if ((paint->brush == NULL) || (paint->brush->gpencil_settings == NULL)) {
    /* create new brushes */
    BKE_brush_gpencil_paint_presets(bmain, ts, true);
  }

  /* ensure a color exists and is assigned to object */
  BKE_gpencil_object_material_ensure_from_active_input_toolsettings(bmain, ob, ts);

  /* ensure multiframe falloff curve */
  if (ts->gp_sculpt.cur_falloff == NULL) {
    ts->gp_sculpt.cur_falloff = BKE_curvemapping_add(1, 0.0f, 0.0f, 1.0f, 1.0f);
    CurveMapping *gp_falloff_curve = ts->gp_sculpt.cur_falloff;
    BKE_curvemapping_initialize(gp_falloff_curve);
    BKE_curvemap_reset(gp_falloff_curve->cm,
                       &gp_falloff_curve->clipr,
                       CURVE_PRESET_GAUSS,
                       CURVEMAP_SLOPE_POSITIVE);
  }
}

/* ******************************************************** */
/* Vertex Groups */

/* assign points to vertex group */
void ED_gpencil_vgroup_assign(bContext *C, Object *ob, float weight)
{
  bGPdata *gpd = (bGPdata *)ob->data;
  const bool is_multiedit = (bool)GPENCIL_MULTIEDIT_SESSIONS_ON(gpd);
  const int def_nr = ob->actdef - 1;
  if (!BLI_findlink(&ob->defbase, def_nr)) {
    return;
  }

  CTX_DATA_BEGIN (C, bGPDlayer *, gpl, editable_gpencil_layers) {
    bGPDframe *init_gpf = (is_multiedit) ? gpl->frames.first : gpl->actframe;
    bGPDstroke *gps = NULL;

    for (bGPDframe *gpf = init_gpf; gpf; gpf = gpf->next) {
      if ((gpf == gpl->actframe) || ((gpf->flag & GP_FRAME_SELECT) && (is_multiedit))) {
        if (gpf == NULL) {
          continue;
        }

        for (gps = gpf->strokes.first; gps; gps = gps->next) {

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

      /* if not multiedit, exit loop*/
      if (!is_multiedit) {
        break;
      }
    }
  }
  CTX_DATA_END;
}

/* remove points from vertex group */
void ED_gpencil_vgroup_remove(bContext *C, Object *ob)
{
  bGPdata *gpd = (bGPdata *)ob->data;
  const bool is_multiedit = (bool)GPENCIL_MULTIEDIT_SESSIONS_ON(gpd);
  const int def_nr = ob->actdef - 1;
  if (!BLI_findlink(&ob->defbase, def_nr)) {
    return;
  }

  CTX_DATA_BEGIN (C, bGPDlayer *, gpl, editable_gpencil_layers) {
    bGPDframe *init_gpf = (is_multiedit) ? gpl->frames.first : gpl->actframe;
    bGPDstroke *gps = NULL;

    for (bGPDframe *gpf = init_gpf; gpf; gpf = gpf->next) {
      if ((gpf == gpl->actframe) || ((gpf->flag & GP_FRAME_SELECT) && (is_multiedit))) {
        if (gpf == NULL) {
          continue;
        }

        for (gps = gpf->strokes.first; gps; gps = gps->next) {

          /* skip strokes that are invalid for current view */
          if (ED_gpencil_stroke_can_use(C, gps) == false) {
            continue;
          }

          for (int i = 0; i < gps->totpoints; i++) {
            bGPDspoint *pt = &gps->points[i];
            if (gps->dvert == NULL) {
              continue;
            }
            MDeformVert *dvert = &gps->dvert[i];

            if ((pt->flag & GP_SPOINT_SELECT) && (dvert->totweight > 0)) {
              MDeformWeight *dw = BKE_defvert_find_index(dvert, def_nr);
              if (dw != NULL) {
                BKE_defvert_remove_group(dvert, dw);
              }
            }
          }
        }
      }

      /* if not multiedit, exit loop*/
      if (!is_multiedit) {
        break;
      }
    }
  }
  CTX_DATA_END;
}

/* select points of vertex group */
void ED_gpencil_vgroup_select(bContext *C, Object *ob)
{
  bGPdata *gpd = (bGPdata *)ob->data;
  const bool is_multiedit = (bool)GPENCIL_MULTIEDIT_SESSIONS_ON(gpd);
  const int def_nr = ob->actdef - 1;
  if (!BLI_findlink(&ob->defbase, def_nr)) {
    return;
  }

  CTX_DATA_BEGIN (C, bGPDlayer *, gpl, editable_gpencil_layers) {
    bGPDframe *init_gpf = (is_multiedit) ? gpl->frames.first : gpl->actframe;
    bGPDstroke *gps = NULL;

    for (bGPDframe *gpf = init_gpf; gpf; gpf = gpf->next) {
      if ((gpf == gpl->actframe) || ((gpf->flag & GP_FRAME_SELECT) && (is_multiedit))) {
        if (gpf == NULL) {
          continue;
        }

        for (gps = gpf->strokes.first; gps; gps = gps->next) {

          /* skip strokes that are invalid for current view */
          if (ED_gpencil_stroke_can_use(C, gps) == false) {
            continue;
          }

          for (int i = 0; i < gps->totpoints; i++) {
            bGPDspoint *pt = &gps->points[i];
            if (gps->dvert == NULL) {
              continue;
            }
            MDeformVert *dvert = &gps->dvert[i];

            if (BKE_defvert_find_index(dvert, def_nr) != NULL) {
              pt->flag |= GP_SPOINT_SELECT;
              gps->flag |= GP_STROKE_SELECT;
            }
          }
        }
      }

      /* if not multiedit, exit loop*/
      if (!is_multiedit) {
        break;
      }
    }
  }
  CTX_DATA_END;
}

/* unselect points of vertex group */
void ED_gpencil_vgroup_deselect(bContext *C, Object *ob)
{
  bGPdata *gpd = (bGPdata *)ob->data;
  const bool is_multiedit = (bool)GPENCIL_MULTIEDIT_SESSIONS_ON(gpd);
  const int def_nr = ob->actdef - 1;
  if (!BLI_findlink(&ob->defbase, def_nr)) {
    return;
  }

  CTX_DATA_BEGIN (C, bGPDlayer *, gpl, editable_gpencil_layers) {
    bGPDframe *init_gpf = (is_multiedit) ? gpl->frames.first : gpl->actframe;
    bGPDstroke *gps = NULL;

    for (bGPDframe *gpf = init_gpf; gpf; gpf = gpf->next) {
      if ((gpf == gpl->actframe) || ((gpf->flag & GP_FRAME_SELECT) && (is_multiedit))) {
        if (gpf == NULL) {
          continue;
        }

        for (gps = gpf->strokes.first; gps; gps = gps->next) {

          /* skip strokes that are invalid for current view */
          if (ED_gpencil_stroke_can_use(C, gps) == false) {
            continue;
          }

          for (int i = 0; i < gps->totpoints; i++) {
            bGPDspoint *pt = &gps->points[i];
            if (gps->dvert == NULL) {
              continue;
            }
            MDeformVert *dvert = &gps->dvert[i];

            if (BKE_defvert_find_index(dvert, def_nr) != NULL) {
              pt->flag &= ~GP_SPOINT_SELECT;
            }
          }
        }
      }

      /* if not multiedit, exit loop*/
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
static bool gp_check_cursor_region(bContext *C, int mval_i[2])
{
  ARegion *region = CTX_wm_region(C);
  ScrArea *area = CTX_wm_area(C);
  Object *ob = CTX_data_active_object(C);

  if ((ob == NULL) || (!ELEM(ob->mode,
                             OB_MODE_PAINT_GPENCIL,
                             OB_MODE_SCULPT_GPENCIL,
                             OB_MODE_WEIGHT_GPENCIL,
                             OB_MODE_VERTEX_GPENCIL))) {
    return false;
  }

  /* TODO: add more spacetypes */
  if (!ELEM(area->spacetype, SPACE_VIEW3D)) {
    return false;
  }
  if ((region) && (region->regiontype != RGN_TYPE_WINDOW)) {
    return false;
  }
  else if (region) {
    return BLI_rcti_isect_pt_v(&region->winrct, mval_i);
  }
  else {
    return false;
  }
}

/* draw eraser cursor */
void ED_gpencil_brush_draw_eraser(Brush *brush, int x, int y)
{
  short radius = (short)brush->size;

  GPUVertFormat *format = immVertexFormat();
  const uint shdr_pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
  immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);

  GPU_line_smooth(true);
  GPU_blend(true);
  glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

  immUniformColor4ub(255, 100, 100, 20);
  imm_draw_circle_fill_2d(shdr_pos, x, y, radius, 40);

  immUnbindProgram();

  immBindBuiltinProgram(GPU_SHADER_2D_LINE_DASHED_UNIFORM_COLOR);

  float viewport_size[4];
  glGetFloatv(GL_VIEWPORT, viewport_size);
  immUniform2f("viewport_size", viewport_size[2], viewport_size[3]);

  immUniformColor4f(1.0f, 0.39f, 0.39f, 0.78f);
  immUniform1i("colors_len", 0); /* "simple" mode */
  immUniform1f("dash_width", 12.0f);
  immUniform1f("dash_factor", 0.5f);

  imm_draw_circle_wire_2d(shdr_pos,
                          x,
                          y,
                          radius,
                          /* XXX Dashed shader gives bad results with sets of small segments
                           * currently, temp hack around the issue. :( */
                          max_ii(8, radius / 2)); /* was fixed 40 */

  immUnbindProgram();

  GPU_blend(false);
  GPU_line_smooth(false);
}

static bool gp_brush_cursor_poll(bContext *C)
{
  if (WM_toolsystem_active_tool_is_brush(C)) {
    return true;
  }
  return false;
}

/* Helper callback for drawing the cursor itself */
static void gp_brush_cursor_draw(bContext *C, int x, int y, void *customdata)
{
  Scene *scene = CTX_data_scene(C);
  Object *ob = CTX_data_active_object(C);
  ARegion *region = CTX_wm_region(C);
  Paint *paint = BKE_paint_get_active_from_context(C);

  bGPdata *gpd = ED_gpencil_data_get_active(C);
  Brush *brush = NULL;
  Material *ma = NULL;
  MaterialGPencilStyle *gp_style = NULL;
  float *last_mouse_position = customdata;

  /* default radius and color */
  float color[3] = {1.0f, 1.0f, 1.0f};
  float darkcolor[3];
  float radius = 3.0f;

  int mval_i[2] = {x, y};
  /* check if cursor is in drawing region and has valid datablock */
  if ((!gp_check_cursor_region(C, mval_i)) || (gpd == NULL)) {
    return;
  }

  /* for paint use paint brush size and color */
  if (gpd->flag & GP_DATA_STROKE_PAINTMODE) {
    brush = scene->toolsettings->gp_paint->paint.brush;
    if ((brush == NULL) || (brush->gpencil_settings == NULL)) {
      return;
    }

    /* while drawing hide */
    if ((gpd->runtime.sbuffer_used > 0) &&
        ((brush->gpencil_settings->flag & GP_BRUSH_STABILIZE_MOUSE) == 0) &&
        ((brush->gpencil_settings->flag & GP_BRUSH_STABILIZE_MOUSE_TEMP) == 0)) {
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

      /* after some testing, display the size of the brush is not practical because
       * is too disruptive and the size of cursor does not change with zoom factor.
       * The decision was to use a fix size, instead of brush->thickness value.
       */
      if ((gp_style) && (GPENCIL_PAINT_MODE(gpd)) &&
          ((brush->gpencil_settings->flag & GP_BRUSH_STABILIZE_MOUSE) == 0) &&
          ((brush->gpencil_settings->flag & GP_BRUSH_STABILIZE_MOUSE_TEMP) == 0) &&
          (brush->gpencil_tool == GPAINT_TOOL_DRAW)) {
        radius = 2.0f;
        copy_v3_v3(color, gp_style->stroke_rgba);
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
    brush = scene->toolsettings->gp_sculptpaint->paint.brush;
    if ((brush == NULL) || (brush->gpencil_settings == NULL)) {
      return;
    }
    if ((paint->flags & PAINT_SHOW_BRUSH) == 0) {
      return;
    }

    radius = brush->size;
    if (brush->gpencil_settings->sculpt_flag &
        (GP_SCULPT_FLAG_INVERT | GP_SCULPT_FLAG_TMP_INVERT)) {
      copy_v3_v3(color, brush->sub_col);
    }
    else {
      copy_v3_v3(color, brush->add_col);
    }
  }

  /* Weight Paint */
  if (GPENCIL_WEIGHT_MODE(gpd)) {
    brush = scene->toolsettings->gp_weightpaint->paint.brush;
    if ((brush == NULL) || (brush->gpencil_settings == NULL)) {
      return;
    }
    if ((paint->flags & PAINT_SHOW_BRUSH) == 0) {
      return;
    }

    radius = brush->size;
    if (brush->gpencil_settings->sculpt_flag &
        (GP_SCULPT_FLAG_INVERT | GP_SCULPT_FLAG_TMP_INVERT)) {
      copy_v3_v3(color, brush->sub_col);
    }
    else {
      copy_v3_v3(color, brush->add_col);
    }
  }

  /* For Vertex Paint use brush size. */
  if (GPENCIL_VERTEX_MODE(gpd)) {
    brush = scene->toolsettings->gp_vertexpaint->paint.brush;
    if ((brush == NULL) || (brush->gpencil_settings == NULL)) {
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
  immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);

  GPU_line_smooth(true);
  GPU_blend(true);

  /* Inner Ring: Color from UI panel */
  immUniformColor4f(color[0], color[1], color[2], 0.8f);
  if ((gp_style) && (GPENCIL_PAINT_MODE(gpd)) &&
      ((brush->gpencil_settings->flag & GP_BRUSH_STABILIZE_MOUSE) == 0) &&
      ((brush->gpencil_settings->flag & GP_BRUSH_STABILIZE_MOUSE_TEMP) == 0) &&
      (brush->gpencil_tool == GPAINT_TOOL_DRAW)) {
    imm_draw_circle_fill_2d(pos, x, y, radius, 40);
  }
  else {
    imm_draw_circle_wire_2d(pos, x, y, radius, 40);
  }

  /* Outer Ring: Dark color for contrast on light backgrounds (e.g. gray on white) */
  mul_v3_v3fl(darkcolor, color, 0.40f);
  immUniformColor4f(darkcolor[0], darkcolor[1], darkcolor[2], 0.8f);
  imm_draw_circle_wire_2d(pos, x, y, radius + 1, 40);

  GPU_blend(false);
  GPU_line_smooth(false);

  /* Draw line for lazy mouse */
  if ((last_mouse_position) && (brush->gpencil_settings->flag & GP_BRUSH_STABILIZE_MOUSE_TEMP)) {
    GPU_line_smooth(true);
    GPU_blend(true);

    copy_v3_v3(color, brush->add_col);
    immUniformColor4f(color[0], color[1], color[2], 0.8f);

    immBegin(GPU_PRIM_LINES, 2);
    immVertex2f(pos, x, y);
    immVertex2f(pos,
                last_mouse_position[0] + region->winrct.xmin,
                last_mouse_position[1] + region->winrct.ymin);
    immEnd();

    GPU_blend(false);
    GPU_line_smooth(false);
  }

  immUnbindProgram();
}

/* Turn brush cursor in on/off */
void ED_gpencil_toggle_brush_cursor(bContext *C, bool enable, void *customdata)
{
  Scene *scene = CTX_data_scene(C);
  GP_Sculpt_Settings *gset = &scene->toolsettings->gp_sculpt;
  float *lastpost = customdata;

  if (gset->paintcursor && !enable) {
    /* clear cursor */
    WM_paint_cursor_end(gset->paintcursor);
    gset->paintcursor = NULL;
  }
  else if (enable) {
    /* in some situations cursor could be duplicated, so it is better disable first if exist */
    if (gset->paintcursor) {
      /* clear cursor */
      WM_paint_cursor_end(gset->paintcursor);
      gset->paintcursor = NULL;
    }
    /* enable cursor */
    gset->paintcursor = WM_paint_cursor_activate(SPACE_TYPE_ANY,
                                                 RGN_TYPE_ANY,
                                                 gp_brush_cursor_poll,
                                                 gp_brush_cursor_draw,
                                                 (lastpost) ? customdata : NULL);
  }
}

/* set object modes */
void ED_gpencil_setup_modes(bContext *C, bGPdata *gpd, int newmode)
{
  if (!gpd) {
    return;
  }

  switch (newmode) {
    case OB_MODE_EDIT_GPENCIL:
      gpd->flag |= GP_DATA_STROKE_EDITMODE;
      gpd->flag &= ~GP_DATA_STROKE_PAINTMODE;
      gpd->flag &= ~GP_DATA_STROKE_SCULPTMODE;
      gpd->flag &= ~GP_DATA_STROKE_WEIGHTMODE;
      gpd->flag &= ~GP_DATA_STROKE_VERTEXMODE;
      ED_gpencil_toggle_brush_cursor(C, false, NULL);
      break;
    case OB_MODE_PAINT_GPENCIL:
      gpd->flag &= ~GP_DATA_STROKE_EDITMODE;
      gpd->flag |= GP_DATA_STROKE_PAINTMODE;
      gpd->flag &= ~GP_DATA_STROKE_SCULPTMODE;
      gpd->flag &= ~GP_DATA_STROKE_WEIGHTMODE;
      gpd->flag &= ~GP_DATA_STROKE_VERTEXMODE;
      ED_gpencil_toggle_brush_cursor(C, true, NULL);
      break;
    case OB_MODE_SCULPT_GPENCIL:
      gpd->flag &= ~GP_DATA_STROKE_EDITMODE;
      gpd->flag &= ~GP_DATA_STROKE_PAINTMODE;
      gpd->flag |= GP_DATA_STROKE_SCULPTMODE;
      gpd->flag &= ~GP_DATA_STROKE_WEIGHTMODE;
      gpd->flag &= ~GP_DATA_STROKE_VERTEXMODE;
      ED_gpencil_toggle_brush_cursor(C, true, NULL);
      break;
    case OB_MODE_WEIGHT_GPENCIL:
      gpd->flag &= ~GP_DATA_STROKE_EDITMODE;
      gpd->flag &= ~GP_DATA_STROKE_PAINTMODE;
      gpd->flag &= ~GP_DATA_STROKE_SCULPTMODE;
      gpd->flag |= GP_DATA_STROKE_WEIGHTMODE;
      gpd->flag &= ~GP_DATA_STROKE_VERTEXMODE;
      ED_gpencil_toggle_brush_cursor(C, true, NULL);
      break;
    case OB_MODE_VERTEX_GPENCIL:
      gpd->flag &= ~GP_DATA_STROKE_EDITMODE;
      gpd->flag &= ~GP_DATA_STROKE_PAINTMODE;
      gpd->flag &= ~GP_DATA_STROKE_SCULPTMODE;
      gpd->flag &= ~GP_DATA_STROKE_WEIGHTMODE;
      gpd->flag |= GP_DATA_STROKE_VERTEXMODE;
      ED_gpencil_toggle_brush_cursor(C, true, NULL);
      break;
    default:
      gpd->flag &= ~GP_DATA_STROKE_EDITMODE;
      gpd->flag &= ~GP_DATA_STROKE_PAINTMODE;
      gpd->flag &= ~GP_DATA_STROKE_SCULPTMODE;
      gpd->flag &= ~GP_DATA_STROKE_WEIGHTMODE;
      gpd->flag &= ~GP_DATA_STROKE_VERTEXMODE;
      ED_gpencil_toggle_brush_cursor(C, false, NULL);
      break;
  }
}

/* helper to convert 2d to 3d for simple drawing buffer */
static void gpencil_stroke_convertcoords(ARegion *region,
                                         const tGPspoint *point2D,
                                         const float origin[3],
                                         float out[3])
{
  float mval_f[2] = {(float)point2D->x, (float)point2D->y};
  float mval_prj[2];
  float rvec[3], dvec[3];
  float zfac;

  copy_v3_v3(rvec, origin);

  zfac = ED_view3d_calc_zfac(region->regiondata, rvec, NULL);

  if (ED_view3d_project_float_global(region, rvec, mval_prj, V3D_PROJ_TEST_NOP) ==
      V3D_PROJ_RET_OK) {
    sub_v2_v2v2(mval_f, mval_prj, mval_f);
    ED_view3d_win_to_delta(region, mval_f, dvec, zfac);
    sub_v3_v3v3(out, rvec, dvec);
  }
  else {
    zero_v3(out);
  }
}

/* convert 2d tGPspoint to 3d bGPDspoint */
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

/* recalc uv for any stroke using the material */
void ED_gpencil_update_color_uv(Main *bmain, Material *mat)
{
  Material *gps_ma = NULL;
  /* read all strokes  */
  for (Object *ob = bmain->objects.first; ob; ob = ob->id.next) {
    if (ob->type == OB_GPENCIL) {
      bGPdata *gpd = ob->data;
      if (gpd == NULL) {
        continue;
      }

      LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
        /* only editable and visible layers are considered */
        if (BKE_gpencil_layer_is_editable(gpl)) {
          LISTBASE_FOREACH (bGPDframe *, gpf, &gpl->frames) {
            LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
              /* check if it is editable */
              if (ED_gpencil_stroke_color_use(ob, gpl, gps) == false) {
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
    float(*points2d)[2] = BLI_ghash_lookup(all_2d, gps_iter);

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

static void gp_copy_points(bGPDstroke *gps, bGPDspoint *pt, bGPDspoint *pt_final, int i, int i2)
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

  if (gps->dvert != NULL) {
    MDeformVert *dvert = &gps->dvert[i];
    MDeformVert *dvert_final = &gps->dvert[i2];
    MEM_SAFE_FREE(dvert_final->dw);

    dvert_final->totweight = dvert->totweight;
    if (dvert->dw == NULL) {
      dvert_final->dw = NULL;
      dvert_final->totweight = 0;
    }
    else {
      dvert_final->dw = MEM_dupallocN(dvert->dw);
    }
  }
}

static void gp_insert_point(
    bGPDstroke *gps, bGPDspoint *a_pt, bGPDspoint *b_pt, const float co_a[3], float co_b[3])
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
  temp_points = MEM_dupallocN(gps->points);
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
  gps->points = MEM_recallocN(gps->points, sizeof(*gps->points) * gps->totpoints);
  if (gps->dvert != NULL) {
    gps->dvert = MEM_recallocN(gps->dvert, sizeof(*gps->dvert) * gps->totpoints);
  }

  /* copy all points */
  int i2 = 0;
  for (int i = 0; i < oldtotpoints; i++) {
    bGPDspoint *pt = &temp_points[i];
    bGPDspoint *pt_final = &gps->points[i2];
    gp_copy_points(gps, pt, pt_final, i, i2);

    /* create new point duplicating point and copy location */
    if ((i == a_idx) || (i == b_idx)) {
      i2++;
      pt_final = &gps->points[i2];
      gp_copy_points(gps, pt, pt_final, i, i2);
      copy_v3_v3(&pt_final->x, (i == a_idx) ? co_a : co_b);

      /* unselect */
      pt_final->flag &= ~GP_SPOINT_SELECT;
      /* tag to avoid more checking with this point */
      pt_final->flag |= GP_SPOINT_TAG;
    }

    i2++;
  }
  /* Calc geometry data. */
  BKE_gpencil_stroke_geometry_update(gps);

  MEM_SAFE_FREE(temp_points);
}

static float gp_calc_factor(float p2d_a1[2], float p2d_a2[2], float r_hit2d[2])
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

/* extend selection to stroke intersections */
int ED_gpencil_select_stroke_segment(bGPDlayer *gpl,
                                     bGPDstroke *gps,
                                     bGPDspoint *pt,
                                     bool select,
                                     bool insert,
                                     const float scale,
                                     float r_hita[3],
                                     float r_hitb[3])
{
  const float min_factor = 0.0015f;
  bGPDspoint *pta1 = NULL;
  bGPDspoint *pta2 = NULL;
  float f = 0.0f;
  int i2 = 0;

  bGPDlayer *gpl_orig = (gpl->runtime.gpl_orig) ? gpl->runtime.gpl_orig : gpl;
  bGPDframe *gpf = gpl_orig->actframe;
  if (gpf == NULL) {
    return 0;
  }

  int memsize = BLI_listbase_count(&gpf->strokes);
  bGPDstroke **gps_array = MEM_callocN(sizeof(bGPDstroke *) * memsize, __func__);

  /* save points */
  bGPDspoint *oldpoints = MEM_dupallocN(gps->points);

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

  /* convert all gps points to 2d and save in a hash to avoid recalculation  */
  int direction = 0;
  float(*points2d)[2] = MEM_mallocN(sizeof(*points2d) * gps->totpoints,
                                    "GP Stroke temp 2d points");
  BKE_gpencil_stroke_2d_flat_ref(
      gps->points, gps->totpoints, gps->points, gps->totpoints, points2d, scale, &direction);

  GHash *all_2d = BLI_ghash_ptr_new(__func__);

  for (int s = 0; s < totstrokes; s++) {
    bGPDstroke *gps_iter = gps_array[s];
    float(*points2d_iter)[2] = MEM_mallocN(sizeof(*points2d_iter) * gps_iter->totpoints, __func__);

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
  bGPDspoint *hit_pointa = NULL;
  bGPDspoint *hit_pointb = NULL;

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
        f = gp_calc_factor(p2d_a1, p2d_a2, r_hit2d);
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
      f = gp_calc_factor(p2d_a1, p2d_a2, r_hit2d);
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
    gp_insert_point(gps, hit_pointa, hit_pointb, r_hita, r_hitb);
  }

  /* free memory */
  if (all_2d) {
    GHashIterator gh_iter;
    GHASH_ITER (gh_iter, all_2d) {
      float(*p2d)[2] = BLI_ghashIterator_getValue(&gh_iter);
      MEM_SAFE_FREE(p2d);
    }
    BLI_ghash_free(all_2d, NULL, NULL);
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
  else if (hit_a) {
    return 1;
  }
  else if (hit_b) {
    return 2;
  }
  else {
    return 0;
  }
}

void ED_gpencil_select_toggle_all(bContext *C, int action)
{
  /* for "toggle", test for existing selected strokes */
  if (action == SEL_TOGGLE) {
    action = SEL_SELECT;

    CTX_DATA_BEGIN (C, bGPDstroke *, gps, editable_gpencil_strokes) {
      if (gps->flag & GP_STROKE_SELECT) {
        action = SEL_DESELECT;
        break;  // XXX: this only gets out of the inner loop...
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
    CTX_DATA_BEGIN (C, bGPDlayer *, gpl, editable_gpencil_layers) {

      /* deselect all strokes on all frames */
      LISTBASE_FOREACH (bGPDframe *, gpf, &gpl->frames) {
        bGPDstroke *gps;

        for (gps = gpf->strokes.first; gps; gps = gps->next) {
          bGPDspoint *pt;
          int i;

          /* only edit strokes that are valid in this view... */
          if (ED_gpencil_stroke_can_use(C, gps)) {
            for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
              pt->flag &= ~GP_SPOINT_SELECT;
            }

            gps->flag &= ~GP_STROKE_SELECT;
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
          // case SEL_DESELECT:
          //  pt->flag &= ~GP_SPOINT_SELECT;
          //  break;
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
      }
      else {
        gps->flag &= ~GP_STROKE_SELECT;
      }
    }
    CTX_DATA_END;
  }
}

/* Ensure the SBuffer (while drawing stroke) size is enough to save all points of the stroke */
tGPspoint *ED_gpencil_sbuffer_ensure(tGPspoint *buffer_array,
                                     int *buffer_size,
                                     int *buffer_used,
                                     const bool clear)
{
  tGPspoint *p = NULL;

  /* By default a buffer is created with one block with a predefined number of free points,
   * if the size is not enough, the cache is reallocated adding a new block of free points.
   * This is done in order to keep cache small and improve speed. */
  if (*buffer_used + 1 > *buffer_size) {
    if ((*buffer_size == 0) || (buffer_array == NULL)) {
      p = MEM_callocN(sizeof(struct tGPspoint) * GP_STROKE_BUFFER_CHUNK, "GPencil Sbuffer");
      *buffer_size = GP_STROKE_BUFFER_CHUNK;
    }
    else {
      *buffer_size += GP_STROKE_BUFFER_CHUNK;
      p = MEM_recallocN(buffer_array, sizeof(struct tGPspoint) * *buffer_size);
    }

    if (p == NULL) {
      *buffer_size = *buffer_used = 0;
    }

    buffer_array = p;
  }

  /* clear old data */
  if (clear) {
    *buffer_used = 0;
    if (buffer_array != NULL) {
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

/* Tag all scene grease pencil object to update. */
void ED_gpencil_tag_scene_gpencil(Scene *scene)
{
  /* mark all grease pencil datablocks of the scene */
  FOREACH_SCENE_COLLECTION_BEGIN (scene, collection) {
    FOREACH_COLLECTION_OBJECT_RECURSIVE_BEGIN (collection, ob) {
      if (ob->type == OB_GPENCIL) {
        bGPdata *gpd = (bGPdata *)ob->data;
        gpd->flag |= GP_DATA_CACHE_IS_DIRTY;
        DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
      }
    }
    FOREACH_COLLECTION_OBJECT_RECURSIVE_END;
  }
  FOREACH_SCENE_COLLECTION_END;

  DEG_id_tag_update(&scene->id, ID_RECALC_COPY_ON_WRITE);

  WM_main_add_notifier(NC_GPENCIL | NA_EDITED, NULL);
}

void ED_gpencil_fill_vertex_color_set(ToolSettings *ts, Brush *brush, bGPDstroke *gps)
{
  if (GPENCIL_USE_VERTEX_COLOR_FILL(ts, brush)) {
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
  if (GPENCIL_USE_VERTEX_COLOR_STROKE(ts, brush)) {
    if (tpt == NULL) {
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
  int seed = ((uint)(ceil(PIL_check_seconds_timer())) + 1) % 128;
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

  /* Randomn to color strength. */
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
    bGPdata *gpd, Brush *brush, tGPspoint *tpt, float random_color[3], float pen_pressure)
{
  BrushGpencilSettings *brush_settings = brush->gpencil_settings;
  if (brush_settings->flag & GP_BRUSH_GROUP_RANDOM) {
    int seed = ((uint)(ceil(PIL_check_seconds_timer())) + 1) % 128;

    int ix = (int)(tpt->x * seed);
    int iy = (int)(tpt->y * seed);
    int iz = ix + iy * seed;
    float hsv[3];
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

    CLAMP3(hsv, 0.0f, 1.0f);
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

  int idx = gpd->runtime.sbuffer_used;
  tGPspoint *tpt = (tGPspoint *)gpd->runtime.sbuffer + idx;

  float vertex_color[4];
  copy_v3_v3(vertex_color, brush->rgb);
  vertex_color[3] = brush->gpencil_settings->vertex_factor;
  srgb_to_linearrgb_v4(vertex_color, vertex_color);

  /* Copy fill vertex color. */
  if (GPENCIL_USE_VERTEX_COLOR_FILL(ts, brush)) {
    copy_v4_v4(gpd->runtime.vert_color_fill, vertex_color);
  }
  else {
    copy_v4_v4(gpd->runtime.vert_color_fill, gp_style->fill_rgba);
  }
  /* Copy stroke vertex color. */
  if (GPENCIL_USE_VERTEX_COLOR_STROKE(ts, brush)) {
    copy_v4_v4(tpt->vert_color, vertex_color);
  }
  else {
    copy_v4_v4(tpt->vert_color, gp_style->stroke_rgba);
  }

  /* Random Color. */
  gpencil_sbuffer_vertex_color_random(gpd, brush, tpt, random_color, pen_pressure);

  /* Copy to eval data because paint operators don't tag refresh until end for speedup
     painting. */
  if (gpd_eval != NULL) {
    copy_v4_v4(gpd_eval->runtime.vert_color_fill, gpd->runtime.vert_color_fill);
    gpd_eval->runtime.matid = gpd->runtime.matid;
  }
}

/* Check if the stroke collides with brush. */
bool ED_gpencil_stroke_check_collision(GP_SpaceConversion *gsc,
                                       bGPDstroke *gps,
                                       float mouse[2],
                                       const int radius,
                                       const float diff_mat[4][4])
{
  const int offset = (int)ceil(sqrt((radius * radius) * 2));
  bGPDspoint pt_dummy, pt_dummy_ps;
  float boundbox_min[2] = {0.0f};
  float boundbox_max[2] = {0.0f};

  /* Check we have something to use (only for old files). */
  if (is_zero_v3(gps->boundbox_min)) {
    BKE_gpencil_stroke_boundingbox_calc(gps);
  }

  /* Convert bound box to 2d */
  copy_v3_v3(&pt_dummy.x, gps->boundbox_min);
  gp_point_to_parent_space(&pt_dummy, diff_mat, &pt_dummy_ps);
  gp_point_to_xy_fl(gsc, gps, &pt_dummy_ps, &boundbox_min[0], &boundbox_min[1]);

  copy_v3_v3(&pt_dummy.x, gps->boundbox_max);
  gp_point_to_parent_space(&pt_dummy, diff_mat, &pt_dummy_ps);
  gp_point_to_xy_fl(gsc, gps, &pt_dummy_ps, &boundbox_max[0], &boundbox_max[1]);

  /* Ensure the bounding box is oriented to axis. */
  if (boundbox_max[0] < boundbox_min[0]) {
    SWAP(float, boundbox_min[0], boundbox_max[0]);
  }
  if (boundbox_max[1] < boundbox_min[1]) {
    SWAP(float, boundbox_min[1], boundbox_max[1]);
  }

  rcti rect_stroke = {boundbox_min[0], boundbox_max[0], boundbox_min[1], boundbox_max[1]};

  /* For mouse, add a small offet to avoid false negative in corners. */
  rcti rect_mouse = {mouse[0] - offset, mouse[0] + offset, mouse[1] - offset, mouse[1] + offset};

  /* Check collision between both rectangles. */
  return BLI_rcti_isect(&rect_stroke, &rect_mouse, NULL);
}
