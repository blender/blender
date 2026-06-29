/* SPDX-FileCopyrightText: 2011-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "curves.hh"

#include <pxr/imaging/hd/basisCurvesSchema.h>
#include <pxr/imaging/hd/basisCurvesTopologySchema.h>
#include <pxr/imaging/hd/materialBindingSchema.h>
#include <pxr/imaging/hd/materialBindingsSchema.h>
#include <pxr/imaging/hd/primvarSchema.h>
#include <pxr/imaging/hd/primvarsSchema.h>
#include <pxr/imaging/hd/retainedDataSource.h>
#include <pxr/imaging/hd/tokens.h>

#include "BLI_listbase_iterator.hh"
#include "BLI_math_matrix_c.hh"
#include "BLI_string.hh"

#include "BKE_attribute.hh"
#include "BKE_curves.hh"
#include "BKE_customdata.hh"
#include "BKE_material.hh"
#include "BKE_particle.h"

#include "DEG_depsgraph_query.hh"

#include "DNA_curves_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_particle_types.h"

#include "populate_context.hh"
#include "util.hh"

namespace blender::io::hydra {

static const pxr::TfToken usd_st_token("st", pxr::TfToken::Immortal);

struct CurvesBuilderResult {
  pxr::VtIntArray curve_vertex_counts;
  pxr::VtVec3fArray points;
  pxr::VtFloatArray widths;
  pxr::VtVec2fArray uvs;
};

static CurvesBuilderResult build_curves_data(const Object *object)
{
  CurvesBuilderResult r;
  const Curves *curves_id = id_cast<const Curves *>(object->data);
  const bke::CurvesGeometry &curves = curves_id->geometry.wrap();

  r.curve_vertex_counts.resize(curves.curves_num());
  offset_indices::copy_group_sizes(
      curves.points_by_curve(),
      curves.curves_range(),
      MutableSpan(r.curve_vertex_counts.data(), r.curve_vertex_counts.size()));

  const Span<float3> positions = curves.positions();
  resize_uninitialized(r.points, positions.size());
  for (const int i : positions.index_range()) {
    const float3 &p = positions[i];
    r.points[i] = pxr::GfVec3f(p.x, p.y, p.z);
  }

  const VArray<float> radii = *curves.attributes().lookup_or_default<float>(
      "radius", bke::AttrDomain::Point, 0.01f);
  resize_uninitialized(r.widths, curves.points_num());
  for (const int i : curves.points_range()) {
    r.widths[i] = radii[i] * 2.0f;
  }

  const std::optional<Span<float2>> surface_uv_coords = curves.surface_uv_coords();
  if (surface_uv_coords) {
    const int total = curves.curves_num();
    const int n = std::min<int>(surface_uv_coords->size(), total);
    resize_uninitialized(r.uvs, total);
    for (const int i : IndexRange(n)) {
      const float2 &uv = (*surface_uv_coords)[i];
      r.uvs[i] = pxr::GfVec2f(uv.x, uv.y);
    }
    for (const int i : IndexRange(n, total - n)) {
      r.uvs[i] = pxr::GfVec2f(0.0f, 0.0f);
    }
  }
  return r;
}

/* Build the geometry-only schema sources for one basisCurves prim. */
static EmittedGeometryPrim build_curves_geometry(const CurvesBuilderResult &r,
                                                 const pxr::SdfPath &material_path)
{
  EmittedGeometryPrim out;
  out.schema_token = pxr::HdBasisCurvesSchema::GetSchemaToken();

  pxr::HdContainerDataSourceHandle topology =
      pxr::HdBasisCurvesTopologySchema::Builder()
          .SetCurveVertexCounts(
              pxr::HdRetainedTypedSampledDataSource<pxr::VtIntArray>::New(r.curve_vertex_counts))
          .SetBasis(pxr::HdRetainedTypedSampledDataSource<pxr::TfToken>::New(pxr::TfToken()))
          .SetType(pxr::HdRetainedTypedSampledDataSource<pxr::TfToken>::New(pxr::HdTokens->linear))
          .SetWrap(
              pxr::HdRetainedTypedSampledDataSource<pxr::TfToken>::New(pxr::HdTokens->nonperiodic))
          .Build();
  out.geometry = pxr::HdBasisCurvesSchema::Builder().SetTopology(topology).Build();

  /* Primvars: points (vertex), widths (vertex), st (uniform). */
  HdContainerBuilder primvars;

  if (!r.points.empty()) {
    primvars.add(
        pxr::HdPrimvarsSchemaTokens->points,
        pxr::HdPrimvarSchema::Builder()
            .SetPrimvarValue(
                pxr::HdRetainedTypedSampledDataSource<pxr::VtVec3fArray>::New(r.points))
            .SetInterpolation(pxr::HdPrimvarSchema::BuildInterpolationDataSource(
                pxr::HdPrimvarSchemaTokens->vertex))
            .SetRole(pxr::HdPrimvarSchema::BuildRoleDataSource(pxr::HdPrimvarSchemaTokens->point))
            .Build());
  }
  if (!r.widths.empty()) {
    primvars.add(pxr::HdPrimvarsSchemaTokens->widths,
                 pxr::HdPrimvarSchema::Builder()
                     .SetPrimvarValue(
                         pxr::HdRetainedTypedSampledDataSource<pxr::VtFloatArray>::New(r.widths))
                     .SetInterpolation(pxr::HdPrimvarSchema::BuildInterpolationDataSource(
                         pxr::HdPrimvarSchemaTokens->vertex))
                     .Build());
  }
  if (!r.uvs.empty()) {
    primvars.add(
        usd_st_token,
        pxr::HdPrimvarSchema::Builder()
            .SetPrimvarValue(pxr::HdRetainedTypedSampledDataSource<pxr::VtVec2fArray>::New(r.uvs))
            .SetInterpolation(pxr::HdPrimvarSchema::BuildInterpolationDataSource(
                pxr::HdPrimvarSchemaTokens->uniform))
            .SetRole(pxr::HdPrimvarSchema::BuildRoleDataSource(
                pxr::HdPrimvarSchemaTokens->textureCoordinate))
            .Build());
  }
  out.primvars = pxr::HdPrimvarsSchema::BuildRetained(
      primvars.names.size(), primvars.names.data(), primvars.values.data());

  if (!material_path.IsEmpty()) {
    pxr::HdContainerDataSourceHandle binding =
        pxr::HdMaterialBindingSchema::Builder()
            .SetPath(pxr::HdRetainedTypedSampledDataSource<pxr::SdfPath>::New(material_path))
            .Build();
    out.bindings = pxr::HdRetainedContainerDataSource::New(
        pxr::HdMaterialBindingsSchemaTokens->allPurpose, binding);
  }
  return out;
}

/* Resolve a single-slot material binding for curves and hair. */
struct CurvesMaterial {
  pxr::SdfPath path;
  const Material *material;
};
static CurvesMaterial resolve_curves_material(PopulateContext &ctx, Object *object, const int slot)
{
  const int mat_count = BKE_object_material_count_eval(object);
  const Material *material = (mat_count > 0) ? BKE_object_material_get_eval(
                                                   object, std::clamp(slot, 1, mat_count)) :
                                               nullptr;
  const EmittedMaterial *entry = ctx.get_or_create_material(material);
  return {entry ? entry->path : pxr::SdfPath(), material};
}

/* Look up or build the geometry-only data source for a curves object. */
static const EmittedGeometry &get_or_build_emitted_curves(PopulateContext &ctx,
                                                          const BObjectInfo &info,
                                                          const EmittedGeometryKey &key)
{
  ctx.used_emitted_geometry.add(key);

  if (!info.is_real_object_data() && info.object_data) {
    ctx.instance_geometries_by_object.lookup_or_add_default(info.real_object)
        .append_non_duplicates(info.object_data);
    ctx.used_instance_sources.add(info.real_object);
  }

  if (EmittedGeometry *cached = ctx.emitted_geometry.lookup_ptr(key)) {
    for (const Material *mat : cached->materials) {
      ctx.get_or_create_material(mat);
    }
    return *cached;
  }

  EmittedGeometry &entry = ctx.emitted_geometry.lookup_or_add_default(key);
  CurvesMaterial mat = resolve_curves_material(ctx, info.real_object, 1);
  CurvesBuilderResult curves = build_curves_data(info.real_object);
  entry.prims.append(build_curves_geometry(curves, mat.path));
  entry.materials.append(mat.material);
  return entry;
}

void emit_curves_object(PopulateContext &ctx, const BObjectInfo &info, EmittedObject &emitted)
{
  /* TODO: Using only first material. Add support for multi-material. */
  const EmittedGeometryKey key = ctx.emitted_geometry_key(info,
                                                          pxr::HdPrimTypeTokens->basisCurves);
  const EmittedGeometry &cached = get_or_build_emitted_curves(ctx, info, key);
  if (cached.prims.is_empty()) {
    return;
  }
  emitted.geometry_keys.append_non_duplicates(key);
  for (const Material *m : cached.materials) {
    emitted.materials.append_non_duplicates(m);
  }
  const Object *object = info.iter_object;
  const pxr::SdfPath path = ctx.object_prim_id(object);
  pxr::HdContainerDataSourceHandle prim_ds = compose_gprim_data_source(
      cached.prims[0], gf_matrix_from_transform(object->object_to_world().ptr()), true);
  ctx.emit_object_prim(emitted, path, pxr::HdPrimTypeTokens->basisCurves, prim_ds);
}

void emit_curves_proto(PopulateContext &ctx, const BObjectInfo &info)
{
  const EmittedGeometryKey key = ctx.emitted_geometry_key(info,
                                                          pxr::HdPrimTypeTokens->basisCurves);
  const EmittedGeometry &cached = get_or_build_emitted_curves(ctx, info, key);
  if (cached.prims.is_empty()) {
    return;
  }
  const Object *source = info.real_object;
  const pxr::SdfPath proto_root = ctx.instancer_proto_root(source);
  const pxr::SdfPath proto_path = proto_root.AppendElementString("Curves");
  pxr::HdContainerDataSourceHandle prim_ds = compose_gprim_data_source(
      cached.prims[0], pxr::GfMatrix4d(1.0), true, ctx.proto_instanced_by(source));

  ctx.emit_prim(proto_path, pxr::HdPrimTypeTokens->basisCurves, prim_ds);
  ctx.all_proto_paths.append(proto_path);
  ctx.per_proto_indices.append(pxr::VtIntArray());
}

static CurvesBuilderResult build_hair_data(Object *object, ParticleSystem *psys)
{
  CurvesBuilderResult r;
  ParticleCacheKey **cache = psys->pathcache;
  if (!cache) {
    return r;
  }
  r.curve_vertex_counts.reserve(psys->totpart);

  const float scale = psys->part->rad_scale;
  const float root = scale * psys->part->rad_root;
  const float tip = scale * psys->part->rad_tip;
  const float shape = psys->part->shape;

  const bool has_particles = (psys->particles != nullptr);
  const ParticleSystemModifierData *psmd = has_particles ? psys_get_modifier(object, psys) :
                                                           nullptr;
  const bool emit_uvs = psmd && psmd->mesh_final &&
                        ELEM(psys->part->from, PART_FROM_FACE, PART_FROM_VOLUME);
  const MFace *mfaces = nullptr;
  const MTFace *mtfaces = nullptr;
  if (emit_uvs) {
    mfaces = static_cast<const MFace *>(
        CustomData_get_layer(&psmd->mesh_final->fdata_legacy, CD_MFACE));
    mtfaces = static_cast<const MTFace *>(
        CustomData_get_layer(&psmd->mesh_final->fdata_legacy, CD_MTFACE));
  }
  if (has_particles) {
    r.uvs.reserve(psys->totpart);
  }

  /* The pathcache stores positions in world space, transform to local
   * so we can use a regular object transform with it. */
  for (int pa_index = 0; pa_index < psys->totpart; ++pa_index) {
    ParticleCacheKey *strand = cache[pa_index];
    const int point_count = strand->segments + 1;
    r.curve_vertex_counts.push_back(point_count);

    for (int point_index = 0; point_index < point_count; ++point_index, ++strand) {
      float local_co[3];
      mul_v3_m4v3(local_co, psys->imat, strand->co);
      r.points.push_back(pxr::GfVec3f(local_co));
      const float x = float(point_index) / float(point_count - 1);
      const float radius = std::pow(x, std::pow(10.0f, -shape));
      r.widths.push_back(root + (tip - root) * radius);
    }

    if (psys->part->shape_flag & PART_SHAPE_CLOSE_TIP) {
      r.widths[r.widths.size() - 1] = 0.0f;
    }

    if (has_particles) {
      const ParticleData &pa = psys->particles[pa_index];
      const int num = ELEM(pa.num_dmcache, DMCACHE_ISCHILD, DMCACHE_NOTFOUND) ? pa.num :
                                                                                pa.num_dmcache;
      float uv[2] = {0.0f, 0.0f};
      if (emit_uvs && mfaces && mtfaces && !ELEM(num, DMCACHE_NOTFOUND, DMCACHE_ISCHILD)) {
        psys_interpolate_uvs(mtfaces + num, mfaces->v4, pa.fuv, uv);
      }
      r.uvs.push_back(pxr::GfVec2f(uv[0], uv[1]));
    }
  }
  return r;
}

void emit_hair_for_object(PopulateContext &ctx, Object *object, EmittedObject &emitted)
{
  const bool for_render = (DEG_get_mode(ctx.depsgraph) == DAG_EVAL_RENDER);
  for (ParticleSystem &psys : object->particlesystem) {
    if (!psys.part || psys.part->type != PART_HAIR) {
      continue;
    }
    if (!psys_check_enabled(object, &psys, for_render)) {
      continue;
    }
    if (psys_in_edit_mode(ctx.depsgraph, &psys)) {
      continue;
    }
    CurvesBuilderResult hair = build_hair_data(object, &psys);
    if (hair.curve_vertex_counts.empty()) {
      continue;
    }
    const CurvesMaterial mat = resolve_curves_material(ctx, object, psys.part->omat);
    if (mat.material) {
      emitted.materials.append_non_duplicates(mat.material);
    }
    const pxr::SdfPath path = ctx.hair_prim_id(object, &psys);
    const EmittedGeometryPrim gprim = build_curves_geometry(hair, mat.path);
    pxr::HdContainerDataSourceHandle prim_ds = compose_gprim_data_source(
        gprim, gf_matrix_from_transform(object->object_to_world().ptr()), true);
    ctx.emit_object_prim(emitted, path, pxr::HdPrimTypeTokens->basisCurves, prim_ds);
  }
}

void emit_hair_proto(PopulateContext &ctx, Object *source)
{
  const bool for_render = (DEG_get_mode(ctx.depsgraph) == DAG_EVAL_RENDER);
  const pxr::SdfPath proto_root = ctx.instancer_proto_root(source);
  const pxr::HdContainerDataSourceHandle instanced_by_ds = ctx.proto_instanced_by(source);

  for (ParticleSystem &psys : source->particlesystem) {
    if (!psys.part || psys.part->type != PART_HAIR) {
      continue;
    }
    if (!psys_check_enabled(source, &psys, for_render)) {
      continue;
    }
    if (psys_in_edit_mode(ctx.depsgraph, &psys)) {
      continue;
    }
    CurvesBuilderResult hair = build_hair_data(source, &psys);
    if (hair.curve_vertex_counts.empty()) {
      continue;
    }
    const CurvesMaterial mat = resolve_curves_material(ctx, source, psys.part->omat);

    char ps_name[32];
    SNPRINTF(ps_name, "PS_%p", &psys);
    const pxr::SdfPath proto_path = proto_root.AppendElementString(ps_name);
    const EmittedGeometryPrim gprim = build_curves_geometry(hair, mat.path);
    pxr::HdContainerDataSourceHandle prim_ds = compose_gprim_data_source(
        gprim, pxr::GfMatrix4d(1.0), true, instanced_by_ds);

    ctx.emit_prim(proto_path, pxr::HdPrimTypeTokens->basisCurves, prim_ds);
    ctx.all_proto_paths.append(proto_path);
    ctx.per_proto_indices.append(pxr::VtIntArray());
  }
}

}  // namespace blender::io::hydra
