/* SPDX-FileCopyrightText: 2023 Blender Authors
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
#include "BKE_context.hh"
#include "BKE_lib_query.hh"
#include "BKE_mesh.hh"
#include "BKE_object.hh"
#include "BKE_scene.h"
#include "BKE_screen.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.h"

#include "BLO_read_write.hh"

#include "DEG_depsgraph_build.hh"
#include "DEG_depsgraph_query.hh"

#include "GEO_mesh_primitive_cuboid.hh"

#include "MOD_modifiertypes.hh"
#include "MOD_ui_common.hh"

#if defined(WITH_USD) || defined(WITH_ALEMBIC)
#  include "BKE_global.h"
#  include "BKE_lib_id.hh"
#endif

#ifdef WITH_ALEMBIC
#  include "ABC_alembic.h"
#endif

#ifdef WITH_USD
#  include "usd.hh"
#endif

using blender::float3;
using blender::Span;

static void init_data(ModifierData *md)
{
  MeshSeqCacheModifierData *mcmd = reinterpret_cast<MeshSeqCacheModifierData *>(md);

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(mcmd, modifier));

  mcmd->cache_file = nullptr;
  mcmd->object_path[0] = '\0';
  mcmd->read_flag = MOD_MESHSEQ_READ_ALL;

  MEMCPY_STRUCT_AFTER(mcmd, DNA_struct_default_get(MeshSeqCacheModifierData), modifier);
}

static void copy_data(const ModifierData *md, ModifierData *target, const int flag)
{
#if 0
  const MeshSeqCacheModifierData *mcmd = (const MeshSeqCacheModifierData *)md;
#endif
  MeshSeqCacheModifierData *tmcmd = (MeshSeqCacheModifierData *)target;

  BKE_modifier_copydata_generic(md, target, flag);

  tmcmd->reader = nullptr;
  tmcmd->reader_object_path[0] = '\0';
}

static void free_data(ModifierData *md)
{
  MeshSeqCacheModifierData *mcmd = reinterpret_cast<MeshSeqCacheModifierData *>(md);

  if (mcmd->reader) {
    mcmd->reader_object_path[0] = '\0';
    BKE_cachefile_reader_free(mcmd->cache_file, &mcmd->reader);
  }
}

static bool is_disabled(const Scene * /*scene*/, ModifierData *md, bool /*use_render_params*/)
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
      if (!blender::io::usd::USD_mesh_topology_changed(
              mcmd->reader, ctx->object, mesh, time, err_str))
      {
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
  const std::optional<Bounds<float3>> bounds = org_mesh->bounds_min_max();
  if (!bounds) {
    return nullptr;
  }

  Mesh *result = geometry::create_cuboid_mesh(bounds->max - bounds->min, 2, 2, 2);
  if (org_mesh->mat) {
    result->mat = static_cast<Material **>(MEM_dupallocN(org_mesh->mat));
    result->totcol = org_mesh->totcol;
  }
  BKE_mesh_translate(result, math::midpoint(bounds->min, bounds->max), false);

  return result;
}

#endif

static Mesh *modify_mesh(ModifierData *md, const ModifierEvalContext *ctx, Mesh *mesh)
{
#if defined(WITH_USD) || defined(WITH_ALEMBIC)
  MeshSeqCacheModifierData *mcmd = reinterpret_cast<MeshSeqCacheModifierData *>(md);

  /* Only used to check whether we are operating on org data or not... */
  Mesh *object_mesh = (ctx->object->type == OB_MESH) ? static_cast<Mesh *>(ctx->object->data) :
                                                       nullptr;
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

  if (object_mesh != nullptr) {
    const Span<float3> mesh_positions = mesh->vert_positions();
    const Span<blender::int2> mesh_edges = mesh->edges();
    const blender::OffsetIndices mesh_faces = mesh->faces();
    const Span<float3> me_positions = object_mesh->vert_positions();
    const Span<blender::int2> me_edges = object_mesh->edges();
    const blender::OffsetIndices me_faces = object_mesh->faces();

    /* TODO(sybren+bastien): possibly check relevant custom data layers (UV/color depending on
     * flags) and duplicate those too.
     * XXX(Hans): This probably isn't true anymore with various CoW improvements, etc. */
    if ((me_positions.data() == mesh_positions.data()) || (me_edges.data() == mesh_edges.data()) ||
        (me_faces.data() == mesh_faces.data()))
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
      const blender::io::usd::USDMeshReadParams params = blender::io::usd::create_mesh_read_params(
          time * FPS, mcmd->read_flag);
      result = blender::io::usd::USD_read_mesh(mcmd->reader, ctx->object, mesh, params, &err_str);
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

static bool depends_on_time(Scene *scene, ModifierData *md)
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

static void foreach_ID_link(ModifierData *md, Object *ob, IDWalkFunc walk, void *user_data)
{
  MeshSeqCacheModifierData *mcmd = reinterpret_cast<MeshSeqCacheModifierData *>(md);

  walk(user_data, ob, reinterpret_cast<ID **>(&mcmd->cache_file), IDWALK_CB_USER);
}

static void update_depsgraph(ModifierData *md, const ModifierUpdateDepsgraphContext *ctx)
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
    uiItemR(layout, ptr, "use_vertex_interpolation", UI_ITEM_NONE, nullptr, ICON_NONE);
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
  uiItemR(layout, ptr, "velocity_scale", UI_ITEM_NONE, nullptr, ICON_NONE);
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

static void panel_register(ARegionType *region_type)
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

static void blend_read(BlendDataReader * /*reader*/, ModifierData *md)
{
  MeshSeqCacheModifierData *msmcd = reinterpret_cast<MeshSeqCacheModifierData *>(md);
  msmcd->reader = nullptr;
  msmcd->reader_object_path[0] = '\0';
}

ModifierTypeInfo modifierType_MeshSequenceCache = {
    /*idname*/ "MeshSequenceCache",
    /*name*/ N_("MeshSequenceCache"),
    /*struct_name*/ "MeshSeqCacheModifierData",
    /*struct_size*/ sizeof(MeshSeqCacheModifierData),
    /*srna*/ &RNA_MeshSequenceCacheModifier,
    /*type*/ ModifierTypeType::Constructive,
    /*flags*/
    static_cast<ModifierTypeFlag>(eModifierTypeFlag_AcceptsMesh | eModifierTypeFlag_AcceptsCVs),
    /*icon*/ ICON_MOD_MESHDEFORM, /* TODO: Use correct icon. */

    /*copy_data*/ copy_data,

    /*deform_verts*/ nullptr,
    /*deform_matrices*/ nullptr,
    /*deform_verts_EM*/ nullptr,
    /*deform_matrices_EM*/ nullptr,
    /*modify_mesh*/ modify_mesh,
    /*modify_geometry_set*/ nullptr,

    /*init_data*/ init_data,
    /*required_data_mask*/ nullptr,
    /*free_data*/ free_data,
    /*is_disabled*/ is_disabled,
    /*update_depsgraph*/ update_depsgraph,
    /*depends_on_time*/ depends_on_time,
    /*depends_on_normals*/ nullptr,
    /*foreach_ID_link*/ foreach_ID_link,
    /*foreach_tex_link*/ nullptr,
    /*free_runtime_data*/ nullptr,
    /*panel_register*/ panel_register,
    /*blend_write*/ nullptr,
    /*blend_read*/ blend_read,
    /*foreach_cache*/ nullptr,
};
