/*
 * Copyright 2011-2013 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "render/attribute.h"
#include "render/camera.h"
#include "render/curves.h"
#include "render/mesh.h"
#include "render/object.h"
#include "render/scene.h"

#include "blender/blender_sync.h"
#include "blender/blender_util.h"

#include "util/util_foreach.h"
#include "util/util_hash.h"
#include "util/util_logging.h"

CCL_NAMESPACE_BEGIN

ParticleCurveData::ParticleCurveData()
{
}

ParticleCurveData::~ParticleCurveData()
{
}

static void interp_weights(float t, float data[4])
{
  /* Cardinal curve interpolation */
  float t2 = t * t;
  float t3 = t2 * t;
  float fc = 0.71f;

  data[0] = -fc * t3 + 2.0f * fc * t2 - fc * t;
  data[1] = (2.0f - fc) * t3 + (fc - 3.0f) * t2 + 1.0f;
  data[2] = (fc - 2.0f) * t3 + (3.0f - 2.0f * fc) * t2 + fc * t;
  data[3] = fc * t3 - fc * t2;
}

static void curveinterp_v3_v3v3v3v3(
    float3 *p, float3 *v1, float3 *v2, float3 *v3, float3 *v4, const float w[4])
{
  p->x = v1->x * w[0] + v2->x * w[1] + v3->x * w[2] + v4->x * w[3];
  p->y = v1->y * w[0] + v2->y * w[1] + v3->y * w[2] + v4->y * w[3];
  p->z = v1->z * w[0] + v2->z * w[1] + v3->z * w[2] + v4->z * w[3];
}

static float shaperadius(float shape, float root, float tip, float time)
{
  assert(time >= 0.0f);
  assert(time <= 1.0f);
  float radius = 1.0f - time;

  if (shape != 0.0f) {
    if (shape < 0.0f)
      radius = powf(radius, 1.0f + shape);
    else
      radius = powf(radius, 1.0f / (1.0f - shape));
  }
  return (radius * (root - tip)) + tip;
}

/* curve functions */

static void InterpolateKeySegments(
    int seg, int segno, int key, int curve, float3 *keyloc, float *time, ParticleCurveData *CData)
{
  float3 ckey_loc1 = CData->curvekey_co[key];
  float3 ckey_loc2 = ckey_loc1;
  float3 ckey_loc3 = CData->curvekey_co[key + 1];
  float3 ckey_loc4 = ckey_loc3;

  if (key > CData->curve_firstkey[curve])
    ckey_loc1 = CData->curvekey_co[key - 1];

  if (key < CData->curve_firstkey[curve] + CData->curve_keynum[curve] - 2)
    ckey_loc4 = CData->curvekey_co[key + 2];

  float time1 = CData->curvekey_time[key] / CData->curve_length[curve];
  float time2 = CData->curvekey_time[key + 1] / CData->curve_length[curve];

  float dfra = (time2 - time1) / (float)segno;

  if (time)
    *time = (dfra * seg) + time1;

  float t[4];

  interp_weights((float)seg / (float)segno, t);

  if (keyloc)
    curveinterp_v3_v3v3v3v3(keyloc, &ckey_loc1, &ckey_loc2, &ckey_loc3, &ckey_loc4, t);
}

static bool ObtainCacheParticleData(
    Mesh *mesh, BL::Mesh *b_mesh, BL::Object *b_ob, ParticleCurveData *CData, bool background)
{
  int curvenum = 0;
  int keyno = 0;

  if (!(mesh && b_mesh && b_ob && CData))
    return false;

  Transform tfm = get_transform(b_ob->matrix_world());
  Transform itfm = transform_quick_inverse(tfm);

  BL::Object::modifiers_iterator b_mod;
  for (b_ob->modifiers.begin(b_mod); b_mod != b_ob->modifiers.end(); ++b_mod) {
    if ((b_mod->type() == b_mod->type_PARTICLE_SYSTEM) &&
        (background ? b_mod->show_render() : b_mod->show_viewport())) {
      BL::ParticleSystemModifier psmd((const PointerRNA)b_mod->ptr);
      BL::ParticleSystem b_psys((const PointerRNA)psmd.particle_system().ptr);
      BL::ParticleSettings b_part((const PointerRNA)b_psys.settings().ptr);

      if ((b_part.render_type() == BL::ParticleSettings::render_type_PATH) &&
          (b_part.type() == BL::ParticleSettings::type_HAIR)) {
        int shader = clamp(b_part.material() - 1, 0, mesh->used_shaders.size() - 1);
        int display_step = background ? b_part.render_step() : b_part.display_step();
        int totparts = b_psys.particles.length();
        int totchild = background ? b_psys.child_particles.length() :
                                    (int)((float)b_psys.child_particles.length() *
                                          (float)b_part.display_percentage() / 100.0f);
        int totcurves = totchild;

        if (b_part.child_type() == 0 || totchild == 0)
          totcurves += totparts;

        if (totcurves == 0)
          continue;

        int ren_step = (1 << display_step) + 1;
        if (b_part.kink() == BL::ParticleSettings::kink_SPIRAL)
          ren_step += b_part.kink_extra_steps();

        CData->psys_firstcurve.push_back_slow(curvenum);
        CData->psys_curvenum.push_back_slow(totcurves);
        CData->psys_shader.push_back_slow(shader);

        float radius = b_part.radius_scale() * 0.5f;

        CData->psys_rootradius.push_back_slow(radius * b_part.root_radius());
        CData->psys_tipradius.push_back_slow(radius * b_part.tip_radius());
        CData->psys_shape.push_back_slow(b_part.shape());
        CData->psys_closetip.push_back_slow(b_part.use_close_tip());

        int pa_no = 0;
        if (!(b_part.child_type() == 0) && totchild != 0)
          pa_no = totparts;

        int num_add = (totparts + totchild - pa_no);
        CData->curve_firstkey.reserve(CData->curve_firstkey.size() + num_add);
        CData->curve_keynum.reserve(CData->curve_keynum.size() + num_add);
        CData->curve_length.reserve(CData->curve_length.size() + num_add);
        CData->curvekey_co.reserve(CData->curvekey_co.size() + num_add * ren_step);
        CData->curvekey_time.reserve(CData->curvekey_time.size() + num_add * ren_step);

        for (; pa_no < totparts + totchild; pa_no++) {
          int keynum = 0;
          CData->curve_firstkey.push_back_slow(keyno);

          float curve_length = 0.0f;
          float3 pcKey;
          for (int step_no = 0; step_no < ren_step; step_no++) {
            float nco[3];
            b_psys.co_hair(*b_ob, pa_no, step_no, nco);
            float3 cKey = make_float3(nco[0], nco[1], nco[2]);
            cKey = transform_point(&itfm, cKey);
            if (step_no > 0) {
              const float step_length = len(cKey - pcKey);
              curve_length += step_length;
            }
            CData->curvekey_co.push_back_slow(cKey);
            CData->curvekey_time.push_back_slow(curve_length);
            pcKey = cKey;
            keynum++;
          }
          keyno += keynum;

          CData->curve_keynum.push_back_slow(keynum);
          CData->curve_length.push_back_slow(curve_length);
          curvenum++;
        }
      }
    }
  }

  return true;
}

static bool ObtainCacheParticleUV(Mesh *mesh,
                                  BL::Mesh *b_mesh,
                                  BL::Object *b_ob,
                                  ParticleCurveData *CData,
                                  bool background,
                                  int uv_num)
{
  if (!(mesh && b_mesh && b_ob && CData))
    return false;

  CData->curve_uv.clear();

  BL::Object::modifiers_iterator b_mod;
  for (b_ob->modifiers.begin(b_mod); b_mod != b_ob->modifiers.end(); ++b_mod) {
    if ((b_mod->type() == b_mod->type_PARTICLE_SYSTEM) &&
        (background ? b_mod->show_render() : b_mod->show_viewport())) {
      BL::ParticleSystemModifier psmd((const PointerRNA)b_mod->ptr);
      BL::ParticleSystem b_psys((const PointerRNA)psmd.particle_system().ptr);
      BL::ParticleSettings b_part((const PointerRNA)b_psys.settings().ptr);

      if ((b_part.render_type() == BL::ParticleSettings::render_type_PATH) &&
          (b_part.type() == BL::ParticleSettings::type_HAIR)) {
        int totparts = b_psys.particles.length();
        int totchild = background ? b_psys.child_particles.length() :
                                    (int)((float)b_psys.child_particles.length() *
                                          (float)b_part.display_percentage() / 100.0f);
        int totcurves = totchild;

        if (b_part.child_type() == 0 || totchild == 0)
          totcurves += totparts;

        if (totcurves == 0)
          continue;

        int pa_no = 0;
        if (!(b_part.child_type() == 0) && totchild != 0)
          pa_no = totparts;

        int num_add = (totparts + totchild - pa_no);
        CData->curve_uv.reserve(CData->curve_uv.size() + num_add);

        BL::ParticleSystem::particles_iterator b_pa;
        b_psys.particles.begin(b_pa);
        for (; pa_no < totparts + totchild; pa_no++) {
          /* Add UVs */
          BL::Mesh::uv_layers_iterator l;
          b_mesh->uv_layers.begin(l);

          float2 uv = make_float2(0.0f, 0.0f);
          if (b_mesh->uv_layers.length())
            b_psys.uv_on_emitter(psmd, *b_pa, pa_no, uv_num, &uv.x);
          CData->curve_uv.push_back_slow(uv);

          if (pa_no < totparts && b_pa != b_psys.particles.end())
            ++b_pa;
        }
      }
    }
  }

  return true;
}

static bool ObtainCacheParticleVcol(Mesh *mesh,
                                    BL::Mesh *b_mesh,
                                    BL::Object *b_ob,
                                    ParticleCurveData *CData,
                                    bool background,
                                    int vcol_num)
{
  if (!(mesh && b_mesh && b_ob && CData))
    return false;

  CData->curve_vcol.clear();

  BL::Object::modifiers_iterator b_mod;
  for (b_ob->modifiers.begin(b_mod); b_mod != b_ob->modifiers.end(); ++b_mod) {
    if ((b_mod->type() == b_mod->type_PARTICLE_SYSTEM) &&
        (background ? b_mod->show_render() : b_mod->show_viewport())) {
      BL::ParticleSystemModifier psmd((const PointerRNA)b_mod->ptr);
      BL::ParticleSystem b_psys((const PointerRNA)psmd.particle_system().ptr);
      BL::ParticleSettings b_part((const PointerRNA)b_psys.settings().ptr);

      if ((b_part.render_type() == BL::ParticleSettings::render_type_PATH) &&
          (b_part.type() == BL::ParticleSettings::type_HAIR)) {
        int totparts = b_psys.particles.length();
        int totchild = background ? b_psys.child_particles.length() :
                                    (int)((float)b_psys.child_particles.length() *
                                          (float)b_part.display_percentage() / 100.0f);
        int totcurves = totchild;

        if (b_part.child_type() == 0 || totchild == 0)
          totcurves += totparts;

        if (totcurves == 0)
          continue;

        int pa_no = 0;
        if (!(b_part.child_type() == 0) && totchild != 0)
          pa_no = totparts;

        int num_add = (totparts + totchild - pa_no);
        CData->curve_vcol.reserve(CData->curve_vcol.size() + num_add);

        BL::ParticleSystem::particles_iterator b_pa;
        b_psys.particles.begin(b_pa);
        for (; pa_no < totparts + totchild; pa_no++) {
          /* Add vertex colors */
          BL::Mesh::vertex_colors_iterator l;
          b_mesh->vertex_colors.begin(l);

          float3 vcol = make_float3(0.0f, 0.0f, 0.0f);
          if (b_mesh->vertex_colors.length())
            b_psys.mcol_on_emitter(psmd, *b_pa, pa_no, vcol_num, &vcol.x);
          CData->curve_vcol.push_back_slow(vcol);

          if (pa_no < totparts && b_pa != b_psys.particles.end())
            ++b_pa;
        }
      }
    }
  }

  return true;
}

static void ExportCurveTrianglePlanes(Mesh *mesh,
                                      ParticleCurveData *CData,
                                      float3 RotCam,
                                      bool is_ortho)
{
  int vertexno = mesh->verts.size();
  int vertexindex = vertexno;
  int numverts = 0, numtris = 0;

  /* compute and reserve size of arrays */
  for (int sys = 0; sys < CData->psys_firstcurve.size(); sys++) {
    for (int curve = CData->psys_firstcurve[sys];
         curve < CData->psys_firstcurve[sys] + CData->psys_curvenum[sys];
         curve++) {
      numverts += 2 + (CData->curve_keynum[curve] - 1) * 2;
      numtris += (CData->curve_keynum[curve] - 1) * 2;
    }
  }

  mesh->reserve_mesh(mesh->verts.size() + numverts, mesh->num_triangles() + numtris);

  /* actually export */
  for (int sys = 0; sys < CData->psys_firstcurve.size(); sys++) {
    for (int curve = CData->psys_firstcurve[sys];
         curve < CData->psys_firstcurve[sys] + CData->psys_curvenum[sys];
         curve++) {
      float3 xbasis;
      float3 v1;
      float time = 0.0f;
      float3 ickey_loc = CData->curvekey_co[CData->curve_firstkey[curve]];
      float radius = shaperadius(
          CData->psys_shape[sys], CData->psys_rootradius[sys], CData->psys_tipradius[sys], 0.0f);
      v1 = CData->curvekey_co[CData->curve_firstkey[curve] + 1] -
           CData->curvekey_co[CData->curve_firstkey[curve]];
      if (is_ortho)
        xbasis = normalize(cross(RotCam, v1));
      else
        xbasis = normalize(cross(RotCam - ickey_loc, v1));
      float3 ickey_loc_shfl = ickey_loc - radius * xbasis;
      float3 ickey_loc_shfr = ickey_loc + radius * xbasis;
      mesh->add_vertex(ickey_loc_shfl);
      mesh->add_vertex(ickey_loc_shfr);
      vertexindex += 2;

      for (int curvekey = CData->curve_firstkey[curve] + 1;
           curvekey < CData->curve_firstkey[curve] + CData->curve_keynum[curve];
           curvekey++) {
        ickey_loc = CData->curvekey_co[curvekey];

        if (curvekey == CData->curve_firstkey[curve] + CData->curve_keynum[curve] - 1)
          v1 = CData->curvekey_co[curvekey] -
               CData->curvekey_co[max(curvekey - 1, CData->curve_firstkey[curve])];
        else
          v1 = CData->curvekey_co[curvekey + 1] - CData->curvekey_co[curvekey - 1];

        time = CData->curvekey_time[curvekey] / CData->curve_length[curve];
        radius = shaperadius(
            CData->psys_shape[sys], CData->psys_rootradius[sys], CData->psys_tipradius[sys], time);

        if (curvekey == CData->curve_firstkey[curve] + CData->curve_keynum[curve] - 1)
          radius = shaperadius(CData->psys_shape[sys],
                               CData->psys_rootradius[sys],
                               CData->psys_tipradius[sys],
                               0.95f);

        if (CData->psys_closetip[sys] &&
            (curvekey == CData->curve_firstkey[curve] + CData->curve_keynum[curve] - 1))
          radius = shaperadius(CData->psys_shape[sys], CData->psys_rootradius[sys], 0.0f, 0.95f);

        if (is_ortho)
          xbasis = normalize(cross(RotCam, v1));
        else
          xbasis = normalize(cross(RotCam - ickey_loc, v1));
        float3 ickey_loc_shfl = ickey_loc - radius * xbasis;
        float3 ickey_loc_shfr = ickey_loc + radius * xbasis;
        mesh->add_vertex(ickey_loc_shfl);
        mesh->add_vertex(ickey_loc_shfr);
        mesh->add_triangle(
            vertexindex - 2, vertexindex, vertexindex - 1, CData->psys_shader[sys], true);
        mesh->add_triangle(
            vertexindex + 1, vertexindex - 1, vertexindex, CData->psys_shader[sys], true);
        vertexindex += 2;
      }
    }
  }

  mesh->resize_mesh(mesh->verts.size(), mesh->num_triangles());
  mesh->attributes.remove(ATTR_STD_VERTEX_NORMAL);
  mesh->attributes.remove(ATTR_STD_FACE_NORMAL);
  mesh->add_face_normals();
  mesh->add_vertex_normals();
  mesh->attributes.remove(ATTR_STD_FACE_NORMAL);

  /* texture coords still needed */
}

static void ExportCurveTriangleGeometry(Mesh *mesh, ParticleCurveData *CData, int resolution)
{
  int vertexno = mesh->verts.size();
  int vertexindex = vertexno;
  int numverts = 0, numtris = 0;

  /* compute and reserve size of arrays */
  for (int sys = 0; sys < CData->psys_firstcurve.size(); sys++) {
    for (int curve = CData->psys_firstcurve[sys];
         curve < CData->psys_firstcurve[sys] + CData->psys_curvenum[sys];
         curve++) {
      numverts += (CData->curve_keynum[curve] - 1) * resolution + resolution;
      numtris += (CData->curve_keynum[curve] - 1) * 2 * resolution;
    }
  }

  mesh->reserve_mesh(mesh->verts.size() + numverts, mesh->num_triangles() + numtris);

  /* actually export */
  for (int sys = 0; sys < CData->psys_firstcurve.size(); sys++) {
    for (int curve = CData->psys_firstcurve[sys];
         curve < CData->psys_firstcurve[sys] + CData->psys_curvenum[sys];
         curve++) {
      float3 firstxbasis = cross(make_float3(1.0f, 0.0f, 0.0f),
                                 CData->curvekey_co[CData->curve_firstkey[curve] + 1] -
                                     CData->curvekey_co[CData->curve_firstkey[curve]]);
      if (!is_zero(firstxbasis))
        firstxbasis = normalize(firstxbasis);
      else
        firstxbasis = normalize(cross(make_float3(0.0f, 1.0f, 0.0f),
                                      CData->curvekey_co[CData->curve_firstkey[curve] + 1] -
                                          CData->curvekey_co[CData->curve_firstkey[curve]]));

      for (int curvekey = CData->curve_firstkey[curve];
           curvekey < CData->curve_firstkey[curve] + CData->curve_keynum[curve] - 1;
           curvekey++) {
        float3 xbasis = firstxbasis;
        float3 v1;
        float3 v2;

        if (curvekey == CData->curve_firstkey[curve]) {
          v1 = CData->curvekey_co[min(
                   curvekey + 2, CData->curve_firstkey[curve] + CData->curve_keynum[curve] - 1)] -
               CData->curvekey_co[curvekey + 1];
          v2 = CData->curvekey_co[curvekey + 1] - CData->curvekey_co[curvekey];
        }
        else if (curvekey == CData->curve_firstkey[curve] + CData->curve_keynum[curve] - 1) {
          v1 = CData->curvekey_co[curvekey] - CData->curvekey_co[curvekey - 1];
          v2 = CData->curvekey_co[curvekey - 1] -
               CData->curvekey_co[max(curvekey - 2, CData->curve_firstkey[curve])];
        }
        else {
          v1 = CData->curvekey_co[curvekey + 1] - CData->curvekey_co[curvekey];
          v2 = CData->curvekey_co[curvekey] - CData->curvekey_co[curvekey - 1];
        }

        xbasis = cross(v1, v2);

        if (len_squared(xbasis) >= 0.05f * len_squared(v1) * len_squared(v2)) {
          firstxbasis = normalize(xbasis);
          break;
        }
      }

      for (int curvekey = CData->curve_firstkey[curve];
           curvekey < CData->curve_firstkey[curve] + CData->curve_keynum[curve] - 1;
           curvekey++) {
        int subv = 1;
        float3 xbasis;
        float3 ybasis;
        float3 v1;
        float3 v2;

        if (curvekey == CData->curve_firstkey[curve]) {
          subv = 0;
          v1 = CData->curvekey_co[min(
                   curvekey + 2, CData->curve_firstkey[curve] + CData->curve_keynum[curve] - 1)] -
               CData->curvekey_co[curvekey + 1];
          v2 = CData->curvekey_co[curvekey + 1] - CData->curvekey_co[curvekey];
        }
        else if (curvekey == CData->curve_firstkey[curve] + CData->curve_keynum[curve] - 1) {
          v1 = CData->curvekey_co[curvekey] - CData->curvekey_co[curvekey - 1];
          v2 = CData->curvekey_co[curvekey - 1] -
               CData->curvekey_co[max(curvekey - 2, CData->curve_firstkey[curve])];
        }
        else {
          v1 = CData->curvekey_co[curvekey + 1] - CData->curvekey_co[curvekey];
          v2 = CData->curvekey_co[curvekey] - CData->curvekey_co[curvekey - 1];
        }

        xbasis = cross(v1, v2);

        if (len_squared(xbasis) >= 0.05f * len_squared(v1) * len_squared(v2)) {
          xbasis = normalize(xbasis);
          firstxbasis = xbasis;
        }
        else
          xbasis = firstxbasis;

        ybasis = normalize(cross(xbasis, v2));

        for (; subv <= 1; subv++) {
          float3 ickey_loc = make_float3(0.0f, 0.0f, 0.0f);
          float time = 0.0f;

          InterpolateKeySegments(subv, 1, curvekey, curve, &ickey_loc, &time, CData);

          float radius = shaperadius(CData->psys_shape[sys],
                                     CData->psys_rootradius[sys],
                                     CData->psys_tipradius[sys],
                                     time);

          if ((curvekey == CData->curve_firstkey[curve] + CData->curve_keynum[curve] - 2) &&
              (subv == 1))
            radius = shaperadius(CData->psys_shape[sys],
                                 CData->psys_rootradius[sys],
                                 CData->psys_tipradius[sys],
                                 0.95f);

          if (CData->psys_closetip[sys] && (subv == 1) &&
              (curvekey == CData->curve_firstkey[curve] + CData->curve_keynum[curve] - 2))
            radius = shaperadius(CData->psys_shape[sys], CData->psys_rootradius[sys], 0.0f, 0.95f);

          float angle = M_2PI_F / (float)resolution;
          for (int section = 0; section < resolution; section++) {
            float3 ickey_loc_shf = ickey_loc + radius * (cosf(angle * section) * xbasis +
                                                         sinf(angle * section) * ybasis);
            mesh->add_vertex(ickey_loc_shf);
          }

          if (subv != 0) {
            for (int section = 0; section < resolution - 1; section++) {
              mesh->add_triangle(vertexindex - resolution + section,
                                 vertexindex + section,
                                 vertexindex - resolution + section + 1,
                                 CData->psys_shader[sys],
                                 true);
              mesh->add_triangle(vertexindex + section + 1,
                                 vertexindex - resolution + section + 1,
                                 vertexindex + section,
                                 CData->psys_shader[sys],
                                 true);
            }
            mesh->add_triangle(vertexindex - 1,
                               vertexindex + resolution - 1,
                               vertexindex - resolution,
                               CData->psys_shader[sys],
                               true);
            mesh->add_triangle(vertexindex,
                               vertexindex - resolution,
                               vertexindex + resolution - 1,
                               CData->psys_shader[sys],
                               true);
          }
          vertexindex += resolution;
        }
      }
    }
  }

  mesh->resize_mesh(mesh->verts.size(), mesh->num_triangles());
  mesh->attributes.remove(ATTR_STD_VERTEX_NORMAL);
  mesh->attributes.remove(ATTR_STD_FACE_NORMAL);
  mesh->add_face_normals();
  mesh->add_vertex_normals();
  mesh->attributes.remove(ATTR_STD_FACE_NORMAL);

  /* texture coords still needed */
}

static void ExportCurveSegments(Scene *scene, Mesh *mesh, ParticleCurveData *CData)
{
  int num_keys = 0;
  int num_curves = 0;

  if (mesh->num_curves())
    return;

  Attribute *attr_intercept = NULL;
  Attribute *attr_random = NULL;

  if (mesh->need_attribute(scene, ATTR_STD_CURVE_INTERCEPT))
    attr_intercept = mesh->curve_attributes.add(ATTR_STD_CURVE_INTERCEPT);
  if (mesh->need_attribute(scene, ATTR_STD_CURVE_RANDOM))
    attr_random = mesh->curve_attributes.add(ATTR_STD_CURVE_RANDOM);

  /* compute and reserve size of arrays */
  for (int sys = 0; sys < CData->psys_firstcurve.size(); sys++) {
    for (int curve = CData->psys_firstcurve[sys];
         curve < CData->psys_firstcurve[sys] + CData->psys_curvenum[sys];
         curve++) {
      num_keys += CData->curve_keynum[curve];
      num_curves++;
    }
  }

  if (num_curves > 0) {
    VLOG(1) << "Exporting curve segments for mesh " << mesh->name;
  }

  mesh->reserve_curves(mesh->num_curves() + num_curves, mesh->curve_keys.size() + num_keys);

  num_keys = 0;
  num_curves = 0;

  /* actually export */
  for (int sys = 0; sys < CData->psys_firstcurve.size(); sys++) {
    for (int curve = CData->psys_firstcurve[sys];
         curve < CData->psys_firstcurve[sys] + CData->psys_curvenum[sys];
         curve++) {
      size_t num_curve_keys = 0;

      for (int curvekey = CData->curve_firstkey[curve];
           curvekey < CData->curve_firstkey[curve] + CData->curve_keynum[curve];
           curvekey++) {
        const float3 ickey_loc = CData->curvekey_co[curvekey];
        const float curve_time = CData->curvekey_time[curvekey];
        const float curve_length = CData->curve_length[curve];
        const float time = (curve_length > 0.0f) ? curve_time / curve_length : 0.0f;
        float radius = shaperadius(
            CData->psys_shape[sys], CData->psys_rootradius[sys], CData->psys_tipradius[sys], time);
        if (CData->psys_closetip[sys] &&
            (curvekey == CData->curve_firstkey[curve] + CData->curve_keynum[curve] - 1)) {
          radius = 0.0f;
        }
        mesh->add_curve_key(ickey_loc, radius);
        if (attr_intercept)
          attr_intercept->add(time);

        num_curve_keys++;
      }

      if (attr_random != NULL) {
        attr_random->add(hash_int_01(num_curves));
      }

      mesh->add_curve(num_keys, CData->psys_shader[sys]);
      num_keys += num_curve_keys;
      num_curves++;
    }
  }

  /* check allocation */
  if ((mesh->curve_keys.size() != num_keys) || (mesh->num_curves() != num_curves)) {
    VLOG(1) << "Allocation failed, clearing data";
    mesh->clear();
  }
}

static float4 CurveSegmentMotionCV(ParticleCurveData *CData, int sys, int curve, int curvekey)
{
  const float3 ickey_loc = CData->curvekey_co[curvekey];
  const float curve_time = CData->curvekey_time[curvekey];
  const float curve_length = CData->curve_length[curve];
  float time = (curve_length > 0.0f) ? curve_time / curve_length : 0.0f;
  float radius = shaperadius(
      CData->psys_shape[sys], CData->psys_rootradius[sys], CData->psys_tipradius[sys], time);

  if (CData->psys_closetip[sys] &&
      (curvekey == CData->curve_firstkey[curve] + CData->curve_keynum[curve] - 1))
    radius = 0.0f;

  /* curve motion keys store both position and radius in float4 */
  float4 mP = float3_to_float4(ickey_loc);
  mP.w = radius;
  return mP;
}

static float4 LerpCurveSegmentMotionCV(ParticleCurveData *CData, int sys, int curve, float step)
{
  assert(step >= 0.0f);
  assert(step <= 1.0f);
  const int first_curve_key = CData->curve_firstkey[curve];
  const float curve_key_f = step * (CData->curve_keynum[curve] - 1);
  int curvekey = (int)floorf(curve_key_f);
  const float remainder = curve_key_f - curvekey;
  if (remainder == 0.0f) {
    return CurveSegmentMotionCV(CData, sys, curve, first_curve_key + curvekey);
  }
  int curvekey2 = curvekey + 1;
  if (curvekey2 >= (CData->curve_keynum[curve] - 1)) {
    curvekey2 = (CData->curve_keynum[curve] - 1);
    curvekey = curvekey2 - 1;
  }
  const float4 mP = CurveSegmentMotionCV(CData, sys, curve, first_curve_key + curvekey);
  const float4 mP2 = CurveSegmentMotionCV(CData, sys, curve, first_curve_key + curvekey2);
  return lerp(mP, mP2, remainder);
}

static void ExportCurveSegmentsMotion(Mesh *mesh, ParticleCurveData *CData, int motion_step)
{
  VLOG(1) << "Exporting curve motion segments for mesh " << mesh->name << ", motion step "
          << motion_step;

  /* find attribute */
  Attribute *attr_mP = mesh->curve_attributes.find(ATTR_STD_MOTION_VERTEX_POSITION);
  bool new_attribute = false;

  /* add new attribute if it doesn't exist already */
  if (!attr_mP) {
    VLOG(1) << "Creating new motion vertex position attribute";
    attr_mP = mesh->curve_attributes.add(ATTR_STD_MOTION_VERTEX_POSITION);
    new_attribute = true;
  }

  /* export motion vectors for curve keys */
  size_t numkeys = mesh->curve_keys.size();
  float4 *mP = attr_mP->data_float4() + motion_step * numkeys;
  bool have_motion = false;
  int i = 0;
  int num_curves = 0;

  for (int sys = 0; sys < CData->psys_firstcurve.size(); sys++) {
    for (int curve = CData->psys_firstcurve[sys];
         curve < CData->psys_firstcurve[sys] + CData->psys_curvenum[sys];
         curve++) {
      /* Curve lengths may not match! Curves can be clipped. */
      int curve_key_end = (num_curves + 1 < (int)mesh->curve_first_key.size() ?
                               mesh->curve_first_key[num_curves + 1] :
                               (int)mesh->curve_keys.size());
      const int num_center_curve_keys = curve_key_end - mesh->curve_first_key[num_curves];
      const int is_num_keys_different = CData->curve_keynum[curve] - num_center_curve_keys;

      if (!is_num_keys_different) {
        for (int curvekey = CData->curve_firstkey[curve];
             curvekey < CData->curve_firstkey[curve] + CData->curve_keynum[curve];
             curvekey++) {
          if (i < mesh->curve_keys.size()) {
            mP[i] = CurveSegmentMotionCV(CData, sys, curve, curvekey);
            if (!have_motion) {
              /* unlike mesh coordinates, these tend to be slightly different
               * between frames due to particle transforms into/out of object
               * space, so we use an epsilon to detect actual changes */
              float4 curve_key = float3_to_float4(mesh->curve_keys[i]);
              curve_key.w = mesh->curve_radius[i];
              if (len_squared(mP[i] - curve_key) > 1e-5f * 1e-5f)
                have_motion = true;
            }
          }
          i++;
        }
      }
      else {
        /* Number of keys has changed. Generate an interpolated version
         * to preserve motion blur. */
        const float step_size = num_center_curve_keys > 1 ? 1.0f / (num_center_curve_keys - 1) :
                                                            0.0f;
        for (int step_index = 0; step_index < num_center_curve_keys; ++step_index) {
          const float step = step_index * step_size;
          mP[i] = LerpCurveSegmentMotionCV(CData, sys, curve, step);
          i++;
        }
        have_motion = true;
      }
      num_curves++;
    }
  }

  /* in case of new attribute, we verify if there really was any motion */
  if (new_attribute) {
    if (i != numkeys || !have_motion) {
      /* No motion or hair "topology" changed, remove attributes again. */
      if (i != numkeys) {
        VLOG(1) << "Hair topology changed, removing attribute.";
      }
      else {
        VLOG(1) << "No motion, removing attribute.";
      }
      mesh->curve_attributes.remove(ATTR_STD_MOTION_VERTEX_POSITION);
    }
    else if (motion_step > 0) {
      VLOG(1) << "Filling in new motion vertex position for motion_step " << motion_step;
      /* motion, fill up previous steps that we might have skipped because
       * they had no motion, but we need them anyway now */
      for (int step = 0; step < motion_step; step++) {
        float4 *mP = attr_mP->data_float4() + step * numkeys;

        for (int key = 0; key < numkeys; key++) {
          mP[key] = float3_to_float4(mesh->curve_keys[key]);
          mP[key].w = mesh->curve_radius[key];
        }
      }
    }
  }
}

static void ExportCurveTriangleUV(ParticleCurveData *CData,
                                  int vert_offset,
                                  int resol,
                                  float2 *uvdata)
{
  if (uvdata == NULL)
    return;
  int vertexindex = vert_offset;

  for (int sys = 0; sys < CData->psys_firstcurve.size(); sys++) {
    for (int curve = CData->psys_firstcurve[sys];
         curve < CData->psys_firstcurve[sys] + CData->psys_curvenum[sys];
         curve++) {
      for (int curvekey = CData->curve_firstkey[curve];
           curvekey < CData->curve_firstkey[curve] + CData->curve_keynum[curve] - 1;
           curvekey++) {
        for (int section = 0; section < resol; section++) {
          uvdata[vertexindex] = CData->curve_uv[curve];
          vertexindex++;
          uvdata[vertexindex] = CData->curve_uv[curve];
          vertexindex++;
          uvdata[vertexindex] = CData->curve_uv[curve];
          vertexindex++;
          uvdata[vertexindex] = CData->curve_uv[curve];
          vertexindex++;
          uvdata[vertexindex] = CData->curve_uv[curve];
          vertexindex++;
          uvdata[vertexindex] = CData->curve_uv[curve];
          vertexindex++;
        }
      }
    }
  }
}

static void ExportCurveTriangleVcol(ParticleCurveData *CData,
                                    int vert_offset,
                                    int resol,
                                    uchar4 *cdata)
{
  if (cdata == NULL)
    return;

  int vertexindex = vert_offset;

  for (int sys = 0; sys < CData->psys_firstcurve.size(); sys++) {
    for (int curve = CData->psys_firstcurve[sys];
         curve < CData->psys_firstcurve[sys] + CData->psys_curvenum[sys];
         curve++) {
      for (int curvekey = CData->curve_firstkey[curve];
           curvekey < CData->curve_firstkey[curve] + CData->curve_keynum[curve] - 1;
           curvekey++) {
        for (int section = 0; section < resol; section++) {
          /* Encode vertex color using the sRGB curve. */
          cdata[vertexindex] = color_float_to_byte(
              color_srgb_to_linear_v3(CData->curve_vcol[curve]));
          vertexindex++;
          cdata[vertexindex] = color_float_to_byte(
              color_srgb_to_linear_v3(CData->curve_vcol[curve]));
          vertexindex++;
          cdata[vertexindex] = color_float_to_byte(
              color_srgb_to_linear_v3(CData->curve_vcol[curve]));
          vertexindex++;
          cdata[vertexindex] = color_float_to_byte(
              color_srgb_to_linear_v3(CData->curve_vcol[curve]));
          vertexindex++;
          cdata[vertexindex] = color_float_to_byte(
              color_srgb_to_linear_v3(CData->curve_vcol[curve]));
          vertexindex++;
          cdata[vertexindex] = color_float_to_byte(
              color_srgb_to_linear_v3(CData->curve_vcol[curve]));
          vertexindex++;
        }
      }
    }
  }
}

/* Hair Curve Sync */

void BlenderSync::sync_curve_settings()
{
  PointerRNA csscene = RNA_pointer_get(&b_scene.ptr, "cycles_curves");

  CurveSystemManager *curve_system_manager = scene->curve_system_manager;
  CurveSystemManager prev_curve_system_manager = *curve_system_manager;

  curve_system_manager->use_curves = get_boolean(csscene, "use_curves");

  curve_system_manager->primitive = (CurvePrimitiveType)get_enum(
      csscene, "primitive", CURVE_NUM_PRIMITIVE_TYPES, CURVE_LINE_SEGMENTS);
  curve_system_manager->curve_shape = (CurveShapeType)get_enum(
      csscene, "shape", CURVE_NUM_SHAPE_TYPES, CURVE_THICK);
  curve_system_manager->resolution = get_int(csscene, "resolution");
  curve_system_manager->subdivisions = get_int(csscene, "subdivisions");
  curve_system_manager->use_backfacing = !get_boolean(csscene, "cull_backfacing");

  /* Triangles */
  if (curve_system_manager->primitive == CURVE_TRIANGLES) {
    /* camera facing planes */
    if (curve_system_manager->curve_shape == CURVE_RIBBON) {
      curve_system_manager->triangle_method = CURVE_CAMERA_TRIANGLES;
      curve_system_manager->resolution = 1;
    }
    else if (curve_system_manager->curve_shape == CURVE_THICK) {
      curve_system_manager->triangle_method = CURVE_TESSELATED_TRIANGLES;
    }
  }
  /* Line Segments */
  else if (curve_system_manager->primitive == CURVE_LINE_SEGMENTS) {
    if (curve_system_manager->curve_shape == CURVE_RIBBON) {
      /* tangent shading */
      curve_system_manager->line_method = CURVE_UNCORRECTED;
      curve_system_manager->use_encasing = true;
      curve_system_manager->use_backfacing = false;
      curve_system_manager->use_tangent_normal_geometry = true;
    }
    else if (curve_system_manager->curve_shape == CURVE_THICK) {
      curve_system_manager->line_method = CURVE_ACCURATE;
      curve_system_manager->use_encasing = false;
      curve_system_manager->use_tangent_normal_geometry = false;
    }
  }
  /* Curve Segments */
  else if (curve_system_manager->primitive == CURVE_SEGMENTS) {
    if (curve_system_manager->curve_shape == CURVE_RIBBON) {
      curve_system_manager->primitive = CURVE_RIBBONS;
      curve_system_manager->use_backfacing = false;
    }
  }

  if (curve_system_manager->modified_mesh(prev_curve_system_manager)) {
    BL::BlendData::objects_iterator b_ob;

    for (b_data.objects.begin(b_ob); b_ob != b_data.objects.end(); ++b_ob) {
      if (object_is_mesh(*b_ob)) {
        BL::Object::particle_systems_iterator b_psys;
        for (b_ob->particle_systems.begin(b_psys); b_psys != b_ob->particle_systems.end();
             ++b_psys) {
          if ((b_psys->settings().render_type() == BL::ParticleSettings::render_type_PATH) &&
              (b_psys->settings().type() == BL::ParticleSettings::type_HAIR)) {
            BL::ID key = BKE_object_is_modified(*b_ob) ? *b_ob : b_ob->data();
            mesh_map.set_recalc(key);
            object_map.set_recalc(*b_ob);
          }
        }
      }
    }
  }

  if (curve_system_manager->modified(prev_curve_system_manager))
    curve_system_manager->tag_update(scene);
}

void BlenderSync::sync_curves(
    Mesh *mesh, BL::Mesh &b_mesh, BL::Object &b_ob, bool motion, int motion_step)
{
  if (!motion) {
    /* Clear stored curve data */
    mesh->curve_keys.clear();
    mesh->curve_radius.clear();
    mesh->curve_first_key.clear();
    mesh->curve_shader.clear();
    mesh->curve_attributes.clear();
  }

  /* obtain general settings */
  const bool use_curves = scene->curve_system_manager->use_curves;

  if (!(use_curves && b_ob.mode() != b_ob.mode_PARTICLE_EDIT && b_ob.mode() != b_ob.mode_EDIT)) {
    if (!motion)
      mesh->compute_bounds();
    return;
  }

  const int primitive = scene->curve_system_manager->primitive;
  const int triangle_method = scene->curve_system_manager->triangle_method;
  const int resolution = scene->curve_system_manager->resolution;
  const size_t vert_num = mesh->verts.size();
  const size_t tri_num = mesh->num_triangles();
  int used_res = 1;

  /* extract particle hair data - should be combined with connecting to mesh later*/

  ParticleCurveData CData;

  ObtainCacheParticleData(mesh, &b_mesh, &b_ob, &CData, !preview);

  /* add hair geometry to mesh */
  if (primitive == CURVE_TRIANGLES) {
    if (triangle_method == CURVE_CAMERA_TRIANGLES) {
      /* obtain camera parameters */
      float3 RotCam;
      Camera *camera = scene->camera;
      Transform &ctfm = camera->matrix;
      if (camera->type == CAMERA_ORTHOGRAPHIC) {
        RotCam = -make_float3(ctfm.x.z, ctfm.y.z, ctfm.z.z);
      }
      else {
        Transform tfm = get_transform(b_ob.matrix_world());
        Transform itfm = transform_quick_inverse(tfm);
        RotCam = transform_point(&itfm, make_float3(ctfm.x.w, ctfm.y.w, ctfm.z.w));
      }
      bool is_ortho = camera->type == CAMERA_ORTHOGRAPHIC;
      ExportCurveTrianglePlanes(mesh, &CData, RotCam, is_ortho);
    }
    else {
      ExportCurveTriangleGeometry(mesh, &CData, resolution);
      used_res = resolution;
    }
  }
  else {
    if (motion)
      ExportCurveSegmentsMotion(mesh, &CData, motion_step);
    else
      ExportCurveSegments(scene, mesh, &CData);
  }

  /* generated coordinates from first key. we should ideally get this from
   * blender to handle deforming objects */
  if (!motion) {
    if (mesh->need_attribute(scene, ATTR_STD_GENERATED)) {
      float3 loc, size;
      mesh_texture_space(b_mesh, loc, size);

      if (primitive == CURVE_TRIANGLES) {
        Attribute *attr_generated = mesh->attributes.add(ATTR_STD_GENERATED);
        float3 *generated = attr_generated->data_float3();

        for (size_t i = vert_num; i < mesh->verts.size(); i++)
          generated[i] = mesh->verts[i] * size - loc;
      }
      else {
        Attribute *attr_generated = mesh->curve_attributes.add(ATTR_STD_GENERATED);
        float3 *generated = attr_generated->data_float3();

        for (size_t i = 0; i < mesh->num_curves(); i++) {
          float3 co = mesh->curve_keys[mesh->get_curve(i).first_key];
          generated[i] = co * size - loc;
        }
      }
    }
  }

  /* create vertex color attributes */
  if (!motion) {
    BL::Mesh::vertex_colors_iterator l;
    int vcol_num = 0;

    for (b_mesh.vertex_colors.begin(l); l != b_mesh.vertex_colors.end(); ++l, vcol_num++) {
      if (!mesh->need_attribute(scene, ustring(l->name().c_str())))
        continue;

      ObtainCacheParticleVcol(mesh, &b_mesh, &b_ob, &CData, !preview, vcol_num);

      if (primitive == CURVE_TRIANGLES) {
        Attribute *attr_vcol = mesh->attributes.add(
            ustring(l->name().c_str()), TypeDesc::TypeColor, ATTR_ELEMENT_CORNER_BYTE);

        uchar4 *cdata = attr_vcol->data_uchar4();

        ExportCurveTriangleVcol(&CData, tri_num * 3, used_res, cdata);
      }
      else {
        Attribute *attr_vcol = mesh->curve_attributes.add(
            ustring(l->name().c_str()), TypeDesc::TypeColor, ATTR_ELEMENT_CURVE);

        float3 *fdata = attr_vcol->data_float3();

        if (fdata) {
          size_t i = 0;

          /* Encode vertex color using the sRGB curve. */
          for (size_t curve = 0; curve < CData.curve_vcol.size(); curve++) {
            fdata[i++] = color_srgb_to_linear_v3(CData.curve_vcol[curve]);
          }
        }
      }
    }
  }

  /* create UV attributes */
  if (!motion) {
    BL::Mesh::uv_layers_iterator l;
    int uv_num = 0;

    for (b_mesh.uv_layers.begin(l); l != b_mesh.uv_layers.end(); ++l, uv_num++) {
      bool active_render = l->active_render();
      AttributeStandard std = (active_render) ? ATTR_STD_UV : ATTR_STD_NONE;
      ustring name = ustring(l->name().c_str());

      /* UV map */
      if (mesh->need_attribute(scene, name) || mesh->need_attribute(scene, std)) {
        Attribute *attr_uv;

        ObtainCacheParticleUV(mesh, &b_mesh, &b_ob, &CData, !preview, uv_num);

        if (primitive == CURVE_TRIANGLES) {
          if (active_render)
            attr_uv = mesh->attributes.add(std, name);
          else
            attr_uv = mesh->attributes.add(name, TypeFloat2, ATTR_ELEMENT_CORNER);

          float2 *uv = attr_uv->data_float2();

          ExportCurveTriangleUV(&CData, tri_num * 3, used_res, uv);
        }
        else {
          if (active_render)
            attr_uv = mesh->curve_attributes.add(std, name);
          else
            attr_uv = mesh->curve_attributes.add(name, TypeFloat2, ATTR_ELEMENT_CURVE);

          float2 *uv = attr_uv->data_float2();

          if (uv) {
            size_t i = 0;

            for (size_t curve = 0; curve < CData.curve_uv.size(); curve++) {
              uv[i++] = CData.curve_uv[curve];
            }
          }
        }
      }
    }
  }

  mesh->compute_bounds();
}

CCL_NAMESPACE_END
