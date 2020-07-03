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
 * \ingroup balembic
 */

#include "abc_writer_hair.h"
#include "intern/abc_axis_conversion.h"

#include <cstdio>

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"

#include "BLI_math_geom.h"

#include "BKE_mesh.h"
#include "BKE_mesh_runtime.h"
#include "BKE_particle.h"

#include "CLG_log.h"
static CLG_LogRef LOG = {"io.alembic"};

using Alembic::Abc::P3fArraySamplePtr;
using Alembic::AbcGeom::OCurves;
using Alembic::AbcGeom::OCurvesSchema;
using Alembic::AbcGeom::ON3fGeomParam;
using Alembic::AbcGeom::OV2fGeomParam;

namespace blender {
namespace io {
namespace alembic {

ABCHairWriter::ABCHairWriter(const ABCWriterConstructorArgs &args)
    : ABCAbstractWriter(args), uv_warning_shown_(false)
{
}

void ABCHairWriter::create_alembic_objects(const HierarchyContext * /*context*/)
{
  CLOG_INFO(&LOG, 2, "exporting %s", args_.abc_path.c_str());
  abc_curves_ = OCurves(args_.abc_parent, args_.abc_name, timesample_index_);
  abc_curves_schema_ = abc_curves_.getSchema();
}

const Alembic::Abc::OObject ABCHairWriter::get_alembic_object() const
{
  return abc_curves_;
}

bool ABCHairWriter::check_is_animated(const HierarchyContext & /*context*/) const
{
  /* We assume that hair particles are always animated. */
  return true;
}

void ABCHairWriter::do_write(HierarchyContext &context)
{
  Scene *scene_eval = DEG_get_evaluated_scene(args_.depsgraph);
  Mesh *mesh = mesh_get_eval_final(args_.depsgraph, scene_eval, context.object, &CD_MASK_MESH);
  BKE_mesh_tessface_ensure(mesh);

  std::vector<Imath::V3f> verts;
  std::vector<int32_t> hvertices;
  std::vector<Imath::V2f> uv_values;
  std::vector<Imath::V3f> norm_values;

  ParticleSystem *psys = context.particle_system;
  if (psys->pathcache) {
    ParticleSettings *part = psys->part;
    bool export_children = psys->childcache && part->childtype != 0;

    if (!export_children || part->draw & PART_DRAW_PARENT) {
      write_hair_sample(context, mesh, verts, norm_values, uv_values, hvertices);
    }

    if (export_children) {
      write_hair_child_sample(context, mesh, verts, norm_values, uv_values, hvertices);
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
  invert_m4_m4_safe(inv_mat, context.object->obmat);

  MTFace *mtface = mesh->mtface;
  MFace *mface = mesh->mface;
  MVert *mverts = mesh->mvert;

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

      if (num < mesh->totface) {
        /* TODO(Sybren): check whether the NULL check here and if(mface) are actually required */
        MFace *face = mface == NULL ? NULL : &mface[num];
        MTFace *tface = mtface + num;

        if (mface) {
          float r_uv[2], mapfw[4], vec[3];

          psys_interpolate_uvs(tface, face->v4, pa->fuv, r_uv);
          uv_values.push_back(Imath::V2f(r_uv[0], r_uv[1]));

          psys_interpolate_face(mverts, face, tface, NULL, mapfw, vec, normal, NULL, NULL, NULL);

          copy_yup_from_zup(tmp_nor.getValue(), normal);
          norm_values.push_back(tmp_nor);
        }
      }
      else {
        std::fprintf(stderr, "Particle to faces overflow (%d/%d)\n", num, mesh->totface);
      }
    }
    else if (part->from == PART_FROM_VERT && mtface) {
      /* vertex id */
      const int num = (pa->num_dmcache >= 0) ? pa->num_dmcache : pa->num;

      /* iterate over all faces to find a corresponding underlying UV */
      for (int n = 0; n < mesh->totface; n++) {
        MFace *face = &mface[n];
        MTFace *tface = mtface + n;
        unsigned int vtx[4];
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
            uv_values.push_back(Imath::V2f(tface->uv[o][0], tface->uv[o][1]));

            MVert *mv = mverts + vtx[o];

            normal_short_to_float_v3(normal, mv->no);
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
      verts.push_back(Imath::V3f(vert[0], vert[2], -vert[1]));
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
  invert_m4_m4_safe(inv_mat, context.object->obmat);

  MTFace *mtface = mesh->mtface;
  MVert *mverts = mesh->mvert;

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

      MFace *face = &mesh->mface[num];
      MTFace *tface = mtface + num;

      float r_uv[2], tmpnor[3], mapfw[4], vec[3];

      psys_interpolate_uvs(tface, face->v4, pc->fuv, r_uv);
      uv_values.push_back(Imath::V2f(r_uv[0], r_uv[1]));

      psys_interpolate_face(mverts, face, tface, NULL, mapfw, vec, tmpnor, NULL, NULL, NULL);

      /* Convert Z-up to Y-up. */
      norm_values.push_back(Imath::V3f(tmpnor[0], tmpnor[2], -tmpnor[1]));
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
      verts.push_back(Imath::V3f(vert[0], vert[2], -vert[1]));

      path++;
    }
  }
}

}  // namespace alembic
}  // namespace io
}  // namespace blender
