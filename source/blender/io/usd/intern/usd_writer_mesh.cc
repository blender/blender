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
 * The Original Code is Copyright (C) 2019 Blender Foundation.
 * All rights reserved.
 */
#include "usd_writer_mesh.h"
#include "usd_hierarchy_iterator.h"

#include <pxr/usd/usdGeom/mesh.h>
#include <pxr/usd/usdGeom/primvarsAPI.h>
#include <pxr/usd/usdShade/material.h>
#include <pxr/usd/usdShade/materialBindingAPI.h>

#include "BLI_assert.h"
#include "BLI_math_vector.h"

#include "BKE_customdata.h"
#include "BKE_lib_id.h"
#include "BKE_library.h"
#include "BKE_material.h"
#include "BKE_mesh.h"
#include "BKE_mesh_runtime.h"
#include "BKE_modifier.h"
#include "BKE_object.h"

#include "DEG_depsgraph.h"

#include "DNA_layer_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_fluidsim_types.h"
#include "DNA_object_types.h"
#include "DNA_particle_types.h"

#include <iostream>

namespace blender::io::usd {

/* TfToken objects are not cheap to construct, so we do it once. */
namespace usdtokens {
static const pxr::TfToken blenderName("userProperties:blenderName", pxr::TfToken::Immortal);
static const pxr::TfToken blenderNameNS("userProperties:blenderName:", pxr::TfToken::Immortal);
static const pxr::TfToken blenderObject("object", pxr::TfToken::Immortal);
static const pxr::TfToken blenderObjectNS("object:", pxr::TfToken::Immortal);
static const pxr::TfToken blenderData("data", pxr::TfToken::Immortal);
static const pxr::TfToken blenderDataNS("data:", pxr::TfToken::Immortal);
}  // namespace usdtokens

/* check if the mesh is a subsurf, ignoring disabled modifiers and
 * displace if it's after subsurf. */
static ModifierData *get_subsurf_modifier(Scene *scene, Object *ob)
{
  ModifierData *md = static_cast<ModifierData *>(ob->modifiers.last);

  for (; md; md = md->prev) {
    if (BKE_modifier_is_enabled(scene, md, eModifierMode_Render)) {
      continue;
    }

    if (md->type == eModifierType_Subsurf) {
      SubsurfModifierData *smd = reinterpret_cast<SubsurfModifierData *>(md);

      if (smd->subdivType == ME_CC_SUBSURF) {
        return md;
      }
    }

    /* mesh is not a subsurf. break */
    if ((md->type != eModifierType_Displace) && (md->type != eModifierType_ParticleSystem)) {
      return NULL;
    }
  }

  return NULL;
}

USDGenericMeshWriter::USDGenericMeshWriter(const USDExporterContext &ctx) : USDAbstractWriter(ctx)
{
}

bool USDGenericMeshWriter::is_supported(const HierarchyContext *context) const
{
  // TODO(makowalski) -- Check if we should be calling is_object_visible() below.
  // if (usd_export_context_.export_params.visible_objects_only) {
  // return context->is_object_visible(usd_export_context_.export_params.evaluation_mode);
  // }

  if (!usd_export_context_.export_params.visible_objects_only) {
    // We can skip the visibility test.
    return true;
  }

  Object *object = context->object;
  bool is_dupli = context->duplicator != nullptr;
  int base_flag;

  if (is_dupli) {
    /* Construct the object's base flags from its dupliparent, just like is done in
     * deg_objects_dupli_iterator_next(). Without this, the visiblity check below will fail. Doing
     * this here, instead of a more suitable location in AbstractHierarchyIterator, prevents
     * copying the Object for every dupli. */
    base_flag = object->base_flag;
    object->base_flag = context->duplicator->base_flag | BASE_FROM_DUPLI;
  }

  int visibility = BKE_object_visibility(object,
                                         usd_export_context_.export_params.evaluation_mode);

  if (is_dupli) {
    object->base_flag = base_flag;
  }

  return (visibility & OB_VISIBLE_SELF) != 0;
}

void USDGenericMeshWriter::do_write(HierarchyContext &context)
{
  Object *object_eval = context.object;

  m_subsurf_mod = get_subsurf_modifier(DEG_get_evaluated_scene(usd_export_context_.depsgraph),
                                       context.object);

  if (m_subsurf_mod && !usd_export_context_.export_params.apply_subdiv) {
    m_subsurf_mod->mode |= eModifierMode_DisableTemporary;
  }

  bool needsfree = false;
  Mesh *mesh = get_export_mesh(object_eval, needsfree);

  if (mesh == nullptr) {
    return;
  }

  try {
    write_mesh(context, mesh);

    if (needsfree) {
      free_export_mesh(mesh);
    }
  }
  catch (...) {
    if (needsfree) {
      free_export_mesh(mesh);
    }
    throw;
  }

  auto prim = usd_export_context_.stage->GetPrimAtPath(usd_export_context_.usd_path);
  if (prim.IsValid() && object_eval)
    prim.SetActive((object_eval->duplicator_visibility_flag & OB_DUPLI_FLAG_RENDER) != 0);

  if (usd_export_context_.export_params.export_custom_properties && mesh)
    write_id_properties(prim, mesh->id, get_export_time_code());

  if (m_subsurf_mod && !usd_export_context_.export_params.apply_subdiv) {
    m_subsurf_mod->mode &= ~eModifierMode_DisableTemporary;
  }
}

void USDGenericMeshWriter::free_export_mesh(Mesh *mesh)
{
  BKE_id_free(nullptr, mesh);
}

struct USDMeshData {
  pxr::VtArray<pxr::GfVec3f> points;
  pxr::VtIntArray face_vertex_counts;
  pxr::VtIntArray face_indices;
  std::map<short, pxr::VtIntArray> face_groups;

  /* The length of this array specifies the number of creases on the surface. Each element gives
   * the number of (must be adjacent) vertices in each crease, whose indices are linearly laid out
   * in the 'creaseIndices' attribute. Since each crease must be at least one edge long, each
   * element of this array should be greater than one. */
  pxr::VtIntArray crease_lengths;
  /* The indices of all vertices forming creased edges. The size of this array must be equal to the
   * sum of all elements of the 'creaseLengths' attribute. */
  pxr::VtIntArray crease_vertex_indices;
  /* The per-crease or per-edge sharpness for all creases (Usd.Mesh.SHARPNESS_INFINITE for a
   * perfectly sharp crease). Since 'creaseLengths' encodes the number of vertices in each crease,
   * the number of elements in this array will be either 'len(creaseLengths)' or the sum over all X
   * of '(creaseLengths[X] - 1)'. Note that while the RI spec allows each crease to have either a
   * single sharpness or a value per-edge, USD will encode either a single sharpness per crease on
   * a mesh, or sharpness's for all edges making up the creases on a mesh. */
  pxr::VtFloatArray crease_sharpnesses;
};

void USDGenericMeshWriter::write_custom_data(const Mesh *mesh, pxr::UsdGeomMesh usd_mesh)
{
  const CustomData *ldata = &mesh->ldata;
  for (int layer_idx = 0; layer_idx < ldata->totlayer; layer_idx++) {
    const CustomDataLayer *layer = &ldata->layers[layer_idx];
    if (layer->type == CD_MLOOPUV && usd_export_context_.export_params.export_uvmaps) {
      write_uv_maps(mesh, usd_mesh, layer);
    }
    else if (layer->type == CD_MLOOPCOL &&
             usd_export_context_.export_params.export_vertex_colors) {
      write_vertex_colors(mesh, usd_mesh, layer);
    }
  }
}

void USDGenericMeshWriter::write_uv_maps(const Mesh *mesh,
                                         pxr::UsdGeomMesh usd_mesh,
                                         const CustomDataLayer *layer)
{
  pxr::UsdTimeCode timecode = get_export_time_code();

  /* UV coordinates are stored in a Primvar on the Mesh, and can be referenced from materials.
   * The primvar name is the same as the UV Map name. This is to allow the standard name "st"
   * for texture coordinates by naming the UV Map as such, without having to guess which UV Map
   * is the "standard" one. */
  pxr::TfToken primvar_name(pxr::TfMakeValidIdentifier(layer->name));

  if (usd_export_context_.export_params.author_blender_name) {
    // Store original layer name in blender
    usd_mesh.GetPrim()
        .CreateAttribute(pxr::TfToken(usdtokens::blenderNameNS.GetString() +
                                      usdtokens::blenderDataNS.GetString() +
                                      primvar_name.GetString()),
                         pxr::SdfValueTypeNames->String,
                         true)
        .Set(std::string(layer->name), pxr::UsdTimeCode::Default());
  }

  if (usd_export_context_.export_params.convert_uv_to_st)
    primvar_name = pxr::TfToken("st");

  pxr::UsdGeomPrimvar uv_coords_primvar = usd_mesh.CreatePrimvar(
      primvar_name, pxr::SdfValueTypeNames->TexCoord2fArray, pxr::UsdGeomTokens->faceVarying);

  MLoopUV *mloopuv = static_cast<MLoopUV *>(layer->data);
  pxr::VtArray<pxr::GfVec2f> uv_coords;
  for (int loop_idx = 0; loop_idx < mesh->totloop; loop_idx++) {
    uv_coords.push_back(pxr::GfVec2f(mloopuv[loop_idx].uv));
  }

  // NOTE (Marcelo Sercheli): Code to set values at default time was removed since
  // `timecode` will be default time in case of non-animation exports. For animated
  // exports, USD will inter/extrapolate values linearly.
  const pxr::UsdAttribute &uv_coords_attr = uv_coords_primvar.GetAttr();
  usd_value_writer_.SetAttribute(uv_coords_attr, pxr::VtValue(uv_coords), timecode);
}

void USDGenericMeshWriter::write_vertex_colors(const Mesh *mesh,
                                               pxr::UsdGeomMesh usd_mesh,
                                               const CustomDataLayer *layer)
{
  pxr::UsdTimeCode timecode = get_export_time_code();
  pxr::TfToken primvar_name(pxr::TfMakeValidIdentifier(layer->name));

  const float cscale = 1.0f / 255.0f;

  if (usd_export_context_.export_params.author_blender_name) {
    // Store original layer name in blender
    usd_mesh.GetPrim()
        .CreateAttribute(pxr::TfToken(usdtokens::blenderNameNS.GetString() +
                                      usdtokens::blenderDataNS.GetString() +
                                      primvar_name.GetString()),
                         pxr::SdfValueTypeNames->String,
                         true)
        .Set(std::string(layer->name), pxr::UsdTimeCode::Default());
  }

  pxr::UsdGeomPrimvarsAPI pvApi = pxr::UsdGeomPrimvarsAPI(usd_mesh);

  // TODO: Allow option of vertex varying primvar
  pxr::UsdGeomPrimvar vertex_colors_pv = pvApi.CreatePrimvar(
      primvar_name, pxr::SdfValueTypeNames->Color3fArray, pxr::UsdGeomTokens->faceVarying);

  MCol *vertCol = static_cast<MCol *>(layer->data);
  pxr::VtArray<pxr::GfVec3f> vertex_colors;

  for (int loop_idx = 0; loop_idx < mesh->totloop; loop_idx++) {
    pxr::GfVec3f col = pxr::GfVec3f((int)vertCol[loop_idx].b * cscale,
                                    (int)vertCol[loop_idx].g * cscale,
                                    (int)vertCol[loop_idx].r * cscale);
    vertex_colors.push_back(col);
  }

  vertex_colors_pv.Set(vertex_colors, timecode);

  const pxr::UsdAttribute &vertex_colors_attr = vertex_colors_pv.GetAttr();
  usd_value_writer_.SetAttribute(vertex_colors_attr, pxr::VtValue(vertex_colors), timecode);
}

void USDGenericMeshWriter::write_vertex_groups(const Object *ob,
                                               const Mesh *mesh,
                                               pxr::UsdGeomMesh usd_mesh,
                                               bool as_point_groups)
{
  if (!ob)
    return;

  pxr::UsdTimeCode timecode = get_export_time_code();

  int i, j;
  bDeformGroup *def;
  std::vector<pxr::UsdGeomPrimvar> pv_groups;
  std::vector<pxr::VtArray<float>> pv_data;

  // Create vertex groups primvars
  for (def = (bDeformGroup *)ob->defbase.first, i = 0, j = 0; def; def = def->next, i++) {
    if (!def)
      continue;
    pxr::TfToken primvar_name(pxr::TfMakeValidIdentifier(def->name));
    pxr::TfToken primvar_interpolation = (as_point_groups) ? pxr::UsdGeomTokens->vertex :
                                                             pxr::UsdGeomTokens->faceVarying;
    pv_groups.push_back(usd_mesh.CreatePrimvar(
        primvar_name, pxr::SdfValueTypeNames->FloatArray, primvar_interpolation));

    size_t primvar_size = 0;

    if (as_point_groups) {
      primvar_size = mesh->totvert;
    }
    else {
      MPoly *mpoly = mesh->mpoly;
      for (int poly_idx = 0, totpoly = mesh->totpoly; poly_idx < totpoly; ++poly_idx, ++mpoly) {
        primvar_size += mpoly->totloop;
      }
    }
    pv_data.push_back(pxr::VtArray<float>(primvar_size));
  }

  size_t num_groups = pv_groups.size();

  if (num_groups == 0)
    return;

  // Extract vertex groups
  if (as_point_groups) {
    for (i = 0; i < mesh->totvert; i++) {
      // Init to zero
      for (j = 0; j < num_groups; j++) {
        pv_data[j][i] = 0.0f;
      }

      MDeformVert *vert = &mesh->dvert[i];
      if (vert) {
        for (j = 0; j < vert->totweight; j++) {
          uint idx = vert->dw[j].def_nr;
          float w = vert->dw[j].weight;
          /* This out of bounds check is necessary because MDeformVert.totweight can be
          larger than the number of bDeformGroup structs in Object.defbase. It appears to be
          a Blender bug that can cause this scenario.*/
          if (idx < num_groups) {
            pv_data[idx][i] = w;
          }
        }
      }
    }
  }
  else {
    MPoly *mpoly = mesh->mpoly;
    for (i = 0; i < mesh->totvert; i++) {
      // Init to zero
      for (j = 0; j < num_groups; j++) {
        pv_data[j][i] = 0.0f;
      }
    }
    // const MVert *mvert = mesh->mvert;
    int p_idx = 0;
    for (int poly_idx = 0, totpoly = mesh->totpoly; poly_idx < totpoly; ++poly_idx, ++mpoly) {
      MLoop *mloop = mesh->mloop + mpoly->loopstart;
      for (int loop_idx = 0; loop_idx < mpoly->totloop; ++loop_idx, ++mloop) {
        MDeformVert *vert = &mesh->dvert[mloop->v];

        if (vert) {
          for (j = 0; j < vert->totweight; j++) {
            uint idx = vert->dw[j].def_nr;
            float w = vert->dw[j].weight;
            /* This out of bounds check is necessary because MDeformVert.totweight can be
            larger than the number of bDeformGroup structs in Object.defbase. Appears to be
            a Blender bug that can cause this scenario.*/
            if (idx < num_groups) {
              pv_data[idx][p_idx] = w;
            }
          }
        }
        p_idx++;
      }
    }
  }

  // Store data in usd
  for (i = 0; i < num_groups; i++) {
    pv_groups[i].Set(pv_data[i], timecode);

    const pxr::UsdAttribute &vertex_colors_attr = pv_groups[i].GetAttr();
    usd_value_writer_.SetAttribute(vertex_colors_attr, pxr::VtValue(pv_data[i]), timecode);
  }
}

void USDGenericMeshWriter::write_face_maps(const Object *ob,
                                           const Mesh *mesh,
                                           pxr::UsdGeomMesh usd_mesh)
{
  if (!ob)
    return;

  pxr::UsdTimeCode timecode = get_export_time_code();

  std::vector<pxr::UsdGeomPrimvar> pv_groups;
  std::vector<pxr::VtArray<float>> pv_data;

  int i;
  size_t mpoly_len = mesh->totpoly;

  for (bFaceMap *fmap = (bFaceMap *)ob->fmaps.first; fmap; fmap = fmap->next) {
    if (!fmap)
      continue;
    pxr::TfToken primvar_name(pxr::TfMakeValidIdentifier(fmap->name));
    pxr::TfToken primvar_interpolation = pxr::UsdGeomTokens->uniform;
    pv_groups.push_back(usd_mesh.CreatePrimvar(
        primvar_name, pxr::SdfValueTypeNames->FloatArray, primvar_interpolation));

    pv_data.push_back(pxr::VtArray<float>(mpoly_len));

    // Init data
    for (i = 0; i < mpoly_len; i++) {
      pv_data[pv_data.size() - 1][i] = 0.0f;
    }
  }

  size_t num_groups = pv_groups.size();

  if (num_groups == 0)
    return;

  const int *facemap_data = (int *)CustomData_get_layer(&mesh->pdata, CD_FACEMAP);

  if (facemap_data) {
    for (i = 0; i < mpoly_len; i++) {
      if (facemap_data[i] >= 0) {
        pv_data[facemap_data[i]][i] = 1.0f;
      }
    }
  }

  // Store data in usd
  for (i = 0; i < num_groups; i++) {
    pv_groups[i].Set(pv_data[i], timecode);

    const pxr::UsdAttribute &vertex_colors_attr = pv_groups[i].GetAttr();
    usd_value_writer_.SetAttribute(vertex_colors_attr, pxr::VtValue(pv_data[i]), timecode);
  }
}

void USDGenericMeshWriter::write_mesh(HierarchyContext &context, Mesh *mesh)
{
  pxr::UsdTimeCode timecode = get_export_time_code();
  pxr::UsdStageRefPtr stage = usd_export_context_.stage;

  pxr::UsdGeomMesh usd_mesh =
      (usd_export_context_.export_params.export_as_overs) ?
          pxr::UsdGeomMesh(usd_export_context_.stage->OverridePrim(usd_export_context_.usd_path)) :
          pxr::UsdGeomMesh::Define(usd_export_context_.stage, usd_export_context_.usd_path);

  write_visibility(context, timecode, usd_mesh);

  USDMeshData usd_mesh_data;
  get_geometry_data(mesh, usd_mesh_data);

  if (usd_export_context_.export_params.export_vertices) {
    pxr::UsdAttribute attr_points = usd_mesh.CreatePointsAttr(pxr::VtValue(), true);
    pxr::UsdAttribute attr_face_vertex_counts = usd_mesh.CreateFaceVertexCountsAttr(pxr::VtValue(),
                                                                                    true);

    pxr::UsdAttribute attr_face_vertex_indices = usd_mesh.CreateFaceVertexIndicesAttr(
        pxr::VtValue(), true);

    // NOTE (Marcelo Sercheli): Code to set values at default time was removed since
    // `timecode` will be default time in case of non-animation exports. For animated
    // exports, USD will inter/extrapolate values linearly.
    usd_value_writer_.SetAttribute(attr_points, pxr::VtValue(usd_mesh_data.points), timecode);
    usd_value_writer_.SetAttribute(
        attr_face_vertex_counts, pxr::VtValue(usd_mesh_data.face_vertex_counts), timecode);
    usd_value_writer_.SetAttribute(
        attr_face_vertex_indices, pxr::VtValue(usd_mesh_data.face_indices), timecode);

    if (!usd_mesh_data.crease_lengths.empty()) {
      pxr::UsdAttribute attr_crease_lengths = usd_mesh.CreateCreaseLengthsAttr(pxr::VtValue(),
                                                                               true);
      pxr::UsdAttribute attr_crease_indices = usd_mesh.CreateCreaseIndicesAttr(pxr::VtValue(),
                                                                               true);
      pxr::UsdAttribute attr_crease_sharpness = usd_mesh.CreateCreaseSharpnessesAttr(
          pxr::VtValue(), true);

      // NOTE (Marcelo Sercheli): Code to set values at default time was removed since
      // `timecode` will be default time in case of non-animation exports. For animated
      // exports, USD will inter/extrapolate values linearly.
      usd_value_writer_.SetAttribute(
          attr_crease_lengths, pxr::VtValue(usd_mesh_data.crease_lengths), timecode);
      usd_value_writer_.SetAttribute(
          attr_crease_indices, pxr::VtValue(usd_mesh_data.crease_vertex_indices), timecode);
      usd_value_writer_.SetAttribute(
          attr_crease_sharpness, pxr::VtValue(usd_mesh_data.crease_sharpnesses), timecode);
    }
  }
  write_custom_data(mesh, usd_mesh);

  if (usd_export_context_.export_params.export_vertex_groups) {
    write_vertex_groups(context.object,
                        mesh,
                        usd_mesh,
                        !usd_export_context_.export_params.vertex_data_as_face_varying);
    write_face_maps(context.object, mesh, usd_mesh);
  }

  if (usd_export_context_.export_params.export_normals) {
    write_normals(mesh, usd_mesh);
  }
  write_surface_velocity(context.object, mesh, usd_mesh);

  /* TODO(Sybren): figure out what happens when the face groups change. */
  if (frame_has_been_written_) {
    return;
  }

  if (usd_export_context_.export_params.export_vertices) {
    usd_mesh.CreateSubdivisionSchemeAttr().Set(
        (m_subsurf_mod == NULL) ? pxr::UsdGeomTokens->none : pxr::UsdGeomTokens->catmullClark);
  }

  if (usd_export_context_.export_params.export_materials) {
    assign_materials(context, usd_mesh, usd_mesh_data.face_groups);
  }
}

static void get_vertices(const Mesh *mesh, USDMeshData &usd_mesh_data)
{
  usd_mesh_data.points.reserve(mesh->totvert);

  const MVert *verts = mesh->mvert;
  for (int i = 0; i < mesh->totvert; ++i) {
    usd_mesh_data.points.push_back(pxr::GfVec3f(verts[i].co));
  }
}

static void get_loops_polys(const Mesh *mesh, USDMeshData &usd_mesh_data)
{
  /* Only construct face groups (a.k.a. geometry subsets) when we need them for material
   * assignments. */
  bool construct_face_groups = mesh->totcol > 1;

  usd_mesh_data.face_vertex_counts.reserve(mesh->totpoly);
  usd_mesh_data.face_indices.reserve(mesh->totloop);

  MLoop *mloop = mesh->mloop;
  MPoly *mpoly = mesh->mpoly;
  for (int i = 0; i < mesh->totpoly; ++i, ++mpoly) {
    MLoop *loop = mloop + mpoly->loopstart;
    usd_mesh_data.face_vertex_counts.push_back(mpoly->totloop);
    for (int j = 0; j < mpoly->totloop; ++j, ++loop) {
      usd_mesh_data.face_indices.push_back(loop->v);
    }

    if (construct_face_groups) {
      usd_mesh_data.face_groups[mpoly->mat_nr].push_back(i);
    }
  }
}

static void get_creases(const Mesh *mesh, USDMeshData &usd_mesh_data)
{
  const float factor = 1.0f / 255.0f;

  MEdge *edge = mesh->medge;
  float sharpness;
  for (int edge_idx = 0, totedge = mesh->totedge; edge_idx < totedge; ++edge_idx, ++edge) {
    if (edge->crease == 0) {
      continue;
    }

    if (edge->crease == 255) {
      sharpness = pxr::UsdGeomMesh::SHARPNESS_INFINITE;
    }
    else {
      sharpness = static_cast<float>(edge->crease) * factor;
    }

    usd_mesh_data.crease_vertex_indices.push_back(edge->v1);
    usd_mesh_data.crease_vertex_indices.push_back(edge->v2);
    usd_mesh_data.crease_lengths.push_back(2);
    usd_mesh_data.crease_sharpnesses.push_back(sharpness);
  }
}

void USDGenericMeshWriter::get_geometry_data(const Mesh *mesh, USDMeshData &usd_mesh_data)
{
  get_vertices(mesh, usd_mesh_data);
  get_loops_polys(mesh, usd_mesh_data);
  get_creases(mesh, usd_mesh_data);
}

void USDGenericMeshWriter::assign_materials(const HierarchyContext &context,
                                            pxr::UsdGeomMesh usd_mesh,
                                            const MaterialFaceGroups &usd_face_groups)
{
  if (context.object->totcol == 0) {
    return;
  }

  /* Binding a material to a geometry subset isn't supported by the Hydra GL viewport yet,
   * which is why we always bind the first material to the entire mesh. See
   * https://github.com/PixarAnimationStudios/USD/issues/542 for more info. */
  bool mesh_material_bound = false;
  pxr::UsdShadeMaterialBindingAPI material_binding_api(usd_mesh.GetPrim());
  for (int mat_num = 0; mat_num < context.object->totcol; mat_num++) {
    Material *material = BKE_object_material_get(context.object, mat_num + 1);
    if (material == nullptr) {
      continue;
    }

    pxr::UsdShadeMaterialBindingAPI api = pxr::UsdShadeMaterialBindingAPI(usd_mesh.GetPrim());
    pxr::UsdShadeMaterial usd_material = ensure_usd_material(material, context);
    api.Bind(usd_material);

    /* USD seems to support neither per-material nor per-face-group double-sidedness, so we just
     * use the flag from the first non-empty material slot. */
    usd_mesh.CreateDoubleSidedAttr(
        pxr::VtValue((material->blend_flag & MA_BL_CULL_BACKFACE) == 0));

    mesh_material_bound = true;
    break;
  }

  if (!mesh_material_bound) {
    /* Blender defaults to double-sided, but USD to single-sided. */
    usd_mesh.CreateDoubleSidedAttr(pxr::VtValue(true));
  }

  if (!mesh_material_bound || usd_face_groups.size() < 2) {
    /* Either all material slots were empty or there is only one material in use. As geometry
     * subsets are only written when actually used to assign a material, and the mesh already has
     * the material assigned, there is no need to continue. */
    return;
  }

  /* Define a geometry subset per material. */
  for (const MaterialFaceGroups::value_type &face_group : usd_face_groups) {
    short material_number = face_group.first;
    const pxr::VtIntArray &face_indices = face_group.second;

    Material *material = BKE_object_material_get(context.object, material_number + 1);
    if (material == nullptr) {
      continue;
    }

    pxr::UsdShadeMaterial usd_material = ensure_usd_material(material, context);
    pxr::TfToken material_name = usd_material.GetPath().GetNameToken();

    pxr::UsdShadeMaterialBindingAPI api = pxr::UsdShadeMaterialBindingAPI(usd_mesh.GetPrim());
    pxr::UsdGeomSubset usd_face_subset = api.CreateMaterialBindSubset(material_name, face_indices);
    pxr::UsdShadeMaterialBindingAPI(usd_face_subset.GetPrim()).Bind(usd_material);
  }
}

void USDGenericMeshWriter::write_normals(const Mesh *mesh, pxr::UsdGeomMesh usd_mesh)
{
  pxr::UsdTimeCode timecode = get_export_time_code();
  const float(*lnors)[3] = static_cast<float(*)[3]>(CustomData_get_layer(&mesh->ldata, CD_NORMAL));

  pxr::VtVec3fArray loop_normals;
  loop_normals.reserve(mesh->totloop);

  if (lnors != nullptr) {
    /* Export custom loop normals. */
    for (int loop_idx = 0, totloop = mesh->totloop; loop_idx < totloop; ++loop_idx) {
      loop_normals.push_back(pxr::GfVec3f(lnors[loop_idx]));
    }
  }
  else {
    /* Compute the loop normals based on the 'smooth' flag. */
    float normal[3];
    MPoly *mpoly = mesh->mpoly;
    const MVert *mvert = mesh->mvert;
    for (int poly_idx = 0, totpoly = mesh->totpoly; poly_idx < totpoly; ++poly_idx, ++mpoly) {
      MLoop *mloop = mesh->mloop + mpoly->loopstart;

      if ((mpoly->flag & ME_SMOOTH) == 0) {
        /* Flat shaded, use common normal for all verts. */
        BKE_mesh_calc_poly_normal(mpoly, mloop, mvert, normal);
        pxr::GfVec3f pxr_normal(normal);
        for (int loop_idx = 0; loop_idx < mpoly->totloop; ++loop_idx) {
          loop_normals.push_back(pxr_normal);
        }
      }
      else {
        /* Smooth shaded, use individual vert normals. */
        for (int loop_idx = 0; loop_idx < mpoly->totloop; ++loop_idx, ++mloop) {
          normal_short_to_float_v3(normal, mvert[mloop->v].no);
          loop_normals.push_back(pxr::GfVec3f(normal));
        }
      }
    }
  }

  pxr::UsdAttribute attr_normals = usd_mesh.CreateNormalsAttr(pxr::VtValue(), true);

  // NOTE (Marcelo Sercheli): Code to set values at default time was removed since
  // `timecode` will be default time in case of non-animation exports. For animated
  // exports, USD will inter/extrapolate values linearly.
  usd_value_writer_.SetAttribute(attr_normals, pxr::VtValue(loop_normals), timecode);
  usd_mesh.SetNormalsInterpolation(pxr::UsdGeomTokens->faceVarying);
}

void USDGenericMeshWriter::write_surface_velocity(Object *object,
                                                  const Mesh *mesh,
                                                  pxr::UsdGeomMesh usd_mesh)
{
  /* Only velocities from the fluid simulation are exported. This is the most important case,
   * though, as the baked mesh changes topology all the time, and thus computing the velocities
   * at import time in a post-processing step is hard. */
  ModifierData *md = BKE_modifiers_findby_type(object, eModifierType_Fluidsim);
  if (md == nullptr) {
    return;
  }

  /* Check that the fluid sim modifier is enabled and has useful data. */
  const bool use_render = (DEG_get_mode(usd_export_context_.depsgraph) == DAG_EVAL_RENDER);
  const ModifierMode required_mode = use_render ? eModifierMode_Render : eModifierMode_Realtime;
  const Scene *scene = DEG_get_evaluated_scene(usd_export_context_.depsgraph);
  if (!BKE_modifier_is_enabled(scene, md, required_mode)) {
    return;
  }
  FluidsimModifierData *fsmd = reinterpret_cast<FluidsimModifierData *>(md);
  if (!fsmd->fss || fsmd->fss->type != OB_FLUIDSIM_DOMAIN) {
    return;
  }
  FluidsimSettings *fss = fsmd->fss;
  if (!fss->meshVelocities) {
    return;
  }

  /* Export per-vertex velocity vectors. */
  pxr::VtVec3fArray usd_velocities;
  usd_velocities.reserve(mesh->totvert);

  FluidVertexVelocity *mesh_velocities = fss->meshVelocities;
  for (int vertex_idx = 0, totvert = mesh->totvert; vertex_idx < totvert;
       ++vertex_idx, ++mesh_velocities) {
    usd_velocities.push_back(pxr::GfVec3f(mesh_velocities->vel));
  }

  pxr::UsdTimeCode timecode = get_export_time_code();
  usd_mesh.CreateVelocitiesAttr().Set(usd_velocities, timecode);
}

USDMeshWriter::USDMeshWriter(const USDExporterContext &ctx) : USDGenericMeshWriter(ctx)
{
}

Mesh *USDMeshWriter::get_export_mesh(Object *object_eval, bool & /*r_needsfree*/)
{
  Scene *scene = DEG_get_evaluated_scene(usd_export_context_.depsgraph);
  // Assumed safe because the original depsgraph was nonconst in usd_capi...
  Depsgraph *dg = const_cast<Depsgraph *>(usd_export_context_.depsgraph);
  return mesh_get_eval_final(dg, scene, object_eval, &CD_MASK_MESH);
}

}  // namespace blender::io::usd
