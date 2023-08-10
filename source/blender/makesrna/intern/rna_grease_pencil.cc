/* SPDX-FileCopyrightText: 2023 Blender Foundation
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

#include "rna_internal.h"

#include "WM_api.hh"

#ifdef RNA_RUNTIME

#  include "BKE_grease_pencil.hh"

#  include "BLI_span.hh"

#  include "DEG_depsgraph.h"

static GreasePencil *rna_grease_pencil(const PointerRNA *ptr)
{
  return reinterpret_cast<GreasePencil *>(ptr->owner_id);
}

static void rna_grease_pencil_update(Main * /*bmain*/, Scene * /*scene*/, PointerRNA *ptr)
{
  DEG_id_tag_update(&rna_grease_pencil(ptr)->id, ID_RECALC_GEOMETRY);
  WM_main_add_notifier(NC_GPENCIL | NA_EDITED, nullptr);
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

static char *rna_GreasePencilLayer_path(const PointerRNA *ptr)
{
  GreasePencilLayer *layer = static_cast<GreasePencilLayer *>(ptr->data);

  BLI_assert(layer->base.name);
  const size_t name_length = strlen(layer->base.name);

  std::string name_esc(name_length * 2, '\0');
  BLI_str_escape(name_esc.data(), layer->base.name, name_length * 2);

  return BLI_sprintfN("layers[\"%s\"]", name_esc.c_str());
}

static void rna_GreasePencilLayer_name_get(PointerRNA *ptr, char *value)
{
  GreasePencilLayer *layer = static_cast<GreasePencilLayer *>(ptr->data);

  if (layer->base.name) {
    strcpy(value, layer->base.name);
  }
  else {
    value[0] = '\0';
  }
}

static int rna_GreasePencilLayer_name_length(PointerRNA *ptr)
{
  GreasePencilLayer *layer = static_cast<GreasePencilLayer *>(ptr->data);
  return layer->base.name ? strlen(layer->base.name) : 0;
}

static void rna_GreasePencilLayer_name_set(PointerRNA *ptr, const char *value)
{
  GreasePencil *grease_pencil = rna_grease_pencil(ptr);
  GreasePencilLayer *layer = static_cast<GreasePencilLayer *>(ptr->data);

  grease_pencil->rename_layer(layer->wrap(), value);
}

static PointerRNA rna_GreasePencil_active_layer_get(PointerRNA *ptr)
{
  GreasePencil *grease_pencil = rna_grease_pencil(ptr);
  if (grease_pencil->has_active_layer()) {
    return rna_pointer_inherit_refine(
        ptr,
        &RNA_GreasePencilLayer,
        static_cast<void *>(grease_pencil->get_active_layer_for_write()));
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

static char *rna_GreasePencilLayerGroup_path(const PointerRNA *ptr)
{
  GreasePencilLayerTreeGroup *group = static_cast<GreasePencilLayerTreeGroup *>(ptr->data);

  BLI_assert(group->base.name);
  const size_t name_length = strlen(group->base.name);

  std::string name_esc(name_length * 2, '\0');
  BLI_str_escape(name_esc.data(), group->base.name, name_length * 2);

  return BLI_sprintfN("layer_groups[\"%s\"]", name_esc.c_str());
}

static void rna_GreasePencilLayerGroup_name_get(PointerRNA *ptr, char *value)
{
  GreasePencilLayerTreeGroup *group = static_cast<GreasePencilLayerTreeGroup *>(ptr->data);

  if (group->base.name) {
    strcpy(value, group->base.name);
  }
  else {
    value[0] = '\0';
  }
}

static int rna_GreasePencilLayerGroup_name_length(PointerRNA *ptr)
{
  GreasePencilLayerTreeGroup *group = static_cast<GreasePencilLayerTreeGroup *>(ptr->data);
  return group->base.name ? strlen(group->base.name) : 0;
}

static void rna_GreasePencilLayerGroup_name_set(PointerRNA *ptr, const char *value)
{
  GreasePencil *grease_pencil = rna_grease_pencil(ptr);
  GreasePencilLayerTreeGroup *group = static_cast<GreasePencilLayerTreeGroup *>(ptr->data);

  grease_pencil->rename_group(group->wrap(), value);
}

static void rna_iterator_grease_pencil_layer_groups_begin(CollectionPropertyIterator *iter,
                                                          PointerRNA *ptr)
{
  using namespace blender::bke::greasepencil;
  GreasePencil *grease_pencil = rna_grease_pencil(ptr);

  blender::Span<LayerGroup *> groups = grease_pencil->groups_for_write();

  rna_iterator_array_begin(
      iter, (void *)groups.data(), sizeof(LayerGroup *), groups.size(), 0, nullptr);
}

static int rna_iterator_grease_pencil_layer_groups_length(PointerRNA *ptr)
{
  GreasePencil *grease_pencil = rna_grease_pencil(ptr);
  return grease_pencil->groups().size();
}

#else

static void rna_def_grease_pencil_layer(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

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
}

static void rna_def_grease_pencil_data(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "GreasePencilv3", "ID");
  RNA_def_struct_sdna(srna, "GreasePencil");
  RNA_def_struct_ui_text(srna, "Grease Pencil", "Grease Pencil data-block");
  RNA_def_struct_ui_icon(srna, ICON_OUTLINER_DATA_GREASEPENCIL);

  /* Animation Data */
  rna_def_animdata_common(srna);

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
  prop = RNA_def_property(srna, "groups", PROP_COLLECTION, PROP_NONE);
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
}

void RNA_def_grease_pencil(BlenderRNA *brna)
{
  rna_def_grease_pencil_data(brna);
  rna_def_grease_pencil_layer(brna);
  rna_def_grease_pencil_layer_group(brna);
}

#endif
