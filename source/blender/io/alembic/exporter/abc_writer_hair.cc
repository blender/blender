/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup balembic
 */

#include "abc_writer_hair.h"
#include "intern/abc_axis_conversion.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"

#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"

#include "BKE_customdata.hh"
#include "BKE_mesh_legacy_convert.hh"
#include "BKE_mesh_runtime.hh"
#include "BKE_object.hh"
#include "BKE_particle.h"

#include "CLG_log.h"
static CLG_LogRef LOG = {"io.alembic"};

using Alembic::Abc::P3fArraySamplePtr;
using Alembic::AbcGeom::OCurves;
using Alembic::AbcGeom::OCurvesSchema;
using Alembic::AbcGeom::ON3fGeomParam;
using Alembic::AbcGeom::OV2fGeomParam;

namespace blender::io::alembic {

ABCHairWriter::ABCHairWriter(const ABCWriterConstructorArgs &args)
    : ABCAbstractWriter(args), uv_warning_shown_(false)
{
}

void ABCHairWriter::create_alembic_objects(const HierarchyContext * /*context*/)
{
  CLOG_DEBUG(&LOG, "exporting %s", args_.abc_path.c_str());
  abc_curves_ = OCurves(args_.abc_parent, args_.abc_name, timesample_index_);
  abc_curves_schema_ = abc_curves_.getSchema();
}

Alembic::Abc::OObject ABCHairWriter::get_alembic_object() const
{
  return abc_curves_;
}

Alembic::Abc::OCompoundProperty ABCHairWriter::abc_prop_for_custom_props()
{
  return abc_schema_prop_for_custom_props(abc_curves_schema_);
}

bool ABCHairWriter::check_is_animated(const HierarchyContext & /*context*/) const
{
  /* We assume that hair particles are always animated. */
  return true;
}

void ABCHairWriter::do_write(HierarchyContext &context)
{
  const Mesh *mesh = BKE_object_get_evaluated_mesh(context.object);
  if (!mesh) {
    return;
  }
  BKE_mesh_tessface_ensure(const_cast<Mesh *>(mesh));

  std::vector<Imath::V3f> verts;
  std::vector<int32_t> hvertices;
  std::vector<Imath::V2f> uv_values;
  std::vector<Imath::V3f> norm_values;

  ParticleSystem *psys = context.particle_system;
  if (psys->pathcache) {
    ParticleSettings *part = psys->part;
    bool export_children = psys->childcache && part->childtype != 0;

    if (!export_children || part->draw & PART_DRAW_PARENT) {
      write_hair_sample(
          context, const_cast<Mesh *>(mesh), verts, norm_values, uv_values, hvertices);
    }

    if (export_children) {
      write_hair_child_sample(
          context, const_cast<Mesh *>(mesh), verts, norm_values, uv_values, hvertices);
    }
  }

  Alembic::Abc::P3fArraySample iPos(verts);
  OCurvesSchema::Sample sample(iPos, hvertices);
  sample.setBasis(Alembic::AbcGeom::kNoBasis);
  sample.setType(Alembic::AbcGeom::kLinear);
  sample.setWrap(Alembic::AbcGeom::kNonPeriodic);

  if (!uv_values.empty()) {
    OV2fGeomParam::Sample uv_smp;
    uv_smp.setVals(uv_values);
    sample.setUVs(uv_smp);
  }

  if (!norm_values.empty()) {
    ON3fGeomParam::Sample norm_smp;
    norm_smp.setVals(norm_values);
    sample.setNormals(norm_smp);
  }

  update_bounding_box(context.object);
  sample.setSelfBounds(bounding_box_);
  abc_curves_schema_.set(sample);
}

void ABCHairWriter::write_hair_sample(const HierarchyContext &context,
                                      Mesh *mesh,
                                      std::vector<Imath::V3f> &verts,
                                      std::vector<Imath::V3f> &norm_values,
                                      std::vector<Imath::V2f> &uv_values,
                                      std::vector<int32_t> &hvertices)
{
  /* Get untransformed vertices, there's a xform under the hair. */
  float inv_mat[4][4];
  invert_m4_m4_safe(inv_mat, context.object->object_to_world().ptr());

  MTFace *mtface = (MTFace *)CustomData_get_layer_for_write(
      &mesh->fdata_legacy, CD_MTFACE, mesh->totface_legacy);
  const MFace *mface = (const MFace *)CustomData_get_layer(&mesh->fdata_legacy, CD_MFACE);
  const Span<float3> positions = mesh->vert_positions();
  const Span<float3> vert_normals = mesh->vert_normals();

  if ((!mtface || !mface) && !uv_warning_shown_) {
    std::fprintf(stderr,
                 "Warning, no UV set found for underlying geometry of %s.\n",
                 context.object->id.name + 2);
    uv_warning_shown_ = true;
  }

  ParticleSystem *psys = context.particle_system;
  ParticleSettings *part = psys->part;
  ParticleData *pa = psys->particles;
  int k;

  ParticleCacheKey **cache = psys->pathcache;
  ParticleCacheKey *path;
  float normal[3];
  Imath::V3f tmp_nor;

  for (int p = 0; p < psys->totpart; p++, pa++) {
    /* underlying info for faces-only emission */
    path = cache[p];

    /* Write UV and normal vectors */
    if (part->from == PART_FROM_FACE && mtface) {
      const int num = pa->num_dmcache >= 0 ? pa->num_dmcache : pa->num;

      if (num < mesh->totface_legacy) {
        /* TODO(Sybren): check whether the null check here and if(mface) are actually required
         */
        const MFace *face = mface == nullptr ? nullptr : &mface[num];
        MTFace *tface = mtface + num;

        if (mface) {
          float uv[2], mapfw[4], vec[3];

          psys_interpolate_uvs(tface, face->v4, pa->fuv, uv);
          uv_values.emplace_back(uv[0], uv[1]);

          psys_interpolate_face(mesh,
                                reinterpret_cast<const float (*)[3]>(positions.data()),
                                reinterpret_cast<const float (*)[3]>(vert_normals.data()),
                                face,
                                tface,
                                nullptr,
                                mapfw,
                                vec,
                                normal,
                                nullptr,
                                nullptr,
                                nullptr);

          copy_yup_from_zup(tmp_nor.getValue(), normal);
          norm_values.push_back(tmp_nor);
        }
      }
      else {
        std::fprintf(stderr, "Particle to faces overflow (%d/%d)\n", num, mesh->totface_legacy);
      }
    }
    else if (part->from == PART_FROM_VERT && mtface) {
      /* vertex id */
      const int num = (pa->num_dmcache >= 0) ? pa->num_dmcache : pa->num;

      /* iterate over all faces to find a corresponding underlying UV */
      for (int n = 0; n < mesh->totface_legacy; n++) {
        const MFace *face = &mface[n];
        const MTFace *tface = mtface + n;
        uint vtx[4];
        vtx[0] = face->v1;
        vtx[1] = face->v2;
        vtx[2] = face->v3;
        vtx[3] = face->v4;
        bool found = false;

        for (int o = 0; o < 4; o++) {
          if (o > 2 && vtx[o] == 0) {
            break;
          }

          if (vtx[o] == num) {
            uv_values.emplace_back(tface->uv[o][0], tface->uv[o][1]);
            copy_v3_v3(normal, vert_normals[vtx[o]]);
            copy_yup_from_zup(tmp_nor.getValue(), normal);
            norm_values.push_back(tmp_nor);
            found = true;
            break;
          }
        }

        if (found) {
          break;
        }
      }
    }

    int steps = path->segments + 1;
    hvertices.push_back(steps);

    for (k = 0; k < steps; k++, path++) {
      float vert[3];
      copy_v3_v3(vert, path->co);
      mul_m4_v3(inv_mat, vert);

      /* Convert Z-up to Y-up. */
      verts.emplace_back(vert[0], vert[2], -vert[1]);
    }
  }
}

void ABCHairWriter::write_hair_child_sample(const HierarchyContext &context,
                                            Mesh *mesh,
                                            std::vector<Imath::V3f> &verts,
                                            std::vector<Imath::V3f> &norm_values,
                                            std::vector<Imath::V2f> &uv_values,
                                            std::vector<int32_t> &hvertices)
{
  /* Get untransformed vertices, there's a xform under the hair. */
  float inv_mat[4][4];
  invert_m4_m4_safe(inv_mat, context.object->object_to_world().ptr());

  const MFace *mface = (const MFace *)CustomData_get_layer(&mesh->fdata_legacy, CD_MFACE);
  MTFace *mtface = (MTFace *)CustomData_get_layer_for_write(
      &mesh->fdata_legacy, CD_MTFACE, mesh->totface_legacy);
  const Span<float3> positions = mesh->vert_positions();
  const Span<float3> vert_normals = mesh->vert_normals();

  ParticleSystem *psys = context.particle_system;
  ParticleSettings *part = psys->part;
  ParticleCacheKey **cache = psys->childcache;
  ParticleCacheKey *path;

  ChildParticle *pc = psys->child;

  for (int p = 0; p < psys->totchild; p++, pc++) {
    path = cache[p];

    if (part->from == PART_FROM_FACE && part->childtype != PART_CHILD_PARTICLES && mtface) {
      const int num = pc->num;
      if (num < 0) {
        CLOG_WARN(
            &LOG,
            "Child particle of hair system %s has unknown face index of geometry of %s, skipping "
            "child hair.",
            psys->name,
            context.object->id.name + 2);
        continue;
      }

      const MFace *face = &mface[num];
      MTFace *tface = mtface + num;

      float uv[2], tmpnor[3], mapfw[4], vec[3];

      psys_interpolate_uvs(tface, face->v4, pc->fuv, uv);
      uv_values.emplace_back(uv[0], uv[1]);

      psys_interpolate_face(mesh,
                            reinterpret_cast<const float (*)[3]>(positions.data()),
                            reinterpret_cast<const float (*)[3]>(vert_normals.data()),
                            face,
                            tface,
                            nullptr,
                            mapfw,
                            vec,
                            tmpnor,
                            nullptr,
                            nullptr,
                            nullptr);

      /* Convert Z-up to Y-up. */
      norm_values.emplace_back(tmpnor[0], tmpnor[2], -tmpnor[1]);
    }
    else {
      if (!uv_values.empty()) {
        uv_values.push_back(uv_values[pc->parent]);
      }
      if (!norm_values.empty()) {
        norm_values.push_back(norm_values[pc->parent]);
      }
    }

    int steps = path->segments + 1;
    hvertices.push_back(steps);

    for (int k = 0; k < steps; k++) {
      float vert[3];
      copy_v3_v3(vert, path->co);
      mul_m4_v3(inv_mat, vert);

      /* Convert Z-up to Y-up. */
      verts.emplace_back(vert[0], vert[2], -vert[1]);

      path++;
    }
  }
}

}  // namespace blender::io::alembic
