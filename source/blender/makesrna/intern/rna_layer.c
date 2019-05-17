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
 */

/** \file
 * \ingroup RNA
 */

#include "DNA_scene_types.h"
#include "DNA_layer_types.h"
#include "DNA_view3d_types.h"

#include "BLT_translation.h"

#include "ED_object.h"
#include "ED_render.h"

#include "RE_engine.h"

#include "DRW_engine.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_define.h"

#include "rna_internal.h"

#ifdef RNA_RUNTIME

#  ifdef WITH_PYTHON
#    include "BPY_extern.h"
#  endif

#  include "DNA_collection_types.h"
#  include "DNA_object_types.h"

#  include "RNA_access.h"

#  include "BKE_idprop.h"
#  include "BKE_layer.h"
#  include "BKE_node.h"
#  include "BKE_scene.h"
#  include "BKE_mesh.h"

#  include "DEG_depsgraph_build.h"
#  include "DEG_depsgraph_query.h"

/***********************************/

static PointerRNA rna_ViewLayer_active_layer_collection_get(PointerRNA *ptr)
{
  ViewLayer *view_layer = (ViewLayer *)ptr->data;
  LayerCollection *lc = view_layer->active_collection;
  return rna_pointer_inherit_refine(ptr, &RNA_LayerCollection, lc);
}

static void rna_ViewLayer_active_layer_collection_set(struct ReportList *UNUSED(reports),
                                                      PointerRNA *ptr,
                                                      PointerRNA value)
{
  ViewLayer *view_layer = (ViewLayer *)ptr->data;
  LayerCollection *lc = (LayerCollection *)value.data;
  const int index = BKE_layer_collection_findindex(view_layer, lc);
  if (index != -1) {
    BKE_layer_collection_activate(view_layer, lc);
  }
}

static PointerRNA rna_LayerObjects_active_object_get(PointerRNA *ptr)
{
  ViewLayer *view_layer = (ViewLayer *)ptr->data;
  return rna_pointer_inherit_refine(
      ptr, &RNA_Object, view_layer->basact ? view_layer->basact->object : NULL);
}

static void rna_LayerObjects_active_object_set(struct ReportList *UNUSED(reports),
                                               PointerRNA *ptr,
                                               PointerRNA value)
{
  ViewLayer *view_layer = (ViewLayer *)ptr->data;
  if (value.data)
    view_layer->basact = BKE_view_layer_base_find(view_layer, (Object *)value.data);
  else
    view_layer->basact = NULL;
}

static char *rna_ViewLayer_path(PointerRNA *ptr)
{
  ViewLayer *srl = (ViewLayer *)ptr->data;
  char name_esc[sizeof(srl->name) * 2];

  BLI_strescape(name_esc, srl->name, sizeof(name_esc));
  return BLI_sprintfN("view_layers[\"%s\"]", name_esc);
}

static IDProperty *rna_ViewLayer_idprops(PointerRNA *ptr, bool create)
{
  ViewLayer *view_layer = (ViewLayer *)ptr->data;

  if (create && !view_layer->id_properties) {
    IDPropertyTemplate val = {0};
    view_layer->id_properties = IDP_New(IDP_GROUP, &val, "ViewLayer ID properties");
  }

  return view_layer->id_properties;
}

static void rna_ViewLayer_update_render_passes(ID *id)
{
  Scene *scene = (Scene *)id;
  if (scene->nodetree)
    ntreeCompositUpdateRLayers(scene->nodetree);
}

static PointerRNA rna_ViewLayer_objects_get(CollectionPropertyIterator *iter)
{
  ListBaseIterator *internal = &iter->internal.listbase;

  /* we are actually iterating a ObjectBase list */
  Base *base = (Base *)internal->link;
  return rna_pointer_inherit_refine(&iter->parent, &RNA_Object, base->object);
}

static int rna_ViewLayer_objects_selected_skip(CollectionPropertyIterator *iter,
                                               void *UNUSED(data))
{
  ListBaseIterator *internal = &iter->internal.listbase;
  Base *base = (Base *)internal->link;

  if ((base->flag & BASE_SELECTED) != 0) {
    return 0;
  }

  return 1;
};

static PointerRNA rna_ViewLayer_depsgraph_get(PointerRNA *ptr)
{
  ID *id = ptr->id.data;
  if (GS(id->name) == ID_SCE) {
    Scene *scene = (Scene *)id;
    ViewLayer *view_layer = (ViewLayer *)ptr->data;
    Depsgraph *depsgraph = BKE_scene_get_depsgraph(scene, view_layer, false);
    return rna_pointer_inherit_refine(ptr, &RNA_Depsgraph, depsgraph);
  }
  return PointerRNA_NULL;
}

static void rna_LayerObjects_selected_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  ViewLayer *view_layer = (ViewLayer *)ptr->data;
  rna_iterator_listbase_begin(
      iter, &view_layer->object_bases, rna_ViewLayer_objects_selected_skip);
}

static void rna_ViewLayer_update_tagged(ID *id_ptr, ViewLayer *view_layer, Main *bmain)
{
#  ifdef WITH_PYTHON
  /* Allow drivers to be evaluated */
  BPy_BEGIN_ALLOW_THREADS;
#  endif

  Scene *scene = (Scene *)id_ptr;
  Depsgraph *depsgraph = BKE_scene_get_depsgraph(scene, view_layer, true);
  BKE_scene_graph_update_tagged(depsgraph, bmain);

#  ifdef WITH_PYTHON
  BPy_END_ALLOW_THREADS;
#  endif
}

static void rna_ObjectBase_select_update(Main *UNUSED(bmain),
                                         Scene *UNUSED(scene),
                                         PointerRNA *ptr)
{
  Base *base = (Base *)ptr->data;
  short mode = (base->flag & BASE_SELECTED) ? BA_SELECT : BA_DESELECT;
  ED_object_base_select(base, mode);
}

static void rna_ObjectBase_hide_viewport_update(bContext *C, PointerRNA *UNUSED(ptr))
{
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  BKE_layer_collection_sync(scene, view_layer);
  DEG_id_tag_update(&scene->id, ID_RECALC_BASE_FLAGS);
  WM_event_add_notifier(C, NC_SCENE | ND_OB_SELECT, scene);
}

static void rna_LayerCollection_name_get(struct PointerRNA *ptr, char *value)
{
  ID *id = (ID *)((LayerCollection *)ptr->data)->collection;
  BLI_strncpy(value, id->name + 2, sizeof(id->name) - 2);
}

int rna_LayerCollection_name_length(PointerRNA *ptr)
{
  ID *id = (ID *)((LayerCollection *)ptr->data)->collection;
  return strlen(id->name + 2);
}

static void rna_LayerCollection_exclude_update_recursive(ListBase *lb, const bool exclude)
{
  for (LayerCollection *lc = lb->first; lc; lc = lc->next) {
    if (exclude) {
      lc->flag |= LAYER_COLLECTION_EXCLUDE;
    }
    else {
      lc->flag &= ~LAYER_COLLECTION_EXCLUDE;
    }
    rna_LayerCollection_exclude_update_recursive(&lc->layer_collections, exclude);
  }
}

static void rna_LayerCollection_exclude_update(Main *bmain, Scene *UNUSED(scene), PointerRNA *ptr)
{
  Scene *scene = (Scene *)ptr->id.data;
  LayerCollection *lc = (LayerCollection *)ptr->data;
  ViewLayer *view_layer = BKE_view_layer_find_from_collection(scene, lc);

  /* Set/Unset it recursively to match the behaviour of excluding via the menu or shortcuts. */
  rna_LayerCollection_exclude_update_recursive(&lc->layer_collections,
                                               (lc->flag & LAYER_COLLECTION_EXCLUDE) != 0);

  BKE_layer_collection_sync(scene, view_layer);

  DEG_id_tag_update(&scene->id, ID_RECALC_BASE_FLAGS);
  DEG_relations_tag_update(bmain);
  WM_main_add_notifier(NC_SCENE | ND_LAYER_CONTENT, NULL);
}

static void rna_LayerCollection_update(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *ptr)
{
  Scene *scene = (Scene *)ptr->id.data;
  LayerCollection *lc = (LayerCollection *)ptr->data;
  ViewLayer *view_layer = BKE_view_layer_find_from_collection(scene, lc);

  BKE_layer_collection_sync(scene, view_layer);

  DEG_id_tag_update(&scene->id, ID_RECALC_BASE_FLAGS);

  WM_main_add_notifier(NC_SCENE | ND_LAYER_CONTENT, NULL);
}

static bool rna_LayerCollection_has_objects(LayerCollection *lc)
{
  return (lc->runtime_flag & LAYER_COLLECTION_HAS_OBJECTS) != 0;
}

static bool rna_LayerCollection_has_selected_objects(LayerCollection *lc, ViewLayer *view_layer)
{
  return BKE_layer_collection_has_selected_objects(view_layer, lc);
}

#else

static void rna_def_layer_collection(BlenderRNA *brna)
{
  StructRNA *srna;
  FunctionRNA *func;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "LayerCollection", NULL);
  RNA_def_struct_ui_text(srna, "Layer Collection", "Layer collection");
  RNA_def_struct_ui_icon(srna, ICON_GROUP);

  prop = RNA_def_property(srna, "collection", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE | PROP_ANIMATABLE);
  RNA_def_property_struct_type(prop, "Collection");
  RNA_def_property_ui_text(prop, "Collection", "Collection this layer collection is wrapping");

  prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, NULL, "collection->id.name");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE | PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Name", "Name of this view layer (same as its collection one)");
  RNA_def_property_string_funcs(
      prop, "rna_LayerCollection_name_get", "rna_LayerCollection_name_length", NULL);
  RNA_def_struct_name_property(srna, prop);

  prop = RNA_def_property(srna, "children", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, NULL, "layer_collections", NULL);
  RNA_def_property_struct_type(prop, "LayerCollection");
  RNA_def_property_ui_text(prop, "Children", "Child layer collections");

  /* Restriction flags. */
  prop = RNA_def_property(srna, "exclude", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", LAYER_COLLECTION_EXCLUDE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Exclude from View Layer", "Exclude from view layer");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_icon(prop, ICON_CHECKBOX_HLT, -1);
  RNA_def_property_update(prop, NC_SCENE | ND_LAYER, "rna_LayerCollection_exclude_update");

  prop = RNA_def_property(srna, "holdout", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", LAYER_COLLECTION_HOLDOUT);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_icon(prop, ICON_CLIPUV_HLT, -1);
  RNA_def_property_ui_text(prop, "Holdout", "Mask out objects in collection from view layer");
  RNA_def_property_update(prop, NC_SCENE | ND_LAYER, "rna_LayerCollection_update");

  prop = RNA_def_property(srna, "indirect_only", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", LAYER_COLLECTION_INDIRECT_ONLY);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_icon(prop, ICON_MOD_PHYSICS, 0);
  RNA_def_property_ui_text(
      prop,
      "Indirect Only",
      "Objects in collection only contribute indirectly (through shadows and reflections) "
      "in the view layer");
  RNA_def_property_update(prop, NC_SCENE | ND_LAYER, "rna_LayerCollection_update");

  prop = RNA_def_property(srna, "hide_viewport", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", LAYER_COLLECTION_HIDE);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_STATIC);
  RNA_def_property_ui_icon(prop, ICON_HIDE_OFF, -1);
  RNA_def_property_ui_text(prop, "Hide in Viewport", "Temporarily hide in viewport");
  RNA_def_property_update(prop, NC_SCENE | ND_LAYER_CONTENT, "rna_LayerCollection_update");

  /* Run-time flags. */
  prop = RNA_def_property(srna, "is_visible", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "runtime_flag", LAYER_COLLECTION_VISIBLE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(
      prop,
      "Visible",
      "Whether this collection is visible, take into account the collection parent");

  func = RNA_def_function(srna, "has_objects", "rna_LayerCollection_has_objects");
  RNA_def_function_ui_description(func, "");
  RNA_def_function_return(func, RNA_def_boolean(func, "result", 0, "", ""));

  func = RNA_def_function(
      srna, "has_selected_objects", "rna_LayerCollection_has_selected_objects");
  RNA_def_function_ui_description(func, "");
  prop = RNA_def_pointer(
      func, "view_layer", "ViewLayer", "", "ViewLayer the layer collection belongs to");
  RNA_def_parameter_flags(prop, 0, PARM_REQUIRED);
  RNA_def_function_return(func, RNA_def_boolean(func, "result", 0, "", ""));
}

static void rna_def_layer_objects(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;
  PropertyRNA *prop;

  RNA_def_property_srna(cprop, "LayerObjects");
  srna = RNA_def_struct(brna, "LayerObjects", NULL);
  RNA_def_struct_sdna(srna, "ViewLayer");
  RNA_def_struct_ui_text(srna, "Layer Objects", "Collections of objects");

  prop = RNA_def_property(srna, "active", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "Object");
  RNA_def_property_pointer_funcs(prop,
                                 "rna_LayerObjects_active_object_get",
                                 "rna_LayerObjects_active_object_set",
                                 NULL,
                                 NULL);
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_NEVER_UNLINK);
  RNA_def_property_ui_text(prop, "Active Object", "Active object for this layer");
  /* Could call: ED_object_base_activate(C, rl->basact);
   * but would be a bad level call and it seems the notifier is enough */
  RNA_def_property_update(prop, NC_SCENE | ND_OB_ACTIVE, NULL);

  prop = RNA_def_property(srna, "selected", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, NULL, "object_bases", NULL);
  RNA_def_property_struct_type(prop, "Object");
  RNA_def_property_collection_funcs(prop,
                                    "rna_LayerObjects_selected_begin",
                                    "rna_iterator_listbase_next",
                                    "rna_iterator_listbase_end",
                                    "rna_ViewLayer_objects_get",
                                    NULL,
                                    NULL,
                                    NULL,
                                    NULL);
  RNA_def_property_ui_text(prop, "Selected Objects", "All the selected objects of this layer");
}

static void rna_def_object_base(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "ObjectBase", NULL);
  RNA_def_struct_sdna(srna, "Base");
  RNA_def_struct_ui_text(srna, "Object Base", "An object instance in a render layer");
  RNA_def_struct_ui_icon(srna, ICON_OBJECT_DATA);

  prop = RNA_def_property(srna, "object", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, NULL, "object");
  RNA_def_property_ui_text(prop, "Object", "Object this base links to");

  prop = RNA_def_property(srna, "select", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", BASE_SELECTED);
  RNA_def_property_ui_text(prop, "Select", "Object base selection state");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, "rna_ObjectBase_select_update");

  prop = RNA_def_property(srna, "hide_viewport", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", BASE_HIDDEN);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_STATIC);
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

  srna = RNA_def_struct(brna, "ViewLayer", NULL);
  RNA_def_struct_ui_text(srna, "View Layer", "View layer");
  RNA_def_struct_ui_icon(srna, ICON_RENDER_RESULT);
  RNA_def_struct_path_func(srna, "rna_ViewLayer_path");
  RNA_def_struct_idprops_func(srna, "rna_ViewLayer_idprops");

  rna_def_view_layer_common(srna, true);

  func = RNA_def_function(srna, "update_render_passes", "rna_ViewLayer_update_render_passes");
  RNA_def_function_ui_description(func,
                                  "Requery the enabled render passes from the render engine");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_NO_SELF);

  prop = RNA_def_property(srna, "layer_collection", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "LayerCollection");
  RNA_def_property_pointer_sdna(prop, NULL, "layer_collections.first");
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_ui_text(
      prop,
      "Layer Collection",
      "Root of collections hierarchy of this view layer,"
      "its 'collection' pointer property is the same as the scene's master collection");

  prop = RNA_def_property(srna, "active_layer_collection", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "LayerCollection");
  RNA_def_property_pointer_funcs(prop,
                                 "rna_ViewLayer_active_layer_collection_get",
                                 "rna_ViewLayer_active_layer_collection_set",
                                 NULL,
                                 NULL);
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_NEVER_NULL);
  RNA_def_property_ui_text(
      prop, "Active Layer Collection", "Active layer collection in this view layer's hierarchy");
  RNA_def_property_update(prop, NC_SCENE | ND_LAYER, NULL);

  prop = RNA_def_property(srna, "objects", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, NULL, "object_bases", NULL);
  RNA_def_property_struct_type(prop, "Object");
  RNA_def_property_collection_funcs(
      prop, NULL, NULL, NULL, "rna_ViewLayer_objects_get", NULL, NULL, NULL, NULL);
  RNA_def_property_ui_text(prop, "Objects", "All the objects in this layer");
  rna_def_layer_objects(brna, prop);

  /* layer options */
  prop = RNA_def_property(srna, "use", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", VIEW_LAYER_RENDER);
  RNA_def_property_ui_text(prop, "Enabled", "Enable or disable rendering of this View Layer");
  RNA_def_property_update(prop, NC_SCENE | ND_LAYER, NULL);

  prop = RNA_def_property(srna, "use_freestyle", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", VIEW_LAYER_FREESTYLE);
  RNA_def_property_ui_text(prop, "Freestyle", "Render stylized strokes in this Layer");
  RNA_def_property_update(prop, NC_SCENE | ND_LAYER, NULL);

  /* Freestyle */
  rna_def_freestyle_settings(brna);

  prop = RNA_def_property(srna, "freestyle_settings", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_pointer_sdna(prop, NULL, "freestyle_config");
  RNA_def_property_struct_type(prop, "FreestyleSettings");
  RNA_def_property_ui_text(prop, "Freestyle Settings", "");

  /* debug update routine */
  func = RNA_def_function(srna, "update", "rna_ViewLayer_update_tagged");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_MAIN);
  RNA_def_function_ui_description(
      func, "Update data tagged to be updated from previous access to data or operators");

  /* Dependency Graph */
  prop = RNA_def_property(srna, "depsgraph", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "Depsgraph");
  RNA_def_property_ui_text(prop, "Dependency Graph", "Dependencies in the scene data");
  RNA_def_property_pointer_funcs(prop, "rna_ViewLayer_depsgraph_get", NULL, NULL, NULL);

  /* Nested Data  */
  /* *** Non-Animated *** */
  RNA_define_animate_sdna(false);
  rna_def_layer_collection(brna);
  rna_def_object_base(brna);
  RNA_define_animate_sdna(true);
  /* *** Animated *** */
}

#endif
