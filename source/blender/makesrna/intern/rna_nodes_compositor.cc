/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup RNA
 */

#include "BLT_translation.hh"

#include "BKE_node.hh"
#include "BKE_node_legacy_types.hh"

#include "IMB_colormanagement.hh"

#include "RNA_enum_types.hh"

#include "UI_interface_c.hh"

#include "rna_internal.hh"
#include "rna_nodetree_common.hh"

namespace blender {

static const EnumPropertyItem node_cryptomatte_layer_name_items[] = {
    {0, "CryptoObject", 0, "Object", "Use Object layer"},
    {1, "CryptoMaterial", 0, "Material", "Use Material layer"},
    {2, "CryptoAsset", 0, "Asset", "Use Asset layer"},
    {0, nullptr, 0, nullptr, nullptr},
};

}  // namespace blender

#ifdef RNA_RUNTIME

#  include "BKE_cryptomatte.hh"
#  include "BKE_image.hh"
#  include "BKE_layer.hh"

#  include "NOD_composite.hh"
#  include "NOD_compositor_file_output.hh"

#  include "RE_engine.h"

namespace blender {

using nodes::FileOutputItemsAccessor;

static void rna_Node_scene_set(PointerRNA *ptr, PointerRNA value, ReportList * /*reports*/)
{
  bNode *node = ptr->data_as<bNode>();

  if (node->id) {
    id_us_min(node->id);
    node->id = nullptr;
  }

  node->id = static_cast<ID *>(value.data);

  id_us_plus(node->id);
}

static void rna_Node_image_layer_view_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  bNode *node = ptr->data_as<bNode>();
  Image *ima = reinterpret_cast<Image *>(node->id);
  ImageUser *iuser = static_cast<ImageUser *>(node->storage);

  if (node->type_legacy == CMP_NODE_CRYPTOMATTE &&
      node->custom1 != CMP_NODE_CRYPTOMATTE_SOURCE_IMAGE)
  {
    return;
  }

  BKE_image_multilayer_index(ima->rr, iuser);
  BKE_image_signal(bmain, ima, iuser, IMA_SIGNAL_SRC_CHANGE);

  rna_Node_update(bmain, scene, ptr);
}

static const EnumPropertyItem *rna_Node_image_layer_itemf(bContext * /*C*/,
                                                          PointerRNA *ptr,
                                                          PropertyRNA * /*prop*/,
                                                          bool *r_free)
{
  bNode *node = ptr->data_as<bNode>();
  Image *ima = reinterpret_cast<Image *>(node->id);
  const EnumPropertyItem *item = nullptr;

  if (node->type_legacy == CMP_NODE_CRYPTOMATTE &&
      node->custom1 != CMP_NODE_CRYPTOMATTE_SOURCE_IMAGE)
  {
    return rna_enum_dummy_NULL_items;
  }

  if (ima == nullptr || ima->rr == nullptr) {
    *r_free = false;
    return rna_enum_dummy_NULL_items;
  }

  const auto &item_from_render_layer = [&](const RenderLayer &rl,
                                           const int index) -> EnumPropertyItem {
    EnumPropertyItem tmp = {0};
    tmp.identifier = rl.name;
    /* Little trick: using space char instead empty string
     * makes the item selectable in the drop-down. */
    if (rl.name[0] == '\0') {
      tmp.name = " ";
    }
    else {
      tmp.name = rl.name;
    }
    tmp.value = index;
    return tmp;
  };
  item = rna_enum_property_items_from_listbase(ima->rr->layers, item_from_render_layer);

  *r_free = true;

  return item;
}

static bool rna_Node_image_has_layers_get(PointerRNA *ptr)
{
  bNode *node = ptr->data_as<bNode>();
  Image *ima = reinterpret_cast<Image *>(node->id);

  if (node->type_legacy == CMP_NODE_CRYPTOMATTE &&
      node->custom1 != CMP_NODE_CRYPTOMATTE_SOURCE_IMAGE)
  {
    return false;
  }

  if (!ima || !(ima->rr)) {
    return false;
  }

  return RE_layers_have_name(ima->rr);
}

static bool rna_Node_image_has_views_get(PointerRNA *ptr)
{
  bNode *node = ptr->data_as<bNode>();
  Image *ima = reinterpret_cast<Image *>(node->id);

  if (node->type_legacy == CMP_NODE_CRYPTOMATTE &&
      node->custom1 != CMP_NODE_CRYPTOMATTE_SOURCE_IMAGE)
  {
    return false;
  }

  if (!ima || !(ima->rr)) {
    return false;
  }

  return BLI_listbase_count_at_most(&ima->rr->views, 2) > 1;
}

static const EnumPropertyItem *renderresult_views_add_enum(RenderView *rv)
{
  EnumPropertyItem *item = nullptr;
  EnumPropertyItem tmp = {0, "ALL", 0, "All", ""};
  int i = 1, totitem = 0;

  /* option to use all views */
  RNA_enum_item_add(&item, &totitem, &tmp);

  while (rv) {
    tmp.identifier = rv->name;
    /* Little trick: using space char instead empty string
     * makes the item selectable in the drop-down. */
    if (rv->name[0] == '\0') {
      tmp.name = " ";
    }
    else {
      tmp.name = rv->name;
    }
    tmp.value = i++;
    RNA_enum_item_add(&item, &totitem, &tmp);
    rv = rv->next;
  }

  RNA_enum_item_end(&item, &totitem);

  return item;
}

static const EnumPropertyItem *rna_Node_image_view_itemf(bContext * /*C*/,
                                                         PointerRNA *ptr,
                                                         PropertyRNA * /*prop*/,
                                                         bool *r_free)
{
  bNode *node = ptr->data_as<bNode>();
  Image *ima = reinterpret_cast<Image *>(node->id);
  const EnumPropertyItem *item = nullptr;
  RenderView *rv;

  if (node->type_legacy == CMP_NODE_CRYPTOMATTE &&
      node->custom1 != CMP_NODE_CRYPTOMATTE_SOURCE_IMAGE)
  {
    return rna_enum_dummy_NULL_items;
  }

  if (ima == nullptr || ima->rr == nullptr) {
    *r_free = false;
    return rna_enum_dummy_NULL_items;
  }

  rv = ima->rr->views.first();
  item = renderresult_views_add_enum(rv);

  *r_free = true;

  return item;
}

static const EnumPropertyItem *rna_Node_view_layer_itemf(bContext * /*C*/,
                                                         PointerRNA *ptr,
                                                         PropertyRNA * /*prop*/,
                                                         bool *r_free)
{
  bNode *node = ptr->data_as<bNode>();
  Scene *sce = reinterpret_cast<Scene *>(node->id);
  const EnumPropertyItem *item = nullptr;

  if (sce == nullptr) {
    *r_free = false;
    return rna_enum_dummy_NULL_items;
  }

  const auto &item_from_view_layer = [&](const ViewLayer &vl,
                                         const int index) -> EnumPropertyItem {
    EnumPropertyItem tmp = {0};
    tmp.identifier = vl.name;
    /* Little trick: using space char instead empty string
     * makes the item selectable in the drop-down. */
    if (vl.name[0] == '\0') {
      tmp.name = " ";
    }
    else {
      tmp.name = vl.name;
    }
    tmp.value = index;
    return tmp;
  };
  item = rna_enum_property_items_from_listbase(sce->view_layers, item_from_view_layer);

  *r_free = true;

  return item;
}

static void rna_Image_Node_update_id(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  bNode *node = ptr->data_as<bNode>();

  bke::node_tag_update_id(*node);
  rna_Node_update_relations(bmain, scene, ptr);
}

/* --------------------------------------------------------------------
 * White Balance Node.
 */

static void rna_NodeColorBalance_input_whitepoint_get(PointerRNA *ptr, float value[3])
{
  bNode *node = ptr->data_as<bNode>();
  bNodeSocket *temperature_input = bke::node_find_socket(*node, SOCK_IN, "Input Temperature"_ustr);
  bNodeSocket *tint_input = bke::node_find_socket(*node, SOCK_IN, "Input Tint"_ustr);
  IMB_colormanagement_get_whitepoint(
      temperature_input->default_value_typed<bNodeSocketValueFloat>()->value,
      tint_input->default_value_typed<bNodeSocketValueFloat>()->value,
      value);
}

static void rna_NodeColorBalance_input_whitepoint_set(PointerRNA *ptr, const float value[3])
{
  bNode *node = ptr->data_as<bNode>();
  bNodeSocket *temperature_input = bke::node_find_socket(*node, SOCK_IN, "Input Temperature"_ustr);
  bNodeSocket *tint_input = bke::node_find_socket(*node, SOCK_IN, "Input Tint"_ustr);
  IMB_colormanagement_set_whitepoint(
      value,
      temperature_input->default_value_typed<bNodeSocketValueFloat>()->value,
      tint_input->default_value_typed<bNodeSocketValueFloat>()->value);
}

static void rna_NodeColorBalance_output_whitepoint_get(PointerRNA *ptr, float value[3])
{
  bNode *node = ptr->data_as<bNode>();
  bNodeSocket *temperature_input = bke::node_find_socket(
      *node, SOCK_IN, "Output Temperature"_ustr);
  bNodeSocket *tint_input = bke::node_find_socket(*node, SOCK_IN, "Output Tint"_ustr);
  IMB_colormanagement_get_whitepoint(
      temperature_input->default_value_typed<bNodeSocketValueFloat>()->value,
      tint_input->default_value_typed<bNodeSocketValueFloat>()->value,
      value);
}

static void rna_NodeColorBalance_output_whitepoint_set(PointerRNA *ptr, const float value[3])
{
  bNode *node = ptr->data_as<bNode>();
  bNodeSocket *temperature_input = bke::node_find_socket(
      *node, SOCK_IN, "Output Temperature"_ustr);
  bNodeSocket *tint_input = bke::node_find_socket(*node, SOCK_IN, "Output Tint"_ustr);
  IMB_colormanagement_set_whitepoint(
      value,
      temperature_input->default_value_typed<bNodeSocketValueFloat>()->value,
      tint_input->default_value_typed<bNodeSocketValueFloat>()->value);
}

static void rna_NodeCryptomatte_source_set(PointerRNA *ptr, int value)
{
  bNode *node = ptr->data_as<bNode>();
  if (node->id && node->custom1 != value) {
    id_us_min(node->id);
    node->id = nullptr;
  }
  node->custom1 = value;
}

static int rna_NodeCryptomatte_layer_name_get(PointerRNA *ptr)
{

  bNode *node = ptr->data_as<bNode>();
  NodeCryptomatte *storage = static_cast<NodeCryptomatte *>(node->storage);
  for (const auto [index, layer] : storage->runtime.layers.enumerate()) {
    if (STREQLEN(storage->layer_name, layer.name, sizeof(storage->layer_name))) {
      return index;
    }
  }
  return 0;
}

static void rna_NodeCryptomatte_layer_name_set(PointerRNA *ptr, int new_value)
{
  bNode *node = ptr->data_as<bNode>();
  NodeCryptomatte *storage = static_cast<NodeCryptomatte *>(node->storage);

  CryptomatteLayer *layer = static_cast<CryptomatteLayer *>(
      BLI_findlink(&storage->runtime.layers, new_value));
  if (layer) {
    STRNCPY(storage->layer_name, layer->name);
  }
}

static const EnumPropertyItem *rna_NodeCryptomatte_layer_name_itemf(bContext * /*C*/,
                                                                    PointerRNA *ptr,
                                                                    PropertyRNA * /*prop*/,
                                                                    bool *r_free)
{
  bNode *node = ptr->data_as<bNode>();
  NodeCryptomatte *storage = static_cast<NodeCryptomatte *>(node->storage);
  EnumPropertyItem *item = nullptr;
  EnumPropertyItem temp = {0, "", 0, "", ""};
  int totitem = 0;

  for (const auto [layer_index, layer] : storage->runtime.layers.enumerate()) {
    temp.value = layer_index;
    temp.identifier = layer.name;
    temp.name = layer.name;
    RNA_enum_item_add(&item, &totitem, &temp);
  }

  RNA_enum_item_end(&item, &totitem);
  *r_free = true;

  return item;
}

static PointerRNA rna_NodeCryptomatte_scene_get(PointerRNA *ptr)
{
  bNode *node = ptr->data_as<bNode>();

  ID *scene = (node->custom1 == CMP_NODE_CRYPTOMATTE_SOURCE_RENDER) ? node->id : nullptr;
  return RNA_id_pointer_create(scene);
}

static void rna_NodeCryptomatte_scene_set(PointerRNA *ptr, PointerRNA value, ReportList *reports)
{
  bNode *node = ptr->data_as<bNode>();

  if (node->custom1 == CMP_NODE_CRYPTOMATTE_SOURCE_RENDER) {
    rna_Node_scene_set(ptr, value, reports);
  }
}

static PointerRNA rna_NodeCryptomatte_image_get(PointerRNA *ptr)
{
  bNode *node = ptr->data_as<bNode>();

  ID *image = (node->custom1 == CMP_NODE_CRYPTOMATTE_SOURCE_IMAGE) ? node->id : nullptr;
  return RNA_id_pointer_create(image);
}

static void rna_NodeCryptomatte_image_set(PointerRNA *ptr,
                                          PointerRNA value,
                                          ReportList * /*reports*/)
{
  bNode *node = ptr->data_as<bNode>();

  if (node->custom1 == CMP_NODE_CRYPTOMATTE_SOURCE_IMAGE) {
    if (node->id) {
      id_us_min(node->id);
    }
    if (value) {
      id_us_plus(static_cast<ID *>(value.data));
    }
    node->id = static_cast<ID *>(value.data);
  }
}

static bool rna_NodeCryptomatte_image_poll(PointerRNA * /*ptr*/, PointerRNA value)
{
  Image *image = reinterpret_cast<Image *>(value.owner_id);
  return image->type == IMA_TYPE_MULTILAYER;
}

static void rna_NodeCryptomatte_matte_get(PointerRNA *ptr, char *value)
{
  bNode *node = ptr->data_as<bNode>();
  NodeCryptomatte *nc = static_cast<NodeCryptomatte *>(node->storage);
  char *matte_id = BKE_cryptomatte_entries_to_matte_id(nc);
  strcpy(value, matte_id);
  MEM_delete(matte_id);
}

static int rna_NodeCryptomatte_matte_length(PointerRNA *ptr)
{
  bNode *node = ptr->data_as<bNode>();
  NodeCryptomatte *nc = static_cast<NodeCryptomatte *>(node->storage);
  char *matte_id = BKE_cryptomatte_entries_to_matte_id(nc);
  int result = strlen(matte_id);
  MEM_delete(matte_id);
  return result;
}

static void rna_NodeCryptomatte_matte_set(PointerRNA *ptr, const char *value)
{
  bNode *node = ptr->data_as<bNode>();
  NodeCryptomatte *nc = static_cast<NodeCryptomatte *>(node->storage);
  BKE_cryptomatte_matte_id_to_entries(nc, value);
}

static void rna_NodeCryptomatte_update_add(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  BKE_main_view_layers_synced_ensure(bmain);
  ntreeCompositCryptomatteSyncFromAdd(ptr->data_as<bNode>());
  rna_Node_update(bmain, scene, ptr);
}

static void rna_NodeCryptomatte_update_remove(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  ntreeCompositCryptomatteSyncFromRemove(ptr->data_as<bNode>());
  rna_Node_update(bmain, scene, ptr);
}

static int rna_NodeConvertColorSpace_from_color_space_get(PointerRNA *ptr)
{
  bNode *node = ptr->data_as<bNode>();
  NodeConvertColorSpace *node_storage = static_cast<NodeConvertColorSpace *>(node->storage);
  return IMB_colormanagement_colorspace_get_named_index(node_storage->from_color_space);
}

static void rna_NodeConvertColorSpace_from_color_space_set(PointerRNA *ptr, int value)
{
  bNode *node = ptr->data_as<bNode>();
  NodeConvertColorSpace *node_storage = static_cast<NodeConvertColorSpace *>(node->storage);
  const char *name = IMB_colormanagement_colorspace_get_indexed_name(value);

  if (name && name[0]) {
    IMB_colormanagement_colorspace_name_set(
        node_storage->from_color_space, node_storage->from_interop_id, name);
  }
}

static int rna_NodeConvertColorSpace_from_interop_id_get(PointerRNA *ptr)
{
  bNode *node = ptr->data_as<bNode>();
  NodeConvertColorSpace *node_storage = static_cast<NodeConvertColorSpace *>(node->storage);
  return IMB_colormanagement_colorspace_get_interop_id_index(node_storage->from_color_space,
                                                             node_storage->from_interop_id);
}

static void rna_NodeConvertColorSpace_from_interop_id_set(PointerRNA *ptr, int value)
{
  bNode *node = ptr->data_as<bNode>();
  NodeConvertColorSpace *node_storage = static_cast<NodeConvertColorSpace *>(node->storage);
  IMB_colormanagement_colorspace_interop_id_set(
      node_storage->from_color_space, node_storage->from_interop_id, value);
}

static int rna_NodeConvertColorSpace_to_color_space_get(PointerRNA *ptr)
{
  bNode *node = ptr->data_as<bNode>();
  NodeConvertColorSpace *node_storage = static_cast<NodeConvertColorSpace *>(node->storage);
  return IMB_colormanagement_colorspace_get_named_index(node_storage->to_color_space);
}

static void rna_NodeConvertColorSpace_to_color_space_set(PointerRNA *ptr, int value)
{
  bNode *node = ptr->data_as<bNode>();
  NodeConvertColorSpace *node_storage = static_cast<NodeConvertColorSpace *>(node->storage);
  const char *name = IMB_colormanagement_colorspace_get_indexed_name(value);

  if (name && name[0]) {
    IMB_colormanagement_colorspace_name_set(
        node_storage->to_color_space, node_storage->to_interop_id, name);
  }
}

static int rna_NodeConvertColorSpace_to_interop_id_get(PointerRNA *ptr)
{
  bNode *node = ptr->data_as<bNode>();
  NodeConvertColorSpace *node_storage = static_cast<NodeConvertColorSpace *>(node->storage);
  return IMB_colormanagement_colorspace_get_interop_id_index(node_storage->to_color_space,
                                                             node_storage->to_interop_id);
}

static void rna_NodeConvertColorSpace_to_interop_id_set(PointerRNA *ptr, int value)
{
  bNode *node = ptr->data_as<bNode>();
  NodeConvertColorSpace *node_storage = static_cast<NodeConvertColorSpace *>(node->storage);
  IMB_colormanagement_colorspace_interop_id_set(
      node_storage->to_color_space, node_storage->to_interop_id, value);
}

static const EnumPropertyItem *rna_NodeConvertColorSpace_interop_id_itemf(bContext * /*C*/,
                                                                          PointerRNA * /*ptr*/,
                                                                          PropertyRNA * /*prop*/,
                                                                          bool *r_free)
{
  EnumPropertyItem *items = nullptr;
  int totitem = 0;

  RNA_enum_items_add(&items, &totitem, rna_enum_color_space_interop_id_default_items);
  IMB_colormanagement_interop_id_items_add(&items, &totitem);
  RNA_enum_item_end(&items, &totitem);

  *r_free = true;

  return items;
}

static const EnumPropertyItem *rna_NodeConvertColorSpace_color_space_itemf(bContext * /*C*/,
                                                                           PointerRNA * /*ptr*/,
                                                                           PropertyRNA * /*prop*/,
                                                                           bool *r_free)
{
  EnumPropertyItem *items = nullptr;
  int totitem = 0;

  IMB_colormanagement_colorspace_items_add(&items, &totitem);
  RNA_enum_item_end(&items, &totitem);

  *r_free = true;

  return items;
}

}  // namespace blender

#else

namespace blender {

static const EnumPropertyItem prop_image_layer_items[] = {
    {0, "PLACEHOLDER", 0, "Placeholder", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

static const EnumPropertyItem prop_image_view_items[] = {
    {0, "ALL", 0, "All", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

static const EnumPropertyItem prop_view_layer_items[] = {
    {0, "PLACEHOLDER", 0, "Placeholder", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

static const EnumPropertyItem node_ycc_items[] = {
    {0, "ITUBT601", 0, "ITU 601", ""},
    {1, "ITUBT709", 0, "ITU 709", ""},
    {2, "JFIF", 0, "JPEG", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

static void def_node_image_user(BlenderRNA * /*brna*/, StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "frame_duration", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "frames");
  RNA_def_property_range(prop, 0, MAXFRAMEF);
  RNA_def_property_ui_text(
      prop, "Frames", "Number of images of a movie to use"); /* copied from the rna_image.cc */
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "frame_start", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "sfra");
  RNA_def_property_range(prop, MINAFRAMEF, MAXFRAMEF);
  /* copied from the rna_image.cc */
  RNA_def_property_ui_text(
      prop,
      "Start Frame",
      "Global starting frame of the movie/sequence, assuming first picture has a #1");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "frame_offset", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "offset");
  RNA_def_property_range(prop, MINAFRAMEF, MAXFRAMEF);
  /* copied from the rna_image.cc */
  RNA_def_property_ui_text(
      prop, "Offset", "Offset the number of the frame to use in the animation");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "use_cyclic", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "cycl", 1);
  RNA_def_property_ui_text(
      prop, "Cyclic", "Cycle the images in the movie"); /* copied from the rna_image.cc */
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "use_auto_refresh", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", IMA_ANIM_ALWAYS);
  /* copied from the rna_image.cc */
  RNA_def_property_ui_text(prop, "Auto-Refresh", "Always refresh image on frame changes");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "layer", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "layer");
  RNA_def_property_enum_items(prop, prop_image_layer_items);
  RNA_def_property_enum_funcs(prop, nullptr, nullptr, "rna_Node_image_layer_itemf");
  RNA_def_property_flag(prop, PROP_ENUM_NO_TRANSLATE);
  RNA_def_property_ui_text(prop, "Layer", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_image_layer_view_update");

  prop = RNA_def_property(srna, "has_layers", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(prop, "rna_Node_image_has_layers_get", nullptr);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Has Layers", "True if this image has any named layer");

  prop = RNA_def_property(srna, "view", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "view");
  RNA_def_property_enum_items(prop, prop_image_view_items);
  RNA_def_property_enum_funcs(prop, nullptr, nullptr, "rna_Node_image_view_itemf");
  RNA_def_property_flag(prop, PROP_ENUM_NO_TRANSLATE);
  RNA_def_property_ui_text(prop, "View", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_image_layer_view_update");

  prop = RNA_def_property(srna, "has_views", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(prop, "rna_Node_image_has_views_get", nullptr);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Has View", "True if this image has multiple views");
}

static void def_cmp_image(BlenderRNA *brna, StructRNA *srna)
{
  PropertyRNA *prop = RNA_def_property(srna, "image", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "id");
  RNA_def_property_struct_type(prop, "Image");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop, "Image", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Image_Node_update_id");
  RNA_def_property_pointer_funcs(
      prop, nullptr, nullptr, nullptr, "rna_Image_no_renderresult_or_viewer_poll");

  /* NOTE: Image user properties used in the UI are redefined in def_node_image_user,
   * to trigger correct updates of the node editor. RNA design problem that prevents
   * updates from nested structs. */
  RNA_def_struct_sdna_from(srna, "ImageUser", "storage");
  def_node_image_user(brna, srna);
}

static void def_cmp_render_layers(BlenderRNA * /*brna*/, StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "scene", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "id");
  RNA_def_property_pointer_funcs(prop, nullptr, "rna_Node_scene_set", nullptr, nullptr);
  RNA_def_property_struct_type(prop, "Scene");
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_REFCOUNT);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop, "Scene", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update_relations");

  prop = RNA_def_property(srna, "layer", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "custom1");
  RNA_def_property_enum_items(prop, prop_view_layer_items);
  RNA_def_property_enum_funcs(prop, nullptr, nullptr, "rna_Node_view_layer_itemf");
  RNA_def_property_flag(prop, PROP_ENUM_NO_TRANSLATE);
  RNA_def_property_ui_text(prop, "Layer", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update_relations");
}

static void rna_def_cmp_file_output_item(BlenderRNA *brna)
{
  StructRNA *srna = RNA_def_struct(brna, "NodeCompositorFileOutputItem", nullptr);
  RNA_def_struct_ui_text(srna, "File Output Item", "");

  rna_def_node_item_array_socket_item_common(srna, "FileOutputItemsAccessor", true, true);

  PropertyRNA *prop = RNA_def_property(srna, "override_node_format", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "override_node_format", 1);
  RNA_def_property_ui_text(prop,
                           "Override Node Format",
                           "Use a different format instead of the node format for this file");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, nullptr);

  prop = RNA_def_property(srna, "save_as_render", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "save_as_render", 1);
  RNA_def_property_ui_text(
      prop, "Save as Render", "Apply render part of display transform when saving byte image");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, nullptr);

  prop = RNA_def_property(srna, "format", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "ImageFormatSettings");
}

static void rna_def_cmp_file_output_items(BlenderRNA *brna)
{
  StructRNA *srna = RNA_def_struct(brna, "NodeCompositorFileOutputItems", nullptr);
  RNA_def_struct_sdna(srna, "bNode");
  RNA_def_struct_ui_text(srna, "Items", "Collection of file output items");

  rna_def_node_item_array_new_with_socket_and_name(
      srna, "NodeCompositorFileOutputItem", "FileOutputItemsAccessor");
  rna_def_node_item_array_common_functions(
      srna, "NodeCompositorFileOutputItem", "FileOutputItemsAccessor");
}

static void def_cmp_file_output(BlenderRNA *brna, StructRNA *srna)
{
  PropertyRNA *prop;

  rna_def_cmp_file_output_item(brna);
  rna_def_cmp_file_output_items(brna);

  RNA_def_struct_sdna_from(srna, "NodeCompositorFileOutput", "storage");

  prop = RNA_def_property(srna, "file_output_items", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, nullptr, "items", "items_count");
  RNA_def_property_struct_type(prop, "NodeCompositorFileOutputItem");
  RNA_def_property_ui_text(prop, "Items", "");
  RNA_def_property_srna(prop, "NodeCompositorFileOutputItems");

  prop = RNA_def_property(srna, "active_item_index", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, nullptr, "active_item_index");
  RNA_def_property_ui_text(prop, "Active Item Index", "Index of the active item");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_flag(prop, PROP_NO_DEG_UPDATE);
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, nullptr);

  prop = RNA_def_property(srna, "directory", PROP_STRING, PROP_DIRPATH);
  RNA_def_property_string_sdna(prop, nullptr, "directory");
  RNA_def_property_ui_text(prop, "Directory", "The directory where the image will be written");
  RNA_def_property_flag(
      prop, PROP_PATH_OUTPUT | PROP_PATH_SUPPORTS_BLEND_RELATIVE | PROP_PATH_SUPPORTS_TEMPLATES);
  RNA_def_property_path_template_type(prop, PROP_VARIABLES_RENDER_OUTPUT);
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "file_name", PROP_STRING, PROP_FILEPATH);
  RNA_def_property_string_sdna(prop, nullptr, "file_name");
  RNA_def_property_ui_text(prop,
                           "File Name",
                           "The base name of the file. Other information might be included in the "
                           "final file name depending on the node options");
  RNA_def_property_flag(prop, PROP_PATH_OUTPUT | PROP_PATH_SUPPORTS_TEMPLATES);
  RNA_def_property_path_template_type(prop, PROP_VARIABLES_RENDER_OUTPUT);
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "format", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "ImageFormatSettings");

  prop = RNA_def_property(srna, "save_as_render", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "save_as_render", 1);
  RNA_def_property_boolean_default(prop, true);
  RNA_def_property_ui_text(
      prop, "Save as Render", "Apply render part of display transform when saving byte image");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, nullptr);

  prop = RNA_def_property(srna, "use_file_extension", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "use_file_extension", 1);
  RNA_def_property_boolean_default(prop, true);
  RNA_def_property_ui_text(
      prop,
      "File Extensions",
      "Add the file format extensions to the saved file name (eg: filename + .jpg)");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, nullptr);
}

static void def_cmp_convert_color_space(BlenderRNA * /*brna*/, StructRNA *srna)
{
  PropertyRNA *prop;
  RNA_def_struct_sdna_from(srna, "NodeConvertColorSpace", "storage");

  prop = RNA_def_property(srna, "from_color_space", PROP_ENUM, PROP_NONE);
  RNA_def_property_flag(prop, PROP_ENUM_NO_CONTEXT);
  RNA_def_property_enum_items(prop, rna_enum_color_space_convert_default_items);
  RNA_def_property_enum_funcs(prop,
                              "rna_NodeConvertColorSpace_from_color_space_get",
                              "rna_NodeConvertColorSpace_from_color_space_set",
                              "rna_NodeConvertColorSpace_color_space_itemf");
  RNA_def_property_ui_text(prop, "From", "Color space of the input image");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_COLOR_MANAGEMENT);
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "from_interop_id", PROP_ENUM, PROP_NONE);
  RNA_def_property_flag(prop, PROP_ENUM_NO_CONTEXT);
  RNA_def_property_enum_items(prop, rna_enum_color_space_interop_id_default_items);
  RNA_def_property_enum_funcs(prop,
                              "rna_NodeConvertColorSpace_from_interop_id_get",
                              "rna_NodeConvertColorSpace_from_interop_id_set",
                              "rna_NodeConvertColorSpace_interop_id_itemf");
  RNA_def_property_ui_text(prop, "From Interop ID", "Interop ID of the input color space");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "to_color_space", PROP_ENUM, PROP_NONE);
  RNA_def_property_flag(prop, PROP_ENUM_NO_CONTEXT);
  RNA_def_property_enum_items(prop, rna_enum_color_space_convert_default_items);
  RNA_def_property_enum_funcs(prop,
                              "rna_NodeConvertColorSpace_to_color_space_get",
                              "rna_NodeConvertColorSpace_to_color_space_set",
                              "rna_NodeConvertColorSpace_color_space_itemf");
  RNA_def_property_ui_text(prop, "To", "Color space of the output image");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_COLOR_MANAGEMENT);
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "to_interop_id", PROP_ENUM, PROP_NONE);
  RNA_def_property_flag(prop, PROP_ENUM_NO_CONTEXT);
  RNA_def_property_enum_items(prop, rna_enum_color_space_interop_id_default_items);
  RNA_def_property_enum_funcs(prop,
                              "rna_NodeConvertColorSpace_to_interop_id_get",
                              "rna_NodeConvertColorSpace_to_interop_id_set",
                              "rna_NodeConvertColorSpace_interop_id_itemf");
  RNA_def_property_ui_text(prop, "To Interop ID", "Interop ID of the output color space");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_cmp_convert_to_display(BlenderRNA * /*brna*/, StructRNA *srna)
{
  PropertyRNA *prop;
  RNA_def_struct_sdna_from(srna, "NodeConvertToDisplay", "storage");

  prop = RNA_def_property(srna, "view_settings", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "view_settings");
  RNA_def_property_struct_type(prop, "ColorManagedViewSettings");
  RNA_def_property_ui_text(prop, "View Settings", "Color management view transform settings");

  prop = RNA_def_property(srna, "display_settings", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "display_settings");
  RNA_def_property_struct_type(prop, "ColorManagedDisplaySettings");
  RNA_def_property_ui_text(prop, "Display Settings", "Color management display device settings");
}

static void def_cmp_defocus(BlenderRNA * /*brna*/, StructRNA *srna)
{
  PropertyRNA *prop;

  static const EnumPropertyItem bokeh_items[] = {
      {8, "OCTAGON", 0, "Octagonal", "8 sides"},
      {7, "HEPTAGON", 0, "Heptagonal", "7 sides"},
      {6, "HEXAGON", 0, "Hexagonal", "6 sides"},
      {5, "PENTAGON", 0, "Pentagonal", "5 sides"},
      {4, "SQUARE", 0, "Square", "4 sides"},
      {3, "TRIANGLE", 0, "Triangular", "3 sides"},
      {0, "CIRCLE", 0, "Circular", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  prop = RNA_def_property(srna, "scene", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "id");
  RNA_def_property_pointer_funcs(prop, nullptr, "rna_Node_scene_set", nullptr, nullptr);
  RNA_def_property_struct_type(prop, "Scene");
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_REFCOUNT);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(
      prop, "Scene", "Scene from which to select the active camera (render scene if undefined)");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update_relations");

  RNA_def_struct_sdna_from(srna, "NodeDefocus", "storage");

  prop = RNA_def_property(srna, "bokeh", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "bktype");
  RNA_def_property_enum_items(prop, bokeh_items);
  RNA_def_property_ui_text(prop, "Bokeh Type", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "angle", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_float_sdna(prop, nullptr, "rotation");
  RNA_def_property_range(prop, 0.0f, DEG2RADF(90.0f));
  RNA_def_property_ui_text(prop, "Angle", "Bokeh shape rotation offset");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "f_stop", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "fstop");
  RNA_def_property_range(prop, 0.0f, 128.0f);
  RNA_def_property_float_default(prop, 128.0f);
  RNA_def_property_ui_text(
      prop,
      "F-Stop",
      "Amount of focal blur, 128 (infinity) is perfect focus, half the value doubles "
      "the blur radius");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "blur_max", PROP_FLOAT, PROP_PIXEL);
  RNA_def_property_float_sdna(prop, nullptr, "maxblur");
  RNA_def_property_range(prop, 0.0f, 10000.0f);
  RNA_def_property_float_default(prop, 16.0f);
  RNA_def_property_ui_text(prop, "Max Blur", "Blur limit, maximum CoC radius");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "use_zbuffer", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "no_zbuf", 1);
  RNA_def_property_ui_text(prop,
                           "Use Z-Buffer",
                           "Disable when using an image as input instead of actual z-buffer "
                           "(auto enabled if node not image based, eg. time node)");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "z_scale", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "scale");
  RNA_def_property_range(prop, 0.0f, 1000.0f);
  RNA_def_property_float_default(prop, 1.0f);
  RNA_def_property_ui_text(
      prop,
      "Z-Scale",
      "Scale the Z input when not using a z-buffer, controls maximum blur designated "
      "by the color white or input value 1");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_cmp_colorbalance(BlenderRNA * /*brna*/, StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "input_whitepoint", PROP_FLOAT, PROP_COLOR);
  RNA_def_property_array(prop, 3);
  RNA_def_property_float_funcs(prop,
                               "rna_NodeColorBalance_input_whitepoint_get",
                               "rna_NodeColorBalance_input_whitepoint_set",
                               nullptr);
  RNA_def_property_ui_text(prop,
                           "Input White Point",
                           "The color which gets mapped to white "
                           "(automatically converted to/from temperature and tint)");
  RNA_def_property_update(prop, NC_WINDOW, "rna_Node_update");

  prop = RNA_def_property(srna, "output_whitepoint", PROP_FLOAT, PROP_COLOR);
  RNA_def_property_array(prop, 3);
  RNA_def_property_float_funcs(prop,
                               "rna_NodeColorBalance_output_whitepoint_get",
                               "rna_NodeColorBalance_output_whitepoint_set",
                               nullptr);
  RNA_def_property_ui_text(prop,
                           "Output White Point",
                           "The color which gets white gets mapped to "
                           "(automatically converted to/from temperature and tint)");
  RNA_def_property_update(prop, NC_WINDOW, "rna_Node_update");
}

static void def_cmp_huecorrect(BlenderRNA * /*brna*/, StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "mapping", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "storage");
  RNA_def_property_struct_type(prop, "CurveMapping");
  RNA_def_property_ui_text(prop, "Mapping", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_cmp_combsep_color(BlenderRNA * /*brna*/, StructRNA *srna)
{
  static const EnumPropertyItem mode_items[] = {
      {CMP_NODE_COMBSEP_COLOR_RGB,
       "RGB",
       ICON_NONE,
       "RGB",
       "Use RGB (Red, Green, Blue) color processing"},
      {CMP_NODE_COMBSEP_COLOR_HSV,
       "HSV",
       ICON_NONE,
       "HSV",
       "Use HSV (Hue, Saturation, Value) color processing"},
      {CMP_NODE_COMBSEP_COLOR_HSL,
       "HSL",
       ICON_NONE,
       "HSL",
       "Use HSL (Hue, Saturation, Lightness) color processing"},
      {CMP_NODE_COMBSEP_COLOR_YCC,
       "YCC",
       ICON_NONE,
       "YCbCr",
       "Use YCbCr (Y - luma, Cb - blue-difference chroma, Cr - red-difference chroma) color "
       "processing"},
      {CMP_NODE_COMBSEP_COLOR_YUV,
       "YUV",
       ICON_NONE,
       "YUV",
       "Use YUV (Y - luma, U V - chroma) color processing"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  PropertyRNA *prop;

  RNA_def_struct_sdna_from(srna, "NodeCMPCombSepColor", "storage");

  prop = RNA_def_property(srna, "mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, mode_items);
  RNA_def_property_ui_text(prop, "Mode", "Mode of color processing");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_socket_update");

  prop = RNA_def_property(srna, "ycc_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, node_ycc_items);
  RNA_def_property_enum_default(prop, 1); /* ITU BT.709 */
  RNA_def_property_ui_text(prop, "Color Space", "Color space used for YCbCrA processing");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_cmp_movieclip(BlenderRNA * /*brna*/, StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "clip", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "id");
  RNA_def_property_struct_type(prop, "MovieClip");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop, "Movie Clip", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update_relations");

  RNA_def_struct_sdna_from(srna, "MovieClipUser", "storage");
}

static void def_cmp_stabilize2d(BlenderRNA * /*brna*/, StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "clip", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "id");
  RNA_def_property_struct_type(prop, "MovieClip");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop, "Movie Clip", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update_relations");
}

static void def_cmp_moviedistortion(BlenderRNA * /*brna*/, StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "clip", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "id");
  RNA_def_property_struct_type(prop, "MovieClip");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop, "Movie Clip", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update_relations");
}

static void def_cmp_mask(BlenderRNA * /*brna*/, StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "mask", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "id");
  RNA_def_property_struct_type(prop, "Mask");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop, "Mask", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update_relations");
}

static void def_cmp_viewer(BlenderRNA * /*brna*/, StructRNA *srna)
{
  PropertyRNA *prop = RNA_def_property(srna, "ui_shortcut", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "custom1");
  RNA_def_property_int_funcs(prop, nullptr, "rna_Node_Viewer_shortcut_node_set", nullptr);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_IGNORE);
  RNA_def_property_int_default(prop, NODE_VIEWER_SHORTCUT_NONE);
  RNA_def_property_update(prop, NC_NODE | ND_DISPLAY, nullptr);
}

static void def_cmp_keyingscreen(BlenderRNA * /*brna*/, StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "clip", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "id");
  RNA_def_property_struct_type(prop, "MovieClip");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop, "Movie Clip", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update_relations");

  RNA_def_struct_sdna_from(srna, "NodeKeyingScreenData", "storage");

  prop = RNA_def_property(srna, "tracking_object", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "tracking_object");
  RNA_def_property_ui_text(prop, "Tracking Object", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_cmp_trackpos(BlenderRNA * /*brna*/, StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "clip", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "id");
  RNA_def_property_struct_type(prop, "MovieClip");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop, "Movie Clip", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update_relations");

  RNA_def_struct_sdna_from(srna, "NodeTrackPosData", "storage");

  prop = RNA_def_property(srna, "tracking_object", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "tracking_object");
  RNA_def_property_ui_text(prop, "Tracking Object", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "track_name", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "track_name");
  RNA_def_property_ui_text(prop, "Track", "");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_MOVIECLIP);
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_cmp_planetrackdeform(BlenderRNA * /*brna*/, StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "clip", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "id");
  RNA_def_property_struct_type(prop, "MovieClip");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop, "Movie Clip", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update_relations");

  RNA_def_struct_sdna_from(srna, "NodePlaneTrackDeformData", "storage");

  prop = RNA_def_property(srna, "tracking_object", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "tracking_object");
  RNA_def_property_ui_text(prop, "Tracking Object", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "plane_track_name", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "plane_track_name");
  RNA_def_property_ui_text(prop, "Plane Track", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");
}

static void def_cmp_cryptomatte_common(BlenderRNA * /*brna*/, StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "matte_id", PROP_STRING, PROP_NONE);
  RNA_def_property_string_funcs(prop,
                                "rna_NodeCryptomatte_matte_get",
                                "rna_NodeCryptomatte_matte_length",
                                "rna_NodeCryptomatte_matte_set");
  RNA_def_property_ui_text(
      prop, "Matte Objects", "List of object and material crypto IDs to include in matte");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "add", PROP_FLOAT, PROP_COLOR);
  RNA_def_property_float_sdna(prop, nullptr, "runtime.add");
  RNA_def_property_range(prop, -FLT_MAX, FLT_MAX);
  RNA_def_property_ui_text(
      prop, "Add", "Add object or material to matte, by picking a color from the Pick output");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_OPERATOR_DEFAULT);
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeCryptomatte_update_add");

  prop = RNA_def_property(srna, "remove", PROP_FLOAT, PROP_COLOR);
  RNA_def_property_float_sdna(prop, nullptr, "runtime.remove");
  RNA_def_property_range(prop, -FLT_MAX, FLT_MAX);
  RNA_def_property_ui_text(
      prop,
      "Remove",
      "Remove object or material from matte, by picking a color from the Pick output");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_OPERATOR_DEFAULT);
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_NodeCryptomatte_update_remove");
}

static void def_cmp_cryptomatte_legacy(BlenderRNA *brna, StructRNA *srna)
{
  RNA_def_struct_sdna_from(srna, "NodeCryptomatte", "storage");
  def_cmp_cryptomatte_common(brna, srna);
}

static void def_cmp_cryptomatte(BlenderRNA *brna, StructRNA *srna)
{
  PropertyRNA *prop;

  static const EnumPropertyItem cryptomatte_source_items[] = {
      {CMP_NODE_CRYPTOMATTE_SOURCE_RENDER,
       "RENDER",
       0,
       "Render",
       "Use Cryptomatte passes from a render"},
      {CMP_NODE_CRYPTOMATTE_SOURCE_IMAGE,
       "IMAGE",
       0,
       "Image",
       "Use Cryptomatte passes from an image"},
      {0, nullptr, 0, nullptr, nullptr}};

  prop = RNA_def_property(srna, "source", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "custom1");
  RNA_def_property_enum_items(prop, cryptomatte_source_items);
  RNA_def_property_enum_funcs(prop, nullptr, "rna_NodeCryptomatte_source_set", nullptr);
  RNA_def_property_ui_text(prop, "Source", "Where the Cryptomatte passes are loaded from");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update_relations");

  prop = RNA_def_property(srna, "scene", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_funcs(
      prop, "rna_NodeCryptomatte_scene_get", "rna_NodeCryptomatte_scene_set", nullptr, nullptr);
  RNA_def_property_struct_type(prop, "Scene");
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_REFCOUNT);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop, "Scene", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update_relations");

  prop = RNA_def_property(srna, "image", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_funcs(prop,
                                 "rna_NodeCryptomatte_image_get",
                                 "rna_NodeCryptomatte_image_set",
                                 nullptr,
                                 "rna_NodeCryptomatte_image_poll");
  RNA_def_property_struct_type(prop, "Image");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop, "Image", "");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update_relations");

  RNA_def_struct_sdna_from(srna, "NodeCryptomatte", "storage");
  def_cmp_cryptomatte_common(brna, srna);

  prop = RNA_def_property(srna, "layer_name", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, node_cryptomatte_layer_name_items);
  RNA_def_property_enum_funcs(prop,
                              "rna_NodeCryptomatte_layer_name_get",
                              "rna_NodeCryptomatte_layer_name_set",
                              "rna_NodeCryptomatte_layer_name_itemf");
  RNA_def_property_ui_text(prop, "Cryptomatte Layer", "What Cryptomatte layer is used");
  RNA_def_property_update(prop, NC_NODE | NA_EDITED, "rna_Node_update");

  prop = RNA_def_property(srna, "entries", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, nullptr, "entries", nullptr);
  RNA_def_property_struct_type(prop, "CryptomatteEntry");
  RNA_def_property_ui_text(prop, "Mattes", "");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);

  /* Included here instead of defining image_user as a property of the node,
   * see def_cmp_image for details.
   * As mentioned in DNA_node_types.h, iuser is the first member of the Cryptomatte
   * storage type, so we can cast node->storage to ImageUser.
   * That is required since we can't define the properties from storage->iuser directly... */
  RNA_def_struct_sdna_from(srna, "ImageUser", "storage");
  def_node_image_user(brna, srna);
}

void RNA_def_nodes_compositor(BlenderRNA *brna)
{
  const auto define = [&](const char *base_name,
                          const char *struct_name,
                          void (*func)(BlenderRNA *, StructRNA *) = nullptr) {
    StructRNA *srna = define_specific_node(brna, struct_name, base_name);
    if (func) {
      func(brna, srna);
    }
  };

  /* clang-format off */

  define("CompositorNode", "CompositorNodeAlphaOver");
  define("CompositorNode", "CompositorNodeAntiAliasing");
  define("CompositorNode", "CompositorNodeBilateralblur");
  define("CompositorNode", "CompositorNodeBlankImage");
  define("CompositorNode", "CompositorNodeBlur");
  define("CompositorNode", "CompositorNodeBokehBlur");
  define("CompositorNode", "CompositorNodeBokehImage");
  define("CompositorNode", "CompositorNodeBoxMask");
  define("CompositorNode", "CompositorNodeBrightContrast");
  define("CompositorNode", "CompositorNodeChannelMatte");
  define("CompositorNode", "CompositorNodeChromaMatte");
  define("CompositorNode", "CompositorNodeColorBalance", def_cmp_colorbalance);
  define("CompositorNode", "CompositorNodeColorCorrection");
  define("CompositorNode", "CompositorNodeColorMatte");
  define("CompositorNode", "CompositorNodeColorSpill");
  define("CompositorNode", "CompositorNodeConvolve");
  define("CompositorNode", "CompositorNodeCombineColor", def_cmp_combsep_color);
  define("CompositorNode", "CompositorNodeConvertColorSpace", def_cmp_convert_color_space);
  define("CompositorNode", "CompositorNodeConvertToDisplay", def_cmp_convert_to_display);
  define("CompositorNode", "CompositorNodeCornerPin");
  define("CompositorNode", "CompositorNodeCrop");
  define("CompositorNode", "CompositorNodeCryptomatte", def_cmp_cryptomatte_legacy);
  define("CompositorNode", "CompositorNodeCryptomatteV2", def_cmp_cryptomatte);
  define("CompositorNode", "CompositorNodeCurveRGB", def_rgb_curve);
  define("CompositorNode", "CompositorNodeDBlur");
  define("CompositorNode", "CompositorNodeDefocus", def_cmp_defocus);
  define("CompositorNode", "CompositorNodeDenoise");
  define("CompositorNode", "CompositorNodeDespeckle");
  define("CompositorNode", "CompositorNodeDiffMatte");
  define("CompositorNode", "CompositorNodeDilateErode");
  define("CompositorNode", "CompositorNodeDisplace");
  define("CompositorNode", "CompositorNodeDistanceMatte");
  define("CompositorNode", "CompositorNodeDoubleEdgeMask");
  define("CompositorNode", "CompositorNodeEllipseMask");
  define("CompositorNode", "CompositorNodeExposure");
  define("CompositorNode", "CompositorNodeFilter");
  define("CompositorNode", "CompositorNodeFlip");
  define("CompositorNode", "CompositorNodeGamma");
  define("CompositorNode", "CompositorNodeGlare");
  define("CompositorNode", "CompositorNodeHueCorrect", def_cmp_huecorrect);
  define("CompositorNode", "CompositorNodeHueSat");
  define("CompositorNode", "CompositorNodeIDMask");
  define("CompositorNode", "CompositorNodeImage", def_cmp_image);
  define("CompositorNode", "CompositorNodeImageCoordinates");
  define("CompositorNode", "CompositorNodeImageInfo");
  define("CompositorNode", "CompositorNodeInpaint");
  define("CompositorNode", "CompositorNodeInvert");
  define("CompositorNode", "CompositorNodeKeying");
  define("CompositorNode", "CompositorNodeKeyingScreen", def_cmp_keyingscreen);
  define("CompositorNode", "CompositorNodeKuwahara");
  define("CompositorNode", "CompositorNodeLensdist");
  define("CompositorNode", "CompositorNodeLevels");
  define("CompositorNode", "CompositorNodeLumaMatte");
  define("CompositorNode", "CompositorNodeMapUV");
  define("CompositorNode", "CompositorNodeMask", def_cmp_mask);
  define("CompositorNode", "CompositorNodeMovieClip", def_cmp_movieclip);
  define("CompositorNode", "CompositorNodeMovieDistortion", def_cmp_moviedistortion);
  define("CompositorNode", "CompositorNodeNormal");
  define("CompositorNode", "CompositorNodeNormalize");
  define("CompositorNode", "CompositorNodeOutputFile", def_cmp_file_output);
  define("CompositorNode", "CompositorNodePixelate");
  define("CompositorNode", "CompositorNodePlaneTrackDeform", def_cmp_planetrackdeform);
  define("CompositorNode", "CompositorNodePosterize");
  define("CompositorNode", "CompositorNodePremulKey");
  define("CompositorNode", "CompositorNodeRelativeToPixel");
  define("CompositorNode", "CompositorNodeRGB");
  define("CompositorNode", "CompositorNodeRGBToBW");
  define("CompositorNode", "CompositorNodeRLayers", def_cmp_render_layers);
  define("CompositorNode", "CompositorNodeRotate");
  define("CompositorNode", "CompositorNodeScale");
  define("CompositorNode", "CompositorNodeSceneTime");
  define("CompositorNode", "CompositorNodeSeparateColor", def_cmp_combsep_color);
  define("CompositorNode", "CompositorNodeSetAlpha");
  define("CompositorNode", "CompositorNodeMaskToSDF");
  define("CompositorNode", "CompositorNodeSplit");
  define("CompositorNode", "CompositorNodeStabilize", def_cmp_stabilize2d);
  define("CompositorNode", "CompositorNodeSequencerStripInfo");
  define("CompositorNode", "CompositorNodeStringToImage");
  define("CompositorNode", "CompositorNodeSwitch");
  define("CompositorNode", "CompositorNodeSwitchView");
  define("CompositorNode", "CompositorNodeTime", def_time);
  define("CompositorNode", "CompositorNodeTonemap");
  define("CompositorNode", "CompositorNodeTrackPos", def_cmp_trackpos);
  define("CompositorNode", "CompositorNodeTransform");
  define("CompositorNode", "CompositorNodeTranslate");
  define("CompositorNode", "CompositorNodeVecBlur");
  define("CompositorNode", "CompositorNodeViewer", def_cmp_viewer);
  define("CompositorNode", "CompositorNodeZcombine");
  define("CompositorNode", "CompositorNodeGroup", def_group);

  /* clang-format on */
}

}  // namespace blender

#endif
