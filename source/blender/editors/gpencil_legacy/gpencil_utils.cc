/* SPDX-FileCopyrightText: 2014 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edgpencil
 */

#include <cstdlib>
#include <cstring>

#include "MEM_guardedalloc.h"

#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"
#include "BLI_utildefines.h"

#include "DNA_gpencil_legacy_types.h"
#include "DNA_material_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_view3d_types.h"

#include "BKE_context.hh"
#include "BKE_gpencil_legacy.h"
#include "BKE_paint.hh"
#include "BKE_tracking.h"

#include "WM_api.hh"
#include "WM_toolsystem.hh"
#include "WM_types.hh"

#include "RNA_access.hh"
#include "RNA_enum_types.hh"
#include "RNA_prototypes.hh"

#include "UI_view2d.hh"

#include "ED_clip.hh"
#include "ED_gpencil_legacy.hh"
#include "ED_object.hh"
#include "ED_select_utils.hh"
#include "ED_view3d.hh"

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
          *r_ptr = RNA_pointer_create_discrete(screen_id, &RNA_SpaceSequenceEditor, sseq);
        }
        return &sseq->gpd;
      }
      case SPACE_IMAGE: /* Image/UV Editor */
      {
        SpaceImage *sima = (SpaceImage *)sl;

        /* For now, Grease Pencil data is associated with the space... */
        if (r_ptr) {
          *r_ptr = RNA_pointer_create_discrete(screen_id, &RNA_SpaceImageEditor, sima);
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
              *r_ptr = RNA_pointer_create_discrete(&clip->id, &RNA_MovieTrackingTrack, track);
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

bGPdata *ED_annotation_data_get_active_direct(ID *screen_id, ScrArea *area, Scene *scene)
{
  bGPdata **gpd_ptr = ED_annotation_data_get_pointers_direct(screen_id, area, scene, nullptr);
  return (gpd_ptr) ? *(gpd_ptr) : nullptr;
}

bGPdata *ED_annotation_data_get_active(const bContext *C)
{
  bGPdata **gpd_ptr = ED_annotation_data_get_pointers(C, nullptr);
  return (gpd_ptr) ? *(gpd_ptr) : nullptr;
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

/* ******************************************************** */
/* Space Conversion */

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
