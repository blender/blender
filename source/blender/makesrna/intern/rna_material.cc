/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup RNA
 */

#include <cfloat>
#include <cstdlib>

#include "DNA_material_types.h"
#include "DNA_mesh_types.h"

#include "BLI_math_rotation.h"

#include "BLT_translation.hh"

#include "BKE_customdata.hh"

#include "RNA_define.hh"
#include "RNA_enum_types.hh"

#include "rna_internal.hh"

#include "WM_api.hh"
#include "WM_types.hh"

const EnumPropertyItem rna_enum_ramp_blend_items[] = {
    {MA_RAMP_BLEND, "MIX", 0, "Mix", ""},
    RNA_ENUM_ITEM_SEPR,
    {MA_RAMP_DARK, "DARKEN", 0, "Darken", ""},
    {MA_RAMP_MULT, "MULTIPLY", 0, "Multiply", ""},
    {MA_RAMP_BURN, "BURN", 0, "Color Burn", ""},
    RNA_ENUM_ITEM_SEPR,
    {MA_RAMP_LIGHT, "LIGHTEN", 0, "Lighten", ""},
    {MA_RAMP_SCREEN, "SCREEN", 0, "Screen", ""},
    {MA_RAMP_DODGE, "DODGE", 0, "Color Dodge", ""},
    {MA_RAMP_ADD, "ADD", 0, "Add", ""},
    RNA_ENUM_ITEM_SEPR,
    {MA_RAMP_OVERLAY, "OVERLAY", 0, "Overlay", ""},
    {MA_RAMP_SOFT, "SOFT_LIGHT", 0, "Soft Light", ""},
    {MA_RAMP_LINEAR, "LINEAR_LIGHT", 0, "Linear Light", ""},
    RNA_ENUM_ITEM_SEPR,
    {MA_RAMP_DIFF, "DIFFERENCE", 0, "Difference", ""},
    {MA_RAMP_EXCLUSION, "EXCLUSION", 0, "Exclusion", ""},
    {MA_RAMP_SUB, "SUBTRACT", 0, "Subtract", ""},
    {MA_RAMP_DIV, "DIVIDE", 0, "Divide", ""},
    RNA_ENUM_ITEM_SEPR,
    {MA_RAMP_HUE, "HUE", 0, "Hue", ""},
    {MA_RAMP_SAT, "SATURATION", 0, "Saturation", ""},
    {MA_RAMP_COLOR, "COLOR", 0, "Color", ""},
    {MA_RAMP_VAL, "VALUE", 0, "Value", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

#ifdef RNA_RUNTIME

#  include "MEM_guardedalloc.h"

#  include "DNA_gpencil_legacy_types.h"
#  include "DNA_meshdata_types.h"
#  include "DNA_node_types.h"
#  include "DNA_object_types.h"
#  include "DNA_screen_types.h"
#  include "DNA_space_types.h"

#  include "BKE_attribute.h"
#  include "BKE_attribute.hh"
#  include "BKE_colorband.hh"
#  include "BKE_context.hh"
#  include "BKE_editmesh.hh"
#  include "BKE_gpencil_legacy.h"
#  include "BKE_grease_pencil.hh"
#  include "BKE_main.hh"
#  include "BKE_material.hh"
#  include "BKE_mesh.hh"
#  include "BKE_mesh_types.hh"
#  include "BKE_node.hh"
#  include "BKE_paint.hh"
#  include "BKE_scene.hh"
#  include "BKE_texture.h"
#  include "BKE_workspace.hh"

#  include "DEG_depsgraph.hh"
#  include "DEG_depsgraph_build.hh"

#  include "ED_gpencil_legacy.hh"
#  include "ED_image.hh"
#  include "ED_node.hh"
#  include "ED_screen.hh"

static void rna_Material_update(Main * /*bmain*/, Scene * /*scene*/, PointerRNA *ptr)
{
  Material *ma = (Material *)ptr->owner_id;

  DEG_id_tag_update(&ma->id, ID_RECALC_SHADING);
  WM_main_add_notifier(NC_MATERIAL | ND_SHADING, ma);
}

static void rna_Material_update_previews(Main * /*bmain*/, Scene * /*scene*/, PointerRNA *ptr)
{
  Material *ma = (Material *)ptr->owner_id;

  WM_main_add_notifier(NC_MATERIAL | ND_SHADING_PREVIEW, ma);
}

static void rna_MaterialGpencil_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  Material *ma = (Material *)ptr->owner_id;
  rna_Material_update(bmain, scene, ptr);

  /* Need set all caches as dirty. */
  for (Object *ob = static_cast<Object *>(bmain->objects.first); ob;
       ob = static_cast<Object *>(ob->id.next))
  {
    if (ob->type == OB_GREASE_PENCIL) {
      GreasePencil &grease_pencil = *static_cast<GreasePencil *>(ob->data);
      DEG_id_tag_update(&grease_pencil.id, ID_RECALC_GEOMETRY);
    }
  }

  WM_main_add_notifier(NC_GPENCIL | ND_DATA, ma);
}

static void rna_MaterialLineArt_update(Main * /*bmain*/, Scene * /*scene*/, PointerRNA *ptr)
{
  Material *ma = (Material *)ptr->owner_id;
  /* Need to tag geometry for line art modifier updates. */
  DEG_id_tag_update(&ma->id, ID_RECALC_GEOMETRY);
  WM_main_add_notifier(NC_MATERIAL | ND_SHADING_DRAW, ma);
}

static std::optional<std::string> rna_MaterialLineArt_path(const PointerRNA * /*ptr*/)
{
  return "lineart";
}

static void rna_Material_draw_update(Main * /*bmain*/, Scene * /*scene*/, PointerRNA *ptr)
{
  Material *ma = (Material *)ptr->owner_id;

  DEG_id_tag_update(&ma->id, ID_RECALC_SHADING);
  WM_main_add_notifier(NC_MATERIAL | ND_SHADING_DRAW, ma);
}

static void rna_Material_texpaint_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  Material *ma = (Material *)ptr->data;
  rna_iterator_array_begin(
      iter, ptr, (void *)ma->texpaintslot, sizeof(TexPaintSlot), ma->tot_slots, 0, nullptr);
}

static void rna_Material_active_paint_texture_index_update(bContext *C, PointerRNA *ptr)
{
  using namespace blender;
  Main *bmain = CTX_data_main(C);
  Material *ma = (Material *)ptr->owner_id;

  if (ma->nodetree) {
    bNode *node = BKE_texpaint_slot_material_find_node(ma, ma->paint_active_slot);

    if (node) {
      blender::bke::node_set_active(*ma->nodetree, *node);
    }
  }

  if (ma->texpaintslot && (ma->tot_slots > ma->paint_active_slot)) {
    TexPaintSlot *slot = &ma->texpaintslot[ma->paint_active_slot];
    Image *image = slot->ima;
    if (image) {
      ED_space_image_sync(bmain, image, false);
    }

    /* For compatibility reasons with vertex paint we activate the color attribute. */
    if (const char *name = slot->attribute_name) {
      Object *ob = CTX_data_active_object(C);
      if (ob != nullptr && ob->type == OB_MESH) {
        Mesh *mesh = static_cast<Mesh *>(ob->data);
        if (mesh->runtime->edit_mesh) {
          if (const BMDataLayerLookup attr = BM_data_layer_lookup(*mesh->runtime->edit_mesh->bm,
                                                                  name))
          {
            BKE_id_attributes_active_color_set(&mesh->id, name);
          }
        }
        else {
          const bke::AttributeAccessor attributes = mesh->attributes();
          if (bke::mesh::is_color_attribute(attributes.lookup_meta_data(name))) {
            BKE_id_attributes_active_color_set(&mesh->id, name);
          }
        }
        DEG_id_tag_update(&ob->id, 0);
        WM_main_add_notifier(NC_GEOM | ND_DATA, &ob->id);
      }
    }
  }

  DEG_id_tag_update(&ma->id, 0);
  WM_main_add_notifier(NC_MATERIAL | ND_SHADING, ma);
}

static int rna_Material_blend_method_get(PointerRNA *ptr)
{
  Material *material = (Material *)ptr->owner_id;
  switch (material->surface_render_method) {
    case MA_SURFACE_METHOD_DEFERRED:
      return MA_BM_HASHED;
    case MA_SURFACE_METHOD_FORWARD:
      return MA_BM_BLEND;
  }
  return MA_BM_HASHED;
}

static void rna_Material_blend_method_set(PointerRNA *ptr, int new_blend_method)
{
  Material *material = (Material *)ptr->owner_id;
  switch (new_blend_method) {
    case MA_BM_SOLID:
    case MA_BM_CLIP:
    case MA_BM_HASHED:
      material->surface_render_method = MA_SURFACE_METHOD_DEFERRED;
      break;
    case MA_BM_BLEND:
      material->surface_render_method = MA_SURFACE_METHOD_FORWARD;
      break;
  }
}

static void rna_Material_render_method_set(PointerRNA *ptr, int new_render_method)
{
  Material *material = (Material *)ptr->owner_id;
  material->surface_render_method = new_render_method;

  /* Still sets the legacy property for forward compatibility. */
  switch (new_render_method) {
    case MA_SURFACE_METHOD_DEFERRED:
      material->blend_method = MA_BM_HASHED;
      break;
    case MA_SURFACE_METHOD_FORWARD:
      material->blend_method = MA_BM_BLEND;
      break;
  }
}
static void rna_Material_transparent_shadow_set(PointerRNA *ptr, bool new_value)
{
  Material *material = (Material *)ptr->owner_id;
  SET_FLAG_FROM_TEST(material->blend_flag, new_value, MA_BL_TRANSPARENT_SHADOW);
  /* Still sets the legacy property for forward compatibility. */
  material->blend_shadow = new_value ? MA_BS_HASHED : MA_BS_SOLID;
}

static bool rna_Material_use_nodes_get(PointerRNA * /*ptr*/)
{
  /* #use_nodes is deprecated. All materials now use nodes. */
  return true;
}

static void rna_Material_use_nodes_set(PointerRNA * /*ptr*/, bool /*new_value*/)
{
  /* #use_nodes is deprecated. Setting the property has no effect.
   * Note: Users will get a warning through the RNA deprecation warning, so no need to log a
   * warning here. */
  return;
}

MTex *rna_mtex_texture_slots_add(ID *self_id, bContext *C, ReportList *reports)
{
  MTex *mtex = BKE_texture_mtex_add_id(self_id, -1);
  if (mtex == nullptr) {
    BKE_reportf(reports, RPT_ERROR, "Maximum number of textures added %d", MAX_MTEX);
    return nullptr;
  }

  /* for redraw only */
  WM_event_add_notifier(C, NC_TEXTURE, CTX_data_scene(C));

  return mtex;
}

MTex *rna_mtex_texture_slots_create(ID *self_id, bContext *C, ReportList *reports, int index)
{
  MTex *mtex;

  if (index < 0 || index >= MAX_MTEX) {
    BKE_reportf(reports, RPT_ERROR, "Index %d is invalid", index);
    return nullptr;
  }

  mtex = BKE_texture_mtex_add_id(self_id, index);

  /* for redraw only */
  WM_event_add_notifier(C, NC_TEXTURE, CTX_data_scene(C));

  return mtex;
}

void rna_mtex_texture_slots_clear(ID *self_id, bContext *C, ReportList *reports, int index)
{
  MTex **mtex_ar;
  short act;

  give_active_mtex(self_id, &mtex_ar, &act);

  if (mtex_ar == nullptr) {
    BKE_report(reports, RPT_ERROR, "Mtex not found for this type");
    return;
  }

  if (index < 0 || index >= MAX_MTEX) {
    BKE_reportf(reports, RPT_ERROR, "Index %d is invalid", index);
    return;
  }

  if (mtex_ar[index]) {
    id_us_min((ID *)mtex_ar[index]->tex);
    MEM_freeN(mtex_ar[index]);
    mtex_ar[index] = nullptr;
    DEG_id_tag_update(self_id, 0);
  }

  /* for redraw only */
  WM_event_add_notifier(C, NC_TEXTURE, CTX_data_scene(C));
}

static void rna_TexPaintSlot_uv_layer_get(PointerRNA *ptr, char *value)
{
  TexPaintSlot *data = (TexPaintSlot *)(ptr->data);

  if (data->uvname != nullptr) {
    strcpy(value, data->uvname);
  }
  else {
    value[0] = '\0';
  }
}

static int rna_TexPaintSlot_uv_layer_length(PointerRNA *ptr)
{
  TexPaintSlot *data = (TexPaintSlot *)(ptr->data);
  return data->uvname == nullptr ? 0 : strlen(data->uvname);
}

static void rna_TexPaintSlot_uv_layer_set(PointerRNA *ptr, const char *value)
{
  TexPaintSlot *data = (TexPaintSlot *)(ptr->data);

  if (data->uvname != nullptr) {
    BLI_strncpy_utf8(data->uvname, value, MAX_CUSTOMDATA_LAYER_NAME_NO_PREFIX);
  }
}

static void rna_TexPaintSlot_name_get(PointerRNA *ptr, char *value)
{
  TexPaintSlot *data = (TexPaintSlot *)(ptr->data);

  if (data->ima != nullptr) {
    strcpy(value, data->ima->id.name + 2);
    return;
  }

  if (data->attribute_name != nullptr) {
    strcpy(value, data->attribute_name);
    return;
  }

  value[0] = '\0';
}

static int rna_TexPaintSlot_name_length(PointerRNA *ptr)
{
  TexPaintSlot *data = (TexPaintSlot *)(ptr->data);
  if (data->ima != nullptr) {
    return strlen(data->ima->id.name) - 2;
  }
  if (data->attribute_name != nullptr) {
    return strlen(data->attribute_name);
  }

  return 0;
}

static int rna_TexPaintSlot_icon_get(PointerRNA *ptr)
{
  TexPaintSlot *data = (TexPaintSlot *)(ptr->data);
  if (data->ima != nullptr) {
    return ICON_IMAGE;
  }
  if (data->attribute_name != nullptr) {
    return ICON_COLOR;
  }

  return ICON_NONE;
}

static bool rna_is_grease_pencil_get(PointerRNA *ptr)
{
  Material *ma = (Material *)ptr->data;
  if (ma->gp_style != nullptr) {
    return true;
  }

  return false;
}

static std::optional<std::string> rna_GpencilColorData_path(const PointerRNA * /*ptr*/)
{
  return "grease_pencil";
}

static bool rna_GpencilColorData_is_stroke_visible_get(PointerRNA *ptr)
{
  MaterialGPencilStyle *pcolor = static_cast<MaterialGPencilStyle *>(ptr->data);
  return (pcolor->stroke_rgba[3] > GPENCIL_ALPHA_OPACITY_THRESH);
}

static bool rna_GpencilColorData_is_fill_visible_get(PointerRNA *ptr)
{
  MaterialGPencilStyle *pcolor = (MaterialGPencilStyle *)ptr->data;
  return ((pcolor->fill_rgba[3] > GPENCIL_ALPHA_OPACITY_THRESH) || (pcolor->fill_style > 0));
}

static void rna_GpencilColorData_stroke_image_set(PointerRNA *ptr,
                                                  PointerRNA value,
                                                  ReportList * /*reports*/)
{
  MaterialGPencilStyle *pcolor = static_cast<MaterialGPencilStyle *>(ptr->data);
  ID *id = static_cast<ID *>(value.data);

  id_us_plus(id);
  pcolor->sima = (Image *)id;
}

static void rna_GpencilColorData_fill_image_set(PointerRNA *ptr,
                                                PointerRNA value,
                                                ReportList * /*reports*/)
{
  MaterialGPencilStyle *pcolor = (MaterialGPencilStyle *)ptr->data;
  ID *id = static_cast<ID *>(value.data);

  id_us_plus(id);
  pcolor->ima = (Image *)id;
}

#else

static void rna_def_material_display(StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "diffuse_color", PROP_FLOAT, PROP_COLOR);
  RNA_def_property_float_sdna(prop, nullptr, "r");
  RNA_def_property_array(prop, 4);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop, "Diffuse Color", "Diffuse color of the material");
  /* See #82514 for details, for now re-define defaults here. Keep in sync with
   * #DNA_material_defaults.h */
  static const float diffuse_color_default[4] = {0.8f, 0.8f, 0.8f, 1.0f};
  RNA_def_property_float_array_default(prop, diffuse_color_default);
  RNA_def_property_update(prop, 0, "rna_Material_draw_update");

  prop = RNA_def_property(srna, "specular_color", PROP_FLOAT, PROP_COLOR);
  RNA_def_property_float_sdna(prop, nullptr, "specr");
  RNA_def_property_array(prop, 3);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop, "Specular Color", "Specular color of the material");
  RNA_def_property_update(prop, 0, "rna_Material_draw_update");

  prop = RNA_def_property(srna, "roughness", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "roughness");
  RNA_def_property_range(prop, 0, 1);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop, "Roughness", "Roughness of the material");
  RNA_def_property_update(prop, 0, "rna_Material_draw_update");

  prop = RNA_def_property(srna, "specular_intensity", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "spec");
  RNA_def_property_range(prop, 0, 1);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop, "Specular", "How intense (bright) the specular reflection is");
  RNA_def_property_update(prop, 0, "rna_Material_draw_update");

  prop = RNA_def_property(srna, "metallic", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "metallic");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop, "Metallic", "Amount of mirror reflection for raytrace");
  RNA_def_property_update(prop, 0, "rna_Material_update");

  /* Freestyle line color */
  prop = RNA_def_property(srna, "line_color", PROP_FLOAT, PROP_COLOR);
  RNA_def_property_float_sdna(prop, nullptr, "line_col");
  RNA_def_property_array(prop, 4);
  RNA_def_property_ui_text(prop, "Line Color", "Line color used for Freestyle line rendering");
  RNA_def_property_update(prop, 0, "rna_Material_update");

  prop = RNA_def_property(srna, "line_priority", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "line_priority");
  RNA_def_property_range(prop, 0, 32767);
  RNA_def_property_ui_text(
      prop, "Line Priority", "The line color of a higher priority is used at material boundaries");
  RNA_def_property_update(prop, 0, "rna_Material_update");
}

static void rna_def_material_greasepencil(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  /* mode type styles */
  static const EnumPropertyItem gpcolordata_mode_types_items[] = {
      {GP_MATERIAL_MODE_LINE, "LINE", 0, "Line", "Draw strokes using a continuous line"},
      {GP_MATERIAL_MODE_DOT, "DOTS", 0, "Dots", "Draw strokes using separated dots"},
      {GP_MATERIAL_MODE_SQUARE, "BOX", 0, "Squares", "Draw strokes using separated squares"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  /* stroke styles */
  static const EnumPropertyItem stroke_style_items[] = {
      {GP_MATERIAL_STROKE_STYLE_SOLID, "SOLID", 0, "Solid", "Draw strokes with solid color"},
      {GP_MATERIAL_STROKE_STYLE_TEXTURE, "TEXTURE", 0, "Texture", "Draw strokes using texture"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  /* fill styles */
  static const EnumPropertyItem fill_style_items[] = {
      {GP_MATERIAL_FILL_STYLE_SOLID, "SOLID", 0, "Solid", "Fill area with solid color"},
      {GP_MATERIAL_FILL_STYLE_GRADIENT,
       "GRADIENT",
       0,
       "Gradient",
       "Fill area with gradient color"},
      {GP_MATERIAL_FILL_STYLE_TEXTURE, "TEXTURE", 0, "Texture", "Fill area with image texture"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem fill_gradient_items[] = {
      {GP_MATERIAL_GRADIENT_LINEAR, "LINEAR", 0, "Linear", "Fill area with gradient color"},
      {GP_MATERIAL_GRADIENT_RADIAL, "RADIAL", 0, "Radial", "Fill area with radial gradient"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem alignment_draw_items[] = {
      {GP_MATERIAL_FOLLOW_PATH,
       "PATH",
       0,
       "Path",
       "Follow stroke drawing path and object rotation"},
      {GP_MATERIAL_FOLLOW_OBJ, "OBJECT", 0, "Object", "Follow object rotation only"},
      {GP_MATERIAL_FOLLOW_FIXED,
       "FIXED",
       0,
       "Fixed",
       "Do not follow drawing path or object rotation and keeps aligned with viewport"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  srna = RNA_def_struct(brna, "MaterialGPencilStyle", nullptr);
  RNA_def_struct_sdna(srna, "MaterialGPencilStyle");
  RNA_def_struct_ui_text(srna, "Grease Pencil Color", "");
  RNA_def_struct_path_func(srna, "rna_GpencilColorData_path");

  prop = RNA_def_property(srna, "color", PROP_FLOAT, PROP_COLOR);
  RNA_def_property_range(prop, 0.0, 1.0);
  RNA_def_property_float_sdna(prop, nullptr, "stroke_rgba");
  RNA_def_property_array(prop, 4);
  RNA_def_property_ui_text(prop, "Color", "");
  RNA_def_property_update(prop, NC_GPENCIL | ND_SHADING, "rna_MaterialGpencil_update");

  /* Fill Drawing Color */
  prop = RNA_def_property(srna, "fill_color", PROP_FLOAT, PROP_COLOR);
  RNA_def_property_float_sdna(prop, nullptr, "fill_rgba");
  RNA_def_property_array(prop, 4);
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Fill Color", "Color for filling region bounded by each stroke");
  RNA_def_property_update(prop, NC_GPENCIL | ND_SHADING, "rna_MaterialGpencil_update");

  /* Secondary Drawing Color */
  prop = RNA_def_property(srna, "mix_color", PROP_FLOAT, PROP_COLOR);
  RNA_def_property_float_sdna(prop, nullptr, "mix_rgba");
  RNA_def_property_array(prop, 4);
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Mix Color", "Color for mixing with primary filling color");
  RNA_def_property_update(prop, NC_GPENCIL | ND_SHADING, "rna_MaterialGpencil_update");

  /* Mix factor */
  prop = RNA_def_property(srna, "mix_factor", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "mix_factor");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Mix", "Mix Factor");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_GPENCIL);
  RNA_def_property_update(prop, NC_GPENCIL | ND_SHADING, "rna_MaterialGpencil_update");

  /* Stroke Mix factor */
  prop = RNA_def_property(srna, "mix_stroke_factor", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "mix_stroke_factor");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Mix", "Mix Stroke Factor");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_GPENCIL);
  RNA_def_property_update(prop, NC_GPENCIL | ND_SHADING, "rna_MaterialGpencil_update");

  /* Texture angle */
  prop = RNA_def_property(srna, "texture_angle", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_float_sdna(prop, nullptr, "texture_angle");
  RNA_def_property_ui_text(prop, "Angle", "Texture Orientation Angle");
  RNA_def_property_update(prop, NC_GPENCIL | ND_SHADING, "rna_MaterialGpencil_update");

  /* Scale factor for texture */
  prop = RNA_def_property(srna, "texture_scale", PROP_FLOAT, PROP_COORDS);
  RNA_def_property_float_sdna(prop, nullptr, "texture_scale");
  RNA_def_property_array(prop, 2);
  RNA_def_property_ui_text(prop, "Scale", "Scale Factor for Texture");
  RNA_def_property_update(prop, NC_GPENCIL | ND_SHADING, "rna_MaterialGpencil_update");

  /* Shift factor to move texture in 2d space */
  prop = RNA_def_property(srna, "texture_offset", PROP_FLOAT, PROP_COORDS);
  RNA_def_property_float_sdna(prop, nullptr, "texture_offset");
  RNA_def_property_array(prop, 2);
  RNA_def_property_ui_text(prop, "Offset", "Shift Texture in 2d Space");
  RNA_def_property_update(prop, NC_GPENCIL | ND_SHADING, "rna_MaterialGpencil_update");

  /* texture pixsize factor (used for UV along the stroke) */
  prop = RNA_def_property(srna, "pixel_size", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "texture_pixsize");
  RNA_def_property_range(prop, 1, 5000);
  RNA_def_property_ui_text(prop, "UV Factor", "Texture Pixel Size factor along the stroke");
  RNA_def_property_update(prop, NC_GPENCIL | ND_SHADING, "rna_MaterialGpencil_update");

  /* Flags */
  prop = RNA_def_property(srna, "hide", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_MATERIAL_HIDE);
  RNA_def_property_ui_icon(prop, ICON_HIDE_OFF, -1);
  RNA_def_property_ui_text(prop, "Hide", "Set color Visibility");
  RNA_def_property_update(prop, NC_GPENCIL | ND_SHADING, "rna_MaterialGpencil_update");

  prop = RNA_def_property(srna, "lock", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_MATERIAL_LOCKED);
  RNA_def_property_ui_icon(prop, ICON_UNLOCKED, 1);
  RNA_def_property_ui_text(
      prop, "Locked", "Protect color from further editing and/or frame changes");
  RNA_def_property_update(prop, NC_GPENCIL | ND_SHADING, "rna_MaterialGpencil_update");

  prop = RNA_def_property(srna, "ghost", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_MATERIAL_HIDE_ONIONSKIN);
  RNA_def_property_ui_icon(prop, ICON_GHOST_ENABLED, 0);
  RNA_def_property_ui_text(
      prop, "Show in Ghosts", "Display strokes using this color when showing onion skins");
  RNA_def_property_update(prop, NC_GPENCIL | ND_SHADING, "rna_MaterialGpencil_update");

  prop = RNA_def_property(srna, "texture_clamp", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_MATERIAL_TEX_CLAMP);
  RNA_def_property_ui_text(prop, "Clamp", "Do not repeat texture and clamp to one instance only");
  RNA_def_property_update(prop, NC_GPENCIL | ND_SHADING, "rna_MaterialGpencil_update");

  prop = RNA_def_property(srna, "flip", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_MATERIAL_FLIP_FILL);
  RNA_def_property_ui_text(prop, "Flip", "Flip filling colors");
  RNA_def_property_update(prop, NC_GPENCIL | ND_SHADING, "rna_MaterialGpencil_update");

  prop = RNA_def_property(srna, "use_overlap_strokes", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_MATERIAL_DISABLE_STENCIL);
  RNA_def_property_ui_text(
      prop, "Self Overlap", "Disable stencil and overlap self intersections with alpha materials");
  RNA_def_property_update(prop, NC_GPENCIL | ND_SHADING, "rna_MaterialGpencil_update");

  prop = RNA_def_property(srna, "use_stroke_holdout", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_MATERIAL_IS_STROKE_HOLDOUT);
  RNA_def_property_ui_text(
      prop, "Holdout", "Remove the color from underneath this stroke by using it as a mask");
  RNA_def_property_update(prop, NC_GPENCIL | ND_SHADING, "rna_MaterialGpencil_update");

  prop = RNA_def_property(srna, "use_fill_holdout", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_MATERIAL_IS_FILL_HOLDOUT);
  RNA_def_property_ui_text(
      prop, "Holdout", "Remove the color from underneath this stroke by using it as a mask");
  RNA_def_property_update(prop, NC_GPENCIL | ND_SHADING, "rna_MaterialGpencil_update");

  prop = RNA_def_property(srna, "show_stroke", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_MATERIAL_STROKE_SHOW);
  RNA_def_property_ui_text(prop, "Show Stroke", "Show stroke lines of this material");
  RNA_def_property_update(prop, NC_GPENCIL | ND_SHADING, "rna_MaterialGpencil_update");

  prop = RNA_def_property(srna, "show_fill", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_MATERIAL_FILL_SHOW);
  RNA_def_property_ui_text(prop, "Show Fill", "Show stroke fills of this material");
  RNA_def_property_update(prop, NC_GPENCIL | ND_SHADING, "rna_MaterialGpencil_update");

  /* Mode to align Dots and Boxes to drawing path and object rotation */
  prop = RNA_def_property(srna, "alignment_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, nullptr, "alignment_mode");
  RNA_def_property_enum_items(prop, alignment_draw_items);
  RNA_def_property_ui_text(
      prop, "Alignment", "Defines how align Dots and Boxes with drawing path and object rotation");
  RNA_def_property_update(prop, NC_GPENCIL | ND_SHADING, "rna_MaterialGpencil_update");

  /* Rotation of texture for Dots or Strokes. */
  prop = RNA_def_property(srna, "alignment_rotation", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_float_sdna(prop, nullptr, "alignment_rotation");
  RNA_def_property_float_default(prop, 0.0f);
  RNA_def_property_range(prop, -DEG2RADF(90.0f), DEG2RADF(90.0f));
  RNA_def_property_ui_range(prop, -DEG2RADF(90.0f), DEG2RADF(90.0f), 10, 3);
  RNA_def_property_ui_text(prop,
                           "Rotation",
                           "Additional rotation applied to dots and square texture of strokes. "
                           "Only applies in texture shading mode.");
  RNA_def_property_update(prop, NC_GPENCIL | ND_SHADING, "rna_MaterialGpencil_update");

  /* pass index for future compositing and editing tools */
  prop = RNA_def_property(srna, "pass_index", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, nullptr, "index");
  RNA_def_property_ui_text(prop, "Pass Index", "Index number for the \"Color Index\" pass");
  RNA_def_property_update(prop, NC_GPENCIL | ND_SHADING, "rna_MaterialGpencil_update");

  /* mode type */
  prop = RNA_def_property(srna, "mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, nullptr, "mode");
  RNA_def_property_enum_items(prop, gpcolordata_mode_types_items);
  RNA_def_property_ui_text(prop, "Line Type", "Select line type for strokes");
  RNA_def_property_update(prop, NC_GPENCIL | ND_SHADING, "rna_MaterialGpencil_update");

  /* stroke style */
  prop = RNA_def_property(srna, "stroke_style", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, nullptr, "stroke_style");
  RNA_def_property_enum_items(prop, stroke_style_items);
  RNA_def_property_ui_text(prop, "Stroke Style", "Select style used to draw strokes");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_GPENCIL);
  RNA_def_property_update(prop, NC_GPENCIL | ND_SHADING, "rna_MaterialGpencil_update");

  /* stroke image texture */
  prop = RNA_def_property(srna, "stroke_image", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "sima");
  RNA_def_property_pointer_funcs(
      prop, nullptr, "rna_GpencilColorData_stroke_image_set", nullptr, nullptr);
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop, "Image", "");
  RNA_def_property_update(prop, NC_GPENCIL | ND_SHADING, "rna_MaterialGpencil_update");

  /* fill style */
  prop = RNA_def_property(srna, "fill_style", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, nullptr, "fill_style");
  RNA_def_property_enum_items(prop, fill_style_items);
  RNA_def_property_ui_text(prop, "Fill Style", "Select style used to fill strokes");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_GPENCIL);
  RNA_def_property_update(prop, NC_GPENCIL | ND_SHADING, "rna_MaterialGpencil_update");

  /* gradient type */
  prop = RNA_def_property(srna, "gradient_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, nullptr, "gradient_type");
  RNA_def_property_enum_items(prop, fill_gradient_items);
  RNA_def_property_ui_text(prop, "Gradient Type", "Select type of gradient used to fill strokes");
  RNA_def_property_update(prop, NC_GPENCIL | ND_SHADING, "rna_MaterialGpencil_update");

  /* fill image texture */
  prop = RNA_def_property(srna, "fill_image", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "ima");
  RNA_def_property_pointer_funcs(
      prop, nullptr, "rna_GpencilColorData_fill_image_set", nullptr, nullptr);
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop, "Image", "");
  RNA_def_property_update(prop, NC_GPENCIL | ND_SHADING, "rna_MaterialGpencil_update");

  /* Read-only state props (for simpler UI code) */
  prop = RNA_def_property(srna, "is_stroke_visible", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(prop, "rna_GpencilColorData_is_stroke_visible_get", nullptr);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(
      prop, "Is Stroke Visible", "True when opacity of stroke is set high enough to be visible");

  prop = RNA_def_property(srna, "is_fill_visible", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(prop, "rna_GpencilColorData_is_fill_visible_get", nullptr);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(
      prop, "Is Fill Visible", "True when opacity of fill is set high enough to be visible");
}
static void rna_def_material_lineart(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "MaterialLineArt", nullptr);
  RNA_def_struct_sdna(srna, "MaterialLineArt");
  RNA_def_struct_ui_text(srna, "Material Line Art", "");
  RNA_def_struct_path_func(srna, "rna_MaterialLineArt_path");

  prop = RNA_def_property(srna, "use_material_mask", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_default(prop, false);
  RNA_def_property_boolean_sdna(prop, nullptr, "flags", LRT_MATERIAL_MASK_ENABLED);
  RNA_def_property_ui_text(
      prop, "Use Material Mask", "Use material masks to filter out occluded strokes");
  RNA_def_property_update(prop, NC_GPENCIL | ND_SHADING, "rna_MaterialLineArt_update");

  prop = RNA_def_property(srna, "use_material_mask_bits", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_default(prop, false);
  RNA_def_property_boolean_bitset_array_sdna(prop, nullptr, "material_mask_bits", 1 << 0, 8);
  RNA_def_property_ui_text(prop, "Mask", "");
  RNA_def_property_update(prop, NC_GPENCIL | ND_SHADING, "rna_MaterialLineArt_update");

  prop = RNA_def_property(srna, "mat_occlusion", PROP_INT, PROP_NONE);
  RNA_def_property_int_default(prop, 1);
  RNA_def_property_ui_range(prop, 0.0f, 5.0f, 1.0f, 1);
  RNA_def_property_ui_text(
      prop,
      "Effectiveness",
      "Faces with this material will behave as if it has set number of layers in occlusion");
  RNA_def_property_update(prop, NC_GPENCIL | ND_SHADING, "rna_MaterialLineArt_update");

  prop = RNA_def_property(srna, "intersection_priority", PROP_INT, PROP_NONE);
  RNA_def_property_range(prop, 0, 255);
  RNA_def_property_ui_text(prop,
                           "Intersection Priority",
                           "The intersection line will be included into the object with the "
                           "higher intersection priority value");
  RNA_def_property_update(prop, NC_GPENCIL | ND_SHADING, "rna_MaterialLineArt_update");

  prop = RNA_def_property(srna, "use_intersection_priority_override", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_default(prop, false);
  RNA_def_property_boolean_sdna(prop, nullptr, "flags", LRT_MATERIAL_CUSTOM_INTERSECTION_PRIORITY);
  RNA_def_property_ui_text(prop,
                           "Use Intersection Priority",
                           "Override object and collection intersection priority value");
  RNA_def_property_update(prop, NC_GPENCIL | ND_SHADING, "rna_MaterialLineArt_update");
}

void RNA_def_material(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  /* Render Preview Types */
  static const EnumPropertyItem preview_type_items[] = {
      {MA_FLAT, "FLAT", ICON_MATPLANE, "Flat", "Flat XY plane"},
      {MA_SPHERE, "SPHERE", ICON_MATSPHERE, "Sphere", "Sphere"},
      {MA_CUBE, "CUBE", ICON_MATCUBE, "Cube", "Cube"},
      {MA_HAIR, "HAIR", ICON_CURVES, "Hair", "Hair strands"},
      {MA_SHADERBALL, "SHADERBALL", ICON_MATSHADERBALL, "Shader Ball", "Shader ball"},
      {MA_CLOTH, "CLOTH", ICON_MATCLOTH, "Cloth", "Cloth"},
      {MA_FLUID, "FLUID", ICON_MATFLUID, "Fluid", "Fluid"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem prop_eevee_volume_isect_method_items[] = {
      {MA_VOLUME_ISECT_FAST,
       "FAST",
       0,
       "Fast",
       "Each face is considered as a medium interface. Gives correct results for manifold "
       "geometry that contains no inner parts."},
      {MA_VOLUME_ISECT_ACCURATE,
       "ACCURATE",
       0,
       "Accurate",
       "Faces are considered as medium interface only when they have different consecutive "
       "facing. Gives correct results as long as the max ray depth is not exceeded. Have "
       "significant memory overhead compared to the fast method."},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem prop_eevee_thickness_method_items[] = {
      {MA_THICKNESS_SPHERE,
       "SPHERE",
       0,
       "Sphere",
       "Approximate the object as a sphere whose diameter is equal to the thickness defined by "
       "the node tree"},
      {MA_THICKNESS_SLAB,
       "SLAB",
       0,
       "Slab",
       "Approximate the object as an infinite slab of thickness defined by the node tree"},
      {0, nullptr, 0, nullptr, nullptr},
  };

#  if 1 /* Delete this section once we remove old eevee. */
  static const EnumPropertyItem prop_eevee_blend_items[] = {
      {MA_BM_SOLID, "OPAQUE", 0, "Opaque", "Render surface without transparency"},
      {MA_BM_CLIP,
       "CLIP",
       0,
       "Alpha Clip",
       "Use the alpha threshold to clip the visibility (binary visibility)"},
      {MA_BM_HASHED,
       "HASHED",
       0,
       "Alpha Hashed",
       "Use noise to dither the binary visibility (works well with multi-samples)"},
      {MA_BM_BLEND,
       "BLEND",
       0,
       "Alpha Blend",
       "Render polygon transparent, depending on alpha channel of the texture"},
      {0, nullptr, 0, nullptr, nullptr},
  };
#  endif

  static const EnumPropertyItem prop_eevee_surface_render_method_items[] = {
      {MA_SURFACE_METHOD_DEFERRED,
       "DITHERED",
       0,
       "Dithered",
       "Allows for grayscale hashed transparency, and compatible with render passes and "
       "raytracing. Also known as deferred rendering."},
      {MA_SURFACE_METHOD_FORWARD,
       "BLENDED",
       0,
       "Blended",
       "Allows for colored transparency, but incompatible with render passes and raytracing. Also "
       "known as forward rendering."},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem prop_displacement_method_items[] = {
      {MA_DISPLACEMENT_BUMP,
       "BUMP",
       0,
       "Bump Only",
       "Bump mapping to simulate the appearance of displacement"},
      {MA_DISPLACEMENT_DISPLACE,
       "DISPLACEMENT",
       0,
       "Displacement Only",
       "Use true displacement of surface only, requires fine subdivision"},
      {MA_DISPLACEMENT_BOTH,
       "BOTH",
       0,
       "Displacement and Bump",
       "Combination of true displacement and bump mapping for finer detail"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  srna = RNA_def_struct(brna, "Material", "ID");
  RNA_def_struct_ui_text(
      srna,
      "Material",
      "Material data-block to define the appearance of geometric objects for rendering");
  RNA_def_struct_ui_icon(srna, ICON_MATERIAL_DATA);

  prop = RNA_def_property(srna, "surface_render_method", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, prop_eevee_surface_render_method_items);
  RNA_def_property_ui_text(prop,
                           "Surface Render Method",
                           "Controls the blending and the compatibility with certain features");
  /* Setter function for forward compatibility. */
  RNA_def_property_enum_funcs(prop, nullptr, "rna_Material_render_method_set", nullptr);
  RNA_def_property_update(prop, 0, "rna_Material_draw_update");

  prop = RNA_def_property(srna, "displacement_method", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, prop_displacement_method_items);
  RNA_def_property_ui_text(prop, "Displacement Method", "Method to use for the displacement");
  RNA_def_property_update(prop, 0, "rna_Material_draw_update");

#  if 1 /* Delete this section once we remove old eevee. */
  /* Blending (only Eevee for now) */
  prop = RNA_def_property(srna, "blend_method", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, prop_eevee_blend_items);
  RNA_def_property_ui_text(
      prop,
      "Blend Mode",
      "Blend Mode for Transparent Faces (Deprecated: use 'surface_render_method')");
  RNA_def_property_enum_funcs(
      prop, "rna_Material_blend_method_get", "rna_Material_blend_method_set", nullptr);
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_MATERIAL);
  RNA_def_property_update(prop, 0, "rna_Material_draw_update");

  prop = RNA_def_property(srna, "alpha_threshold", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_range(prop, 0, 1);
  RNA_def_property_ui_text(prop,
                           "Clip Threshold",
                           "A pixel is rendered only if its alpha value is above this threshold");
  RNA_def_property_update(prop, 0, "rna_Material_draw_update");
#  endif

  prop = RNA_def_property(srna, "use_transparency_overlap", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "blend_flag", MA_BL_HIDE_BACKFACE);
  RNA_def_property_ui_text(prop,
                           "Use Transparency Overlap",
                           "Render multiple transparent layers "
                           "(may introduce transparency sorting problems)");

#  if 1 /* This should be deleted in Blender 4.5 */
  RNA_def_property_update(prop, 0, "rna_Material_draw_update");
  prop = RNA_def_property(srna, "show_transparent_back", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "blend_flag", MA_BL_HIDE_BACKFACE);
  RNA_def_property_ui_text(
      prop,
      "Show Backface",
      "Render multiple transparent layers "
      "(may introduce transparency sorting problems) (Deprecated: use 'use_tranparency_overlap')");
  RNA_def_property_update(prop, 0, "rna_Material_draw_update");
#  endif

  prop = RNA_def_property(srna, "use_backface_culling", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "blend_flag", MA_BL_CULL_BACKFACE);
  RNA_def_property_ui_text(
      prop, "Backface Culling", "Use back face culling to hide the back side of faces");
  RNA_def_property_update(prop, 0, "rna_Material_draw_update");

  prop = RNA_def_property(srna, "use_backface_culling_shadow", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "blend_flag", MA_BL_CULL_BACKFACE_SHADOW);
  RNA_def_property_ui_text(
      prop, "Shadow Backface Culling", "Use back face culling when casting shadows");
  RNA_def_property_update(prop, 0, "rna_Material_draw_update");

  prop = RNA_def_property(srna, "use_backface_culling_lightprobe_volume", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(
      prop, nullptr, "blend_flag", MA_BL_LIGHTPROBE_VOLUME_DOUBLE_SIDED);
  RNA_def_property_ui_text(
      prop,
      "Light Probe Volume Backface Culling",
      "Consider material single sided for light probe volume capture. "
      "Additionally helps rejecting probes inside the object to avoid light leaks.");
  RNA_def_property_update(prop, 0, "rna_Material_draw_update");

  prop = RNA_def_property(srna, "use_transparent_shadow", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "blend_flag", MA_BL_TRANSPARENT_SHADOW);
  RNA_def_property_boolean_funcs(prop, nullptr, "rna_Material_transparent_shadow_set");
  RNA_def_property_ui_text(
      prop,
      "Transparent Shadows",
      "Use transparent shadows for this material if it contains a Transparent BSDF, "
      "disabling will render faster but not give accurate shadows");
  RNA_def_property_update(prop, 0, "rna_Material_draw_update");

  prop = RNA_def_property(srna, "use_raytrace_refraction", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "blend_flag", MA_BL_SS_REFRACTION);
  RNA_def_property_ui_text(
      prop,
      "Raytrace Transmission",
      "Use raytracing to determine transmitted color instead of using only light probes. "
      "This prevents the surface from contributing to the lighting of surfaces not using this "
      "setting.");
  RNA_def_property_update(prop, 0, "rna_Material_draw_update");

#  if 1 /* This should be deleted in Blender 4.5 */
  prop = RNA_def_property(srna, "use_screen_refraction", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "blend_flag", MA_BL_SS_REFRACTION);
  RNA_def_property_ui_text(
      prop,
      "Raytrace Transmission",
      "Use raytracing to determine transmitted color instead of using only light probes. "
      "This prevents the surface from contributing to the lighting of surfaces not using this "
      "setting. Deprecated: use 'use_raytrace_refraction'.");
  RNA_def_property_update(prop, 0, "rna_Material_draw_update");

  prop = RNA_def_property(srna, "use_sss_translucency", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "blend_flag", MA_BL_TRANSLUCENCY);
  RNA_def_property_ui_text(
      prop, "Subsurface Translucency", "Add translucency effect to subsurface (Deprecated)");
  RNA_def_property_update(prop, 0, "rna_Material_draw_update");

  prop = RNA_def_property(srna, "refraction_depth", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, nullptr, "refract_depth");
  RNA_def_property_range(prop, 0.0f, FLT_MAX);
  RNA_def_property_ui_text(prop,
                           "Refraction Depth",
                           "Approximate the thickness of the object to compute two refraction "
                           "events (0 is disabled) (Deprecated)");
  RNA_def_property_update(prop, 0, "rna_Material_draw_update");
#  endif

  prop = RNA_def_property(srna, "thickness_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, prop_eevee_thickness_method_items);
  RNA_def_property_ui_text(prop,
                           "Thickness Mode",
                           "Approximation used to model the light interactions inside the object");
  RNA_def_property_update(prop, 0, "rna_Material_draw_update");

  prop = RNA_def_property(srna, "use_thickness_from_shadow", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "blend_flag", MA_BL_THICKNESS_FROM_SHADOW);
  RNA_def_property_ui_text(prop,
                           "Thickness From Shadow",
                           "Use the shadow maps from shadow casting lights "
                           "to refine the thickness defined by the material node tree");
  RNA_def_property_update(prop, 0, "rna_Material_draw_update");

  prop = RNA_def_property(srna, "volume_intersection_method", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, prop_eevee_volume_isect_method_items);
  RNA_def_property_ui_text(
      prop,
      "Volume Intersection Method",
      "Determines which inner part of the mesh will produce volumetric effect");
  RNA_def_property_update(prop, 0, "rna_Material_draw_update");

  prop = RNA_def_property(srna, "max_vertex_displacement", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, nullptr, "inflate_bounds");
  RNA_def_property_range(prop, 0.0f, FLT_MAX);
  RNA_def_property_ui_text(prop,
                           "Max Vertex Displacement",
                           "The max distance a vertex can be displaced. "
                           "Displacements over this threshold may cause visibility issues.");
  RNA_def_property_update(prop, 0, "rna_Material_draw_update");

  /* For Preview Render */
  prop = RNA_def_property(srna, "preview_render_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "pr_type");
  RNA_def_property_enum_items(prop, preview_type_items);
  RNA_def_property_ui_text(prop, "Preview Render Type", "Type of preview render");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_MATERIAL);
  RNA_def_property_update(prop, 0, "rna_Material_update_previews");

  prop = RNA_def_property(srna, "use_preview_world", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "pr_flag", MA_PREVIEW_WORLD);
  RNA_def_property_ui_text(
      prop, "Preview World", "Use the current world background to light the preview render");
  RNA_def_property_update(prop, 0, "rna_Material_update_previews");

  prop = RNA_def_property(srna, "pass_index", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, nullptr, "index");
  RNA_def_property_ui_text(
      prop, "Pass Index", "Index number for the \"Material Index\" render pass");
  RNA_def_property_update(prop, NC_OBJECT, "rna_Material_update");

  /* nodetree */
  prop = RNA_def_property(srna, "node_tree", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "nodetree");
  RNA_def_property_clear_flag(prop, PROP_PTR_NO_OWNERSHIP);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop, "Node Tree", "Node tree for node based materials");

  prop = RNA_def_property(srna, "use_nodes", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "use_nodes", 1);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Use Nodes", "Use shader nodes to render the material");
  RNA_def_property_boolean_funcs(prop, "rna_Material_use_nodes_get", "rna_Material_use_nodes_set");
  RNA_def_property_deprecated(prop,
                              "Unused but kept for compatibility reasons. Setting the property "
                              "has no effect, and getting it always returns True.",
                              500,
                              600);

  /* common */
  rna_def_animdata_common(srna);
  rna_def_texpaint_slots(brna, srna);

  rna_def_material_display(srna);

  /* grease pencil */
  prop = RNA_def_property(srna, "grease_pencil", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "gp_style");
  RNA_def_property_ui_text(
      prop, "Grease Pencil Settings", "Grease Pencil color settings for material");

  prop = RNA_def_property(srna, "is_grease_pencil", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(prop, "rna_is_grease_pencil_get", nullptr);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(
      prop, "Is Grease Pencil", "True if this material has Grease Pencil data");

  /* line art */
  prop = RNA_def_property(srna, "lineart", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "lineart");
  RNA_def_property_ui_text(prop, "Line Art Settings", "Line Art settings for material");

  rna_def_material_greasepencil(brna);
  rna_def_material_lineart(brna);

  RNA_api_material(srna);
}

static void rna_def_texture_slots(BlenderRNA *brna,
                                  PropertyRNA *cprop,
                                  const char *structname,
                                  const char *structname_slots)
{
  StructRNA *srna;

  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, structname_slots);
  srna = RNA_def_struct(brna, structname_slots, nullptr);
  RNA_def_struct_sdna(srna, "ID");
  RNA_def_struct_ui_text(srna, "Texture Slots", "Collection of texture slots");

  /* functions */
  func = RNA_def_function(srna, "add", "rna_mtex_texture_slots_add");
  RNA_def_function_flag(func,
                        FUNC_USE_SELF_ID | FUNC_NO_SELF | FUNC_USE_CONTEXT | FUNC_USE_REPORTS);
  parm = RNA_def_pointer(func, "mtex", structname, "", "The newly initialized mtex");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "create", "rna_mtex_texture_slots_create");
  RNA_def_function_flag(func,
                        FUNC_USE_SELF_ID | FUNC_NO_SELF | FUNC_USE_CONTEXT | FUNC_USE_REPORTS);
  parm = RNA_def_int(
      func, "index", 0, 0, INT_MAX, "Index", "Slot index to initialize", 0, INT_MAX);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_pointer(func, "mtex", structname, "", "The newly initialized mtex");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "clear", "rna_mtex_texture_slots_clear");
  RNA_def_function_flag(func,
                        FUNC_USE_SELF_ID | FUNC_NO_SELF | FUNC_USE_CONTEXT | FUNC_USE_REPORTS);
  parm = RNA_def_int(func, "index", 0, 0, INT_MAX, "Index", "Slot index to clear", 0, INT_MAX);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
}

void rna_def_mtex_common(BlenderRNA *brna,
                         StructRNA *srna,
                         const char *begin,
                         const char *activeget,
                         const char *activeset,
                         const char *activeeditable,
                         const char *structname,
                         const char *structname_slots,
                         const char *update,
                         const char *update_index)
{
  PropertyRNA *prop;

  /* mtex */
  prop = RNA_def_property(srna, "texture_slots", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(prop, structname);
  RNA_def_property_collection_funcs(prop,
                                    begin,
                                    "rna_iterator_array_next",
                                    "rna_iterator_array_end",
                                    "rna_iterator_array_dereference_get",
                                    nullptr,
                                    nullptr,
                                    nullptr,
                                    nullptr);
  RNA_def_property_ui_text(
      prop, "Textures", "Texture slots defining the mapping and influence of textures");
  rna_def_texture_slots(brna, prop, structname, structname_slots);

  prop = RNA_def_property(srna, "active_texture", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "Texture");
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_REFCOUNT);
  if (activeeditable) {
    RNA_def_property_editable_func(prop, activeeditable);
  }
  RNA_def_property_pointer_funcs(prop, activeget, activeset, nullptr, nullptr);
  RNA_def_property_ui_text(prop, "Active Texture", "Active texture slot being displayed");
  RNA_def_property_update(prop, NC_MATERIAL | ND_SHADING_LINKS, update);

  prop = RNA_def_property(srna, "active_texture_index", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, nullptr, "texact");
  RNA_def_property_range(prop, 0, MAX_MTEX - 1);
  RNA_def_property_ui_text(prop, "Active Texture Index", "Index of active texture slot");
  RNA_def_property_update(prop, NC_MATERIAL | ND_SHADING_LINKS, update_index);
}

static void rna_def_tex_slot(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "TexPaintSlot", nullptr);
  RNA_def_struct_ui_text(
      srna, "Texture Paint Slot", "Slot that contains information about texture painting");

  prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_string_funcs(
      prop, "rna_TexPaintSlot_name_get", "rna_TexPaintSlot_name_length", nullptr);
  RNA_def_property_ui_text(prop, "Name", "Name of the slot");
  RNA_def_struct_name_property(srna, prop);

  prop = RNA_def_property(srna, "icon_value", PROP_INT, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_int_funcs(prop, "rna_TexPaintSlot_icon_get", nullptr, nullptr);
  RNA_def_property_ui_text(prop, "Icon", "Paint slot icon");

  prop = RNA_def_property(srna, "uv_layer", PROP_STRING, PROP_NONE);
  RNA_def_property_string_maxlength(
      prop, MAX_CUSTOMDATA_LAYER_NAME_NO_PREFIX); /* Else it uses the pointer size! */
  RNA_def_property_string_sdna(prop, nullptr, "uvname");
  RNA_def_property_string_funcs(prop,
                                "rna_TexPaintSlot_uv_layer_get",
                                "rna_TexPaintSlot_uv_layer_length",
                                "rna_TexPaintSlot_uv_layer_set");
  RNA_def_property_ui_text(prop, "UV Map", "Name of UV map");
  RNA_def_property_update(prop, NC_GEOM | ND_DATA, "rna_Material_update");

  prop = RNA_def_property(srna, "is_valid", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "valid", 1);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Valid", "Slot has a valid image and UV map");
}

void rna_def_texpaint_slots(BlenderRNA *brna, StructRNA *srna)
{
  PropertyRNA *prop;

  rna_def_tex_slot(brna);

  /* mtex */
  prop = RNA_def_property(srna, "texture_paint_images", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, nullptr, "texpaintslot", nullptr);
  RNA_def_property_collection_funcs(prop,
                                    "rna_Material_texpaint_begin",
                                    "rna_iterator_array_next",
                                    "rna_iterator_array_end",
                                    "rna_iterator_array_dereference_get",
                                    nullptr,
                                    nullptr,
                                    nullptr,
                                    nullptr);
  RNA_def_property_struct_type(prop, "Image");
  RNA_def_property_ui_text(
      prop, "Texture Slot Images", "Texture images used for texture painting");

  prop = RNA_def_property(srna, "texture_paint_slots", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_funcs(prop,
                                    "rna_Material_texpaint_begin",
                                    "rna_iterator_array_next",
                                    "rna_iterator_array_end",
                                    "rna_iterator_array_get",
                                    nullptr,
                                    nullptr,
                                    nullptr,
                                    nullptr);
  RNA_def_property_struct_type(prop, "TexPaintSlot");
  RNA_def_property_ui_text(
      prop, "Texture Slots", "Texture slots defining the mapping and influence of textures");

  prop = RNA_def_property(srna, "paint_active_slot", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_range(prop, 0, SHRT_MAX);
  RNA_def_property_ui_text(
      prop, "Active Paint Texture Index", "Index of active texture paint slot");
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
  RNA_def_property_update(
      prop, NC_MATERIAL | ND_SHADING_LINKS, "rna_Material_active_paint_texture_index_update");

  prop = RNA_def_property(srna, "paint_clone_slot", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_range(prop, 0, SHRT_MAX);
  RNA_def_property_ui_text(prop, "Clone Paint Texture Index", "Index of clone texture paint slot");
  RNA_def_property_update(prop, NC_MATERIAL | ND_SHADING_LINKS, nullptr);
}

#endif
