/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

#include <vector>

#include "BKE_geometry_set.hh"
#include "BKE_lib_query.h"
#include "BKE_mesh_runtime.h"
#include "BKE_mesh_wrapper.h"
#include "BKE_modifier.h"
#include "BKE_object.h"
#include "BKE_volume.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"
#include "DNA_volume_types.h"

#include "DEG_depsgraph.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "BLO_read_write.h"

#include "MEM_guardedalloc.h"

#include "MOD_modifiertypes.h"
#include "MOD_ui_common.h"

#include "BLI_float4x4.hh"
#include "BLI_index_range.hh"
#include "BLI_span.hh"

#include "RNA_access.h"
#include "RNA_prototypes.h"

#ifdef WITH_OPENVDB
#  include <openvdb/openvdb.h>
#  include <openvdb/tools/MeshToVolume.h>
#endif

#ifdef WITH_OPENVDB
namespace blender {
/* This class follows the MeshDataAdapter interface from openvdb. */
class OpenVDBMeshAdapter {
 private:
  Span<MVert> vertices_;
  Span<MLoop> loops_;
  Span<MLoopTri> looptris_;
  float4x4 transform_;

 public:
  OpenVDBMeshAdapter(Mesh &mesh, float4x4 transform)
      : vertices_(mesh.mvert, mesh.totvert),
        loops_(mesh.mloop, mesh.totloop),
        transform_(transform)
  {
    const MLoopTri *looptries = BKE_mesh_runtime_looptri_ensure(&mesh);
    const int looptries_len = BKE_mesh_runtime_looptri_len(&mesh);
    looptris_ = Span(looptries, looptries_len);
  }

  size_t polygonCount() const
  {
    return static_cast<size_t>(looptris_.size());
  }

  size_t pointCount() const
  {
    return static_cast<size_t>(vertices_.size());
  }

  size_t vertexCount(size_t UNUSED(polygon_index)) const
  {
    /* All polygons are triangles. */
    return 3;
  }

  void getIndexSpacePoint(size_t polygon_index, size_t vertex_index, openvdb::Vec3d &pos) const
  {
    const MLoopTri &looptri = looptris_[polygon_index];
    const MVert &vertex = vertices_[loops_[looptri.tri[vertex_index]].v];
    const float3 transformed_co = transform_ * float3(vertex.co);
    pos = &transformed_co.x;
  }
};
}  // namespace blender
#endif

static void initData(ModifierData *md)
{
  MeshToVolumeModifierData *mvmd = reinterpret_cast<MeshToVolumeModifierData *>(md);
  mvmd->object = nullptr;
  mvmd->resolution_mode = MESH_TO_VOLUME_RESOLUTION_MODE_VOXEL_AMOUNT;
  mvmd->voxel_size = 0.1f;
  mvmd->voxel_amount = 32;
  mvmd->fill_volume = true;
  mvmd->interior_band_width = 0.1f;
  mvmd->exterior_band_width = 0.1f;
  mvmd->density = 1.0f;
}

static void updateDepsgraph(ModifierData *md, const ModifierUpdateDepsgraphContext *ctx)
{
  MeshToVolumeModifierData *mvmd = reinterpret_cast<MeshToVolumeModifierData *>(md);
  DEG_add_modifier_to_transform_relation(ctx->node, "Mesh to Volume Modifier");
  if (mvmd->object) {
    DEG_add_object_relation(
        ctx->node, mvmd->object, DEG_OB_COMP_GEOMETRY, "Mesh to Volume Modifier");
    DEG_add_object_relation(
        ctx->node, mvmd->object, DEG_OB_COMP_TRANSFORM, "Mesh to Volume Modifier");
  }
}

static void foreachIDLink(ModifierData *md, Object *ob, IDWalkFunc walk, void *userData)
{
  MeshToVolumeModifierData *mvmd = reinterpret_cast<MeshToVolumeModifierData *>(md);
  walk(userData, ob, (ID **)&mvmd->object, IDWALK_CB_NOP);
}

static void panel_draw(const bContext *UNUSED(C), Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, nullptr);
  MeshToVolumeModifierData *mvmd = static_cast<MeshToVolumeModifierData *>(ptr->data);

  uiLayoutSetPropSep(layout, true);

  uiItemR(layout, ptr, "object", 0, nullptr, ICON_NONE);
  uiItemR(layout, ptr, "density", 0, nullptr, ICON_NONE);

  {
    uiLayout *col = uiLayoutColumn(layout, false);
    uiItemR(col, ptr, "use_fill_volume", 0, nullptr, ICON_NONE);
    uiItemR(col, ptr, "exterior_band_width", 0, nullptr, ICON_NONE);

    uiLayout *subcol = uiLayoutColumn(col, false);
    uiLayoutSetActive(subcol, !mvmd->fill_volume);
    uiItemR(subcol, ptr, "interior_band_width", 0, nullptr, ICON_NONE);
  }
  {
    uiLayout *col = uiLayoutColumn(layout, false);
    uiItemR(col, ptr, "resolution_mode", 0, nullptr, ICON_NONE);
    if (mvmd->resolution_mode == MESH_TO_VOLUME_RESOLUTION_MODE_VOXEL_AMOUNT) {
      uiItemR(col, ptr, "voxel_amount", 0, nullptr, ICON_NONE);
    }
    else {
      uiItemR(col, ptr, "voxel_size", 0, nullptr, ICON_NONE);
    }
  }

  modifier_panel_end(layout, ptr);
}

static void panelRegister(ARegionType *region_type)
{
  modifier_panel_register(region_type, eModifierType_MeshToVolume, panel_draw);
}

#ifdef WITH_OPENVDB
static float compute_voxel_size(const ModifierEvalContext *ctx,
                                const MeshToVolumeModifierData *mvmd,
                                const blender::float4x4 &transform)
{
  using namespace blender;

  float volume_simplify = BKE_volume_simplify_factor(ctx->depsgraph);
  if (volume_simplify == 0.0f) {
    return 0.0f;
  }

  if (mvmd->resolution_mode == MESH_TO_VOLUME_RESOLUTION_MODE_VOXEL_SIZE) {
    return mvmd->voxel_size / volume_simplify;
  }
  if (mvmd->voxel_amount <= 0) {
    return 0;
  }
  /* Compute the voxel size based on the desired number of voxels and the approximated bounding box
   * of the volume. */
  const BoundBox *bb = BKE_object_boundbox_get(mvmd->object);
  const float diagonal = math::distance(transform * float3(bb->vec[6]),
                                        transform * float3(bb->vec[0]));
  const float approximate_volume_side_length = diagonal + mvmd->exterior_band_width * 2.0f;
  const float voxel_size = approximate_volume_side_length / mvmd->voxel_amount / volume_simplify;
  return voxel_size;
}
#endif

static Volume *mesh_to_volume(ModifierData *md,
                              const ModifierEvalContext *ctx,
                              Volume *input_volume)
{
#ifdef WITH_OPENVDB
  using namespace blender;

  MeshToVolumeModifierData *mvmd = reinterpret_cast<MeshToVolumeModifierData *>(md);
  Object *object_to_convert = mvmd->object;

  if (object_to_convert == nullptr) {
    return input_volume;
  }
  Mesh *mesh = BKE_modifier_get_evaluated_mesh_from_evaluated_object(object_to_convert, false);
  if (mesh == nullptr) {
    return input_volume;
  }
  BKE_mesh_wrapper_ensure_mdata(mesh);

  const float4x4 mesh_to_own_object_space_transform = float4x4(ctx->object->imat) *
                                                      float4x4(object_to_convert->obmat);
  const float voxel_size = compute_voxel_size(ctx, mvmd, mesh_to_own_object_space_transform);
  if (voxel_size == 0.0f) {
    return input_volume;
  }

  float4x4 mesh_to_index_space_transform;
  scale_m4_fl(mesh_to_index_space_transform.values, 1.0f / voxel_size);
  mul_m4_m4_post(mesh_to_index_space_transform.values, mesh_to_own_object_space_transform.values);
  /* Better align generated grid with the source mesh. */
  add_v3_fl(mesh_to_index_space_transform.values[3], -0.5f);

  OpenVDBMeshAdapter mesh_adapter{*mesh, mesh_to_index_space_transform};

  /* Convert the bandwidths from object in index space. */
  const float exterior_band_width = MAX2(0.001f, mvmd->exterior_band_width / voxel_size);
  const float interior_band_width = MAX2(0.001f, mvmd->interior_band_width / voxel_size);

  openvdb::FloatGrid::Ptr new_grid;
  if (mvmd->fill_volume) {
    /* Setting the interior bandwidth to FLT_MAX, will make it fill the entire volume. */
    new_grid = openvdb::tools::meshToVolume<openvdb::FloatGrid>(
        mesh_adapter, {}, exterior_band_width, FLT_MAX);
  }
  else {
    new_grid = openvdb::tools::meshToVolume<openvdb::FloatGrid>(
        mesh_adapter, {}, exterior_band_width, interior_band_width);
  }

  /* Create a new volume object and add the density grid. */
  Volume *volume = BKE_volume_new_for_eval(input_volume);
  VolumeGrid *c_density_grid = BKE_volume_grid_add(volume, "density", VOLUME_GRID_FLOAT);
  openvdb::FloatGrid::Ptr density_grid = openvdb::gridPtrCast<openvdb::FloatGrid>(
      BKE_volume_grid_openvdb_for_write(volume, c_density_grid, false));

  /* Merge the generated grid into the density grid. Should be cheap because density_grid has just
   * been created as well. */
  density_grid->merge(*new_grid);

  /* Change transform so that the index space is correctly transformed to object space. */
  density_grid->transform().postScale(voxel_size);

  /* Give each grid cell a fixed density for now. */
  openvdb::tools::foreach (
      density_grid->beginValueOn(),
      [&](const openvdb::FloatGrid::ValueOnIter &iter) { iter.setValue(mvmd->density); });

  return volume;

#else
  UNUSED_VARS(md);
  BKE_modifier_set_error(ctx->object, md, "Compiled without OpenVDB");
  return input_volume;
#endif
}

static void modifyGeometrySet(ModifierData *md,
                              const ModifierEvalContext *ctx,
                              GeometrySet *geometry_set)
{
  Volume *input_volume = geometry_set->get_volume_for_write();
  Volume *result_volume = mesh_to_volume(md, ctx, input_volume);
  if (result_volume != input_volume) {
    geometry_set->replace_volume(result_volume);
  }
}

ModifierTypeInfo modifierType_MeshToVolume = {
    /* name */ "Mesh to Volume",
    /* structName */ "MeshToVolumeModifierData",
    /* structSize */ sizeof(MeshToVolumeModifierData),
    /* srna */ &RNA_MeshToVolumeModifier,
    /* type */ eModifierTypeType_Constructive,
    /* flags */ static_cast<ModifierTypeFlag>(0),
    /* icon */ ICON_VOLUME_DATA, /* TODO: Use correct icon. */

    /* copyData */ BKE_modifier_copydata_generic,

    /* deformVerts */ nullptr,
    /* deformMatrices */ nullptr,
    /* deformVertsEM */ nullptr,
    /* deformMatricesEM */ nullptr,
    /* modifyMesh */ nullptr,
    /* modifyGeometrySet */ modifyGeometrySet,

    /* initData */ initData,
    /* requiredDataMask */ nullptr,
    /* freeData */ nullptr,
    /* isDisabled */ nullptr,
    /* updateDepsgraph */ updateDepsgraph,
    /* dependsOnTime */ nullptr,
    /* dependsOnNormals */ nullptr,
    /* foreachIDLink */ foreachIDLink,
    /* foreachTexLink */ nullptr,
    /* freeRuntimeData */ nullptr,
    /* panelRegister */ panelRegister,
    /* blendWrite */ nullptr,
    /* blendRead */ nullptr,
};
