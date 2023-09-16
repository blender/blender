/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include <optional>

#include "BKE_curves.hh"
#include "blender/sync.h"
#include "blender/util.h"

#include "scene/attribute.h"
#include "scene/camera.h"
#include "scene/curves.h"
#include "scene/hair.h"
#include "scene/object.h"
#include "scene/scene.h"

#include "util/color.h"
#include "util/foreach.h"
#include "util/hash.h"
#include "util/log.h"

CCL_NAMESPACE_BEGIN

ParticleCurveData::ParticleCurveData() {}

ParticleCurveData::~ParticleCurveData() {}

static float shaperadius(float shape, float root, float tip, float time)
{
  assert(time >= 0.0f);
  assert(time <= 1.0f);
  float radius = 1.0f - time;

  if (shape != 0.0f) {
    if (shape < 0.0f) {
      radius = powf(radius, 1.0f + shape);
    }
    else {
      radius = powf(radius, 1.0f / (1.0f - shape));
    }
  }
  return (radius * (root - tip)) + tip;
}

/* curve functions */

static bool ObtainCacheParticleData(
    Hair *hair, BL::Mesh *b_mesh, BL::Object *b_ob, ParticleCurveData *CData, bool background)
{
  int curvenum = 0;
  int keyno = 0;

  if (!(hair && b_mesh && b_ob && CData)) {
    return false;
  }

  Transform tfm = get_transform(b_ob->matrix_world());
  Transform itfm = transform_inverse(tfm);

  for (BL::Modifier &b_mod : b_ob->modifiers) {
    if ((b_mod.type() == b_mod.type_PARTICLE_SYSTEM) &&
        (background ? b_mod.show_render() : b_mod.show_viewport()))
    {
      BL::ParticleSystemModifier psmd((const PointerRNA)b_mod.ptr);
      BL::ParticleSystem b_psys((const PointerRNA)psmd.particle_system().ptr);
      BL::ParticleSettings b_part((const PointerRNA)b_psys.settings().ptr);

      if ((b_part.render_type() == BL::ParticleSettings::render_type_PATH) &&
          (b_part.type() == BL::ParticleSettings::type_HAIR))
      {
        int shader = clamp(b_part.material() - 1, 0, hair->get_used_shaders().size() - 1);
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
          float3 prev_co_world = zero_float3();
          float3 prev_co_object = zero_float3();
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
  if (!(hair && b_mesh && b_ob && CData)) {
    return false;
  }

  CData->curve_uv.clear();

  for (BL::Modifier &b_mod : b_ob->modifiers) {
    if ((b_mod.type() == b_mod.type_PARTICLE_SYSTEM) &&
        (background ? b_mod.show_render() : b_mod.show_viewport()))
    {
      BL::ParticleSystemModifier psmd((const PointerRNA)b_mod.ptr);
      BL::ParticleSystem b_psys((const PointerRNA)psmd.particle_system().ptr);
      BL::ParticleSettings b_part((const PointerRNA)b_psys.settings().ptr);

      if ((b_part.render_type() == BL::ParticleSettings::render_type_PATH) &&
          (b_part.type() == BL::ParticleSettings::type_HAIR))
      {
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

          float2 uv = zero_float2();
          if (!b_mesh->uv_layers.empty())
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
  if (!(hair && b_mesh && b_ob && CData)) {
    return false;
  }

  CData->curve_vcol.clear();

  for (BL::Modifier &b_mod : b_ob->modifiers) {
    if ((b_mod.type() == b_mod.type_PARTICLE_SYSTEM) &&
        (background ? b_mod.show_render() : b_mod.show_viewport()))
    {
      BL::ParticleSystemModifier psmd((const PointerRNA)b_mod.ptr);
      BL::ParticleSystem b_psys((const PointerRNA)psmd.particle_system().ptr);
      BL::ParticleSettings b_part((const PointerRNA)b_psys.settings().ptr);

      if ((b_part.render_type() == BL::ParticleSettings::render_type_PATH) &&
          (b_part.type() == BL::ParticleSettings::type_HAIR))
      {
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

          float4 vcol = make_float4(0.0f, 0.0f, 0.0f, 1.0f);
          if (!b_mesh->vertex_colors.empty())
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

  if (hair->num_curves()) {
    return;
  }

  Attribute *attr_normal = NULL;
  Attribute *attr_intercept = NULL;
  Attribute *attr_length = NULL;
  Attribute *attr_random = NULL;

  if (hair->need_attribute(scene, ATTR_STD_VERTEX_NORMAL)) {
    attr_normal = hair->attributes.add(ATTR_STD_VERTEX_NORMAL);
  }
  if (hair->need_attribute(scene, ATTR_STD_CURVE_INTERCEPT)) {
    attr_intercept = hair->attributes.add(ATTR_STD_CURVE_INTERCEPT);
  }
  if (hair->need_attribute(scene, ATTR_STD_CURVE_LENGTH)) {
    attr_length = hair->attributes.add(ATTR_STD_CURVE_LENGTH);
  }
  if (hair->need_attribute(scene, ATTR_STD_CURVE_RANDOM)) {
    attr_random = hair->attributes.add(ATTR_STD_CURVE_RANDOM);
  }

  /* compute and reserve size of arrays */
  for (int sys = 0; sys < CData->psys_firstcurve.size(); sys++) {
    for (int curve = CData->psys_firstcurve[sys];
         curve < CData->psys_firstcurve[sys] + CData->psys_curvenum[sys];
         curve++)
    {
      num_keys += CData->curve_keynum[curve];
      num_curves++;
    }
  }

  hair->reserve_curves(hair->num_curves() + num_curves, hair->get_curve_keys().size() + num_keys);

  num_keys = 0;
  num_curves = 0;

  /* actually export */
  for (int sys = 0; sys < CData->psys_firstcurve.size(); sys++) {
    for (int curve = CData->psys_firstcurve[sys];
         curve < CData->psys_firstcurve[sys] + CData->psys_curvenum[sys];
         curve++)
    {
      size_t num_curve_keys = 0;

      for (int curvekey = CData->curve_firstkey[curve];
           curvekey < CData->curve_firstkey[curve] + CData->curve_keynum[curve];
           curvekey++)
      {
        const float3 ickey_loc = CData->curvekey_co[curvekey];
        const float curve_time = CData->curvekey_time[curvekey];
        const float curve_length = CData->curve_length[curve];
        const float time = (curve_length > 0.0f) ? curve_time / curve_length : 0.0f;
        float radius = shaperadius(
            CData->psys_shape[sys], CData->psys_rootradius[sys], CData->psys_tipradius[sys], time);
        if (CData->psys_closetip[sys] &&
            (curvekey == CData->curve_firstkey[curve] + CData->curve_keynum[curve] - 1))
        {
          radius = 0.0f;
        }
        hair->add_curve_key(ickey_loc, radius);
        if (attr_intercept) {
          attr_intercept->add(time);
        }

        if (attr_normal) {
          /* NOTE: the geometry normals are not computed for legacy particle hairs. This hair
           * system is expected to be deprecated. */
          attr_normal->add(make_float3(0.0f, 0.0f, 0.0f));
        }

        num_curve_keys++;
      }

      if (attr_length != NULL) {
        attr_length->add(CData->curve_length[curve]);
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
  if ((hair->get_curve_keys().size() != num_keys) || (hair->num_curves() != num_curves)) {
    VLOG_WARNING << "Hair memory allocation failed, clearing data.";
    hair->clear(true);
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
  {
    radius = 0.0f;
  }

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
  return mix(mP, mP2, remainder);
}

static void export_hair_motion_validate_attribute(Hair *hair,
                                                  int motion_step,
                                                  int num_motion_keys,
                                                  bool have_motion)
{
  Attribute *attr_mP = hair->attributes.find(ATTR_STD_MOTION_VERTEX_POSITION);
  const int num_keys = hair->get_curve_keys().size();

  if (num_motion_keys != num_keys || !have_motion) {
    /* No motion or hair "topology" changed, remove attributes again. */
    if (num_motion_keys != num_keys) {
      VLOG_WORK << "Hair topology changed, removing motion attribute.";
    }
    hair->attributes.remove(ATTR_STD_MOTION_VERTEX_POSITION);
  }
  else if (motion_step > 0) {
    /* Motion, fill up previous steps that we might have skipped because
     * they had no motion, but we need them anyway now. */
    for (int step = 0; step < motion_step; step++) {
      float4 *mP = attr_mP->data_float4() + step * num_keys;

      for (int key = 0; key < num_keys; key++) {
        mP[key] = float3_to_float4(hair->get_curve_keys()[key]);
        mP[key].w = hair->get_curve_radius()[key];
      }
    }
  }
}

static void ExportCurveSegmentsMotion(Hair *hair, ParticleCurveData *CData, int motion_step)
{
  /* find attribute */
  Attribute *attr_mP = hair->attributes.find(ATTR_STD_MOTION_VERTEX_POSITION);
  bool new_attribute = false;

  /* add new attribute if it doesn't exist already */
  if (!attr_mP) {
    attr_mP = hair->attributes.add(ATTR_STD_MOTION_VERTEX_POSITION);
    new_attribute = true;
  }

  /* export motion vectors for curve keys */
  size_t numkeys = hair->get_curve_keys().size();
  float4 *mP = attr_mP->data_float4() + motion_step * numkeys;
  bool have_motion = false;
  int i = 0;
  int num_curves = 0;

  for (int sys = 0; sys < CData->psys_firstcurve.size(); sys++) {
    for (int curve = CData->psys_firstcurve[sys];
         curve < CData->psys_firstcurve[sys] + CData->psys_curvenum[sys];
         curve++)
    {
      /* Curve lengths may not match! Curves can be clipped. */
      int curve_key_end = (num_curves + 1 < (int)hair->get_curve_first_key().size() ?
                               hair->get_curve_first_key()[num_curves + 1] :
                               (int)hair->get_curve_keys().size());
      const int num_center_curve_keys = curve_key_end - hair->get_curve_first_key()[num_curves];
      const int is_num_keys_different = CData->curve_keynum[curve] - num_center_curve_keys;

      if (!is_num_keys_different) {
        for (int curvekey = CData->curve_firstkey[curve];
             curvekey < CData->curve_firstkey[curve] + CData->curve_keynum[curve];
             curvekey++)
        {
          if (i < hair->get_curve_keys().size()) {
            mP[i] = CurveSegmentMotionCV(CData, sys, curve, curvekey);
            if (!have_motion) {
              /* unlike mesh coordinates, these tend to be slightly different
               * between frames due to particle transforms into/out of object
               * space, so we use an epsilon to detect actual changes */
              float4 curve_key = float3_to_float4(hair->get_curve_keys()[i]);
              curve_key.w = hair->get_curve_radius()[i];
              if (len_squared(mP[i] - curve_key) > 1e-5f * 1e-5f) {
                have_motion = true;
              }
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

bool BlenderSync::object_has_particle_hair(BL::Object b_ob)
{
  /* Test if the object has a particle modifier with hair. */
  for (BL::Modifier &b_mod : b_ob.modifiers) {
    if ((b_mod.type() == b_mod.type_PARTICLE_SYSTEM) &&
        (preview ? b_mod.show_viewport() : b_mod.show_render()))
    {
      BL::ParticleSystemModifier psmd((const PointerRNA)b_mod.ptr);
      BL::ParticleSystem b_psys((const PointerRNA)psmd.particle_system().ptr);
      BL::ParticleSettings b_part((const PointerRNA)b_psys.settings().ptr);

      if ((b_part.render_type() == BL::ParticleSettings::render_type_PATH) &&
          (b_part.type() == BL::ParticleSettings::type_HAIR))
      {
        return true;
      }
    }
  }

  return false;
}

/* Old particle hair. */
void BlenderSync::sync_particle_hair(
    Hair *hair, BL::Mesh &b_mesh, BObjectInfo &b_ob_info, bool motion, int motion_step)
{
  if (!b_ob_info.is_real_object_data()) {
    return;
  }
  BL::Object b_ob = b_ob_info.real_object;

  /* obtain general settings */
  if (b_ob.mode() == b_ob.mode_PARTICLE_EDIT || b_ob.mode() == b_ob.mode_EDIT) {
    return;
  }

  /* Extract particle hair data - should be combined with connecting to mesh later. */

  ParticleCurveData CData;

  ObtainCacheParticleData(hair, &b_mesh, &b_ob, &CData, !preview);

  /* add hair geometry */
  if (motion) {
    ExportCurveSegmentsMotion(hair, &CData, motion_step);
  }
  else {
    ExportCurveSegments(scene, hair, &CData);
  }

  /* generated coordinates from first key. we should ideally get this from
   * blender to handle deforming objects */
  if (!motion) {
    if (hair->need_attribute(scene, ATTR_STD_GENERATED)) {
      float3 loc, size;
      mesh_texture_space(b_mesh, loc, size);

      Attribute *attr_generated = hair->attributes.add(ATTR_STD_GENERATED);
      float3 *generated = attr_generated->data_float3();

      for (size_t i = 0; i < hair->num_curves(); i++) {
        float3 co = hair->get_curve_keys()[hair->get_curve(i).first_key];
        generated[i] = co * size - loc;
      }
    }
  }

  /* create vertex color attributes */
  if (!motion) {
    BL::Mesh::vertex_colors_iterator l;
    int vcol_num = 0;

    for (b_mesh.vertex_colors.begin(l); l != b_mesh.vertex_colors.end(); ++l, vcol_num++) {
      if (!hair->need_attribute(scene, ustring(l->name().c_str()))) {
        continue;
      }

      ObtainCacheParticleVcol(hair, &b_mesh, &b_ob, &CData, !preview, vcol_num);

      Attribute *attr_vcol = hair->attributes.add(
          ustring(l->name().c_str()), TypeRGBA, ATTR_ELEMENT_CURVE);

      float4 *fdata = attr_vcol->data_float4();

      if (fdata) {
        size_t i = 0;

        /* Encode vertex color using the sRGB curve. */
        for (size_t curve = 0; curve < CData.curve_vcol.size(); curve++) {
          fdata[i++] = color_srgb_to_linear_v4(CData.curve_vcol[curve]);
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

        if (active_render) {
          attr_uv = hair->attributes.add(std, name);
        }
        else {
          attr_uv = hair->attributes.add(name, TypeFloat2, ATTR_ELEMENT_CURVE);
        }

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

static const float *find_radius_attribute(BL::Curves b_curves)
{
  for (BL::Attribute &b_attribute : b_curves.attributes) {
    if (b_attribute.name() != "radius") {
      continue;
    }
    if (b_attribute.data_type() != BL::Attribute::data_type_FLOAT) {
      continue;
    }
    BL::FloatAttribute b_float_attribute{b_attribute};
    if (b_float_attribute.data.length() == 0) {
      return nullptr;
    }
    return static_cast<const float *>(b_float_attribute.data[0].ptr.data);
  }
  return nullptr;
}

static const float (*find_position_attribute(BL::Curves b_curves))[3]
{
  for (BL::Attribute &b_attribute : b_curves.attributes) {
    if (b_attribute.name() != "position") {
      continue;
    }
    if (b_attribute.data_type() != BL::Attribute::data_type_FLOAT_VECTOR) {
      continue;
    }
    BL::FloatVectorAttribute b_float3_attribute{b_attribute};
    if (b_float3_attribute.data.length() == 0) {
      return nullptr;
    }
    return static_cast<const float(*)[3]>(b_float3_attribute.data[0].ptr.data);
  }
  /* The position attribute must exist. */
  assert(false);
  return nullptr;
}

template<typename TypeInCycles, typename GetValueAtIndex>
static void fill_generic_attribute(const int num_curves,
                                   const int num_points,
                                   TypeInCycles *data,
                                   const AttributeElement element,
                                   const GetValueAtIndex &get_value_at_index)
{
  switch (element) {
    case ATTR_ELEMENT_CURVE_KEY: {
      for (int i = 0; i < num_points; i++) {
        data[i] = get_value_at_index(i);
      }
      break;
    }
    case ATTR_ELEMENT_CURVE: {
      for (int i = 0; i < num_curves; i++) {
        data[i] = get_value_at_index(i);
      }
      break;
    }
    default: {
      assert(false);
      break;
    }
  }
}

static void attr_create_motion(Hair *hair, BL::Attribute &b_attribute, const float motion_scale)
{
  if (!(b_attribute.domain() == BL::Attribute::domain_POINT) &&
      (b_attribute.data_type() == BL::Attribute::data_type_FLOAT_VECTOR))
  {
    return;
  }

  BL::FloatVectorAttribute b_vector_attribute(b_attribute);
  const float(*src)[3] = static_cast<const float(*)[3]>(b_vector_attribute.data[0].ptr.data);
  const int num_curve_keys = hair->get_curve_keys().size();

  /* Find or add attribute */
  float3 *P = &hair->get_curve_keys()[0];
  Attribute *attr_mP = hair->attributes.find(ATTR_STD_MOTION_VERTEX_POSITION);

  if (!attr_mP) {
    attr_mP = hair->attributes.add(ATTR_STD_MOTION_VERTEX_POSITION);
  }

  /* Only export previous and next frame, we don't have any in between data. */
  float motion_times[2] = {-1.0f, 1.0f};
  for (int step = 0; step < 2; step++) {
    const float relative_time = motion_times[step] * 0.5f * motion_scale;
    float3 *mP = attr_mP->data_float3() + step * num_curve_keys;

    for (int i = 0; i < num_curve_keys; i++) {
      mP[i] = P[i] + make_float3(src[i][0], src[i][1], src[i][2]) * relative_time;
    }
  }
}

static void attr_create_uv(AttributeSet &attributes,
                           const int num_curves,
                           const int num_points,
                           BL::Attribute &b_attribute,
                           const ustring name)
{
  BL::Float2Attribute b_float2_attribute{b_attribute};
  const float(*src)[2] = static_cast<const float(*)[2]>(b_float2_attribute.data[0].ptr.data);
  Attribute *attr = attributes.add(ATTR_STD_UV, name);

  float2 *data = attr->data_float2();
  fill_generic_attribute(num_curves, num_points, data, ATTR_ELEMENT_CURVE, [&](int i) {
    return make_float2(src[i][0], src[i][1]);
  });
}

static void attr_create_generic(Scene *scene,
                                Hair *hair,
                                BL::Curves &b_curves,
                                const bool need_motion,
                                const float motion_scale)
{
  const int num_keys = b_curves.points.length();
  const int num_curves = b_curves.curves.length();

  AttributeSet &attributes = hair->attributes;
  static const ustring u_velocity("velocity");
  const bool need_uv = hair->need_attribute(scene, ATTR_STD_UV);
  bool have_uv = false;

  for (BL::Attribute &b_attribute : b_curves.attributes) {
    const ustring name{b_attribute.name().c_str()};

    const BL::Attribute::domain_enum b_domain = b_attribute.domain();
    const BL::Attribute::data_type_enum b_data_type = b_attribute.data_type();

    if (need_motion && name == u_velocity) {
      attr_create_motion(hair, b_attribute, motion_scale);
      continue;
    }

    /* Weak, use first float2 attribute as standard UV. */
    if (need_uv && !have_uv && b_data_type == BL::Attribute::data_type_FLOAT2 &&
        b_domain == BL::Attribute::domain_CURVE)
    {
      attr_create_uv(attributes, num_curves, num_keys, b_attribute, name);
      have_uv = true;
      continue;
    }

    if (!hair->need_attribute(scene, name)) {
      continue;
    }
    if (attributes.find(name)) {
      continue;
    }

    AttributeElement element = ATTR_ELEMENT_NONE;
    switch (b_domain) {
      case BL::Attribute::domain_POINT:
        element = ATTR_ELEMENT_CURVE_KEY;
        break;
      case BL::Attribute::domain_CURVE:
        element = ATTR_ELEMENT_CURVE;
        break;
      default:
        break;
    }
    if (element == ATTR_ELEMENT_NONE) {
      /* Not supported. */
      continue;
    }
    switch (b_data_type) {
      case BL::Attribute::data_type_FLOAT: {
        BL::FloatAttribute b_float_attribute{b_attribute};
        const float *src = static_cast<const float *>(b_float_attribute.data[0].ptr.data);
        Attribute *attr = attributes.add(name, TypeFloat, element);
        float *data = attr->data_float();
        fill_generic_attribute(num_curves, num_keys, data, element, [&](int i) { return src[i]; });
        break;
      }
      case BL::Attribute::data_type_BOOLEAN: {
        BL::BoolAttribute b_bool_attribute{b_attribute};
        const bool *src = static_cast<const bool *>(b_bool_attribute.data[0].ptr.data);
        Attribute *attr = attributes.add(name, TypeFloat, element);
        float *data = attr->data_float();
        fill_generic_attribute(
            num_curves, num_keys, data, element, [&](int i) { return float(src[i]); });
        break;
      }
      case BL::Attribute::data_type_INT: {
        BL::IntAttribute b_int_attribute{b_attribute};
        const int *src = static_cast<const int *>(b_int_attribute.data[0].ptr.data);
        Attribute *attr = attributes.add(name, TypeFloat, element);
        float *data = attr->data_float();
        fill_generic_attribute(
            num_curves, num_keys, data, element, [&](int i) { return float(src[i]); });
        break;
      }
      case BL::Attribute::data_type_INT32_2D: {
        BL::Int2Attribute b_int2_attribute{b_attribute};
        const int2 *src = static_cast<const int2 *>(b_int2_attribute.data[0].ptr.data);
        Attribute *attr = attributes.add(name, TypeFloat2, element);
        float2 *data = attr->data_float2();
        fill_generic_attribute(num_curves, num_keys, data, element, [&](int i) {
          return make_float2(float(src[i][0]), float(src[i][1]));
        });
        break;
      }
      case BL::Attribute::data_type_FLOAT_VECTOR: {
        BL::FloatVectorAttribute b_vector_attribute{b_attribute};
        const float(*src)[3] = static_cast<const float(*)[3]>(b_vector_attribute.data[0].ptr.data);
        Attribute *attr = attributes.add(name, TypeVector, element);
        float3 *data = attr->data_float3();
        fill_generic_attribute(num_curves, num_keys, data, element, [&](int i) {
          return make_float3(src[i][0], src[i][1], src[i][2]);
        });
        break;
      }
      case BL::Attribute::data_type_BYTE_COLOR: {
        BL::ByteColorAttribute b_color_attribute{b_attribute};
        const uchar(*src)[4] = static_cast<const uchar(*)[4]>(b_color_attribute.data[0].ptr.data);
        Attribute *attr = attributes.add(name, TypeRGBA, element);
        float4 *data = attr->data_float4();
        fill_generic_attribute(num_curves, num_keys, data, element, [&](int i) {
          return make_float4(color_srgb_to_linear(byte_to_float(src[i][0])),
                             color_srgb_to_linear(byte_to_float(src[i][1])),
                             color_srgb_to_linear(byte_to_float(src[i][2])),
                             color_srgb_to_linear(byte_to_float(src[i][3])));
        });
        break;
      }
      case BL::Attribute::data_type_FLOAT_COLOR: {
        BL::FloatColorAttribute b_color_attribute{b_attribute};
        const float(*src)[4] = static_cast<const float(*)[4]>(b_color_attribute.data[0].ptr.data);
        Attribute *attr = attributes.add(name, TypeRGBA, element);
        float4 *data = attr->data_float4();
        fill_generic_attribute(num_curves, num_keys, data, element, [&](int i) {
          return make_float4(src[i][0], src[i][1], src[i][2], src[i][3]);
        });
        break;
      }
      case BL::Attribute::data_type_FLOAT2: {
        BL::Float2Attribute b_float2_attribute{b_attribute};
        const float(*src)[2] = static_cast<const float(*)[2]>(b_float2_attribute.data[0].ptr.data);
        Attribute *attr = attributes.add(name, TypeFloat2, element);
        float2 *data = attr->data_float2();
        fill_generic_attribute(num_curves, num_keys, data, element, [&](int i) {
          return make_float2(src[i][0], src[i][1]);
        });
        break;
      }
      default:
        /* Not supported. */
        break;
    }
  }
}

static float4 curve_point_as_float4(const float (*b_attr_position)[3],
                                    const float *b_attr_radius,
                                    const int index)
{
  float4 mP = make_float4(
      b_attr_position[index][0], b_attr_position[index][1], b_attr_position[index][2], 0.0f);
  mP.w = b_attr_radius ? b_attr_radius[index] : 0.005f;
  return mP;
}

static float4 interpolate_curve_points(const float (*b_attr_position)[3],
                                       const float *b_attr_radius,
                                       const int first_point_index,
                                       const int num_points,
                                       const float step)
{
  const float curve_t = step * (num_points - 1);
  const int point_a = clamp((int)curve_t, 0, num_points - 1);
  const int point_b = min(point_a + 1, num_points - 1);
  const float t = curve_t - (float)point_a;
  return mix(curve_point_as_float4(b_attr_position, b_attr_radius, first_point_index + point_a),
             curve_point_as_float4(b_attr_position, b_attr_radius, first_point_index + point_b),
             t);
}

static void export_hair_curves(Scene *scene,
                               Hair *hair,
                               BL::Curves b_curves,
                               const bool need_motion,
                               const float motion_scale)
{
  const int num_keys = b_curves.points.length();
  const int num_curves = b_curves.curves.length();

  hair->resize_curves(num_curves, num_keys);

  float3 *curve_keys = hair->get_curve_keys().data();
  float *curve_radius = hair->get_curve_radius().data();
  int *curve_first_key = hair->get_curve_first_key().data();
  int *curve_shader = hair->get_curve_shader().data();

  /* Add requested attributes. */
  float *attr_intercept = NULL;
  float *attr_length = NULL;

  if (hair->need_attribute(scene, ATTR_STD_VERTEX_NORMAL)) {
    /* Get geometry normals. */
    float3 *attr_normal = hair->attributes.add(ATTR_STD_VERTEX_NORMAL)->data_float3();
    int i = 0;
    for (BL::FloatVectorValueReadOnly &normal : b_curves.normals) {
      attr_normal[i++] = get_float3(normal.vector());
    }
  }
  if (hair->need_attribute(scene, ATTR_STD_CURVE_INTERCEPT)) {
    attr_intercept = hair->attributes.add(ATTR_STD_CURVE_INTERCEPT)->data_float();
  }
  if (hair->need_attribute(scene, ATTR_STD_CURVE_LENGTH)) {
    attr_length = hair->attributes.add(ATTR_STD_CURVE_LENGTH)->data_float();
  }
  if (hair->need_attribute(scene, ATTR_STD_CURVE_RANDOM)) {
    float *attr_random = hair->attributes.add(ATTR_STD_CURVE_RANDOM)->data_float();
    for (int i = 0; i < num_curves; i++) {
      attr_random[i] = hash_uint2_to_float(i, 0);
    }
  }

  const int *point_offsets = static_cast<const int *>(b_curves.curve_offset_data[0].ptr.data);
  const float(*b_attr_position)[3] = find_position_attribute(b_curves);
  const float *b_attr_radius = find_radius_attribute(b_curves);

  std::copy(point_offsets, point_offsets + num_curves, curve_first_key);
  std::fill(curve_shader, curve_shader + num_curves, 0);
  if (b_attr_radius) {
    std::copy(b_attr_radius, b_attr_radius + num_keys, curve_radius);
  }
  else {
    std::fill(curve_radius, curve_radius + num_keys, 0.005f);
  }

  /* Export curves and points. */
  for (int i = 0; i < num_curves; i++) {
    const int first_point_index = point_offsets[i];
    const int num_points = point_offsets[i + 1] - first_point_index;

    float3 prev_co = zero_float3();
    float length = 0.0f;

    /* Position and radius. */
    for (int j = 0; j < num_points; j++) {
      const int point = first_point_index + j;
      const float3 co = make_float3(
          b_attr_position[point][0], b_attr_position[point][1], b_attr_position[point][2]);

      curve_keys[point] = co;

      if (attr_length || attr_intercept) {
        if (j > 0) {
          length += len(co - prev_co);
        }
        prev_co = co;

        if (attr_intercept) {
          attr_intercept[point] = length;
        }
      }
    }

    /* Normalized 0..1 attribute along curve. */
    if (attr_intercept && length > 0.0f) {
      for (int j = 1; j < num_points; j++) {
        const int point = first_point_index + j;
        attr_intercept[point] /= length;
      }
    }

    /* Curve length. */
    if (attr_length) {
      attr_length[i] = length;
    }
  }

  attr_create_generic(scene, hair, b_curves, need_motion, motion_scale);
}

static void export_hair_curves_motion(Hair *hair, BL::Curves b_curves, int motion_step)
{
  /* Find or add attribute. */
  Attribute *attr_mP = hair->attributes.find(ATTR_STD_MOTION_VERTEX_POSITION);
  bool new_attribute = false;

  if (!attr_mP) {
    attr_mP = hair->attributes.add(ATTR_STD_MOTION_VERTEX_POSITION);
    new_attribute = true;
  }

  /* Export motion keys. */
  const int num_keys = hair->get_curve_keys().size();
  const int num_curves = b_curves.curves.length();
  float4 *mP = attr_mP->data_float4() + motion_step * num_keys;
  bool have_motion = false;
  int num_motion_keys = 0;
  int curve_index = 0;

  const int *point_offsets = static_cast<const int *>(b_curves.curve_offset_data[0].ptr.data);
  const float(*b_attr_position)[3] = find_position_attribute(b_curves);
  const float *b_attr_radius = find_radius_attribute(b_curves);

  for (int i = 0; i < num_curves; i++) {
    const int first_point_index = point_offsets[i];
    const int num_points = point_offsets[i + 1] - first_point_index;

    Hair::Curve curve = hair->get_curve(curve_index);
    curve_index++;

    if (num_points == curve.num_keys) {
      /* Number of keys matches. */
      for (int i = 0; i < num_points; i++) {
        int point = first_point_index + i;

        if (point < num_keys) {
          mP[num_motion_keys] = curve_point_as_float4(b_attr_position, b_attr_radius, point);
          num_motion_keys++;

          if (!have_motion) {
            /* TODO: use epsilon for comparison? Was needed for particles due to
             * transform, but ideally should not happen anymore. */
            float4 curve_key = float3_to_float4(hair->get_curve_keys()[i]);
            curve_key.w = hair->get_curve_radius()[i];
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
        mP[num_motion_keys] = interpolate_curve_points(
            b_attr_position, b_attr_radius, first_point_index, num_points, step);
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

/* Hair object. */
void BlenderSync::sync_hair(Hair *hair, BObjectInfo &b_ob_info, bool motion, int motion_step)
{
  /* Motion blur attribute is relative to seconds, we need it relative to frames. */
  const bool need_motion = object_need_motion_attribute(b_ob_info, scene);
  const float motion_scale = (need_motion) ?
                                 scene->motion_shutter_time() /
                                     (b_scene.render().fps() / b_scene.render().fps_base()) :
                                 0.0f;

  /* Convert Blender hair to Cycles curves. */
  BL::Curves b_curves(b_ob_info.object_data);
  if (motion) {
    export_hair_curves_motion(hair, b_curves, motion_step);
  }
  else {
    export_hair_curves(scene, hair, b_curves, need_motion, motion_scale);
  }
}

void BlenderSync::sync_hair(BL::Depsgraph b_depsgraph, BObjectInfo &b_ob_info, Hair *hair)
{
  /* make a copy of the shaders as the caller in the main thread still need them for syncing the
   * attributes */
  array<Node *> used_shaders = hair->get_used_shaders();

  Hair new_hair;
  new_hair.set_used_shaders(used_shaders);

  if (view_layer.use_hair) {
    if (b_ob_info.object_data.is_a(&RNA_Curves)) {
      /* Hair object. */
      sync_hair(&new_hair, b_ob_info, false);
    }
    else {
      /* Particle hair. */
      bool need_undeformed = new_hair.need_attribute(scene, ATTR_STD_GENERATED);
      BL::Mesh b_mesh = object_to_mesh(
          b_data, b_ob_info, b_depsgraph, need_undeformed, Mesh::SUBDIVISION_NONE);

      if (b_mesh) {
        sync_particle_hair(&new_hair, b_mesh, b_ob_info, false);
        free_object_to_mesh(b_data, b_ob_info, b_mesh);
      }
    }
  }

  /* update original sockets */

  for (const SocketType &socket : new_hair.type->inputs) {
    /* Those sockets are updated in sync_object, so do not modify them. */
    if (socket.name == "use_motion_blur" || socket.name == "motion_steps" ||
        socket.name == "used_shaders")
    {
      continue;
    }
    hair->set_value(socket, new_hair, socket);
  }

  hair->attributes.update(std::move(new_hair.attributes));

  /* tag update */

  /* Compares curve_keys rather than strands in order to handle quick hair
   * adjustments in dynamic BVH - other methods could probably do this better. */
  const bool rebuild = (hair->curve_keys_is_modified() || hair->curve_radius_is_modified());

  hair->tag_update(scene, rebuild);
}

void BlenderSync::sync_hair_motion(BL::Depsgraph b_depsgraph,
                                   BObjectInfo &b_ob_info,
                                   Hair *hair,
                                   int motion_step)
{
  /* Skip if nothing exported. */
  if (hair->num_keys() == 0) {
    return;
  }

  /* Export deformed coordinates. */
  if (ccl::BKE_object_is_deform_modified(b_ob_info, b_scene, preview)) {
    if (b_ob_info.object_data.is_a(&RNA_Curves)) {
      /* Hair object. */
      sync_hair(hair, b_ob_info, true, motion_step);
      return;
    }
    else {
      /* Particle hair. */
      BL::Mesh b_mesh = object_to_mesh(
          b_data, b_ob_info, b_depsgraph, false, Mesh::SUBDIVISION_NONE);
      if (b_mesh) {
        sync_particle_hair(hair, b_mesh, b_ob_info, true, motion_step);
        free_object_to_mesh(b_data, b_ob_info, b_mesh);
        return;
      }
    }
  }

  /* No deformation on this frame, copy coordinates if other frames did have it. */
  hair->copy_center_to_motion_step(motion_step);
}

CCL_NAMESPACE_END
