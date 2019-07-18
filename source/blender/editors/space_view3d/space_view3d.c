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
 * \ingroup spview3d
 */

#include <string.h>
#include <stdio.h>

#include "DNA_lightprobe_types.h"
#include "DNA_material_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_gpencil_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "BKE_context.h"
#include "BKE_curve.h"
#include "BKE_icons.h"
#include "BKE_lattice.h"
#include "BKE_main.h"
#include "BKE_mball.h"
#include "BKE_mesh.h"
#include "BKE_object.h"
#include "BKE_scene.h"
#include "BKE_screen.h"
#include "BKE_workspace.h"

#include "ED_space_api.h"
#include "ED_screen.h"
#include "ED_transform.h"

#include "GPU_framebuffer.h"
#include "GPU_material.h"
#include "GPU_viewport.h"
#include "GPU_matrix.h"

#include "DRW_engine.h"

#include "WM_api.h"
#include "WM_types.h"
#include "WM_message.h"
#include "WM_toolsystem.h"

#include "RE_engine.h"
#include "RE_pipeline.h"

#include "RNA_access.h"

#include "UI_interface.h"
#include "UI_resources.h"

#ifdef WITH_PYTHON
#  include "BPY_extern.h"
#endif

#include "DEG_depsgraph.h"

#include "view3d_intern.h" /* own include */

/* ******************** manage regions ********************* */

/* function to always find a regionview3d context inside 3D window */
RegionView3D *ED_view3d_context_rv3d(bContext *C)
{
  RegionView3D *rv3d = CTX_wm_region_view3d(C);

  if (rv3d == NULL) {
    ScrArea *sa = CTX_wm_area(C);
    if (sa && sa->spacetype == SPACE_VIEW3D) {
      ARegion *ar = BKE_area_find_region_active_win(sa);
      if (ar) {
        rv3d = ar->regiondata;
      }
    }
  }
  return rv3d;
}

/* ideally would return an rv3d but in some cases the region is needed too
 * so return that, the caller can then access the ar->regiondata */
bool ED_view3d_context_user_region(bContext *C, View3D **r_v3d, ARegion **r_ar)
{
  ScrArea *sa = CTX_wm_area(C);

  *r_v3d = NULL;
  *r_ar = NULL;

  if (sa && sa->spacetype == SPACE_VIEW3D) {
    ARegion *ar = CTX_wm_region(C);
    View3D *v3d = (View3D *)sa->spacedata.first;

    if (ar) {
      RegionView3D *rv3d;
      if ((ar->regiontype == RGN_TYPE_WINDOW) && (rv3d = ar->regiondata) &&
          (rv3d->viewlock & RV3D_LOCKED) == 0) {
        *r_v3d = v3d;
        *r_ar = ar;
        return true;
      }
      else {
        ARegion *ar_unlock_user = NULL;
        ARegion *ar_unlock = NULL;
        for (ar = sa->regionbase.first; ar; ar = ar->next) {
          /* find the first unlocked rv3d */
          if (ar->regiondata && ar->regiontype == RGN_TYPE_WINDOW) {
            rv3d = ar->regiondata;
            if ((rv3d->viewlock & RV3D_LOCKED) == 0) {
              ar_unlock = ar;
              if (rv3d->persp == RV3D_PERSP || rv3d->persp == RV3D_CAMOB) {
                ar_unlock_user = ar;
                break;
              }
            }
          }
        }

        /* camera/perspective view get priority when the active region is locked */
        if (ar_unlock_user) {
          *r_v3d = v3d;
          *r_ar = ar_unlock_user;
          return true;
        }

        if (ar_unlock) {
          *r_v3d = v3d;
          *r_ar = ar_unlock;
          return true;
        }
      }
    }
  }

  return false;
}

/* Most of the time this isn't needed since you could assume the view matrix was
 * set while drawing, however when functions like mesh_foreachScreenVert are
 * called by selection tools, we can't be sure this object was the last.
 *
 * for example, transparent objects are drawn after editmode and will cause
 * the rv3d mat's to change and break selection.
 *
 * 'ED_view3d_init_mats_rv3d' should be called before
 * view3d_project_short_clip and view3d_project_short_noclip in cases where
 * these functions are not used during draw_object
 */
void ED_view3d_init_mats_rv3d(struct Object *ob, struct RegionView3D *rv3d)
{
  /* local viewmat and persmat, to calculate projections */
  mul_m4_m4m4(rv3d->viewmatob, rv3d->viewmat, ob->obmat);
  mul_m4_m4m4(rv3d->persmatob, rv3d->persmat, ob->obmat);

  /* initializes object space clipping, speeds up clip tests */
  ED_view3d_clipping_local(rv3d, ob->obmat);
}

void ED_view3d_init_mats_rv3d_gl(struct Object *ob, struct RegionView3D *rv3d)
{
  ED_view3d_init_mats_rv3d(ob, rv3d);

  /* we have to multiply instead of loading viewmatob to make
   * it work with duplis using displists, otherwise it will
   * override the dupli-matrix */
  GPU_matrix_mul(ob->obmat);
}

#ifdef DEBUG
/* ensure we correctly initialize */
void ED_view3d_clear_mats_rv3d(struct RegionView3D *rv3d)
{
  zero_m4(rv3d->viewmatob);
  zero_m4(rv3d->persmatob);
}

void ED_view3d_check_mats_rv3d(struct RegionView3D *rv3d)
{
  BLI_ASSERT_ZERO_M4(rv3d->viewmatob);
  BLI_ASSERT_ZERO_M4(rv3d->persmatob);
}
#endif

void ED_view3d_stop_render_preview(wmWindowManager *wm, ARegion *ar)
{
  RegionView3D *rv3d = ar->regiondata;

  if (rv3d->render_engine) {
#ifdef WITH_PYTHON
    BPy_BEGIN_ALLOW_THREADS;
#endif

    WM_jobs_kill_type(wm, ar, WM_JOB_TYPE_RENDER_PREVIEW);

#ifdef WITH_PYTHON
    BPy_END_ALLOW_THREADS;
#endif

    RE_engine_free(rv3d->render_engine);
    rv3d->render_engine = NULL;
  }
}

void ED_view3d_shade_update(Main *bmain, View3D *v3d, ScrArea *sa)
{
  wmWindowManager *wm = bmain->wm.first;

  if (v3d->shading.type != OB_RENDER) {
    ARegion *ar;

    for (ar = sa->regionbase.first; ar; ar = ar->next) {
      if ((ar->regiontype == RGN_TYPE_WINDOW) && ar->regiondata) {
        ED_view3d_stop_render_preview(wm, ar);
        break;
      }
    }
  }
}

/* ******************** default callbacks for view3d space ***************** */

static SpaceLink *view3d_new(const ScrArea *UNUSED(sa), const Scene *scene)
{
  ARegion *ar;
  View3D *v3d;
  RegionView3D *rv3d;

  v3d = MEM_callocN(sizeof(View3D), "initview3d");
  v3d->spacetype = SPACE_VIEW3D;
  if (scene) {
    v3d->camera = scene->camera;
  }
  v3d->scenelock = true;
  v3d->grid = 1.0f;
  v3d->gridlines = 16;
  v3d->gridsubdiv = 10;
  BKE_screen_view3d_shading_init(&v3d->shading);

  v3d->overlay.wireframe_threshold = 1.0f;
  v3d->overlay.xray_alpha_bone = 0.5f;
  v3d->overlay.texture_paint_mode_opacity = 1.0f;
  v3d->overlay.weight_paint_mode_opacity = 1.0f;
  v3d->overlay.vertex_paint_mode_opacity = 1.0f;
  /* Intentionally different to vertex/paint mode,
   * we typically want to see shading too. */
  v3d->overlay.sculpt_mode_mask_opacity = 0.75f;

  v3d->overlay.edit_flag = V3D_OVERLAY_EDIT_FACES | V3D_OVERLAY_EDIT_SEAMS |
                           V3D_OVERLAY_EDIT_SHARP | V3D_OVERLAY_EDIT_FREESTYLE_EDGE |
                           V3D_OVERLAY_EDIT_FREESTYLE_FACE | V3D_OVERLAY_EDIT_EDGES |
                           V3D_OVERLAY_EDIT_CREASES | V3D_OVERLAY_EDIT_BWEIGHTS |
                           V3D_OVERLAY_EDIT_CU_HANDLES | V3D_OVERLAY_EDIT_CU_NORMALS;

  v3d->gridflag = V3D_SHOW_X | V3D_SHOW_Y | V3D_SHOW_FLOOR | V3D_SHOW_ORTHO_GRID;

  v3d->flag = V3D_SELECT_OUTLINE;
  v3d->flag2 = V3D_SHOW_RECONSTRUCTION | V3D_SHOW_ANNOTATION;

  v3d->lens = 50.0f;
  v3d->clip_start = 0.01f;
  v3d->clip_end = 1000.0f;

  v3d->overlay.gpencil_paper_opacity = 0.5f;
  v3d->overlay.gpencil_grid_opacity = 0.9f;

  v3d->bundle_size = 0.2f;
  v3d->bundle_drawtype = OB_PLAINAXES;

  /* stereo */
  v3d->stereo3d_camera = STEREO_3D_ID;
  v3d->stereo3d_flag |= V3D_S3D_DISPPLANE;
  v3d->stereo3d_convergence_alpha = 0.15f;
  v3d->stereo3d_volume_alpha = 0.05f;

  /* grease pencil settings */
  v3d->vertex_opacity = 1.0f;
  v3d->gp_flag |= V3D_GP_SHOW_EDIT_LINES;

  /* tool header */
  ar = MEM_callocN(sizeof(ARegion), "tool header for view3d");

  BLI_addtail(&v3d->regionbase, ar);
  ar->regiontype = RGN_TYPE_TOOL_HEADER;
  ar->alignment = (U.uiflag & USER_HEADER_BOTTOM) ? RGN_ALIGN_BOTTOM : RGN_ALIGN_TOP;
  ar->flag = RGN_FLAG_HIDDEN | RGN_FLAG_HIDDEN_BY_USER;

  /* header */
  ar = MEM_callocN(sizeof(ARegion), "header for view3d");

  BLI_addtail(&v3d->regionbase, ar);
  ar->regiontype = RGN_TYPE_HEADER;
  ar->alignment = (U.uiflag & USER_HEADER_BOTTOM) ? RGN_ALIGN_BOTTOM : RGN_ALIGN_TOP;

  /* tool shelf */
  ar = MEM_callocN(sizeof(ARegion), "toolshelf for view3d");

  BLI_addtail(&v3d->regionbase, ar);
  ar->regiontype = RGN_TYPE_TOOLS;
  ar->alignment = RGN_ALIGN_LEFT;
  ar->flag = RGN_FLAG_HIDDEN;

  /* buttons/list view */
  ar = MEM_callocN(sizeof(ARegion), "buttons for view3d");

  BLI_addtail(&v3d->regionbase, ar);
  ar->regiontype = RGN_TYPE_UI;
  ar->alignment = RGN_ALIGN_RIGHT;
  ar->flag = RGN_FLAG_HIDDEN;

  /* main region */
  ar = MEM_callocN(sizeof(ARegion), "main region for view3d");

  BLI_addtail(&v3d->regionbase, ar);
  ar->regiontype = RGN_TYPE_WINDOW;

  ar->regiondata = MEM_callocN(sizeof(RegionView3D), "region view3d");
  rv3d = ar->regiondata;
  rv3d->viewquat[0] = 1.0f;
  rv3d->persp = RV3D_PERSP;
  rv3d->view = RV3D_VIEW_USER;
  rv3d->dist = 10.0;

  return (SpaceLink *)v3d;
}

/* not spacelink itself */
static void view3d_free(SpaceLink *sl)
{
  View3D *vd = (View3D *)sl;

  if (vd->localvd) {
    MEM_freeN(vd->localvd);
  }

  if (vd->runtime.properties_storage) {
    MEM_freeN(vd->runtime.properties_storage);
  }

  if (vd->fx_settings.ssao) {
    MEM_freeN(vd->fx_settings.ssao);
  }
  if (vd->fx_settings.dof) {
    MEM_freeN(vd->fx_settings.dof);
  }
}

/* spacetype; init callback */
static void view3d_init(wmWindowManager *UNUSED(wm), ScrArea *UNUSED(sa))
{
}

static SpaceLink *view3d_duplicate(SpaceLink *sl)
{
  View3D *v3do = (View3D *)sl;
  View3D *v3dn = MEM_dupallocN(sl);

  /* clear or remove stuff from old */

  if (v3dn->localvd) {
    v3dn->localvd = NULL;
    v3dn->runtime.properties_storage = NULL;
  }

  if (v3dn->shading.type == OB_RENDER) {
    v3dn->shading.type = OB_SOLID;
  }

  /* copy or clear inside new stuff */

  v3dn->runtime.properties_storage = NULL;
  if (v3dn->fx_settings.dof) {
    v3dn->fx_settings.dof = MEM_dupallocN(v3do->fx_settings.dof);
  }
  if (v3dn->fx_settings.ssao) {
    v3dn->fx_settings.ssao = MEM_dupallocN(v3do->fx_settings.ssao);
  }

  return (SpaceLink *)v3dn;
}

/* add handlers, stuff you only do once or on area/region changes */
static void view3d_main_region_init(wmWindowManager *wm, ARegion *ar)
{
  ListBase *lb;
  wmKeyMap *keymap;

  /* object ops. */

  /* important to be before Pose keymap since they can both be enabled at once */
  keymap = WM_keymap_ensure(wm->defaultconf, "Face Mask", 0, 0);
  WM_event_add_keymap_handler(&ar->handlers, keymap);

  keymap = WM_keymap_ensure(wm->defaultconf, "Weight Paint Vertex Selection", 0, 0);
  WM_event_add_keymap_handler(&ar->handlers, keymap);

  /* pose is not modal, operator poll checks for this */
  keymap = WM_keymap_ensure(wm->defaultconf, "Pose", 0, 0);
  WM_event_add_keymap_handler(&ar->handlers, keymap);

  keymap = WM_keymap_ensure(wm->defaultconf, "Object Mode", 0, 0);
  WM_event_add_keymap_handler(&ar->handlers, keymap);

  keymap = WM_keymap_ensure(wm->defaultconf, "Paint Curve", 0, 0);
  WM_event_add_keymap_handler(&ar->handlers, keymap);

  keymap = WM_keymap_ensure(wm->defaultconf, "Curve", 0, 0);
  WM_event_add_keymap_handler(&ar->handlers, keymap);

  keymap = WM_keymap_ensure(wm->defaultconf, "Image Paint", 0, 0);
  WM_event_add_keymap_handler(&ar->handlers, keymap);

  keymap = WM_keymap_ensure(wm->defaultconf, "Vertex Paint", 0, 0);
  WM_event_add_keymap_handler(&ar->handlers, keymap);

  keymap = WM_keymap_ensure(wm->defaultconf, "Weight Paint", 0, 0);
  WM_event_add_keymap_handler(&ar->handlers, keymap);

  keymap = WM_keymap_ensure(wm->defaultconf, "Sculpt", 0, 0);
  WM_event_add_keymap_handler(&ar->handlers, keymap);

  keymap = WM_keymap_ensure(wm->defaultconf, "Mesh", 0, 0);
  WM_event_add_keymap_handler(&ar->handlers, keymap);

  keymap = WM_keymap_ensure(wm->defaultconf, "Curve", 0, 0);
  WM_event_add_keymap_handler(&ar->handlers, keymap);

  keymap = WM_keymap_ensure(wm->defaultconf, "Armature", 0, 0);
  WM_event_add_keymap_handler(&ar->handlers, keymap);

  keymap = WM_keymap_ensure(wm->defaultconf, "Pose", 0, 0);
  WM_event_add_keymap_handler(&ar->handlers, keymap);

  keymap = WM_keymap_ensure(wm->defaultconf, "Metaball", 0, 0);
  WM_event_add_keymap_handler(&ar->handlers, keymap);

  keymap = WM_keymap_ensure(wm->defaultconf, "Lattice", 0, 0);
  WM_event_add_keymap_handler(&ar->handlers, keymap);

  keymap = WM_keymap_ensure(wm->defaultconf, "Particle", 0, 0);
  WM_event_add_keymap_handler(&ar->handlers, keymap);

  /* editfont keymap swallows all... */
  keymap = WM_keymap_ensure(wm->defaultconf, "Font", 0, 0);
  WM_event_add_keymap_handler(&ar->handlers, keymap);

  keymap = WM_keymap_ensure(wm->defaultconf, "Object Non-modal", 0, 0);
  WM_event_add_keymap_handler(&ar->handlers, keymap);

  keymap = WM_keymap_ensure(wm->defaultconf, "Frames", 0, 0);
  WM_event_add_keymap_handler(&ar->handlers, keymap);

  /* own keymap, last so modes can override it */
  keymap = WM_keymap_ensure(wm->defaultconf, "3D View Generic", SPACE_VIEW3D, 0);
  WM_event_add_keymap_handler(&ar->handlers, keymap);

  keymap = WM_keymap_ensure(wm->defaultconf, "3D View", SPACE_VIEW3D, 0);
  WM_event_add_keymap_handler(&ar->handlers, keymap);

  /* add drop boxes */
  lb = WM_dropboxmap_find("View3D", SPACE_VIEW3D, RGN_TYPE_WINDOW);

  WM_event_add_dropbox_handler(&ar->handlers, lb);
}

static void view3d_main_region_exit(wmWindowManager *wm, ARegion *ar)
{
  ED_view3d_stop_render_preview(wm, ar);
}

static bool view3d_ob_drop_poll(bContext *UNUSED(C),
                                wmDrag *drag,
                                const wmEvent *UNUSED(event),
                                const char **UNUSED(tooltip))
{
  return WM_drag_ID(drag, ID_OB) != NULL;
}

static bool view3d_collection_drop_poll(bContext *UNUSED(C),
                                        wmDrag *drag,
                                        const wmEvent *UNUSED(event),
                                        const char **UNUSED(tooltip))
{
  return WM_drag_ID(drag, ID_GR) != NULL;
}

static bool view3d_mat_drop_poll(bContext *UNUSED(C),
                                 wmDrag *drag,
                                 const wmEvent *UNUSED(event),
                                 const char **UNUSED(tooltip))
{
  return WM_drag_ID(drag, ID_MA) != NULL;
}

static bool view3d_ima_drop_poll(bContext *UNUSED(C),
                                 wmDrag *drag,
                                 const wmEvent *UNUSED(event),
                                 const char **UNUSED(tooltip))
{
  if (drag->type == WM_DRAG_PATH) {
    /* rule might not work? */
    return (ELEM(drag->icon, 0, ICON_FILE_IMAGE, ICON_FILE_MOVIE));
  }
  else {
    return WM_drag_ID(drag, ID_IM) != NULL;
  }
}

static bool view3d_ima_bg_is_camera_view(bContext *C)
{
  RegionView3D *rv3d = CTX_wm_region_view3d(C);
  if ((rv3d && (rv3d->persp == RV3D_CAMOB))) {
    View3D *v3d = CTX_wm_view3d(C);
    if (v3d && v3d->camera && v3d->camera->type == OB_CAMERA) {
      return true;
    }
  }
  return false;
}

static bool view3d_ima_bg_drop_poll(bContext *C,
                                    wmDrag *drag,
                                    const wmEvent *event,
                                    const char **tooltip)
{
  if (!view3d_ima_drop_poll(C, drag, event, tooltip)) {
    return false;
  }

  if (ED_view3d_is_object_under_cursor(C, event->mval)) {
    return false;
  }

  return view3d_ima_bg_is_camera_view(C);
}

static bool view3d_ima_empty_drop_poll(bContext *C,
                                       wmDrag *drag,
                                       const wmEvent *event,
                                       const char **tooltip)
{
  if (!view3d_ima_drop_poll(C, drag, event, tooltip)) {
    return false;
  }

  Object *ob = ED_view3d_give_object_under_cursor(C, event->mval);

  if (ob == NULL) {
    return true;
  }

  if (ob->type == OB_EMPTY && ob->empty_drawtype == OB_EMPTY_IMAGE) {
    return true;
  }

  return false;
}

static void view3d_ob_drop_copy(wmDrag *drag, wmDropBox *drop)
{
  ID *id = WM_drag_ID(drag, ID_OB);

  RNA_string_set(drop->ptr, "name", id->name + 2);
}

static void view3d_collection_drop_copy(wmDrag *drag, wmDropBox *drop)
{
  ID *id = WM_drag_ID(drag, ID_GR);

  drop->opcontext = WM_OP_EXEC_DEFAULT;
  RNA_string_set(drop->ptr, "name", id->name + 2);
}

static void view3d_id_drop_copy(wmDrag *drag, wmDropBox *drop)
{
  ID *id = WM_drag_ID(drag, 0);

  RNA_string_set(drop->ptr, "name", id->name + 2);
}

static void view3d_id_path_drop_copy(wmDrag *drag, wmDropBox *drop)
{
  ID *id = WM_drag_ID(drag, 0);

  if (id) {
    RNA_string_set(drop->ptr, "name", id->name + 2);
    RNA_struct_property_unset(drop->ptr, "filepath");
  }
  else if (drag->path[0]) {
    RNA_string_set(drop->ptr, "filepath", drag->path);
    RNA_struct_property_unset(drop->ptr, "image");
  }
}

static void view3d_lightcache_update(bContext *C)
{
  PointerRNA op_ptr;

  Scene *scene = CTX_data_scene(C);

  if (strcmp(scene->r.engine, RE_engine_id_BLENDER_EEVEE) != 0) {
    /* Only do auto bake if eevee is the active engine */
    return;
  }

  WM_operator_properties_create(&op_ptr, "SCENE_OT_light_cache_bake");
  RNA_int_set(&op_ptr, "delay", 200);
  RNA_enum_set_identifier(C, &op_ptr, "subset", "DIRTY");

  WM_operator_name_call(C, "SCENE_OT_light_cache_bake", WM_OP_INVOKE_DEFAULT, &op_ptr);

  WM_operator_properties_free(&op_ptr);
}

/* region dropbox definition */
static void view3d_dropboxes(void)
{
  ListBase *lb = WM_dropboxmap_find("View3D", SPACE_VIEW3D, RGN_TYPE_WINDOW);

  WM_dropbox_add(lb, "OBJECT_OT_add_named", view3d_ob_drop_poll, view3d_ob_drop_copy);
  WM_dropbox_add(lb, "OBJECT_OT_drop_named_material", view3d_mat_drop_poll, view3d_id_drop_copy);
  WM_dropbox_add(
      lb, "VIEW3D_OT_background_image_add", view3d_ima_bg_drop_poll, view3d_id_path_drop_copy);
  WM_dropbox_add(
      lb, "OBJECT_OT_drop_named_image", view3d_ima_empty_drop_poll, view3d_id_path_drop_copy);
  WM_dropbox_add(lb,
                 "OBJECT_OT_collection_instance_add",
                 view3d_collection_drop_poll,
                 view3d_collection_drop_copy);
}

static void view3d_widgets(void)
{
  wmGizmoMapType *gzmap_type = WM_gizmomaptype_ensure(
      &(const struct wmGizmoMapType_Params){SPACE_VIEW3D, RGN_TYPE_WINDOW});

  WM_gizmogrouptype_append_and_link(gzmap_type, VIEW3D_GGT_xform_gizmo_context);
  WM_gizmogrouptype_append_and_link(gzmap_type, VIEW3D_GGT_light_spot);
  WM_gizmogrouptype_append_and_link(gzmap_type, VIEW3D_GGT_light_area);
  WM_gizmogrouptype_append_and_link(gzmap_type, VIEW3D_GGT_light_target);
  WM_gizmogrouptype_append_and_link(gzmap_type, VIEW3D_GGT_force_field);
  WM_gizmogrouptype_append_and_link(gzmap_type, VIEW3D_GGT_camera);
  WM_gizmogrouptype_append_and_link(gzmap_type, VIEW3D_GGT_camera_view);
  WM_gizmogrouptype_append_and_link(gzmap_type, VIEW3D_GGT_empty_image);
  /* TODO(campbell): Not working well enough, disable for now. */
#if 0
  WM_gizmogrouptype_append_and_link(gzmap_type, VIEW3D_GGT_armature_spline);
#endif

  WM_gizmogrouptype_append(VIEW3D_GGT_xform_gizmo);
  WM_gizmogrouptype_append(VIEW3D_GGT_xform_cage);
  WM_gizmogrouptype_append(VIEW3D_GGT_xform_shear);
  WM_gizmogrouptype_append(VIEW3D_GGT_xform_extrude);
  WM_gizmogrouptype_append(VIEW3D_GGT_mesh_preselect_elem);
  WM_gizmogrouptype_append(VIEW3D_GGT_mesh_preselect_edgering);

  WM_gizmogrouptype_append(VIEW3D_GGT_ruler);
  WM_gizmotype_append(VIEW3D_GT_ruler_item);

  WM_gizmogrouptype_append_and_link(gzmap_type, VIEW3D_GGT_navigate);
  WM_gizmotype_append(VIEW3D_GT_navigate_rotate);
}

/* type callback, not region itself */
static void view3d_main_region_free(ARegion *ar)
{
  RegionView3D *rv3d = ar->regiondata;

  if (rv3d) {
    if (rv3d->localvd) {
      MEM_freeN(rv3d->localvd);
    }
    if (rv3d->clipbb) {
      MEM_freeN(rv3d->clipbb);
    }

    if (rv3d->render_engine) {
      RE_engine_free(rv3d->render_engine);
    }

    if (rv3d->depths) {
      if (rv3d->depths->depths) {
        MEM_freeN(rv3d->depths->depths);
      }
      MEM_freeN(rv3d->depths);
    }
    if (rv3d->sms) {
      MEM_freeN(rv3d->sms);
    }

    MEM_freeN(rv3d);
    ar->regiondata = NULL;
  }
}

/* copy regiondata */
static void *view3d_main_region_duplicate(void *poin)
{
  if (poin) {
    RegionView3D *rv3d = poin, *new;

    new = MEM_dupallocN(rv3d);
    if (rv3d->localvd) {
      new->localvd = MEM_dupallocN(rv3d->localvd);
    }
    if (rv3d->clipbb) {
      new->clipbb = MEM_dupallocN(rv3d->clipbb);
    }

    new->depths = NULL;
    new->render_engine = NULL;
    new->sms = NULL;
    new->smooth_timer = NULL;

    return new;
  }
  return NULL;
}

static void view3d_main_region_listener(
    wmWindow *UNUSED(win), ScrArea *sa, ARegion *ar, wmNotifier *wmn, const Scene *scene)
{
  View3D *v3d = sa->spacedata.first;
  RegionView3D *rv3d = ar->regiondata;
  wmGizmoMap *gzmap = ar->gizmo_map;

  /* context changes */
  switch (wmn->category) {
    case NC_WM:
      if (ELEM(wmn->data, ND_UNDO)) {
        WM_gizmomap_tag_refresh(gzmap);
      }
      break;
    case NC_ANIMATION:
      switch (wmn->data) {
        case ND_KEYFRAME_PROP:
        case ND_NLA_ACTCHANGE:
          ED_region_tag_redraw(ar);
          break;
        case ND_NLA:
        case ND_KEYFRAME:
          if (ELEM(wmn->action, NA_EDITED, NA_ADDED, NA_REMOVED)) {
            ED_region_tag_redraw(ar);
          }
          break;
        case ND_ANIMCHAN:
          if (ELEM(wmn->action, NA_EDITED, NA_ADDED, NA_REMOVED, NA_SELECTED)) {
            ED_region_tag_redraw(ar);
          }
          break;
      }
      break;
    case NC_SCENE:
      switch (wmn->data) {
        case ND_SCENEBROWSE:
        case ND_LAYER_CONTENT:
          ED_region_tag_redraw(ar);
          WM_gizmomap_tag_refresh(gzmap);
          break;
        case ND_LAYER:
          if (wmn->reference) {
            BKE_screen_view3d_sync(v3d, wmn->reference);
          }
          ED_region_tag_redraw(ar);
          WM_gizmomap_tag_refresh(gzmap);
          break;
        case ND_OB_ACTIVE:
        case ND_OB_SELECT:
          ATTR_FALLTHROUGH;
        case ND_FRAME:
        case ND_TRANSFORM:
        case ND_OB_VISIBLE:
        case ND_RENDER_OPTIONS:
        case ND_MARKERS:
        case ND_MODE:
          ED_region_tag_redraw(ar);
          WM_gizmomap_tag_refresh(gzmap);
          break;
        case ND_WORLD:
          /* handled by space_view3d_listener() for v3d access */
          break;
        case ND_DRAW_RENDER_VIEWPORT: {
          if (v3d->camera && (scene == wmn->reference)) {
            if (rv3d->persp == RV3D_CAMOB) {
              ED_region_tag_redraw(ar);
            }
          }
          break;
        }
      }
      if (wmn->action == NA_EDITED) {
        ED_region_tag_redraw(ar);
      }
      break;
    case NC_OBJECT:
      switch (wmn->data) {
        case ND_BONE_ACTIVE:
        case ND_BONE_SELECT:
        case ND_TRANSFORM:
        case ND_POSE:
        case ND_DRAW:
        case ND_MODIFIER:
        case ND_CONSTRAINT:
        case ND_KEYS:
        case ND_PARTICLE:
        case ND_POINTCACHE:
        case ND_LOD:
          ED_region_tag_redraw(ar);
          WM_gizmomap_tag_refresh(gzmap);
          break;
      }
      switch (wmn->action) {
        case NA_ADDED:
          ED_region_tag_redraw(ar);
          break;
      }
      break;
    case NC_GEOM:
      switch (wmn->data) {
        case ND_SELECT: {
          WM_gizmomap_tag_refresh(gzmap);
          ATTR_FALLTHROUGH;
        }
        case ND_DATA:
        case ND_VERTEX_GROUP:
          ED_region_tag_redraw(ar);
          break;
      }
      switch (wmn->action) {
        case NA_EDITED:
          ED_region_tag_redraw(ar);
          break;
      }
      break;
    case NC_CAMERA:
      switch (wmn->data) {
        case ND_DRAW_RENDER_VIEWPORT: {
          if (v3d->camera && (v3d->camera->data == wmn->reference)) {
            if (rv3d->persp == RV3D_CAMOB) {
              ED_region_tag_redraw(ar);
            }
          }
          break;
        }
      }
      break;
    case NC_GROUP:
      /* all group ops for now */
      ED_region_tag_redraw(ar);
      break;
    case NC_BRUSH:
      switch (wmn->action) {
        case NA_EDITED:
          ED_region_tag_redraw_overlay(ar);
          break;
        case NA_SELECTED:
          /* used on brush changes - needed because 3d cursor
           * has to be drawn if clone brush is selected */
          ED_region_tag_redraw(ar);
          break;
      }
      break;
    case NC_MATERIAL:
      switch (wmn->data) {
        case ND_SHADING:
        case ND_NODES:
          /* TODO(sergey) This is a bit too much updates, but needed to
           * have proper material drivers update in the viewport.
           *
           * How to solve?
           */
          ED_region_tag_redraw(ar);
          break;
        case ND_SHADING_DRAW:
        case ND_SHADING_LINKS:
          ED_region_tag_redraw(ar);
          break;
      }
      break;
    case NC_WORLD:
      switch (wmn->data) {
        case ND_WORLD_DRAW:
          /* handled by space_view3d_listener() for v3d access */
          break;
        case ND_WORLD:
          /* Needed for updating world materials */
          ED_region_tag_redraw(ar);
          break;
      }
      break;
    case NC_LAMP:
      switch (wmn->data) {
        case ND_LIGHTING:
          /* TODO(sergey): This is a bit too much, but needed to
           * handle updates from new depsgraph.
           */
          ED_region_tag_redraw(ar);
          break;
        case ND_LIGHTING_DRAW:
          ED_region_tag_redraw(ar);
          WM_gizmomap_tag_refresh(gzmap);
          break;
      }
      break;
    case NC_LIGHTPROBE:
      ED_area_tag_refresh(sa);
      break;
    case NC_IMAGE:
      /* this could be more fine grained checks if we had
       * more context than just the region */
      ED_region_tag_redraw(ar);
      break;
    case NC_TEXTURE:
      /* same as above */
      ED_region_tag_redraw(ar);
      break;
    case NC_MOVIECLIP:
      if (wmn->data == ND_DISPLAY || wmn->action == NA_EDITED) {
        ED_region_tag_redraw(ar);
      }
      break;
    case NC_SPACE:
      if (wmn->data == ND_SPACE_VIEW3D) {
        if (wmn->subtype == NS_VIEW3D_GPU) {
          rv3d->rflag |= RV3D_GPULIGHT_UPDATE;
        }
        ED_region_tag_redraw(ar);
        WM_gizmomap_tag_refresh(gzmap);
      }
      break;
    case NC_ID:
      if (wmn->action == NA_RENAME) {
        ED_region_tag_redraw(ar);
      }
      break;
    case NC_SCREEN:
      switch (wmn->data) {
        case ND_ANIMPLAY:
        case ND_SKETCH:
          ED_region_tag_redraw(ar);
          break;
        case ND_LAYOUTBROWSE:
        case ND_LAYOUTDELETE:
        case ND_LAYOUTSET:
          WM_gizmomap_tag_refresh(gzmap);
          ED_region_tag_redraw(ar);
          break;
        case ND_LAYER:
          ED_region_tag_redraw(ar);
          break;
      }

      break;
    case NC_GPENCIL:
      if (wmn->data == ND_DATA || ELEM(wmn->action, NA_EDITED, NA_SELECTED)) {
        ED_region_tag_redraw(ar);
      }
      break;
  }
}

static void view3d_main_region_message_subscribe(const struct bContext *C,
                                                 struct WorkSpace *UNUSED(workspace),
                                                 struct Scene *UNUSED(scene),
                                                 struct bScreen *UNUSED(screen),
                                                 struct ScrArea *sa,
                                                 struct ARegion *ar,
                                                 struct wmMsgBus *mbus)
{
  /* Developer note: there are many properties that impact 3D view drawing,
   * so instead of subscribing to individual properties, just subscribe to types
   * accepting some redundant redraws.
   *
   * For other space types we might try avoid this, keep the 3D view as an exceptional case! */
  wmMsgParams_RNA msg_key_params = {{{0}}};

  /* Only subscribe to types. */
  StructRNA *type_array[] = {
      &RNA_Window,

      /* These object have properties that impact drawing. */
      &RNA_AreaLight,
      &RNA_Camera,
      &RNA_Light,
      &RNA_Speaker,
      &RNA_SunLight,

      /* General types the 3D view depends on. */
      &RNA_Object,
      &RNA_UnitSettings, /* grid-floor */

      &RNA_View3DOverlay,
      &RNA_View3DShading,
      &RNA_World,
  };

  wmMsgSubscribeValue msg_sub_value_region_tag_redraw = {
      .owner = ar,
      .user_data = ar,
      .notify = ED_region_do_msg_notify_tag_redraw,
  };

  for (int i = 0; i < ARRAY_SIZE(type_array); i++) {
    msg_key_params.ptr.type = type_array[i];
    WM_msg_subscribe_rna_params(mbus, &msg_key_params, &msg_sub_value_region_tag_redraw, __func__);
  }

  /* Subscribe to a handful of other properties. */
  RegionView3D *rv3d = ar->regiondata;

  WM_msg_subscribe_rna_anon_prop(mbus, RenderSettings, engine, &msg_sub_value_region_tag_redraw);
  WM_msg_subscribe_rna_anon_prop(
      mbus, RenderSettings, resolution_x, &msg_sub_value_region_tag_redraw);
  WM_msg_subscribe_rna_anon_prop(
      mbus, RenderSettings, resolution_y, &msg_sub_value_region_tag_redraw);
  WM_msg_subscribe_rna_anon_prop(
      mbus, RenderSettings, pixel_aspect_x, &msg_sub_value_region_tag_redraw);
  WM_msg_subscribe_rna_anon_prop(
      mbus, RenderSettings, pixel_aspect_y, &msg_sub_value_region_tag_redraw);
  if (rv3d->persp == RV3D_CAMOB) {
    WM_msg_subscribe_rna_anon_prop(
        mbus, RenderSettings, use_border, &msg_sub_value_region_tag_redraw);
  }

  WM_msg_subscribe_rna_anon_type(mbus, SceneEEVEE, &msg_sub_value_region_tag_redraw);
  WM_msg_subscribe_rna_anon_type(mbus, SceneDisplay, &msg_sub_value_region_tag_redraw);
  WM_msg_subscribe_rna_anon_type(mbus, ObjectDisplay, &msg_sub_value_region_tag_redraw);

  ViewLayer *view_layer = CTX_data_view_layer(C);
  Object *obact = OBACT(view_layer);
  if (obact != NULL) {
    switch (obact->mode) {
      case OB_MODE_PARTICLE_EDIT:
        WM_msg_subscribe_rna_anon_type(mbus, ParticleEdit, &msg_sub_value_region_tag_redraw);
        break;
      default:
        break;
    }
  }

  {
    wmMsgSubscribeValue msg_sub_value_region_tag_refresh = {
        .owner = ar,
        .user_data = sa,
        .notify = WM_toolsystem_do_msg_notify_tag_refresh,
    };
    WM_msg_subscribe_rna_anon_prop(mbus, Object, mode, &msg_sub_value_region_tag_refresh);
    WM_msg_subscribe_rna_anon_prop(mbus, LayerObjects, active, &msg_sub_value_region_tag_refresh);
  }
}

/* concept is to retrieve cursor type context-less */
static void view3d_main_region_cursor(wmWindow *win, ScrArea *sa, ARegion *ar)
{
  if (WM_cursor_set_from_tool(win, sa, ar)) {
    return;
  }

  ViewLayer *view_layer = WM_window_get_active_view_layer(win);
  Object *obedit = OBEDIT_FROM_VIEW_LAYER(view_layer);
  if (obedit) {
    WM_cursor_set(win, CURSOR_EDIT);
  }
  else {
    WM_cursor_set(win, CURSOR_STD);
  }
}

/* add handlers, stuff you only do once or on area/region changes */
static void view3d_header_region_init(wmWindowManager *wm, ARegion *ar)
{
  wmKeyMap *keymap = WM_keymap_ensure(wm->defaultconf, "3D View Generic", SPACE_VIEW3D, 0);

  WM_event_add_keymap_handler(&ar->handlers, keymap);

  ED_region_header_init(ar);
}

static void view3d_header_region_draw(const bContext *C, ARegion *ar)
{
  ED_region_header(C, ar);
}

static void view3d_header_region_listener(wmWindow *UNUSED(win),
                                          ScrArea *UNUSED(sa),
                                          ARegion *ar,
                                          wmNotifier *wmn,
                                          const Scene *UNUSED(scene))
{
  /* context changes */
  switch (wmn->category) {
    case NC_SCENE:
      switch (wmn->data) {
        case ND_FRAME:
        case ND_OB_ACTIVE:
        case ND_OB_SELECT:
        case ND_OB_VISIBLE:
        case ND_MODE:
        case ND_LAYER:
        case ND_TOOLSETTINGS:
        case ND_LAYER_CONTENT:
        case ND_RENDER_OPTIONS:
          ED_region_tag_redraw(ar);
          break;
      }
      break;
    case NC_SPACE:
      if (wmn->data == ND_SPACE_VIEW3D) {
        ED_region_tag_redraw(ar);
      }
      break;
    case NC_GPENCIL:
      if (wmn->data & ND_GPENCIL_EDITMODE) {
        ED_region_tag_redraw(ar);
      }
      break;
    case NC_BRUSH:
      ED_region_tag_redraw(ar);
      break;
  }

    /* From topbar, which ones are needed? split per header? */
    /* Disable for now, re-enable if neede, or remove - campbell. */
#if 0
  /* context changes */
  switch (wmn->category) {
    case NC_WM:
      if (wmn->data == ND_HISTORY) {
        ED_region_tag_redraw(ar);
      }
      break;
    case NC_SCENE:
      if (wmn->data == ND_MODE) {
        ED_region_tag_redraw(ar);
      }
      break;
    case NC_SPACE:
      if (wmn->data == ND_SPACE_VIEW3D) {
        ED_region_tag_redraw(ar);
      }
      break;
    case NC_GPENCIL:
      if (wmn->data == ND_DATA) {
        ED_region_tag_redraw(ar);
      }
      break;
  }
#endif
}

static void view3d_header_region_message_subscribe(const struct bContext *UNUSED(C),
                                                   struct WorkSpace *UNUSED(workspace),
                                                   struct Scene *UNUSED(scene),
                                                   struct bScreen *UNUSED(screen),
                                                   struct ScrArea *UNUSED(sa),
                                                   struct ARegion *ar,
                                                   struct wmMsgBus *mbus)
{
  wmMsgParams_RNA msg_key_params = {{{0}}};

  /* Only subscribe to types. */
  StructRNA *type_array[] = {
      &RNA_View3DShading,
  };

  wmMsgSubscribeValue msg_sub_value_region_tag_redraw = {
      .owner = ar,
      .user_data = ar,
      .notify = ED_region_do_msg_notify_tag_redraw,
  };

  for (int i = 0; i < ARRAY_SIZE(type_array); i++) {
    msg_key_params.ptr.type = type_array[i];
    WM_msg_subscribe_rna_params(mbus, &msg_key_params, &msg_sub_value_region_tag_redraw, __func__);
  }
}

/* add handlers, stuff you only do once or on area/region changes */
static void view3d_buttons_region_init(wmWindowManager *wm, ARegion *ar)
{
  wmKeyMap *keymap;

  ED_region_panels_init(wm, ar);

  keymap = WM_keymap_ensure(wm->defaultconf, "3D View Generic", SPACE_VIEW3D, 0);
  WM_event_add_keymap_handler(&ar->handlers, keymap);
}

void ED_view3d_buttons_region_layout_ex(const bContext *C,
                                        ARegion *ar,
                                        const char *category_override)
{
  const enum eContextObjectMode mode = CTX_data_mode_enum(C);

  const char *contexts_base[4] = {NULL};
  contexts_base[0] = CTX_data_mode_string(C);

  const char **contexts = &contexts_base[1];

  switch (mode) {
    case CTX_MODE_EDIT_MESH:
      ARRAY_SET_ITEMS(contexts, ".mesh_edit");
      break;
    case CTX_MODE_EDIT_CURVE:
      ARRAY_SET_ITEMS(contexts, ".curve_edit");
      break;
    case CTX_MODE_EDIT_SURFACE:
      ARRAY_SET_ITEMS(contexts, ".curve_edit");
      break;
    case CTX_MODE_EDIT_TEXT:
      ARRAY_SET_ITEMS(contexts, ".text_edit");
      break;
    case CTX_MODE_EDIT_ARMATURE:
      ARRAY_SET_ITEMS(contexts, ".armature_edit");
      break;
    case CTX_MODE_EDIT_METABALL:
      ARRAY_SET_ITEMS(contexts, ".mball_edit");
      break;
    case CTX_MODE_EDIT_LATTICE:
      ARRAY_SET_ITEMS(contexts, ".lattice_edit");
      break;
    case CTX_MODE_POSE:
      ARRAY_SET_ITEMS(contexts, ".posemode");
      break;
    case CTX_MODE_SCULPT:
      ARRAY_SET_ITEMS(contexts, ".paint_common", ".sculpt_mode");
      break;
    case CTX_MODE_PAINT_WEIGHT:
      ARRAY_SET_ITEMS(contexts, ".paint_common", ".weightpaint");
      break;
    case CTX_MODE_PAINT_VERTEX:
      ARRAY_SET_ITEMS(contexts, ".paint_common", ".vertexpaint");
      break;
    case CTX_MODE_PAINT_TEXTURE:
      ARRAY_SET_ITEMS(contexts, ".paint_common", ".imagepaint");
      break;
    case CTX_MODE_PARTICLE:
      ARRAY_SET_ITEMS(contexts, ".paint_common", ".particlemode");
      break;
    case CTX_MODE_OBJECT:
      ARRAY_SET_ITEMS(contexts, ".objectmode");
      break;
    case CTX_MODE_PAINT_GPENCIL:
      ARRAY_SET_ITEMS(contexts, ".greasepencil_paint");
      break;
    case CTX_MODE_SCULPT_GPENCIL:
      ARRAY_SET_ITEMS(contexts, ".greasepencil_sculpt");
      break;
    case CTX_MODE_WEIGHT_GPENCIL:
      ARRAY_SET_ITEMS(contexts, ".greasepencil_weight");
      break;
    default:
      break;
  }

  switch (mode) {
    case CTX_MODE_PAINT_GPENCIL:
      ARRAY_SET_ITEMS(contexts, ".greasepencil_paint");
      break;
    case CTX_MODE_SCULPT_GPENCIL:
      ARRAY_SET_ITEMS(contexts, ".greasepencil_sculpt");
      break;
    case CTX_MODE_WEIGHT_GPENCIL:
      ARRAY_SET_ITEMS(contexts, ".greasepencil_weight");
      break;
    case CTX_MODE_EDIT_GPENCIL:
      ARRAY_SET_ITEMS(contexts, ".greasepencil_edit");
      break;
    default:
      break;
  }

  ListBase *paneltypes = &ar->type->paneltypes;

  /* Allow drawing 3D view toolbar from non 3D view space type. */
  if (category_override != NULL) {
    SpaceType *st = BKE_spacetype_from_id(SPACE_VIEW3D);
    ARegionType *art = BKE_regiontype_from_id(st, RGN_TYPE_UI);
    paneltypes = &art->paneltypes;
  }

  const bool vertical = true;
  ED_region_panels_layout_ex(C, ar, paneltypes, contexts_base, -1, vertical, category_override);
}

static void view3d_buttons_region_layout(const bContext *C, ARegion *ar)
{
  ED_view3d_buttons_region_layout_ex(C, ar, NULL);
}

static void view3d_buttons_region_listener(wmWindow *UNUSED(win),
                                           ScrArea *UNUSED(sa),
                                           ARegion *ar,
                                           wmNotifier *wmn,
                                           const Scene *UNUSED(scene))
{
  /* context changes */
  switch (wmn->category) {
    case NC_ANIMATION:
      switch (wmn->data) {
        case ND_KEYFRAME_PROP:
        case ND_NLA_ACTCHANGE:
          ED_region_tag_redraw(ar);
          break;
        case ND_NLA:
        case ND_KEYFRAME:
          if (ELEM(wmn->action, NA_EDITED, NA_ADDED, NA_REMOVED)) {
            ED_region_tag_redraw(ar);
          }
          break;
      }
      break;
    case NC_SCENE:
      switch (wmn->data) {
        case ND_FRAME:
        case ND_OB_ACTIVE:
        case ND_OB_SELECT:
        case ND_OB_VISIBLE:
        case ND_MODE:
        case ND_LAYER:
        case ND_LAYER_CONTENT:
        case ND_TOOLSETTINGS:
          ED_region_tag_redraw(ar);
          break;
      }
      switch (wmn->action) {
        case NA_EDITED:
          ED_region_tag_redraw(ar);
          break;
      }
      break;
    case NC_OBJECT:
      switch (wmn->data) {
        case ND_BONE_ACTIVE:
        case ND_BONE_SELECT:
        case ND_TRANSFORM:
        case ND_POSE:
        case ND_DRAW:
        case ND_KEYS:
        case ND_MODIFIER:
          ED_region_tag_redraw(ar);
          break;
      }
      break;
    case NC_GEOM:
      switch (wmn->data) {
        case ND_DATA:
        case ND_VERTEX_GROUP:
        case ND_SELECT:
          ED_region_tag_redraw(ar);
          break;
      }
      if (wmn->action == NA_EDITED) {
        ED_region_tag_redraw(ar);
      }
      break;
    case NC_TEXTURE:
    case NC_MATERIAL:
      /* for brush textures */
      ED_region_tag_redraw(ar);
      break;
    case NC_BRUSH:
      /* NA_SELECTED is used on brush changes */
      if (ELEM(wmn->action, NA_EDITED, NA_SELECTED)) {
        ED_region_tag_redraw(ar);
      }
      break;
    case NC_SPACE:
      if (wmn->data == ND_SPACE_VIEW3D) {
        ED_region_tag_redraw(ar);
      }
      break;
    case NC_ID:
      if (wmn->action == NA_RENAME) {
        ED_region_tag_redraw(ar);
      }
      break;
    case NC_GPENCIL:
      if ((wmn->data & (ND_DATA | ND_GPENCIL_EDITMODE)) || (wmn->action == NA_EDITED)) {
        ED_region_tag_redraw(ar);
      }
      break;
    case NC_IMAGE:
      /* Update for the image layers in texture paint. */
      if (wmn->action == NA_EDITED) {
        ED_region_tag_redraw(ar);
      }
      break;
  }
}

/* add handlers, stuff you only do once or on area/region changes */
static void view3d_tools_region_init(wmWindowManager *wm, ARegion *ar)
{
  wmKeyMap *keymap;

  ED_region_panels_init(wm, ar);

  keymap = WM_keymap_ensure(wm->defaultconf, "3D View Generic", SPACE_VIEW3D, 0);
  WM_event_add_keymap_handler(&ar->handlers, keymap);
}

static void view3d_tools_region_draw(const bContext *C, ARegion *ar)
{
  ED_region_panels_ex(C, ar, (const char *[]){CTX_data_mode_string(C), NULL}, -1, true);
}

/* area (not region) level listener */
static void space_view3d_listener(wmWindow *UNUSED(win),
                                  ScrArea *sa,
                                  struct wmNotifier *wmn,
                                  Scene *UNUSED(scene))
{
  View3D *v3d = sa->spacedata.first;

  /* context changes */
  switch (wmn->category) {
    case NC_SCENE:
      switch (wmn->data) {
        case ND_WORLD:
          if (v3d->flag2 & V3D_HIDE_OVERLAYS) {
            ED_area_tag_redraw_regiontype(sa, RGN_TYPE_WINDOW);
          }
          break;
      }
      break;
    case NC_WORLD:
      switch (wmn->data) {
        case ND_WORLD_DRAW:
        case ND_WORLD:
          if (v3d->shading.background_type == V3D_SHADING_BACKGROUND_WORLD) {
            ED_area_tag_redraw_regiontype(sa, RGN_TYPE_WINDOW);
          }
          break;
      }
      break;
    case NC_MATERIAL:
      switch (wmn->data) {
        case ND_NODES:
          if (v3d->shading.type == OB_TEXTURE) {
            ED_area_tag_redraw_regiontype(sa, RGN_TYPE_WINDOW);
          }
          break;
      }
      break;
  }
}

static void space_view3d_refresh(const bContext *C, ScrArea *UNUSED(sa))
{
  Scene *scene = CTX_data_scene(C);
  LightCache *lcache = scene->eevee.light_cache;

  if (lcache && (lcache->flag & LIGHTCACHE_UPDATE_AUTO) != 0) {
    lcache->flag &= ~LIGHTCACHE_UPDATE_AUTO;
    view3d_lightcache_update((bContext *)C);
  }
}

const char *view3d_context_dir[] = {
    "active_base",
    "active_object",
    NULL,
};

static int view3d_context(const bContext *C, const char *member, bContextDataResult *result)
{
  /* fallback to the scene layer,
   * allows duplicate and other object operators to run outside the 3d view */

  if (CTX_data_dir(member)) {
    CTX_data_dir_set(result, view3d_context_dir);
  }
  else if (CTX_data_equals(member, "active_base")) {
    Scene *scene = CTX_data_scene(C);
    ViewLayer *view_layer = CTX_data_view_layer(C);
    if (view_layer->basact) {
      Object *ob = view_layer->basact->object;
      /* if hidden but in edit mode, we still display, can happen with animation */
      if ((view_layer->basact->flag & BASE_VISIBLE) != 0 || (ob->mode & OB_MODE_EDIT)) {
        CTX_data_pointer_set(result, &scene->id, &RNA_ObjectBase, view_layer->basact);
      }
    }

    return 1;
  }
  else if (CTX_data_equals(member, "active_object")) {
    ViewLayer *view_layer = CTX_data_view_layer(C);
    if (view_layer->basact) {
      Object *ob = view_layer->basact->object;
      /* if hidden but in edit mode, we still display, can happen with animation */
      if ((view_layer->basact->flag & BASE_VISIBLE) != 0 || (ob->mode & OB_MODE_EDIT) != 0) {
        CTX_data_id_pointer_set(result, &ob->id);
      }
    }

    return 1;
  }
  else {
    return 0; /* not found */
  }

  return -1; /* found but not available */
}

static void view3d_id_remap(ScrArea *sa, SpaceLink *slink, ID *old_id, ID *new_id)
{
  View3D *v3d;
  ARegion *ar;
  bool is_local = false;

  if (!ELEM(GS(old_id->name), ID_OB, ID_MA, ID_IM, ID_MC)) {
    return;
  }

  for (v3d = (View3D *)slink; v3d; v3d = v3d->localvd, is_local = true) {
    if ((ID *)v3d->camera == old_id) {
      v3d->camera = (Object *)new_id;
      if (!new_id) {
        /* 3D view might be inactive, in that case needs to use slink->regionbase */
        ListBase *regionbase = (slink == sa->spacedata.first) ? &sa->regionbase :
                                                                &slink->regionbase;
        for (ar = regionbase->first; ar; ar = ar->next) {
          if (ar->regiontype == RGN_TYPE_WINDOW) {
            RegionView3D *rv3d = is_local ? ((RegionView3D *)ar->regiondata)->localvd :
                                            ar->regiondata;
            if (rv3d && (rv3d->persp == RV3D_CAMOB)) {
              rv3d->persp = RV3D_PERSP;
            }
          }
        }
      }
    }

    /* Values in local-view aren't used, see: T52663 */
    if (is_local == false) {
      if ((ID *)v3d->ob_centre == old_id) {
        v3d->ob_centre = (Object *)new_id;
        /* Otherwise, bonename may remain valid...
         * We could be smart and check this, too? */
        if (new_id == NULL) {
          v3d->ob_centre_bone[0] = '\0';
        }
      }
    }

    if (is_local) {
      break;
    }
  }
}

/* only called once, from space/spacetypes.c */
void ED_spacetype_view3d(void)
{
  SpaceType *st = MEM_callocN(sizeof(SpaceType), "spacetype view3d");
  ARegionType *art;

  st->spaceid = SPACE_VIEW3D;
  strncpy(st->name, "View3D", BKE_ST_MAXNAME);

  st->new = view3d_new;
  st->free = view3d_free;
  st->init = view3d_init;
  st->listener = space_view3d_listener;
  st->refresh = space_view3d_refresh;
  st->duplicate = view3d_duplicate;
  st->operatortypes = view3d_operatortypes;
  st->keymap = view3d_keymap;
  st->dropboxes = view3d_dropboxes;
  st->gizmos = view3d_widgets;
  st->context = view3d_context;
  st->id_remap = view3d_id_remap;

  /* regions: main window */
  art = MEM_callocN(sizeof(ARegionType), "spacetype view3d main region");
  art->regionid = RGN_TYPE_WINDOW;
  art->keymapflag = ED_KEYMAP_GIZMO | ED_KEYMAP_TOOL | ED_KEYMAP_GPENCIL;
  art->draw = view3d_main_region_draw;
  art->init = view3d_main_region_init;
  art->exit = view3d_main_region_exit;
  art->free = view3d_main_region_free;
  art->duplicate = view3d_main_region_duplicate;
  art->listener = view3d_main_region_listener;
  art->message_subscribe = view3d_main_region_message_subscribe;
  art->cursor = view3d_main_region_cursor;
  art->lock = 1; /* can become flag, see BKE_spacedata_draw_locks */
  BLI_addhead(&st->regiontypes, art);

  /* regions: listview/buttons */
  art = MEM_callocN(sizeof(ARegionType), "spacetype view3d buttons region");
  art->regionid = RGN_TYPE_UI;
  art->prefsizex = UI_SIDEBAR_PANEL_WIDTH;
  art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_FRAMES;
  art->listener = view3d_buttons_region_listener;
  art->message_subscribe = ED_area_do_mgs_subscribe_for_tool_ui;
  art->init = view3d_buttons_region_init;
  art->layout = view3d_buttons_region_layout;
  art->draw = ED_region_panels_draw;
  BLI_addhead(&st->regiontypes, art);

  view3d_buttons_register(art);

  /* regions: tool(bar) */
  art = MEM_callocN(sizeof(ARegionType), "spacetype view3d tools region");
  art->regionid = RGN_TYPE_TOOLS;
  art->prefsizex = 58; /* XXX */
  art->prefsizey = 50; /* XXX */
  art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_FRAMES;
  art->listener = view3d_buttons_region_listener;
  art->message_subscribe = ED_region_generic_tools_region_message_subscribe;
  art->snap_size = ED_region_generic_tools_region_snap_size;
  art->init = view3d_tools_region_init;
  art->draw = view3d_tools_region_draw;
  BLI_addhead(&st->regiontypes, art);

  /* regions: tool header */
  art = MEM_callocN(sizeof(ARegionType), "spacetype view3d tool header region");
  art->regionid = RGN_TYPE_TOOL_HEADER;
  art->prefsizey = HEADERY;
  art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_VIEW2D | ED_KEYMAP_FRAMES | ED_KEYMAP_HEADER;
  art->listener = view3d_header_region_listener;
  art->message_subscribe = ED_area_do_mgs_subscribe_for_tool_header;
  art->init = view3d_header_region_init;
  art->draw = view3d_header_region_draw;
  BLI_addhead(&st->regiontypes, art);

  /* regions: header */
  art = MEM_callocN(sizeof(ARegionType), "spacetype view3d header region");
  art->regionid = RGN_TYPE_HEADER;
  art->prefsizey = HEADERY;
  art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_VIEW2D | ED_KEYMAP_FRAMES | ED_KEYMAP_HEADER;
  art->listener = view3d_header_region_listener;
  art->message_subscribe = view3d_header_region_message_subscribe;
  art->init = view3d_header_region_init;
  art->draw = view3d_header_region_draw;
  BLI_addhead(&st->regiontypes, art);

  /* regions: hud */
  art = ED_area_type_hud(st->spaceid);
  BLI_addhead(&st->regiontypes, art);

  BKE_spacetype_register(st);
}
