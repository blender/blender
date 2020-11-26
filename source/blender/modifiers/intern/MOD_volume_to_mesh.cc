/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/** \file
 * \ingroup modifiers
 */

#include <vector>

#include "BKE_lib_query.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"
#include "BKE_volume.h"

#include "MOD_modifiertypes.h"
#include "MOD_ui_common.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"
#include "DNA_volume_types.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "RNA_access.h"

#include "BLI_float4x4.hh"
#include "BLI_math_vector.h"
#include "BLI_span.hh"
#include "BLI_timeit.hh"

#include "DEG_depsgraph_query.h"

#ifdef WITH_OPENVDB
#  include <openvdb/tools/GridTransformer.h>
#  include <openvdb/tools/VolumeToMesh.h>
#endif

using blender::float3;
using blender::float4x4;
using blender::Span;

static void initData(ModifierData *md)
{
  VolumeToMeshModifierData *vmmd = reinterpret_cast<VolumeToMeshModifierData *>(md);
  vmmd->object = nullptr;
  vmmd->threshold = 0.1f;
  strncpy(vmmd->grid_name, "density", MAX_NAME);
  vmmd->adaptivity = 0.0f;
  vmmd->resolution_mode = VOLUME_TO_MESH_RESOLUTION_MODE_GRID;
  vmmd->voxel_amount = 32;
  vmmd->voxel_size = 0.1f;
  vmmd->flag = 0;
}

static void updateDepsgraph(ModifierData *md, const ModifierUpdateDepsgraphContext *ctx)
{
  VolumeToMeshModifierData *vmmd = reinterpret_cast<VolumeToMeshModifierData *>(md);
  DEG_add_modifier_to_transform_relation(ctx->node, "Volume to Mesh Modifier");
  if (vmmd->object) {
    DEG_add_object_relation(
        ctx->node, vmmd->object, DEG_OB_COMP_GEOMETRY, "Volume to Mesh Modifier");
    DEG_add_object_relation(
        ctx->node, vmmd->object, DEG_OB_COMP_TRANSFORM, "Volume to Mesh Modifier");
  }
}

static void foreachIDLink(ModifierData *md, Object *ob, IDWalkFunc walk, void *userData)
{
  VolumeToMeshModifierData *vmmd = reinterpret_cast<VolumeToMeshModifierData *>(md);
  walk(userData, ob, (ID **)&vmmd->object, IDWALK_CB_NOP);
}

static void panel_draw(const bContext *UNUSED(C), Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, nullptr);
  VolumeToMeshModifierData *vmmd = static_cast<VolumeToMeshModifierData *>(ptr->data);

  uiLayoutSetPropSep(layout, true);

  {
    uiLayout *col = uiLayoutColumn(layout, false);
    uiItemR(col, ptr, "object", 0, nullptr, ICON_NONE);
    uiItemR(col, ptr, "grid_name", 0, nullptr, ICON_NONE);
  }

  {
    uiLayout *col = uiLayoutColumn(layout, false);
    uiItemR(col, ptr, "resolution_mode", 0, nullptr, ICON_NONE);
    if (vmmd->resolution_mode == VOLUME_TO_MESH_RESOLUTION_MODE_VOXEL_AMOUNT) {
      uiItemR(col, ptr, "voxel_amount", 0, nullptr, ICON_NONE);
    }
    else if (vmmd->resolution_mode == VOLUME_TO_MESH_RESOLUTION_MODE_VOXEL_SIZE) {
      uiItemR(col, ptr, "voxel_size", 0, nullptr, ICON_NONE);
    }
  }

  {
    uiLayout *col = uiLayoutColumn(layout, false);
    uiItemR(col, ptr, "threshold", 0, nullptr, ICON_NONE);
    uiItemR(col, ptr, "adaptivity", 0, nullptr, ICON_NONE);
    uiItemR(col, ptr, "use_smooth_shade", 0, nullptr, ICON_NONE);
  }

  modifier_panel_end(layout, ptr);
}

static void panelRegister(ARegionType *region_type)
{
  modifier_panel_register(region_type, eModifierType_VolumeToMesh, panel_draw);
}

#ifdef WITH_OPENVDB

struct VolumeToMeshOp {
  const openvdb::GridBase &base_grid;
  VolumeToMeshModifierData &vmmd;
  const ModifierEvalContext &ctx;
  std::vector<openvdb::Vec3s> verts;
  std::vector<openvdb::Vec3I> tris;
  std::vector<openvdb::Vec4I> quads;

  template<typename GridType> bool operator()()
  {
    if constexpr (std::is_scalar_v<typename GridType::ValueType>) {
      this->generate_mesh_data<GridType>();
      return true;
    }
    else {
      return false;
    }
  }

  template<typename GridType> void generate_mesh_data()
  {
    /* Make a new transform from the index space into the mesh object space. */
    openvdb::math::Transform::Ptr transform = this->base_grid.transform().copy();
    transform->postMult(openvdb::Mat4d((float *)vmmd.object->obmat));
    openvdb::Mat4d imat = openvdb::Mat4d((float *)ctx.object->imat);
    /* `imat` had floating point issues and wasn't affine. */
    imat.setCol(3, openvdb::Vec4d(0, 0, 0, 1));
    transform->postMult(imat);

    /* Create a new grid with a different transform. The underlying tree is shared. */
    typename GridType::ConstPtr grid = openvdb::gridConstPtrCast<GridType>(
        this->base_grid.copyGridReplacingTransform(transform));

    if (this->vmmd.resolution_mode == VOLUME_TO_MESH_RESOLUTION_MODE_GRID) {
      this->grid_to_mesh(*grid);
      return;
    }

    const float resolution_factor = this->compute_resolution_factor(*grid);
    typename GridType::Ptr temp_grid = this->create_grid_with_changed_resolution(
        *grid, resolution_factor);
    this->grid_to_mesh(*temp_grid);
  }

  template<typename GridType>
  typename GridType::Ptr create_grid_with_changed_resolution(const GridType &old_grid,
                                                             const float resolution_factor)
  {
    BLI_assert(resolution_factor > 0.0f);

    openvdb::Mat4R xform;
    xform.setToScale(openvdb::Vec3d(resolution_factor));
    openvdb::tools::GridTransformer transformer{xform};

    typename GridType::Ptr new_grid = GridType::create();
    transformer.transformGrid<openvdb::tools::BoxSampler>(old_grid, *new_grid);
    new_grid->transform() = old_grid.transform();
    new_grid->transform().preScale(1.0f / resolution_factor);
    return new_grid;
  }

  float compute_resolution_factor(const openvdb::GridBase &grid) const
  {
    const openvdb::Vec3s voxel_size{grid.voxelSize()};
    const float current_voxel_size = std::max({voxel_size[0], voxel_size[1], voxel_size[2]});
    const float desired_voxel_size = this->compute_desired_voxel_size(grid);
    return current_voxel_size / desired_voxel_size;
  }

  float compute_desired_voxel_size(const openvdb::GridBase &grid) const
  {
    if (this->vmmd.resolution_mode == VOLUME_TO_MESH_RESOLUTION_MODE_VOXEL_SIZE) {
      return this->vmmd.voxel_size;
    }
    const openvdb::CoordBBox coord_bbox = base_grid.evalActiveVoxelBoundingBox();
    const openvdb::BBoxd bbox = grid.transform().indexToWorld(coord_bbox);
    const float max_extent = bbox.extents()[bbox.maxExtent()];
    const float voxel_size = max_extent / this->vmmd.voxel_amount;
    return voxel_size;
  }

  template<typename GridType> void grid_to_mesh(const GridType &grid)
  {
    openvdb::tools::volumeToMesh(
        grid, this->verts, this->tris, this->quads, this->vmmd.threshold, this->vmmd.adaptivity);
  }
};

static Mesh *new_mesh_from_openvdb_data(Span<openvdb::Vec3s> verts,
                                        Span<openvdb::Vec3I> tris,
                                        Span<openvdb::Vec4I> quads)
{
  const int tot_loops = 3 * tris.size() + 4 * quads.size();
  const int tot_polys = tris.size() + quads.size();

  Mesh *mesh = BKE_mesh_new_nomain(verts.size(), 0, 0, tot_loops, tot_polys);

  /* Write vertices. */
  for (const int i : verts.index_range()) {
    const blender::float3 co = blender::float3(verts[i].asV());
    copy_v3_v3(mesh->mvert[i].co, co);
  }

  /* Write triangles. */
  for (const int i : tris.index_range()) {
    mesh->mpoly[i].loopstart = 3 * i;
    mesh->mpoly[i].totloop = 3;
    for (int j = 0; j < 3; j++) {
      /* Reverse vertex order to get correct normals. */
      mesh->mloop[3 * i + j].v = tris[i][2 - j];
    }
  }

  /* Write quads. */
  const int poly_offset = tris.size();
  const int loop_offset = tris.size() * 3;
  for (const int i : quads.index_range()) {
    mesh->mpoly[poly_offset + i].loopstart = loop_offset + 4 * i;
    mesh->mpoly[poly_offset + i].totloop = 4;
    for (int j = 0; j < 4; j++) {
      /* Reverse vertex order to get correct normals. */
      mesh->mloop[loop_offset + 4 * i + j].v = quads[i][3 - j];
    }
  }

  BKE_mesh_calc_edges(mesh, false, false);
  BKE_mesh_calc_normals(mesh);
  return mesh;
}
#endif

static Mesh *create_empty_mesh(const Mesh *input_mesh)
{
  Mesh *new_mesh = BKE_mesh_new_nomain(0, 0, 0, 0, 0);
  BKE_mesh_copy_settings(new_mesh, input_mesh);
  return new_mesh;
}

static Mesh *modifyMesh(ModifierData *md, const ModifierEvalContext *ctx, Mesh *input_mesh)
{
#ifdef WITH_OPENVDB
  VolumeToMeshModifierData *vmmd = reinterpret_cast<VolumeToMeshModifierData *>(md);
  if (vmmd->object == nullptr) {
    return create_empty_mesh(input_mesh);
  }
  if (vmmd->object->type != OB_VOLUME) {
    return create_empty_mesh(input_mesh);
  }
  if (vmmd->resolution_mode == VOLUME_TO_MESH_RESOLUTION_MODE_VOXEL_SIZE &&
      vmmd->voxel_size == 0.0f) {
    return create_empty_mesh(input_mesh);
  }
  if (vmmd->resolution_mode == VOLUME_TO_MESH_RESOLUTION_MODE_VOXEL_AMOUNT &&
      vmmd->voxel_amount == 0) {
    return create_empty_mesh(input_mesh);
  }

  Volume *volume = static_cast<Volume *>(vmmd->object->data);

  BKE_volume_load(volume, DEG_get_bmain(ctx->depsgraph));
  VolumeGrid *volume_grid = BKE_volume_grid_find(volume, vmmd->grid_name);
  if (volume_grid == nullptr) {
    BKE_modifier_set_error(ctx->object, md, "Cannot find '%s' grid", vmmd->grid_name);
    return create_empty_mesh(input_mesh);
  }

  const openvdb::GridBase::ConstPtr grid = BKE_volume_grid_openvdb_for_read(volume, volume_grid);

  const VolumeGridType grid_type = BKE_volume_grid_type(volume_grid);
  VolumeToMeshOp to_mesh_op{*grid, *vmmd, *ctx};
  if (!BKE_volume_grid_type_operation(grid_type, to_mesh_op)) {
    BKE_modifier_set_error(ctx->object, md, "Expected a scalar grid");
    return create_empty_mesh(input_mesh);
  }

  Mesh *mesh = new_mesh_from_openvdb_data(to_mesh_op.verts, to_mesh_op.tris, to_mesh_op.quads);
  BKE_mesh_copy_settings(mesh, input_mesh);
  if (vmmd->flag & VOLUME_TO_MESH_USE_SMOOTH_SHADE) {
    BKE_mesh_smooth_flag_set(mesh, true);
  }
  return mesh;
#else
  UNUSED_VARS(md);
  BKE_modifier_set_error(ctx->object, md, "Compiled without OpenVDB");
  return create_empty_mesh(input_mesh);
#endif
}

ModifierTypeInfo modifierType_VolumeToMesh = {
    /* name */ "Volume to Mesh",
    /* structName */ "VolumeToMeshModifierData",
    /* structSize */ sizeof(VolumeToMeshModifierData),
    /* srna */ &RNA_VolumeToMeshModifier,
    /* type */ eModifierTypeType_Constructive,
    /* flags */ eModifierTypeFlag_AcceptsMesh,
    /* icon */ ICON_VOLUME_DATA, /* TODO: Use correct icon. */

    /* copyData */ BKE_modifier_copydata_generic,

    /* deformVerts */ nullptr,
    /* deformMatrices */ nullptr,
    /* deformVertsEM */ nullptr,
    /* deformMatricesEM */ nullptr,
    /* modifyMesh */ modifyMesh,
    /* modifyHair */ nullptr,
    /* modifyPointCloud */ nullptr,
    /* modifyVolume */ nullptr,

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
