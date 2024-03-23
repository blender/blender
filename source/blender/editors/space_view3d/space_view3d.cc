/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spview3d
 */

/* Allow using deprecated functionality for .blend file I/O. */
#define DNA_DEPRECATED_ALLOW

#include <cstdio>
#include <cstring>

#include "AS_asset_representation.hh"

#include "DNA_collection_types.h"
#include "DNA_defaults.h"
#include "DNA_lightprobe_types.h"
#include "DNA_material_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_view3d_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"
#include "BLI_math_vector.hh"
#include "BLI_utildefines.h"

#include "BLT_translation.hh"

#include "BKE_asset.hh"
#include "BKE_context.hh"
#include "BKE_global.hh"
#include "BKE_gpencil_legacy.h"
#include "BKE_idprop.h"
#include "BKE_layer.hh"
#include "BKE_lib_id.hh"
#include "BKE_lib_query.hh"
#include "BKE_lib_remap.hh"
#include "BKE_main.hh"
#include "BKE_object.hh"
#include "BKE_scene.hh"
#include "BKE_screen.hh"
#include "BKE_viewer_path.hh"

#include "ED_asset_shelf.hh"
#include "ED_geometry.hh"
#include "ED_object.hh"
#include "ED_outliner.hh"
#include "ED_render.hh"
#include "ED_screen.hh"
#include "ED_space_api.hh"
#include "ED_transform.hh"
#include "ED_undo.hh"

#include "GPU_matrix.hh"

#include "DRW_engine.hh"

#include "WM_api.hh"
#include "WM_message.hh"
#include "WM_toolsystem.hh"
#include "WM_types.hh"

#include "RNA_access.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "BLO_read_write.hh"

#ifdef WITH_PYTHON
#  include "BPY_extern.h"
#endif

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_build.hh"

#include "view3d_intern.h" /* own include */
#include "view3d_navigate.hh"

/* ******************** manage regions ********************* */

bool ED_view3d_area_user_region(const ScrArea *area, const View3D *v3d, ARegion **r_region)
{
  RegionView3D *rv3d = nullptr;
  ARegion *region_unlock_user = nullptr;
  ARegion *region_unlock = nullptr;
  const ListBase *region_list = (v3d == area->spacedata.first) ? &area->regionbase :
                                                                 &v3d->regionbase;

  BLI_assert(v3d->spacetype == SPACE_VIEW3D);

  LISTBASE_FOREACH (ARegion *, region, region_list) {
    /* find the first unlocked rv3d */
    if (region->regiondata && region->regiontype == RGN_TYPE_WINDOW) {
      rv3d = static_cast<RegionView3D *>(region->regiondata);
      if ((rv3d->viewlock & RV3D_LOCK_ROTATION) == 0) {
        region_unlock = region;
        if (ELEM(rv3d->persp, RV3D_PERSP, RV3D_CAMOB)) {
          region_unlock_user = region;
          break;
        }
      }
    }
  }

  /* camera/perspective view get priority when the active region is locked */
  if (region_unlock_user) {
    *r_region = region_unlock_user;
    return true;
  }

  if (region_unlock) {
    *r_region = region_unlock;
    return true;
  }

  return false;
}

void ED_view3d_init_mats_rv3d(const Object *ob, RegionView3D *rv3d)
{
  /* local viewmat and persmat, to calculate projections */
  mul_m4_m4m4(rv3d->viewmatob, rv3d->viewmat, ob->object_to_world().ptr());
  mul_m4_m4m4(rv3d->persmatob, rv3d->persmat, ob->object_to_world().ptr());

  /* initializes object space clipping, speeds up clip tests */
  ED_view3d_clipping_local(rv3d, ob->object_to_world().ptr());
}

void ED_view3d_init_mats_rv3d_gl(const Object *ob, RegionView3D *rv3d)
{
  ED_view3d_init_mats_rv3d(ob, rv3d);

  /* We have to multiply instead of loading `viewmatob` to make
   * it work with duplis using display-lists, otherwise it will
   * override the dupli-matrix. */
  GPU_matrix_mul(ob->object_to_world().ptr());
}

#ifndef NDEBUG
void ED_view3d_clear_mats_rv3d(RegionView3D *rv3d)
{
  zero_m4(rv3d->viewmatob);
  zero_m4(rv3d->persmatob);
}

void ED_view3d_check_mats_rv3d(RegionView3D *rv3d)
{
  BLI_ASSERT_ZERO_M4(rv3d->viewmatob);
  BLI_ASSERT_ZERO_M4(rv3d->persmatob);
}
#endif

void ED_view3d_stop_render_preview(wmWindowManager *wm, ARegion *region)
{
  RegionView3D *rv3d = static_cast<RegionView3D *>(region->regiondata);

  if (rv3d->view_render) {
#ifdef WITH_PYTHON
    BPy_BEGIN_ALLOW_THREADS;
#endif

    WM_jobs_kill_type(wm, region, WM_JOB_TYPE_RENDER_PREVIEW);

#ifdef WITH_PYTHON
    BPy_END_ALLOW_THREADS;
#endif

    DRW_engine_external_free(rv3d);
  }

  /* A bit overkill but this make sure the viewport is reset completely. (fclem) */
  WM_draw_region_free(region);
}

void ED_view3d_shade_update(Main *bmain, View3D *v3d, ScrArea *area)
{
  wmWindowManager *wm = static_cast<wmWindowManager *>(bmain->wm.first);

  if (v3d->shading.type != OB_RENDER) {
    LISTBASE_FOREACH (ARegion *, region, &area->regionbase) {
      if ((region->regiontype == RGN_TYPE_WINDOW) && region->regiondata) {
        ED_view3d_stop_render_preview(wm, region);
      }
    }
  }
}

/* ******************** default callbacks for view3d space ***************** */

static SpaceLink *view3d_create(const ScrArea * /*area*/, const Scene *scene)
{
  ARegion *region;
  View3D *v3d;
  RegionView3D *rv3d;

  v3d = DNA_struct_default_alloc(View3D);

  if (scene) {
    v3d->camera = scene->camera;
  }

  /* header */
  region = MEM_cnew<ARegion>("header for view3d");

  BLI_addtail(&v3d->regionbase, region);
  region->regiontype = RGN_TYPE_HEADER;
  region->alignment = (U.uiflag & USER_HEADER_BOTTOM) ? RGN_ALIGN_BOTTOM : RGN_ALIGN_TOP;

  /* tool header */
  region = MEM_cnew<ARegion>("tool header for view3d");

  BLI_addtail(&v3d->regionbase, region);
  region->regiontype = RGN_TYPE_TOOL_HEADER;
  region->alignment = (U.uiflag & USER_HEADER_BOTTOM) ? RGN_ALIGN_BOTTOM : RGN_ALIGN_TOP;
  region->flag = RGN_FLAG_HIDDEN | RGN_FLAG_HIDDEN_BY_USER;

  /* asset shelf */
  region = MEM_cnew<ARegion>("asset shelf for view3d");

  BLI_addtail(&v3d->regionbase, region);
  region->regiontype = RGN_TYPE_ASSET_SHELF;
  region->alignment = RGN_ALIGN_BOTTOM;
  region->flag |= RGN_FLAG_HIDDEN;

  /* asset shelf header */
  region = MEM_cnew<ARegion>("asset shelf header for view3d");
  BLI_addtail(&v3d->regionbase, region);
  region->regiontype = RGN_TYPE_ASSET_SHELF_HEADER;
  region->alignment = RGN_ALIGN_BOTTOM | RGN_ALIGN_HIDE_WITH_PREV;

  /* tool shelf */
  region = MEM_cnew<ARegion>("toolshelf for view3d");

  BLI_addtail(&v3d->regionbase, region);
  region->regiontype = RGN_TYPE_TOOLS;
  region->alignment = RGN_ALIGN_LEFT;
  region->flag = RGN_FLAG_HIDDEN;

  /* buttons/list view */
  region = MEM_cnew<ARegion>("buttons for view3d");

  BLI_addtail(&v3d->regionbase, region);
  region->regiontype = RGN_TYPE_UI;
  region->alignment = RGN_ALIGN_RIGHT;
  region->flag = RGN_FLAG_HIDDEN;

  /* main region */
  region = MEM_cnew<ARegion>("main region for view3d");

  BLI_addtail(&v3d->regionbase, region);
  region->regiontype = RGN_TYPE_WINDOW;

  region->regiondata = MEM_cnew<RegionView3D>("region view3d");
  rv3d = static_cast<RegionView3D *>(region->regiondata);
  rv3d->viewquat[0] = 1.0f;
  rv3d->persp = RV3D_PERSP;
  rv3d->view = RV3D_VIEW_USER;
  rv3d->dist = 10.0;

  return (SpaceLink *)v3d;
}

/* Doesn't free the space-link itself. */
static void view3d_free(SpaceLink *sl)
{
  View3D *vd = (View3D *)sl;

  if (vd->localvd) {
    MEM_freeN(vd->localvd);
  }

  MEM_SAFE_FREE(vd->runtime.local_stats);

  if (vd->runtime.properties_storage_free) {
    vd->runtime.properties_storage_free(vd->runtime.properties_storage);
    vd->runtime.properties_storage_free = nullptr;
  }

  if (vd->shading.prop) {
    IDP_FreeProperty(vd->shading.prop);
    vd->shading.prop = nullptr;
  }

  BKE_viewer_path_clear(&vd->viewer_path);
}

/* spacetype; init callback */
static void view3d_init(wmWindowManager * /*wm*/, ScrArea * /*area*/) {}

static void view3d_exit(wmWindowManager * /*wm*/, ScrArea *area)
{
  BLI_assert(area->spacetype == SPACE_VIEW3D);
  View3D *v3d = static_cast<View3D *>(area->spacedata.first);
  MEM_SAFE_FREE(v3d->runtime.local_stats);
}

static SpaceLink *view3d_duplicate(SpaceLink *sl)
{
  View3D *v3do = (View3D *)sl;
  View3D *v3dn = static_cast<View3D *>(MEM_dupallocN(sl));

  memset(&v3dn->runtime, 0x0, sizeof(v3dn->runtime));

  /* clear or remove stuff from old */

  if (v3dn->localvd) {
    v3dn->localvd = nullptr;
  }

  v3dn->local_collections_uid = 0;
  v3dn->flag &= ~(V3D_LOCAL_COLLECTIONS | V3D_XR_SESSION_MIRROR);

  if (v3dn->shading.type == OB_RENDER) {
    v3dn->shading.type = OB_SOLID;
  }

  if (v3dn->shading.prop) {
    v3dn->shading.prop = IDP_CopyProperty(v3do->shading.prop);
  }

  BKE_viewer_path_copy(&v3dn->viewer_path, &v3do->viewer_path);

  /* copy or clear inside new stuff */

  return (SpaceLink *)v3dn;
}

/* add handlers, stuff you only do once or on area/region changes */
static void view3d_main_region_init(wmWindowManager *wm, ARegion *region)
{
  ListBase *lb;
  wmKeyMap *keymap;

  /* object ops. */

  /* important to be before Pose keymap since they can both be enabled at once */
  keymap = WM_keymap_ensure(
      wm->defaultconf, "Paint Face Mask (Weight, Vertex, Texture)", SPACE_EMPTY, RGN_TYPE_WINDOW);
  WM_event_add_keymap_handler(&region->handlers, keymap);

  keymap = WM_keymap_ensure(
      wm->defaultconf, "Paint Vertex Selection (Weight, Vertex)", SPACE_EMPTY, RGN_TYPE_WINDOW);
  WM_event_add_keymap_handler(&region->handlers, keymap);

  /* Before 'Weight/Vertex Paint' so adding curve points is not overridden. */
  keymap = WM_keymap_ensure(wm->defaultconf, "Paint Curve", SPACE_EMPTY, RGN_TYPE_WINDOW);
  WM_event_add_keymap_handler(&region->handlers, keymap);

  /* Before 'Pose' so weight paint menus aren't overridden by pose menus. */
  keymap = WM_keymap_ensure(wm->defaultconf, "Weight Paint", SPACE_EMPTY, RGN_TYPE_WINDOW);
  WM_event_add_keymap_handler(&region->handlers, keymap);

  keymap = WM_keymap_ensure(wm->defaultconf, "Vertex Paint", SPACE_EMPTY, RGN_TYPE_WINDOW);
  WM_event_add_keymap_handler(&region->handlers, keymap);

  /* pose is not modal, operator poll checks for this */
  keymap = WM_keymap_ensure(wm->defaultconf, "Pose", SPACE_EMPTY, RGN_TYPE_WINDOW);
  WM_event_add_keymap_handler(&region->handlers, keymap);

  keymap = WM_keymap_ensure(wm->defaultconf, "Object Mode", SPACE_EMPTY, RGN_TYPE_WINDOW);
  WM_event_add_keymap_handler(&region->handlers, keymap);

  keymap = WM_keymap_ensure(wm->defaultconf, "Curve", SPACE_EMPTY, RGN_TYPE_WINDOW);
  WM_event_add_keymap_handler(&region->handlers, keymap);

  keymap = WM_keymap_ensure(wm->defaultconf, "Curves", SPACE_EMPTY, RGN_TYPE_WINDOW);
  WM_event_add_keymap_handler(&region->handlers, keymap);

  keymap = WM_keymap_ensure(wm->defaultconf, "Image Paint", SPACE_EMPTY, RGN_TYPE_WINDOW);
  WM_event_add_keymap_handler(&region->handlers, keymap);

  keymap = WM_keymap_ensure(wm->defaultconf, "Sculpt", SPACE_EMPTY, RGN_TYPE_WINDOW);
  WM_event_add_keymap_handler(&region->handlers, keymap);

  keymap = WM_keymap_ensure(wm->defaultconf, "Mesh", SPACE_EMPTY, RGN_TYPE_WINDOW);
  WM_event_add_keymap_handler(&region->handlers, keymap);

  keymap = WM_keymap_ensure(wm->defaultconf, "Armature", SPACE_EMPTY, RGN_TYPE_WINDOW);
  WM_event_add_keymap_handler(&region->handlers, keymap);

  keymap = WM_keymap_ensure(wm->defaultconf, "Metaball", SPACE_EMPTY, RGN_TYPE_WINDOW);
  WM_event_add_keymap_handler(&region->handlers, keymap);

  keymap = WM_keymap_ensure(wm->defaultconf, "Lattice", SPACE_EMPTY, RGN_TYPE_WINDOW);
  WM_event_add_keymap_handler(&region->handlers, keymap);

  keymap = WM_keymap_ensure(wm->defaultconf, "Particle", SPACE_EMPTY, RGN_TYPE_WINDOW);
  WM_event_add_keymap_handler(&region->handlers, keymap);

  keymap = WM_keymap_ensure(wm->defaultconf, "Sculpt Curves", SPACE_EMPTY, RGN_TYPE_WINDOW);
  WM_event_add_keymap_handler(&region->handlers, keymap);

  /* Note: Grease Pencil handlers used to be added using `ED_KEYMAP_GPENCIL` in
   * `ed_default_handlers` because it needed to be added to multiple editors (as other editors use
   * annotations.). But for OB_GREASE_PENCIL, we only need it to register the keymaps for the
   * 3D View. */
  keymap = WM_keymap_ensure(
      wm->defaultconf, "Grease Pencil Edit Mode", SPACE_EMPTY, RGN_TYPE_WINDOW);
  WM_event_add_keymap_handler(&region->handlers, keymap);

  keymap = WM_keymap_ensure(
      wm->defaultconf, "Grease Pencil Paint Mode", SPACE_EMPTY, RGN_TYPE_WINDOW);
  WM_event_add_keymap_handler(&region->handlers, keymap);

  /* Edit-font key-map swallows almost all (because of text input). */
  keymap = WM_keymap_ensure(wm->defaultconf, "Font", SPACE_EMPTY, RGN_TYPE_WINDOW);
  WM_event_add_keymap_handler(&region->handlers, keymap);

  keymap = WM_keymap_ensure(wm->defaultconf, "Object Non-modal", SPACE_EMPTY, RGN_TYPE_WINDOW);
  WM_event_add_keymap_handler(&region->handlers, keymap);

  keymap = WM_keymap_ensure(wm->defaultconf, "Frames", SPACE_EMPTY, RGN_TYPE_WINDOW);
  WM_event_add_keymap_handler(&region->handlers, keymap);

  /* own keymap, last so modes can override it */
  keymap = WM_keymap_ensure(wm->defaultconf, "3D View Generic", SPACE_VIEW3D, RGN_TYPE_WINDOW);
  WM_event_add_keymap_handler(&region->handlers, keymap);

  keymap = WM_keymap_ensure(wm->defaultconf, "3D View", SPACE_VIEW3D, RGN_TYPE_WINDOW);
  WM_event_add_keymap_handler(&region->handlers, keymap);

  /* add drop boxes */
  lb = WM_dropboxmap_find("View3D", SPACE_VIEW3D, RGN_TYPE_WINDOW);

  WM_event_add_dropbox_handler(&region->handlers, lb);
}

static void view3d_main_region_exit(wmWindowManager *wm, ARegion *region)
{
  ED_view3d_stop_render_preview(wm, region);
}

static bool view3d_drop_in_main_region_poll(bContext *C, const wmEvent *event)
{
  ScrArea *area = CTX_wm_area(C);
  return ED_region_overlap_isect_any_xy(area, event->xy) == false;
}

static ID_Type view3d_drop_id_in_main_region_poll_get_id_type(bContext *C,
                                                              wmDrag *drag,
                                                              const wmEvent *event)
{
  const ScrArea *area = CTX_wm_area(C);

  if (ED_region_overlap_isect_any_xy(area, event->xy)) {
    return ID_Type(0);
  }
  if (!view3d_drop_in_main_region_poll(C, event)) {
    return ID_Type(0);
  }

  ID *local_id = WM_drag_get_local_ID(drag, 0);
  if (local_id) {
    return GS(local_id->name);
  }

  wmDragAsset *asset_drag = WM_drag_get_asset_data(drag, 0);
  if (asset_drag) {
    return asset_drag->asset->get_id_type();
  }

  return ID_Type(0);
}

static bool view3d_drop_id_in_main_region_poll(bContext *C,
                                               wmDrag *drag,
                                               const wmEvent *event,
                                               ID_Type id_type)
{
  if (!view3d_drop_in_main_region_poll(C, event)) {
    return false;
  }

  return WM_drag_is_ID_type(drag, id_type);
}

static void view3d_ob_drop_on_enter(wmDropBox *drop, wmDrag *drag)
{
  V3DSnapCursorState *state = static_cast<V3DSnapCursorState *>(drop->draw_data);
  if (state) {
    return;
  }

  /* Don't use the snap cursor when linking the object. Object transform isn't editable then and
   * would be reset on reload. */
  if (WM_drag_asset_will_import_linked(drag)) {
    return;
  }

  state = static_cast<V3DSnapCursorState *>(ED_view3d_cursor_snap_state_create());
  drop->draw_data = state;
  state->draw_plane = true;

  float dimensions[3] = {0.0f};
  if (drag->type == WM_DRAG_ID) {
    Object *ob = (Object *)WM_drag_get_local_ID(drag, ID_OB);
    BKE_object_dimensions_eval_cached_get(ob, dimensions);
  }
  else {
    AssetMetaData *meta_data = WM_drag_get_asset_meta_data(drag, ID_OB);
    IDProperty *dimensions_prop = BKE_asset_metadata_idprop_find(meta_data, "dimensions");
    if (dimensions_prop) {
      copy_v3_v3(dimensions, static_cast<float *>(IDP_Array(dimensions_prop)));
    }
  }

  if (!is_zero_v3(dimensions)) {
    mul_v3_v3fl(state->box_dimensions, dimensions, 0.5f);
    UI_GetThemeColor4ubv(TH_GIZMO_PRIMARY, state->color_box);
    state->draw_box = true;
  }
}

static void view3d_ob_drop_on_exit(wmDropBox *drop, wmDrag * /*drag*/)
{
  V3DSnapCursorState *state = static_cast<V3DSnapCursorState *>(drop->draw_data);
  if (state) {
    ED_view3d_cursor_snap_state_free(state);
    drop->draw_data = nullptr;
  }
}

static bool view3d_ob_drop_poll(bContext *C, wmDrag *drag, const wmEvent *event)
{
  return view3d_drop_id_in_main_region_poll(C, drag, event, ID_OB);
}
static bool view3d_ob_drop_poll_external_asset(bContext *C, wmDrag *drag, const wmEvent *event)
{
  if (!view3d_ob_drop_poll(C, drag, event) || (drag->type != WM_DRAG_ASSET)) {
    return false;
  }
  return true;
}

/**
 * \note the term local here refers to not being an external asset,
 * poll will succeed for linked library objects.
 */
static bool view3d_ob_drop_poll_local_id(bContext *C, wmDrag *drag, const wmEvent *event)
{
  if (!view3d_ob_drop_poll(C, drag, event) || (drag->type != WM_DRAG_ID)) {
    return false;
  }
  return true;
}

static bool view3d_collection_drop_poll(bContext *C, wmDrag *drag, const wmEvent *event)
{
  return view3d_drop_id_in_main_region_poll(C, drag, event, ID_GR);
}

static bool view3d_collection_drop_poll_local_id(bContext *C, wmDrag *drag, const wmEvent *event)
{
  if (!view3d_collection_drop_poll(C, drag, event) || (drag->type != WM_DRAG_ID)) {
    return false;
  }
  return true;
}

static bool view3d_collection_drop_poll_external_asset(bContext *C,
                                                       wmDrag *drag,
                                                       const wmEvent *event)
{
  if (!view3d_collection_drop_poll(C, drag, event) || (drag->type != WM_DRAG_ASSET)) {
    return false;
  }
  return true;
}

static bool view3d_mat_drop_poll(bContext *C, wmDrag *drag, const wmEvent *event)
{
  return view3d_drop_id_in_main_region_poll(C, drag, event, ID_MA);
}

static std::string view3d_mat_drop_tooltip(bContext *C,
                                           wmDrag *drag,
                                           const int xy[2],
                                           wmDropBox * /*drop*/)
{
  const char *name = WM_drag_get_item_name(drag);
  ARegion *region = CTX_wm_region(C);
  const int mval[2] = {
      xy[0] - region->winrct.xmin,
      xy[1] - region->winrct.ymin,
  };
  return ED_object_ot_drop_named_material_tooltip(C, name, mval);
}

static bool view3d_world_drop_poll(bContext *C, wmDrag *drag, const wmEvent *event)
{
  return view3d_drop_id_in_main_region_poll(C, drag, event, ID_WO);
}

static bool view3d_object_data_drop_poll(bContext *C, wmDrag *drag, const wmEvent *event)
{
  ID_Type id_type = view3d_drop_id_in_main_region_poll_get_id_type(C, drag, event);
  if (id_type && OB_DATA_SUPPORT_ID(id_type)) {
    return true;
  }
  return false;
}

static std::string view3d_object_data_drop_tooltip(bContext * /*C*/,
                                                   wmDrag * /*drag*/,
                                                   const int /*xy*/[2],
                                                   wmDropBox * /*drop*/)
{
  return TIP_("Create object instance from object-data");
}

static bool view3d_ima_drop_poll(bContext *C, wmDrag *drag, const wmEvent *event)
{
  if (ED_region_overlap_isect_any_xy(CTX_wm_area(C), event->xy)) {
    return false;
  }
  if (drag->type == WM_DRAG_PATH) {
    const eFileSel_File_Types file_type = eFileSel_File_Types(WM_drag_get_path_file_type(drag));
    return ELEM(file_type, FILE_TYPE_IMAGE, FILE_TYPE_MOVIE);
  }

  return WM_drag_is_ID_type(drag, ID_IM);
}

static bool view3d_ima_bg_is_camera_view(bContext *C)
{
  RegionView3D *rv3d = CTX_wm_region_view3d(C);
  if (rv3d && (rv3d->persp == RV3D_CAMOB)) {
    View3D *v3d = CTX_wm_view3d(C);
    if (v3d && v3d->camera && v3d->camera->type == OB_CAMERA) {
      return true;
    }
  }
  return false;
}

static bool view3d_ima_bg_drop_poll(bContext *C, wmDrag *drag, const wmEvent *event)
{
  if (!view3d_ima_drop_poll(C, drag, event)) {
    return false;
  }

  if (ED_view3d_is_object_under_cursor(C, event->mval)) {
    return false;
  }

  return view3d_ima_bg_is_camera_view(C);
}

static bool view3d_ima_empty_drop_poll(bContext *C, wmDrag *drag, const wmEvent *event)
{
  if (!view3d_ima_drop_poll(C, drag, event)) {
    return false;
  }

  Object *ob = ED_view3d_give_object_under_cursor(C, event->mval);

  if (ob == nullptr) {
    return true;
  }

  if (ob->type == OB_EMPTY && ob->empty_drawtype == OB_EMPTY_IMAGE) {
    return true;
  }

  return false;
}

static bool view3d_volume_drop_poll(bContext * /*C*/, wmDrag *drag, const wmEvent * /*event*/)
{
  const eFileSel_File_Types file_type = eFileSel_File_Types(WM_drag_get_path_file_type(drag));
  return (drag->type == WM_DRAG_PATH) && (file_type == FILE_TYPE_VOLUME);
}

static bool view3d_geometry_nodes_drop_poll(bContext *C, wmDrag *drag, const wmEvent *event)
{
  if (!view3d_drop_id_in_main_region_poll(C, drag, event, ID_NT)) {
    return false;
  }

  if (drag->type == WM_DRAG_ID) {
    const bNodeTree *node_tree = reinterpret_cast<const bNodeTree *>(
        WM_drag_get_local_ID(drag, ID_NT));
    if (!node_tree) {
      return false;
    }
    return node_tree->type == NTREE_GEOMETRY;
  }

  if (drag->type == WM_DRAG_ASSET) {
    const wmDragAsset *asset_data = WM_drag_get_asset_data(drag, ID_NT);
    if (!asset_data) {
      return false;
    }
    const AssetMetaData *metadata = &asset_data->asset->get_metadata();
    const IDProperty *tree_type = BKE_asset_metadata_idprop_find(metadata, "type");
    if (!tree_type || IDP_Int(tree_type) != NTREE_GEOMETRY) {
      return false;
    }
    if (wmDropBox *drop_box = drag->drop_state.active_dropbox) {
      const uint32_t uid = RNA_int_get(drop_box->ptr, "session_uid");
      const bNodeTree *node_tree = reinterpret_cast<const bNodeTree *>(
          BKE_libblock_find_session_uid(CTX_data_main(C), ID_NT, uid));
      if (node_tree) {
        return node_tree->type == NTREE_GEOMETRY;
      }
    }
  }
  return true;
}

static std::string view3d_geometry_nodes_drop_tooltip(bContext *C,
                                                      wmDrag * /*drag*/,
                                                      const int xy[2],
                                                      wmDropBox *drop)
{
  ARegion *region = CTX_wm_region(C);
  int mval[2] = {xy[0] - region->winrct.xmin, xy[1] - region->winrct.ymin};
  return ED_object_ot_drop_geometry_nodes_tooltip(C, drop->ptr, mval);
}

static void view3d_ob_drop_matrix_from_snap(V3DSnapCursorState *snap_state,
                                            Object *ob,
                                            float obmat_final[4][4])
{
  using namespace blender;
  V3DSnapCursorData *snap_data = ED_view3d_cursor_snap_data_get();
  BLI_assert(snap_state->draw_box || snap_state->draw_plane);
  UNUSED_VARS_NDEBUG(snap_state);
  copy_m4_m3(obmat_final, snap_data->plane_omat);
  copy_v3_v3(obmat_final[3], snap_data->loc);

  float scale[3];
  mat4_to_size(scale, ob->object_to_world().ptr());
  rescale_m4(obmat_final, scale);

  if (const std::optional<Bounds<float3>> bb = BKE_object_boundbox_get(ob)) {
    float3 offset = math::midpoint(bb->min, bb->max);
    offset[2] = bb->min[2];
    mul_mat3_m4_v3(obmat_final, offset);
    sub_v3_v3(obmat_final[3], offset);
  }
}

static void view3d_ob_drop_copy_local_id(bContext * /*C*/, wmDrag *drag, wmDropBox *drop)
{
  ID *id = WM_drag_get_local_ID(drag, ID_OB);

  RNA_int_set(drop->ptr, "session_uid", id->session_uid);
  /* Don't duplicate ID's which were just imported. Only do that for existing, local IDs. */
  BLI_assert(drag->type != WM_DRAG_ASSET);

  V3DSnapCursorState *snap_state = ED_view3d_cursor_snap_state_active_get();
  float obmat_final[4][4];

  view3d_ob_drop_matrix_from_snap(snap_state, (Object *)id, obmat_final);

  RNA_float_set_array(drop->ptr, "matrix", &obmat_final[0][0]);
}

/* Mostly the same logic as #view3d_collection_drop_copy_external_asset(), just different enough to
 * make sharing code a bit difficult. */
static void view3d_ob_drop_copy_external_asset(bContext *C, wmDrag *drag, wmDropBox *drop)
{
  /* NOTE(@ideasman42): Selection is handled here, de-selecting objects before append,
   * using auto-select to ensure the new objects are selected.
   * This is done so #OBJECT_OT_transform_to_mouse (which runs after this drop handler)
   * can use the context setup here to place the objects. */
  BLI_assert(drag->type == WM_DRAG_ASSET);

  wmDragAsset *asset_drag = WM_drag_get_asset_data(drag, 0);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);

  BKE_view_layer_base_deselect_all(scene, view_layer);

  ID *id = WM_drag_asset_id_import(C, asset_drag, FILE_AUTOSELECT);

  /* TODO(sergey): Only update relations for the current scene. */
  DEG_relations_tag_update(CTX_data_main(C));
  WM_event_add_notifier(C, NC_SCENE | ND_LAYER_CONTENT, scene);

  RNA_int_set(drop->ptr, "session_uid", id->session_uid);

  BKE_view_layer_synced_ensure(scene, view_layer);
  Base *base = BKE_view_layer_base_find(view_layer, (Object *)id);
  if (base != nullptr) {
    BKE_view_layer_base_select_and_set_active(view_layer, base);
    WM_main_add_notifier(NC_SCENE | ND_OB_ACTIVE, scene);
  }
  DEG_id_tag_update(&scene->id, ID_RECALC_SELECT);
  ED_outliner_select_sync_from_object_tag(C);

  /* Make sure the depsgraph is evaluated so the new object's transforms are up-to-date.
   * The evaluated #Object::object_to_world() will be copied back to the original object
   * and used below. */
  CTX_data_ensure_evaluated_depsgraph(C);

  V3DSnapCursorState *snap_state = static_cast<V3DSnapCursorState *>(drop->draw_data);
  if (snap_state) {
    float obmat_final[4][4];

    view3d_ob_drop_matrix_from_snap(snap_state, (Object *)id, obmat_final);

    RNA_float_set_array(drop->ptr, "matrix", &obmat_final[0][0]);
  }
}

static void view3d_collection_drop_copy_local_id(bContext * /*C*/, wmDrag *drag, wmDropBox *drop)
{
  ID *id = WM_drag_get_local_ID(drag, ID_GR);
  RNA_int_set(drop->ptr, "session_uid", int(id->session_uid));
}

/* Mostly the same logic as #view3d_ob_drop_copy_external_asset(), just different enough to make
 * sharing code a bit difficult. */
static void view3d_collection_drop_copy_external_asset(bContext *C, wmDrag *drag, wmDropBox *drop)
{
  BLI_assert(drag->type == WM_DRAG_ASSET);

  wmDragAsset *asset_drag = WM_drag_get_asset_data(drag, 0);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);

  BKE_view_layer_base_deselect_all(scene, view_layer);

  ID *id = WM_drag_asset_id_import(C, asset_drag, FILE_AUTOSELECT);
  Collection *collection = (Collection *)id;

  /* TODO(sergey): Only update relations for the current scene. */
  DEG_relations_tag_update(CTX_data_main(C));
  WM_event_add_notifier(C, NC_SCENE | ND_LAYER_CONTENT, scene);

  RNA_int_set(drop->ptr, "session_uid", int(id->session_uid));

  /* Make an object active, just use the first one in the collection. */
  CollectionObject *cobject = static_cast<CollectionObject *>(collection->gobject.first);
  BKE_view_layer_synced_ensure(scene, view_layer);
  Base *base = cobject ? BKE_view_layer_base_find(view_layer, cobject->ob) : nullptr;
  if (base) {
    BLI_assert((base->flag & BASE_SELECTABLE) && (base->flag & BASE_ENABLED_VIEWPORT));
    BKE_view_layer_base_select_and_set_active(view_layer, base);
    WM_main_add_notifier(NC_SCENE | ND_OB_ACTIVE, scene);
  }
  DEG_id_tag_update(&scene->id, ID_RECALC_SELECT);
  ED_outliner_select_sync_from_object_tag(C);

  /* XXX Without an undo push here, there will be a crash when the user modifies operator
   * properties. The stuff we do in these drop callbacks just isn't safe over undo/redo. */
  ED_undo_push(C, "Collection_Drop");
}

static void view3d_id_drop_copy(bContext *C, wmDrag *drag, wmDropBox *drop)
{
  ID *id = WM_drag_get_local_ID_or_import_from_asset(C, drag, 0);

  WM_operator_properties_id_lookup_set_from_id(drop->ptr, id);
  RNA_boolean_set(drop->ptr, "show_datablock_in_modifier", (drag->type != WM_DRAG_ASSET));
}

static void view3d_geometry_nodes_drop_copy(bContext *C, wmDrag *drag, wmDropBox *drop)
{
  view3d_id_drop_copy(C, drag, drop);
  RNA_boolean_set(drop->ptr, "show_datablock_in_modifier", (drag->type != WM_DRAG_ASSET));
}

static void view3d_id_drop_copy_with_type(bContext *C, wmDrag *drag, wmDropBox *drop)
{
  ID *id = WM_drag_get_local_ID_or_import_from_asset(C, drag, 0);

  RNA_enum_set(drop->ptr, "type", GS(id->name));
  WM_operator_properties_id_lookup_set_from_id(drop->ptr, id);
}

static void view3d_id_path_drop_copy(bContext *C, wmDrag *drag, wmDropBox *drop)
{
  ID *id = WM_drag_get_local_ID_or_import_from_asset(C, drag, 0);

  if (id) {
    WM_operator_properties_id_lookup_set_from_id(drop->ptr, id);
    RNA_struct_property_unset(drop->ptr, "filepath");
    return;
  }
  const char *path = WM_drag_get_single_path(drag);
  if (path) {
    RNA_string_set(drop->ptr, "filepath", path);
    RNA_struct_property_unset(drop->ptr, "image");
  }
}

static void view3d_lightcache_update(bContext *C)
{
  PointerRNA op_ptr;

  Scene *scene = CTX_data_scene(C);

  if (!BKE_scene_uses_blender_eevee(scene)) {
    /* Only do auto bake if eevee is the active engine */
    return;
  }

  wmOperatorType *ot = WM_operatortype_find("SCENE_OT_light_cache_bake", true);
  WM_operator_properties_create_ptr(&op_ptr, ot);
  RNA_int_set(&op_ptr, "delay", 200);
  RNA_enum_set_identifier(C, &op_ptr, "subset", "DIRTY");

  WM_operator_name_call_ptr(C, ot, WM_OP_INVOKE_DEFAULT, &op_ptr, nullptr);

  WM_operator_properties_free(&op_ptr);
}

/* region dropbox definition */
static void view3d_dropboxes()
{
  ListBase *lb = WM_dropboxmap_find("View3D", SPACE_VIEW3D, RGN_TYPE_WINDOW);

  wmDropBox *drop;
  drop = WM_dropbox_add(lb,
                        "OBJECT_OT_add_named",
                        view3d_ob_drop_poll_local_id,
                        view3d_ob_drop_copy_local_id,
                        WM_drag_free_imported_drag_ID,
                        nullptr);

  drop->draw_droptip = WM_drag_draw_item_name_fn;
  drop->on_enter = view3d_ob_drop_on_enter;
  drop->on_exit = view3d_ob_drop_on_exit;

  drop = WM_dropbox_add(lb,
                        "OBJECT_OT_transform_to_mouse",
                        view3d_ob_drop_poll_external_asset,
                        view3d_ob_drop_copy_external_asset,
                        WM_drag_free_imported_drag_ID,
                        nullptr);

  drop->draw_droptip = WM_drag_draw_item_name_fn;
  drop->on_enter = view3d_ob_drop_on_enter;
  drop->on_exit = view3d_ob_drop_on_exit;

  WM_dropbox_add(lb,
                 "OBJECT_OT_collection_external_asset_drop",
                 view3d_collection_drop_poll_external_asset,
                 view3d_collection_drop_copy_external_asset,
                 WM_drag_free_imported_drag_ID,
                 nullptr);
  WM_dropbox_add(lb,
                 "OBJECT_OT_collection_instance_add",
                 view3d_collection_drop_poll_local_id,
                 view3d_collection_drop_copy_local_id,
                 WM_drag_free_imported_drag_ID,
                 nullptr);

  WM_dropbox_add(lb,
                 "OBJECT_OT_drop_named_material",
                 view3d_mat_drop_poll,
                 view3d_id_drop_copy,
                 WM_drag_free_imported_drag_ID,
                 view3d_mat_drop_tooltip);
  WM_dropbox_add(lb,
                 "OBJECT_OT_drop_geometry_nodes",
                 view3d_geometry_nodes_drop_poll,
                 view3d_geometry_nodes_drop_copy,
                 WM_drag_free_imported_drag_ID,
                 view3d_geometry_nodes_drop_tooltip);
  WM_dropbox_add(lb,
                 "VIEW3D_OT_camera_background_image_add",
                 view3d_ima_bg_drop_poll,
                 view3d_id_path_drop_copy,
                 WM_drag_free_imported_drag_ID,
                 nullptr);
  WM_dropbox_add(lb,
                 "OBJECT_OT_empty_image_add",
                 view3d_ima_empty_drop_poll,
                 view3d_id_path_drop_copy,
                 WM_drag_free_imported_drag_ID,
                 nullptr);
  WM_dropbox_add(lb,
                 "OBJECT_OT_volume_import",
                 view3d_volume_drop_poll,
                 view3d_id_path_drop_copy,
                 WM_drag_free_imported_drag_ID,
                 nullptr);
  WM_dropbox_add(lb,
                 "OBJECT_OT_data_instance_add",
                 view3d_object_data_drop_poll,
                 view3d_id_drop_copy_with_type,
                 WM_drag_free_imported_drag_ID,
                 view3d_object_data_drop_tooltip);
  WM_dropbox_add(lb,
                 "VIEW3D_OT_drop_world",
                 view3d_world_drop_poll,
                 view3d_id_drop_copy,
                 WM_drag_free_imported_drag_ID,
                 nullptr);
}

static void view3d_widgets()
{
  wmGizmoMapType_Params params{SPACE_VIEW3D, RGN_TYPE_WINDOW};
  wmGizmoMapType *gzmap_type = WM_gizmomaptype_ensure(&params);

  WM_gizmogrouptype_append_and_link(gzmap_type, VIEW3D_GGT_xform_gizmo_context);
  WM_gizmogrouptype_append_and_link(gzmap_type, VIEW3D_GGT_light_spot);
  WM_gizmogrouptype_append_and_link(gzmap_type, VIEW3D_GGT_light_point);
  WM_gizmogrouptype_append_and_link(gzmap_type, VIEW3D_GGT_light_area);
  WM_gizmogrouptype_append_and_link(gzmap_type, VIEW3D_GGT_light_target);
  WM_gizmogrouptype_append_and_link(gzmap_type, VIEW3D_GGT_force_field);
  WM_gizmogrouptype_append_and_link(gzmap_type, VIEW3D_GGT_camera);
  WM_gizmogrouptype_append_and_link(gzmap_type, VIEW3D_GGT_camera_view);
  WM_gizmogrouptype_append_and_link(gzmap_type, VIEW3D_GGT_empty_image);
  /* TODO(@ideasman42): Not working well enough, disable for now. */
#if 0
  WM_gizmogrouptype_append_and_link(gzmap_type, VIEW3D_GGT_armature_spline);
#endif

  WM_gizmogrouptype_append(VIEW3D_GGT_xform_gizmo);
  WM_gizmogrouptype_append(VIEW3D_GGT_xform_cage);
  WM_gizmogrouptype_append(VIEW3D_GGT_xform_shear);
  WM_gizmogrouptype_append(VIEW3D_GGT_xform_extrude);
  WM_gizmogrouptype_append(VIEW3D_GGT_mesh_preselect_elem);
  WM_gizmogrouptype_append(VIEW3D_GGT_mesh_preselect_edgering);
  WM_gizmogrouptype_append(VIEW3D_GGT_tool_generic_handle_normal);
  WM_gizmogrouptype_append(VIEW3D_GGT_tool_generic_handle_free);

  WM_gizmogrouptype_append(VIEW3D_GGT_ruler);
  WM_gizmotype_append(VIEW3D_GT_ruler_item);

  WM_gizmogrouptype_append(VIEW3D_GGT_placement);

  WM_gizmogrouptype_append_and_link(gzmap_type, VIEW3D_GGT_navigate);
  WM_gizmotype_append(VIEW3D_GT_navigate_rotate);
}

/* type callback, not region itself */
static void view3d_main_region_free(ARegion *region)
{
  RegionView3D *rv3d = static_cast<RegionView3D *>(region->regiondata);

  if (rv3d) {
    if (rv3d->localvd) {
      MEM_freeN(rv3d->localvd);
    }
    if (rv3d->clipbb) {
      MEM_freeN(rv3d->clipbb);
    }

    if (rv3d->view_render) {
      DRW_engine_external_free(rv3d);
    }

    if (rv3d->sms) {
      MEM_freeN(rv3d->sms);
    }

    MEM_freeN(rv3d);
    region->regiondata = nullptr;
  }
}

/* copy regiondata */
static void *view3d_main_region_duplicate(void *poin)
{
  if (poin) {
    RegionView3D *rv3d = static_cast<RegionView3D *>(poin);
    RegionView3D *new_rv3d;

    new_rv3d = static_cast<RegionView3D *>(MEM_dupallocN(rv3d));
    if (rv3d->localvd) {
      new_rv3d->localvd = static_cast<RegionView3D *>(MEM_dupallocN(rv3d->localvd));
    }
    if (rv3d->clipbb) {
      new_rv3d->clipbb = static_cast<BoundBox *>(MEM_dupallocN(rv3d->clipbb));
    }

    new_rv3d->view_render = nullptr;
    new_rv3d->sms = nullptr;
    new_rv3d->smooth_timer = nullptr;

    return new_rv3d;
  }
  return nullptr;
}

static void view3d_main_region_listener(const wmRegionListenerParams *params)
{
  wmWindow *window = params->window;
  ScrArea *area = params->area;
  ARegion *region = params->region;
  const wmNotifier *wmn = params->notifier;
  const Scene *scene = params->scene;
  View3D *v3d = static_cast<View3D *>(area->spacedata.first);
  RegionView3D *rv3d = static_cast<RegionView3D *>(region->regiondata);
  wmGizmoMap *gzmap = region->gizmo_map;

  /* context changes */
  switch (wmn->category) {
    case NC_WM:
      if (ELEM(wmn->data, ND_UNDO)) {
        WM_gizmomap_tag_refresh(gzmap);
      }
      else if (ELEM(wmn->data, ND_XR_DATA_CHANGED)) {
        /* Only cause a redraw if this a VR session mirror. Should more features be added that
         * require redraws, we could pass something to wmn->reference, e.g. the flag value. */
        if (v3d->flag & V3D_XR_SESSION_MIRROR) {
          ED_region_tag_redraw(region);
        }
      }
      break;
    case NC_ANIMATION:
      switch (wmn->data) {
        case ND_KEYFRAME_PROP:
        case ND_NLA_ACTCHANGE:
          ED_region_tag_redraw(region);
          break;
        case ND_NLA:
        case ND_KEYFRAME:
          if (ELEM(wmn->action, NA_EDITED, NA_ADDED, NA_REMOVED)) {
            ED_region_tag_redraw(region);
          }
          break;
        case ND_ANIMCHAN:
          if (ELEM(wmn->action, NA_EDITED, NA_ADDED, NA_REMOVED, NA_SELECTED)) {
            ED_region_tag_redraw(region);
          }
          break;
      }
      break;
    case NC_SCENE:
      switch (wmn->data) {
        case ND_SCENEBROWSE:
        case ND_LAYER_CONTENT:
          ED_region_tag_redraw(region);
          WM_gizmomap_tag_refresh(gzmap);
          break;
        case ND_LAYER:
          if (wmn->reference) {
            BKE_screen_view3d_sync(v3d, static_cast<Scene *>(wmn->reference));
          }
          ED_region_tag_redraw(region);
          WM_gizmomap_tag_refresh(gzmap);
          break;
        case ND_OB_ACTIVE:
        case ND_OB_SELECT:
          [[fallthrough]];
        case ND_FRAME:
        case ND_TRANSFORM:
        case ND_OB_VISIBLE:
        case ND_RENDER_OPTIONS:
        case ND_MARKERS:
        case ND_MODE:
          ED_region_tag_redraw(region);
          WM_gizmomap_tag_refresh(gzmap);
          break;
        case ND_WORLD:
          /* handled by space_view3d_listener() for v3d access */
          break;
        case ND_DRAW_RENDER_VIEWPORT: {
          if (v3d->camera && (scene == wmn->reference)) {
            if (rv3d->persp == RV3D_CAMOB) {
              ED_region_tag_redraw(region);
            }
          }
          break;
        }
      }
      if (wmn->action == NA_EDITED) {
        ED_region_tag_redraw(region);
      }
      break;
    case NC_OBJECT:
      switch (wmn->data) {
        case ND_BONE_ACTIVE:
        case ND_BONE_SELECT:
        case ND_BONE_COLLECTION:
        case ND_TRANSFORM:
        case ND_POSE:
        case ND_DRAW:
        case ND_MODIFIER:
        case ND_SHADERFX:
        case ND_CONSTRAINT:
        case ND_KEYS:
        case ND_PARTICLE:
        case ND_POINTCACHE:
        case ND_LOD:
          ED_region_tag_redraw(region);
          WM_gizmomap_tag_refresh(gzmap);
          break;
        case ND_DRAW_ANIMVIZ:
          ED_region_tag_redraw(region);
          break;
      }
      switch (wmn->action) {
        case NA_ADDED:
          ED_region_tag_redraw(region);
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
          ED_region_tag_redraw(region);
          WM_gizmomap_tag_refresh(gzmap);
          break;
        case ND_VERTEX_GROUP:
          ED_region_tag_redraw(region);
          break;
      }
      switch (wmn->action) {
        case NA_EDITED:
          ED_region_tag_redraw(region);
          break;
      }
      break;
    case NC_CAMERA:
      switch (wmn->data) {
        case ND_DRAW_RENDER_VIEWPORT: {
          if (v3d->camera && (v3d->camera->data == wmn->reference)) {
            if (rv3d->persp == RV3D_CAMOB) {
              ED_region_tag_redraw(region);
            }
          }
          break;
        }
      }
      break;
    case NC_GROUP:
      /* all group ops for now */
      ED_region_tag_redraw(region);
      break;
    case NC_BRUSH:
      switch (wmn->action) {
        case NA_EDITED:
          ED_region_tag_redraw_cursor(region);
          break;
        case NA_SELECTED:
          /* used on brush changes - needed because 3d cursor
           * has to be drawn if clone brush is selected */
          ED_region_tag_redraw(region);
          break;
      }
      break;
    case NC_MATERIAL:
      switch (wmn->data) {
        case ND_SHADING:
        case ND_NODES:
          /* TODO(sergey): This is a bit too much updates, but needed to
           * have proper material drivers update in the viewport.
           *
           * How to solve?
           */
          ED_region_tag_redraw(region);
          break;
        case ND_SHADING_DRAW:
        case ND_SHADING_LINKS:
          ED_region_tag_redraw(region);
          break;
      }
      break;
    case NC_NODE:
      ED_region_tag_redraw(region);
      break;
    case NC_WORLD:
      switch (wmn->data) {
        case ND_WORLD_DRAW:
          /* handled by space_view3d_listener() for v3d access */
          break;
        case ND_WORLD:
          /* Needed for updating world materials */
          ED_region_tag_redraw(region);
          break;
      }
      break;
    case NC_LAMP:
      switch (wmn->data) {
        case ND_LIGHTING:
          /* TODO(sergey): This is a bit too much, but needed to
           * handle updates from new depsgraph.
           */
          ED_region_tag_redraw(region);
          break;
        case ND_LIGHTING_DRAW:
          ED_region_tag_redraw(region);
          WM_gizmomap_tag_refresh(gzmap);
          break;
      }
      break;
    case NC_LIGHTPROBE:
      ED_area_tag_refresh(area);
      break;
    case NC_IMAGE:
      /* this could be more fine grained checks if we had
       * more context than just the region */
      ED_region_tag_redraw(region);
      break;
    case NC_TEXTURE:
      /* same as above */
      ED_region_tag_redraw(region);
      break;
    case NC_MOVIECLIP:
      if (wmn->data == ND_DISPLAY || wmn->action == NA_EDITED) {
        ED_region_tag_redraw(region);
      }
      break;
    case NC_SPACE:
      if (wmn->data == ND_SPACE_VIEW3D) {
        if (wmn->subtype == NS_VIEW3D_GPU) {
          rv3d->rflag |= RV3D_GPULIGHT_UPDATE;
        }
        else if (wmn->subtype == NS_VIEW3D_SHADING) {
#ifdef WITH_XR_OPENXR
          ED_view3d_xr_shading_update(
              static_cast<wmWindowManager *>(G_MAIN->wm.first), v3d, scene);
#endif

          ViewLayer *view_layer = WM_window_get_active_view_layer(window);
          Depsgraph *depsgraph = BKE_scene_get_depsgraph(scene, view_layer);
          if (depsgraph) {
            ED_render_view3d_update(depsgraph, window, area, true);
          }
        }
        ED_region_tag_redraw(region);
        WM_gizmomap_tag_refresh(gzmap);
      }
      break;
    case NC_ID:
      if (ELEM(wmn->action, NA_RENAME, NA_EDITED, NA_ADDED, NA_REMOVED)) {
        ED_region_tag_redraw(region);
      }
      break;
    case NC_SCREEN:
      switch (wmn->data) {
        case ND_ANIMPLAY:
        case ND_SKETCH:
          ED_region_tag_redraw(region);
          break;
        case ND_LAYOUTBROWSE:
        case ND_LAYOUTDELETE:
        case ND_LAYOUTSET:
          WM_gizmomap_tag_refresh(gzmap);
          ED_region_tag_redraw(region);
          break;
        case ND_LAYER:
          ED_region_tag_redraw(region);
          break;
      }

      break;
    case NC_GPENCIL:
      if (wmn->data == ND_DATA || ELEM(wmn->action, NA_EDITED, NA_SELECTED)) {
        ED_region_tag_redraw(region);
      }
      break;
    case NC_WORKSPACE:
      /* In case the region displays workspace settings. */
      ED_region_tag_redraw(region);
      break;
    case NC_VIEWER_PATH: {
      if (v3d->flag2 & V3D_SHOW_VIEWER) {
        ViewLayer *view_layer = WM_window_get_active_view_layer(window);
        if (Depsgraph *depsgraph = BKE_scene_get_depsgraph(scene, view_layer)) {
          ED_render_view3d_update(depsgraph, window, area, true);
        }
        ED_region_tag_redraw(region);
      }
      break;
    }
  }
}

static void view3d_do_msg_notify_workbench_view_update(bContext *C,
                                                       wmMsgSubscribeKey * /*msg_key*/,
                                                       wmMsgSubscribeValue *msg_val)
{
  Scene *scene = CTX_data_scene(C);
  ScrArea *area = (ScrArea *)msg_val->user_data;
  View3D *v3d = (View3D *)area->spacedata.first;
  if (v3d->shading.type == OB_SOLID) {
    RenderEngineType *engine_type = ED_view3d_engine_type(scene, v3d->shading.type);
    DRWUpdateContext drw_context = {nullptr};
    drw_context.bmain = CTX_data_main(C);
    drw_context.depsgraph = CTX_data_depsgraph_pointer(C);
    drw_context.scene = scene;
    drw_context.view_layer = CTX_data_view_layer(C);
    drw_context.region = (ARegion *)(msg_val->owner);
    drw_context.v3d = v3d;
    drw_context.engine_type = engine_type;
    DRW_notify_view_update(&drw_context);
  }
}

static void view3d_main_region_message_subscribe(const wmRegionMessageSubscribeParams *params)
{
  wmMsgBus *mbus = params->message_bus;
  const bContext *C = params->context;
  ScrArea *area = params->area;
  ARegion *region = params->region;

  /* Developer NOTE: there are many properties that impact 3D view drawing,
   * so instead of subscribing to individual properties, just subscribe to types
   * accepting some redundant redraws.
   *
   * For other space types we might try avoid this, keep the 3D view as an exceptional case! */
  wmMsgParams_RNA msg_key_params{};

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

      &RNA_View3DCursor,
      &RNA_View3DOverlay,
      &RNA_View3DShading,
      &RNA_World,
  };

  wmMsgSubscribeValue msg_sub_value_region_tag_redraw{};
  msg_sub_value_region_tag_redraw.owner = region;
  msg_sub_value_region_tag_redraw.user_data = region;
  msg_sub_value_region_tag_redraw.notify = ED_region_do_msg_notify_tag_redraw;

  wmMsgSubscribeValue msg_sub_value_workbench_view_update{};
  msg_sub_value_workbench_view_update.owner = region;
  msg_sub_value_workbench_view_update.user_data = area;
  msg_sub_value_workbench_view_update.notify = view3d_do_msg_notify_workbench_view_update;

  for (int i = 0; i < ARRAY_SIZE(type_array); i++) {
    msg_key_params.ptr.type = type_array[i];
    WM_msg_subscribe_rna_params(mbus, &msg_key_params, &msg_sub_value_region_tag_redraw, __func__);
  }

  /* Subscribe to a handful of other properties. */
  RegionView3D *rv3d = static_cast<RegionView3D *>(region->regiondata);

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

  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  BKE_view_layer_synced_ensure(scene, view_layer);
  Object *obact = BKE_view_layer_active_object_get(view_layer);
  if (obact != nullptr) {
    switch (obact->mode) {
      case OB_MODE_PARTICLE_EDIT:
        WM_msg_subscribe_rna_anon_type(mbus, ParticleEdit, &msg_sub_value_region_tag_redraw);
        break;

      case OB_MODE_SCULPT:
        WM_msg_subscribe_rna_anon_prop(
            mbus, WorkSpace, tools, &msg_sub_value_workbench_view_update);
        break;
      default:
        break;
    }
  }

  {
    wmMsgSubscribeValue msg_sub_value_region_tag_refresh{};
    msg_sub_value_region_tag_refresh.owner = region;
    msg_sub_value_region_tag_refresh.user_data = area;
    msg_sub_value_region_tag_refresh.notify = WM_toolsystem_do_msg_notify_tag_refresh;
    WM_msg_subscribe_rna_anon_prop(mbus, Object, mode, &msg_sub_value_region_tag_refresh);
    WM_msg_subscribe_rna_anon_prop(mbus, LayerObjects, active, &msg_sub_value_region_tag_refresh);
  }
}

/* concept is to retrieve cursor type context-less */
static void view3d_main_region_cursor(wmWindow *win, ScrArea *area, ARegion *region)
{
  if (WM_cursor_set_from_tool(win, area, region)) {
    return;
  }

  Scene *scene = WM_window_get_active_scene(win);
  ViewLayer *view_layer = WM_window_get_active_view_layer(win);
  BKE_view_layer_synced_ensure(scene, view_layer);
  Object *obedit = BKE_view_layer_edit_object_get(view_layer);
  if (obedit) {
    WM_cursor_set(win, WM_CURSOR_EDIT);
  }
  else {
    WM_cursor_set(win, WM_CURSOR_DEFAULT);
  }
}

/* add handlers, stuff you only do once or on area/region changes */
static void view3d_header_region_init(wmWindowManager *wm, ARegion *region)
{
  wmKeyMap *keymap = WM_keymap_ensure(
      wm->defaultconf, "3D View Generic", SPACE_VIEW3D, RGN_TYPE_WINDOW);

  WM_event_add_keymap_handler(&region->handlers, keymap);

  ED_region_header_init(region);
}

static void view3d_header_region_draw(const bContext *C, ARegion *region)
{
  ED_region_header(C, region);
}

static void view3d_header_region_listener(const wmRegionListenerParams *params)
{
  ARegion *region = params->region;
  const wmNotifier *wmn = params->notifier;

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
          ED_region_tag_redraw(region);
          break;
      }
      break;
    case NC_SPACE:
      switch (wmn->data) {
        case ND_SPACE_VIEW3D:
          ED_region_tag_redraw(region);
          break;
        case ND_SPACE_ASSET_PARAMS:
          blender::ed::geometry::clear_operator_asset_trees();
          ED_region_tag_redraw(region);
          break;
      }
      break;
    case NC_ASSET:
      switch (wmn->data) {
        case ND_ASSET_CATALOGS:
        case ND_ASSET_LIST_READING:
          blender::ed::geometry::clear_operator_asset_trees();
          ED_region_tag_redraw(region);
          break;
        default:
          if (ELEM(wmn->action, NA_ADDED, NA_REMOVED)) {
            blender::ed::geometry::clear_operator_asset_trees();
            ED_region_tag_redraw(region);
          }
      }
      break;
    case NC_NODE:
      switch (wmn->data) {
        case ND_NODE_ASSET_DATA:
          blender::ed::geometry::clear_operator_asset_trees();
          ED_region_tag_redraw(region);
          break;
      }
      break;
    case NC_GPENCIL:
      if (wmn->data & ND_GPENCIL_EDITMODE) {
        ED_region_tag_redraw(region);
      }
      else if (wmn->action == NA_EDITED) {
        ED_region_tag_redraw(region);
      }
      break;
    case NC_BRUSH:
      ED_region_tag_redraw(region);
      break;
    case NC_GEOM:
      if (wmn->data == ND_VERTEX_GROUP || wmn->data == ND_DATA) {
        ED_region_tag_redraw(region);
      }
      break;
    case NC_MATERIAL:
      /* For the canvas picker. */
      if (wmn->data == ND_SHADING_LINKS) {
        ED_region_tag_redraw(region);
      }
      break;
  }

    /* From top-bar, which ones are needed? split per header? */
    /* Disable for now, re-enable if needed, or remove - campbell. */
#if 0
  /* context changes */
  switch (wmn->category) {
    case NC_WM:
      if (wmn->data == ND_HISTORY) {
        ED_region_tag_redraw(region);
      }
      break;
    case NC_SCENE:
      if (wmn->data == ND_MODE) {
        ED_region_tag_redraw(region);
      }
      break;
    case NC_SPACE:
      if (wmn->data == ND_SPACE_VIEW3D) {
        ED_region_tag_redraw(region);
      }
      break;
    case NC_GPENCIL:
      if (wmn->data == ND_DATA) {
        ED_region_tag_redraw(region);
      }
      break;
  }
#endif
}

static void view3d_header_region_message_subscribe(const wmRegionMessageSubscribeParams *params)
{
  wmMsgBus *mbus = params->message_bus;
  ARegion *region = params->region;

  wmMsgParams_RNA msg_key_params{};

  /* Only subscribe to types. */
  StructRNA *type_array[] = {
      &RNA_View3DShading,
  };

  wmMsgSubscribeValue msg_sub_value_region_tag_redraw{};
  msg_sub_value_region_tag_redraw.owner = region;
  msg_sub_value_region_tag_redraw.user_data = region;
  msg_sub_value_region_tag_redraw.notify = ED_region_do_msg_notify_tag_redraw;

  for (int i = 0; i < ARRAY_SIZE(type_array); i++) {
    msg_key_params.ptr.type = type_array[i];
    WM_msg_subscribe_rna_params(mbus, &msg_key_params, &msg_sub_value_region_tag_redraw, __func__);
  }
}

/* add handlers, stuff you only do once or on area/region changes */
static void view3d_buttons_region_init(wmWindowManager *wm, ARegion *region)
{
  wmKeyMap *keymap;

  ED_region_panels_init(wm, region);

  keymap = WM_keymap_ensure(wm->defaultconf, "3D View Generic", SPACE_VIEW3D, RGN_TYPE_WINDOW);
  WM_event_add_keymap_handler(&region->handlers, keymap);
}

void ED_view3d_buttons_region_layout_ex(const bContext *C,
                                        ARegion *region,
                                        const char *category_override)
{
  const enum eContextObjectMode mode = CTX_data_mode_enum(C);

  const char *contexts_base[4] = {nullptr};
  contexts_base[0] = CTX_data_mode_string(C);

  const char **contexts = &contexts_base[1];

  switch (mode) {
    case CTX_MODE_EDIT_MESH:
      ARRAY_SET_ITEMS(contexts, ".mesh_edit");
      break;
    case CTX_MODE_EDIT_CURVE:
      ARRAY_SET_ITEMS(contexts, ".curve_edit");
      break;
    case CTX_MODE_EDIT_CURVES:
      ARRAY_SET_ITEMS(contexts, ".curves_edit");
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
    case CTX_MODE_EDIT_GREASE_PENCIL:
      ARRAY_SET_ITEMS(contexts, ".grease_pencil_edit");
      break;
    case CTX_MODE_PAINT_GREASE_PENCIL:
      ARRAY_SET_ITEMS(contexts, ".grease_pencil_paint");
      break;
    case CTX_MODE_EDIT_POINT_CLOUD:
      ARRAY_SET_ITEMS(contexts, ".point_cloud_edit");
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
    case CTX_MODE_PAINT_GPENCIL_LEGACY:
      ARRAY_SET_ITEMS(contexts, ".greasepencil_paint");
      break;
    case CTX_MODE_SCULPT_GPENCIL_LEGACY:
      ARRAY_SET_ITEMS(contexts, ".greasepencil_sculpt");
      break;
    case CTX_MODE_WEIGHT_GPENCIL_LEGACY:
      ARRAY_SET_ITEMS(contexts, ".greasepencil_weight");
      break;
    case CTX_MODE_VERTEX_GPENCIL_LEGACY:
      ARRAY_SET_ITEMS(contexts, ".greasepencil_vertex");
      break;
    case CTX_MODE_SCULPT_CURVES:
      ARRAY_SET_ITEMS(contexts, ".paint_common", ".curves_sculpt");
      break;
    default:
      break;
  }

  switch (mode) {
    case CTX_MODE_PAINT_GPENCIL_LEGACY:
      ARRAY_SET_ITEMS(contexts, ".greasepencil_paint");
      break;
    case CTX_MODE_SCULPT_GPENCIL_LEGACY:
      ARRAY_SET_ITEMS(contexts, ".greasepencil_sculpt");
      break;
    case CTX_MODE_WEIGHT_GPENCIL_LEGACY:
      ARRAY_SET_ITEMS(contexts, ".greasepencil_weight");
      break;
    case CTX_MODE_EDIT_GPENCIL_LEGACY:
      ARRAY_SET_ITEMS(contexts, ".greasepencil_edit");
      break;
    case CTX_MODE_VERTEX_GPENCIL_LEGACY:
      ARRAY_SET_ITEMS(contexts, ".greasepencil_vertex");
      break;
    default:
      break;
  }

  ListBase *paneltypes = &region->type->paneltypes;

  /* Allow drawing 3D view toolbar from non 3D view space type. */
  if (category_override != nullptr) {
    SpaceType *st = BKE_spacetype_from_id(SPACE_VIEW3D);
    ARegionType *art = BKE_regiontype_from_id(st, RGN_TYPE_UI);
    paneltypes = &art->paneltypes;
  }

  ED_region_panels_layout_ex(
      C, region, paneltypes, WM_OP_INVOKE_REGION_WIN, contexts_base, category_override);
}

static void view3d_buttons_region_layout(const bContext *C, ARegion *region)
{
  ED_view3d_buttons_region_layout_ex(C, region, nullptr);
}

static void view3d_buttons_region_listener(const wmRegionListenerParams *params)
{
  ARegion *region = params->region;
  const wmNotifier *wmn = params->notifier;

  /* context changes */
  switch (wmn->category) {
    case NC_ANIMATION:
      switch (wmn->data) {
        case ND_KEYFRAME_PROP:
        case ND_NLA_ACTCHANGE:
          ED_region_tag_redraw(region);
          break;
        case ND_NLA:
        case ND_KEYFRAME:
          if (ELEM(wmn->action, NA_EDITED, NA_ADDED, NA_REMOVED)) {
            ED_region_tag_redraw(region);
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
          ED_region_tag_redraw(region);
          break;
      }
      switch (wmn->action) {
        case NA_EDITED:
          ED_region_tag_redraw(region);
          break;
      }
      break;
    case NC_OBJECT:
      switch (wmn->data) {
        case ND_BONE_ACTIVE:
        case ND_BONE_SELECT:
        case ND_BONE_COLLECTION:
        case ND_TRANSFORM:
        case ND_POSE:
        case ND_DRAW:
        case ND_KEYS:
        case ND_MODIFIER:
        case ND_SHADERFX:
          ED_region_tag_redraw(region);
          break;
      }
      break;
    case NC_GEOM:
      switch (wmn->data) {
        case ND_DATA:
        case ND_VERTEX_GROUP:
        case ND_SELECT:
          ED_region_tag_redraw(region);
          break;
      }
      if (wmn->action == NA_EDITED) {
        ED_region_tag_redraw(region);
      }
      break;
    case NC_TEXTURE:
    case NC_MATERIAL:
      /* for brush textures */
      ED_region_tag_redraw(region);
      break;
    case NC_BRUSH:
      /* NA_SELECTED is used on brush changes */
      if (ELEM(wmn->action, NA_EDITED, NA_SELECTED)) {
        ED_region_tag_redraw(region);
      }
      break;
    case NC_SPACE:
      if (wmn->data == ND_SPACE_VIEW3D) {
        ED_region_tag_redraw(region);
      }
      break;
    case NC_ID:
      if (wmn->action == NA_RENAME) {
        ED_region_tag_redraw(region);
      }
      break;
    case NC_GPENCIL:
      if ((wmn->data & (ND_DATA | ND_GPENCIL_EDITMODE)) || (wmn->action == NA_EDITED)) {
        ED_region_tag_redraw(region);
      }
      break;
    case NC_IMAGE:
      /* Update for the image layers in texture paint. */
      if (wmn->action == NA_EDITED) {
        ED_region_tag_redraw(region);
      }
      break;
    case NC_WM:
      if (wmn->data == ND_XR_DATA_CHANGED) {
        ED_region_tag_redraw(region);
      }
      break;
  }
}

/* add handlers, stuff you only do once or on area/region changes */
static void view3d_tools_region_init(wmWindowManager *wm, ARegion *region)
{
  wmKeyMap *keymap;

  ED_region_panels_init(wm, region);

  keymap = WM_keymap_ensure(wm->defaultconf, "3D View Generic", SPACE_VIEW3D, RGN_TYPE_WINDOW);
  WM_event_add_keymap_handler(&region->handlers, keymap);
}

static void view3d_tools_region_draw(const bContext *C, ARegion *region)
{
  const char *contexts[] = {CTX_data_mode_string(C), nullptr};
  ED_region_panels_ex(C, region, WM_OP_INVOKE_REGION_WIN, contexts);
}

static void view3d_tools_header_region_draw(const bContext *C, ARegion *region)
{
  ED_region_header_with_button_sections(
      C,
      region,
      (RGN_ALIGN_ENUM_FROM_MASK(region->alignment) == RGN_ALIGN_TOP) ?
          uiButtonSectionsAlign::Top :
          uiButtonSectionsAlign::Bottom);
}

/* add handlers, stuff you only do once or on area/region changes */
static void view3d_asset_shelf_region_init(wmWindowManager *wm, ARegion *region)
{
  using namespace blender::ed;
  wmKeyMap *keymap = WM_keymap_ensure(
      wm->defaultconf, "3D View Generic", SPACE_VIEW3D, RGN_TYPE_WINDOW);
  WM_event_add_keymap_handler(&region->handlers, keymap);

  asset::shelf::region_init(wm, region);
}

/* area (not region) level listener */
static void space_view3d_listener(const wmSpaceTypeListenerParams *params)
{
  ScrArea *area = params->area;
  const wmNotifier *wmn = params->notifier;
  View3D *v3d = static_cast<View3D *>(area->spacedata.first);

  /* context changes */
  switch (wmn->category) {
    case NC_SCENE:
      switch (wmn->data) {
        case ND_WORLD: {
          const bool use_scene_world = V3D_USES_SCENE_WORLD(v3d);
          if (v3d->flag2 & V3D_HIDE_OVERLAYS || use_scene_world) {
            ED_area_tag_redraw_regiontype(area, RGN_TYPE_WINDOW);
          }
          break;
        }
      }
      break;
    case NC_WORLD:
      switch (wmn->data) {
        case ND_WORLD_DRAW:
        case ND_WORLD:
          if (v3d->shading.background_type == V3D_SHADING_BACKGROUND_WORLD) {
            ED_area_tag_redraw_regiontype(area, RGN_TYPE_WINDOW);
          }
          break;
      }
      break;
    case NC_MATERIAL:
      switch (wmn->data) {
        case ND_NODES:
          if (v3d->shading.type == OB_TEXTURE) {
            ED_area_tag_redraw_regiontype(area, RGN_TYPE_WINDOW);
          }
          break;
      }
      break;
  }
}

static void space_view3d_refresh(const bContext *C, ScrArea *area)
{
  Scene *scene = CTX_data_scene(C);
  LightCache *lcache = scene->eevee.light_cache_data;

  if (lcache && (lcache->flag & LIGHTCACHE_UPDATE_AUTO) != 0) {
    lcache->flag &= ~LIGHTCACHE_UPDATE_AUTO;
    view3d_lightcache_update((bContext *)C);
  }

  View3D *v3d = (View3D *)area->spacedata.first;
  MEM_SAFE_FREE(v3d->runtime.local_stats);
}

static void view3d_id_remap_v3d_ob_centers(View3D *v3d,
                                           const blender::bke::id::IDRemapper &mappings)
{
  if (mappings.apply(reinterpret_cast<ID **>(&v3d->ob_center), ID_REMAP_APPLY_DEFAULT) ==
      ID_REMAP_RESULT_SOURCE_UNASSIGNED)
  {
    /* Otherwise, bone-name may remain valid...
     * We could be smart and check this, too? */
    v3d->ob_center_bone[0] = '\0';
  }
}

static void view3d_id_remap_v3d(ScrArea *area,
                                SpaceLink *slink,
                                View3D *v3d,
                                const blender::bke::id::IDRemapper &mappings,
                                const bool is_local)
{
  if (mappings.apply(reinterpret_cast<ID **>(&v3d->camera), ID_REMAP_APPLY_DEFAULT) ==
      ID_REMAP_RESULT_SOURCE_UNASSIGNED)
  {
    /* 3D view might be inactive, in that case needs to use slink->regionbase */
    ListBase *regionbase = (slink == area->spacedata.first) ? &area->regionbase :
                                                              &slink->regionbase;
    LISTBASE_FOREACH (ARegion *, region, regionbase) {
      if (region->regiontype == RGN_TYPE_WINDOW) {
        RegionView3D *rv3d = is_local ? ((RegionView3D *)region->regiondata)->localvd :
                                        static_cast<RegionView3D *>(region->regiondata);
        if (rv3d && (rv3d->persp == RV3D_CAMOB)) {
          rv3d->persp = RV3D_PERSP;
        }
      }
    }
  }
}

static void view3d_id_remap(ScrArea *area,
                            SpaceLink *slink,
                            const blender::bke::id::IDRemapper &mappings)
{
  if (!mappings.contains_mappings_for_any(FILTER_ID_OB | FILTER_ID_MA | FILTER_ID_IM |
                                          FILTER_ID_MC))
  {
    return;
  }

  View3D *view3d = (View3D *)slink;
  view3d_id_remap_v3d(area, slink, view3d, mappings, false);
  view3d_id_remap_v3d_ob_centers(view3d, mappings);
  if (view3d->localvd != nullptr) {
    /* Object centers in local-view aren't used, see: #52663 */
    view3d_id_remap_v3d(area, slink, view3d->localvd, mappings, true);
  }
  BKE_viewer_path_id_remap(&view3d->viewer_path, mappings);
}

static void view3d_foreach_id(SpaceLink *space_link, LibraryForeachIDData *data)
{
  View3D *v3d = reinterpret_cast<View3D *>(space_link);

  BKE_LIB_FOREACHID_PROCESS_IDSUPER(data, v3d->camera, IDWALK_CB_NOP);
  BKE_LIB_FOREACHID_PROCESS_IDSUPER(data, v3d->ob_center, IDWALK_CB_NOP);
  if (v3d->localvd) {
    BKE_LIB_FOREACHID_PROCESS_IDSUPER(data, v3d->localvd->camera, IDWALK_CB_NOP);
  }
  BKE_viewer_path_foreach_id(data, &v3d->viewer_path);
}

static void view3d_space_blend_read_data(BlendDataReader *reader, SpaceLink *sl)
{
  View3D *v3d = (View3D *)sl;

  memset(&v3d->runtime, 0x0, sizeof(v3d->runtime));

  if (v3d->gpd) {
    BLO_read_data_address(reader, &v3d->gpd);
    BKE_gpencil_blend_read_data(reader, v3d->gpd);
  }
  BLO_read_data_address(reader, &v3d->localvd);

  /* render can be quite heavy, set to solid on load */
  if (v3d->shading.type == OB_RENDER) {
    v3d->shading.type = OB_SOLID;
  }
  v3d->shading.prev_type = OB_SOLID;

  BKE_screen_view3d_shading_blend_read_data(reader, &v3d->shading);

  BKE_screen_view3d_do_versions_250(v3d, &sl->regionbase);

  BKE_viewer_path_blend_read_data(reader, &v3d->viewer_path);
}

static void view3d_space_blend_write(BlendWriter *writer, SpaceLink *sl)
{
  View3D *v3d = (View3D *)sl;
  BLO_write_struct(writer, View3D, v3d);

  if (v3d->localvd) {
    BLO_write_struct(writer, View3D, v3d->localvd);
  }

  BKE_screen_view3d_shading_blend_write(writer, &v3d->shading);

  BKE_viewer_path_blend_write(writer, &v3d->viewer_path);
}

void ED_spacetype_view3d()
{
  using namespace blender::ed;
  std::unique_ptr<SpaceType> st = std::make_unique<SpaceType>();
  ARegionType *art;

  st->spaceid = SPACE_VIEW3D;
  STRNCPY(st->name, "View3D");

  st->create = view3d_create;
  st->free = view3d_free;
  st->init = view3d_init;
  st->exit = view3d_exit;
  st->listener = space_view3d_listener;
  st->refresh = space_view3d_refresh;
  st->duplicate = view3d_duplicate;
  st->operatortypes = view3d_operatortypes;
  st->keymap = view3d_keymap;
  st->dropboxes = view3d_dropboxes;
  st->gizmos = view3d_widgets;
  st->context = view3d_context;
  st->id_remap = view3d_id_remap;
  st->foreach_id = view3d_foreach_id;
  st->blend_read_data = view3d_space_blend_read_data;
  st->blend_read_after_liblink = nullptr;
  st->blend_write = view3d_space_blend_write;

  /* regions: main window */
  art = MEM_cnew<ARegionType>("spacetype view3d main region");
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

  /* regions: list-view/buttons */
  art = MEM_cnew<ARegionType>("spacetype view3d buttons region");
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
  art = MEM_cnew<ARegionType>("spacetype view3d tools region");
  art->regionid = RGN_TYPE_TOOLS;
  art->prefsizex = int(UI_TOOLBAR_WIDTH);
  art->prefsizey = 50; /* XXX */
  art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_FRAMES;
  art->listener = view3d_buttons_region_listener;
  art->message_subscribe = ED_region_generic_tools_region_message_subscribe;
  art->snap_size = ED_region_generic_tools_region_snap_size;
  art->init = view3d_tools_region_init;
  art->draw = view3d_tools_region_draw;
  BLI_addhead(&st->regiontypes, art);

  /* regions: tool header */
  art = MEM_cnew<ARegionType>("spacetype view3d tool header region");
  art->regionid = RGN_TYPE_TOOL_HEADER;
  art->prefsizey = HEADERY;
  art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_VIEW2D | ED_KEYMAP_FRAMES | ED_KEYMAP_HEADER;
  art->listener = view3d_header_region_listener;
  art->message_subscribe = ED_area_do_mgs_subscribe_for_tool_header;
  art->init = view3d_header_region_init;
  art->draw = view3d_tools_header_region_draw;
  BLI_addhead(&st->regiontypes, art);

  /* regions: header */
  art = MEM_cnew<ARegionType>("spacetype view3d header region");
  art->regionid = RGN_TYPE_HEADER;
  art->prefsizey = HEADERY;
  art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_VIEW2D | ED_KEYMAP_FRAMES | ED_KEYMAP_HEADER;
  art->listener = view3d_header_region_listener;
  art->message_subscribe = view3d_header_region_message_subscribe;
  art->init = view3d_header_region_init;
  art->draw = view3d_header_region_draw;
  BLI_addhead(&st->regiontypes, art);

  /* regions: asset shelf */
  art = MEM_cnew<ARegionType>("spacetype view3d asset shelf region");
  art->regionid = RGN_TYPE_ASSET_SHELF;
  art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_ASSET_SHELF | ED_KEYMAP_FRAMES;
  art->duplicate = asset::shelf::region_duplicate;
  art->free = asset::shelf::region_free;
  art->listener = asset::shelf::region_listen;
  art->poll = asset::shelf::regions_poll;
  art->snap_size = asset::shelf::region_snap;
  art->on_user_resize = asset::shelf::region_on_user_resize;
  art->context = asset::shelf::context;
  art->init = view3d_asset_shelf_region_init;
  art->layout = asset::shelf::region_layout;
  art->draw = asset::shelf::region_draw;
  BLI_addhead(&st->regiontypes, art);

  /* regions: asset shelf header */
  art = MEM_cnew<ARegionType>("spacetype view3d asset shelf header region");
  art->regionid = RGN_TYPE_ASSET_SHELF_HEADER;
  art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_ASSET_SHELF | ED_KEYMAP_VIEW2D | ED_KEYMAP_FOOTER;
  art->init = asset::shelf::header_region_init;
  art->poll = asset::shelf::regions_poll;
  art->draw = asset::shelf::header_region;
  art->listener = asset::shelf::header_region_listen;
  art->context = asset::shelf::context;
  BLI_addhead(&st->regiontypes, art);
  asset::shelf::header_regiontype_register(art, SPACE_VIEW3D);

  /* regions: hud */
  art = ED_area_type_hud(st->spaceid);
  BLI_addhead(&st->regiontypes, art);

  /* regions: xr */
  art = MEM_cnew<ARegionType>("spacetype view3d xr region");
  art->regionid = RGN_TYPE_XR;
  BLI_addhead(&st->regiontypes, art);

  WM_menutype_add(
      MEM_new<MenuType>(__func__, blender::ed::geometry::node_group_operator_assets_menu()));
  WM_menutype_add(MEM_new<MenuType>(
      __func__, blender::ed::geometry::node_group_operator_assets_menu_unassigned()));

  BKE_spacetype_register(std::move(st));
}
