/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spview3d
 */

#include "AS_asset_representation.hh"

#include "BKE_asset.hh"
#include "BKE_context.hh"
#include "BKE_idprop.hh"
#include "BKE_layer.hh"
#include "BKE_lib_id.hh"
#include "BKE_library.hh"
#include "BKE_object.hh"

#include "BLI_math_matrix.h"
#include "BLI_math_rotation.h"
#include "BLI_math_vector.h"

#include "BLT_translation.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_build.hh"

#include "DNA_ID.h"
#include "DNA_collection_types.h"
#include "DNA_screen_types.h"

#include "ED_object.hh"
#include "ED_outliner.hh"
#include "ED_screen.hh"
#include "ED_undo.hh"
#include "ED_view3d.hh"

#include "UI_resources.hh"

#include "RNA_access.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "view3d_intern.hh" /* own include */

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

static V3DSnapCursorState *view3d_drop_snap_init(wmDropBox *drop)
{
  V3DSnapCursorState *state = static_cast<V3DSnapCursorState *>(drop->draw_data);
  if (state) {
    return state;
  }

  state = ED_view3d_cursor_snap_state_create();
  drop->draw_data = state;
  state->draw_plane = true;
  return state;
}

static void view3d_drop_snap_exit(wmDropBox *drop, wmDrag * /*drag*/)
{
  V3DSnapCursorState *state = static_cast<V3DSnapCursorState *>(drop->draw_data);
  if (state) {
    ED_view3d_cursor_snap_state_free(state);
    drop->draw_data = nullptr;
  }
}

static void view3d_ob_drop_on_enter(wmDropBox *drop, wmDrag *drag)
{
  /* Don't use the snap cursor when linking the object. Object transform isn't editable then and
   * would be reset on reload. */
  if (WM_drag_asset_will_import_linked(drag)) {
    return;
  }

  V3DSnapCursorState *state = view3d_drop_snap_init(drop);

  float dimensions[3] = {0.0f};
  if (drag->type == WM_DRAG_ID) {
    Object *ob = (Object *)WM_drag_get_local_ID(drag, ID_OB);
    BKE_object_dimensions_eval_cached_get(ob, dimensions);
  }
  else {
    AssetMetaData *meta_data = WM_drag_get_asset_meta_data(drag, ID_OB);
    IDProperty *dimensions_prop = BKE_asset_metadata_idprop_find(meta_data, "dimensions");
    if (dimensions_prop) {
      copy_v3_v3(dimensions, IDP_array_float_get(dimensions_prop));
    }
  }

  if (!is_zero_v3(dimensions)) {
    mul_v3_v3fl(state->box_dimensions, dimensions, 0.5f);
    UI_GetThemeColor4ubv(TH_GIZMO_PRIMARY, state->color_box);
    state->draw_box = true;
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
  if (!view3d_drop_id_in_main_region_poll(C, drag, event, ID_MA)) {
    return false;
  }

  Object *ob = ED_view3d_give_object_under_cursor(C, event->mval);

  return (ob && ID_IS_EDITABLE(&ob->id) && !ID_IS_OVERRIDE_LIBRARY(&ob->id));
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
  return blender::ed::object::drop_named_material_tooltip(C, name, mval);
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
    if (!tree_type || IDP_int_get(tree_type) != NTREE_GEOMETRY) {
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
  return blender::ed::object::drop_geometry_nodes_tooltip(C, drop->ptr, mval);
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

static void view3d_collection_drop_on_enter(wmDropBox *drop, wmDrag *drag)
{
  if (WM_drag_asset_will_import_linked(drag)) {
    const wmDragAsset *asset_drag = WM_drag_get_asset_data(drag, 0);
    /* Linked collections cannot be transformed except when using instancing. Don't enable
     * snapping. */
    if (!asset_drag->import_settings.use_instance_collections) {
      return;
    }
  }

  view3d_drop_snap_init(drop);
}

static void view3d_collection_drop_matrix_from_snap(V3DSnapCursorState *snap_state,
                                                    float r_loc[3],
                                                    float r_rot[3])
{
  using namespace blender;
  V3DSnapCursorData *snap_data = ED_view3d_cursor_snap_data_get();
  BLI_assert(snap_state->draw_box || snap_state->draw_plane);
  UNUSED_VARS_NDEBUG(snap_state);

  mat3_normalized_to_eul(r_rot, snap_data->plane_omat);
  copy_v3_v3(r_loc, snap_data->loc);
}

static void view3d_collection_drop_copy_local_id(bContext * /*C*/, wmDrag *drag, wmDropBox *drop)
{
  ID *id = WM_drag_get_local_ID(drag, ID_GR);
  RNA_int_set(drop->ptr, "session_uid", int(id->session_uid));

  V3DSnapCursorState *snap_state = ED_view3d_cursor_snap_state_active_get();

  float loc[3], rot[3];
  view3d_collection_drop_matrix_from_snap(snap_state, loc, rot);
  RNA_float_set_array(drop->ptr, "location", loc);
  RNA_float_set_array(drop->ptr, "rotation", rot);
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

  const bool use_instance_collections = asset_drag->import_settings.use_instance_collections;
  /* Temporarily disable instancing for the import, the drop operator handles that. */
  asset_drag->import_settings.use_instance_collections = false;

  ID *id = WM_drag_asset_id_import(C, asset_drag, FILE_AUTOSELECT);
  Collection *collection = (Collection *)id;

  /* Reset temporary override. */
  asset_drag->import_settings.use_instance_collections = use_instance_collections;

  /* TODO(sergey): Only update relations for the current scene. */
  DEG_relations_tag_update(CTX_data_main(C));
  WM_event_add_notifier(C, NC_SCENE | ND_LAYER_CONTENT, scene);

  RNA_int_set(drop->ptr, "session_uid", int(id->session_uid));
  RNA_boolean_set(drop->ptr, "use_instance", asset_drag->import_settings.use_instance_collections);

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

  V3DSnapCursorState *snap_state = static_cast<V3DSnapCursorState *>(drop->draw_data);
  if (snap_state) {
    float loc[3], rot[3];
    view3d_collection_drop_matrix_from_snap(snap_state, loc, rot);
    RNA_float_set_array(drop->ptr, "location", loc);
    RNA_float_set_array(drop->ptr, "rotation", rot);
  }

  /* XXX Without an undo push here, there will be a crash when the user modifies operator
   * properties. The stuff we do in these drop callbacks just isn't safe over undo/redo. */
  ED_undo_push(C, "Drop Collection");
}

static void view3d_id_drop_copy(bContext *C, wmDrag *drag, wmDropBox *drop)
{
  ID *id = WM_drag_get_local_ID_or_import_from_asset(C, drag, 0);

  WM_operator_properties_id_lookup_set_from_id(drop->ptr, id);
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
}

void view3d_dropboxes()
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
  drop->on_exit = view3d_drop_snap_exit;

  drop = WM_dropbox_add(lb,
                        "OBJECT_OT_transform_to_mouse",
                        view3d_ob_drop_poll_external_asset,
                        view3d_ob_drop_copy_external_asset,
                        WM_drag_free_imported_drag_ID,
                        nullptr);

  drop->draw_droptip = WM_drag_draw_item_name_fn;
  drop->on_enter = view3d_ob_drop_on_enter;
  drop->on_exit = view3d_drop_snap_exit;

  drop = WM_dropbox_add(lb,
                        "OBJECT_OT_collection_external_asset_drop",
                        view3d_collection_drop_poll_external_asset,
                        view3d_collection_drop_copy_external_asset,
                        WM_drag_free_imported_drag_ID,
                        nullptr);
  drop->draw_droptip = WM_drag_draw_item_name_fn;
  drop->on_enter = view3d_collection_drop_on_enter;
  drop->on_exit = view3d_drop_snap_exit;
  drop = WM_dropbox_add(lb,
                        "OBJECT_OT_collection_instance_add",
                        view3d_collection_drop_poll_local_id,
                        view3d_collection_drop_copy_local_id,
                        WM_drag_free_imported_drag_ID,
                        nullptr);
  drop->draw_droptip = WM_drag_draw_item_name_fn;
  drop->on_enter = view3d_collection_drop_on_enter;
  drop->on_exit = view3d_drop_snap_exit;

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
