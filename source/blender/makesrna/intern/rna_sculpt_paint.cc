/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup RNA
 */

#include <cstdlib>

#include "BLI_math_base.h"

#include "BLT_translation.hh"

#include "RNA_define.hh"
#include "RNA_enum_types.hh"

#include "rna_internal.hh"

#include "DNA_brush_types.h"
#include "DNA_scene_types.h"

#include "BKE_paint.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "bmesh.hh"

const EnumPropertyItem rna_enum_particle_edit_hair_brush_items[] = {
    {PE_BRUSH_COMB, "COMB", 0, "Comb", "Comb hairs"},
    {PE_BRUSH_SMOOTH, "SMOOTH", 0, "Smooth", "Smooth hairs"},
    {PE_BRUSH_ADD, "ADD", 0, "Add", "Add hairs"},
    {PE_BRUSH_LENGTH, "LENGTH", 0, "Length", "Make hairs longer or shorter"},
    {PE_BRUSH_PUFF, "PUFF", 0, "Puff", "Make hairs stand up"},
    {PE_BRUSH_CUT, "CUT", 0, "Cut", "Cut hairs"},
    {PE_BRUSH_WEIGHT, "WEIGHT", 0, "Weight", "Weight hair particles"},
    {0, nullptr, 0, nullptr, nullptr},
};

#ifndef RNA_RUNTIME
static const EnumPropertyItem rna_enum_gpencil_lock_axis_items[] = {
    {GP_LOCKAXIS_VIEW,
     "VIEW",
     ICON_RESTRICT_VIEW_ON,
     "View",
     "Align strokes to current view plane"},
    {GP_LOCKAXIS_Y,
     "AXIS_Y",
     ICON_AXIS_FRONT,
     "Front (X-Z)",
     "Project strokes to plane locked to Y"},
    {GP_LOCKAXIS_X,
     "AXIS_X",
     ICON_AXIS_SIDE,
     "Side (Y-Z)",
     "Project strokes to plane locked to X"},
    {GP_LOCKAXIS_Z, "AXIS_Z", ICON_AXIS_TOP, "Top (X-Y)", "Project strokes to plane locked to Z"},
    {GP_LOCKAXIS_CURSOR,
     "CURSOR",
     ICON_PIVOT_CURSOR,
     "Cursor",
     "Align strokes to current 3D cursor orientation"},
    {0, nullptr, 0, nullptr, nullptr},
};

static const EnumPropertyItem rna_enum_gpencil_paint_mode[] = {
    {GPPAINT_FLAG_USE_MATERIAL,
     "MATERIAL",
     0,
     "Material",
     "Paint using the active material base color"},
    {GPPAINT_FLAG_USE_VERTEXCOLOR,
     "VERTEXCOLOR",
     0,
     "Color Attribute",
     "Paint the material with a color attribute"},
    {0, nullptr, 0, nullptr, nullptr},
};
#endif

static const EnumPropertyItem rna_enum_canvas_source_items[] = {
    {PAINT_CANVAS_SOURCE_COLOR_ATTRIBUTE, "COLOR_ATTRIBUTE", 0, "Color Attribute", ""},
    {PAINT_CANVAS_SOURCE_MATERIAL, "MATERIAL", 0, "Material", ""},
    {PAINT_CANVAS_SOURCE_IMAGE, "IMAGE", 0, "Image", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

const EnumPropertyItem rna_enum_symmetrize_direction_items[] = {
    {BMO_SYMMETRIZE_NEGATIVE_X, "NEGATIVE_X", 0, "-X to +X", ""},
    {BMO_SYMMETRIZE_POSITIVE_X, "POSITIVE_X", 0, "+X to -X", ""},

    {BMO_SYMMETRIZE_NEGATIVE_Y, "NEGATIVE_Y", 0, "-Y to +Y", ""},
    {BMO_SYMMETRIZE_POSITIVE_Y, "POSITIVE_Y", 0, "+Y to -Y", ""},

    {BMO_SYMMETRIZE_NEGATIVE_Z, "NEGATIVE_Z", 0, "-Z to +Z", ""},
    {BMO_SYMMETRIZE_POSITIVE_Z, "POSITIVE_Z", 0, "+Z to -Z", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

#ifdef RNA_RUNTIME
#  include "MEM_guardedalloc.h"

#  include "BKE_brush.hh"
#  include "BKE_collection.hh"
#  include "BKE_colortools.hh"
#  include "BKE_context.hh"
#  include "BKE_gpencil_legacy.h"
#  include "BKE_layer.hh"
#  include "BKE_material.hh"
#  include "BKE_object.hh"
#  include "BKE_paint.hh"
#  include "BKE_paint_types.hh"
#  include "BKE_particle.h"
#  include "BKE_pointcache.h"

#  include "DEG_depsgraph.hh"

#  include "ED_gpencil_legacy.hh"
#  include "ED_image.hh"
#  include "ED_paint.hh"
#  include "ED_particle.hh"

const EnumPropertyItem rna_enum_particle_edit_disconnected_hair_brush_items[] = {
    {PE_BRUSH_COMB, "COMB", 0, "Comb", "Comb hairs"},
    {PE_BRUSH_SMOOTH, "SMOOTH", 0, "Smooth", "Smooth hairs"},
    {PE_BRUSH_LENGTH, "LENGTH", 0, "Length", "Make hairs longer or shorter"},
    {PE_BRUSH_CUT, "CUT", 0, "Cut", "Cut hairs"},
    {PE_BRUSH_WEIGHT, "WEIGHT", 0, "Weight", "Weight hair particles"},
    {0, nullptr, 0, nullptr, nullptr},
};

static const EnumPropertyItem particle_edit_cache_brush_items[] = {
    {PE_BRUSH_COMB, "COMB", 0, "Comb", "Comb paths"},
    {PE_BRUSH_SMOOTH, "SMOOTH", 0, "Smooth", "Smooth paths"},
    {PE_BRUSH_LENGTH, "LENGTH", 0, "Length", "Make paths longer or shorter"},
    {0, nullptr, 0, nullptr, nullptr},
};

static PointerRNA rna_ParticleEdit_brush_get(PointerRNA *ptr)
{
  ParticleEditSettings *pset = (ParticleEditSettings *)ptr->data;
  ParticleBrushData *brush = nullptr;

  brush = &pset->brush[pset->brushtype];

  return RNA_pointer_create_with_parent(*ptr, &RNA_ParticleBrush, brush);
}

static PointerRNA rna_ParticleBrush_curve_get(PointerRNA * /*ptr*/)
{
  return PointerRNA_NULL;
}

static void rna_ParticleEdit_redo(bContext *C, PointerRNA * /*ptr*/)
{
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  BKE_view_layer_synced_ensure(scene, view_layer);
  Object *ob = BKE_view_layer_active_object_get(view_layer);
  PTCacheEdit *edit = PE_get_current(depsgraph, scene, ob);

  if (!edit) {
    return;
  }

  if (ob) {
    DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  }

  if (edit->psys) {
    BKE_particle_batch_cache_dirty_tag(edit->psys, BKE_PARTICLE_BATCH_DIRTY_ALL);
    psys_free_path_cache(edit->psys, edit);
  }
  DEG_id_tag_update(&scene->id, ID_RECALC_SYNC_TO_EVAL);
}

static void rna_ParticleEdit_update(bContext *C, PointerRNA * /*ptr*/)
{
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  BKE_view_layer_synced_ensure(scene, view_layer);
  Object *ob = BKE_view_layer_active_object_get(view_layer);

  if (ob) {
    DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  }

  /* Sync tool setting changes from original to evaluated scenes. */
  DEG_id_tag_update(&scene->id, ID_RECALC_SYNC_TO_EVAL);
}

static void rna_ParticleEdit_tool_set(PointerRNA *ptr, int value)
{
  ParticleEditSettings *pset = (ParticleEditSettings *)ptr->data;

  /* redraw hair completely if weight brush is/was used */
  if ((pset->brushtype == PE_BRUSH_WEIGHT || value == PE_BRUSH_WEIGHT) && pset->object) {
    Object *ob = pset->object;
    if (ob) {
      DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
      WM_main_add_notifier(NC_OBJECT | ND_PARTICLE | NA_EDITED, nullptr);
    }
  }

  pset->brushtype = value;
}
static const EnumPropertyItem *rna_ParticleEdit_tool_itemf(bContext *C,
                                                           PointerRNA * /*ptr*/,
                                                           PropertyRNA * /*prop*/,
                                                           bool * /*r_free*/)
{
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  BKE_view_layer_synced_ensure(scene, view_layer);
  Object *ob = BKE_view_layer_active_object_get(view_layer);
#  if 0
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  Scene *scene = CTX_data_scene(C);
  PTCacheEdit *edit = PE_get_current(depsgraph, scene, ob);
  ParticleSystem *psys = edit ? edit->psys : nullptr;
#  else
  /* use this rather than PE_get_current() - because the editing cache is
   * dependent on the cache being updated which can happen after this UI
   * draws causing a glitch #28883. */
  ParticleSystem *psys = psys_get_current(ob);
#  endif

  if (psys) {
    if (psys->flag & PSYS_GLOBAL_HAIR) {
      return rna_enum_particle_edit_disconnected_hair_brush_items;
    }
    else {
      return rna_enum_particle_edit_hair_brush_items;
    }
  }

  return particle_edit_cache_brush_items;
}

static bool rna_ParticleEdit_editable_get(PointerRNA *ptr)
{
  ParticleEditSettings *pset = (ParticleEditSettings *)ptr->data;

  return (pset->object && pset->scene && PE_get_current(nullptr, pset->scene, pset->object));
}
static bool rna_ParticleEdit_hair_get(PointerRNA *ptr)
{
  ParticleEditSettings *pset = (ParticleEditSettings *)ptr->data;

  if (pset->scene) {
    PTCacheEdit *edit = PE_get_current(nullptr, pset->scene, pset->object);

    return (edit && edit->psys);
  }

  return 0;
}

static std::optional<std::string> rna_ParticleEdit_path(const PointerRNA * /*ptr*/)
{
  return "tool_settings.particle_edit";
}

static PointerRNA rna_Paint_brush_get(PointerRNA *ptr)
{
  Paint *paint = static_cast<Paint *>(ptr->data);
  Brush *brush = BKE_paint_brush(paint);
  if (!brush) {
    return PointerRNA_NULL;
  }
  return RNA_id_pointer_create(&brush->id);
}

static bool rna_Paint_brush_poll(PointerRNA *ptr, PointerRNA value)
{
  const Paint *paint = static_cast<Paint *>(ptr->data);
  const Brush *brush = static_cast<Brush *>(value.data);

  return (brush == nullptr) || (paint->runtime->ob_mode & brush->ob_mode) != 0;
}

static PointerRNA rna_Paint_eraser_brush_get(PointerRNA *ptr)
{
  Paint *paint = static_cast<Paint *>(ptr->data);
  Brush *brush = BKE_paint_eraser_brush(paint);
  if (!brush) {
    return PointerRNA_NULL;
  }
  return RNA_id_pointer_create(&brush->id);
}

static void rna_Paint_eraser_brush_set(PointerRNA *ptr, PointerRNA value, ReportList * /*reports*/)
{
  Paint *paint = static_cast<Paint *>(ptr->data);
  Brush *brush = static_cast<Brush *>(value.data);
  BKE_paint_eraser_brush_set(paint, brush);
  BKE_paint_invalidate_overlay_all();
}

static bool rna_Paint_eraser_brush_poll(PointerRNA *ptr, PointerRNA value)
{
  const Paint *paint = static_cast<Paint *>(ptr->data);
  const Brush *brush = static_cast<Brush *>(value.data);

  return (brush == nullptr) || (paint->runtime->ob_mode & brush->ob_mode) != 0;
}

static void rna_Sculpt_update(bContext *C, PointerRNA * /*ptr*/)
{
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  BKE_view_layer_synced_ensure(scene, view_layer);
  Object *ob = BKE_view_layer_active_object_get(view_layer);

  if (ob) {
    DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
    WM_main_add_notifier(NC_OBJECT | ND_MODIFIER, ob);
  }
}

static std::optional<std::string> rna_Sculpt_path(const PointerRNA * /*ptr*/)
{
  return "tool_settings.sculpt";
}

static std::optional<std::string> rna_VertexPaint_path(const PointerRNA *ptr)
{
  const Scene *scene = (Scene *)ptr->owner_id;
  const ToolSettings *ts = scene->toolsettings;
  if (ptr->data == ts->vpaint) {
    return "tool_settings.vertex_paint";
  }
  return "tool_settings.weight_paint";
}

static std::optional<std::string> rna_ImagePaintSettings_path(const PointerRNA * /*ptr*/)
{
  return "tool_settings.image_paint";
}

static std::optional<std::string> rna_PaintModeSettings_path(const PointerRNA * /*ptr*/)
{
  return "tool_settings.paint_mode";
}

static std::optional<std::string> rna_UvSculpt_path(const PointerRNA * /*ptr*/)
{
  return "tool_settings.uv_sculpt";
}

static std::optional<std::string> rna_CurvesSculpt_path(const PointerRNA * /*ptr*/)
{
  return "tool_settings.curves_sculpt";
}

static std::optional<std::string> rna_GpPaint_path(const PointerRNA * /*ptr*/)
{
  return "tool_settings.gpencil_paint";
}

static std::optional<std::string> rna_GpVertexPaint_path(const PointerRNA * /*ptr*/)
{
  return "tool_settings.gpencil_vertex_paint";
}

static std::optional<std::string> rna_GpSculptPaint_path(const PointerRNA * /*ptr*/)
{
  return "tool_settings.gpencil_sculpt_paint";
}

static std::optional<std::string> rna_GpWeightPaint_path(const PointerRNA * /*ptr*/)
{
  return "tool_settings.gpencil_weight_paint";
}

static std::optional<std::string> rna_ParticleBrush_path(const PointerRNA * /*ptr*/)
{
  return "tool_settings.particle_edit.brush";
}

static void rna_ImaPaint_viewport_update(Main * /*bmain*/, Scene * /*scene*/, PointerRNA * /*ptr*/)
{
  /* not the best solution maybe, but will refresh the 3D viewport */
  WM_main_add_notifier(NC_OBJECT | ND_DRAW, nullptr);
}

static void rna_ImaPaint_mode_update(bContext *C, PointerRNA * /*ptr*/)
{
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  BKE_view_layer_synced_ensure(scene, view_layer);
  Object *ob = BKE_view_layer_active_object_get(view_layer);

  if (ob && ob->type == OB_MESH) {
    /* of course we need to invalidate here */
    BKE_texpaint_slots_refresh_object(scene, ob);

    /* We assume that changing the current mode will invalidate the uv layers
     * so we need to refresh display. */
    ED_paint_proj_mesh_data_check(*scene, *ob, nullptr, nullptr, nullptr, nullptr);
    WM_main_add_notifier(NC_OBJECT | ND_DRAW, nullptr);
  }
}

static void rna_ImaPaint_stencil_update(bContext *C, PointerRNA * /*ptr*/)
{
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  BKE_view_layer_synced_ensure(scene, view_layer);
  Object *ob = BKE_view_layer_active_object_get(view_layer);

  if (ob && ob->type == OB_MESH) {
    ED_paint_proj_mesh_data_check(*scene, *ob, nullptr, nullptr, nullptr, nullptr);
    WM_main_add_notifier(NC_OBJECT | ND_DRAW, nullptr);
  }
}

static bool rna_ImaPaint_imagetype_poll(PointerRNA * /*ptr*/, PointerRNA value)
{
  Image *image = (Image *)value.owner_id;
  return image->type != IMA_TYPE_R_RESULT && image->type != IMA_TYPE_COMPOSITE;
}

static void rna_ImaPaint_canvas_update(bContext *C, PointerRNA * /*ptr*/)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  BKE_view_layer_synced_ensure(scene, view_layer);
  Object *ob = BKE_view_layer_active_object_get(view_layer);
  Image *ima = scene->toolsettings->imapaint.canvas;

  ED_space_image_sync(bmain, ima, false);

  if (ob && ob->type == OB_MESH) {
    ED_paint_proj_mesh_data_check(*scene, *ob, nullptr, nullptr, nullptr, nullptr);
    WM_main_add_notifier(NC_OBJECT | ND_DRAW, nullptr);
  }
}

static void rna_UvSculpt_curve_preset_set(PointerRNA *ptr, int value)
{
  Scene *scene = reinterpret_cast<Scene *>(ptr->owner_id);
  if (value == BRUSH_CURVE_CUSTOM) {
    if (!scene->toolsettings->uvsculpt.curve_distance_falloff) {
      scene->toolsettings->uvsculpt.curve_distance_falloff = BKE_curvemapping_add(
          1, 0.0f, 0.0f, 1.0f, 1.0f);
    }
  }
  scene->toolsettings->uvsculpt.curve_distance_falloff_preset = int8_t(value);
}

/** \name Paint mode settings
 * \{ */

static bool rna_PaintModeSettings_canvas_image_poll(PointerRNA * /*ptr*/, PointerRNA value)
{
  Image *image = (Image *)value.owner_id;
  return !ELEM(image->type, IMA_TYPE_COMPOSITE, IMA_TYPE_R_RESULT);
}

static void rna_PaintModeSettings_canvas_source_update(bContext *C, PointerRNA * /*ptr*/)
{
  Scene *scene = CTX_data_scene(C);
  Object *ob = CTX_data_active_object(C);
  /* When canvas source changes the #pbvh::Tree would require updates when switching between color
   * attributes. */
  if (ob && ob->type == OB_MESH) {
    BKE_texpaint_slots_refresh_object(scene, ob);
    DEG_id_tag_update(&ob->id, 0);
    WM_main_add_notifier(NC_GEOM | ND_DATA, &ob->id);
  }
}

/** \} */

static bool rna_ImaPaint_detect_data(ImagePaintSettings *imapaint)
{
  return imapaint->missing_data == 0;
}

static std::optional<std::string> rna_GPencilSculptSettings_path(const PointerRNA * /*ptr*/)
{
  return "tool_settings.gpencil_sculpt";
}

static std::optional<std::string> rna_GPencilSculptGuide_path(const PointerRNA * /*ptr*/)
{
  return "tool_settings.gpencil_sculpt.guide";
}

static void rna_Sculpt_automasking_invert_cavity_set(PointerRNA *ptr, bool val)
{
  Sculpt *sd = (Sculpt *)ptr->data;

  if (val) {
    sd->automasking_flags &= ~BRUSH_AUTOMASKING_CAVITY_NORMAL;
    sd->automasking_flags |= BRUSH_AUTOMASKING_CAVITY_INVERTED;
  }
  else {
    sd->automasking_flags &= ~BRUSH_AUTOMASKING_CAVITY_INVERTED;
  }
}

static void rna_Sculpt_automasking_cavity_set(PointerRNA *ptr, bool val)
{
  Sculpt *sd = (Sculpt *)ptr->data;

  if (val) {
    sd->automasking_flags &= ~BRUSH_AUTOMASKING_CAVITY_INVERTED;
    sd->automasking_flags |= BRUSH_AUTOMASKING_CAVITY_NORMAL;
  }
  else {
    sd->automasking_flags &= ~BRUSH_AUTOMASKING_CAVITY_NORMAL;
  }
}

static void rna_UnifiedPaintSettings_update(bContext *C, PointerRNA * /*ptr*/)
{
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Brush *br = BKE_paint_brush(BKE_paint_get_active(scene, view_layer));
  /* TODO: Verify if tagging the brush for these settings being changed is correct. */
  WM_main_add_notifier(NC_BRUSH | NA_EDITED, br);
  WM_main_add_notifier(NC_SCENE | ND_TOOLSETTINGS, scene);
}

static void rna_UnifiedPaintSettings_color_update(bContext *C, PointerRNA *ptr)
{
  UnifiedPaintSettings *ups = static_cast<UnifiedPaintSettings *>(ptr->data);
  rna_UnifiedPaintSettings_update(C, ptr);
  BKE_brush_color_sync_legacy(ups);
}

static void rna_UnifiedPaintSettings_size_set(PointerRNA *ptr, int value)
{
  UnifiedPaintSettings *ups = static_cast<UnifiedPaintSettings *>(ptr->data);

  /* scale unprojected size so it stays consistent with brush size */
  BKE_brush_scale_unprojected_size(&ups->unprojected_size, value, ups->size);
  ups->size = value;
}

static void rna_UnifiedPaintSettings_unprojected_size_set(PointerRNA *ptr, float value)
{
  UnifiedPaintSettings *ups = static_cast<UnifiedPaintSettings *>(ptr->data);

  /* scale brush size so it stays consistent with unprojected_size */
  BKE_brush_scale_size(&ups->size, value, ups->unprojected_size);
  ups->unprojected_size = value;
}

static void rna_UnifiedPaintSettings_size_update(bContext *C, PointerRNA *ptr)
{
  /* changing the unified size should invalidate the overlay but also update the brush */
  BKE_paint_invalidate_overlay_all();
  rna_UnifiedPaintSettings_update(C, ptr);
}

static const UnifiedPaintSettings *rna_UnifiedPaintSettings_address_get(const Paint *paint)
{
  if (!paint) {
    return nullptr;
  }

  return &paint->unified_paint_settings;
}

static std::optional<std::string> rna_UnifiedPaintSettings_path(const PointerRNA *ptr)
{
  const Scene *scene = reinterpret_cast<Scene *>(ptr->owner_id);
  const ToolSettings *tool_settings = scene ? scene->toolsettings : nullptr;
  if (tool_settings == nullptr) {
    return std::nullopt;
  }
  if (rna_UnifiedPaintSettings_address_get(reinterpret_cast<Paint *>(tool_settings->vpaint)) ==
      ptr->data)
  {
    return "tool_settings.vertex_paint.unified_paint_settings";
  }
  if (rna_UnifiedPaintSettings_address_get(reinterpret_cast<Paint *>(tool_settings->wpaint)) ==
      ptr->data)
  {
    return "tool_settings.weight_paint.unified_paint_settings";
  }
  if (rna_UnifiedPaintSettings_address_get(reinterpret_cast<Paint *>(tool_settings->sculpt)) ==
      ptr->data)
  {
    return "tool_settings.sculpt.unified_paint_settings";
  }
  if (rna_UnifiedPaintSettings_address_get(reinterpret_cast<Paint *>(tool_settings->gp_paint)) ==
      ptr->data)
  {
    return "tool_settings.gpencil_paint.unified_paint_settings";
  }
  if (rna_UnifiedPaintSettings_address_get(
          reinterpret_cast<Paint *>(tool_settings->gp_vertexpaint)) == ptr->data)
  {
    return "tool_settings.gpencil_vertex_paint.unified_paint_settings";
  }
  if (rna_UnifiedPaintSettings_address_get(
          reinterpret_cast<Paint *>(tool_settings->gp_sculptpaint)) == ptr->data)
  {
    return "tool_settings.gpencil_sculpt_paint.unified_paint_settings";
  }
  if (rna_UnifiedPaintSettings_address_get(
          reinterpret_cast<Paint *>(tool_settings->gp_weightpaint)) == ptr->data)
  {
    return "tool_settings.gpencil_weight_paint.unified_paint_settings";
  }
  if (rna_UnifiedPaintSettings_address_get(
          reinterpret_cast<Paint *>(tool_settings->curves_sculpt)) == ptr->data)
  {
    return "tool_settings.curves_sculpt.unified_paint_settings";
  }
  return std::nullopt;
}
#else

static void rna_def_paint_curve(BlenderRNA *brna)
{
  StructRNA *srna;

  srna = RNA_def_struct(brna, "PaintCurve", "ID");
  RNA_def_struct_ui_text(srna, "Paint Curve", "");
  RNA_def_struct_ui_icon(srna, ICON_CURVE_BEZCURVE);
}

static void rna_def_paint_curve_visibility_flag(StructRNA *srna,
                                                const char *prop_name,
                                                const char *ui_name,
                                                const int64_t flag)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, prop_name, PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "curve_visibility_flags", flag);
  RNA_def_property_ui_text(prop, ui_name, nullptr);
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr);
}

static void rna_def_paint(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "Paint", nullptr);
  RNA_def_struct_ui_text(srna, "Paint", "");

  /* Global Settings */
  prop = RNA_def_property(srna, "brush", PROP_POINTER, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_struct_type(prop, "Brush");
  RNA_def_property_pointer_funcs(
      prop, "rna_Paint_brush_get", nullptr, nullptr, "rna_Paint_brush_poll");
  RNA_def_property_ui_text(prop, "Brush", "Active brush");
  RNA_def_property_update(prop, NC_BRUSH | NA_SELECTED, nullptr);

  prop = RNA_def_property(srna, "brush_asset_reference", PROP_POINTER, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop,
                           "Brush Asset Reference",
                           "A weak reference to the matching brush asset, used e.g. to restore "
                           "the last used brush on file load");

  prop = RNA_def_property(srna, "eraser_brush", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_NEVER_UNLINK);
  RNA_def_property_clear_flag(prop, PROP_ID_REFCOUNT);
  RNA_def_property_struct_type(prop, "Brush");
  RNA_def_property_pointer_funcs(prop,
                                 "rna_Paint_eraser_brush_get",
                                 "rna_Paint_eraser_brush_set",
                                 nullptr,
                                 "rna_Paint_eraser_brush_poll");
  RNA_def_property_ui_text(prop,
                           "Default Eraser Brush",
                           "Default eraser brush for quickly alternating with the main brush");
  RNA_def_property_update(prop, NC_BRUSH | NA_SELECTED, nullptr);

  prop = RNA_def_property(srna, "eraser_brush_asset_reference", PROP_POINTER, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop,
                           "Eraser Brush Asset Reference",
                           "A weak reference to the matching brush asset, used e.g. to restore "
                           "the last used brush on file load");

  prop = RNA_def_property(srna, "palette", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_pointer_funcs(prop, nullptr, nullptr, nullptr, nullptr);
  RNA_def_property_ui_text(prop, "Palette", "Active Palette");

  prop = RNA_def_property(srna, "show_brush", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flags", PAINT_SHOW_BRUSH);
  RNA_def_property_ui_text(prop, "Show Brush", "");
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr);

  prop = RNA_def_property(srna, "show_brush_on_surface", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flags", PAINT_SHOW_BRUSH_ON_SURFACE);
  RNA_def_property_ui_text(prop, "Show Brush On Surface", "");
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr);

  prop = RNA_def_property(srna, "show_low_resolution", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flags", PAINT_FAST_NAVIGATE);
  RNA_def_property_ui_text(
      prop, "Fast Navigate", "For multires, show low resolution while navigating the view");
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr);

  prop = RNA_def_property(srna, "use_sculpt_delay_updates", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flags", PAINT_SCULPT_DELAY_UPDATES);
  RNA_def_property_ui_text(
      prop,
      "Delay Viewport Updates",
      "Update the geometry when it enters the view, providing faster view navigation");
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr);

  prop = RNA_def_property(srna, "use_symmetry_x", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "symmetry_flags", PAINT_SYMM_X);
  RNA_def_property_ui_text(prop, "Symmetry X", "Mirror brush across the X axis");
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr);

  prop = RNA_def_property(srna, "use_symmetry_y", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "symmetry_flags", PAINT_SYMM_Y);
  RNA_def_property_ui_text(prop, "Symmetry Y", "Mirror brush across the Y axis");
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr);

  prop = RNA_def_property(srna, "use_symmetry_z", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "symmetry_flags", PAINT_SYMM_Z);
  RNA_def_property_ui_text(prop, "Symmetry Z", "Mirror brush across the Z axis");
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr);

  prop = RNA_def_property(srna, "use_symmetry_feather", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "symmetry_flags", PAINT_SYMMETRY_FEATHER);
  RNA_def_property_ui_text(prop,
                           "Symmetry Feathering",
                           "Reduce the strength of the brush where it overlaps symmetrical daubs");
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr);

  prop = RNA_def_property(srna, "cavity_curve", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_ui_text(prop, "Curve", "Editable cavity curve");
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr);

  prop = RNA_def_property(srna, "use_cavity", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flags", PAINT_USE_CAVITY_MASK);
  RNA_def_property_ui_text(prop, "Cavity Mask", "Mask painting according to mesh geometry cavity");
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr);

  prop = RNA_def_property(srna, "tile_offset", PROP_FLOAT, PROP_XYZ_LENGTH);
  RNA_def_property_float_sdna(prop, nullptr, "tile_offset");
  RNA_def_property_array(prop, 3);
  RNA_def_property_range(prop, 0.01, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.01, 100, 1 * 100, 2);
  RNA_def_property_ui_text(
      prop, "Tiling offset for the X Axis", "Stride at which tiled strokes are copied");

  prop = RNA_def_property(srna, "tile_x", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "symmetry_flags", PAINT_TILE_X);
  RNA_def_property_ui_text(prop, "Tile X", "Tile along X axis");
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr);

  prop = RNA_def_property(srna, "tile_y", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "symmetry_flags", PAINT_TILE_Y);
  RNA_def_property_ui_text(prop, "Tile Y", "Tile along Y axis");
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr);

  prop = RNA_def_property(srna, "tile_z", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "symmetry_flags", PAINT_TILE_Z);
  RNA_def_property_ui_text(prop, "Tile Z", "Tile along Z axis");
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr);

  rna_def_paint_curve_visibility_flag(
      srna, "show_strength_curve", "Show Strength Curve", PAINT_CURVE_SHOW_STRENGTH);
  rna_def_paint_curve_visibility_flag(
      srna, "show_size_curve", "Show Size Curve", PAINT_CURVE_SHOW_SIZE);
  rna_def_paint_curve_visibility_flag(
      srna, "show_jitter_curve", "Show Jitter Curve", PAINT_CURVE_SHOW_JITTER);

  /* Unified Paint Settings */
  prop = RNA_def_property(srna, "unified_paint_settings", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_struct_type(prop, "UnifiedPaintSettings");
  RNA_def_property_ui_text(prop, "Unified Paint Settings", nullptr);
}

static void rna_def_unified_paint_settings(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static const EnumPropertyItem brush_size_unit_items[] = {
      {0, "VIEW", 0, "View", "Measure brush size relative to the view"},
      {UNIFIED_PAINT_BRUSH_LOCK_SIZE,
       "SCENE",
       0,
       "Scene",
       "Measure brush size relative to the scene"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  srna = RNA_def_struct(brna, "UnifiedPaintSettings", nullptr);
  RNA_def_struct_path_func(srna, "rna_UnifiedPaintSettings_path");
  RNA_def_struct_ui_text(
      srna, "Unified Paint Settings", "Overrides for some of the active brush's settings");
  RNA_def_struct_clear_flag(srna, STRUCT_UNDO);

  /* high-level flags to enable or disable unified paint settings */
  prop = RNA_def_property(srna, "use_unified_size", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", UNIFIED_PAINT_SIZE);
  RNA_def_property_ui_text(
      prop, "Use Unified Size", "Instead of per-brush size, the size is shared across brushes");
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr);

  prop = RNA_def_property(srna, "use_unified_strength", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", UNIFIED_PAINT_ALPHA);
  RNA_def_property_ui_text(prop,
                           "Use Unified Strength",
                           "Instead of per-brush strength, the strength is shared across brushes");
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr);

  prop = RNA_def_property(srna, "use_unified_weight", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", UNIFIED_PAINT_WEIGHT);
  RNA_def_property_ui_text(prop,
                           "Use Unified Weight",
                           "Instead of per-brush weight, the weight is shared across brushes");
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr);

  prop = RNA_def_property(srna, "use_unified_color", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", UNIFIED_PAINT_COLOR);
  RNA_def_property_ui_text(
      prop, "Use Unified Color", "Instead of per-brush color, the color is shared across brushes");
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr);

  prop = RNA_def_property(srna, "use_unified_input_samples", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", UNIFIED_PAINT_INPUT_SAMPLES);
  RNA_def_property_ui_text(
      prop,
      "Use Unified Input Samples",
      "Instead of per-brush input samples, the value is shared across brushes");
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr);

  /* unified paint settings that override the equivalent settings
   * from the active brush */
  prop = RNA_def_property(srna, "size", PROP_INT, PROP_PIXEL_DIAMETER);
  RNA_def_property_int_funcs(prop, nullptr, "rna_UnifiedPaintSettings_size_set", nullptr);
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
  RNA_def_property_range(prop, 1, MAX_BRUSH_PIXEL_DIAMETER * 10);
  RNA_def_property_ui_range(prop, 1, MAX_BRUSH_PIXEL_DIAMETER, 1, -1);
  RNA_def_property_ui_text(prop, "Size", "Diameter of the brush");
  RNA_def_property_update(prop, 0, "rna_UnifiedPaintSettings_size_update");

  prop = RNA_def_property(srna, "unprojected_size", PROP_FLOAT, PROP_DISTANCE_DIAMETER);
  RNA_def_property_float_funcs(
      prop, nullptr, "rna_UnifiedPaintSettings_unprojected_size_set", nullptr);
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
  RNA_def_property_range(prop, 0.001, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.001, 1, 1, -1);
  RNA_def_property_ui_text(prop, "Unprojected Size", "Diameter of brush in Blender units");
  RNA_def_property_update(prop, 0, "rna_UnifiedPaintSettings_size_update");

  prop = RNA_def_property(srna, "strength", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "alpha");
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
  RNA_def_property_range(prop, 0.0f, 10.0f);
  RNA_def_property_ui_range(prop, 0.0f, 1.0f, 0.001, 3);
  RNA_def_property_ui_text(
      prop, "Strength", "How powerful the effect of the brush is when applied");
  RNA_def_property_update(prop, 0, "rna_UnifiedPaintSettings_update");

  prop = RNA_def_property(srna, "weight", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "weight");
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_range(prop, 0.0f, 1.0f, 0.001, 3);
  RNA_def_property_ui_text(prop, "Weight", "Weight to assign in vertex groups");
  RNA_def_property_update(prop, 0, "rna_UnifiedPaintSettings_update");

  prop = RNA_def_property(srna, "color", PROP_FLOAT, PROP_COLOR);
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
  RNA_def_property_range(prop, 0.0, 1.0);
  RNA_def_property_float_sdna(prop, nullptr, "color");
  RNA_def_property_ui_text(prop, "Color", "");
  RNA_def_property_update(prop, 0, "rna_UnifiedPaintSettings_color_update");

  prop = RNA_def_property(srna, "secondary_color", PROP_FLOAT, PROP_COLOR);
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
  RNA_def_property_range(prop, 0.0, 1.0);
  RNA_def_property_float_sdna(prop, nullptr, "secondary_color");
  RNA_def_property_ui_text(prop, "Secondary Color", "");
  RNA_def_property_update(prop, 0, "rna_UnifiedPaintSettings_color_update");

  prop = RNA_def_property(srna, "use_color_jitter", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", UNIFIED_PAINT_COLOR_JITTER);
  RNA_def_property_ui_text(prop, "Use Color Jitter", "Jitter brush color");
  RNA_def_property_update(prop, 0, "rna_UnifiedPaintSettings_update");

  prop = RNA_def_property(srna, "hue_jitter", PROP_FLOAT, PROP_NONE);
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
  RNA_def_property_float_sdna(prop, nullptr, "hsv_jitter[0]");
  RNA_def_property_range(prop, 0, 1.0f);
  RNA_def_property_ui_range(prop, 0, 1, 0.05, 2);
  RNA_def_property_ui_text(prop, "Hue Jitter", "Color jitter effect on hue");
  RNA_def_property_update(prop, 0, "rna_UnifiedPaintSettings_update");

  prop = RNA_def_property(srna, "saturation_jitter", PROP_FLOAT, PROP_NONE);
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
  RNA_def_property_float_sdna(prop, nullptr, "hsv_jitter[1]");
  RNA_def_property_range(prop, 0, 1.0f);
  RNA_def_property_ui_range(prop, 0, 1, 0.05, 2);
  RNA_def_property_ui_text(prop, "Saturation Jitter", "Color jitter effect on saturation");
  RNA_def_property_update(prop, 0, "rna_UnifiedPaintSettings_update");

  prop = RNA_def_property(srna, "value_jitter", PROP_FLOAT, PROP_NONE);
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
  RNA_def_property_float_sdna(prop, nullptr, "hsv_jitter[2]");
  RNA_def_property_range(prop, 0, 1.0f);
  RNA_def_property_ui_range(prop, 0, 1, 0.05, 2);
  RNA_def_property_ui_text(prop, "Value Jitter", "Color jitter effect on value");
  RNA_def_property_update(prop, 0, "rna_UnifiedPaintSettings_update");

  prop = RNA_def_property(srna, "use_stroke_random_hue", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
  RNA_def_property_boolean_sdna(
      prop, nullptr, "color_jitter_flag", BRUSH_COLOR_JITTER_USE_HUE_AT_STROKE);
  RNA_def_property_ui_icon(prop, ICON_GP_SELECT_STROKES, 0);
  RNA_def_property_ui_text(prop, "Stroke Random", "Use randomness at stroke level");
  RNA_def_property_update(prop, 0, "rna_UnifiedPaintSettings_update");

  prop = RNA_def_property(srna, "use_stroke_random_sat", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
  RNA_def_property_boolean_sdna(
      prop, nullptr, "color_jitter_flag", BRUSH_COLOR_JITTER_USE_SAT_AT_STROKE);
  RNA_def_property_ui_icon(prop, ICON_GP_SELECT_STROKES, 0);
  RNA_def_property_ui_text(prop, "Stroke Random", "Use randomness at stroke level");
  RNA_def_property_update(prop, 0, "rna_UnifiedPaintSettings_update");

  prop = RNA_def_property(srna, "use_stroke_random_val", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
  RNA_def_property_boolean_sdna(
      prop, nullptr, "color_jitter_flag", BRUSH_COLOR_JITTER_USE_VAL_AT_STROKE);
  RNA_def_property_ui_icon(prop, ICON_GP_SELECT_STROKES, 0);
  RNA_def_property_ui_text(prop, "Stroke Random", "Use randomness at stroke level");
  RNA_def_property_update(prop, 0, "rna_UnifiedPaintSettings_update");

  prop = RNA_def_property(srna, "use_random_press_hue", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
  RNA_def_property_boolean_sdna(
      prop, nullptr, "color_jitter_flag", BRUSH_COLOR_JITTER_USE_HUE_RAND_PRESS);
  RNA_def_property_ui_icon(prop, ICON_STYLUS_PRESSURE, 0);
  RNA_def_property_ui_text(prop, "Use Pressure", "Use pressure to modulate randomness");
  RNA_def_property_update(prop, 0, "rna_UnifiedPaintSettings_update");

  prop = RNA_def_property(srna, "use_random_press_sat", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
  RNA_def_property_boolean_sdna(
      prop, nullptr, "color_jitter_flag", BRUSH_COLOR_JITTER_USE_SAT_RAND_PRESS);
  RNA_def_property_ui_icon(prop, ICON_STYLUS_PRESSURE, 0);
  RNA_def_property_ui_text(prop, "Use Pressure", "Use pressure to modulate randomness");
  RNA_def_property_update(prop, 0, "rna_UnifiedPaintSettings_update");

  prop = RNA_def_property(srna, "use_random_press_val", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
  RNA_def_property_boolean_sdna(
      prop, nullptr, "color_jitter_flag", BRUSH_COLOR_JITTER_USE_VAL_RAND_PRESS);
  RNA_def_property_ui_icon(prop, ICON_STYLUS_PRESSURE, 0);
  RNA_def_property_ui_text(prop, "Use Pressure", "Use pressure to modulate randomness");
  RNA_def_property_update(prop, 0, "rna_UnifiedPaintSettings_update");

  prop = RNA_def_property(srna, "input_samples", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, nullptr, "input_samples");
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
  RNA_def_property_range(prop, 1, PAINT_MAX_INPUT_SAMPLES);
  RNA_def_property_ui_range(prop, 1, PAINT_MAX_INPUT_SAMPLES, 1, -1);
  RNA_def_property_ui_text(
      prop,
      "Input Samples",
      "Number of input samples to average together to smooth the brush stroke");
  RNA_def_property_update(prop, 0, "rna_UnifiedPaintSettings_update");

  prop = RNA_def_property(srna, "use_locked_size", PROP_ENUM, PROP_NONE); /* as an enum */
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
  RNA_def_property_enum_bitflag_sdna(prop, nullptr, "flag");
  RNA_def_property_enum_items(prop, brush_size_unit_items);
  RNA_def_property_ui_text(
      prop, "Size Unit", "Measure brush size relative to the view or the scene");
  RNA_def_property_update(prop, 0, "rna_UnifiedPaintSettings_update");
}

static void rna_def_sculpt(BlenderRNA *brna)
{
  static const EnumPropertyItem detail_refine_items[] = {
      {SCULPT_DYNTOPO_SUBDIVIDE,
       "SUBDIVIDE",
       0,
       "Subdivide Edges",
       "Subdivide long edges to add mesh detail where needed"},
      {SCULPT_DYNTOPO_COLLAPSE,
       "COLLAPSE",
       0,
       "Collapse Edges",
       "Collapse short edges to remove mesh detail where possible"},
      {SCULPT_DYNTOPO_SUBDIVIDE | SCULPT_DYNTOPO_COLLAPSE,
       "SUBDIVIDE_COLLAPSE",
       0,
       "Subdivide Collapse",
       "Both subdivide long edges and collapse short edges to refine mesh detail"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem detail_type_items[] = {
      {0,
       "RELATIVE",
       0,
       "Relative Detail",
       "Mesh detail is relative to the brush size and detail size"},
      {SCULPT_DYNTOPO_DETAIL_CONSTANT,
       "CONSTANT",
       0,
       "Constant Detail",
       "Mesh detail is constant in world space according to detail size"},
      {SCULPT_DYNTOPO_DETAIL_BRUSH,
       "BRUSH",
       0,
       "Brush Detail",
       "Mesh detail is relative to brush size"},
      {SCULPT_DYNTOPO_DETAIL_MANUAL,
       "MANUAL",
       0,
       "Manual Detail",
       "Mesh detail does not change on each stroke, only when using Flood Fill"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem sculpt_transform_mode_items[] = {
      {SCULPT_TRANSFORM_MODE_ALL_VERTICES,
       "ALL_VERTICES",
       0,
       "All Vertices",
       "Applies the transformation to all vertices in the mesh"},
      {SCULPT_TRANSFORM_MODE_RADIUS_ELASTIC,
       "RADIUS_ELASTIC",
       0,
       "Elastic",
       "Applies the transformation simulating elasticity using the radius of the cursor"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "Sculpt", "Paint");
  RNA_def_struct_path_func(srna, "rna_Sculpt_path");
  RNA_def_struct_ui_text(srna, "Sculpt", "");
  RNA_def_struct_clear_flag(srna, STRUCT_UNDO);

  prop = RNA_def_property(srna, "lock_x", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flags", SCULPT_LOCK_X);
  RNA_def_property_ui_text(prop, "Lock X", "Disallow changes to the X axis of vertices");
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr);

  prop = RNA_def_property(srna, "lock_y", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flags", SCULPT_LOCK_Y);
  RNA_def_property_ui_text(prop, "Lock Y", "Disallow changes to the Y axis of vertices");
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr);

  prop = RNA_def_property(srna, "lock_z", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flags", SCULPT_LOCK_Z);
  RNA_def_property_ui_text(prop, "Lock Z", "Disallow changes to the Z axis of vertices");
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr);

  prop = RNA_def_property(srna, "use_deform_only", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flags", SCULPT_ONLY_DEFORM);
  RNA_def_property_ui_text(prop,
                           "Use Deform Only",
                           "Use only deformation modifiers (temporary disable all "
                           "constructive modifiers except multi-resolution)");
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, "rna_Sculpt_update");

  prop = RNA_def_property(srna, "detail_size", PROP_FLOAT, PROP_PIXEL);
  RNA_def_property_range(prop, 0.5, 40.0);
  RNA_def_property_ui_range(prop, 0.5, 40.0, 0.1, 2);
  RNA_def_property_ui_scale_type(prop, PROP_SCALE_CUBIC);
  RNA_def_property_ui_text(
      prop, "Detail Size", "Maximum edge length for dynamic topology sculpting (in pixels)");
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr);

  prop = RNA_def_property(srna, "detail_percent", PROP_FLOAT, PROP_PERCENTAGE);
  RNA_def_property_range(prop, 0.5, 100.0);
  RNA_def_property_ui_range(prop, 0.5, 100.0, 10, 2);
  RNA_def_property_ui_text(
      prop,
      "Detail Percentage",
      "Maximum edge length for dynamic topology sculpting (in brush percentage)");
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr);

  prop = RNA_def_property(srna, "constant_detail_resolution", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "constant_detail");
  RNA_def_property_range(prop, 0.0001, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.001, 1000.0, 10, 2);
  RNA_def_property_ui_text(prop,
                           "Resolution",
                           "Maximum edge length for dynamic topology sculpting (as divisor "
                           "of Blender unit - higher value means smaller edge length)");
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr);

  const EnumPropertyItem *entry = rna_enum_brush_automasking_flag_items;
  do {
    prop = RNA_def_property(srna, entry->identifier, PROP_BOOLEAN, PROP_NONE);
    RNA_def_property_boolean_sdna(prop, nullptr, "automasking_flags", entry->value);
    RNA_def_property_ui_text(prop, entry->name, entry->description);

    if (entry->value == BRUSH_AUTOMASKING_CAVITY_NORMAL) {
      RNA_def_property_boolean_funcs(prop, nullptr, "rna_Sculpt_automasking_cavity_set");
    }
    else if (entry->value == BRUSH_AUTOMASKING_CAVITY_INVERTED) {
      RNA_def_property_boolean_funcs(prop, nullptr, "rna_Sculpt_automasking_invert_cavity_set");
    }

    RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr);
  } while ((++entry)->identifier);

  prop = RNA_def_property(
      srna, "automasking_boundary_edges_propagation_steps", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, nullptr, "automasking_boundary_edges_propagation_steps");
  RNA_def_property_range(prop, 1, AUTOMASKING_BOUNDARY_EDGES_MAX_PROPAGATION_STEPS);
  RNA_def_property_ui_range(prop, 1, AUTOMASKING_BOUNDARY_EDGES_MAX_PROPAGATION_STEPS, 1, -1);
  RNA_def_property_ui_text(prop,
                           "Propagation Steps",
                           "Distance where boundary edge automasking is going to protect vertices "
                           "from the fully masked edge");
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr);

  prop = RNA_def_property(srna, "automasking_cavity_factor", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "automasking_cavity_factor");
  RNA_def_property_ui_text(prop, "Cavity Factor", "The contrast of the cavity mask");
  RNA_def_property_float_default(prop, 1.0f);
  RNA_def_property_range(prop, 0.0f, 5.0f);
  RNA_def_property_ui_range(prop, 0.0f, 1.0f, 0.1, 3);
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr);

  prop = RNA_def_property(srna, "automasking_cavity_blur_steps", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "automasking_cavity_blur_steps");
  RNA_def_property_ui_text(prop, "Blur Steps", "The number of times the cavity mask is blurred");
  RNA_def_property_int_default(prop, 0);
  RNA_def_property_range(prop, 0, 25);
  RNA_def_property_ui_range(prop, 0, 10, 1, 1);
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr);

  prop = RNA_def_property(srna, "automasking_cavity_curve", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "automasking_cavity_curve");
  RNA_def_property_struct_type(prop, "CurveMapping");
  RNA_def_property_ui_text(prop, "Cavity Curve", "Curve used for the sensitivity");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr);

  prop = RNA_def_property(srna, "automasking_cavity_curve_op", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "automasking_cavity_curve_op");
  RNA_def_property_struct_type(prop, "CurveMapping");
  RNA_def_property_ui_text(prop, "Cavity Curve", "Curve used for the sensitivity");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr);

  prop = RNA_def_property(srna, "use_automasking_start_normal", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(
      prop, nullptr, "automasking_flags", BRUSH_AUTOMASKING_BRUSH_NORMAL);
  RNA_def_property_ui_text(
      prop,
      "Area Normal",
      "Affect only vertices with a similar normal to where the stroke starts");
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr);

  prop = RNA_def_property(srna, "use_automasking_view_normal", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "automasking_flags", BRUSH_AUTOMASKING_VIEW_NORMAL);
  RNA_def_property_ui_text(
      prop, "View Normal", "Affect only vertices with a normal that faces the viewer");
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr);

  prop = RNA_def_property(srna, "use_automasking_view_occlusion", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(
      prop, nullptr, "automasking_flags", BRUSH_AUTOMASKING_VIEW_OCCLUSION);
  RNA_def_property_ui_text(
      prop,
      "Occlusion",
      "Only affect vertices that are not occluded by other faces (slower performance)");
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr);

  prop = RNA_def_property(srna, "automasking_start_normal_limit", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_float_sdna(prop, nullptr, "automasking_start_normal_limit");
  RNA_def_property_range(prop, 0.0001f, M_PI);
  RNA_def_property_ui_text(prop, "Area Normal Limit", "The range of angles that will be affected");
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr);

  prop = RNA_def_property(srna, "automasking_start_normal_falloff", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "automasking_start_normal_falloff");
  RNA_def_property_range(prop, 0.0001f, 1.0f);
  RNA_def_property_ui_text(
      prop, "Area Normal Falloff", "Extend the angular range with a falloff gradient");
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr);

  prop = RNA_def_property(srna, "automasking_view_normal_limit", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_float_sdna(prop, nullptr, "automasking_view_normal_limit");
  RNA_def_property_range(prop, 0.0001f, M_PI);
  RNA_def_property_ui_text(prop, "View Normal Limit", "The range of angles that will be affected");
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr);

  prop = RNA_def_property(srna, "automasking_view_normal_falloff", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "automasking_view_normal_falloff");
  RNA_def_property_range(prop, 0.0001f, 1.0f);
  RNA_def_property_ui_text(
      prop, "View Normal Falloff", "Extend the angular range with a falloff gradient");
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr);

  prop = RNA_def_property(srna, "symmetrize_direction", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, rna_enum_symmetrize_direction_items);
  RNA_def_property_ui_text(prop, "Direction", "Source and destination for symmetrize operator");

  prop = RNA_def_property(srna, "detail_refine_method", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, nullptr, "flags");
  RNA_def_property_enum_items(prop, detail_refine_items);
  RNA_def_property_ui_text(
      prop, "Detail Refine Method", "In dynamic-topology mode, how to add or remove mesh detail");
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr);

  prop = RNA_def_property(srna, "detail_type_method", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, nullptr, "flags");
  RNA_def_property_enum_items(prop, detail_type_items);
  RNA_def_property_ui_text(
      prop, "Detail Type Method", "In dynamic-topology mode, how mesh detail size is calculated");
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr);

  prop = RNA_def_property(srna, "gravity", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "gravity_factor");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_range(prop, 0.0f, 1.0f, 0.1, 3);
  RNA_def_property_ui_text(prop, "Gravity", "Amount of gravity after each dab");
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr);

  prop = RNA_def_property(srna, "transform_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, sculpt_transform_mode_items);
  RNA_def_property_ui_text(
      prop, "Transform Mode", "How the transformation is going to be applied to the target");
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr);

  prop = RNA_def_property(srna, "gravity_object", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(
      prop, "Orientation", "Object whose Z axis defines orientation of gravity");
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr);
}

static void rna_def_uv_sculpt(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "UvSculpt", nullptr);
  RNA_def_struct_path_func(srna, "rna_UvSculpt_path");
  RNA_def_struct_ui_text(srna, "UV Sculpting", "");
  RNA_def_struct_clear_flag(srna, STRUCT_UNDO);

  prop = RNA_def_property(srna, "size", PROP_INT, PROP_PIXEL_DIAMETER);
  RNA_def_property_ui_range(prop, 1, MAX_BRUSH_PIXEL_DIAMETER, 1, 1);
  RNA_def_property_range(prop, 1, MAX_BRUSH_PIXEL_DIAMETER * 10);
  RNA_def_property_ui_text(prop, "Size", "");
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr);

  prop = RNA_def_property(srna, "strength", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Strength", "");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_AMOUNT);
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr);

  prop = RNA_def_property(srna, "curve_distance_falloff", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "CurveMapping");
  RNA_def_property_pointer_funcs(prop, nullptr, nullptr, nullptr, nullptr);
  RNA_def_property_ui_text(prop, "Falloff Curve", "");
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr);

  prop = RNA_def_property(srna, "curve_distance_falloff_preset", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, rna_enum_brush_curve_preset_items);
  RNA_def_property_ui_text(prop, "Falloff Curve Preset", "");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_CURVE_LEGACY);
  RNA_def_property_enum_funcs(prop, nullptr, "rna_UvSculpt_curve_preset_set", nullptr);
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr);
}

static void rna_def_gp_paint(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "GpPaint", "Paint");
  RNA_def_struct_path_func(srna, "rna_GpPaint_path");
  RNA_def_struct_ui_text(srna, "Grease Pencil Paint", "");
  RNA_def_struct_clear_flag(srna, STRUCT_UNDO);

  /* Use vertex color (main switch). */
  prop = RNA_def_property(srna, "color_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "mode");
  RNA_def_property_enum_items(prop, rna_enum_gpencil_paint_mode);
  RNA_def_property_ui_text(prop, "Mode", "Paint Mode");
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
}

static void rna_def_gp_vertexpaint(BlenderRNA *brna)
{
  StructRNA *srna;

  srna = RNA_def_struct(brna, "GpVertexPaint", "Paint");
  RNA_def_struct_path_func(srna, "rna_GpVertexPaint_path");
  RNA_def_struct_ui_text(srna, "Grease Pencil Vertex Paint", "");
  RNA_def_struct_clear_flag(srna, STRUCT_UNDO);
}

static void rna_def_gp_sculptpaint(BlenderRNA *brna)
{
  StructRNA *srna;

  srna = RNA_def_struct(brna, "GpSculptPaint", "Paint");
  RNA_def_struct_path_func(srna, "rna_GpSculptPaint_path");
  RNA_def_struct_ui_text(srna, "Grease Pencil Sculpt Paint", "");
  RNA_def_struct_clear_flag(srna, STRUCT_UNDO);
}

static void rna_def_gp_weightpaint(BlenderRNA *brna)
{
  StructRNA *srna;

  srna = RNA_def_struct(brna, "GpWeightPaint", "Paint");
  RNA_def_struct_path_func(srna, "rna_GpWeightPaint_path");
  RNA_def_struct_ui_text(srna, "Grease Pencil Weight Paint", "");
  RNA_def_struct_clear_flag(srna, STRUCT_UNDO);
}

/* use for weight paint too */
static void rna_def_vertex_paint(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "VertexPaint", "Paint");
  RNA_def_struct_sdna(srna, "VPaint");
  RNA_def_struct_path_func(srna, "rna_VertexPaint_path");
  RNA_def_struct_ui_text(srna, "Vertex Paint", "Properties of vertex and weight paint mode");
  RNA_def_struct_clear_flag(srna, STRUCT_UNDO);

  /* weight paint only */
  prop = RNA_def_property(srna, "use_group_restrict", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", VP_FLAG_VGROUP_RESTRICT);
  RNA_def_property_ui_text(prop, "Restrict", "Restrict painting to vertices in the group");
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr);
}

static void rna_def_paint_mode(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "PaintModeSettings", nullptr);
  RNA_def_struct_sdna(srna, "PaintModeSettings");
  RNA_def_struct_path_func(srna, "rna_PaintModeSettings_path");
  RNA_def_struct_ui_text(srna, "Paint Mode", "Properties of paint mode");
  RNA_def_struct_clear_flag(srna, STRUCT_UNDO);

  prop = RNA_def_property(srna, "canvas_source", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, rna_enum_canvas_source_items);
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
  RNA_def_property_ui_text(prop, "Source", "Source to select canvas from");
  RNA_def_property_update(prop, 0, "rna_PaintModeSettings_canvas_source_update");

  prop = RNA_def_property(srna, "canvas_image", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_funcs(
      prop, nullptr, nullptr, nullptr, "rna_PaintModeSettings_canvas_image_poll");
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_CONTEXT_UPDATE);
  RNA_def_property_ui_text(prop, "Texture", "Image used as painting target");
}

static void rna_def_image_paint(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;
  FunctionRNA *func;

  static const EnumPropertyItem paint_type_items[] = {
      {IMAGEPAINT_MODE_MATERIAL,
       "MATERIAL",
       0,
       "Material",
       "Detect image slots from the material"},
      {IMAGEPAINT_MODE_IMAGE,
       "IMAGE",
       0,
       "Single Image",
       "Set image for texture painting directly"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem paint_interp_items[] = {
      {IMAGEPAINT_INTERP_LINEAR, "LINEAR", 0, "Linear", "Linear interpolation"},
      {IMAGEPAINT_INTERP_CLOSEST,
       "CLOSEST",
       0,
       "Closest",
       "No interpolation (sample closest texel)"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  srna = RNA_def_struct(brna, "ImagePaint", "Paint");
  RNA_def_struct_sdna(srna, "ImagePaintSettings");
  RNA_def_struct_path_func(srna, "rna_ImagePaintSettings_path");
  RNA_def_struct_ui_text(srna, "Image Paint", "Properties of image and texture painting mode");
  RNA_def_struct_clear_flag(srna, STRUCT_UNDO);

  /* functions */
  func = RNA_def_function(srna, "detect_data", "rna_ImaPaint_detect_data");
  RNA_def_function_ui_description(func, "Check if required texpaint data exist");

  /* return type */
  RNA_def_function_return(func, RNA_def_boolean(func, "ok", true, "", ""));

  /* booleans */
  prop = RNA_def_property(srna, "use_occlude", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "flag", IMAGEPAINT_PROJECT_XRAY);
  RNA_def_property_ui_text(
      prop, "Occlude", "Only paint onto the faces directly under the brush (slower)");
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr);

  prop = RNA_def_property(srna, "use_backface_culling", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "flag", IMAGEPAINT_PROJECT_BACKFACE);
  RNA_def_property_ui_text(prop, "Cull", "Ignore faces pointing away from the view (faster)");
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr);

  prop = RNA_def_property(srna, "use_normal_falloff", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "flag", IMAGEPAINT_PROJECT_FLAT);
  RNA_def_property_ui_text(prop, "Normal", "Paint most on faces pointing towards the view");
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr);

  prop = RNA_def_property(srna, "use_stencil_layer", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", IMAGEPAINT_PROJECT_LAYER_STENCIL);
  RNA_def_property_ui_text(prop, "Stencil Layer", "Set the mask layer from the UV map buttons");
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, "rna_ImaPaint_viewport_update");

  prop = RNA_def_property(srna, "invert_stencil", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", IMAGEPAINT_PROJECT_LAYER_STENCIL_INV);
  RNA_def_property_ui_text(prop, "Invert", "Invert the stencil layer");
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, "rna_ImaPaint_viewport_update");

  prop = RNA_def_property(srna, "stencil_image", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "stencil");
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_CONTEXT_UPDATE);
  RNA_def_property_ui_text(prop, "Stencil Image", "Image used as stencil");
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, "rna_ImaPaint_stencil_update");
  RNA_def_property_pointer_funcs(prop, nullptr, nullptr, nullptr, "rna_ImaPaint_imagetype_poll");

  prop = RNA_def_property(srna, "canvas", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_CONTEXT_UPDATE);
  RNA_def_property_ui_text(prop, "Canvas", "Image used as canvas");
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, "rna_ImaPaint_canvas_update");
  RNA_def_property_pointer_funcs(prop, nullptr, nullptr, nullptr, "rna_ImaPaint_imagetype_poll");

  prop = RNA_def_property(srna, "clone_image", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "clone");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Clone Image", "Image used as clone source");
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr);
  RNA_def_property_pointer_funcs(prop, nullptr, nullptr, nullptr, "rna_ImaPaint_imagetype_poll");

  prop = RNA_def_property(srna, "stencil_color", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_range(prop, 0.0, 1.0);
  RNA_def_property_float_sdna(prop, nullptr, "stencil_col");
  RNA_def_property_ui_text(prop, "Stencil Color", "Stencil color in the viewport");
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, "rna_ImaPaint_viewport_update");

  prop = RNA_def_property(srna, "dither", PROP_FLOAT, PROP_NONE);
  RNA_def_property_range(prop, 0.0, 2.0);
  RNA_def_property_ui_text(prop, "Dither", "Amount of dithering when painting on byte images");
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr);

  prop = RNA_def_property(srna, "use_clone_layer", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", IMAGEPAINT_PROJECT_LAYER_CLONE);
  RNA_def_property_ui_text(
      prop,
      "Clone Map",
      "Use another UV map as clone source, otherwise use the 3D cursor as the source");
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, "rna_ImaPaint_viewport_update");

  /* integers */

  prop = RNA_def_property(srna, "seam_bleed", PROP_INT, PROP_PIXEL);
  RNA_def_property_ui_range(prop, 0, 8, 1, -1);
  RNA_def_property_ui_text(
      prop, "Bleed", "Extend paint beyond the faces' UVs to reduce seams (in pixels, slower)");

  prop = RNA_def_property(srna, "normal_angle", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_range(prop, 0, 90);
  RNA_def_property_ui_text(
      prop, "Angle", "Paint most on faces pointing towards the view according to this angle");

  prop = RNA_def_int_array(srna,
                           "screen_grab_size",
                           2,
                           nullptr,
                           0,
                           0,
                           "Screen Grab Size",
                           "Size to capture the image for re-projecting",
                           0,
                           0);
  RNA_def_property_range(prop, 512, 16384);
  RNA_def_property_subtype(prop, PROP_PIXEL);

  prop = RNA_def_property(srna, "mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
  RNA_def_property_enum_items(prop, paint_type_items);
  RNA_def_property_ui_text(prop, "Mode", "Mode of operation for projection painting");
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, "rna_ImaPaint_mode_update");

  prop = RNA_def_property(srna, "interpolation", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "interp");
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
  RNA_def_property_enum_items(prop, paint_interp_items);
  RNA_def_property_ui_text(prop, "Interpolation", "Texture filtering type");
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, "rna_ImaPaint_mode_update");

  /* Missing data */
  prop = RNA_def_property(srna, "missing_uvs", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "missing_data", IMAGEPAINT_MISSING_UVS);
  RNA_def_property_ui_text(prop, "Missing UVs", "A UV layer is missing on the mesh");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);

  prop = RNA_def_property(srna, "missing_materials", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "missing_data", IMAGEPAINT_MISSING_MATERIAL);
  RNA_def_property_ui_text(prop, "Missing Materials", "The mesh is missing materials");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);

  prop = RNA_def_property(srna, "missing_stencil", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "missing_data", IMAGEPAINT_MISSING_STENCIL);
  RNA_def_property_ui_text(prop, "Missing Stencil", "Image Painting does not have a stencil");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);

  prop = RNA_def_property(srna, "missing_texture", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "missing_data", IMAGEPAINT_MISSING_TEX);
  RNA_def_property_ui_text(
      prop, "Missing Texture", "Image Painting does not have a texture to paint on");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);

  prop = RNA_def_property(srna, "clone_alpha", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "clone_alpha");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Clone Alpha", "Opacity of clone image display");
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr);

  prop = RNA_def_property(srna, "clone_offset", PROP_FLOAT, PROP_XYZ);
  RNA_def_property_float_sdna(prop, nullptr, "clone_offset");
  RNA_def_property_ui_text(prop, "Clone Offset", "");
  RNA_def_property_ui_range(prop, -1.0f, 1.0f, 10.0f, 3);
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr);
}

static void rna_def_particle_edit(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static const EnumPropertyItem select_mode_items[] = {
      {SCE_SELECT_PATH, "PATH", ICON_PARTICLE_PATH, "Path", "Path edit mode"},
      {SCE_SELECT_POINT, "POINT", ICON_PARTICLE_POINT, "Point", "Point select mode"},
      {SCE_SELECT_END, "TIP", ICON_PARTICLE_TIP, "Tip", "Tip select mode"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem puff_mode[] = {
      {0, "ADD", 0, "Add", "Make hairs more puffy"},
      {1, "SUB", 0, "Sub", "Make hairs less puffy"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem length_mode[] = {
      {0, "GROW", 0, "Grow", "Make hairs longer"},
      {1, "SHRINK", 0, "Shrink", "Make hairs shorter"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem edit_type_items[] = {
      {PE_TYPE_PARTICLES, "PARTICLES", 0, "Particles", ""},
      {PE_TYPE_SOFTBODY, "SOFT_BODY", 0, "Soft Body", ""},
      {PE_TYPE_CLOTH, "CLOTH", 0, "Cloth", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  /* edit */

  srna = RNA_def_struct(brna, "ParticleEdit", nullptr);
  RNA_def_struct_sdna(srna, "ParticleEditSettings");
  RNA_def_struct_path_func(srna, "rna_ParticleEdit_path");
  RNA_def_struct_ui_text(srna, "Particle Edit", "Properties of particle editing mode");
  RNA_def_struct_clear_flag(srna, STRUCT_UNDO);

  prop = RNA_def_property(srna, "tool", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "brushtype");
  RNA_def_property_enum_items(prop, rna_enum_particle_edit_hair_brush_items);
  RNA_def_property_enum_funcs(
      prop, nullptr, "rna_ParticleEdit_tool_set", "rna_ParticleEdit_tool_itemf");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_OPERATOR_DEFAULT);
  RNA_def_property_ui_text(prop, "Tool", "");

  prop = RNA_def_property(srna, "select_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, nullptr, "selectmode");
  RNA_def_property_enum_items(prop, select_mode_items);
  RNA_def_property_ui_text(prop, "Selection Mode", "Particle select and display mode");
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, "rna_ParticleEdit_update");

  prop = RNA_def_property(srna, "use_preserve_length", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", PE_KEEP_LENGTHS);
  RNA_def_property_ui_text(prop, "Keep Lengths", "Keep path lengths constant");

  prop = RNA_def_property(srna, "use_preserve_root", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", PE_LOCK_FIRST);
  RNA_def_property_ui_text(prop, "Keep Root", "Keep root keys unmodified");

  prop = RNA_def_property(srna, "use_emitter_deflect", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", PE_DEFLECT_EMITTER);
  RNA_def_property_ui_text(prop, "Deflect Emitter", "Keep paths from intersecting the emitter");

  prop = RNA_def_property(srna, "emitter_distance", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, nullptr, "emitterdist");
  RNA_def_property_ui_range(prop, 0.0f, 10.0f, 10, 3);
  RNA_def_property_ui_text(
      prop, "Emitter Distance", "Distance to keep particles away from the emitter");

  prop = RNA_def_property(srna, "use_fade_time", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", PE_FADE_TIME);
  RNA_def_property_ui_text(
      prop, "Fade Time", "Fade paths and keys further away from current frame");
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, "rna_ParticleEdit_update");

  prop = RNA_def_property(srna, "use_auto_velocity", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", PE_AUTO_VELOCITY);
  RNA_def_property_ui_text(prop, "Auto Velocity", "Calculate point velocities automatically");

  prop = RNA_def_property(srna, "show_particles", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", PE_DRAW_PART);
  RNA_def_property_ui_text(prop, "Display Particles", "Display actual particles");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, "rna_ParticleEdit_redo");

  prop = RNA_def_property(srna, "use_default_interpolate", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", PE_INTERPOLATE_ADDED);
  RNA_def_property_ui_text(
      prop, "Interpolate", "Interpolate new particles from the existing ones");
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr);

  prop = RNA_def_property(srna, "default_key_count", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "totaddkey");
  RNA_def_property_range(prop, 2, SHRT_MAX);
  RNA_def_property_ui_range(prop, 2, 20, 10, 3);
  RNA_def_property_ui_text(prop, "Keys", "How many keys to make new particles with");

  prop = RNA_def_property(srna, "brush", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "ParticleBrush");
  RNA_def_property_pointer_funcs(prop, "rna_ParticleEdit_brush_get", nullptr, nullptr, nullptr);
  RNA_def_property_ui_text(prop, "Brush", "");

  prop = RNA_def_property(srna, "display_step", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "draw_step");
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
  RNA_def_property_range(prop, 1, 10);
  RNA_def_property_ui_text(prop, "Steps", "How many steps to display the path with");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, "rna_ParticleEdit_redo");

  prop = RNA_def_property(srna, "fade_frames", PROP_INT, PROP_NONE);
  RNA_def_property_range(prop, 1, 100);
  RNA_def_property_ui_text(prop, "Frames", "How many frames to fade");
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, "rna_ParticleEdit_update");

  prop = RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
  RNA_def_property_enum_sdna(prop, nullptr, "edittype");
  RNA_def_property_enum_items(prop, edit_type_items);
  RNA_def_property_ui_text(prop, "Type", "");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, "rna_ParticleEdit_redo");

  prop = RNA_def_property(srna, "is_editable", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(prop, "rna_ParticleEdit_editable_get", nullptr);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Editable", "A valid edit mode exists");

  prop = RNA_def_property(srna, "is_hair", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(prop, "rna_ParticleEdit_hair_get", nullptr);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Hair", "Editing hair");

  prop = RNA_def_property(srna, "object", PROP_POINTER, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Object", "The edited object");

  prop = RNA_def_property(srna, "shape_object", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_CONTEXT_UPDATE);
  RNA_def_property_ui_text(prop, "Shape Object", "Outer shape to use for tools");
  RNA_def_property_pointer_funcs(prop, nullptr, nullptr, nullptr, "rna_Mesh_object_poll");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, "rna_ParticleEdit_redo");

  /* brush */

  srna = RNA_def_struct(brna, "ParticleBrush", nullptr);
  RNA_def_struct_sdna(srna, "ParticleBrushData");
  RNA_def_struct_path_func(srna, "rna_ParticleBrush_path");
  RNA_def_struct_ui_text(srna, "Particle Brush", "Particle editing brush");

  prop = RNA_def_property(srna, "size", PROP_INT, PROP_PIXEL);
  RNA_def_property_range(prop, 1, SHRT_MAX);
  RNA_def_property_ui_range(prop, 1, MAX_BRUSH_PIXEL_RADIUS, 10, 3);
  RNA_def_property_ui_text(prop, "Radius", "Radius of the brush in pixels");

  prop = RNA_def_property(srna, "strength", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_range(prop, 0.001, 1.0);
  RNA_def_property_ui_text(prop, "Strength", "Brush strength");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_AMOUNT);

  prop = RNA_def_property(srna, "count", PROP_INT, PROP_NONE);
  RNA_def_property_range(prop, 1, 1000);
  RNA_def_property_ui_range(prop, 1, 100, 10, 3);
  RNA_def_property_ui_text(prop, "Count", "Particle count");

  prop = RNA_def_property(srna, "steps", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "step");
  RNA_def_property_range(prop, 1, SHRT_MAX);
  RNA_def_property_ui_range(prop, 1, 50, 10, 3);
  RNA_def_property_ui_text(prop, "Steps", "Brush steps");

  prop = RNA_def_property(srna, "puff_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "invert");
  RNA_def_property_enum_items(prop, puff_mode);
  RNA_def_property_ui_text(prop, "Puff Mode", "");

  prop = RNA_def_property(srna, "use_puff_volume", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", PE_BRUSH_DATA_PUFF_VOLUME);
  RNA_def_property_ui_text(
      prop,
      "Puff Volume",
      "Apply puff to unselected end-points (helps maintain hair volume when puffing root)");

  prop = RNA_def_property(srna, "length_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "invert");
  RNA_def_property_enum_items(prop, length_mode);
  RNA_def_property_ui_text(prop, "Length Mode", "");

  /* dummy */
  prop = RNA_def_property(srna, "curve", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "CurveMapping");
  RNA_def_property_pointer_funcs(prop, "rna_ParticleBrush_curve_get", nullptr, nullptr, nullptr);
  RNA_def_property_ui_text(prop, "Curve", "");
}

/* srna -- gpencil speed guides */
static void rna_def_gpencil_guides(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "GPencilSculptGuide", nullptr);
  RNA_def_struct_sdna(srna, "GP_Sculpt_Guide");
  RNA_def_struct_path_func(srna, "rna_GPencilSculptGuide_path");
  RNA_def_struct_ui_text(srna, "Grease Pencil Sculpt Guide", "Guides for drawing");

  static const EnumPropertyItem prop_gpencil_guidetypes[] = {
      {GP_GUIDE_CIRCULAR, "CIRCULAR", 0, "Circular", "Use single point to create rings"},
      {GP_GUIDE_RADIAL, "RADIAL", 0, "Radial", "Use single point as direction"},
      {GP_GUIDE_PARALLEL, "PARALLEL", 0, "Parallel", "Parallel lines"},
      {GP_GUIDE_GRID, "GRID", 0, "Grid", "Grid allows horizontal and vertical lines"},
      {GP_GUIDE_ISO, "ISO", 0, "Isometric", "Grid allows isometric and vertical lines"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem prop_gpencil_guide_references[] = {
      {GP_GUIDE_REF_CURSOR, "CURSOR", 0, "Cursor", "Use cursor as reference point"},
      {GP_GUIDE_REF_CUSTOM, "CUSTOM", 0, "Custom", "Use custom reference point"},
      {GP_GUIDE_REF_OBJECT, "OBJECT", 0, "Object", "Use object as reference point"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  prop = RNA_def_property(srna, "use_guide", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "use_guide", false);
  RNA_def_property_boolean_default(prop, false);
  RNA_def_property_ui_text(prop, "Use Guides", "Enable speed guides");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr);

  prop = RNA_def_property(srna, "use_snapping", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "use_snapping", false);
  RNA_def_property_boolean_default(prop, false);
  RNA_def_property_ui_text(
      prop, "Use Snapping", "Enable snapping to guides angle or spacing options");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr);

  prop = RNA_def_property(srna, "reference_object", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "reference_object");
  RNA_def_property_ui_text(prop, "Object", "Object used for reference point");
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, "rna_ImaPaint_viewport_update");

  prop = RNA_def_property(srna, "reference_point", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "reference_point");
  RNA_def_property_enum_items(prop, prop_gpencil_guide_references);
  RNA_def_property_ui_text(prop, "Type", "Type of speed guide");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, "rna_ImaPaint_viewport_update");

  prop = RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "type");
  RNA_def_property_enum_items(prop, prop_gpencil_guidetypes);
  RNA_def_property_ui_text(prop, "Type", "Type of speed guide");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr);

  prop = RNA_def_property(srna, "angle", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_float_sdna(prop, nullptr, "angle");
  RNA_def_property_range(prop, -(M_PI * 2.0f), (M_PI * 2.0f));
  RNA_def_property_ui_text(prop, "Angle", "Direction of lines");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr);

  prop = RNA_def_property(srna, "angle_snap", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_float_sdna(prop, nullptr, "angle_snap");
  RNA_def_property_range(prop, -(M_PI * 2.0f), (M_PI * 2.0f));
  RNA_def_property_ui_text(prop, "Angle Snap", "Angle snapping");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr);

  prop = RNA_def_property(srna, "spacing", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, nullptr, "spacing");
  RNA_def_property_range(prop, 0.0f, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.0f, FLT_MAX, 1, 3);
  RNA_def_property_ui_text(prop, "Spacing", "Guide spacing");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr);

  prop = RNA_def_property(srna, "location", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, nullptr, "location");
  RNA_def_property_array(prop, 3);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Location", "Custom reference point for guides");
  RNA_def_property_range(prop, -FLT_MAX, FLT_MAX);
  RNA_def_property_ui_range(prop, -FLT_MAX, FLT_MAX, 1, 3);
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, "rna_ImaPaint_viewport_update");
}

static void rna_def_gpencil_sculpt(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  /* == Settings == */
  srna = RNA_def_struct(brna, "GPencilSculptSettings", nullptr);
  RNA_def_struct_sdna(srna, "GP_Sculpt_Settings");
  RNA_def_struct_path_func(srna, "rna_GPencilSculptSettings_path");
  RNA_def_struct_ui_text(srna,
                         "GPencil Sculpt Settings",
                         "General properties for Grease Pencil stroke sculpting tools");
  RNA_def_struct_clear_flag(srna, STRUCT_UNDO);

  prop = RNA_def_property(srna, "guide", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "GPencilSculptGuide");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Guide", "");

  prop = RNA_def_property(srna, "use_multiframe_falloff", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_SCULPT_SETT_FLAG_FRAME_FALLOFF);
  RNA_def_property_ui_text(
      prop,
      "Use Falloff",
      "Use falloff effect when edit in multiframe mode to compute brush effect by frame");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr);

  prop = RNA_def_property(srna, "use_thickness_curve", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_SCULPT_SETT_FLAG_PRIMITIVE_CURVE);
  RNA_def_property_ui_text(prop, "Use Curve", "Use curve to define primitive stroke thickness");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr);

  prop = RNA_def_property(srna, "use_scale_thickness", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_SCULPT_SETT_FLAG_SCALE_THICKNESS);
  RNA_def_property_ui_text(
      prop, "Scale Stroke Thickness", "Scale the stroke thickness when transforming strokes");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr);

  prop = RNA_def_property(srna, "use_automasking_stroke", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_SCULPT_SETT_FLAG_AUTOMASK_STROKE);
  RNA_def_property_ui_text(prop, "Auto-Masking Strokes", "Affect only strokes below the cursor");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr);

  prop = RNA_def_property(srna, "use_automasking_layer_stroke", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_SCULPT_SETT_FLAG_AUTOMASK_LAYER_STROKE);
  RNA_def_property_ui_text(prop, "Auto-Masking Layer", "Affect only strokes below the cursor");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr);

  prop = RNA_def_property(srna, "use_automasking_material_stroke", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(
      prop, nullptr, "flag", GP_SCULPT_SETT_FLAG_AUTOMASK_MATERIAL_STROKE);
  RNA_def_property_ui_text(prop, "Auto-Masking Material", "Affect only strokes below the cursor");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr);

  prop = RNA_def_property(srna, "use_automasking_layer_active", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", GP_SCULPT_SETT_FLAG_AUTOMASK_LAYER_ACTIVE);
  RNA_def_property_ui_text(prop, "Auto-Masking Layer", "Affect only the Active Layer");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr);

  prop = RNA_def_property(srna, "use_automasking_material_active", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(
      prop, nullptr, "flag", GP_SCULPT_SETT_FLAG_AUTOMASK_MATERIAL_ACTIVE);
  RNA_def_property_ui_text(prop, "Auto-Masking Material", "Affect only the Active Material");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr);

  /* custom falloff curve */
  prop = RNA_def_property(srna, "multiframe_falloff_curve", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "cur_falloff");
  RNA_def_property_struct_type(prop, "CurveMapping");
  RNA_def_property_ui_text(
      prop, "Curve", "Custom curve to control falloff of brush effect by Grease Pencil frames");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr);

  /* custom primitive curve */
  prop = RNA_def_property(srna, "thickness_primitive_curve", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "cur_primitive");
  RNA_def_property_struct_type(prop, "CurveMapping");
  RNA_def_property_ui_text(prop, "Curve", "Custom curve to control primitive thickness");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr);

  /* lock axis */
  prop = RNA_def_property(srna, "lock_axis", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "lock_axis");
  RNA_def_property_enum_items(prop, rna_enum_gpencil_lock_axis_items);
  RNA_def_property_ui_text(prop, "Lock Axis", "");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, nullptr);

  /* threshold for cutter */
  prop = RNA_def_property(srna, "intersection_threshold", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "isect_threshold");
  RNA_def_property_range(prop, 0.0f, 10.0f);
  RNA_def_property_float_default(prop, 0.1f);
  RNA_def_property_ui_text(prop, "Threshold", "Threshold for stroke intersections");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
}

static void rna_def_curves_sculpt(BlenderRNA *brna)
{
  StructRNA *srna;

  srna = RNA_def_struct(brna, "CurvesSculpt", "Paint");
  RNA_def_struct_path_func(srna, "rna_CurvesSculpt_path");
  RNA_def_struct_ui_text(srna, "Curves Sculpt Paint", "");
  RNA_def_struct_clear_flag(srna, STRUCT_UNDO);
}

void RNA_def_sculpt_paint(BlenderRNA *brna)
{
  /* *** Non-Animated *** */
  RNA_define_animate_sdna(false);
  rna_def_paint_curve(brna);
  rna_def_paint(brna);
  rna_def_unified_paint_settings(brna);
  rna_def_sculpt(brna);
  rna_def_uv_sculpt(brna);
  rna_def_gp_paint(brna);
  rna_def_gp_vertexpaint(brna);
  rna_def_gp_sculptpaint(brna);
  rna_def_gp_weightpaint(brna);
  rna_def_vertex_paint(brna);
  rna_def_paint_mode(brna);
  rna_def_image_paint(brna);
  rna_def_particle_edit(brna);
  rna_def_gpencil_guides(brna);
  rna_def_gpencil_sculpt(brna);
  rna_def_curves_sculpt(brna);
  RNA_define_animate_sdna(true);
}

#endif
