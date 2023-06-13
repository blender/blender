/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

#include <cstring>
#include <limits>

#include "BLI_math_vector.hh"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "DNA_cachefile_types.h"
#include "DNA_defaults.h"
#include "DNA_mesh_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"

#include "MEM_guardedalloc.h"

#include "BKE_cachefile.h"
#include "BKE_context.h"
#include "BKE_lib_query.h"
#include "BKE_mesh.hh"
#include "BKE_object.h"
#include "BKE_scene.h"
#include "BKE_screen.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "RNA_access.h"
#include "RNA_prototypes.h"

#include "BLO_read_write.h"

#include "DEG_depsgraph_build.h"
#include "DEG_depsgraph_query.h"

#include "GEO_mesh_primitive_cuboid.hh"

#include "MOD_modifiertypes.hh"
#include "MOD_ui_common.hh"

#if defined(WITH_USD) || defined(WITH_ALEMBIC)
#  include "BKE_global.h"
#  include "BKE_lib_id.h"
#endif

#ifdef WITH_ALEMBIC
#  include "ABC_alembic.h"
#endif

#ifdef WITH_USD
#  include "usd.h"
#endif

using blender::float3;
using blender::Span;

static void initData(ModifierData *md)
{
  MeshSeqCacheModifierData *mcmd = reinterpret_cast<MeshSeqCacheModifierData *>(md);

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(mcmd, modifier));

  mcmd->cache_file = nullptr;
  mcmd->object_path[0] = '\0';
  mcmd->read_flag = MOD_MESHSEQ_READ_ALL;

  MEMCPY_STRUCT_AFTER(mcmd, DNA_struct_default_get(MeshSeqCacheModifierData), modifier);
}

static void copyData(const ModifierData *md, ModifierData *target, const int flag)
{
#if 0
  const MeshSeqCacheModifierData *mcmd = (const MeshSeqCacheModifierData *)md;
#endif
  MeshSeqCacheModifierData *tmcmd = (MeshSeqCacheModifierData *)target;

  BKE_modifier_copydata_generic(md, target, flag);

  tmcmd->reader = nullptr;
  tmcmd->reader_object_path[0] = '\0';
}

static void freeData(ModifierData *md)
{
  MeshSeqCacheModifierData *mcmd = reinterpret_cast<MeshSeqCacheModifierData *>(md);

  if (mcmd->reader) {
    mcmd->reader_object_path[0] = '\0';
    BKE_cachefile_reader_free(mcmd->cache_file, &mcmd->reader);
  }
}

static bool isDisabled(const Scene * /*scene*/, ModifierData *md, bool /*useRenderParams*/)
{
  MeshSeqCacheModifierData *mcmd = reinterpret_cast<MeshSeqCacheModifierData *>(md);

  /* leave it up to the modifier to check the file is valid on calculation */
  return (mcmd->cache_file == nullptr) || (mcmd->object_path[0] == '\0');
}

#if defined(WITH_USD) || defined(WITH_ALEMBIC)

/* Return true if the modifier evaluation is for the ORCO mesh and the mesh hasn't changed
 * topology.
 */
static bool can_use_mesh_for_orco_evaluation(MeshSeqCacheModifierData *mcmd,
                                             const ModifierEvalContext *ctx,
                                             const Mesh *mesh,
                                             const float time,
                                             const char **err_str)
{
  if ((ctx->flag & MOD_APPLY_ORCO) == 0) {
    return false;
  }

  CacheFile *cache_file = mcmd->cache_file;

  switch (cache_file->type) {
    case CACHEFILE_TYPE_ALEMBIC:
#  ifdef WITH_ALEMBIC
      if (!ABC_mesh_topology_changed(mcmd->reader, ctx->object, mesh, time, err_str)) {
        return true;
      }
#  endif
      break;
    case CACHEFILE_TYPE_USD:
#  ifdef WITH_USD
      if (!USD_mesh_topology_changed(mcmd->reader, ctx->object, mesh, time, err_str)) {
        return true;
      }
#  endif
      break;
    case CACHE_FILE_TYPE_INVALID:
      break;
  }

  return false;
}

static Mesh *generate_bounding_box_mesh(const Mesh *org_mesh)
{
  using namespace blender;
  float3 min(std::numeric_limits<float>::max());
  float3 max(-std::numeric_limits<float>::max());
  if (!BKE_mesh_minmax(org_mesh, min, max)) {
    return nullptr;
  }

  Mesh *result = geometry::create_cuboid_mesh(max - min, 2, 2, 2);
  if (org_mesh->mat) {
    result->mat = static_cast<Material **>(MEM_dupallocN(org_mesh->mat));
    result->totcol = org_mesh->totcol;
  }
  BKE_mesh_translate(result, math::midpoint(min, max), false);

  return result;
}

#endif

static Mesh *modifyMesh(ModifierData *md, const ModifierEvalContext *ctx, Mesh *mesh)
{
#if defined(WITH_USD) || defined(WITH_ALEMBIC)
  MeshSeqCacheModifierData *mcmd = reinterpret_cast<MeshSeqCacheModifierData *>(md);

  /* Only used to check whether we are operating on org data or not... */
  Mesh *me = (ctx->object->type == OB_MESH) ? static_cast<Mesh *>(ctx->object->data) : nullptr;
  Mesh *org_mesh = mesh;

  Scene *scene = DEG_get_evaluated_scene(ctx->depsgraph);
  CacheFile *cache_file = mcmd->cache_file;
  const float frame = DEG_get_ctime(ctx->depsgraph);
  const double time = BKE_cachefile_time_offset(cache_file, double(frame), FPS);
  const char *err_str = nullptr;

  if (!mcmd->reader || !STREQ(mcmd->reader_object_path, mcmd->object_path)) {
    STRNCPY(mcmd->reader_object_path, mcmd->object_path);
    BKE_cachefile_reader_open(cache_file, &mcmd->reader, ctx->object, mcmd->object_path);
    if (!mcmd->reader) {
      BKE_modifier_set_error(
          ctx->object, md, "Could not create reader for file %s", cache_file->filepath);
      return mesh;
    }
  }

  /* Do not process data if using a render procedural, return a box instead for displaying in the
   * viewport. */
  if (BKE_cache_file_uses_render_procedural(cache_file, scene)) {
    return generate_bounding_box_mesh(org_mesh);
  }

  /* If this invocation is for the ORCO mesh, and the mesh hasn't changed topology, we
   * must return the mesh as-is instead of deforming it. */
  if (can_use_mesh_for_orco_evaluation(mcmd, ctx, mesh, time, &err_str)) {
    return mesh;
  }

  if (me != nullptr) {
    const Span<float3> mesh_positions = mesh->vert_positions();
    const Span<blender::int2> mesh_edges = mesh->edges();
    const blender::OffsetIndices mesh_polys = mesh->polys();
    const Span<float3> me_positions = me->vert_positions();
    const Span<blender::int2> me_edges = me->edges();
    const blender::OffsetIndices me_polys = me->polys();

    /* TODO(sybren+bastien): possibly check relevant custom data layers (UV/color depending on
     * flags) and duplicate those too.
     * XXX(Hans): This probably isn't true anymore with various CoW improvements, etc. */
    if ((me_positions.data() == mesh_positions.data()) || (me_edges.data() == mesh_edges.data()) ||
        (me_polys.data() == mesh_polys.data()))
    {
      /* We need to duplicate data here, otherwise we'll modify org mesh, see #51701. */
      mesh = reinterpret_cast<Mesh *>(
          BKE_id_copy_ex(nullptr,
                         &mesh->id,
                         nullptr,
                         LIB_ID_CREATE_NO_MAIN | LIB_ID_CREATE_NO_USER_REFCOUNT |
                             LIB_ID_CREATE_NO_DEG_TAG | LIB_ID_COPY_NO_PREVIEW));
    }
  }

  Mesh *result = nullptr;

  switch (cache_file->type) {
    case CACHEFILE_TYPE_ALEMBIC: {
#  ifdef WITH_ALEMBIC
      /* Time (in frames or seconds) between two velocity samples. Automatically computed to
       * scale the velocity vectors at render time for generating proper motion blur data. */
      float velocity_scale = mcmd->velocity_scale;
      if (mcmd->cache_file->velocity_unit == CACHEFILE_VELOCITY_UNIT_FRAME) {
        velocity_scale *= FPS;
      }

      ABCReadParams params = {};
      params.time = time;
      params.read_flags = mcmd->read_flag;
      params.velocity_name = mcmd->cache_file->velocity_name;
      params.velocity_scale = velocity_scale;

      result = ABC_read_mesh(mcmd->reader, ctx->object, mesh, &params, &err_str);
#  endif
      break;
    }
    case CACHEFILE_TYPE_USD: {
#  ifdef WITH_USD
      const USDMeshReadParams params = create_mesh_read_params(time * FPS, mcmd->read_flag);
      result = USD_read_mesh(mcmd->reader, ctx->object, mesh, params, &err_str);
#  endif
      break;
    }
    case CACHE_FILE_TYPE_INVALID:
      break;
  }

  if (err_str) {
    BKE_modifier_set_error(ctx->object, md, "%s", err_str);
  }

  if (!ELEM(result, nullptr, mesh) && (mesh != org_mesh)) {
    BKE_id_free(nullptr, mesh);
    mesh = org_mesh;
  }

  return result ? result : mesh;
#else
  UNUSED_VARS(ctx, md);
  return mesh;
#endif
}

static bool dependsOnTime(Scene *scene, ModifierData *md)
{
#if defined(WITH_USD) || defined(WITH_ALEMBIC)
  MeshSeqCacheModifierData *mcmd = reinterpret_cast<MeshSeqCacheModifierData *>(md);
  /* Do not evaluate animations if using the render engine procedural. */
  return (mcmd->cache_file != nullptr) &&
         !BKE_cache_file_uses_render_procedural(mcmd->cache_file, scene);
#else
  UNUSED_VARS(scene, md);
  return false;
#endif
}

static void foreachIDLink(ModifierData *md, Object *ob, IDWalkFunc walk, void *userData)
{
  MeshSeqCacheModifierData *mcmd = reinterpret_cast<MeshSeqCacheModifierData *>(md);

  walk(userData, ob, reinterpret_cast<ID **>(&mcmd->cache_file), IDWALK_CB_USER);
}

static void updateDepsgraph(ModifierData *md, const ModifierUpdateDepsgraphContext *ctx)
{
  MeshSeqCacheModifierData *mcmd = reinterpret_cast<MeshSeqCacheModifierData *>(md);

  if (mcmd->cache_file != nullptr) {
    DEG_add_object_cache_relation(
        ctx->node, mcmd->cache_file, DEG_OB_COMP_CACHE, "Mesh Cache File");
  }
}

static void panel_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA ob_ptr;
  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, &ob_ptr);

  PointerRNA cache_file_ptr = RNA_pointer_get(ptr, "cache_file");
  bool has_cache_file = !RNA_pointer_is_null(&cache_file_ptr);

  uiLayoutSetPropSep(layout, true);

  uiTemplateCacheFile(layout, C, ptr, "cache_file");

  if (has_cache_file) {
    uiItemPointerR(
        layout, ptr, "object_path", &cache_file_ptr, "object_paths", nullptr, ICON_NONE);
  }

  if (RNA_enum_get(&ob_ptr, "type") == OB_MESH) {
    uiItemR(layout, ptr, "read_data", UI_ITEM_R_EXPAND, nullptr, ICON_NONE);
    uiItemR(layout, ptr, "use_vertex_interpolation", 0, nullptr, ICON_NONE);
  }

  modifier_panel_end(layout, ptr);
}

static void velocity_panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA ob_ptr;
  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, &ob_ptr);

  PointerRNA fileptr;
  if (!uiTemplateCacheFilePointer(ptr, "cache_file", &fileptr)) {
    return;
  }

  uiLayoutSetPropSep(layout, true);
  uiTemplateCacheFileVelocity(layout, &fileptr);
  uiItemR(layout, ptr, "velocity_scale", 0, nullptr, ICON_NONE);
}

static void time_panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA ob_ptr;
  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, &ob_ptr);

  PointerRNA fileptr;
  if (!uiTemplateCacheFilePointer(ptr, "cache_file", &fileptr)) {
    return;
  }

  uiLayoutSetPropSep(layout, true);
  uiTemplateCacheFileTimeSettings(layout, &fileptr);
}

static void render_procedural_panel_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA ob_ptr;
  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, &ob_ptr);

  PointerRNA fileptr;
  if (!uiTemplateCacheFilePointer(ptr, "cache_file", &fileptr)) {
    return;
  }

  uiLayoutSetPropSep(layout, true);
  uiTemplateCacheFileProcedural(layout, C, &fileptr);
}

static void override_layers_panel_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA ob_ptr;
  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, &ob_ptr);

  PointerRNA fileptr;
  if (!uiTemplateCacheFilePointer(ptr, "cache_file", &fileptr)) {
    return;
  }

  uiLayoutSetPropSep(layout, true);
  uiTemplateCacheFileLayers(layout, C, &fileptr);
}

static void panelRegister(ARegionType *region_type)
{
  PanelType *panel_type = modifier_panel_register(
      region_type, eModifierType_MeshSequenceCache, panel_draw);
  modifier_subpanel_register(region_type, "time", "Time", nullptr, time_panel_draw, panel_type);
  modifier_subpanel_register(region_type,
                             "render_procedural",
                             "Render Procedural",
                             nullptr,
                             render_procedural_panel_draw,
                             panel_type);
  modifier_subpanel_register(
      region_type, "velocity", "Velocity", nullptr, velocity_panel_draw, panel_type);
  modifier_subpanel_register(region_type,
                             "override_layers",
                             "Override Layers",
                             nullptr,
                             override_layers_panel_draw,
                             panel_type);
}

static void blendRead(BlendDataReader * /*reader*/, ModifierData *md)
{
  MeshSeqCacheModifierData *msmcd = reinterpret_cast<MeshSeqCacheModifierData *>(md);
  msmcd->reader = nullptr;
  msmcd->reader_object_path[0] = '\0';
}

ModifierTypeInfo modifierType_MeshSequenceCache = {
    /*name*/ N_("MeshSequenceCache"),
    /*structName*/ "MeshSeqCacheModifierData",
    /*structSize*/ sizeof(MeshSeqCacheModifierData),
    /*srna*/ &RNA_MeshSequenceCacheModifier,
    /*type*/ eModifierTypeType_Constructive,
    /*flags*/
    static_cast<ModifierTypeFlag>(eModifierTypeFlag_AcceptsMesh | eModifierTypeFlag_AcceptsCVs),
    /*icon*/ ICON_MOD_MESHDEFORM, /* TODO: Use correct icon. */

    /*copyData*/ copyData,

    /*deformVerts*/ nullptr,
    /*deformMatrices*/ nullptr,
    /*deformVertsEM*/ nullptr,
    /*deformMatricesEM*/ nullptr,
    /*modifyMesh*/ modifyMesh,
    /*modifyGeometrySet*/ nullptr,

    /*initData*/ initData,
    /*requiredDataMask*/ nullptr,
    /*freeData*/ freeData,
    /*isDisabled*/ isDisabled,
    /*updateDepsgraph*/ updateDepsgraph,
    /*dependsOnTime*/ dependsOnTime,
    /*dependsOnNormals*/ nullptr,
    /*foreachIDLink*/ foreachIDLink,
    /*foreachTexLink*/ nullptr,
    /*freeRuntimeData*/ nullptr,
    /*panelRegister*/ panelRegister,
    /*blendWrite*/ nullptr,
    /*blendRead*/ blendRead,
};
