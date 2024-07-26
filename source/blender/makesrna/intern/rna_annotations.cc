/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup RNA
 */

#include "DNA_gpencil_legacy_types.h"
#include "DNA_scene_types.h"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_enum_types.hh"

#include "rna_internal.hh"

#include "WM_types.hh"

#ifdef RNA_RUNTIME

#  include <fmt/format.h>

#  include "BLT_translation.hh"

#  include "BLI_string_utils.hh"

#  include "BKE_animsys.h"
#  include "BKE_gpencil_legacy.h"
#  include "BKE_icons.h"

#  include "DEG_depsgraph.hh"

#  include "WM_api.hh"

static bGPdata *rna_annotations(const PointerRNA *ptr)
{
  return reinterpret_cast<bGPdata *>(ptr->owner_id);
}

static void rna_annotation_update(Main * /*bmain*/, Scene * /*scene*/, PointerRNA *ptr)
{
  DEG_id_tag_update(ptr->owner_id, ID_RECALC_GEOMETRY);
  WM_main_add_notifier(NC_GPENCIL | NA_EDITED, nullptr);
}

/* Poll Callback to filter legacy GP Datablocks to only show those for Annotations */
bool rna_GPencil_datablocks_annotations_poll(PointerRNA * /*ptr*/, const PointerRNA value)
{
  bGPdata *gpd = static_cast<bGPdata *>(value.data);
  return (gpd->flag & GP_DATA_ANNOTATIONS) != 0;
}

static std::optional<std::string> rna_annotation_layer_path(const PointerRNA *ptr)
{
  bGPDlayer *gpl = static_cast<bGPDlayer *>(ptr->data);
  char name_esc[sizeof(gpl->info) * 2];

  BLI_str_escape(name_esc, gpl->info, sizeof(name_esc));

  return fmt::format("layers[\"{}\"]", name_esc);
}

static int rna_annotation_layer_active_frame_editable(const PointerRNA *ptr,
                                                      const char ** /*r_info*/)
{
  bGPDlayer *gpl = static_cast<bGPDlayer *>(ptr->data);

  /* surely there must be other criteria too... */
  if (gpl->flag & GP_LAYER_LOCKED) {
    return 0;
  }
  else {
    return PROP_EDITABLE;
  }
}

static void rna_annotation_layer_info_set(PointerRNA *ptr, const char *value)
{
  bGPdata *gpd = rna_annotations(ptr);
  bGPDlayer *gpl = static_cast<bGPDlayer *>(ptr->data);

  char oldname[128] = "";
  STRNCPY(oldname, gpl->info);

  /* copy the new name into the name slot */
  STRNCPY_UTF8(gpl->info, value);

  BLI_uniquename(
      &gpd->layers, gpl, DATA_("GP_Layer"), '.', offsetof(bGPDlayer, info), sizeof(gpl->info));

  /* now fix animation paths */
  BKE_animdata_fix_paths_rename_all(&gpd->id, "layers", oldname, gpl->info);

  /* Fix mask layers. */
  LISTBASE_FOREACH (bGPDlayer *, gpl_, &gpd->layers) {
    LISTBASE_FOREACH (bGPDlayer_Mask *, mask, &gpl_->mask_layers) {
      if (STREQ(mask->name, oldname)) {
        STRNCPY(mask->name, gpl->info);
      }
    }
  }
}

static int rna_annotation_active_layer_index_get(PointerRNA *ptr)
{
  bGPdata *gpd = rna_annotations(ptr);
  bGPDlayer *gpl = BKE_gpencil_layer_active_get(gpd);

  return BLI_findindex(&gpd->layers, gpl);
}

static void rna_annotation_active_layer_index_set(PointerRNA *ptr, int value)
{
  bGPdata *gpd = rna_annotations(ptr);
  bGPDlayer *gpl = static_cast<bGPDlayer *>(BLI_findlink(&gpd->layers, value));

  BKE_gpencil_layer_active_set(gpd, gpl);

  /* Now do standard updates... */
  DEG_id_tag_update(&gpd->id, ID_RECALC_GEOMETRY);
  WM_main_add_notifier(NC_GPENCIL | ND_DATA | NA_EDITED | ND_SPACE_PROPERTIES, nullptr);
}

static void rna_annotation_active_layer_index_range(
    PointerRNA *ptr, int *min, int *max, int *softmin, int *softmax)
{
  bGPdata *gpd = rna_annotations(ptr);

  *min = 0;
  *max = max_ii(0, BLI_listbase_count(&gpd->layers) - 1);

  *softmin = *min;
  *softmax = *max;
}

static const EnumPropertyItem *rna_annotation_active_layer_itemf(bContext *C,
                                                                 PointerRNA *ptr,
                                                                 PropertyRNA * /*prop*/,
                                                                 bool *r_free)
{
  bGPdata *gpd = rna_annotations(ptr);
  bGPDlayer *gpl;
  EnumPropertyItem *item = nullptr, item_tmp = {0};
  int totitem = 0;
  int i = 0;

  if (ELEM(nullptr, C, gpd)) {
    return rna_enum_dummy_NULL_items;
  }

  /* Existing layers */
  for (gpl = static_cast<bGPDlayer *>(gpd->layers.first), i = 0; gpl; gpl = gpl->next, i++) {
    item_tmp.identifier = gpl->info;
    item_tmp.name = gpl->info;
    item_tmp.value = i;

    item_tmp.icon = (gpd->flag & GP_DATA_ANNOTATIONS) ? BKE_icon_gplayer_color_ensure(gpl) :
                                                        ICON_GREASEPENCIL;

    RNA_enum_item_add(&item, &totitem, &item_tmp);
  }

  RNA_enum_item_end(&item, &totitem);
  *r_free = true;

  return item;
}

#else

static void rna_def_annotation_stroke_point(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "GPencilStrokePoint", nullptr);
  RNA_def_struct_sdna(srna, "bGPDspoint");
  RNA_def_struct_ui_text(srna, "Annotation Stroke Point", "Data point for freehand stroke curve");

  prop = RNA_def_property(srna, "co", PROP_FLOAT, PROP_XYZ);
  RNA_def_property_float_sdna(prop, nullptr, "x");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Coordinates", "");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_annotation_update");
}

static void rna_def_annotation_stroke(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "GPencilStroke", nullptr);
  RNA_def_struct_sdna(srna, "bGPDstroke");
  RNA_def_struct_ui_text(srna, "Annotation Stroke", "Freehand curve defining part of a sketch");

  /* Points */
  prop = RNA_def_property(srna, "points", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, nullptr, "points", "totpoints");
  RNA_def_property_struct_type(prop, "GPencilStrokePoint");
  RNA_def_property_ui_text(prop, "Stroke Points", "Stroke data points");
}

static void rna_def_annotation_frame(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "GPencilFrame", nullptr);
  RNA_def_struct_sdna(srna, "bGPDframe");
  RNA_def_struct_ui_text(
      srna, "Annotation Frame", "Collection of related sketches on a particular frame");

  /* Strokes */
  prop = RNA_def_property(srna, "strokes", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, nullptr, "strokes", nullptr);
  RNA_def_property_struct_type(prop, "GPencilStroke");
  RNA_def_property_ui_text(prop, "Strokes", "Freehand curves defining the sketch on this frame");

  /* Frame Number */
  prop = RNA_def_property(srna, "frame_number", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "framenum");
  /* XXX NOTE: this cannot occur on the same frame as another sketch. */
  RNA_def_property_range(prop, -MAXFRAME, MAXFRAME);
  RNA_def_property_ui_text(prop, "Frame Number", "The frame on which this sketch appears");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_annotation_update");

  prop = RNA_def_property(srna, "select", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_FRAME_SELECT);
  RNA_def_property_ui_text(prop, "Select", "Frame is selected for editing in the Dope Sheet");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_annotation_update");
}

static void rna_def_annotation_layer(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static const float default_onion_color_b[] = {0.302f, 0.851f, 0.302f};
  static const float default_onion_color_a[] = {0.250f, 0.1f, 1.0f};

  srna = RNA_def_struct(brna, "GPencilLayer", nullptr);
  RNA_def_struct_sdna(srna, "bGPDlayer");
  RNA_def_struct_ui_text(srna, "Annotation Layer", "Collection of related sketches");
  RNA_def_struct_path_func(srna, "rna_annotation_layer_path");

  /* Name */
  prop = RNA_def_property(srna, "info", PROP_STRING, PROP_NONE);
  RNA_def_property_ui_text(prop, "Info", "Layer name");
  RNA_def_property_string_funcs(prop, nullptr, nullptr, "rna_annotation_layer_info_set");
  RNA_def_struct_name_property(srna, prop);
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA | NA_RENAME, "rna_annotation_update");

  /* Frames */
  prop = RNA_def_property(srna, "frames", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, nullptr, "frames", nullptr);
  RNA_def_property_struct_type(prop, "GPencilFrame");
  RNA_def_property_ui_text(prop, "Frames", "Sketches for this layer on different frames");

  /* Active Frame */
  prop = RNA_def_property(srna, "active_frame", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "actframe");
  RNA_def_property_ui_text(prop, "Active Frame", "Frame currently being displayed for this layer");
  RNA_def_property_editable_func(prop, "rna_annotation_layer_active_frame_editable");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, nullptr);

  /* Layer Opacity (Annotations). */
  prop = RNA_def_property(srna, "annotation_opacity", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "opacity");
  RNA_def_property_range(prop, 0.0, 1.0f);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Opacity", "Annotation Layer Opacity");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_annotation_update");

  /* Stroke Drawing Color (Annotations) */
  prop = RNA_def_property(srna, "color", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 3);
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Color", "Color for all strokes in this layer");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_annotation_update");

  /* Line Thickness (Annotations) */
  prop = RNA_def_property(srna, "thickness", PROP_INT, PROP_PIXEL);
  RNA_def_property_int_sdna(prop, nullptr, "thickness");
  RNA_def_property_range(prop, 1, 10);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Thickness", "Thickness of annotation strokes");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_annotation_update");

  /* Onion-Skinning */
  prop = RNA_def_property(srna, "use_annotation_onion_skinning", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "onion_flag", GP_LAYER_ONIONSKIN);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(
      prop, "Onion Skinning", "Display annotation onion skins before and after the current frame");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_annotation_update");

  prop = RNA_def_property(srna, "annotation_onion_before_range", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "gstep");
  RNA_def_property_range(prop, -1, 120);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(
      prop, "Frames Before", "Maximum number of frames to show before current frame");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_annotation_update");

  prop = RNA_def_property(srna, "annotation_onion_after_range", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "gstep_next");
  RNA_def_property_range(prop, -1, 120);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(
      prop, "Frames After", "Maximum number of frames to show after current frame");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_annotation_update");

  prop = RNA_def_property(srna, "annotation_onion_before_color", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, nullptr, "gcolor_prev");
  RNA_def_property_array(prop, 3);
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_float_array_default(prop, default_onion_color_b);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Before Color", "Base color for ghosts before the active frame");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_annotation_update");

  prop = RNA_def_property(srna, "annotation_onion_after_color", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, nullptr, "gcolor_next");
  RNA_def_property_array(prop, 3);
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_float_array_default(prop, default_onion_color_a);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "After Color", "Base color for ghosts after the active frame");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_annotation_update");

  prop = RNA_def_property(srna, "annotation_onion_use_custom_color", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "onion_flag", GP_LAYER_ONIONSKIN_CUSTOM_COLOR);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop,
                           "Custom Onion Skin Colors",
                           "Use custom colors for onion skinning instead of the theme");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_annotation_update");

  prop = RNA_def_property(srna, "annotation_hide", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_LAYER_HIDE);
  RNA_def_property_ui_icon(prop, ICON_HIDE_OFF, -1);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Hide", "Set annotation Visibility");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_annotation_update");

  prop = RNA_def_property(srna, "lock", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_LAYER_LOCKED);
  RNA_def_property_ui_icon(prop, ICON_UNLOCKED, 1);
  RNA_def_property_ui_text(
      prop, "Locked", "Protect layer from further editing and/or frame changes");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_annotation_update");

  prop = RNA_def_property(srna, "lock_frame", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_LAYER_FRAMELOCK);
  RNA_def_property_ui_icon(prop, ICON_UNLOCKED, 1);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Frame Locked", "Lock current frame displayed by layer");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_annotation_update");

  prop = RNA_def_property(srna, "is_ruler", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_LAYER_IS_RULER);
  RNA_def_property_ui_text(prop, "Ruler", "This is a special ruler layer");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);

  prop = RNA_def_property(srna, "select", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_LAYER_SELECT);
  RNA_def_property_ui_text(prop, "Select", "Layer is selected for editing in the Dope Sheet");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA | NA_SELECTED, "rna_annotation_update");

  /* In Front */
  prop = RNA_def_property(srna, "show_in_front", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "flag", GP_LAYER_NO_XRAY);
  RNA_def_property_ui_text(prop, "In Front", "Make the layer display in front of objects");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_annotation_update");
}

static void rna_def_annotation_layers_api(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;
  PropertyRNA *prop;

  RNA_def_property_srna(cprop, "GreasePencilLayers");
  srna = RNA_def_struct(brna, "GreasePencilLayers", nullptr);
  RNA_def_struct_sdna(srna, "bGPdata");
  RNA_def_struct_ui_text(srna, "Annotation Layers", "Collection of annotation layers");

  prop = RNA_def_property(srna, "active_index", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_funcs(prop,
                             "rna_annotation_active_layer_index_get",
                             "rna_annotation_active_layer_index_set",
                             "rna_annotation_active_layer_index_range");
  RNA_def_property_ui_text(prop, "Active Layer Index", "Index of active annotation layer");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA | NA_SELECTED, nullptr);

  /* Active Layer - As an enum (for selecting active layer for annotations) */
  prop = RNA_def_property(srna, "active_note", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_funcs(prop,
                              "rna_annotation_active_layer_index_get",
                              "rna_annotation_active_layer_index_set",
                              "rna_annotation_active_layer_itemf");
  RNA_def_property_enum_items(
      prop, rna_enum_dummy_DEFAULT_items); /* purely dynamic, as it maps to user-data */
  RNA_def_property_ui_text(prop, "Active Note", "Note/Layer to add annotation strokes to");
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_annotation_update");
}

static void rna_def_annotation_data(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  /* NOTE: This used to be the legacy Grease Pencil ID type, hence the name. */
  srna = RNA_def_struct(brna, "GreasePencil", "ID");
  RNA_def_struct_sdna(srna, "bGPdata");
  RNA_def_struct_ui_text(srna, "Annotation", "Freehand annotation sketchbook");
  RNA_def_struct_ui_icon(srna, ICON_OUTLINER_DATA_GREASEPENCIL);

  /* Layers */
  prop = RNA_def_property(srna, "layers", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, nullptr, "layers", nullptr);
  RNA_def_property_struct_type(prop, "GPencilLayer");
  RNA_def_property_ui_text(prop, "Layers", "");

  rna_def_annotation_layers_api(brna, prop);

  /* Animation Data */
  rna_def_animdata_common(srna);
}

void RNA_def_annotations(BlenderRNA *brna)
{
  rna_def_annotation_data(brna);
  rna_def_annotation_layer(brna);
  rna_def_annotation_frame(brna);
  rna_def_annotation_stroke(brna);
  rna_def_annotation_stroke_point(brna);
}

#endif
