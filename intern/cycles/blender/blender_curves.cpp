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
#include "render/hair.h"
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

static bool ObtainCacheParticleData(
    Hair *hair, BL::Mesh *b_mesh, BL::Object *b_ob, ParticleCurveData *CData, bool background)
{
  int curvenum = 0;
  int keyno = 0;

  if (!(hair && b_mesh && b_ob && CData))
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
        int shader = clamp(b_part.material() - 1, 0, hair->used_shaders.size() - 1);
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
          float3 prev_co_world = make_float3(0.0f, 0.0f, 0.0f);
          float3 prev_co_object = make_float3(0.0f, 0.0f, 0.0f);
          for (int step_no = 0; step_no < ren_step; step_no++) {
            float3 co_world = prev_co_world;
            b_psys.co_hair(*b_ob, pa_no, step_no, &co_world.x);
            float3 co_object = transform_point(&itfm, co_world);
            if (step_no > 0) {
              const float step_length = len(co_object - prev_co_object);
              curve_length += step_length;
            }
            CData->curvekey_co.push_back_slow(co_object);
            CData->curvekey_time.push_back_slow(curve_length);
            prev_co_object = co_object;
            prev_co_world = co_world;
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

static bool ObtainCacheParticleUV(Hair *hair,
                                  BL::Mesh *b_mesh,
                                  BL::Object *b_ob,
                                  ParticleCurveData *CData,
                                  bool background,
                                  int uv_num)
{
  if (!(hair && b_mesh && b_ob && CData))
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

static bool ObtainCacheParticleVcol(Hair *hair,
                                    BL::Mesh *b_mesh,
                                    BL::Object *b_ob,
                                    ParticleCurveData *CData,
                                    bool background,
                                    int vcol_num)
{
  if (!(hair && b_mesh && b_ob && CData))
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

static void ExportCurveSegments(Scene *scene, Hair *hair, ParticleCurveData *CData)
{
  int num_keys = 0;
  int num_curves = 0;

  if (hair->num_curves())
    return;

  Attribute *attr_intercept = NULL;
  Attribute *attr_random = NULL;

  if (hair->need_attribute(scene, ATTR_STD_CURVE_INTERCEPT))
    attr_intercept = hair->attributes.add(ATTR_STD_CURVE_INTERCEPT);
  if (hair->need_attribute(scene, ATTR_STD_CURVE_RANDOM))
    attr_random = hair->attributes.add(ATTR_STD_CURVE_RANDOM);

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
    VLOG(1) << "Exporting curve segments for mesh " << hair->name;
  }

  hair->reserve_curves(hair->num_curves() + num_curves, hair->curve_keys.size() + num_keys);

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
        hair->add_curve_key(ickey_loc, radius);
        if (attr_intercept)
          attr_intercept->add(time);

        num_curve_keys++;
      }

      if (attr_random != NULL) {
        attr_random->add(hash_uint2_to_float(num_curves, 0));
      }

      hair->add_curve(num_keys, CData->psys_shader[sys]);
      num_keys += num_curve_keys;
      num_curves++;
    }
  }

  /* check allocation */
  if ((hair->curve_keys.size() != num_keys) || (hair->num_curves() != num_curves)) {
    VLOG(1) << "Allocation failed, clearing data";
    hair->clear();
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

static void export_hair_motion_validate_attribute(Hair *hair,
                                                  int motion_step,
                                                  int num_motion_keys,
                                                  bool have_motion)
{
  Attribute *attr_mP = hair->attributes.find(ATTR_STD_MOTION_VERTEX_POSITION);
  const int num_keys = hair->curve_keys.size();

  if (num_motion_keys != num_keys || !have_motion) {
    /* No motion or hair "topology" changed, remove attributes again. */
    if (num_motion_keys != num_keys) {
      VLOG(1) << "Hair topology changed, removing attribute.";
    }
    else {
      VLOG(1) << "No motion, removing attribute.";
    }
    hair->attributes.remove(ATTR_STD_MOTION_VERTEX_POSITION);
  }
  else if (motion_step > 0) {
    VLOG(1) << "Filling in new motion vertex position for motion_step " << motion_step;

    /* Motion, fill up previous steps that we might have skipped because
     * they had no motion, but we need them anyway now. */
    for (int step = 0; step < motion_step; step++) {
      float4 *mP = attr_mP->data_float4() + step * num_keys;

      for (int key = 0; key < num_keys; key++) {
        mP[key] = float3_to_float4(hair->curve_keys[key]);
        mP[key].w = hair->curve_radius[key];
      }
    }
  }
}

static void ExportCurveSegmentsMotion(Hair *hair, ParticleCurveData *CData, int motion_step)
{
  VLOG(1) << "Exporting curve motion segments for hair " << hair->name << ", motion step "
          << motion_step;

  /* find attribute */
  Attribute *attr_mP = hair->attributes.find(ATTR_STD_MOTION_VERTEX_POSITION);
  bool new_attribute = false;

  /* add new attribute if it doesn't exist already */
  if (!attr_mP) {
    VLOG(1) << "Creating new motion vertex position attribute";
    attr_mP = hair->attributes.add(ATTR_STD_MOTION_VERTEX_POSITION);
    new_attribute = true;
  }

  /* export motion vectors for curve keys */
  size_t numkeys = hair->curve_keys.size();
  float4 *mP = attr_mP->data_float4() + motion_step * numkeys;
  bool have_motion = false;
  int i = 0;
  int num_curves = 0;

  for (int sys = 0; sys < CData->psys_firstcurve.size(); sys++) {
    for (int curve = CData->psys_firstcurve[sys];
         curve < CData->psys_firstcurve[sys] + CData->psys_curvenum[sys];
         curve++) {
      /* Curve lengths may not match! Curves can be clipped. */
      int curve_key_end = (num_curves + 1 < (int)hair->curve_first_key.size() ?
                               hair->curve_first_key[num_curves + 1] :
                               (int)hair->curve_keys.size());
      const int num_center_curve_keys = curve_key_end - hair->curve_first_key[num_curves];
      const int is_num_keys_different = CData->curve_keynum[curve] - num_center_curve_keys;

      if (!is_num_keys_different) {
        for (int curvekey = CData->curve_firstkey[curve];
             curvekey < CData->curve_firstkey[curve] + CData->curve_keynum[curve];
             curvekey++) {
          if (i < hair->curve_keys.size()) {
            mP[i] = CurveSegmentMotionCV(CData, sys, curve, curvekey);
            if (!have_motion) {
              /* unlike mesh coordinates, these tend to be slightly different
               * between frames due to particle transforms into/out of object
               * space, so we use an epsilon to detect actual changes */
              float4 curve_key = float3_to_float4(hair->curve_keys[i]);
              curve_key.w = hair->curve_radius[i];
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

  /* In case of new attribute, we verify if there really was any motion. */
  if (new_attribute) {
    export_hair_motion_validate_attribute(hair, motion_step, i, have_motion);
  }
}

/* Hair Curve Sync */

void BlenderSync::sync_curve_settings(BL::Depsgraph &b_depsgraph)
{
  PointerRNA csscene = RNA_pointer_get(&b_scene.ptr, "cycles_curves");

  CurveSystemManager *curve_system_manager = scene->curve_system_manager;
  CurveSystemManager prev_curve_system_manager = *curve_system_manager;

  curve_system_manager->use_curves = get_boolean(csscene, "use_curves");

  curve_system_manager->curve_shape = (CurveShapeType)get_enum(
      csscene, "shape", CURVE_NUM_SHAPE_TYPES, CURVE_THICK);
  curve_system_manager->subdivisions = get_int(csscene, "subdivisions");
  curve_system_manager->use_backfacing = !get_boolean(csscene, "cull_backfacing");

  if (curve_system_manager->modified_mesh(prev_curve_system_manager)) {
    BL::Depsgraph::objects_iterator b_ob;

    for (b_depsgraph.objects.begin(b_ob); b_ob != b_data.objects.end(); ++b_ob) {
      if (object_is_mesh(*b_ob)) {
        BL::Object::particle_systems_iterator b_psys;
        for (b_ob->particle_systems.begin(b_psys); b_psys != b_ob->particle_systems.end();
             ++b_psys) {
          if ((b_psys->settings().render_type() == BL::ParticleSettings::render_type_PATH) &&
              (b_psys->settings().type() == BL::ParticleSettings::type_HAIR)) {
            BL::ID key = BKE_object_is_modified(*b_ob) ? *b_ob : b_ob->data();
            geometry_map.set_recalc(key);
            object_map.set_recalc(*b_ob);
          }
        }
      }
    }
  }

  if (curve_system_manager->modified(prev_curve_system_manager))
    curve_system_manager->tag_update(scene);
}

bool BlenderSync::object_has_particle_hair(BL::Object b_ob)
{
  /* Test if the object has a particle modifier with hair. */
  BL::Object::modifiers_iterator b_mod;
  for (b_ob.modifiers.begin(b_mod); b_mod != b_ob.modifiers.end(); ++b_mod) {
    if ((b_mod->type() == b_mod->type_PARTICLE_SYSTEM) &&
        (preview ? b_mod->show_viewport() : b_mod->show_render())) {
      BL::ParticleSystemModifier psmd((const PointerRNA)b_mod->ptr);
      BL::ParticleSystem b_psys((const PointerRNA)psmd.particle_system().ptr);
      BL::ParticleSettings b_part((const PointerRNA)b_psys.settings().ptr);

      if ((b_part.render_type() == BL::ParticleSettings::render_type_PATH) &&
          (b_part.type() == BL::ParticleSettings::type_HAIR)) {
        return true;
      }
    }
  }

  return false;
}

/* Old particle hair. */
void BlenderSync::sync_particle_hair(
    Hair *hair, BL::Mesh &b_mesh, BL::Object &b_ob, bool motion, int motion_step)
{
  /* obtain general settings */
  if (b_ob.mode() == b_ob.mode_PARTICLE_EDIT || b_ob.mode() == b_ob.mode_EDIT) {
    return;
  }

  /* extract particle hair data - should be combined with connecting to mesh later*/

  ParticleCurveData CData;

  ObtainCacheParticleData(hair, &b_mesh, &b_ob, &CData, !preview);

  /* add hair geometry */
  if (motion)
    ExportCurveSegmentsMotion(hair, &CData, motion_step);
  else
    ExportCurveSegments(scene, hair, &CData);

  /* generated coordinates from first key. we should ideally get this from
   * blender to handle deforming objects */
  if (!motion) {
    if (hair->need_attribute(scene, ATTR_STD_GENERATED)) {
      float3 loc, size;
      mesh_texture_space(b_mesh, loc, size);

      Attribute *attr_generated = hair->attributes.add(ATTR_STD_GENERATED);
      float3 *generated = attr_generated->data_float3();

      for (size_t i = 0; i < hair->num_curves(); i++) {
        float3 co = hair->curve_keys[hair->get_curve(i).first_key];
        generated[i] = co * size - loc;
      }
    }
  }

  /* create vertex color attributes */
  if (!motion) {
    BL::Mesh::vertex_colors_iterator l;
    int vcol_num = 0;

    for (b_mesh.vertex_colors.begin(l); l != b_mesh.vertex_colors.end(); ++l, vcol_num++) {
      if (!hair->need_attribute(scene, ustring(l->name().c_str())))
        continue;

      ObtainCacheParticleVcol(hair, &b_mesh, &b_ob, &CData, !preview, vcol_num);

      Attribute *attr_vcol = hair->attributes.add(
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

  /* create UV attributes */
  if (!motion) {
    BL::Mesh::uv_layers_iterator l;
    int uv_num = 0;

    for (b_mesh.uv_layers.begin(l); l != b_mesh.uv_layers.end(); ++l, uv_num++) {
      bool active_render = l->active_render();
      AttributeStandard std = (active_render) ? ATTR_STD_UV : ATTR_STD_NONE;
      ustring name = ustring(l->name().c_str());

      /* UV map */
      if (hair->need_attribute(scene, name) || hair->need_attribute(scene, std)) {
        Attribute *attr_uv;

        ObtainCacheParticleUV(hair, &b_mesh, &b_ob, &CData, !preview, uv_num);

        if (active_render)
          attr_uv = hair->attributes.add(std, name);
        else
          attr_uv = hair->attributes.add(name, TypeFloat2, ATTR_ELEMENT_CURVE);

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

#ifdef WITH_NEW_OBJECT_TYPES
static float4 hair_point_as_float4(BL::HairPoint b_point)
{
  float4 mP = float3_to_float4(get_float3(b_point.co()));
  mP.w = b_point.radius();
  return mP;
}

static float4 interpolate_hair_points(BL::Hair b_hair,
                                      const int first_point_index,
                                      const int num_points,
                                      const float step)
{
  const float curve_t = step * (num_points - 1);
  const int point_a = clamp((int)curve_t, 0, num_points - 1);
  const int point_b = min(point_a + 1, num_points - 1);
  const float t = curve_t - (float)point_a;
  return lerp(hair_point_as_float4(b_hair.points[first_point_index + point_a]),
              hair_point_as_float4(b_hair.points[first_point_index + point_b]),
              t);
}

static void export_hair_curves(Scene *scene, Hair *hair, BL::Hair b_hair)
{
  /* TODO: optimize so we can straight memcpy arrays from Blender? */

  /* Add requested attributes. */
  Attribute *attr_intercept = NULL;
  Attribute *attr_random = NULL;

  if (hair->need_attribute(scene, ATTR_STD_CURVE_INTERCEPT)) {
    attr_intercept = hair->attributes.add(ATTR_STD_CURVE_INTERCEPT);
  }
  if (hair->need_attribute(scene, ATTR_STD_CURVE_RANDOM)) {
    attr_random = hair->attributes.add(ATTR_STD_CURVE_RANDOM);
  }

  /* Reserve memory. */
  const int num_keys = b_hair.points.length();
  const int num_curves = b_hair.curves.length();

  if (num_curves > 0) {
    VLOG(1) << "Exporting curve segments for hair " << hair->name;
  }

  hair->reserve_curves(num_curves, num_keys);

  /* Export curves and points. */
  vector<float> points_length;

  BL::Hair::curves_iterator b_curve_iter;
  for (b_hair.curves.begin(b_curve_iter); b_curve_iter != b_hair.curves.end(); ++b_curve_iter) {
    BL::HairCurve b_curve = *b_curve_iter;
    const int first_point_index = b_curve.first_point_index();
    const int num_points = b_curve.num_points();

    float3 prev_co = make_float3(0.0f, 0.0f, 0.0f);
    float length = 0.0f;
    if (attr_intercept) {
      points_length.clear();
      points_length.reserve(num_points);
    }

    /* Position and radius. */
    for (int i = 0; i < num_points; i++) {
      BL::HairPoint b_point = b_hair.points[first_point_index + i];

      const float3 co = get_float3(b_point.co());
      const float radius = b_point.radius();
      hair->add_curve_key(co, radius);

      if (attr_intercept) {
        if (i > 0) {
          length += len(co - prev_co);
          points_length.push_back(length);
        }
        prev_co = co;
      }
    }

    /* Normalized 0..1 attribute along curve. */
    if (attr_intercept) {
      for (int i = 0; i < num_points; i++) {
        attr_intercept->add((length == 0.0f) ? 0.0f : points_length[i] / length);
      }
    }

    /* Random number per curve. */
    if (attr_random != NULL) {
      attr_random->add(hash_uint2_to_float(b_curve.index(), 0));
    }

    /* Curve. */
    const int shader_index = 0;
    hair->add_curve(first_point_index, shader_index);
  }
}

static void export_hair_curves_motion(Hair *hair, BL::Hair b_hair, int motion_step)
{
  VLOG(1) << "Exporting curve motion segments for hair " << hair->name << ", motion step "
          << motion_step;

  /* Find or add attribute. */
  Attribute *attr_mP = hair->attributes.find(ATTR_STD_MOTION_VERTEX_POSITION);
  bool new_attribute = false;

  if (!attr_mP) {
    VLOG(1) << "Creating new motion vertex position attribute";
    attr_mP = hair->attributes.add(ATTR_STD_MOTION_VERTEX_POSITION);
    new_attribute = true;
  }

  /* Export motion keys. */
  const int num_keys = hair->curve_keys.size();
  float4 *mP = attr_mP->data_float4() + motion_step * num_keys;
  bool have_motion = false;
  int num_motion_keys = 0;
  int curve_index = 0;

  BL::Hair::curves_iterator b_curve_iter;
  for (b_hair.curves.begin(b_curve_iter); b_curve_iter != b_hair.curves.end(); ++b_curve_iter) {
    BL::HairCurve b_curve = *b_curve_iter;
    const int first_point_index = b_curve.first_point_index();
    const int num_points = b_curve.num_points();

    Hair::Curve curve = hair->get_curve(curve_index);
    curve_index++;

    if (num_points == curve.num_keys) {
      /* Number of keys matches. */
      for (int i = 0; i < num_points; i++) {
        int point_index = first_point_index + i;

        if (point_index < num_keys) {
          mP[num_motion_keys] = hair_point_as_float4(b_hair.points[point_index]);
          num_motion_keys++;

          if (!have_motion) {
            /* TODO: use epsilon for comparison? Was needed for particles due to
             * transform, but ideally should not happen anymore. */
            float4 curve_key = float3_to_float4(hair->curve_keys[i]);
            curve_key.w = hair->curve_radius[i];
            have_motion = !(mP[i] == curve_key);
          }
        }
      }
    }
    else {
      /* Number of keys has changed. Generate an interpolated version
       * to preserve motion blur. */
      const float step_size = curve.num_keys > 1 ? 1.0f / (curve.num_keys - 1) : 0.0f;
      for (int i = 0; i < curve.num_keys; i++) {
        const float step = i * step_size;
        mP[num_motion_keys] = interpolate_hair_points(b_hair, first_point_index, num_points, step);
        num_motion_keys++;
      }
      have_motion = true;
    }
  }

  /* In case of new attribute, we verify if there really was any motion. */
  if (new_attribute) {
    export_hair_motion_validate_attribute(hair, motion_step, num_motion_keys, have_motion);
  }
}
#endif /* WITH_NEW_OBJECT_TYPES */

/* Hair object. */
void BlenderSync::sync_hair(Hair *hair, BL::Object &b_ob, bool motion, int motion_step)
{
#ifdef WITH_NEW_OBJECT_TYPES
  /* Convert Blender hair to Cycles curves. */
  BL::Hair b_hair(b_ob.data());
  if (motion) {
    export_hair_curves_motion(hair, b_hair, motion_step);
  }
  else {
    export_hair_curves(scene, hair, b_hair);
  }
#else
  (void)hair;
  (void)b_ob;
  (void)motion;
  (void)motion_step;
#endif /* WITH_NEW_OBJECT_TYPES */
}

void BlenderSync::sync_hair(BL::Depsgraph b_depsgraph,
                            BL::Object b_ob,
                            Hair *hair,
                            const vector<Shader *> &used_shaders)
{
  /* Compares curve_keys rather than strands in order to handle quick hair
   * adjustments in dynamic BVH - other methods could probably do this better. */
  array<float3> oldcurve_keys;
  array<float> oldcurve_radius;
  oldcurve_keys.steal_data(hair->curve_keys);
  oldcurve_radius.steal_data(hair->curve_radius);

  hair->clear();
  hair->used_shaders = used_shaders;

  if (view_layer.use_hair && scene->curve_system_manager->use_curves) {
#ifdef WITH_NEW_OBJECT_TYPES
    if (b_ob.type() == BL::Object::type_HAIR) {
      /* Hair object. */
      sync_hair(hair, b_ob, false);
      assert(mesh == NULL);
    }
    else
#endif
    {
      /* Particle hair. */
      bool need_undeformed = hair->need_attribute(scene, ATTR_STD_GENERATED);
      BL::Mesh b_mesh = object_to_mesh(
          b_data, b_ob, b_depsgraph, need_undeformed, Mesh::SUBDIVISION_NONE);

      if (b_mesh) {
        sync_particle_hair(hair, b_mesh, b_ob, false);
        free_object_to_mesh(b_data, b_ob, b_mesh);
      }
    }
  }

  /* tag update */
  const bool rebuild = ((oldcurve_keys != hair->curve_keys) ||
                        (oldcurve_radius != hair->curve_radius));

  hair->tag_update(scene, rebuild);
}

void BlenderSync::sync_hair_motion(BL::Depsgraph b_depsgraph,
                                   BL::Object b_ob,
                                   Hair *hair,
                                   int motion_step)
{
  /* Skip if nothing exported. */
  if (hair->num_keys() == 0) {
    return;
  }

  /* Export deformed coordinates. */
  if (ccl::BKE_object_is_deform_modified(b_ob, b_scene, preview)) {
#ifdef WITH_NEW_OBJECT_TYPES
    if (b_ob.type() == BL::Object::type_HAIR) {
      /* Hair object. */
      sync_hair(hair, b_ob, true, motion_step);
      assert(mesh == NULL);
      return;
    }
    else
#endif
    {
      /* Particle hair. */
      BL::Mesh b_mesh = object_to_mesh(b_data, b_ob, b_depsgraph, false, Mesh::SUBDIVISION_NONE);
      if (b_mesh) {
        sync_particle_hair(hair, b_mesh, b_ob, true, motion_step);
        free_object_to_mesh(b_data, b_ob, b_mesh);
        return;
      }
    }
  }

  /* No deformation on this frame, copy coordinates if other frames did have it. */
  hair->copy_center_to_motion_step(motion_step);
}

CCL_NAMESPACE_END
