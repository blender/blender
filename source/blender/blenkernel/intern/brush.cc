/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */
#include <array>
#include <optional>

#include "MEM_guardedalloc.h"

/* Allow using deprecated functionality for .blend file I/O. */
#define DNA_DEPRECATED_ALLOW

#include "DNA_ID.h"
#include "DNA_brush_types.h"
#include "DNA_defaults.h"
#include "DNA_material_types.h"
#include "DNA_scene_types.h"

#include "BLI_listbase.h"
#include "BLI_math_base.hh"
#include "BLI_math_color.h"
#include "BLI_rand.h"

#include "BLT_translation.hh"

#include "BKE_asset.hh"
#include "BKE_bpath.hh"
#include "BKE_brush.hh"
#include "BKE_colorband.hh"
#include "BKE_colortools.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_idprop.hh"
#include "BKE_idtype.hh"
#include "BKE_lib_id.hh"
#include "BKE_lib_query.hh"
#include "BKE_lib_remap.hh"
#include "BKE_main.hh"
#include "BKE_material.hh"
#include "BKE_paint.hh"
#include "BKE_paint_types.hh"
#include "BKE_preview_image.hh"
#include "BKE_texture.h"

#include "IMB_colormanagement.hh"
#include "IMB_imbuf.hh"
#include "IMB_imbuf_types.hh"

#include "RE_texture.h" /* RE_texture_evaluate */

#include "BLO_read_write.hh"

static void brush_init_data(ID *id)
{
  Brush *brush = reinterpret_cast<Brush *>(id);
  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(brush, id));

  MEMCPY_STRUCT_AFTER(brush, DNA_struct_default_get(Brush), id);

  /* enable fake user by default */
  id_fake_user_set(&brush->id);

  /* the default alpha falloff curve */
  BKE_brush_curve_preset(brush, CURVE_PRESET_SMOOTH);

  brush->automasking_cavity_curve = BKE_paint_default_curve();

  brush->curve_rand_hue = BKE_paint_default_curve();
  brush->curve_rand_saturation = BKE_paint_default_curve();
  brush->curve_rand_value = BKE_paint_default_curve();

  brush->curve_size = BKE_paint_default_curve();
  brush->curve_strength = BKE_paint_default_curve();
  brush->curve_jitter = BKE_paint_default_curve();
}

static void brush_copy_data(Main * /*bmain*/,
                            std::optional<Library *> /*owner_library*/,
                            ID *id_dst,
                            const ID *id_src,
                            const int flag)
{
  Brush *brush_dst = reinterpret_cast<Brush *>(id_dst);
  const Brush *brush_src = reinterpret_cast<const Brush *>(id_src);

  if ((flag & LIB_ID_COPY_NO_PREVIEW) == 0) {
    BKE_previewimg_id_copy(&brush_dst->id, &brush_src->id);
  }
  else {
    brush_dst->preview = nullptr;
  }

  brush_dst->curve_distance_falloff = BKE_curvemapping_copy(brush_src->curve_distance_falloff);
  brush_dst->automasking_cavity_curve = BKE_curvemapping_copy(brush_src->automasking_cavity_curve);

  brush_dst->curve_rand_hue = BKE_curvemapping_copy(brush_src->curve_rand_hue);
  brush_dst->curve_rand_saturation = BKE_curvemapping_copy(brush_src->curve_rand_saturation);
  brush_dst->curve_rand_value = BKE_curvemapping_copy(brush_src->curve_rand_value);

  brush_dst->curve_size = BKE_curvemapping_copy(brush_src->curve_size);
  brush_dst->curve_strength = BKE_curvemapping_copy(brush_src->curve_strength);
  brush_dst->curve_jitter = BKE_curvemapping_copy(brush_src->curve_jitter);

  if (brush_src->gpencil_settings != nullptr) {
    brush_dst->gpencil_settings = MEM_dupallocN<BrushGpencilSettings>(
        __func__, *(brush_src->gpencil_settings));
    brush_dst->gpencil_settings->curve_sensitivity = BKE_curvemapping_copy(
        brush_src->gpencil_settings->curve_sensitivity);
    brush_dst->gpencil_settings->curve_strength = BKE_curvemapping_copy(
        brush_src->gpencil_settings->curve_strength);
    brush_dst->gpencil_settings->curve_jitter = BKE_curvemapping_copy(
        brush_src->gpencil_settings->curve_jitter);

    brush_dst->gpencil_settings->curve_rand_pressure = BKE_curvemapping_copy(
        brush_src->gpencil_settings->curve_rand_pressure);
    brush_dst->gpencil_settings->curve_rand_strength = BKE_curvemapping_copy(
        brush_src->gpencil_settings->curve_rand_strength);
    brush_dst->gpencil_settings->curve_rand_uv = BKE_curvemapping_copy(
        brush_src->gpencil_settings->curve_rand_uv);
    brush_dst->gpencil_settings->curve_rand_hue = BKE_curvemapping_copy(
        brush_src->gpencil_settings->curve_rand_hue);
    brush_dst->gpencil_settings->curve_rand_saturation = BKE_curvemapping_copy(
        brush_src->gpencil_settings->curve_rand_saturation);
    brush_dst->gpencil_settings->curve_rand_value = BKE_curvemapping_copy(
        brush_src->gpencil_settings->curve_rand_value);
  }
  if (brush_src->curves_sculpt_settings != nullptr) {
    brush_dst->curves_sculpt_settings = MEM_dupallocN<BrushCurvesSculptSettings>(
        __func__, *(brush_src->curves_sculpt_settings));
    brush_dst->curves_sculpt_settings->curve_parameter_falloff = BKE_curvemapping_copy(
        brush_src->curves_sculpt_settings->curve_parameter_falloff);
  }

  /* enable fake user by default */
  id_fake_user_set(&brush_dst->id);
}

static void brush_free_data(ID *id)
{
  Brush *brush = reinterpret_cast<Brush *>(id);
  BKE_curvemapping_free(brush->curve_distance_falloff);
  BKE_curvemapping_free(brush->automasking_cavity_curve);

  BKE_curvemapping_free(brush->curve_rand_hue);
  BKE_curvemapping_free(brush->curve_rand_saturation);
  BKE_curvemapping_free(brush->curve_rand_value);

  BKE_curvemapping_free(brush->curve_size);
  BKE_curvemapping_free(brush->curve_strength);
  BKE_curvemapping_free(brush->curve_jitter);

  if (brush->gpencil_settings != nullptr) {
    BKE_curvemapping_free(brush->gpencil_settings->curve_sensitivity);
    BKE_curvemapping_free(brush->gpencil_settings->curve_strength);
    BKE_curvemapping_free(brush->gpencil_settings->curve_jitter);

    BKE_curvemapping_free(brush->gpencil_settings->curve_rand_pressure);
    BKE_curvemapping_free(brush->gpencil_settings->curve_rand_strength);
    BKE_curvemapping_free(brush->gpencil_settings->curve_rand_uv);
    BKE_curvemapping_free(brush->gpencil_settings->curve_rand_hue);
    BKE_curvemapping_free(brush->gpencil_settings->curve_rand_saturation);
    BKE_curvemapping_free(brush->gpencil_settings->curve_rand_value);

    MEM_SAFE_FREE(brush->gpencil_settings);
  }
  if (brush->curves_sculpt_settings != nullptr) {
    BKE_curvemapping_free(brush->curves_sculpt_settings->curve_parameter_falloff);
    MEM_freeN(brush->curves_sculpt_settings);
  }

  MEM_SAFE_FREE(brush->gradient);

  BKE_previewimg_free(&(brush->preview));
}

static void brush_make_local(Main *bmain, ID *id, const int flags)
{
  if (!ID_IS_LINKED(id)) {
    return;
  }

  Brush *brush = reinterpret_cast<Brush *>(id);
  const bool lib_local = (flags & LIB_ID_MAKELOCAL_FULL_LIBRARY) != 0;

  bool force_local, force_copy;
  BKE_lib_id_make_local_generic_action_define(bmain, id, flags, &force_local, &force_copy);

  if (force_local) {
    BKE_lib_id_clear_library_data(bmain, &brush->id, flags);
    BKE_lib_id_expand_local(bmain, &brush->id, flags);

    /* enable fake user by default */
    id_fake_user_set(&brush->id);
  }
  else if (force_copy) {
    Brush *brush_new = reinterpret_cast<Brush *>(
        BKE_id_copy(bmain, &brush->id)); /* Ensures FAKE_USER is set */

    id_us_min(&brush_new->id);

    BLI_assert(brush_new->id.flag & ID_FLAG_FAKEUSER);
    BLI_assert(brush_new->id.us == 1);

    /* Setting `newid` is mandatory for complex #make_lib_local logic. */
    ID_NEW_SET(brush, brush_new);

    if (!lib_local) {
      BKE_libblock_remap(bmain, brush, brush_new, ID_REMAP_SKIP_INDIRECT_USAGE);
    }
  }
}

static void brush_foreach_id(ID *id, LibraryForeachIDData *data)
{
  Brush *brush = reinterpret_cast<Brush *>(id);

  BKE_LIB_FOREACHID_PROCESS_IDSUPER(data, brush->paint_curve, IDWALK_CB_USER);
  if (brush->gpencil_settings) {
    BKE_LIB_FOREACHID_PROCESS_IDSUPER(data, brush->gpencil_settings->material, IDWALK_CB_USER);
    BKE_LIB_FOREACHID_PROCESS_IDSUPER(data, brush->gpencil_settings->material_alt, IDWALK_CB_USER);
  }
  BKE_LIB_FOREACHID_PROCESS_FUNCTION_CALL(data, BKE_texture_mtex_foreach_id(data, &brush->mtex));
  BKE_LIB_FOREACHID_PROCESS_FUNCTION_CALL(data,
                                          BKE_texture_mtex_foreach_id(data, &brush->mask_mtex));
}

static void brush_foreach_working_space_color(ID *id, const IDTypeForeachColorFunctionCallback &fn)
{
  Brush *brush = reinterpret_cast<Brush *>(id);

  fn.single(brush->color);
  fn.single(brush->secondary_color);
  if (brush->gradient) {
    BKE_colorband_foreach_working_space_color(brush->gradient, fn);
  }

  BKE_brush_color_sync_legacy(brush);
}

static void brush_blend_write(BlendWriter *writer, ID *id, const void *id_address)
{
  Brush *brush = reinterpret_cast<Brush *>(id);

  BLO_write_id_struct(writer, Brush, id_address, &brush->id);
  BKE_id_blend_write(writer, &brush->id);

  if (brush->curve_distance_falloff) {
    BKE_curvemapping_blend_write(writer, brush->curve_distance_falloff);
  }

  if (brush->automasking_cavity_curve) {
    BKE_curvemapping_blend_write(writer, brush->automasking_cavity_curve);
  }

  if (brush->curve_rand_hue) {
    BKE_curvemapping_blend_write(writer, brush->curve_rand_hue);
  }
  if (brush->curve_rand_saturation) {
    BKE_curvemapping_blend_write(writer, brush->curve_rand_saturation);
  }
  if (brush->curve_rand_value) {
    BKE_curvemapping_blend_write(writer, brush->curve_rand_value);
  }

  if (brush->curve_size) {
    BKE_curvemapping_blend_write(writer, brush->curve_size);
  }
  if (brush->curve_strength) {
    BKE_curvemapping_blend_write(writer, brush->curve_strength);
  }
  if (brush->curve_jitter) {
    BKE_curvemapping_blend_write(writer, brush->curve_jitter);
  }

  if (brush->gpencil_settings) {
    BLO_write_struct(writer, BrushGpencilSettings, brush->gpencil_settings);

    if (brush->gpencil_settings->curve_sensitivity) {
      BKE_curvemapping_blend_write(writer, brush->gpencil_settings->curve_sensitivity);
    }
    if (brush->gpencil_settings->curve_strength) {
      BKE_curvemapping_blend_write(writer, brush->gpencil_settings->curve_strength);
    }
    if (brush->gpencil_settings->curve_jitter) {
      BKE_curvemapping_blend_write(writer, brush->gpencil_settings->curve_jitter);
    }
    if (brush->gpencil_settings->curve_rand_pressure) {
      BKE_curvemapping_blend_write(writer, brush->gpencil_settings->curve_rand_pressure);
    }
    if (brush->gpencil_settings->curve_rand_strength) {
      BKE_curvemapping_blend_write(writer, brush->gpencil_settings->curve_rand_strength);
    }
    if (brush->gpencil_settings->curve_rand_uv) {
      BKE_curvemapping_blend_write(writer, brush->gpencil_settings->curve_rand_uv);
    }
    if (brush->gpencil_settings->curve_rand_hue) {
      BKE_curvemapping_blend_write(writer, brush->gpencil_settings->curve_rand_hue);
    }
    if (brush->gpencil_settings->curve_rand_saturation) {
      BKE_curvemapping_blend_write(writer, brush->gpencil_settings->curve_rand_saturation);
    }
    if (brush->gpencil_settings->curve_rand_value) {
      BKE_curvemapping_blend_write(writer, brush->gpencil_settings->curve_rand_value);
    }
  }
  if (brush->curves_sculpt_settings) {
    BLO_write_struct(writer, BrushCurvesSculptSettings, brush->curves_sculpt_settings);
    BKE_curvemapping_blend_write(writer, brush->curves_sculpt_settings->curve_parameter_falloff);
  }
  if (brush->gradient) {
    BLO_write_struct(writer, ColorBand, brush->gradient);
  }

  BKE_previewimg_blend_write(writer, brush->preview);
}

static void brush_blend_read_data(BlendDataReader *reader, ID *id)
{
  Brush *brush = reinterpret_cast<Brush *>(id);

  /* Falloff curve. */
  BLO_read_struct(reader, CurveMapping, &brush->curve_distance_falloff);

  BLO_read_struct(reader, ColorBand, &brush->gradient);

  if (brush->curve_distance_falloff) {
    BKE_curvemapping_blend_read(reader, brush->curve_distance_falloff);
  }
  else {
    BKE_brush_curve_preset(brush, CURVE_PRESET_SHARP);
  }

  BLO_read_struct(reader, CurveMapping, &brush->automasking_cavity_curve);
  if (brush->automasking_cavity_curve) {
    BKE_curvemapping_blend_read(reader, brush->automasking_cavity_curve);
  }
  else {
    brush->automasking_cavity_curve = BKE_sculpt_default_cavity_curve();
  }

  BLO_read_struct(reader, CurveMapping, &brush->curve_rand_hue);
  if (brush->curve_rand_hue) {
    BKE_curvemapping_blend_read(reader, brush->curve_rand_hue);
  }
  else {
    brush->curve_rand_hue = BKE_paint_default_curve();
  }

  BLO_read_struct(reader, CurveMapping, &brush->curve_rand_saturation);
  if (brush->curve_rand_saturation) {
    BKE_curvemapping_blend_read(reader, brush->curve_rand_saturation);
  }
  else {
    brush->curve_rand_saturation = BKE_paint_default_curve();
  }

  BLO_read_struct(reader, CurveMapping, &brush->curve_rand_value);
  if (brush->curve_rand_value) {
    BKE_curvemapping_blend_read(reader, brush->curve_rand_value);
  }
  else {
    brush->curve_rand_value = BKE_paint_default_curve();
  }

  BLO_read_struct(reader, CurveMapping, &brush->curve_size);
  if (brush->curve_size) {
    BKE_curvemapping_blend_read(reader, brush->curve_size);
  }
  else {
    brush->curve_size = BKE_paint_default_curve();
  }

  BLO_read_struct(reader, CurveMapping, &brush->curve_strength);
  if (brush->curve_strength) {
    BKE_curvemapping_blend_read(reader, brush->curve_strength);
  }
  else {
    brush->curve_strength = BKE_paint_default_curve();
  }

  BLO_read_struct(reader, CurveMapping, &brush->curve_jitter);
  if (brush->curve_jitter) {
    BKE_curvemapping_blend_read(reader, brush->curve_jitter);
  }
  else {
    brush->curve_jitter = BKE_paint_default_curve();
  }

  /* grease pencil */
  BLO_read_struct(reader, BrushGpencilSettings, &brush->gpencil_settings);
  if (brush->gpencil_settings != nullptr) {
    BLO_read_struct(reader, CurveMapping, &brush->gpencil_settings->curve_sensitivity);
    BLO_read_struct(reader, CurveMapping, &brush->gpencil_settings->curve_strength);
    BLO_read_struct(reader, CurveMapping, &brush->gpencil_settings->curve_jitter);

    BLO_read_struct(reader, CurveMapping, &brush->gpencil_settings->curve_rand_pressure);
    BLO_read_struct(reader, CurveMapping, &brush->gpencil_settings->curve_rand_strength);
    BLO_read_struct(reader, CurveMapping, &brush->gpencil_settings->curve_rand_uv);
    BLO_read_struct(reader, CurveMapping, &brush->gpencil_settings->curve_rand_hue);
    BLO_read_struct(reader, CurveMapping, &brush->gpencil_settings->curve_rand_saturation);
    BLO_read_struct(reader, CurveMapping, &brush->gpencil_settings->curve_rand_value);

    if (brush->gpencil_settings->curve_sensitivity) {
      BKE_curvemapping_blend_read(reader, brush->gpencil_settings->curve_sensitivity);
    }

    if (brush->gpencil_settings->curve_strength) {
      BKE_curvemapping_blend_read(reader, brush->gpencil_settings->curve_strength);
    }

    if (brush->gpencil_settings->curve_jitter) {
      BKE_curvemapping_blend_read(reader, brush->gpencil_settings->curve_jitter);
    }

    if (brush->gpencil_settings->curve_rand_pressure) {
      BKE_curvemapping_blend_read(reader, brush->gpencil_settings->curve_rand_pressure);
    }

    if (brush->gpencil_settings->curve_rand_strength) {
      BKE_curvemapping_blend_read(reader, brush->gpencil_settings->curve_rand_strength);
    }

    if (brush->gpencil_settings->curve_rand_uv) {
      BKE_curvemapping_blend_read(reader, brush->gpencil_settings->curve_rand_uv);
    }

    if (brush->gpencil_settings->curve_rand_hue) {
      BKE_curvemapping_blend_read(reader, brush->gpencil_settings->curve_rand_hue);
    }

    if (brush->gpencil_settings->curve_rand_saturation) {
      BKE_curvemapping_blend_read(reader, brush->gpencil_settings->curve_rand_saturation);
    }

    if (brush->gpencil_settings->curve_rand_value) {
      BKE_curvemapping_blend_read(reader, brush->gpencil_settings->curve_rand_value);
    }
  }

  BLO_read_struct(reader, BrushCurvesSculptSettings, &brush->curves_sculpt_settings);
  if (brush->curves_sculpt_settings) {
    BLO_read_struct(reader, CurveMapping, &brush->curves_sculpt_settings->curve_parameter_falloff);
    if (brush->curves_sculpt_settings->curve_parameter_falloff) {
      BKE_curvemapping_blend_read(reader, brush->curves_sculpt_settings->curve_parameter_falloff);
    }
  }

  BLO_read_struct(reader, PreviewImage, &brush->preview);
  BKE_previewimg_blend_read(reader, brush->preview);

  brush->has_unsaved_changes = false;
}

static void brush_blend_read_after_liblink(BlendLibReader * /*reader*/, ID *id)
{
  Brush *brush = reinterpret_cast<Brush *>(id);

  /* Update brush settings depending on availability of other IDs. */
  if (brush->gpencil_settings != nullptr) {
    if (brush->gpencil_settings->flag & GP_BRUSH_MATERIAL_PINNED) {
      if (!brush->gpencil_settings->material) {
        brush->gpencil_settings->flag &= ~GP_BRUSH_MATERIAL_PINNED;
      }
    }
    else {
      brush->gpencil_settings->material = nullptr;
    }
  }
}

static void brush_asset_metadata_ensure(void *asset_ptr, AssetMetaData *asset_data)
{
  using namespace blender;
  using namespace blender::bke;

  Brush *brush = reinterpret_cast<Brush *>(asset_ptr);
  BLI_assert(GS(brush->id.name) == ID_BR);

  /* Most names copied from brush RNA (not all are available there though). */
  constexpr std::array mode_map{
      std::tuple{"use_paint_sculpt", OB_MODE_SCULPT, "sculpt_brush_type"},
      std::tuple{"use_paint_vertex", OB_MODE_VERTEX_PAINT, "vertex_brush_type"},
      std::tuple{"use_paint_weight", OB_MODE_WEIGHT_PAINT, "weight_brush_type"},
      std::tuple{"use_paint_image", OB_MODE_TEXTURE_PAINT, "image_brush_type"},
      /* Sculpt UVs in the image editor while in edit mode. */
      std::tuple{"use_paint_uv_sculpt", OB_MODE_EDIT, "image_brush_type"},
      std::tuple{"use_paint_grease_pencil", OB_MODE_PAINT_GREASE_PENCIL, "gpencil_brush_type"},
      /* Note: Not defined in brush RNA, own name. */
      std::tuple{
          "use_sculpt_grease_pencil", OB_MODE_SCULPT_GREASE_PENCIL, "gpencil_sculpt_brush_type"},
      std::tuple{
          "use_vertex_grease_pencil", OB_MODE_VERTEX_GREASE_PENCIL, "gpencil_vertex_brush_type"},
      std::tuple{"use_weight_gpencil", OB_MODE_WEIGHT_GREASE_PENCIL, "gpencil_weight_brush_type"},
      std::tuple{"use_paint_sculpt_curves", OB_MODE_SCULPT_CURVES, "curves_sculpt_brush_type"},
  };

  for (const auto &[prop_name, mode, tool_prop_name] : mode_map) {
    /* Only add booleans for supported modes. */
    if (!(brush->ob_mode & mode)) {
      continue;
    }
    auto mode_property = idprop::create_bool(prop_name, true);
    BKE_asset_metadata_idprop_ensure(asset_data, mode_property.release());

    if (std::optional<int> brush_tool = BKE_paint_get_brush_type_from_obmode(brush, mode)) {
      auto type_property = idprop::create(tool_prop_name, *brush_tool);
      BKE_asset_metadata_idprop_ensure(asset_data, type_property.release());
    }
    else {
      BLI_assert_unreachable();
    }
  }
}

static AssetTypeInfo AssetType_BR = {
    /*pre_save_fn*/ brush_asset_metadata_ensure,
    /*on_mark_asset_fn*/ brush_asset_metadata_ensure,
};

IDTypeInfo IDType_ID_BR = {
    /*id_code*/ Brush::id_type,
    /*id_filter*/ FILTER_ID_BR,
    /*dependencies_id_types*/
    (FILTER_ID_IM | FILTER_ID_PC | FILTER_ID_TE | FILTER_ID_MA),
    /*main_listbase_index*/ INDEX_ID_BR,
    /*struct_size*/ sizeof(Brush),
    /*name*/ "Brush",
    /*name_plural*/ N_("brushes"),
    /*translation_context*/ BLT_I18NCONTEXT_ID_BRUSH,
    /*flags*/ IDTYPE_FLAGS_NO_ANIMDATA | IDTYPE_FLAGS_NO_MEMFILE_UNDO,
    /*asset_type_info*/ &AssetType_BR,

    /*init_data*/ brush_init_data,
    /*copy_data*/ brush_copy_data,
    /*free_data*/ brush_free_data,
    /*make_local*/ brush_make_local,
    /*foreach_id*/ brush_foreach_id,
    /*foreach_cache*/ nullptr,
    /*foreach_path*/ nullptr,
    /*foreach_working_space_color*/ brush_foreach_working_space_color,
    /*owner_pointer_get*/ nullptr,

    /*blend_write*/ brush_blend_write,
    /*blend_read_data*/ brush_blend_read_data,
    /*blend_read_after_liblink*/ brush_blend_read_after_liblink,

    /*blend_read_undo_preserve*/ nullptr,

    /*lib_override_apply_post*/ nullptr,
};

static RNG *brush_rng;

void BKE_brush_system_init()
{
  brush_rng = BLI_rng_new(0);
  BLI_rng_srandom(brush_rng, 31415682);
}

void BKE_brush_system_exit()
{
  if (brush_rng == nullptr) {
    return;
  }
  BLI_rng_free(brush_rng);
  brush_rng = nullptr;
}

static void brush_defaults(Brush *brush)
{

  const Brush *brush_def = DNA_struct_default_get(Brush);

#define FROM_DEFAULT(member) \
  memcpy((void *)&brush->member, (void *)&brush_def->member, sizeof(brush->member))
#define FROM_DEFAULT_PTR(member) memcpy(brush->member, brush_def->member, sizeof(brush->member))

  FROM_DEFAULT(blend);
  FROM_DEFAULT(flag);
  FROM_DEFAULT(weight);
  FROM_DEFAULT(size);
  FROM_DEFAULT(alpha);
  FROM_DEFAULT(hardness);
  FROM_DEFAULT(autosmooth_factor);
  FROM_DEFAULT(topology_rake_factor);
  FROM_DEFAULT(crease_pinch_factor);
  FROM_DEFAULT(normal_radius_factor);
  FROM_DEFAULT(wet_paint_radius_factor);
  FROM_DEFAULT(area_radius_factor);
  FROM_DEFAULT(disconnected_distance_max);
  FROM_DEFAULT(sculpt_plane);
  FROM_DEFAULT(plane_offset);
  FROM_DEFAULT(normal_weight);
  FROM_DEFAULT(fill_threshold);
  FROM_DEFAULT(flag);
  FROM_DEFAULT(sampling_flag);
  FROM_DEFAULT_PTR(color);
  FROM_DEFAULT_PTR(secondary_color);
  FROM_DEFAULT(spacing);
  FROM_DEFAULT(smooth_stroke_radius);
  FROM_DEFAULT(smooth_stroke_factor);
  FROM_DEFAULT(rate);
  FROM_DEFAULT(jitter);
  FROM_DEFAULT(texture_sample_bias);
  FROM_DEFAULT(texture_overlay_alpha);
  FROM_DEFAULT(mask_overlay_alpha);
  FROM_DEFAULT(cursor_overlay_alpha);
  FROM_DEFAULT(overlay_flags);
  FROM_DEFAULT_PTR(add_col);
  FROM_DEFAULT_PTR(sub_col);
  FROM_DEFAULT(stencil_pos);
  FROM_DEFAULT(stencil_dimension);
  FROM_DEFAULT(mtex);
  FROM_DEFAULT(mask_mtex);
  FROM_DEFAULT(falloff_shape);
  FROM_DEFAULT(tip_scale_x);
  FROM_DEFAULT(tip_roundness);

#undef FROM_DEFAULT
#undef FROM_DEFAULT_PTR
}

/* Datablock add/copy/free/make_local */

Brush *BKE_brush_add(Main *bmain, const char *name, const eObjectMode ob_mode)
{
  Brush *brush = BKE_id_new<Brush>(bmain, name);

  brush->ob_mode = ob_mode;

  if (ob_mode == OB_MODE_SCULPT_CURVES) {
    BKE_brush_init_curves_sculpt_settings(brush);
  }
  else if (ELEM(ob_mode,
                OB_MODE_PAINT_GREASE_PENCIL,
                OB_MODE_SCULPT_GREASE_PENCIL,
                OB_MODE_WEIGHT_GREASE_PENCIL,
                OB_MODE_VERTEX_GREASE_PENCIL))
  {
    BKE_brush_init_gpencil_settings(brush);
  }

  return brush;
}

void BKE_brush_init_gpencil_settings(Brush *brush)
{
  if (brush->gpencil_settings == nullptr) {
    brush->gpencil_settings = MEM_callocN<BrushGpencilSettings>("BrushGpencilSettings");
  }

  brush->gpencil_settings->draw_smoothlvl = 1;
  brush->gpencil_settings->flag = 0;
  brush->gpencil_settings->flag |= GP_BRUSH_USE_PRESSURE;
  brush->gpencil_settings->draw_strength = 1.0f;
  brush->gpencil_settings->draw_jitter = 0.0f;
  brush->gpencil_settings->flag |= GP_BRUSH_USE_JITTER_PRESSURE;

  /* curves */
  brush->gpencil_settings->curve_sensitivity = BKE_curvemapping_add(1, 0.0f, 0.0f, 1.0f, 1.0f);
  brush->gpencil_settings->curve_strength = BKE_curvemapping_add(1, 0.0f, 0.0f, 1.0f, 1.0f);
  brush->gpencil_settings->curve_jitter = BKE_curvemapping_add(1, 0.0f, 0.0f, 1.0f, 1.0f);

  brush->gpencil_settings->curve_rand_pressure = BKE_curvemapping_add(1, 0.0f, 0.0f, 1.0f, 1.0f);
  brush->gpencil_settings->curve_rand_strength = BKE_curvemapping_add(1, 0.0f, 0.0f, 1.0f, 1.0f);
  brush->gpencil_settings->curve_rand_uv = BKE_curvemapping_add(1, 0.0f, 0.0f, 1.0f, 1.0f);
  brush->gpencil_settings->curve_rand_hue = BKE_curvemapping_add(1, 0.0f, 0.0f, 1.0f, 1.0f);
  brush->gpencil_settings->curve_rand_saturation = BKE_curvemapping_add(1, 0.0f, 0.0f, 1.0f, 1.0f);
  brush->gpencil_settings->curve_rand_value = BKE_curvemapping_add(1, 0.0f, 0.0f, 1.0f, 1.0f);
}

bool BKE_brush_delete(Main *bmain, Brush *brush)
{
  if (brush->id.tag & ID_TAG_INDIRECT) {
    return false;
  }
  if (ID_REAL_USERS(brush) <= 1 && ID_EXTRA_USERS(brush) == 0 &&
      BKE_library_ID_is_indirectly_used(bmain, brush))
  {
    return false;
  }

  BKE_id_delete(bmain, brush);

  return true;
}

Brush *BKE_brush_duplicate(Main *bmain,
                           Brush *brush,
                           eDupli_ID_Flags /*dupflag*/,
                           /*eLibIDDuplicateFlags*/ uint duplicate_options)
{
  const bool is_subprocess = (duplicate_options & LIB_ID_DUPLICATE_IS_SUBPROCESS) != 0;
  const bool is_root_id = (duplicate_options & LIB_ID_DUPLICATE_IS_ROOT_ID) != 0;

  const eDupli_ID_Flags dupflag = USER_DUP_OBDATA | USER_DUP_LINKED_ID;

  if (!is_subprocess) {
    BKE_main_id_newptr_and_tag_clear(bmain);
  }
  if (is_root_id) {
    duplicate_options &= ~LIB_ID_DUPLICATE_IS_ROOT_ID;
  }

  constexpr int id_copy_flag = LIB_ID_COPY_DEFAULT;

  Brush *new_brush = reinterpret_cast<Brush *>(
      BKE_id_copy_for_duplicate(bmain, &brush->id, dupflag, id_copy_flag));

  /* Currently this duplicates everything and the passed in value of `dupflag` is ignored. Ideally,
   * this should both check user preferences and do further filtering based on eDupli_ID_Flags. */
  auto dependencies_cb = [&](const LibraryIDLinkCallbackData *cb_data) -> int {
    if (cb_data->cb_flag & (IDWALK_CB_EMBEDDED | IDWALK_CB_EMBEDDED_NOT_OWNING)) {
      return IDWALK_NOP;
    }
    if (cb_data->cb_flag & IDWALK_CB_LOOPBACK) {
      return IDWALK_NOP;
    }

    BKE_id_copy_for_duplicate(bmain, *cb_data->id_pointer, dupflag, id_copy_flag);
    return IDWALK_NOP;
  };

  BKE_library_foreach_ID_link(bmain, &new_brush->id, dependencies_cb, nullptr, IDWALK_RECURSE);

  if (!is_subprocess) {
    /* This code will follow into all ID links using an ID tagged with ID_TAG_NEW. */
    BKE_libblock_relink_to_newid(bmain, &new_brush->id, ID_REMAP_SKIP_USER_CLEAR);

#ifndef NDEBUG
    /* Call to `BKE_libblock_relink_to_newid` above is supposed to have cleared all those flags. */
    ID *id_iter;
    FOREACH_MAIN_ID_BEGIN (bmain, id_iter) {
      BLI_assert((id_iter->tag & ID_TAG_NEW) == 0);
    }
    FOREACH_MAIN_ID_END;
#endif

    /* Cleanup. */
    BKE_main_id_newptr_and_tag_clear(bmain);
  }

  return new_brush;
}

void BKE_brush_init_curves_sculpt_settings(Brush *brush)
{
  if (brush->curves_sculpt_settings == nullptr) {
    brush->curves_sculpt_settings = MEM_callocN<BrushCurvesSculptSettings>(__func__);
  }
  BrushCurvesSculptSettings *settings = brush->curves_sculpt_settings;
  settings->flag = BRUSH_CURVES_SCULPT_FLAG_INTERPOLATE_RADIUS;
  settings->add_amount = 1;
  settings->points_per_curve = 8;
  settings->minimum_length = 0.01f;
  settings->curve_length = 0.3f;
  settings->curve_radius = 0.01f;
  settings->density_add_attempts = 100;
  settings->curve_parameter_falloff = BKE_curvemapping_add(1, 0.0f, 0.0f, 1.0f, 1.0f);
}

void BKE_brush_tag_unsaved_changes(Brush *brush)
{
  if (brush && ID_IS_LINKED(brush)) {
    brush->has_unsaved_changes = true;
  }
}

Brush *BKE_brush_first_search(Main *bmain, const eObjectMode ob_mode)
{
  LISTBASE_FOREACH (Brush *, brush, &bmain->brushes) {
    if (brush->ob_mode & ob_mode) {
      return brush;
    }
  }
  return nullptr;
}

void BKE_brush_debug_print_state(Brush *br)
{
  /* create a fake brush and set it to the defaults */
  Brush def = blender::dna::shallow_zero_initialize();
  brush_defaults(&def);

#define BR_TEST(field, t) \
  if (br->field != def.field) { \
    printf("br->" #field " = %" #t ";\n", br->field); \
  } \
  ((void)0)

#define BR_TEST_FLAG(_f) \
  if ((br->flag & _f) && !(def.flag & _f)) { \
    printf("br->flag |= " #_f ";\n"); \
  } \
  else if (!(br->flag & _f) && (def.flag & _f)) { \
    printf("br->flag &= ~" #_f ";\n"); \
  } \
  ((void)0)

#define BR_TEST_FLAG_OVERLAY(_f) \
  if ((br->overlay_flags & _f) && !(def.overlay_flags & _f)) { \
    printf("br->overlay_flags |= " #_f ";\n"); \
  } \
  else if (!(br->overlay_flags & _f) && (def.overlay_flags & _f)) { \
    printf("br->overlay_flags &= ~" #_f ";\n"); \
  } \
  ((void)0)

  /* print out any non-default brush state */
  BR_TEST(normal_weight, f);

  BR_TEST(blend, d);
  BR_TEST(size, d);

  /* br->flag */
  BR_TEST_FLAG(BRUSH_AIRBRUSH);
  BR_TEST_FLAG(BRUSH_ALPHA_PRESSURE);
  BR_TEST_FLAG(BRUSH_SIZE_PRESSURE);
  BR_TEST_FLAG(BRUSH_JITTER_PRESSURE);
  BR_TEST_FLAG(BRUSH_SPACING_PRESSURE);
  BR_TEST_FLAG(BRUSH_ANCHORED);
  BR_TEST_FLAG(BRUSH_DIR_IN);
  BR_TEST_FLAG(BRUSH_SPACE);
  BR_TEST_FLAG(BRUSH_SMOOTH_STROKE);
  BR_TEST_FLAG(BRUSH_PERSISTENT);
  BR_TEST_FLAG(BRUSH_ACCUMULATE);
  BR_TEST_FLAG(BRUSH_LOCK_ALPHA);
  BR_TEST_FLAG(BRUSH_ORIGINAL_NORMAL);
  BR_TEST_FLAG(BRUSH_OFFSET_PRESSURE);
  BR_TEST_FLAG(BRUSH_SPACE_ATTEN);
  BR_TEST_FLAG(BRUSH_ADAPTIVE_SPACE);
  BR_TEST_FLAG(BRUSH_LOCK_SIZE);
  BR_TEST_FLAG(BRUSH_EDGE_TO_EDGE);
  BR_TEST_FLAG(BRUSH_DRAG_DOT);
  BR_TEST_FLAG(BRUSH_INVERSE_SMOOTH_PRESSURE);
  BR_TEST_FLAG(BRUSH_PLANE_TRIM);
  BR_TEST_FLAG(BRUSH_FRONTFACE);

  BR_TEST_FLAG_OVERLAY(BRUSH_OVERLAY_CURSOR);
  BR_TEST_FLAG_OVERLAY(BRUSH_OVERLAY_PRIMARY);
  BR_TEST_FLAG_OVERLAY(BRUSH_OVERLAY_SECONDARY);
  BR_TEST_FLAG_OVERLAY(BRUSH_OVERLAY_CURSOR_OVERRIDE_ON_STROKE);
  BR_TEST_FLAG_OVERLAY(BRUSH_OVERLAY_PRIMARY_OVERRIDE_ON_STROKE);
  BR_TEST_FLAG_OVERLAY(BRUSH_OVERLAY_SECONDARY_OVERRIDE_ON_STROKE);

  BR_TEST(jitter, f);
  BR_TEST(spacing, d);
  BR_TEST(smooth_stroke_radius, d);
  BR_TEST(smooth_stroke_factor, f);
  BR_TEST(rate, f);

  BR_TEST(alpha, f);

  BR_TEST(sculpt_plane, d);

  BR_TEST(plane_offset, f);

  BR_TEST(autosmooth_factor, f);

  BR_TEST(topology_rake_factor, f);

  BR_TEST(crease_pinch_factor, f);

  BR_TEST(plane_trim, f);

  BR_TEST(texture_sample_bias, f);
  BR_TEST(texture_overlay_alpha, d);

  BR_TEST(add_col[0], f);
  BR_TEST(add_col[1], f);
  BR_TEST(add_col[2], f);
  BR_TEST(add_col[3], f);
  BR_TEST(sub_col[0], f);
  BR_TEST(sub_col[1], f);
  BR_TEST(sub_col[2], f);
  BR_TEST(sub_col[3], f);

  printf("\n");

#undef BR_TEST
#undef BR_TEST_FLAG
}

void BKE_brush_curve_preset(Brush *b, eCurveMappingPreset preset)
{
  CurveMapping *cumap = nullptr;
  CurveMap *cuma = nullptr;

  if (!b->curve_distance_falloff) {
    b->curve_distance_falloff = BKE_curvemapping_add(1, 0, 0, 1, 1);
  }
  cumap = b->curve_distance_falloff;
  cumap->flag &= ~CUMA_EXTEND_EXTRAPOLATE;
  cumap->preset = preset;

  cuma = b->curve_distance_falloff->cm;
  BKE_curvemap_reset(cuma, &cumap->clipr, cumap->preset, CurveMapSlopeType::Negative);
  BKE_curvemapping_changed(cumap, false);
  BKE_brush_tag_unsaved_changes(b);
}

const MTex *BKE_brush_mask_texture_get(const Brush *brush, const eObjectMode object_mode)
{
  if (object_mode == OB_MODE_SCULPT) {
    return &brush->mtex;
  }
  return &brush->mask_mtex;
}

const MTex *BKE_brush_color_texture_get(const Brush *brush, const eObjectMode object_mode)
{
  if (object_mode == OB_MODE_SCULPT) {
    return &brush->mask_mtex;
  }
  return &brush->mtex;
}

float BKE_brush_sample_tex_3d(const Paint *paint,
                              const Brush *br,
                              const MTex *mtex,
                              const float point[3],
                              float rgba[4],
                              const int thread,
                              ImagePool *pool)
{
  const blender::bke::PaintRuntime *paint_runtime = paint->runtime;
  float intensity = 1.0;
  bool hasrgb = false;

  if (mtex == nullptr || mtex->tex == nullptr) {
    intensity = 1;
  }
  else if (mtex->brush_map_mode == MTEX_MAP_MODE_3D) {
    /* Get strength by feeding the vertex
     * location directly into a texture */
    hasrgb = RE_texture_evaluate(mtex, point, thread, pool, false, false, &intensity, rgba);
  }
  else if (mtex->brush_map_mode == MTEX_MAP_MODE_STENCIL) {
    float rotation = -mtex->rot;
    const float point_2d[2] = {point[0], point[1]};
    float x, y;
    float co[3];

    x = point_2d[0] - br->stencil_pos[0];
    y = point_2d[1] - br->stencil_pos[1];

    if (rotation > 0.001f || rotation < -0.001f) {
      const float angle = atan2f(y, x) + rotation;
      const float flen = sqrtf(x * x + y * y);

      x = flen * cosf(angle);
      y = flen * sinf(angle);
    }

    if (fabsf(x) > br->stencil_dimension[0] || fabsf(y) > br->stencil_dimension[1]) {
      zero_v4(rgba);
      return 0.0f;
    }
    x /= (br->stencil_dimension[0]);
    y /= (br->stencil_dimension[1]);

    co[0] = x;
    co[1] = y;
    co[2] = 0.0f;

    hasrgb = RE_texture_evaluate(mtex, co, thread, pool, false, false, &intensity, rgba);
  }
  else {
    float rotation = -mtex->rot;
    const float point_2d[2] = {point[0], point[1]};
    float x = 0.0f, y = 0.0f; /* Quite warnings */
    float invradius = 1.0f;   /* Quite warnings */
    float co[3];

    if (mtex->brush_map_mode == MTEX_MAP_MODE_VIEW) {
      /* keep coordinates relative to mouse */

      rotation -= paint_runtime->brush_rotation;

      x = point_2d[0] - paint_runtime->tex_mouse[0];
      y = point_2d[1] - paint_runtime->tex_mouse[1];

      /* use pressure adjusted size for fixed mode */
      invradius = 1.0f / paint_runtime->pixel_radius;
    }
    else if (mtex->brush_map_mode == MTEX_MAP_MODE_TILED) {
      /* leave the coordinates relative to the screen */

      /* use unadjusted size for tiled mode */
      invradius = 1.0f / paint_runtime->start_pixel_radius;

      x = point_2d[0];
      y = point_2d[1];
    }
    else if (mtex->brush_map_mode == MTEX_MAP_MODE_RANDOM) {
      rotation -= paint_runtime->brush_rotation;
      /* these contain a random coordinate */
      x = point_2d[0] - paint_runtime->tex_mouse[0];
      y = point_2d[1] - paint_runtime->tex_mouse[1];

      invradius = 1.0f / paint_runtime->pixel_radius;
    }

    x *= invradius;
    y *= invradius;

    /* it is probably worth optimizing for those cases where
     * the texture is not rotated by skipping the calls to
     * atan2, sqrtf, sin, and cos. */
    if (rotation > 0.001f || rotation < -0.001f) {
      const float angle = atan2f(y, x) + rotation;
      const float flen = sqrtf(x * x + y * y);

      x = flen * cosf(angle);
      y = flen * sinf(angle);
    }

    co[0] = x;
    co[1] = y;
    co[2] = 0.0f;

    hasrgb = RE_texture_evaluate(mtex, co, thread, pool, false, false, &intensity, rgba);
  }

  intensity += br->texture_sample_bias;

  if (!hasrgb) {
    rgba[0] = intensity;
    rgba[1] = intensity;
    rgba[2] = intensity;
    rgba[3] = 1.0f;
  }
  /* For consistency, sampling always returns color in linear space */
  else if (paint_runtime->do_linear_conversion) {
    IMB_colormanagement_colorspace_to_scene_linear_v3(rgba, paint_runtime->colorspace);
  }

  return intensity;
}

float BKE_brush_sample_masktex(
    const Paint *paint, Brush *br, const float point[2], const int thread, ImagePool *pool)
{
  const blender::bke::PaintRuntime *paint_runtime = paint->runtime;
  MTex *mtex = &br->mask_mtex;
  float rgba[4], intensity;

  if (!mtex->tex) {
    return 1.0f;
  }
  if (mtex->brush_map_mode == MTEX_MAP_MODE_STENCIL) {
    float rotation = -mtex->rot;
    const float point_2d[2] = {point[0], point[1]};
    float x, y;
    float co[3];

    x = point_2d[0] - br->mask_stencil_pos[0];
    y = point_2d[1] - br->mask_stencil_pos[1];

    if (rotation > 0.001f || rotation < -0.001f) {
      const float angle = atan2f(y, x) + rotation;
      const float flen = sqrtf(x * x + y * y);

      x = flen * cosf(angle);
      y = flen * sinf(angle);
    }

    if (fabsf(x) > br->mask_stencil_dimension[0] || fabsf(y) > br->mask_stencil_dimension[1]) {
      zero_v4(rgba);
      return 0.0f;
    }
    x /= (br->mask_stencil_dimension[0]);
    y /= (br->mask_stencil_dimension[1]);

    co[0] = x;
    co[1] = y;
    co[2] = 0.0f;

    RE_texture_evaluate(mtex, co, thread, pool, false, false, &intensity, rgba);
  }
  else {
    float rotation = -mtex->rot;
    const float point_2d[2] = {point[0], point[1]};
    float x = 0.0f, y = 0.0f; /* Quite warnings */
    float invradius = 1.0f;   /* Quite warnings */
    float co[3];

    if (mtex->brush_map_mode == MTEX_MAP_MODE_VIEW) {
      /* keep coordinates relative to mouse */

      rotation -= paint_runtime->brush_rotation_sec;

      x = point_2d[0] - paint_runtime->mask_tex_mouse[0];
      y = point_2d[1] - paint_runtime->mask_tex_mouse[1];

      /* use pressure adjusted size for fixed mode */
      invradius = 1.0f / paint_runtime->pixel_radius;
    }
    else if (mtex->brush_map_mode == MTEX_MAP_MODE_TILED) {
      /* leave the coordinates relative to the screen */

      /* use unadjusted size for tiled mode */
      invradius = 1.0f / paint_runtime->start_pixel_radius;

      x = point_2d[0];
      y = point_2d[1];
    }
    else if (mtex->brush_map_mode == MTEX_MAP_MODE_RANDOM) {
      rotation -= paint_runtime->brush_rotation_sec;
      /* these contain a random coordinate */
      x = point_2d[0] - paint_runtime->mask_tex_mouse[0];
      y = point_2d[1] - paint_runtime->mask_tex_mouse[1];

      invradius = 1.0f / paint_runtime->pixel_radius;
    }

    x *= invradius;
    y *= invradius;

    /* it is probably worth optimizing for those cases where
     * the texture is not rotated by skipping the calls to
     * atan2, sqrtf, sin, and cos. */
    if (rotation > 0.001f || rotation < -0.001f) {
      const float angle = atan2f(y, x) + rotation;
      const float flen = sqrtf(x * x + y * y);

      x = flen * cosf(angle);
      y = flen * sinf(angle);
    }

    co[0] = x;
    co[1] = y;
    co[2] = 0.0f;

    RE_texture_evaluate(mtex, co, thread, pool, false, false, &intensity, rgba);
  }

  CLAMP(intensity, 0.0f, 1.0f);

  switch (br->mask_pressure) {
    case BRUSH_MASK_PRESSURE_CUTOFF:
      intensity = ((1.0f - intensity) < paint_runtime->size_pressure_value) ? 1.0f : 0.0f;
      break;
    case BRUSH_MASK_PRESSURE_RAMP:
      intensity = paint_runtime->size_pressure_value +
                  intensity * (1.0f - paint_runtime->size_pressure_value);
      break;
    default:
      break;
  }

  return intensity;
}

/* -------------------------------------------------------------------- */
/** \name Unified Settings
 * \{ */

const float *BKE_brush_color_get(const Paint *paint, const Brush *brush)
{
  if (BKE_paint_use_unified_color(paint)) {
    return paint->unified_paint_settings.color;
  }
  return brush->color;
}

/** Get color jitter settings if enabled. */
std::optional<BrushColorJitterSettings> BKE_brush_color_jitter_get_settings(const Paint *paint,
                                                                            const Brush *brush)
{
  if (BKE_paint_use_unified_color(paint)) {
    if ((paint->unified_paint_settings.flag & UNIFIED_PAINT_COLOR_JITTER) == 0) {
      return std::nullopt;
    }

    const UnifiedPaintSettings &settings = paint->unified_paint_settings;
    return BrushColorJitterSettings{
        settings.color_jitter_flag,
        settings.hsv_jitter[0],
        settings.hsv_jitter[1],
        settings.hsv_jitter[2],
        settings.curve_rand_hue,
        settings.curve_rand_saturation,
        settings.curve_rand_value,
    };
  }

  if ((brush->flag2 & BRUSH_JITTER_COLOR) == 0) {
    return std::nullopt;
  }

  return BrushColorJitterSettings{
      brush->color_jitter_flag,
      brush->hsv_jitter[0],
      brush->hsv_jitter[1],
      brush->hsv_jitter[2],
      brush->curve_rand_hue,
      brush->curve_rand_saturation,
      brush->curve_rand_value,
  };
}

const float *BKE_brush_secondary_color_get(const Paint *paint, const Brush *brush)
{
  if (BKE_paint_use_unified_color(paint)) {
    return paint->unified_paint_settings.secondary_color;
  }
  return brush->secondary_color;
}

void BKE_brush_color_set(Paint *paint, Brush *brush, const float color[3])
{
  if (BKE_paint_use_unified_color(paint)) {
    UnifiedPaintSettings *ups = &paint->unified_paint_settings;
    copy_v3_v3(ups->color, color);
    BKE_brush_color_sync_legacy(ups);
  }
  else {
    copy_v3_v3(brush->color, color);
    BKE_brush_tag_unsaved_changes(brush);
    BKE_brush_color_sync_legacy(brush);
  }
}

void BKE_brush_color_sync_legacy(Brush *brush)
{
  /* For forward compatibility. */
  linearrgb_to_srgb_v3_v3(brush->rgb, brush->color);
  linearrgb_to_srgb_v3_v3(brush->secondary_rgb, brush->secondary_color);
}

void BKE_brush_color_sync_legacy(UnifiedPaintSettings *ups)
{
  /* For forward compatibility. */
  linearrgb_to_srgb_v3_v3(ups->rgb, ups->color);
  linearrgb_to_srgb_v3_v3(ups->secondary_rgb, ups->secondary_color);
}

/* Be careful about setting size and unprojected size because they depend on one another these
 * functions do not set the other corresponding value this can lead to odd behavior if size and
 * unprojected radius become inconsistent. The biggest problem is that it isn't possible to change
 * unprojected radius because a view context is not available. My usual solution to this is to use
 * the ratio of change of the size to change the unprojected radius. Not completely convinced that
 * is correct. In any case, a better solution is needed to prevent inconsistency. */

void BKE_brush_size_set(Paint *paint, Brush *brush, int size)
{
  UnifiedPaintSettings *ups = &paint->unified_paint_settings;

  /* make sure range is sane */
  CLAMP(size, 1, MAX_BRUSH_PIXEL_DIAMETER);

  if (ups->flag & UNIFIED_PAINT_SIZE) {
    ups->size = size;
  }
  else {
    brush->size = size;
    BKE_brush_tag_unsaved_changes(brush);
  }
}

int BKE_brush_size_get(const Paint *paint, const Brush *brush)
{
  const UnifiedPaintSettings *ups = &paint->unified_paint_settings;
  int size = (ups->flag & UNIFIED_PAINT_SIZE) ? ups->size : brush->size;

  return size;
}

float BKE_brush_radius_get(const Paint *paint, const Brush *brush)
{
  return BKE_brush_size_get(paint, brush) / 2.0f;
}

bool BKE_brush_use_locked_size(const Paint *paint, const Brush *brush)
{
  const short us_flag = paint->unified_paint_settings.flag;

  return (us_flag & UNIFIED_PAINT_SIZE) ? (us_flag & UNIFIED_PAINT_BRUSH_LOCK_SIZE) :
                                          (brush->flag & BRUSH_LOCK_SIZE);
}

bool BKE_brush_use_size_pressure(const Brush *brush)
{
  return brush->flag & BRUSH_SIZE_PRESSURE;
}

bool BKE_brush_use_alpha_pressure(const Brush *brush)
{
  return brush->flag & BRUSH_ALPHA_PRESSURE;
}

void BKE_brush_unprojected_size_set(Paint *paint, Brush *brush, float unprojected_size)
{
  UnifiedPaintSettings *ups = &paint->unified_paint_settings;

  if (ups->flag & UNIFIED_PAINT_SIZE) {
    ups->unprojected_size = unprojected_size;
  }
  else {
    brush->unprojected_size = unprojected_size;
    BKE_brush_tag_unsaved_changes(brush);
  }
}

float BKE_brush_unprojected_size_get(const Paint *paint, const Brush *brush)
{
  const UnifiedPaintSettings *ups = &paint->unified_paint_settings;

  return (ups->flag & UNIFIED_PAINT_SIZE) ? ups->unprojected_size : brush->unprojected_size;
}

float BKE_brush_unprojected_radius_get(const Paint *paint, const Brush *brush)
{
  return BKE_brush_unprojected_size_get(paint, brush) / 2.0f;
}

void BKE_brush_scale_unprojected_size(float *unprojected_size,
                                      int new_brush_size,
                                      int old_brush_size)
{
  float scale = new_brush_size;
  /* avoid division by zero */
  if (old_brush_size != 0) {
    scale /= float(old_brush_size);
  }
  (*unprojected_size) *= scale;
}

void BKE_brush_scale_size(int *r_brush_size,
                          float new_unprojected_size,
                          float old_unprojected_size)
{
  float scale = new_unprojected_size;
  /* avoid division by zero */
  if (old_unprojected_size != 0) {
    scale /= new_unprojected_size;
  }
  (*r_brush_size) = int(float(*r_brush_size) * scale);
}

void BKE_brush_alpha_set(Paint *paint, Brush *brush, float alpha)
{
  UnifiedPaintSettings *ups = &paint->unified_paint_settings;

  if (ups->flag & UNIFIED_PAINT_ALPHA) {
    ups->alpha = alpha;
  }
  else {
    brush->alpha = alpha;
    BKE_brush_tag_unsaved_changes(brush);
  }
}

float BKE_brush_alpha_get(const Paint *paint, const Brush *brush)
{
  const UnifiedPaintSettings *ups = &paint->unified_paint_settings;

  return (ups->flag & UNIFIED_PAINT_ALPHA) ? ups->alpha : brush->alpha;
}

float BKE_brush_weight_get(const Paint *paint, const Brush *brush)
{
  const UnifiedPaintSettings *ups = &paint->unified_paint_settings;

  return (ups->flag & UNIFIED_PAINT_WEIGHT) ? ups->weight : brush->weight;
}

void BKE_brush_weight_set(Paint *paint, Brush *brush, float value)
{
  UnifiedPaintSettings *ups = &paint->unified_paint_settings;

  if (ups->flag & UNIFIED_PAINT_WEIGHT) {
    ups->weight = value;
  }
  else {
    brush->weight = value;
    BKE_brush_tag_unsaved_changes(brush);
  }
}

int BKE_brush_input_samples_get(const Paint *paint, const Brush *brush)
{
  const UnifiedPaintSettings *ups = &paint->unified_paint_settings;

  return (ups->flag & UNIFIED_PAINT_INPUT_SAMPLES) ? ups->input_samples : brush->input_samples;
}

void BKE_brush_input_samples_set(Paint *paint, Brush *brush, int value)
{
  UnifiedPaintSettings *ups = &paint->unified_paint_settings;

  if (ups->flag & UNIFIED_PAINT_INPUT_SAMPLES) {
    ups->input_samples = value;
  }
  else {
    brush->input_samples = value;
    BKE_brush_tag_unsaved_changes(brush);
  }
}

/** \} */

void BKE_brush_jitter_pos(const Paint &paint,
                          const Brush &brush,
                          const float pos[2],
                          float jitterpos[2])
{
  float rand_pos[2];
  float spread;
  int diameter;

  do {
    rand_pos[0] = BLI_rng_get_float(brush_rng) - 0.5f;
    rand_pos[1] = BLI_rng_get_float(brush_rng) - 0.5f;
  } while (len_squared_v2(rand_pos) > square_f(0.5f));

  if (brush.flag & BRUSH_ABSOLUTE_JITTER) {
    diameter = 2 * brush.jitter_absolute;
    spread = 1.0;
  }
  else {
    diameter = 2 * BKE_brush_radius_get(&paint, &brush);
    spread = brush.jitter;
  }
  /* find random position within a circle of diameter 1 */
  jitterpos[0] = pos[0] + 2 * rand_pos[0] * diameter * spread;
  jitterpos[1] = pos[1] + 2 * rand_pos[1] * diameter * spread;
}

void BKE_brush_randomize_texture_coords(Paint *paint, bool mask)
{
  blender::bke::PaintRuntime &paint_runtime = *paint->runtime;
  /* we multiply with brush radius as an optimization for the brush
   * texture sampling functions */
  if (mask) {
    paint_runtime.mask_tex_mouse[0] = BLI_rng_get_float(brush_rng) * paint_runtime.pixel_radius;
    paint_runtime.mask_tex_mouse[1] = BLI_rng_get_float(brush_rng) * paint_runtime.pixel_radius;
  }
  else {
    paint_runtime.tex_mouse[0] = BLI_rng_get_float(brush_rng) * paint_runtime.pixel_radius;
    paint_runtime.tex_mouse[1] = BLI_rng_get_float(brush_rng) * paint_runtime.pixel_radius;
  }
}

void BKE_brush_calc_curve_factors(const eBrushCurvePreset preset,
                                  const CurveMapping *cumap,
                                  const blender::Span<float> distances,
                                  const float brush_radius,
                                  const blender::MutableSpan<float> factors)
{
  BLI_assert(factors.size() == distances.size());

  const float radius_rcp = blender::math::rcp(brush_radius);
  switch (preset) {
    case BRUSH_CURVE_CUSTOM: {
      for (const int i : distances.index_range()) {
        const float distance = distances[i];
        if (distance >= brush_radius) {
          factors[i] = 0.0f;
          continue;
        }
        factors[i] *= BKE_curvemapping_evaluateF(cumap, 0, distance * radius_rcp);
      }
      break;
    }
    case BRUSH_CURVE_SHARP: {
      for (const int i : distances.index_range()) {
        const float distance = distances[i];
        if (distance >= brush_radius) {
          factors[i] = 0.0f;
          continue;
        }
        const float factor = 1.0f - distance * radius_rcp;
        factors[i] *= factor * factor;
      }
      break;
    }
    case BRUSH_CURVE_SMOOTH: {
      for (const int i : distances.index_range()) {
        const float distance = distances[i];
        if (distance >= brush_radius) {
          factors[i] = 0.0f;
          continue;
        }
        const float factor = 1.0f - distance * radius_rcp;
        factors[i] *= 3.0f * factor * factor - 2.0f * factor * factor * factor;
      }
      break;
    }
    case BRUSH_CURVE_SMOOTHER: {
      for (const int i : distances.index_range()) {
        const float distance = distances[i];
        if (distance >= brush_radius) {
          factors[i] = 0.0f;
          continue;
        }
        const float factor = 1.0f - distance * radius_rcp;
        factors[i] *= pow3f(factor) * (factor * (factor * 6.0f - 15.0f) + 10.0f);
      }
      break;
    }
    case BRUSH_CURVE_ROOT: {
      for (const int i : distances.index_range()) {
        const float distance = distances[i];
        if (distance >= brush_radius) {
          factors[i] = 0.0f;
          continue;
        }
        const float factor = 1.0f - distance * radius_rcp;
        factors[i] *= sqrtf(factor);
      }
      break;
    }
    case BRUSH_CURVE_LIN: {
      for (const int i : distances.index_range()) {
        const float distance = distances[i];
        if (distance >= brush_radius) {
          factors[i] = 0.0f;
          continue;
        }
        const float factor = 1.0f - distance * radius_rcp;
        factors[i] *= factor;
      }
      break;
    }
    case BRUSH_CURVE_CONSTANT: {
      for (const int i : distances.index_range()) {
        const float distance = distances[i];
        if (distance >= brush_radius) {
          factors[i] = 0.0f;
        }
      }
      break;
    }
    case BRUSH_CURVE_SPHERE: {
      for (const int i : distances.index_range()) {
        const float distance = distances[i];
        if (distance >= brush_radius) {
          factors[i] = 0.0f;
          continue;
        }
        const float factor = 1.0f - distance * radius_rcp;
        factors[i] *= sqrtf(2 * factor - factor * factor);
      }
      break;
    }
    case BRUSH_CURVE_POW4: {
      for (const int i : distances.index_range()) {
        const float distance = distances[i];
        if (distance >= brush_radius) {
          factors[i] = 0.0f;
          continue;
        }
        const float factor = 1.0f - distance * radius_rcp;
        factors[i] *= factor * factor * factor * factor;
      }
      break;
    }
    case BRUSH_CURVE_INVSQUARE: {
      for (const int i : distances.index_range()) {
        const float distance = distances[i];
        if (distance >= brush_radius) {
          factors[i] = 0.0f;
          continue;
        }
        const float factor = 1.0f - distance * radius_rcp;
        factors[i] *= factor * (2.0f - factor);
      }
      break;
    }
  }
}

float BKE_brush_curve_strength(const eBrushCurvePreset preset,
                               const CurveMapping *cumap,
                               const float distance,
                               const float brush_radius)
{
  BLI_assert(distance >= 0.0f);
  BLI_assert(brush_radius >= 0.0f);

  float p = distance;
  float strength = 1.0f;

  if (p >= brush_radius) {
    return 0;
  }

  p = p / brush_radius;
  p = 1.0f - p;

  switch (preset) {
    case BRUSH_CURVE_CUSTOM:
      strength = BKE_curvemapping_evaluateF(cumap, 0, 1.0f - p);
      break;
    case BRUSH_CURVE_SHARP:
      strength = p * p;
      break;
    case BRUSH_CURVE_SMOOTH:
      strength = 3.0f * p * p - 2.0f * p * p * p;
      break;
    case BRUSH_CURVE_SMOOTHER:
      strength = pow3f(p) * (p * (p * 6.0f - 15.0f) + 10.0f);
      break;
    case BRUSH_CURVE_ROOT:
      strength = sqrtf(p);
      break;
    case BRUSH_CURVE_LIN:
      strength = p;
      break;
    case BRUSH_CURVE_CONSTANT:
      strength = 1.0f;
      break;
    case BRUSH_CURVE_SPHERE:
      strength = sqrtf(2 * p - p * p);
      break;
    case BRUSH_CURVE_POW4:
      strength = p * p * p * p;
      break;
    case BRUSH_CURVE_INVSQUARE:
      strength = p * (2.0f - p);
      break;
  }

  return strength;
}

float BKE_brush_curve_strength(const Brush *br, float p, const float len)
{
  return BKE_brush_curve_strength(
      eBrushCurvePreset(br->curve_distance_falloff_preset), br->curve_distance_falloff, p, len);
}

float BKE_brush_curve_strength_clamped(const Brush *br, float p, const float len)
{
  float strength = BKE_brush_curve_strength(br, p, len);

  CLAMP(strength, 0.0f, 1.0f);

  return strength;
}

/* TODO: should probably be unified with BrushPainter stuff? */
static bool brush_gen_texture(const Brush *br,
                              const int side,
                              const bool use_secondary,
                              float *rect)
{
  const MTex *mtex = (use_secondary) ? &br->mask_mtex : &br->mtex;
  if (mtex->tex == nullptr) {
    return false;
  }

  const float step = 2.0 / side;
  int ix, iy;
  float x, y;

  /* Do normalized canonical view coords for texture. */
  for (y = -1.0, iy = 0; iy < side; iy++, y += step) {
    for (x = -1.0, ix = 0; ix < side; ix++, x += step) {
      const float co[3] = {x, y, 0.0f};

      float intensity;
      float rgba_dummy[4];
      RE_texture_evaluate(mtex, co, 0, nullptr, false, false, &intensity, rgba_dummy);

      rect[iy * side + ix] = intensity;
    }
  }

  return true;
}

ImBuf *BKE_brush_gen_radial_control_imbuf(Brush *br, bool secondary, bool display_gradient)
{
  ImBuf *im = MEM_callocN<ImBuf>("radial control texture");
  int side = 512;
  int half = side / 2;

  BKE_curvemapping_init(br->curve_distance_falloff);

  float *rect_float = MEM_calloc_arrayN<float>(size_t(side) * size_t(side), "radial control rect");
  IMB_assign_float_buffer(im, rect_float, IB_DO_NOT_TAKE_OWNERSHIP);

  im->x = im->y = side;

  const bool have_texture = brush_gen_texture(br, side, secondary, im->float_buffer.data);

  if (display_gradient || have_texture) {
    for (int i = 0; i < side; i++) {
      for (int j = 0; j < side; j++) {
        const float magn = sqrtf(pow2f(i - half) + pow2f(j - half));
        const float strength = BKE_brush_curve_strength_clamped(br, magn, half);
        im->float_buffer.data[i * side + j] = (have_texture) ?
                                                  im->float_buffer.data[i * side + j] * strength :
                                                  strength;
      }
    }
  }

  return im;
}

bool BKE_brush_has_cube_tip(const Brush *brush, PaintMode paint_mode)
{
  switch (paint_mode) {
    case PaintMode::Sculpt: {
      if (brush->sculpt_brush_type == SCULPT_BRUSH_TYPE_MULTIPLANE_SCRAPE) {
        return true;
      }

      if (ELEM(brush->sculpt_brush_type, SCULPT_BRUSH_TYPE_CLAY_STRIPS, SCULPT_BRUSH_TYPE_PAINT) &&
          (brush->tip_roundness < 1.0f || brush->tip_scale_x != 1.0f))
      {
        return true;
      }

      break;
    }
    default: {
      break;
    }
  }

  return false;
}

/* -------------------------------------------------------------------- */
/** \name Brush Capabilities
 * \{ */

namespace blender::bke::brush {
bool supports_dyntopo(const Brush &brush)
{
  return !ELEM(brush.sculpt_brush_type,
               /* These brushes, as currently coded, cannot support dynamic topology */
               SCULPT_BRUSH_TYPE_GRAB,
               SCULPT_BRUSH_TYPE_ROTATE,
               SCULPT_BRUSH_TYPE_CLOTH,
               SCULPT_BRUSH_TYPE_THUMB,
               SCULPT_BRUSH_TYPE_LAYER,
               SCULPT_BRUSH_TYPE_DISPLACEMENT_ERASER,
               SCULPT_BRUSH_TYPE_DRAW_SHARP,
               SCULPT_BRUSH_TYPE_SLIDE_RELAX,
               SCULPT_BRUSH_TYPE_ELASTIC_DEFORM,
               SCULPT_BRUSH_TYPE_BOUNDARY,
               SCULPT_BRUSH_TYPE_POSE,
               SCULPT_BRUSH_TYPE_DRAW_FACE_SETS,
               SCULPT_BRUSH_TYPE_PAINT,
               SCULPT_BRUSH_TYPE_SMEAR,

               /* These brushes could handle dynamic topology,
                * but user feedback indicates it's better not to */
               SCULPT_BRUSH_TYPE_SMOOTH,
               SCULPT_BRUSH_TYPE_MASK);
}
bool supports_accumulate(const Brush &brush)
{
  return ELEM(brush.sculpt_brush_type,
              SCULPT_BRUSH_TYPE_DRAW,
              SCULPT_BRUSH_TYPE_DRAW_SHARP,
              SCULPT_BRUSH_TYPE_SLIDE_RELAX,
              SCULPT_BRUSH_TYPE_CREASE,
              SCULPT_BRUSH_TYPE_BLOB,
              SCULPT_BRUSH_TYPE_INFLATE,
              SCULPT_BRUSH_TYPE_CLAY,
              SCULPT_BRUSH_TYPE_CLAY_STRIPS,
              SCULPT_BRUSH_TYPE_CLAY_THUMB,
              SCULPT_BRUSH_TYPE_ROTATE,
              SCULPT_BRUSH_TYPE_PLANE);
}
bool supports_topology_rake(const Brush &brush)
{
  return !ELEM(brush.sculpt_brush_type,
               SCULPT_BRUSH_TYPE_GRAB,
               SCULPT_BRUSH_TYPE_ROTATE,
               SCULPT_BRUSH_TYPE_THUMB,
               SCULPT_BRUSH_TYPE_DRAW_SHARP,
               SCULPT_BRUSH_TYPE_DISPLACEMENT_ERASER,
               SCULPT_BRUSH_TYPE_SLIDE_RELAX,
               SCULPT_BRUSH_TYPE_MASK);
}
bool supports_auto_smooth(const Brush &brush)
{
  /* TODO: Should this support Face Sets...? */
  return !ELEM(brush.sculpt_brush_type,
               SCULPT_BRUSH_TYPE_MASK,
               SCULPT_BRUSH_TYPE_SMOOTH,
               SCULPT_BRUSH_TYPE_PAINT,
               SCULPT_BRUSH_TYPE_SMEAR);
}
bool supports_height(const Brush &brush)
{
  return brush.sculpt_brush_type == SCULPT_BRUSH_TYPE_LAYER;
}
bool supports_plane_height(const Brush &brush)
{
  return ELEM(brush.sculpt_brush_type, SCULPT_BRUSH_TYPE_PLANE);
}
bool supports_plane_depth(const Brush &brush)
{
  return ELEM(brush.sculpt_brush_type, SCULPT_BRUSH_TYPE_PLANE);
}
bool supports_jitter(const Brush &brush)
{
  return !(brush.flag & BRUSH_ANCHORED) && !(brush.flag & BRUSH_DRAG_DOT) &&
         !ELEM(brush.sculpt_brush_type,
               SCULPT_BRUSH_TYPE_GRAB,
               SCULPT_BRUSH_TYPE_ROTATE,
               SCULPT_BRUSH_TYPE_SNAKE_HOOK,
               SCULPT_BRUSH_TYPE_THUMB);
}
bool supports_normal_weight(const Brush &brush)
{
  return ELEM(brush.sculpt_brush_type,
              SCULPT_BRUSH_TYPE_GRAB,
              SCULPT_BRUSH_TYPE_SNAKE_HOOK,
              SCULPT_BRUSH_TYPE_ELASTIC_DEFORM);
}
bool supports_rake_factor(const Brush &brush)
{
  return ELEM(brush.sculpt_brush_type, SCULPT_BRUSH_TYPE_SNAKE_HOOK);
}
bool supports_persistence(const Brush &brush)
{
  return ELEM(brush.sculpt_brush_type, SCULPT_BRUSH_TYPE_LAYER, SCULPT_BRUSH_TYPE_CLOTH);
}
bool supports_pinch_factor(const Brush &brush)
{
  return ELEM(brush.sculpt_brush_type,
              SCULPT_BRUSH_TYPE_BLOB,
              SCULPT_BRUSH_TYPE_CREASE,
              SCULPT_BRUSH_TYPE_SNAKE_HOOK);
}
bool supports_plane_offset(const Brush &brush)
{
  return ELEM(brush.sculpt_brush_type,
              SCULPT_BRUSH_TYPE_CLAY,
              SCULPT_BRUSH_TYPE_CLAY_STRIPS,
              SCULPT_BRUSH_TYPE_CLAY_THUMB,
              SCULPT_BRUSH_TYPE_PLANE);
}
bool supports_random_texture_angle(const Brush &brush)
{
  return !ELEM(brush.sculpt_brush_type,
               SCULPT_BRUSH_TYPE_GRAB,
               SCULPT_BRUSH_TYPE_ROTATE,
               SCULPT_BRUSH_TYPE_SNAKE_HOOK,
               SCULPT_BRUSH_TYPE_THUMB);
}
bool supports_sculpt_plane(const Brush &brush)
{
  /* TODO: Should the face set brush be here...? */
  return !ELEM(brush.sculpt_brush_type,
               SCULPT_BRUSH_TYPE_INFLATE,
               SCULPT_BRUSH_TYPE_MASK,
               SCULPT_BRUSH_TYPE_PINCH,
               SCULPT_BRUSH_TYPE_SMOOTH);
}
bool supports_color(const Brush &brush)
{
  return ELEM(brush.sculpt_brush_type, SCULPT_BRUSH_TYPE_PAINT);
}
bool supports_secondary_cursor_color(const Brush &brush)
{
  return ELEM(brush.sculpt_brush_type,
              SCULPT_BRUSH_TYPE_BLOB,
              SCULPT_BRUSH_TYPE_DRAW,
              SCULPT_BRUSH_TYPE_DRAW_SHARP,
              SCULPT_BRUSH_TYPE_INFLATE,
              SCULPT_BRUSH_TYPE_CLAY,
              SCULPT_BRUSH_TYPE_CLAY_STRIPS,
              SCULPT_BRUSH_TYPE_CLAY_THUMB,
              SCULPT_BRUSH_TYPE_PINCH,
              SCULPT_BRUSH_TYPE_CREASE,
              SCULPT_BRUSH_TYPE_LAYER,
              SCULPT_BRUSH_TYPE_MASK);
}
bool supports_smooth_stroke(const Brush &brush)
{
  return !(brush.flag & BRUSH_ANCHORED) && !(brush.flag & BRUSH_DRAG_DOT) &&
         !(brush.flag & BRUSH_LINE) && !(brush.flag & BRUSH_CURVE) &&
         !ELEM(brush.sculpt_brush_type,
               SCULPT_BRUSH_TYPE_GRAB,
               SCULPT_BRUSH_TYPE_ROTATE,
               SCULPT_BRUSH_TYPE_SNAKE_HOOK,
               SCULPT_BRUSH_TYPE_THUMB);
}
bool supports_space_attenuation(const Brush &brush)
{
  return brush.flag & (BRUSH_SPACE | BRUSH_LINE | BRUSH_CURVE) &&
         !ELEM(brush.sculpt_brush_type,
               SCULPT_BRUSH_TYPE_GRAB,
               SCULPT_BRUSH_TYPE_ROTATE,
               SCULPT_BRUSH_TYPE_SMOOTH,
               SCULPT_BRUSH_TYPE_SNAKE_HOOK);
}

/**
 * A helper method for classifying a certain subset of brush types.
 *
 * Certain sculpt deformations are 'grab-like' in that they behave as if they have an anchored
 * start point.
 */
static bool is_grab_tool(const Brush &brush)
{
  return (brush.sculpt_brush_type == SCULPT_BRUSH_TYPE_CLOTH &&
          brush.cloth_deform_type == BRUSH_CLOTH_DEFORM_GRAB) ||
         ELEM(brush.sculpt_brush_type,
              SCULPT_BRUSH_TYPE_GRAB,
              SCULPT_BRUSH_TYPE_SNAKE_HOOK,
              SCULPT_BRUSH_TYPE_ELASTIC_DEFORM,
              SCULPT_BRUSH_TYPE_POSE,
              SCULPT_BRUSH_TYPE_BOUNDARY,
              SCULPT_BRUSH_TYPE_THUMB,
              SCULPT_BRUSH_TYPE_ROTATE);
}
bool supports_strength_pressure(const Brush &brush)
{
  return !is_grab_tool(brush);
}
bool supports_size_pressure(const Brush &brush)
{
  return !is_grab_tool(brush);
}
bool supports_auto_smooth_pressure(const Brush &brush)
{
  return !is_grab_tool(brush);
}
bool supports_hardness_pressure(const Brush &brush)
{
  return !is_grab_tool(brush);
}
bool supports_inverted_direction(const Brush &brush)
{
  return ELEM(brush.sculpt_brush_type,
              SCULPT_BRUSH_TYPE_DRAW,
              SCULPT_BRUSH_TYPE_DRAW_SHARP,
              SCULPT_BRUSH_TYPE_CLAY,
              SCULPT_BRUSH_TYPE_CLAY_STRIPS,
              SCULPT_BRUSH_TYPE_SMOOTH,
              SCULPT_BRUSH_TYPE_LAYER,
              SCULPT_BRUSH_TYPE_INFLATE,
              SCULPT_BRUSH_TYPE_BLOB,
              SCULPT_BRUSH_TYPE_CREASE,
              SCULPT_BRUSH_TYPE_PLANE,
              SCULPT_BRUSH_TYPE_CLAY,
              SCULPT_BRUSH_TYPE_PINCH,
              SCULPT_BRUSH_TYPE_MASK);
}
bool supports_gravity(const Brush &brush)
{
  return !ELEM(brush.sculpt_brush_type,
               SCULPT_BRUSH_TYPE_PAINT,
               SCULPT_BRUSH_TYPE_SMEAR,
               SCULPT_BRUSH_TYPE_MASK,
               SCULPT_BRUSH_TYPE_DRAW_FACE_SETS,
               SCULPT_BRUSH_TYPE_BOUNDARY,
               SCULPT_BRUSH_TYPE_SMOOTH,
               SCULPT_BRUSH_TYPE_SIMPLIFY,
               SCULPT_BRUSH_TYPE_DISPLACEMENT_SMEAR,
               SCULPT_BRUSH_TYPE_DISPLACEMENT_ERASER);
}
bool supports_tilt(const Brush &brush)
{
  return ELEM(brush.sculpt_brush_type,
              SCULPT_BRUSH_TYPE_DRAW,
              SCULPT_BRUSH_TYPE_DRAW_SHARP,
              SCULPT_BRUSH_TYPE_PLANE,
              SCULPT_BRUSH_TYPE_CLAY_STRIPS);
}
}  // namespace blender::bke::brush

/** \} */
