/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spview3d
 */

#include "DNA_curve_types.h"
#include "DNA_gpencil_legacy_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_rect.h"

#include "BLT_translation.h"

#include "BKE_armature.h"
#include "BKE_context.h"
#include "BKE_gpencil_geom_legacy.h"
#include "BKE_layer.h"
#include "BKE_object.h"
#include "BKE_paint.hh"
#include "BKE_scene.h"
#include "BKE_screen.h"
#include "BKE_vfont.h"

#include "DEG_depsgraph_query.h"

#include "ED_mesh.hh"
#include "ED_particle.hh"
#include "ED_screen.hh"
#include "ED_transform.hh"

#include "WM_api.hh"
#include "WM_message.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "UI_resources.hh"

#include "view3d_intern.h"

#include "view3d_navigate.hh" /* own include */

/* -------------------------------------------------------------------- */
/** \name View Camera Operator
 * \{ */

static int view_camera_exec(bContext *C, wmOperator *op)
{
  View3D *v3d;
  ARegion *region;
  RegionView3D *rv3d;
  const int smooth_viewtx = WM_operator_smooth_viewtx_get(op);

  /* no nullptr check is needed, poll checks */
  ED_view3d_context_user_region(C, &v3d, &region);
  rv3d = static_cast<RegionView3D *>(region->regiondata);

  ED_view3d_smooth_view_force_finish(C, v3d, region);

  if ((RV3D_LOCK_FLAGS(rv3d) & RV3D_LOCK_ANY_TRANSFORM) == 0) {
    ViewLayer *view_layer = CTX_data_view_layer(C);
    Scene *scene = CTX_data_scene(C);

    if (rv3d->persp != RV3D_CAMOB) {
      BKE_view_layer_synced_ensure(scene, view_layer);
      Object *ob = BKE_view_layer_active_object_get(view_layer);

      if (!rv3d->smooth_timer) {
        /* store settings of current view before allowing overwriting with camera view
         * only if we're not currently in a view transition */

        ED_view3d_lastview_store(rv3d);
      }

      /* first get the default camera for the view lock type */
      if (v3d->scenelock) {
        /* sets the camera view if available */
        v3d->camera = scene->camera;
      }
      else {
        /* use scene camera if one is not set (even though we're unlocked) */
        if (v3d->camera == nullptr) {
          v3d->camera = scene->camera;
        }
      }

      /* if the camera isn't found, check a number of options */
      if (v3d->camera == nullptr && ob && ob->type == OB_CAMERA) {
        v3d->camera = ob;
      }

      if (v3d->camera == nullptr) {
        v3d->camera = BKE_view_layer_camera_find(scene, view_layer);
      }

      /* couldn't find any useful camera, bail out */
      if (v3d->camera == nullptr) {
        return OPERATOR_CANCELLED;
      }

      /* important these don't get out of sync for locked scenes */
      if (v3d->scenelock && scene->camera != v3d->camera) {
        scene->camera = v3d->camera;
        DEG_id_tag_update(&scene->id, ID_RECALC_COPY_ON_WRITE);
      }

      /* finally do snazzy view zooming */
      rv3d->persp = RV3D_CAMOB;

      V3D_SmoothParams sview = {nullptr};
      sview.camera = v3d->camera;
      sview.ofs = rv3d->ofs;
      sview.quat = rv3d->viewquat;
      sview.dist = &rv3d->dist;
      sview.lens = &v3d->lens;
      /* No undo because this changes cameras (and wont move the camera). */
      sview.undo_str = nullptr;

      ED_view3d_smooth_view(C, v3d, region, smooth_viewtx, &sview);
    }
    else {
      /* return to settings of last view */
      /* does view3d_smooth_view too */
      axis_set_view(C,
                    v3d,
                    region,
                    rv3d->lviewquat,
                    rv3d->lview,
                    rv3d->lview_axis_roll,
                    rv3d->lpersp,
                    nullptr,
                    smooth_viewtx);
    }
  }

  return OPERATOR_FINISHED;
}

void VIEW3D_OT_view_camera(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "View Camera";
  ot->description = "Toggle the camera view";
  ot->idname = "VIEW3D_OT_view_camera";

  /* api callbacks */
  ot->exec = view_camera_exec;
  ot->poll = ED_operator_rv3d_user_region_poll;

  /* flags */
  ot->flag = 0;
}

/** \} */
