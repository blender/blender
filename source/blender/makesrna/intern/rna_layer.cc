/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup RNA
 */

#include "DNA_layer_types.h"

#include "ED_object.hh"
#include "ED_render.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "RNA_define.hh"

#include "rna_internal.hh"

#ifdef RNA_RUNTIME

#  ifdef WITH_PYTHON
#    include "BPY_extern.hh"
#  endif

#  include "DNA_collection_types.h"
#  include "DNA_object_types.h"

#  include "RNA_access.hh"

#  include "BKE_idprop.hh"
#  include "BKE_layer.hh"
#  include "BKE_mesh.hh"
#  include "BKE_node.hh"
#  include "BKE_scene.hh"

#  include "NOD_composite.hh"

#  include "BLI_listbase.h"

#  include "DEG_depsgraph_build.hh"
#  include "DEG_depsgraph_query.hh"

#  include "RE_engine.h"

/***********************************/

static PointerRNA rna_ViewLayer_active_layer_collection_get(PointerRNA *ptr)
{
  const Scene *scene = (const Scene *)ptr->owner_id;
  ViewLayer *view_layer = (ViewLayer *)ptr->data;
  BKE_view_layer_synced_ensure(scene, view_layer);
  LayerCollection *lc = BKE_view_layer_active_collection_get(view_layer);
  return RNA_pointer_create_with_parent(*ptr, &RNA_LayerCollection, lc);
}

static void rna_ViewLayer_active_layer_collection_set(PointerRNA *ptr,
                                                      PointerRNA value,
                                                      ReportList * /*reports*/)
{
  const Scene *scene = (const Scene *)ptr->owner_id;
  ViewLayer *view_layer = (ViewLayer *)ptr->data;
  LayerCollection *lc = (LayerCollection *)value.data;
  BKE_view_layer_synced_ensure(scene, view_layer);
  const int index = BKE_layer_collection_findindex(view_layer, lc);
  if (index != -1) {
    BKE_layer_collection_activate(view_layer, lc);
  }
}

static PointerRNA rna_LayerObjects_active_object_get(PointerRNA *ptr)
{
  const Scene *scene = (Scene *)ptr->owner_id;
  ViewLayer *view_layer = (ViewLayer *)ptr->data;
  BKE_view_layer_synced_ensure(scene, view_layer);
  return RNA_id_pointer_create(
      reinterpret_cast<ID *>(BKE_view_layer_active_object_get(view_layer)));
}

static void rna_LayerObjects_active_object_set(PointerRNA *ptr,
                                               PointerRNA value,
                                               ReportList *reports)
{
  const Scene *scene = (Scene *)ptr->owner_id;
  ViewLayer *view_layer = (ViewLayer *)ptr->data;
  if (value.data) {
    Object *ob = static_cast<Object *>(value.data);
    BKE_view_layer_synced_ensure(scene, view_layer);
    Base *basact_test = BKE_view_layer_base_find(view_layer, ob);
    if (basact_test != nullptr) {
      view_layer->basact = basact_test;
    }
    else {
      BKE_reportf(reports,
                  RPT_ERROR,
                  "ViewLayer '%s' does not contain object '%s'",
                  view_layer->name,
                  ob->id.name + 2);
    }
  }
  else {
    view_layer->basact = nullptr;
  }
}

size_t rna_ViewLayer_path_buffer_get(const ViewLayer *view_layer,
                                     char *r_rna_path,
                                     const size_t rna_path_buffer_size)
{
  char name_esc[sizeof(view_layer->name) * 2];
  BLI_str_escape(name_esc, view_layer->name, sizeof(name_esc));

  return BLI_snprintf_rlen(r_rna_path, rna_path_buffer_size, "view_layers[\"%s\"]", name_esc);
}

static std::optional<std::string> rna_ViewLayer_path(const PointerRNA *ptr)
{
  const ViewLayer *view_layer = (ViewLayer *)ptr->data;
  char rna_path[sizeof(view_layer->name) * 3];
  rna_ViewLayer_path_buffer_get(view_layer, rna_path, sizeof(rna_path));
  return rna_path;
}

static IDProperty **rna_ViewLayer_idprops(PointerRNA *ptr)
{
  ViewLayer *view_layer = (ViewLayer *)ptr->data;
  return &view_layer->id_properties;
}

static IDProperty **rna_ViewLayer_system_idprops(PointerRNA *ptr)
{
  ViewLayer *view_layer = (ViewLayer *)ptr->data;
  return &view_layer->system_properties;
}

static bool rna_LayerCollection_visible_get(LayerCollection *layer_collection, bContext *C)
{
  View3D *v3d = CTX_wm_view3d(C);

  if ((v3d == nullptr) || ((v3d->flag & V3D_LOCAL_COLLECTIONS) == 0)) {
    return (layer_collection->runtime_flag & LAYER_COLLECTION_VISIBLE_VIEW_LAYER) != 0;
  }

  if (v3d->local_collections_uid & layer_collection->local_collections_bits) {
    return (layer_collection->runtime_flag & LAYER_COLLECTION_HIDE_VIEWPORT) == 0;
  }

  return false;
}

static void rna_ViewLayer_update_render_passes(ID *id)
{
  Scene *scene = (Scene *)id;
  if (scene->compositing_node_group) {
    ntreeCompositUpdateRLayers(scene->compositing_node_group);
  }

  RenderEngineType *engine_type = RE_engines_find(scene->r.engine);
  if (engine_type->update_render_passes) {
    RenderEngine *engine = RE_engine_create(engine_type);
    if (engine) {
      LISTBASE_FOREACH (ViewLayer *, view_layer, &scene->view_layers) {
        BKE_view_layer_verify_aov(engine, scene, view_layer);
      }
    }
    RE_engine_free(engine);
    engine = nullptr;
  }
}

static PointerRNA rna_ViewLayer_objects_get(CollectionPropertyIterator *iter)
{
  ListBaseIterator *internal = &iter->internal.listbase;

  /* we are actually iterating a ObjectBase list */
  Base *base = (Base *)internal->link;
  return RNA_id_pointer_create(reinterpret_cast<ID *>(base->object));
}

static bool rna_ViewLayer_objects_selected_skip(CollectionPropertyIterator *iter, void * /*data*/)
{
  ListBaseIterator *internal = &iter->internal.listbase;
  Base *base = (Base *)internal->link;

  if ((base->flag & BASE_SELECTED) != 0) {
    return false;
  }

  return true;
};

static PointerRNA rna_ViewLayer_depsgraph_get(PointerRNA *ptr)
{
  ID *id = ptr->owner_id;
  if (GS(id->name) == ID_SCE) {
    Scene *scene = (Scene *)id;
    ViewLayer *view_layer = (ViewLayer *)ptr->data;
    Depsgraph *depsgraph = BKE_scene_get_depsgraph(scene, view_layer);
    return RNA_pointer_create_with_parent(*ptr, &RNA_Depsgraph, depsgraph);
  }
  return PointerRNA_NULL;
}

static void rna_ViewLayer_remove_aov(ViewLayer *view_layer, ReportList *reports, ViewLayerAOV *aov)
{
  if (BLI_findindex(&view_layer->aovs, aov) == -1) {
    BKE_reportf(reports, RPT_ERROR, "AOV not found in view-layer '%s'", view_layer->name);
    return;
  }
  BKE_view_layer_remove_aov(view_layer, aov);
}

static void rna_LayerObjects_selected_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  ViewLayer *view_layer = (ViewLayer *)ptr->data;
  rna_iterator_listbase_begin(
      iter, ptr, BKE_view_layer_object_bases_get(view_layer), rna_ViewLayer_objects_selected_skip);
}

static void rna_ViewLayer_update_tagged(ID *id_ptr,
                                        ViewLayer *view_layer,
                                        Main *bmain,
                                        ReportList *reports)
{
  Scene *scene = (Scene *)id_ptr;
  Depsgraph *depsgraph = BKE_scene_ensure_depsgraph(bmain, scene, view_layer);

  if (DEG_is_evaluating(depsgraph)) {
    BKE_report(reports, RPT_ERROR, "Dependency graph update requested during evaluation");
    return;
  }

#  ifdef WITH_PYTHON
  /* Allow drivers to be evaluated */
  BPy_BEGIN_ALLOW_THREADS;
#  endif

  /* NOTE: This is similar to CTX_data_depsgraph_pointer(). Ideally such access would be
   * de-duplicated across all possible cases, but for now this is safest and easiest way to go.
   *
   * The reason for this is that it's possible to have Python operator which asks view layer to
   * be updated. After re-do of such operator view layer's dependency graph will not be marked
   * as active. */
  DEG_make_active(depsgraph);
  BKE_scene_graph_update_tagged(depsgraph, bmain);

#  ifdef WITH_PYTHON
  BPy_END_ALLOW_THREADS;
#  endif
}

static void rna_ObjectBase_select_update(Main * /*bmain*/, Scene * /*scene*/, PointerRNA *ptr)
{
  Base *base = (Base *)ptr->data;
  short mode = (base->flag & BASE_SELECTED) ? blender::ed::object::BA_SELECT :
                                              blender::ed::object::BA_DESELECT;
  blender::ed::object::base_select(base, blender::ed::object::eObjectSelect_Mode(mode));
}

static void rna_ObjectBase_hide_viewport_update(bContext *C, PointerRNA * /*ptr*/)
{
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  BKE_view_layer_need_resync_tag(view_layer);
  DEG_id_tag_update(&scene->id, ID_RECALC_BASE_FLAGS);
  WM_event_add_notifier(C, NC_SCENE | ND_OB_SELECT, scene);
}

static void rna_LayerCollection_name_get(PointerRNA *ptr, char *value)
{
  ID *id = (ID *)((LayerCollection *)ptr->data)->collection;
  strcpy(value, id->name + 2);
}

int rna_LayerCollection_name_length(PointerRNA *ptr)
{
  ID *id = (ID *)((LayerCollection *)ptr->data)->collection;
  return strlen(id->name + 2);
}

static void rna_LayerCollection_flag_set(PointerRNA *ptr, const bool value, const int flag)
{
  LayerCollection *layer_collection = (LayerCollection *)ptr->data;
  Collection *collection = layer_collection->collection;

  if (collection->flag & COLLECTION_IS_MASTER) {
    return;
  }

  if (value) {
    layer_collection->flag |= flag;
  }
  else {
    layer_collection->flag &= ~flag;
  }
}

static void rna_LayerCollection_exclude_set(PointerRNA *ptr, bool value)
{
  rna_LayerCollection_flag_set(ptr, value, LAYER_COLLECTION_EXCLUDE);
}

static void rna_LayerCollection_holdout_set(PointerRNA *ptr, bool value)
{
  rna_LayerCollection_flag_set(ptr, value, LAYER_COLLECTION_HOLDOUT);
}

static void rna_LayerCollection_indirect_only_set(PointerRNA *ptr, bool value)
{
  rna_LayerCollection_flag_set(ptr, value, LAYER_COLLECTION_INDIRECT_ONLY);
}

static void rna_LayerCollection_hide_viewport_set(PointerRNA *ptr, bool value)
{
  rna_LayerCollection_flag_set(ptr, value, LAYER_COLLECTION_HIDE);
}

static void rna_LayerCollection_exclude_update(Main *bmain, Scene * /*scene*/, PointerRNA *ptr)
{
  Scene *scene = (Scene *)ptr->owner_id;
  LayerCollection *lc = (LayerCollection *)ptr->data;
  ViewLayer *view_layer = BKE_view_layer_find_from_collection(scene, lc);

  /* Set/Unset it recursively to match the behavior of excluding via the menu or shortcuts. */
  const bool exclude = (lc->flag & LAYER_COLLECTION_EXCLUDE) != 0;
  BKE_layer_collection_set_flag(lc, LAYER_COLLECTION_EXCLUDE, exclude);

  BKE_view_layer_need_resync_tag(view_layer);

  DEG_id_tag_update(&scene->id, ID_RECALC_BASE_FLAGS);
  if (!exclude) {
    /* We need to update animation of objects added back to the scene through enabling this view
     * layer. */
    FOREACH_OBJECT_BEGIN (scene, view_layer, ob) {
      DEG_id_tag_update(&ob->id, ID_RECALC_ANIMATION);
    }
    FOREACH_OBJECT_END;
  }

  DEG_relations_tag_update(bmain);
  WM_main_add_notifier(NC_SCENE | ND_LAYER_CONTENT, nullptr);
  if (exclude) {
    blender::ed::object::base_active_refresh(bmain, scene, view_layer);
  }
}

static void rna_LayerCollection_update(Main * /*bmain*/, Scene * /*scene*/, PointerRNA *ptr)
{
  Scene *scene = (Scene *)ptr->owner_id;
  LayerCollection *lc = (LayerCollection *)ptr->data;
  ViewLayer *view_layer = BKE_view_layer_find_from_collection(scene, lc);

  BKE_view_layer_need_resync_tag(view_layer);

  DEG_id_tag_update(&scene->id, ID_RECALC_BASE_FLAGS);

  WM_main_add_notifier(NC_SCENE | ND_LAYER_CONTENT, nullptr);
  WM_main_add_notifier(NC_IMAGE | ND_LAYER_CONTENT, nullptr);
}

static bool rna_LayerCollection_has_objects(LayerCollection *lc)
{
  return (lc->runtime_flag & LAYER_COLLECTION_HAS_OBJECTS) != 0;
}

static bool rna_LayerCollection_has_selected_objects(LayerCollection *lc,
                                                     Main *bmain,
                                                     ViewLayer *view_layer)
{
  LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
    LISTBASE_FOREACH (ViewLayer *, scene_view_layer, &scene->view_layers) {
      if (scene_view_layer == view_layer) {
        return BKE_layer_collection_has_selected_objects(scene, view_layer, lc);
      }
    }
  }
  return false;
}

void rna_LayerCollection_children_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  Scene *scene = (Scene *)ptr->owner_id;
  LayerCollection *lc = (LayerCollection *)ptr->data;
  ViewLayer *view_layer = BKE_view_layer_find_from_collection(scene, lc);
  BKE_view_layer_synced_ensure(scene, view_layer);

  rna_iterator_listbase_begin(iter, ptr, &lc->layer_collections, nullptr);
}

static bool rna_LayerCollection_children_lookupint(PointerRNA *ptr, int key, PointerRNA *r_ptr)
{
  Scene *scene = (Scene *)ptr->owner_id;
  LayerCollection *lc = (LayerCollection *)ptr->data;
  /* TODO: replace by using RNA ancestors. */
  ViewLayer *view_layer = BKE_view_layer_find_from_collection(scene, lc);
  BKE_view_layer_synced_ensure(scene, view_layer);

  LayerCollection *child = static_cast<LayerCollection *>(
      BLI_findlink(&lc->layer_collections, key));
  if (!child) {
    return false;
  }
  rna_pointer_create_with_ancestors(*ptr, &RNA_LayerCollection, child, *r_ptr);
  return true;
}

static bool rna_LayerCollection_children_lookupstring(PointerRNA *ptr,
                                                      const char *key,
                                                      PointerRNA *r_ptr)
{
  Scene *scene = (Scene *)ptr->owner_id;
  LayerCollection *lc = (LayerCollection *)ptr->data;
  /* TODO: replace by using RNA ancestors. */
  ViewLayer *view_layer = BKE_view_layer_find_from_collection(scene, lc);
  BKE_view_layer_synced_ensure(scene, view_layer);

  LISTBASE_FOREACH (LayerCollection *, child, &lc->layer_collections) {
    if (STREQ(child->collection->id.name + 2, key)) {
      rna_pointer_create_with_ancestors(*ptr, &RNA_LayerCollection, child, *r_ptr);
      return true;
    }
  }
  return false;
}

#else

static void rna_def_layer_collection(BlenderRNA *brna)
{
  StructRNA *srna;
  FunctionRNA *func;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "LayerCollection", nullptr);
  RNA_def_struct_ui_text(srna, "Layer Collection", "Layer collection");
  RNA_def_struct_ui_icon(srna, ICON_OUTLINER_COLLECTION);

  prop = RNA_def_property(srna, "collection", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE | PROP_ANIMATABLE);
  RNA_def_property_struct_type(prop, "Collection");
  RNA_def_property_ui_text(prop, "Collection", "Collection this layer collection is wrapping");

  prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "collection->id.name");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE | PROP_ANIMATABLE);
  RNA_def_property_ui_text(
      prop, "Name", "Name of this layer collection (same as its collection one)");
  RNA_def_property_string_funcs(
      prop, "rna_LayerCollection_name_get", "rna_LayerCollection_name_length", nullptr);
  RNA_def_struct_name_property(srna, prop);

  prop = RNA_def_property(srna, "children", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, nullptr, "layer_collections", nullptr);
  RNA_def_property_struct_type(prop, "LayerCollection");
  RNA_def_property_ui_text(prop, "Children", "Layer collection children");
  RNA_def_property_collection_funcs(prop,
                                    "rna_LayerCollection_children_begin",
                                    nullptr,
                                    nullptr,
                                    nullptr,
                                    nullptr,
                                    "rna_LayerCollection_children_lookupint",
                                    "rna_LayerCollection_children_lookupstring",
                                    nullptr);

  /* Restriction flags. */
  prop = RNA_def_property(srna, "exclude", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", LAYER_COLLECTION_EXCLUDE);
  RNA_def_property_boolean_funcs(prop, nullptr, "rna_LayerCollection_exclude_set");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Exclude from View Layer", "Exclude from view layer");
  RNA_def_property_ui_icon(prop, ICON_CHECKBOX_HLT, -1);
  RNA_def_property_update(prop, NC_SCENE | ND_LAYER, "rna_LayerCollection_exclude_update");

  prop = RNA_def_property(srna, "holdout", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", LAYER_COLLECTION_HOLDOUT);
  RNA_def_property_boolean_funcs(prop, nullptr, "rna_LayerCollection_holdout_set");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_icon(prop, ICON_HOLDOUT_OFF, 1);
  RNA_def_property_ui_text(prop, "Holdout", "Mask out objects in collection from view layer");
  RNA_def_property_update(prop, NC_SCENE | ND_LAYER, "rna_LayerCollection_update");

  prop = RNA_def_property(srna, "indirect_only", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", LAYER_COLLECTION_INDIRECT_ONLY);
  RNA_def_property_boolean_funcs(prop, nullptr, "rna_LayerCollection_indirect_only_set");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_icon(prop, ICON_INDIRECT_ONLY_OFF, 1);
  RNA_def_property_ui_text(
      prop,
      "Indirect Only",
      "Objects in collection only contribute indirectly (through shadows and reflections) "
      "in the view layer");
  RNA_def_property_update(prop, NC_SCENE | ND_LAYER, "rna_LayerCollection_update");

  prop = RNA_def_property(srna, "hide_viewport", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", LAYER_COLLECTION_HIDE);
  RNA_def_property_boolean_funcs(prop, nullptr, "rna_LayerCollection_hide_viewport_set");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_icon(prop, ICON_HIDE_OFF, -1);
  RNA_def_property_ui_text(prop, "Hide in Viewport", "Temporarily hide in viewport");
  RNA_def_property_update(prop, NC_SCENE | ND_LAYER_CONTENT, "rna_LayerCollection_update");

  func = RNA_def_function(srna, "visible_get", "rna_LayerCollection_visible_get");
  RNA_def_function_ui_description(func,
                                  "Whether this collection is visible, take into account the "
                                  "collection parent and the viewport");
  RNA_def_function_flag(func, FUNC_USE_CONTEXT);
  RNA_def_function_return(func, RNA_def_boolean(func, "result", false, "", ""));

  /* Run-time flags. */
  prop = RNA_def_property(srna, "is_visible", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(
      prop, nullptr, "runtime_flag", LAYER_COLLECTION_VISIBLE_VIEW_LAYER);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop,
                           "Visible",
                           "Whether this collection is visible for the view layer, take into "
                           "account the collection parent");

  func = RNA_def_function(srna, "has_objects", "rna_LayerCollection_has_objects");
  RNA_def_function_ui_description(func, "");
  RNA_def_function_return(func, RNA_def_boolean(func, "result", false, "", ""));

  func = RNA_def_function(
      srna, "has_selected_objects", "rna_LayerCollection_has_selected_objects");
  RNA_def_function_flag(func, FUNC_USE_MAIN);
  RNA_def_function_ui_description(func, "");
  prop = RNA_def_pointer(
      func, "view_layer", "ViewLayer", "", "View layer the layer collection belongs to");
  RNA_def_parameter_flags(prop, PropertyFlag(0), PARM_REQUIRED);
  RNA_def_function_return(func, RNA_def_boolean(func, "result", false, "", ""));
}

static void rna_def_layer_objects(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;
  PropertyRNA *prop;

  RNA_def_property_srna(cprop, "LayerObjects");
  srna = RNA_def_struct(brna, "LayerObjects", nullptr);
  RNA_def_struct_sdna(srna, "ViewLayer");
  RNA_def_struct_ui_text(srna, "Layer Objects", "Collections of objects");

  prop = RNA_def_property(srna, "active", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "Object");
  RNA_def_property_pointer_funcs(prop,
                                 "rna_LayerObjects_active_object_get",
                                 "rna_LayerObjects_active_object_set",
                                 nullptr,
                                 nullptr);
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_NEVER_UNLINK);
  RNA_def_property_ui_text(prop, "Active Object", "Active object for this layer");
  /* Could call: `blender::ed::object::base_activate(C, view_layer->basact);`
   * but would be a bad level call and it seems the notifier is enough */
  RNA_def_property_update(prop, NC_SCENE | ND_OB_ACTIVE, nullptr);

  prop = RNA_def_property(srna, "selected", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, nullptr, "object_bases", nullptr);
  RNA_def_property_struct_type(prop, "Object");
  RNA_def_property_collection_funcs(prop,
                                    "rna_LayerObjects_selected_begin",
                                    "rna_iterator_listbase_next",
                                    "rna_iterator_listbase_end",
                                    "rna_ViewLayer_objects_get",
                                    nullptr,
                                    nullptr,
                                    nullptr,
                                    nullptr);
  RNA_def_property_ui_text(prop, "Selected Objects", "All the selected objects of this layer");
}

static void rna_def_object_base(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "ObjectBase", nullptr);
  RNA_def_struct_sdna(srna, "Base");
  RNA_def_struct_ui_text(
      srna,
      "Object Base",
      "An object instance in a View Layer (currently never exposed in Python API)");
  RNA_def_struct_ui_icon(srna, ICON_OBJECT_DATA);

  prop = RNA_def_property(srna, "object", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "object");
  RNA_def_property_ui_text(prop, "Object", "Object this base links to");

  prop = RNA_def_property(srna, "select", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", BASE_SELECTED);
  RNA_def_property_ui_text(prop, "Select", "Object base selection state");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, "rna_ObjectBase_select_update");

  prop = RNA_def_property(srna, "hide_viewport", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", BASE_HIDDEN);
  RNA_def_property_flag(prop, PROP_LIB_EXCEPTION);
  RNA_def_property_flag(prop, PROP_NO_DEG_UPDATE); /* The update callback does tagging. */
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_icon(prop, ICON_HIDE_OFF, -1);
  RNA_def_property_ui_text(prop, "Hide in Viewport", "Temporarily hide in viewport");
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, "rna_ObjectBase_hide_viewport_update");
}

void RNA_def_view_layer(BlenderRNA *brna)
{
  FunctionRNA *func;
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "ViewLayer", nullptr);
  RNA_def_struct_ui_text(srna, "View Layer", "View layer");
  RNA_def_struct_ui_icon(srna, ICON_RENDER_RESULT);
  RNA_def_struct_path_func(srna, "rna_ViewLayer_path");
  RNA_def_struct_idprops_func(srna, "rna_ViewLayer_idprops");
  RNA_def_struct_system_idprops_func(srna, "rna_ViewLayer_system_idprops");

  rna_def_view_layer_common(brna, srna, true);

  func = RNA_def_function(srna, "update_render_passes", "rna_ViewLayer_update_render_passes");
  RNA_def_function_ui_description(func,
                                  "Requery the enabled render passes from the render engine");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_NO_SELF);

  prop = RNA_def_property(srna, "layer_collection", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "LayerCollection");
  RNA_def_property_pointer_sdna(prop, nullptr, "layer_collections.first");
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_ui_text(
      prop,
      "Layer Collection",
      "Root of collections hierarchy of this view layer, "
      "its 'collection' pointer property is the same as the scene's master collection");

  prop = RNA_def_property(srna, "active_layer_collection", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "LayerCollection");
  RNA_def_property_pointer_funcs(prop,
                                 "rna_ViewLayer_active_layer_collection_get",
                                 "rna_ViewLayer_active_layer_collection_set",
                                 nullptr,
                                 nullptr);
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_NEVER_NULL);
  RNA_def_property_ui_text(
      prop, "Active Layer Collection", "Active layer collection in this view layer's hierarchy");
  RNA_def_property_update(prop, NC_SCENE | ND_LAYER, nullptr);

  prop = RNA_def_property(srna, "objects", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, nullptr, "object_bases", nullptr);
  RNA_def_property_struct_type(prop, "Object");
  RNA_def_property_collection_funcs(prop,
                                    nullptr,
                                    nullptr,
                                    nullptr,
                                    "rna_ViewLayer_objects_get",
                                    nullptr,
                                    nullptr,
                                    nullptr,
                                    nullptr);
  RNA_def_property_ui_text(prop, "Objects", "All the objects in this layer");
  rna_def_layer_objects(brna, prop);

  /* layer options */
  prop = RNA_def_property(srna, "use", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", VIEW_LAYER_RENDER);
  RNA_def_property_ui_text(prop, "Enabled", "Enable or disable rendering of this View Layer");
  RNA_def_property_update(prop, NC_SCENE | ND_LAYER, nullptr);

  /* Cached flag indicating if any Collection in this ViewLayer has an Exporter set. */
  prop = RNA_def_property(srna, "has_export_collections", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", VIEW_LAYER_HAS_EXPORT_COLLECTIONS);
  RNA_def_property_ui_text(prop,
                           "Has export collections",
                           "At least one Collection in this View Layer has an exporter");

  prop = RNA_def_property(srna, "use_freestyle", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", VIEW_LAYER_FREESTYLE);
  RNA_def_property_ui_text(prop, "Freestyle", "Render stylized strokes in this Layer");
  RNA_def_property_update(prop, NC_SCENE | ND_LAYER, nullptr);

  /* Freestyle */
  rna_def_freestyle_settings(brna);

  prop = RNA_def_property(srna, "freestyle_settings", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_pointer_sdna(prop, nullptr, "freestyle_config");
  RNA_def_property_struct_type(prop, "FreestyleSettings");
  RNA_def_property_ui_text(prop, "Freestyle Settings", "");

  /* Grease Pencil */
  prop = RNA_def_property(srna, "use_pass_grease_pencil", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(
      prop, nullptr, "grease_pencil_flags", GREASE_PENCIL_AS_SEPARATE_PASS);
  RNA_def_property_ui_text(
      prop, "Grease Pencil", "Deliver Grease Pencil render result in a separate pass");
  RNA_def_property_update(prop, NC_SCENE | ND_RENDER_OPTIONS, "rna_ViewLayer_pass_update");

  /* debug update routine */
  func = RNA_def_function(srna, "update", "rna_ViewLayer_update_tagged");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_MAIN | FUNC_USE_REPORTS);
  RNA_def_function_ui_description(
      func, "Update data tagged to be updated from previous access to data or operators");

  /* Dependency Graph */
  prop = RNA_def_property(srna, "depsgraph", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "Depsgraph");
  RNA_def_property_flag_hide_from_ui_workaround(prop);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_NO_COMPARISON);
  RNA_def_property_ui_text(prop, "Dependency Graph", "Dependencies in the scene data");
  RNA_def_property_pointer_funcs(prop, "rna_ViewLayer_depsgraph_get", nullptr, nullptr, nullptr);

  /* Nested Data. */
  /* *** Non-Animated *** */
  RNA_define_animate_sdna(false);
  rna_def_layer_collection(brna);
  rna_def_object_base(brna);
  RNA_define_animate_sdna(true);
  /* *** Animated *** */
}

#endif
