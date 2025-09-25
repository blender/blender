/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

#include <cstring>

#include "BLI_math_vector.hh"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "BLT_translation.hh"

#include "DNA_cachefile_types.h"
#include "DNA_defaults.h"
#include "DNA_mesh_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_pointcloud_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"

#include "MEM_guardedalloc.h"

#include "BKE_cachefile.hh"
#include "BKE_geometry_set.hh"
#include "BKE_lib_query.hh"
#include "BKE_mesh.hh"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "DEG_depsgraph_build.hh"
#include "DEG_depsgraph_query.hh"

#include "GEO_mesh_primitive_cuboid.hh"

#include "MOD_modifiertypes.hh"
#include "MOD_ui_common.hh"

#if defined(WITH_USD) || defined(WITH_ALEMBIC)
#  include "BKE_lib_id.hh"
#endif

#ifdef WITH_ALEMBIC
#  include "ABC_alembic.h"
#endif

#ifdef WITH_USD
#  include "usd.hh"
#endif

using namespace blender;

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
                                             const double frame_offset,
                                             const double time_offset,
                                             const char **r_err_str)
{
  if ((ctx->flag & MOD_APPLY_ORCO) == 0) {
    return false;
  }

  CacheFile *cache_file = mcmd->cache_file;

  switch (cache_file->type) {
    case CACHEFILE_TYPE_ALEMBIC:
#  ifdef WITH_ALEMBIC
      if (!ABC_mesh_topology_changed(mcmd->reader, ctx->object, mesh, time_offset, r_err_str)) {
        return true;
      }
#  else
      UNUSED_VARS(time_offset);
#  endif
      break;
    case CACHEFILE_TYPE_USD:
#  ifdef WITH_USD
      if (!blender::io::usd::USD_mesh_topology_changed(
              mcmd->reader, ctx->object, mesh, frame_offset, r_err_str))
      {
        return true;
      }
#  else
      UNUSED_VARS(frame_offset);
#  endif
      break;
    case CACHE_FILE_TYPE_INVALID:
      break;
  }

  return false;
}
#endif

static void modify_geometry_set(ModifierData *md,
                                const ModifierEvalContext *ctx,
                                bke::GeometrySet *geometry_set)
{
#if defined(WITH_USD) || defined(WITH_ALEMBIC)
  MeshSeqCacheModifierData *mcmd = reinterpret_cast<MeshSeqCacheModifierData *>(md);

  Scene *scene = DEG_get_evaluated_scene(ctx->depsgraph);
  CacheFile *cache_file = mcmd->cache_file;
  const double frame = double(DEG_get_ctime(ctx->depsgraph));
  const double frame_offset = BKE_cachefile_frame_offset(cache_file, frame);
  const double time_offset = BKE_cachefile_time_offset(
      cache_file, frame, scene->frames_per_second());
  const char *err_str = nullptr;

  if (!mcmd->reader || !STREQ(mcmd->reader_object_path, mcmd->object_path)) {
    STRNCPY(mcmd->reader_object_path, mcmd->object_path);
    BKE_cachefile_reader_open(cache_file, &mcmd->reader, ctx->object, mcmd->object_path);
    if (!mcmd->reader) {
      BKE_modifier_set_error(
          ctx->object, md, "Could not create cache reader for file %s", cache_file->filepath);
      return;
    }
  }

  if (geometry_set->has_mesh()) {
    const Mesh *mesh = geometry_set->get_mesh();
    if (can_use_mesh_for_orco_evaluation(mcmd, ctx, mesh, frame_offset, time_offset, &err_str)) {
      return;
    }
  }

  /* Time (in frames or seconds) between two velocity samples. Automatically computed to
   * scale the velocity vectors at render time for generating proper motion blur data. */
#  ifdef WITH_ALEMBIC
  float velocity_scale = mcmd->velocity_scale;
  if (mcmd->cache_file->velocity_unit == CACHEFILE_VELOCITY_UNIT_FRAME) {
    velocity_scale *= scene->frames_per_second();
  }
#  endif

  switch (cache_file->type) {
    case CACHEFILE_TYPE_ALEMBIC: {
#  ifdef WITH_ALEMBIC
      ABCReadParams params;
      params.time = time_offset;
      params.read_flags = mcmd->read_flag;
      params.velocity_name = mcmd->cache_file->velocity_name;
      params.velocity_scale = velocity_scale;
      ABC_read_geometry(mcmd->reader, ctx->object, *geometry_set, &params, &err_str);
#  endif
      break;
    }
    case CACHEFILE_TYPE_USD: {
#  ifdef WITH_USD
      const blender::io::usd::USDMeshReadParams params = blender::io::usd::create_mesh_read_params(
          frame_offset, mcmd->read_flag);
      blender::io::usd::USD_read_geometry(
          mcmd->reader, ctx->object, *geometry_set, params, &err_str);
#  endif
      break;
    }
    case CACHE_FILE_TYPE_INVALID:
      break;
  }

  if (err_str) {
    BKE_modifier_set_error(ctx->object, md, "%s", err_str);
  }

#else
  UNUSED_VARS(ctx, md, geometry_set);
  return;
#endif
}

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
  const double frame = double(DEG_get_ctime(ctx->depsgraph));
  const double frame_offset = BKE_cachefile_frame_offset(cache_file, frame);
  const double time_offset = BKE_cachefile_time_offset(
      cache_file, frame, scene->frames_per_second());
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

  /* If this invocation is for the ORCO mesh, and the mesh hasn't changed topology, we
   * must return the mesh as-is instead of deforming it. */
  if (can_use_mesh_for_orco_evaluation(mcmd, ctx, mesh, frame_offset, time_offset, &err_str)) {
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
     * XXX(Hans): This probably isn't true anymore with various copy-on-eval improvements, etc. */
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

  bke::GeometrySet geometry_set = bke::GeometrySet::from_mesh(
      mesh, bke::GeometryOwnershipType::Editable);
  modify_geometry_set(md, ctx, &geometry_set);
  Mesh *result = geometry_set.get_component_for_write<bke::MeshComponent>().release();

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

static bool depends_on_time(Scene * /*scene*/, ModifierData *md)
{
#if defined(WITH_USD) || defined(WITH_ALEMBIC)
  MeshSeqCacheModifierData *mcmd = reinterpret_cast<MeshSeqCacheModifierData *>(md);
  return (mcmd->cache_file != nullptr);
#else
  UNUSED_VARS(md);
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

  layout->use_property_split_set(true);

  uiTemplateCacheFile(layout, C, ptr, "cache_file");

  if (has_cache_file) {
    layout->prop_search(
        ptr, "object_path", &cache_file_ptr, "object_paths", std::nullopt, ICON_NONE);
  }

  if (RNA_enum_get(&ob_ptr, "type") == OB_MESH) {
    layout->prop(ptr, "read_data", UI_ITEM_R_EXPAND, std::nullopt, ICON_NONE);
    layout->prop(ptr, "use_vertex_interpolation", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  }
  else if (RNA_enum_get(&ob_ptr, "type") == OB_CURVES) {
    layout->prop(ptr, "use_vertex_interpolation", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  }

  modifier_error_message_draw(layout, ptr);
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

  layout->use_property_split_set(true);
  uiTemplateCacheFileVelocity(layout, &fileptr);
  layout->prop(ptr, "velocity_scale", UI_ITEM_NONE, std::nullopt, ICON_NONE);
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

  layout->use_property_split_set(true);
  uiTemplateCacheFileTimeSettings(layout, &fileptr);
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

  layout->use_property_split_set(true);
  uiTemplateCacheFileLayers(layout, C, &fileptr);
}

static void panel_register(ARegionType *region_type)
{
  PanelType *panel_type = modifier_panel_register(
      region_type, eModifierType_MeshSequenceCache, panel_draw);
  modifier_subpanel_register(region_type, "time", "Time", nullptr, time_panel_draw, panel_type);
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
    (eModifierTypeFlag_AcceptsMesh | eModifierTypeFlag_AcceptsCVs),
    /*icon*/ ICON_MOD_MESHDEFORM, /* TODO: Use correct icon. */

    /*copy_data*/ copy_data,

    /*deform_verts*/ nullptr,
    /*deform_matrices*/ nullptr,
    /*deform_verts_EM*/ nullptr,
    /*deform_matrices_EM*/ nullptr,
    /*modify_mesh*/ modify_mesh,
    /*modify_geometry_set*/ modify_geometry_set,

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
    /*foreach_working_space_color*/ nullptr,
};
