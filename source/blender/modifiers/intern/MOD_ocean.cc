/* SPDX-FileCopyrightText: Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

#include "BLI_utildefines.h"

#include "BLI_task.h"

#include "BLT_translation.h"

#include "DNA_customdata_types.h"
#include "DNA_defaults.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"

#include "BKE_context.h"
#include "BKE_lib_id.h"
#include "BKE_mesh.hh"
#include "BKE_modifier.h"
#include "BKE_ocean.h"
#include "BKE_screen.h"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.h"

#include "BLO_read_write.hh"

#include "WM_types.hh" /* For UI free bake operator. */

#include "DEG_depsgraph_query.h"

#include "MOD_modifiertypes.hh"
#include "MOD_ui_common.hh"

#ifdef WITH_OCEANSIM
static void init_cache_data(Object *ob, OceanModifierData *omd, const int resolution)
{
  const char *relbase = BKE_modifier_path_relbase_from_global(ob);

  omd->oceancache = BKE_ocean_init_cache(omd->cachepath,
                                         relbase,
                                         omd->bakestart,
                                         omd->bakeend,
                                         omd->wave_scale,
                                         omd->chop_amount,
                                         omd->foam_coverage,
                                         omd->foam_fade,
                                         resolution);
}

static void simulate_ocean_modifier(OceanModifierData *omd)
{
  BKE_ocean_simulate(omd->ocean, omd->time, omd->wave_scale, omd->chop_amount);
}
#endif /* WITH_OCEANSIM */

/* Modifier Code */

static void init_data(ModifierData *md)
{
#ifdef WITH_OCEANSIM
  OceanModifierData *omd = (OceanModifierData *)md;

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(omd, modifier));

  MEMCPY_STRUCT_AFTER(omd, DNA_struct_default_get(OceanModifierData), modifier);

  BKE_modifier_path_init(omd->cachepath, sizeof(omd->cachepath), "cache_ocean");

  omd->ocean = BKE_ocean_add();
  if (BKE_ocean_init_from_modifier(omd->ocean, omd, omd->viewport_resolution)) {
    simulate_ocean_modifier(omd);
  }
#else  /* WITH_OCEANSIM */
  UNUSED_VARS(md);
#endif /* WITH_OCEANSIM */
}

static void free_data(ModifierData *md)
{
#ifdef WITH_OCEANSIM
  OceanModifierData *omd = (OceanModifierData *)md;

  BKE_ocean_free(omd->ocean);
  if (omd->oceancache) {
    BKE_ocean_free_cache(omd->oceancache);
  }
#else  /* WITH_OCEANSIM */
  /* unused */
  (void)md;
#endif /* WITH_OCEANSIM */
}

static void copy_data(const ModifierData *md, ModifierData *target, const int flag)
{
#ifdef WITH_OCEANSIM
#  if 0
  const OceanModifierData *omd = (const OceanModifierData *)md;
#  endif
  OceanModifierData *tomd = (OceanModifierData *)target;

  BKE_modifier_copydata_generic(md, target, flag);

  /* The oceancache object will be recreated for this copy
   * automatically when cached=true */
  tomd->oceancache = nullptr;

  tomd->ocean = BKE_ocean_add();
  if (BKE_ocean_init_from_modifier(tomd->ocean, tomd, tomd->viewport_resolution)) {
    simulate_ocean_modifier(tomd);
  }
#else  /* WITH_OCEANSIM */
  /* unused */
  (void)md;
  (void)target;
  (void)flag;
#endif /* WITH_OCEANSIM */
}

#ifdef WITH_OCEANSIM
static void required_data_mask(ModifierData *md, CustomData_MeshMasks *r_cddata_masks)
{
  OceanModifierData *omd = (OceanModifierData *)md;

  if (omd->flag & MOD_OCEAN_GENERATE_FOAM) {
    r_cddata_masks->fmask |= CD_MASK_MCOL; /* XXX Should be loop cddata I guess? */
  }
}
#else  /* WITH_OCEANSIM */
static void required_data_mask(ModifierData * /*md*/, CustomData_MeshMasks * /*r_cddata_masks*/) {}
#endif /* WITH_OCEANSIM */

static bool depends_on_normals(ModifierData *md)
{
  OceanModifierData *omd = (OceanModifierData *)md;
  return (omd->geometry_mode != MOD_OCEAN_GEOM_GENERATE);
}

#ifdef WITH_OCEANSIM

struct GenerateOceanGeometryData {
  blender::MutableSpan<blender::float3> vert_positions;
  blender::MutableSpan<int> face_offsets;
  blender::MutableSpan<int> corner_verts;
  float (*mloopuvs)[2];

  int res_x, res_y;
  int rx, ry;
  float ox, oy;
  float sx, sy;
  float ix, iy;
};

static void generate_ocean_geometry_verts(void *__restrict userdata,
                                          const int y,
                                          const TaskParallelTLS *__restrict /*tls*/)
{
  GenerateOceanGeometryData *gogd = static_cast<GenerateOceanGeometryData *>(userdata);
  int x;

  for (x = 0; x <= gogd->res_x; x++) {
    const int i = y * (gogd->res_x + 1) + x;
    float *co = gogd->vert_positions[i];
    co[0] = gogd->ox + (x * gogd->sx);
    co[1] = gogd->oy + (y * gogd->sy);
    co[2] = 0.0f;
  }
}

static void generate_ocean_geometry_faces(void *__restrict userdata,
                                          const int y,
                                          const TaskParallelTLS *__restrict /*tls*/)
{
  GenerateOceanGeometryData *gogd = static_cast<GenerateOceanGeometryData *>(userdata);
  int x;

  for (x = 0; x < gogd->res_x; x++) {
    const int fi = y * gogd->res_x + x;
    const int vi = y * (gogd->res_x + 1) + x;

    gogd->corner_verts[fi * 4 + 0] = vi;
    gogd->corner_verts[fi * 4 + 1] = vi + 1;
    gogd->corner_verts[fi * 4 + 2] = vi + 1 + gogd->res_x + 1;
    gogd->corner_verts[fi * 4 + 3] = vi + gogd->res_x + 1;

    gogd->face_offsets[fi] = fi * 4;
  }
}

static void generate_ocean_geometry_uvs(void *__restrict userdata,
                                        const int y,
                                        const TaskParallelTLS *__restrict /*tls*/)
{
  GenerateOceanGeometryData *gogd = static_cast<GenerateOceanGeometryData *>(userdata);
  int x;

  for (x = 0; x < gogd->res_x; x++) {
    const int i = y * gogd->res_x + x;
    float(*luv)[2] = &gogd->mloopuvs[i * 4];

    (*luv)[0] = x * gogd->ix;
    (*luv)[1] = y * gogd->iy;
    luv++;

    (*luv)[0] = (x + 1) * gogd->ix;
    (*luv)[1] = y * gogd->iy;
    luv++;

    (*luv)[0] = (x + 1) * gogd->ix;
    (*luv)[1] = (y + 1) * gogd->iy;
    luv++;

    (*luv)[0] = x * gogd->ix;
    (*luv)[1] = (y + 1) * gogd->iy;
    luv++;
  }
}

static Mesh *generate_ocean_geometry(OceanModifierData *omd, Mesh *mesh_orig, const int resolution)
{
  Mesh *result;

  GenerateOceanGeometryData gogd;

  int verts_num;
  int faces_num;

  const bool use_threading = resolution > 4;

  gogd.rx = resolution * resolution;
  gogd.ry = resolution * resolution;
  gogd.res_x = gogd.rx * omd->repeat_x;
  gogd.res_y = gogd.ry * omd->repeat_y;

  verts_num = (gogd.res_x + 1) * (gogd.res_y + 1);
  faces_num = gogd.res_x * gogd.res_y;

  gogd.sx = omd->size * omd->spatial_size;
  gogd.sy = omd->size * omd->spatial_size;
  gogd.ox = -gogd.sx / 2.0f;
  gogd.oy = -gogd.sy / 2.0f;

  gogd.sx /= gogd.rx;
  gogd.sy /= gogd.ry;

  result = BKE_mesh_new_nomain(verts_num, 0, faces_num, faces_num * 4);
  BKE_mesh_copy_parameters_for_eval(result, mesh_orig);

  gogd.vert_positions = result->vert_positions_for_write();
  gogd.face_offsets = result->face_offsets_for_write();
  gogd.corner_verts = result->corner_verts_for_write();

  TaskParallelSettings settings;
  BLI_parallel_range_settings_defaults(&settings);
  settings.use_threading = use_threading;

  /* create vertices */
  BLI_task_parallel_range(0, gogd.res_y + 1, &gogd, generate_ocean_geometry_verts, &settings);

  /* create faces */
  BLI_task_parallel_range(0, gogd.res_y, &gogd, generate_ocean_geometry_faces, &settings);

  BKE_mesh_calc_edges(result, false, false);

  /* add uvs */
  if (CustomData_number_of_layers(&result->loop_data, CD_PROP_FLOAT2) < MAX_MTFACE) {
    gogd.mloopuvs = static_cast<float(*)[2]>(CustomData_add_layer_named(
        &result->loop_data, CD_PROP_FLOAT2, CD_SET_DEFAULT, faces_num * 4, "UVMap"));

    if (gogd.mloopuvs) { /* unlikely to fail */
      gogd.ix = 1.0 / gogd.rx;
      gogd.iy = 1.0 / gogd.ry;

      BLI_task_parallel_range(0, gogd.res_y, &gogd, generate_ocean_geometry_uvs, &settings);
    }
  }

  return result;
}

static Mesh *doOcean(ModifierData *md, const ModifierEvalContext *ctx, Mesh *mesh)
{
  OceanModifierData *omd = (OceanModifierData *)md;
  if (omd->ocean && !BKE_ocean_is_valid(omd->ocean)) {
    BKE_modifier_set_error(ctx->object, md, "Failed to allocate memory");
    return mesh;
  }
  int cfra_scene = int(DEG_get_ctime(ctx->depsgraph));
  Object *ob = ctx->object;
  bool allocated_ocean = false;

  Mesh *result = nullptr;
  OceanResult ocr;

  const int resolution = (ctx->flag & MOD_APPLY_RENDER) ? omd->resolution :
                                                          omd->viewport_resolution;

  int cfra_for_cache;
  int i, j;

  /* use cached & inverted value for speed
   * expanded this would read...
   *
   * (axis / (omd->size * omd->spatial_size)) + 0.5f) */
#  define OCEAN_CO(_size_co_inv, _v) ((_v * _size_co_inv) + 0.5f)

  const float size_co_inv = 1.0f / (omd->size * omd->spatial_size);

  /* can happen in when size is small, avoid bad array lookups later and quit now */
  if (!isfinite(size_co_inv)) {
    return mesh;
  }

  /* do ocean simulation */
  if (omd->cached) {
    if (!omd->oceancache) {
      init_cache_data(ob, omd, resolution);
    }
    BKE_ocean_simulate_cache(omd->oceancache, cfra_scene);
  }
  else {
    /* omd->ocean is nullptr on an original object (in contrast to an evaluated one).
     * We can create a new one, but we have to free it as well once we're done.
     * This function is only called on an original object when applying the modifier
     * using the 'Apply Modifier' button, and thus it is not called frequently for
     * simulation. */
    allocated_ocean |= BKE_ocean_ensure(omd, resolution);
    simulate_ocean_modifier(omd);
  }

  if (omd->geometry_mode == MOD_OCEAN_GEOM_GENERATE) {
    result = generate_ocean_geometry(omd, mesh, resolution);
  }
  else if (omd->geometry_mode == MOD_OCEAN_GEOM_DISPLACE) {
    result = (Mesh *)BKE_id_copy_ex(nullptr, &mesh->id, nullptr, LIB_ID_COPY_LOCALIZE);
  }

  cfra_for_cache = cfra_scene;
  CLAMP(cfra_for_cache, omd->bakestart, omd->bakeend);
  cfra_for_cache -= omd->bakestart; /* shift to 0 based */

  blender::MutableSpan<blender::float3> positions = result->vert_positions_for_write();
  const blender::OffsetIndices faces = result->faces();

  /* Add vertex-colors before displacement: allows lookup based on position. */

  if (omd->flag & MOD_OCEAN_GENERATE_FOAM) {
    const blender::Span<int> corner_verts = result->corner_verts();
    MLoopCol *mloopcols = static_cast<MLoopCol *>(CustomData_add_layer_named(&result->loop_data,
                                                                             CD_PROP_BYTE_COLOR,
                                                                             CD_SET_DEFAULT,
                                                                             corner_verts.size(),
                                                                             omd->foamlayername));

    MLoopCol *mloopcols_spray = nullptr;
    if (omd->flag & MOD_OCEAN_GENERATE_SPRAY) {
      mloopcols_spray = static_cast<MLoopCol *>(CustomData_add_layer_named(&result->loop_data,
                                                                           CD_PROP_BYTE_COLOR,
                                                                           CD_SET_DEFAULT,
                                                                           corner_verts.size(),
                                                                           omd->spraylayername));
    }

    if (mloopcols) { /* unlikely to fail */

      for (const int i : faces.index_range()) {
        const blender::IndexRange face = faces[i];
        const int *corner_vert = &corner_verts[face.start()];
        MLoopCol *mlcol = &mloopcols[face.start()];

        MLoopCol *mlcolspray = nullptr;
        if (omd->flag & MOD_OCEAN_GENERATE_SPRAY) {
          mlcolspray = &mloopcols_spray[face.start()];
        }

        for (j = face.size(); j--; corner_vert++, mlcol++) {
          const float *vco = positions[*corner_vert];
          const float u = OCEAN_CO(size_co_inv, vco[0]);
          const float v = OCEAN_CO(size_co_inv, vco[1]);
          float foam;

          if (omd->oceancache && omd->cached) {
            BKE_ocean_cache_eval_uv(omd->oceancache, &ocr, cfra_for_cache, u, v);
            foam = ocr.foam;
            CLAMP(foam, 0.0f, 1.0f);
          }
          else {
            BKE_ocean_eval_uv(omd->ocean, &ocr, u, v);
            foam = BKE_ocean_jminus_to_foam(ocr.Jminus, omd->foam_coverage);
          }

          mlcol->r = mlcol->g = mlcol->b = char(foam * 255);
          /* This needs to be set (render engine uses) */
          mlcol->a = 255;

          if (omd->flag & MOD_OCEAN_GENERATE_SPRAY) {
            if (omd->flag & MOD_OCEAN_INVERT_SPRAY) {
              mlcolspray->r = ocr.Eminus[0] * 255;
            }
            else {
              mlcolspray->r = ocr.Eplus[0] * 255;
            }
            mlcolspray->g = 0;
            if (omd->flag & MOD_OCEAN_INVERT_SPRAY) {
              mlcolspray->b = ocr.Eminus[2] * 255;
            }
            else {
              mlcolspray->b = ocr.Eplus[2] * 255;
            }
            mlcolspray->a = 255;
          }
        }
      }
    }
  }

  /* displace the geometry */

  /* NOTE: tried to parallelized that one and previous foam loop,
   * but gives 20% slower results... odd. */
  {
    const int verts_num = result->totvert;

    for (i = 0; i < verts_num; i++) {
      float *vco = positions[i];
      const float u = OCEAN_CO(size_co_inv, vco[0]);
      const float v = OCEAN_CO(size_co_inv, vco[1]);

      if (omd->oceancache && omd->cached) {
        BKE_ocean_cache_eval_uv(omd->oceancache, &ocr, cfra_for_cache, u, v);
      }
      else {
        BKE_ocean_eval_uv(omd->ocean, &ocr, u, v);
      }

      vco[2] += ocr.disp[1];

      if (omd->chop_amount > 0.0f) {
        vco[0] += ocr.disp[0];
        vco[1] += ocr.disp[2];
      }
    }
  }

  BKE_mesh_tag_positions_changed(mesh);

  if (allocated_ocean) {
    BKE_ocean_free(omd->ocean);
    omd->ocean = nullptr;
  }

#  undef OCEAN_CO

  return result;
}
#else  /* WITH_OCEANSIM */
static Mesh *doOcean(ModifierData * /*md*/, const ModifierEvalContext * /*ctx*/, Mesh *mesh)
{
  return mesh;
}
#endif /* WITH_OCEANSIM */

static Mesh *modify_mesh(ModifierData *md, const ModifierEvalContext *ctx, Mesh *mesh)
{
  return doOcean(md, ctx, mesh);
}
// #define WITH_OCEANSIM
static void panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *layout = panel->layout;
#ifdef WITH_OCEANSIM
  uiLayout *col, *sub;

  PointerRNA ob_ptr;
  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, &ob_ptr);

  uiLayoutSetPropSep(layout, true);

  col = uiLayoutColumn(layout, false);
  uiItemR(col, ptr, "geometry_mode", UI_ITEM_NONE, nullptr, ICON_NONE);
  if (RNA_enum_get(ptr, "geometry_mode") == MOD_OCEAN_GEOM_GENERATE) {
    sub = uiLayoutColumn(col, true);
    uiItemR(sub, ptr, "repeat_x", UI_ITEM_NONE, IFACE_("Repeat X"), ICON_NONE);
    uiItemR(sub, ptr, "repeat_y", UI_ITEM_NONE, IFACE_("Y"), ICON_NONE);
  }

  sub = uiLayoutColumn(col, true);
  uiItemR(sub, ptr, "viewport_resolution", UI_ITEM_NONE, IFACE_("Resolution Viewport"), ICON_NONE);
  uiItemR(sub, ptr, "resolution", UI_ITEM_NONE, IFACE_("Render"), ICON_NONE);

  uiItemR(col, ptr, "time", UI_ITEM_NONE, nullptr, ICON_NONE);

  uiItemR(col, ptr, "depth", UI_ITEM_NONE, nullptr, ICON_NONE);
  uiItemR(col, ptr, "size", UI_ITEM_NONE, nullptr, ICON_NONE);
  uiItemR(col, ptr, "spatial_size", UI_ITEM_NONE, nullptr, ICON_NONE);

  uiItemR(col, ptr, "random_seed", UI_ITEM_NONE, nullptr, ICON_NONE);

  uiItemR(col, ptr, "use_normals", UI_ITEM_NONE, nullptr, ICON_NONE);

  modifier_panel_end(layout, ptr);

#else  /* WITH_OCEANSIM */
  uiItemL(layout, TIP_("Built without Ocean modifier"), ICON_NONE);
#endif /* WITH_OCEANSIM */
}

#ifdef WITH_OCEANSIM
static void waves_panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *col, *sub;
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, nullptr);

  uiLayoutSetPropSep(layout, true);

  col = uiLayoutColumn(layout, false);
  uiItemR(col, ptr, "wave_scale", UI_ITEM_NONE, IFACE_("Scale"), ICON_NONE);
  uiItemR(col, ptr, "wave_scale_min", UI_ITEM_NONE, nullptr, ICON_NONE);
  uiItemR(col, ptr, "choppiness", UI_ITEM_NONE, nullptr, ICON_NONE);
  uiItemR(col, ptr, "wind_velocity", UI_ITEM_NONE, nullptr, ICON_NONE);

  uiItemS(layout);

  col = uiLayoutColumn(layout, false);
  uiItemR(col, ptr, "wave_alignment", UI_ITEM_R_SLIDER, IFACE_("Alignment"), ICON_NONE);
  sub = uiLayoutColumn(col, false);
  uiLayoutSetActive(sub, RNA_float_get(ptr, "wave_alignment") > 0.0f);
  uiItemR(sub, ptr, "wave_direction", UI_ITEM_NONE, IFACE_("Direction"), ICON_NONE);
  uiItemR(sub, ptr, "damping", UI_ITEM_NONE, nullptr, ICON_NONE);
}

static void foam_panel_draw_header(const bContext * /*C*/, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, nullptr);

  uiItemR(layout, ptr, "use_foam", UI_ITEM_NONE, IFACE_("Foam"), ICON_NONE);
}

static void foam_panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *col;
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, nullptr);

  bool use_foam = RNA_boolean_get(ptr, "use_foam");

  uiLayoutSetPropSep(layout, true);

  col = uiLayoutColumn(layout, false);
  uiLayoutSetActive(col, use_foam);
  uiItemR(col, ptr, "foam_layer_name", UI_ITEM_NONE, IFACE_("Data Layer"), ICON_NONE);
  uiItemR(col, ptr, "foam_coverage", UI_ITEM_NONE, IFACE_("Coverage"), ICON_NONE);
}

static void spray_panel_draw_header(const bContext * /*C*/, Panel *panel)
{
  uiLayout *row;
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, nullptr);

  bool use_foam = RNA_boolean_get(ptr, "use_foam");

  row = uiLayoutRow(layout, false);
  uiLayoutSetActive(row, use_foam);
  uiItemR(row,
          ptr,
          "use_spray",
          UI_ITEM_NONE,
          CTX_IFACE_(BLT_I18NCONTEXT_ID_MESH, "Spray"),
          ICON_NONE);
}

static void spray_panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *col;
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, nullptr);

  bool use_foam = RNA_boolean_get(ptr, "use_foam");
  bool use_spray = RNA_boolean_get(ptr, "use_spray");

  uiLayoutSetPropSep(layout, true);

  col = uiLayoutColumn(layout, false);
  uiLayoutSetActive(col, use_foam && use_spray);
  uiItemR(col, ptr, "spray_layer_name", UI_ITEM_NONE, IFACE_("Data Layer"), ICON_NONE);
  uiItemR(col, ptr, "invert_spray", UI_ITEM_NONE, IFACE_("Invert"), ICON_NONE);
}

static void spectrum_panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *col;
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, nullptr);

  int spectrum = RNA_enum_get(ptr, "spectrum");

  uiLayoutSetPropSep(layout, true);

  col = uiLayoutColumn(layout, false);
  uiItemR(col, ptr, "spectrum", UI_ITEM_NONE, nullptr, ICON_NONE);
  if (ELEM(spectrum, MOD_OCEAN_SPECTRUM_TEXEL_MARSEN_ARSLOE, MOD_OCEAN_SPECTRUM_JONSWAP)) {
    uiItemR(col, ptr, "sharpen_peak_jonswap", UI_ITEM_R_SLIDER, nullptr, ICON_NONE);
    uiItemR(col, ptr, "fetch_jonswap", UI_ITEM_NONE, nullptr, ICON_NONE);
  }
}

static void bake_panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *col;
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, nullptr);

  uiLayoutSetPropSep(layout, true);

  bool is_cached = RNA_boolean_get(ptr, "is_cached");
  bool use_foam = RNA_boolean_get(ptr, "use_foam");

  if (is_cached) {
    PointerRNA op_ptr;
    uiItemFullO(layout,
                "OBJECT_OT_ocean_bake",
                IFACE_("Delete Bake"),
                ICON_NONE,
                nullptr,
                WM_OP_EXEC_DEFAULT,
                UI_ITEM_NONE,
                &op_ptr);
    RNA_boolean_set(&op_ptr, "free", true);
  }
  else {
    uiItemO(layout, nullptr, ICON_NONE, "OBJECT_OT_ocean_bake");
  }

  uiItemR(layout, ptr, "filepath", UI_ITEM_NONE, nullptr, ICON_NONE);

  col = uiLayoutColumn(layout, true);
  uiLayoutSetEnabled(col, !is_cached);
  uiItemR(col, ptr, "frame_start", UI_ITEM_NONE, IFACE_("Frame Start"), ICON_NONE);
  uiItemR(col, ptr, "frame_end", UI_ITEM_NONE, IFACE_("End"), ICON_NONE);

  col = uiLayoutColumn(layout, false);
  uiLayoutSetActive(col, use_foam);
  uiItemR(col, ptr, "bake_foam_fade", UI_ITEM_NONE, nullptr, ICON_NONE);
}
#endif /* WITH_OCEANSIM */

static void panel_register(ARegionType *region_type)
{
  PanelType *panel_type = modifier_panel_register(region_type, eModifierType_Ocean, panel_draw);
#ifdef WITH_OCEANSIM
  modifier_subpanel_register(region_type, "waves", "Waves", nullptr, waves_panel_draw, panel_type);
  PanelType *foam_panel = modifier_subpanel_register(
      region_type, "foam", "", foam_panel_draw_header, foam_panel_draw, panel_type);
  modifier_subpanel_register(
      region_type, "spray", "", spray_panel_draw_header, spray_panel_draw, foam_panel);
  modifier_subpanel_register(
      region_type, "spectrum", "Spectrum", nullptr, spectrum_panel_draw, panel_type);
  modifier_subpanel_register(region_type, "bake", "Bake", nullptr, bake_panel_draw, panel_type);
#else
  UNUSED_VARS(panel_type);
#endif /* WITH_OCEANSIM */
}

static void blend_read(BlendDataReader * /*reader*/, ModifierData *md)
{
  OceanModifierData *omd = (OceanModifierData *)md;
  omd->oceancache = nullptr;
  omd->ocean = nullptr;
}

ModifierTypeInfo modifierType_Ocean = {
    /*idname*/ "Ocean",
    /*name*/ N_("Ocean"),
    /*struct_name*/ "OceanModifierData",
    /*struct_size*/ sizeof(OceanModifierData),
    /*srna*/ &RNA_OceanModifier,
    /*type*/ eModifierTypeType_Constructive,
    /*flags*/ eModifierTypeFlag_AcceptsMesh | eModifierTypeFlag_SupportsEditmode |
        eModifierTypeFlag_EnableInEditmode,
    /*icon*/ ICON_MOD_OCEAN,

    /*copy_data*/ copy_data,
    /*deform_verts*/ nullptr,

    /*deform_matrices*/ nullptr,
    /*deform_verts_EM*/ nullptr,
    /*deform_matrices_EM*/ nullptr,
    /*modify_mesh*/ modify_mesh,
    /*modify_geometry_set*/ nullptr,

    /*init_data*/ init_data,
    /*required_data_mask*/ required_data_mask,
    /*free_data*/ free_data,
    /*is_disabled*/ nullptr,
    /*update_depsgraph*/ nullptr,
    /*depends_on_time*/ nullptr,
    /*depends_on_normals*/ depends_on_normals,
    /*foreach_ID_link*/ nullptr,
    /*foreach_tex_link*/ nullptr,
    /*free_runtime_data*/ nullptr,
    /*panel_register*/ panel_register,
    /*blend_write*/ nullptr,
    /*blend_read*/ blend_read,
};
