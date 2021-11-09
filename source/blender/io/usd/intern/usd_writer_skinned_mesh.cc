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
 *
 * The Original Code is Copyright (C) 2021 NVIDIA Corporation.
 * All rights reserved.
 */
#include "usd_writer_skinned_mesh.h"
#include "usd_hierarchy_iterator.h"
#include "usd_writer_armature.h"

#include <pxr/base/gf/matrix4d.h>
#include <pxr/base/gf/matrix4f.h>
#include <pxr/usd/usdGeom/mesh.h>

#include "BKE_armature.h"
#include "BKE_mesh.h"
#include "BKE_mesh_runtime.h"
#include "BKE_modifier.h"
#include "BKE_object.h"

#include "DNA_armature_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_meta_types.h"

#include "ED_armature.h"

#include <string>

namespace blender::io::usd {

bool is_skinned_mesh(Object *obj)
{
  if (!(obj && obj->data)) {
    return false;
  }

  if (obj->type != OB_MESH) {
    return false;
  }

  return BKE_modifiers_findby_type(obj, eModifierType_Armature) != nullptr;
}

static Object *get_armature_obj(Object *obj)
{
  if (!(obj && obj->data)) {
    return nullptr;
  }

  if (obj->type != OB_MESH) {
    return nullptr;
  }

  ArmatureModifierData *mod = reinterpret_cast<ArmatureModifierData *>(
      BKE_modifiers_findby_type(obj, eModifierType_Armature));

  return mod ? mod->object : nullptr;
}

USDSkinnedMeshWriter::USDSkinnedMeshWriter(const USDExporterContext &ctx) : USDMeshWriter(ctx)
{
}

void USDSkinnedMeshWriter::do_write(HierarchyContext &context)
{
  Object *arm_obj = get_armature_obj(context.object);

  if (!arm_obj) {
    printf("WARNING: couldn't get armature object for skinned mesh %s\n",
           this->usd_export_context_.usd_path.GetString().c_str());
    return;
  }

  if (!arm_obj->data) {
    printf("WARNING: couldn't get armature object data for skinned mesh %s\n",
           this->usd_export_context_.usd_path.GetString().c_str());
    return;
  }

  /* Before writing the mesh, we set the artmature to edit mode
   * so the mesh is saved in its rest position. */

  bArmature *arm = static_cast<bArmature *>(arm_obj->data);

  bool is_edited = arm->edbo != nullptr;

  if (!is_edited) {
    ED_armature_to_edit(arm);
  }

  USDGenericMeshWriter::do_write(context);

  if (!is_edited) {
    ED_armature_edit_free(arm);
  }

  pxr::UsdStageRefPtr stage = usd_export_context_.stage;
  pxr::UsdTimeCode timecode = get_export_time_code();

  pxr::UsdPrim mesh_prim = stage->GetPrimAtPath(usd_export_context_.usd_path);

  if (!mesh_prim.IsValid()) {
    printf("WARNING: couldn't get valid mesh prim for skinned mesh %s\n",
           this->usd_export_context_.usd_path.GetString().c_str());
    return;
  }

  pxr::UsdSkelBindingAPI usd_skel_api = pxr::UsdSkelBindingAPI::Apply(mesh_prim);

  if (!usd_skel_api) {
    printf("WARNING: couldn't apply UsdSkelBindingAPI to skinned mesh prim %s\n",
           this->usd_export_context_.usd_path.GetString().c_str());
    return;
  }

  ID *arm_id = reinterpret_cast<ID *>(arm_obj->data);

  std::string skel_path = usd_export_context_.hierarchy_iterator->get_object_export_path(arm_id);

  if (skel_path.empty()) {
    printf("WARNING: couldn't get USD skeleton path for skinned mesh %s\n",
           this->usd_export_context_.usd_path.GetString().c_str());
    return;
  }

  if (strlen(usd_export_context_.export_params.root_prim_path) != 0) {
    skel_path = std::string(usd_export_context_.export_params.root_prim_path) + skel_path;
  }

  usd_skel_api.CreateSkeletonRel().SetTargets(pxr::SdfPathVector({pxr::SdfPath(skel_path)}));

  if (pxr::UsdAttribute geom_bind_attr = usd_skel_api.CreateGeomBindTransformAttr()) {
    pxr::GfMatrix4f mat_world(context.matrix_world);
    geom_bind_attr.Set(pxr::GfMatrix4d(mat_world));
  }
  else {
    printf("WARNING: couldn't create geom bind transform attribute for skinned mesh %s\n",
           this->usd_export_context_.usd_path.GetString().c_str());
  }

  std::vector<std::string> bone_names;
  USDArmatureWriter::get_armature_bone_names(arm_obj, bone_names);

  if (bone_names.empty()) {
    printf("WARNING: no armature bones for skinned mesh %s\n",
           this->usd_export_context_.usd_path.GetString().c_str());
    return;
  }

  bool needs_free = false;
  Mesh *mesh = get_export_mesh(context.object, needs_free);
  if (mesh == nullptr) {
    printf("WARNING: couldn't get Blender mesh for skinned mesh %s\n",
           this->usd_export_context_.usd_path.GetString().c_str());
    return;
  }

  write_weights(context.object, mesh, usd_skel_api, bone_names);

  if (needs_free) {
    free_export_mesh(mesh);
  }
}

void USDSkinnedMeshWriter::write_weights(const Object *ob,
                                         const Mesh *mesh,
                                         const pxr::UsdSkelBindingAPI &skel_api,
                                         const std::vector<std::string> &bone_names) const
{
  if (!(skel_api && ob && mesh && mesh->dvert && mesh->totvert > 0)) {
    return;
  }

  if (bone_names.empty()) {
    return;
  }

  std::vector<int> group_to_bone_idx;

  for (const bDeformGroup *def = (const bDeformGroup *)mesh->vertex_group_names.first; def;
       def = def->next) {

    int bone_idx = -1;
    /* For now, n-squared search is acceptable. */
    for (int i = 0; i < bone_names.size(); ++i) {
      if (bone_names[i] == def->name) {
        bone_idx = i;
        break;
      }
    }

    if (bone_idx == -1) {
      printf("WARNING: deform group %s in skinned mesh %s doesn't match any bones\n",
             def->name,
             this->usd_export_context_.usd_path.GetString().c_str());
    }

    group_to_bone_idx.push_back(bone_idx);
  }

  if (group_to_bone_idx.empty()) {
    return;
  }

  int max_totweight = 1;
  for (int i = 0; i < mesh->totvert; ++i) {
    MDeformVert &vert = mesh->dvert[i];
    if (vert.totweight > max_totweight) {
      max_totweight = vert.totweight;
    }
  }

  const int ELEM_SIZE = max_totweight;

  int num_points = mesh->totvert;

  pxr::VtArray<int> joint_indices(num_points * ELEM_SIZE, 0);
  pxr::VtArray<float> joint_weights(num_points * ELEM_SIZE, 0.0f);

  /* Current offset into the indices and weights arrays. */
  int offset = 0;

  /* Record number of out of bounds vert group indices, for error reporting. */
  int num_out_of_bounds = 0;

  for (int i = 0; i < mesh->totvert; ++i) {

    MDeformVert &vert = mesh->dvert[i];

    /* Sum of the weights, for normalizing. */
    float sum_weights = 0.0f;

    for (int j = 0; j < ELEM_SIZE; ++j, ++offset) {

      if (offset >= joint_indices.size()) {
        printf("Programmer error: out of bounds joint indices array offset.\n");
        return;
      }

      if (j >= vert.totweight) {
        continue;
      }

      int def_nr = static_cast<int>(vert.dw[j].def_nr);

      /* This out of bounds check is necessary because MDeformVert.totweight can be
       * larger than the number of bDeformGroup structs in Object.defbase. It appears to be
       * a Blender bug that can cause this scenario. */
      if (def_nr >= group_to_bone_idx.size()) {
        ++num_out_of_bounds;
        continue;
      }

      int bone_idx = group_to_bone_idx[def_nr];

      if (bone_idx == -1) {
        continue;
      }

      joint_indices[offset] = bone_idx;

      float w = vert.dw[j].weight;

      joint_weights[offset] = w;

      sum_weights += w;
    }

    if (sum_weights > .000001f) {
      /* Run over the elements again to normalize the weights. */
      float inv_sum_weights = 1.0f / sum_weights;
      offset -= ELEM_SIZE;
      for (int k = 0; k < ELEM_SIZE; ++k, ++offset) {
        joint_weights[offset] *= inv_sum_weights;
      }
    }
  }

  if (num_out_of_bounds > 0) {
    printf("WARNING: There were %d deform verts with out of bounds deform group numbers.\n",
           num_out_of_bounds);
  }

  skel_api.CreateJointIndicesPrimvar(false, ELEM_SIZE).GetAttr().Set(joint_indices);
  skel_api.CreateJointWeightsPrimvar(false, ELEM_SIZE).GetAttr().Set(joint_weights);
}

bool USDSkinnedMeshWriter::is_supported(const HierarchyContext *context) const
{
  return is_skinned_mesh(context->object) && USDGenericMeshWriter::is_supported(context);
}

bool USDSkinnedMeshWriter::check_is_animated(const HierarchyContext & /*context*/) const
{
  /* We assume that skinned meshes are never animated, as the source of
   * any animation is the mesh's bound skeleton. */
  return false;
}

}  // namespace blender::io::usd
