/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup RNA
 */

#include "BLI_string.h"

#include "DNA_grease_pencil_types.h"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_enum_types.hh"

#include "rna_internal.hh"

#include "WM_api.hh"

#ifdef RNA_RUNTIME

#  include <fmt/format.h>

#  include "BKE_attribute.hh"
#  include "BKE_grease_pencil.hh"

#  include "BLI_span.hh"

#  include "DEG_depsgraph.hh"
#  include "DEG_depsgraph_build.hh"

static GreasePencil *rna_grease_pencil(const PointerRNA *ptr)
{
  return reinterpret_cast<GreasePencil *>(ptr->owner_id);
}

static void rna_grease_pencil_update(Main * /*bmain*/, Scene * /*scene*/, PointerRNA *ptr)
{
  DEG_id_tag_update(&rna_grease_pencil(ptr)->id, ID_RECALC_GEOMETRY);
  WM_main_add_notifier(NC_GPENCIL | NA_EDITED, rna_grease_pencil(ptr));
}

static void rna_grease_pencil_autolock(Main * /*bmain*/, Scene * /*scene*/, PointerRNA *ptr)
{
  using namespace blender::bke::greasepencil;
  GreasePencil *grease_pencil = rna_grease_pencil(ptr);
  if (grease_pencil->flag & GREASE_PENCIL_AUTOLOCK_LAYERS) {
    grease_pencil->autolock_inactive_layers();
  }
  else {
    for (Layer *layer : grease_pencil->layers_for_write()) {
      layer->set_locked(false);
    }
  }

  rna_grease_pencil_update(nullptr, nullptr, ptr);
}

static void rna_grease_pencil_dependency_update(Main *bmain, Scene * /*scene*/, PointerRNA *ptr)
{
  DEG_id_tag_update(&rna_grease_pencil(ptr)->id, ID_RECALC_GEOMETRY);
  DEG_relations_tag_update(bmain);
  WM_main_add_notifier(NC_GPENCIL | NA_EDITED, rna_grease_pencil(ptr));
}

static void rna_grease_pencil_layer_mask_name_get(PointerRNA *ptr, char *dst)
{
  using namespace blender;
  GreasePencilLayerMask *mask = static_cast<GreasePencilLayerMask *>(ptr->data);
  if (mask->layer_name != nullptr) {
    strcpy(dst, mask->layer_name);
  }
  else {
    dst[0] = '\0';
  }
}

static int rna_grease_pencil_layer_mask_name_length(PointerRNA *ptr)
{
  using namespace blender;
  GreasePencilLayerMask *mask = static_cast<GreasePencilLayerMask *>(ptr->data);
  if (mask->layer_name != nullptr) {
    return strlen(mask->layer_name);
  }
  return 0;
}

static void rna_grease_pencil_layer_mask_name_set(PointerRNA *ptr, const char *value)
{
  using namespace blender;
  GreasePencil *grease_pencil = rna_grease_pencil(ptr);
  GreasePencilLayerMask *mask = static_cast<GreasePencilLayerMask *>(ptr->data);

  const std::string oldname(mask->layer_name);
  if (bke::greasepencil::TreeNode *node = grease_pencil->find_node_by_name(oldname)) {
    grease_pencil->rename_node(*node, value);
  }
}

static int rna_grease_pencil_active_mask_index_get(PointerRNA *ptr)
{
  GreasePencilLayer *layer = static_cast<GreasePencilLayer *>(ptr->data);
  return layer->active_mask_index;
}

static void rna_grease_pencil_active_mask_index_set(PointerRNA *ptr, int value)
{
  GreasePencilLayer *layer = static_cast<GreasePencilLayer *>(ptr->data);
  layer->active_mask_index = value;
}

static void rna_grease_pencil_active_mask_index_range(
    PointerRNA *ptr, int *min, int *max, int * /*softmin*/, int * /*softmax*/)
{
  GreasePencilLayer *layer = static_cast<GreasePencilLayer *>(ptr->data);
  *min = 0;
  *max = max_ii(0, BLI_listbase_count(&layer->masks) - 1);
}

static void rna_iterator_grease_pencil_layers_begin(CollectionPropertyIterator *iter,
                                                    PointerRNA *ptr)
{
  using namespace blender::bke::greasepencil;
  GreasePencil *grease_pencil = rna_grease_pencil(ptr);
  blender::Span<Layer *> layers = grease_pencil->layers_for_write();

  rna_iterator_array_begin(
      iter, (void *)layers.data(), sizeof(Layer *), layers.size(), 0, nullptr);
}

static int rna_iterator_grease_pencil_layers_length(PointerRNA *ptr)
{
  GreasePencil *grease_pencil = rna_grease_pencil(ptr);
  return grease_pencil->layers().size();
}

static void tree_node_name_get(blender::bke::greasepencil::TreeNode &node, char *dst)
{
  if (!node.name().is_empty()) {
    strcpy(dst, node.name().c_str());
  }
  else {
    dst[0] = '\0';
  }
}

static int tree_node_name_length(blender::bke::greasepencil::TreeNode &node)
{
  if (!node.name().is_empty()) {
    return node.name().size();
  }
  return 0;
}

static std::optional<std::string> tree_node_name_path(blender::bke::greasepencil::TreeNode &node,
                                                      const char *prefix)
{
  using namespace blender::bke::greasepencil;
  BLI_assert(!node.name().is_empty());
  const size_t name_length = node.name().size();
  std::string name_esc(name_length * 2, '\0');
  BLI_str_escape(name_esc.data(), node.name().c_str(), name_length * 2);
  return fmt::format("{}[\"{}\"]", prefix, name_esc.c_str());
}

static std::optional<std::string> rna_GreasePencilLayer_path(const PointerRNA *ptr)
{
  GreasePencilLayer *layer = static_cast<GreasePencilLayer *>(ptr->data);
  return tree_node_name_path(layer->wrap().as_node(), "layers");
}

static void rna_GreasePencilLayer_name_get(PointerRNA *ptr, char *value)
{
  GreasePencilLayer *layer = static_cast<GreasePencilLayer *>(ptr->data);
  tree_node_name_get(layer->wrap().as_node(), value);
}

static int rna_GreasePencilLayer_name_length(PointerRNA *ptr)
{
  GreasePencilLayer *layer = static_cast<GreasePencilLayer *>(ptr->data);
  return tree_node_name_length(layer->wrap().as_node());
}

static void rna_GreasePencilLayer_name_set(PointerRNA *ptr, const char *value)
{
  GreasePencil *grease_pencil = rna_grease_pencil(ptr);
  GreasePencilLayer *layer = static_cast<GreasePencilLayer *>(ptr->data);

  grease_pencil->rename_node(layer->wrap().as_node(), value);
}

static int rna_GreasePencilLayer_pass_index_get(PointerRNA *ptr)
{
  using namespace blender;
  const GreasePencil &grease_pencil = *rna_grease_pencil(ptr);
  const bke::greasepencil::Layer &layer =
      static_cast<const GreasePencilLayer *>(ptr->data)->wrap();
  const int layer_idx = *grease_pencil.get_layer_index(layer);

  const VArray layer_passes = *grease_pencil.attributes().lookup_or_default<int>(
      "pass_index", bke::AttrDomain::Layer, 0);
  return layer_passes[layer_idx];
}

static void rna_GreasePencilLayer_pass_index_set(PointerRNA *ptr, int value)
{
  using namespace blender;
  GreasePencil &grease_pencil = *rna_grease_pencil(ptr);
  const bke::greasepencil::Layer &layer =
      static_cast<const GreasePencilLayer *>(ptr->data)->wrap();
  const int layer_idx = *grease_pencil.get_layer_index(layer);

  bke::SpanAttributeWriter<int> layer_passes =
      grease_pencil.attributes_for_write().lookup_or_add_for_write_span<int>(
          "pass_index", bke::AttrDomain::Layer);
  layer_passes.span[layer_idx] = std::max(0, value);
  layer_passes.finish();
}

static PointerRNA rna_GreasePencil_active_layer_get(PointerRNA *ptr)
{
  GreasePencil *grease_pencil = rna_grease_pencil(ptr);
  if (grease_pencil->has_active_layer()) {
    return rna_pointer_inherit_refine(
        ptr, &RNA_GreasePencilLayer, static_cast<void *>(grease_pencil->get_active_layer()));
  }
  return rna_pointer_inherit_refine(ptr, nullptr, nullptr);
}

static void rna_GreasePencil_active_layer_set(PointerRNA *ptr,
                                              PointerRNA value,
                                              ReportList * /*reports*/)
{
  GreasePencil *grease_pencil = rna_grease_pencil(ptr);
  grease_pencil->set_active_layer(static_cast<blender::bke::greasepencil::Layer *>(value.data));
  WM_main_add_notifier(NC_GPENCIL | NA_EDITED, nullptr);
}

static std::optional<std::string> rna_GreasePencilLayerGroup_path(const PointerRNA *ptr)
{
  GreasePencilLayerTreeGroup *group = static_cast<GreasePencilLayerTreeGroup *>(ptr->data);
  return tree_node_name_path(group->wrap().as_node(), "layer_groups");
}

static void rna_GreasePencilLayerGroup_name_get(PointerRNA *ptr, char *value)
{
  GreasePencilLayerTreeGroup *group = static_cast<GreasePencilLayerTreeGroup *>(ptr->data);
  tree_node_name_get(group->wrap().as_node(), value);
}

static int rna_GreasePencilLayerGroup_name_length(PointerRNA *ptr)
{
  GreasePencilLayerTreeGroup *group = static_cast<GreasePencilLayerTreeGroup *>(ptr->data);
  return tree_node_name_length(group->wrap().as_node());
}

static void rna_GreasePencilLayerGroup_name_set(PointerRNA *ptr, const char *value)
{
  GreasePencil *grease_pencil = rna_grease_pencil(ptr);
  GreasePencilLayerTreeGroup *group = static_cast<GreasePencilLayerTreeGroup *>(ptr->data);

  grease_pencil->rename_node(group->wrap().as_node(), value);
}

static void rna_iterator_grease_pencil_layer_groups_begin(CollectionPropertyIterator *iter,
                                                          PointerRNA *ptr)
{
  using namespace blender::bke::greasepencil;
  GreasePencil *grease_pencil = rna_grease_pencil(ptr);

  blender::Span<LayerGroup *> groups = grease_pencil->layer_groups_for_write();

  rna_iterator_array_begin(
      iter, (void *)groups.data(), sizeof(LayerGroup *), groups.size(), 0, nullptr);
}

static int rna_iterator_grease_pencil_layer_groups_length(PointerRNA *ptr)
{
  GreasePencil *grease_pencil = rna_grease_pencil(ptr);
  return grease_pencil->layer_groups().size();
}

#else

static void rna_def_grease_pencil_layers_mask_api(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;
  PropertyRNA *prop;

  RNA_def_property_srna(cprop, "GreasePencilLayerMasks");
  srna = RNA_def_struct(brna, "GreasePencilLayerMasks", nullptr);
  RNA_def_struct_sdna(srna, "GreasePencilLayer");
  RNA_def_struct_ui_text(
      srna, "Grease Pencil Mask Layers", "Collection of grease pencil masking layers");

  prop = RNA_def_property(srna, "active_mask_index", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_int_funcs(prop,
                             "rna_grease_pencil_active_mask_index_get",
                             "rna_grease_pencil_active_mask_index_set",
                             "rna_grease_pencil_active_mask_index_range");
  RNA_def_property_ui_text(prop, "Active Layer Mask Index", "Active index in layer mask array");
}

static void rna_def_grease_pencil_layer_mask(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "GreasePencilLayerMask", nullptr);
  RNA_def_struct_sdna(srna, "GreasePencilLayerMask");
  RNA_def_struct_ui_text(srna, "Grease Pencil Masking Layers", "List of Mask Layers");
  // RNA_def_struct_path_func(srna, "rna_GreasePencilLayerMask_path");

  prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
  RNA_def_property_ui_text(prop, "Layer", "Mask layer name");
  RNA_def_property_string_sdna(prop, nullptr, "layer_name");
  RNA_def_property_string_funcs(prop,
                                "rna_grease_pencil_layer_mask_name_get",
                                "rna_grease_pencil_layer_mask_name_length",
                                "rna_grease_pencil_layer_mask_name_set");
  RNA_def_struct_name_property(srna, prop);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA | NA_RENAME, nullptr);

  prop = RNA_def_property(srna, "hide", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_LAYER_MASK_HIDE);
  RNA_def_property_ui_icon(prop, ICON_HIDE_OFF, -1);
  RNA_def_property_ui_text(prop, "Hide", "Set mask Visibility");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_grease_pencil_update");

  prop = RNA_def_property(srna, "invert", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_LAYER_MASK_INVERT);
  RNA_def_property_ui_icon(prop, ICON_SELECT_INTERSECT, 1);
  RNA_def_property_ui_text(prop, "Invert", "Invert mask");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_grease_pencil_update");
}

static void rna_def_grease_pencil_layer(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static const float scale_defaults[3] = {1.0f, 1.0f, 1.0f};

  static const EnumPropertyItem rna_enum_layer_blend_modes_items[] = {
      {GP_LAYER_BLEND_NONE, "REGULAR", 0, "Regular", ""},
      {GP_LAYER_BLEND_HARDLIGHT, "HARDLIGHT", 0, "Hard Light", ""},
      {GP_LAYER_BLEND_ADD, "ADD", 0, "Add", ""},
      {GP_LAYER_BLEND_SUBTRACT, "SUBTRACT", 0, "Subtract", ""},
      {GP_LAYER_BLEND_MULTIPLY, "MULTIPLY", 0, "Multiply", ""},
      {GP_LAYER_BLEND_DIVIDE, "DIVIDE", 0, "Divide", ""},
      {0, nullptr, 0, nullptr, nullptr}};

  srna = RNA_def_struct(brna, "GreasePencilLayer", nullptr);
  RNA_def_struct_sdna(srna, "GreasePencilLayer");
  RNA_def_struct_ui_text(srna, "Grease Pencil Layer", "Collection of related drawings");
  RNA_def_struct_path_func(srna, "rna_GreasePencilLayer_path");

  /* Name */
  prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
  RNA_def_property_ui_text(prop, "Name", "Layer name");
  RNA_def_property_string_funcs(prop,
                                "rna_GreasePencilLayer_name_get",
                                "rna_GreasePencilLayer_name_length",
                                "rna_GreasePencilLayer_name_set");
  RNA_def_struct_name_property(srna, prop);
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA | NA_RENAME, "rna_grease_pencil_update");

  /* Mask Layers */
  prop = RNA_def_property(srna, "mask_layers", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, nullptr, "masks", nullptr);
  RNA_def_property_struct_type(prop, "GreasePencilLayerMask");
  RNA_def_property_ui_text(prop, "Masks", "List of Masking Layers");
  rna_def_grease_pencil_layers_mask_api(brna, prop);

  /* Visibility */
  prop = RNA_def_property(srna, "hide", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(
      prop, "GreasePencilLayerTreeNode", "flag", GP_LAYER_TREE_NODE_HIDE);
  RNA_def_property_ui_icon(prop, ICON_HIDE_OFF, -1);
  RNA_def_property_ui_text(prop, "Hide", "Set layer visibility");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_grease_pencil_update");

  /* Lock */
  prop = RNA_def_property(srna, "lock", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(
      prop, "GreasePencilLayerTreeNode", "flag", GP_LAYER_TREE_NODE_LOCKED);
  RNA_def_property_ui_icon(prop, ICON_UNLOCKED, 1);
  RNA_def_property_ui_text(
      prop, "Locked", "Protect layer from further editing and/or frame changes");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_grease_pencil_update");

  /* Opacity */
  prop = RNA_def_property(srna, "opacity", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, "GreasePencilLayer", "opacity");
  RNA_def_property_ui_text(prop, "Opacity", "Layer Opacity");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_grease_pencil_update");

  /* Onion Skinning. */
  prop = RNA_def_property(srna, "use_onion_skinning", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(
      prop, "GreasePencilLayerTreeNode", "flag", GP_LAYER_TREE_NODE_USE_ONION_SKINNING);
  RNA_def_property_ui_text(
      prop, "Onion Skinning", "Display onion skins before and after the current frame");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_grease_pencil_update");

  /* Use Masks. */
  prop = RNA_def_property(srna, "use_masks", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(
      prop, "GreasePencilLayerTreeNode", "flag", GP_LAYER_TREE_NODE_HIDE_MASKS);
  RNA_def_property_ui_text(
      prop,
      "Use Masks",
      "The visibility of drawings on this layer is affected by the layers in its masks list");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_grease_pencil_update");

  /* pass index for compositing and modifiers */
  prop = RNA_def_property(srna, "pass_index", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_ui_text(prop, "Pass Index", "Index number for the \"Layer Index\" pass");
  RNA_def_property_int_funcs(prop,
                             "rna_GreasePencilLayer_pass_index_get",
                             "rna_GreasePencilLayer_pass_index_set",
                             nullptr);
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_grease_pencil_update");

  prop = RNA_def_property(srna, "parent", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "Object");
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop, "Parent", "Parent object");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_grease_pencil_dependency_update");

  prop = RNA_def_property(srna, "parent_bone", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "parsubstr");
  RNA_def_property_ui_text(
      prop, "Parent Bone", "Name of parent bone. Only used when the parent object is an armature");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_grease_pencil_dependency_update");

  prop = RNA_def_property(srna, "translation", PROP_FLOAT, PROP_TRANSLATION);
  RNA_def_property_array(prop, 3);
  RNA_def_property_float_sdna(prop, nullptr, "translation");
  RNA_def_property_ui_range(prop, -FLT_MAX, FLT_MAX, 1, RNA_TRANSLATION_PREC_DEFAULT);
  RNA_def_property_ui_text(prop, "Translation", "Translation of the layer");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_grease_pencil_update");

  prop = RNA_def_property(srna, "rotation", PROP_FLOAT, PROP_EULER);
  RNA_def_property_array(prop, 3);
  RNA_def_property_float_sdna(prop, nullptr, "rotation");
  RNA_def_property_ui_range(prop, -FLT_MAX, FLT_MAX, 1, RNA_TRANSLATION_PREC_DEFAULT);
  RNA_def_property_ui_text(prop, "Rotation", "Euler rotation of the layer");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_grease_pencil_update");

  prop = RNA_def_property(srna, "scale", PROP_FLOAT, PROP_XYZ);
  RNA_def_property_array(prop, 3);
  RNA_def_property_float_sdna(prop, nullptr, "scale");
  RNA_def_property_float_array_default(prop, scale_defaults);
  RNA_def_property_ui_range(prop, -FLT_MAX, FLT_MAX, 1, 3);
  RNA_def_property_ui_text(prop, "Scale", "Scale of the layer");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_grease_pencil_update");

  prop = RNA_def_property(srna, "viewlayer_render", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "viewlayername");
  RNA_def_property_ui_text(
      prop,
      "ViewLayer",
      "Only include Layer in this View Layer render output (leave blank to include always)");

  prop = RNA_def_property(srna, "blend_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "blend_mode");
  RNA_def_property_enum_items(prop, rna_enum_layer_blend_modes_items);
  RNA_def_property_ui_text(prop, "Blend Mode", "Blend mode");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_grease_pencil_update");
}

static void rna_def_grease_pencil_layers_api(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;
  PropertyRNA *prop;

  RNA_def_property_srna(cprop, "GreasePencilv3Layers");
  srna = RNA_def_struct(brna, "GreasePencilv3Layers", nullptr);
  RNA_def_struct_sdna(srna, "GreasePencil");
  RNA_def_struct_ui_text(srna, "Grease Pencil Layers", "Collection of Grease Pencil layers");

  prop = RNA_def_property(srna, "active", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "GreasePencilLayer");
  RNA_def_property_pointer_funcs(prop,
                                 "rna_GreasePencil_active_layer_get",
                                 "rna_GreasePencil_active_layer_set",
                                 nullptr,
                                 nullptr);
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Active Layer", "Active Grease Pencil layer");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA | NA_SELECTED, nullptr);
}

static void rna_def_grease_pencil_layer_group(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "GreasePencilLayerGroup", nullptr);
  RNA_def_struct_sdna(srna, "GreasePencilLayerTreeGroup");
  RNA_def_struct_ui_text(srna, "Grease Pencil Layer Group", "Group of Grease Pencil layers");
  RNA_def_struct_path_func(srna, "rna_GreasePencilLayerGroup_path");

  /* Name */
  prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
  RNA_def_property_ui_text(prop, "Name", "Group name");
  RNA_def_property_string_funcs(prop,
                                "rna_GreasePencilLayerGroup_name_get",
                                "rna_GreasePencilLayerGroup_name_length",
                                "rna_GreasePencilLayerGroup_name_set");
  RNA_def_struct_name_property(srna, prop);
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA | NA_RENAME, "rna_grease_pencil_update");

  /* Visibility */
  prop = RNA_def_property(srna, "hide", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(
      prop, "GreasePencilLayerTreeNode", "flag", GP_LAYER_TREE_NODE_HIDE);
  RNA_def_property_ui_icon(prop, ICON_HIDE_OFF, -1);
  RNA_def_property_ui_text(prop, "Hide", "Set layer group visibility");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_grease_pencil_update");

  /* Lock */
  prop = RNA_def_property(srna, "lock", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(
      prop, "GreasePencilLayerTreeNode", "flag", GP_LAYER_TREE_NODE_LOCKED);
  RNA_def_property_ui_icon(prop, ICON_UNLOCKED, 1);
  RNA_def_property_ui_text(
      prop, "Locked", "Protect group from further editing and/or frame changes");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_grease_pencil_update");

  /* Use Masks. */
  prop = RNA_def_property(srna, "use_masks", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(
      prop, "GreasePencilLayerTreeNode", "flag", GP_LAYER_TREE_NODE_HIDE_MASKS);
  RNA_def_property_ui_text(prop,
                           "Use Masks",
                           "The visibility of drawings in the layers in this group is affected by "
                           "the layers in the masks lists");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_grease_pencil_update");
}

static void rna_def_grease_pencil_data(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static EnumPropertyItem prop_stroke_depth_order_items[] = {
      {0, "2D", 0, "2D Layers", "Display strokes using grease pencil layers to define order"},
      {GREASE_PENCIL_STROKE_ORDER_3D,
       "3D",
       0,
       "3D Location",
       "Display strokes using real 3D position in 3D space"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  srna = RNA_def_struct(brna, "GreasePencilv3", "ID");
  RNA_def_struct_sdna(srna, "GreasePencil");
  RNA_def_struct_ui_text(srna, "Grease Pencil", "Grease Pencil data-block");
  RNA_def_struct_ui_icon(srna, ICON_OUTLINER_DATA_GREASEPENCIL);

  /* attributes */
  rna_def_attributes_common(srna);

  /* Animation Data */
  rna_def_animdata_common(srna);

  /* Materials */
  prop = RNA_def_property(srna, "materials", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, nullptr, "material_array", "material_array_num");
  RNA_def_property_struct_type(prop, "Material");
  RNA_def_property_ui_text(prop, "Materials", "");
  RNA_def_property_srna(prop, "IDMaterials"); /* see rna_ID.cc */
  RNA_def_property_collection_funcs(prop,
                                    nullptr,
                                    nullptr,
                                    nullptr,
                                    nullptr,
                                    nullptr,
                                    nullptr,
                                    nullptr,
                                    "rna_IDMaterials_assign_int");

  /* Layers */
  prop = RNA_def_property(srna, "layers", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(prop, "GreasePencilLayer");
  RNA_def_property_collection_funcs(prop,
                                    "rna_iterator_grease_pencil_layers_begin",
                                    "rna_iterator_array_next",
                                    "rna_iterator_array_end",
                                    "rna_iterator_array_dereference_get",
                                    "rna_iterator_grease_pencil_layers_length",
                                    nullptr, /* TODO */
                                    nullptr, /* TODO */
                                    nullptr);
  RNA_def_property_ui_text(prop, "Layers", "Grease Pencil layers");
  rna_def_grease_pencil_layers_api(brna, prop);

  /* Layer Groups */
  prop = RNA_def_property(srna, "layer_groups", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(prop, "GreasePencilLayerGroup");
  RNA_def_property_collection_funcs(prop,
                                    "rna_iterator_grease_pencil_layer_groups_begin",
                                    "rna_iterator_array_next",
                                    "rna_iterator_array_end",
                                    "rna_iterator_array_dereference_get",
                                    "rna_iterator_grease_pencil_layer_groups_length",
                                    nullptr, /* TODO */
                                    nullptr, /* TODO */
                                    nullptr);
  RNA_def_property_ui_text(prop, "Layer Groups", "Grease Pencil layer groups");

  prop = RNA_def_property(srna, "use_autolock_layers", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GREASE_PENCIL_AUTOLOCK_LAYERS);
  RNA_def_property_ui_text(
      prop,
      "Auto-Lock Layers",
      "Automatically lock all layers except the active one to avoid accidental changes");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_grease_pencil_autolock");

  /* Uses a single flag, because the depth order can only be 2D or 3D. */
  prop = RNA_def_property(srna, "stroke_depth_order", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, nullptr, "flag");
  RNA_def_property_enum_items(prop, prop_stroke_depth_order_items);
  RNA_def_property_ui_text(
      prop,
      "Stroke Depth Order",
      "Defines how the strokes are ordered in 3D space (for objects not displayed 'In Front')");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_grease_pencil_update");
}

void RNA_def_grease_pencil(BlenderRNA *brna)
{
  rna_def_grease_pencil_data(brna);
  rna_def_grease_pencil_layer(brna);
  rna_def_grease_pencil_layer_mask(brna);
  rna_def_grease_pencil_layer_group(brna);
}

#endif
