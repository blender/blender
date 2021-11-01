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

#include "scene/background.h"
#include "scene/camera.h"
#include "scene/curves.h"
#include "scene/film.h"
#include "scene/integrator.h"
#include "scene/light.h"
#include "scene/mesh.h"
#include "scene/object.h"
#include "scene/procedural.h"
#include "scene/scene.h"
#include "scene/shader.h"
#include "scene/shader_graph.h"
#include "scene/shader_nodes.h"

#include "device/device.h"

#include "blender/device.h"
#include "blender/session.h"
#include "blender/sync.h"
#include "blender/util.h"

#include "util/debug.h"
#include "util/foreach.h"
#include "util/hash.h"
#include "util/log.h"
#include "util/opengl.h"
#include "util/openimagedenoise.h"

CCL_NAMESPACE_BEGIN

static const char *cryptomatte_prefix = "Crypto";

/* Constructor */

BlenderSync::BlenderSync(BL::RenderEngine &b_engine,
                         BL::BlendData &b_data,
                         BL::Scene &b_scene,
                         Scene *scene,
                         bool preview,
                         bool use_developer_ui,
                         Progress &progress)
    : b_engine(b_engine),
      b_data(b_data),
      b_scene(b_scene),
      shader_map(scene),
      object_map(scene),
      procedural_map(scene),
      geometry_map(scene),
      light_map(scene),
      particle_system_map(scene),
      world_map(NULL),
      world_recalc(false),
      scene(scene),
      preview(preview),
      experimental(false),
      use_developer_ui(use_developer_ui),
      dicing_rate(1.0f),
      max_subdivisions(12),
      progress(progress),
      has_updates_(true)
{
  PointerRNA cscene = RNA_pointer_get(&b_scene.ptr, "cycles");
  dicing_rate = preview ? RNA_float_get(&cscene, "preview_dicing_rate") :
                          RNA_float_get(&cscene, "dicing_rate");
  max_subdivisions = RNA_int_get(&cscene, "max_subdivisions");
}

BlenderSync::~BlenderSync()
{
}

void BlenderSync::reset(BL::BlendData &b_data, BL::Scene &b_scene)
{
  /* Update data and scene pointers in case they change in session reset,
   * for example after undo.
   * Note that we do not modify the `has_updates_` flag here because the sync
   * reset is also used during viewport navigation. */
  this->b_data = b_data;
  this->b_scene = b_scene;
}

/* Sync */

void BlenderSync::sync_recalc(BL::Depsgraph &b_depsgraph, BL::SpaceView3D &b_v3d)
{
  /* Sync recalc flags from blender to cycles. Actual update is done separate,
   * so we can do it later on if doing it immediate is not suitable. */

  if (experimental) {
    /* Mark all meshes as needing to be exported again if dicing changed. */
    PointerRNA cscene = RNA_pointer_get(&b_scene.ptr, "cycles");
    bool dicing_prop_changed = false;

    float updated_dicing_rate = preview ? RNA_float_get(&cscene, "preview_dicing_rate") :
                                          RNA_float_get(&cscene, "dicing_rate");

    if (dicing_rate != updated_dicing_rate) {
      dicing_rate = updated_dicing_rate;
      dicing_prop_changed = true;
    }

    int updated_max_subdivisions = RNA_int_get(&cscene, "max_subdivisions");

    if (max_subdivisions != updated_max_subdivisions) {
      max_subdivisions = updated_max_subdivisions;
      dicing_prop_changed = true;
    }

    if (dicing_prop_changed) {
      has_updates_ = true;

      for (const pair<const GeometryKey, Geometry *> &iter : geometry_map.key_to_scene_data()) {
        Geometry *geom = iter.second;
        if (geom->is_mesh()) {
          Mesh *mesh = static_cast<Mesh *>(geom);
          if (mesh->get_subdivision_type() != Mesh::SUBDIVISION_NONE) {
            PointerRNA id_ptr;
            RNA_id_pointer_create((::ID *)iter.first.id, &id_ptr);
            geometry_map.set_recalc(BL::ID(id_ptr));
          }
        }
      }
    }
  }

  /* Iterate over all IDs in this depsgraph. */
  for (BL::DepsgraphUpdate &b_update : b_depsgraph.updates) {
    /* TODO(sergey): Can do more selective filter here. For example, ignore changes made to
     * screen datablock. Note that sync_data() needs to be called after object deletion, and
     * currently this is ensured by the scene ID tagged for update, which sets the `has_updates_`
     * flag. */
    has_updates_ = true;

    BL::ID b_id(b_update.id());

    /* Material */
    if (b_id.is_a(&RNA_Material)) {
      BL::Material b_mat(b_id);
      shader_map.set_recalc(b_mat);
    }
    /* Light */
    else if (b_id.is_a(&RNA_Light)) {
      BL::Light b_light(b_id);
      shader_map.set_recalc(b_light);
    }
    /* Object */
    else if (b_id.is_a(&RNA_Object)) {
      BL::Object b_ob(b_id);
      const bool is_geometry = object_is_geometry(b_ob);
      const bool is_light = !is_geometry && object_is_light(b_ob);

      if (b_ob.is_instancer() && b_update.is_updated_shading()) {
        /* Needed for e.g. object color updates on instancer. */
        object_map.set_recalc(b_ob);
      }

      if (is_geometry || is_light) {
        const bool updated_geometry = b_update.is_updated_geometry();

        /* Geometry (mesh, hair, volume). */
        if (is_geometry) {
          if (b_update.is_updated_transform() || b_update.is_updated_shading()) {
            object_map.set_recalc(b_ob);
          }

          if (updated_geometry ||
              (object_subdivision_type(b_ob, preview, experimental) != Mesh::SUBDIVISION_NONE)) {
            BL::ID key = BKE_object_is_modified(b_ob) ? b_ob : b_ob.data();
            geometry_map.set_recalc(key);
          }

          if (updated_geometry) {
            BL::Object::particle_systems_iterator b_psys;
            for (b_ob.particle_systems.begin(b_psys); b_psys != b_ob.particle_systems.end();
                 ++b_psys) {
              particle_system_map.set_recalc(b_ob);
            }
          }
        }
        /* Light */
        else if (is_light) {
          if (b_update.is_updated_transform() || b_update.is_updated_shading()) {
            object_map.set_recalc(b_ob);
            light_map.set_recalc(b_ob);
          }

          if (updated_geometry) {
            light_map.set_recalc(b_ob);
          }
        }
      }
    }
    /* Mesh */
    else if (b_id.is_a(&RNA_Mesh)) {
      BL::Mesh b_mesh(b_id);
      geometry_map.set_recalc(b_mesh);
    }
    /* World */
    else if (b_id.is_a(&RNA_World)) {
      BL::World b_world(b_id);
      if (world_map == b_world.ptr.data) {
        world_recalc = true;
      }
    }
    /* Volume */
    else if (b_id.is_a(&RNA_Volume)) {
      BL::Volume b_volume(b_id);
      geometry_map.set_recalc(b_volume);
    }
  }

  if (b_v3d) {
    BlenderViewportParameters new_viewport_parameters(b_v3d, use_developer_ui);

    if (viewport_parameters.shader_modified(new_viewport_parameters)) {
      world_recalc = true;
      has_updates_ = true;
    }

    has_updates_ |= viewport_parameters.modified(new_viewport_parameters);
  }
}

void BlenderSync::sync_data(BL::RenderSettings &b_render,
                            BL::Depsgraph &b_depsgraph,
                            BL::SpaceView3D &b_v3d,
                            BL::Object &b_override,
                            int width,
                            int height,
                            void **python_thread_state)
{
  if (!has_updates_) {
    return;
  }

  scoped_timer timer;

  BL::ViewLayer b_view_layer = b_depsgraph.view_layer_eval();

  /* TODO(sergey): This feels weak to pass view layer to the integrator, and even weaker to have an
   * implicit check on whether it is a background render or not. What is the nicer thing here? */
  const bool background = !b_v3d;

  sync_view_layer(b_view_layer);
  sync_integrator(b_view_layer, background);
  sync_film(b_view_layer, b_v3d);
  sync_shaders(b_depsgraph, b_v3d);
  sync_images();

  geometry_synced.clear(); /* use for objects and motion sync */

  if (scene->need_motion() == Scene::MOTION_PASS || scene->need_motion() == Scene::MOTION_NONE ||
      scene->camera->get_motion_position() == Camera::MOTION_POSITION_CENTER) {
    sync_objects(b_depsgraph, b_v3d);
  }
  sync_motion(b_render, b_depsgraph, b_v3d, b_override, width, height, python_thread_state);

  geometry_synced.clear();

  /* Shader sync done at the end, since object sync uses it.
   * false = don't delete unused shaders, not supported. */
  shader_map.post_sync(false);

  free_data_after_sync(b_depsgraph);

  VLOG(1) << "Total time spent synchronizing data: " << timer.get_time();

  has_updates_ = false;
}

/* Integrator */

void BlenderSync::sync_integrator(BL::ViewLayer &b_view_layer, bool background)
{
  PointerRNA cscene = RNA_pointer_get(&b_scene.ptr, "cycles");

  experimental = (get_enum(cscene, "feature_set") != 0);

  Integrator *integrator = scene->integrator;

  integrator->set_min_bounce(get_int(cscene, "min_light_bounces"));
  integrator->set_max_bounce(get_int(cscene, "max_bounces"));

  integrator->set_max_diffuse_bounce(get_int(cscene, "diffuse_bounces"));
  integrator->set_max_glossy_bounce(get_int(cscene, "glossy_bounces"));
  integrator->set_max_transmission_bounce(get_int(cscene, "transmission_bounces"));
  integrator->set_max_volume_bounce(get_int(cscene, "volume_bounces"));

  integrator->set_transparent_min_bounce(get_int(cscene, "min_transparent_bounces"));
  integrator->set_transparent_max_bounce(get_int(cscene, "transparent_max_bounces"));

  integrator->set_volume_max_steps(get_int(cscene, "volume_max_steps"));
  float volume_step_rate = (preview) ? get_float(cscene, "volume_preview_step_rate") :
                                       get_float(cscene, "volume_step_rate");
  integrator->set_volume_step_rate(volume_step_rate);

  integrator->set_caustics_reflective(get_boolean(cscene, "caustics_reflective"));
  integrator->set_caustics_refractive(get_boolean(cscene, "caustics_refractive"));
  integrator->set_filter_glossy(get_float(cscene, "blur_glossy"));

  int seed = get_int(cscene, "seed");
  if (get_boolean(cscene, "use_animated_seed")) {
    seed = hash_uint2(b_scene.frame_current(), get_int(cscene, "seed"));
    if (b_scene.frame_subframe() != 0.0f) {
      /* TODO(sergey): Ideally should be some sort of hash_merge,
       * but this is good enough for now.
       */
      seed += hash_uint2((int)(b_scene.frame_subframe() * (float)INT_MAX),
                         get_int(cscene, "seed"));
    }
  }

  integrator->set_seed(seed);

  integrator->set_sample_clamp_direct(get_float(cscene, "sample_clamp_direct"));
  integrator->set_sample_clamp_indirect(get_float(cscene, "sample_clamp_indirect"));
  if (!preview) {
    integrator->set_motion_blur(view_layer.use_motion_blur);
  }

  integrator->set_light_sampling_threshold(get_float(cscene, "light_sampling_threshold"));

  SamplingPattern sampling_pattern = (SamplingPattern)get_enum(
      cscene, "sampling_pattern", SAMPLING_NUM_PATTERNS, SAMPLING_PATTERN_SOBOL);
  integrator->set_sampling_pattern(sampling_pattern);

  bool use_adaptive_sampling = false;
  if (preview) {
    use_adaptive_sampling = RNA_boolean_get(&cscene, "use_preview_adaptive_sampling");
    integrator->set_use_adaptive_sampling(use_adaptive_sampling);
    integrator->set_adaptive_threshold(get_float(cscene, "preview_adaptive_threshold"));
    integrator->set_adaptive_min_samples(get_int(cscene, "preview_adaptive_min_samples"));
  }
  else {
    use_adaptive_sampling = RNA_boolean_get(&cscene, "use_adaptive_sampling");
    integrator->set_use_adaptive_sampling(use_adaptive_sampling);
    integrator->set_adaptive_threshold(get_float(cscene, "adaptive_threshold"));
    integrator->set_adaptive_min_samples(get_int(cscene, "adaptive_min_samples"));
  }

  int samples = get_int(cscene, "samples");
  float scrambling_distance = get_float(cscene, "scrambling_distance");
  bool adaptive_scrambling_distance = get_boolean(cscene, "adaptive_scrambling_distance");
  if (adaptive_scrambling_distance) {
    scrambling_distance *= 4.0f / sqrtf(samples);
  }

  /* only use scrambling distance in the viewport if user wants to and disable with AS */
  bool preview_scrambling_distance = get_boolean(cscene, "preview_scrambling_distance");
  if ((preview && !preview_scrambling_distance) || use_adaptive_sampling)
    scrambling_distance = 1.0f;

  if (scrambling_distance != 1.0f) {
    VLOG(3) << "Using scrambling distance: " << scrambling_distance;
  }
  integrator->set_scrambling_distance(scrambling_distance);

  if (get_boolean(cscene, "use_fast_gi")) {
    if (preview) {
      integrator->set_ao_bounces(get_int(cscene, "ao_bounces"));
    }
    else {
      integrator->set_ao_bounces(get_int(cscene, "ao_bounces_render"));
    }
  }
  else {
    integrator->set_ao_bounces(0);
  }

  const DenoiseParams denoise_params = get_denoise_params(b_scene, b_view_layer, background);
  integrator->set_use_denoise(denoise_params.use);

  /* Only update denoiser parameters if the denoiser is actually used. This allows to tweak
   * denoiser parameters before enabling it without render resetting on every change. The downside
   * is that the interface and the integrator are technically out of sync. */
  if (denoise_params.use) {
    integrator->set_denoiser_type(denoise_params.type);
    integrator->set_denoise_start_sample(denoise_params.start_sample);
    integrator->set_use_denoise_pass_albedo(denoise_params.use_pass_albedo);
    integrator->set_use_denoise_pass_normal(denoise_params.use_pass_normal);
    integrator->set_denoiser_prefilter(denoise_params.prefilter);
  }

  /* UPDATE_NONE as we don't want to tag the integrator as modified (this was done by the
   * set calls above), but we need to make sure that the dependent things are tagged. */
  integrator->tag_update(scene, Integrator::UPDATE_NONE);
}

/* Film */

void BlenderSync::sync_film(BL::ViewLayer &b_view_layer, BL::SpaceView3D &b_v3d)
{
  PointerRNA cscene = RNA_pointer_get(&b_scene.ptr, "cycles");
  PointerRNA crl = RNA_pointer_get(&b_view_layer.ptr, "cycles");

  Film *film = scene->film;

  if (b_v3d) {
    const BlenderViewportParameters new_viewport_parameters(b_v3d, use_developer_ui);
    film->set_display_pass(new_viewport_parameters.display_pass);
    film->set_show_active_pixels(new_viewport_parameters.show_active_pixels);
  }

  film->set_exposure(get_float(cscene, "film_exposure"));
  film->set_filter_type(
      (FilterType)get_enum(cscene, "pixel_filter_type", FILTER_NUM_TYPES, FILTER_BLACKMAN_HARRIS));
  float filter_width = (film->get_filter_type() == FILTER_BOX) ? 1.0f :
                                                                 get_float(cscene, "filter_width");
  film->set_filter_width(filter_width);

  if (b_scene.world()) {
    BL::WorldMistSettings b_mist = b_scene.world().mist_settings();

    film->set_mist_start(b_mist.start());
    film->set_mist_depth(b_mist.depth());

    switch (b_mist.falloff()) {
      case BL::WorldMistSettings::falloff_QUADRATIC:
        film->set_mist_falloff(2.0f);
        break;
      case BL::WorldMistSettings::falloff_LINEAR:
        film->set_mist_falloff(1.0f);
        break;
      case BL::WorldMistSettings::falloff_INVERSE_QUADRATIC:
        film->set_mist_falloff(0.5f);
        break;
    }
  }

  /* Blender viewport does not support proper shadow catcher compositing, so force an approximate
   * mode to improve visual feedback. */
  if (b_v3d) {
    film->set_use_approximate_shadow_catcher(true);
  }
  else {
    film->set_use_approximate_shadow_catcher(!get_boolean(crl, "use_pass_shadow_catcher"));
  }
}

/* Render Layer */

void BlenderSync::sync_view_layer(BL::ViewLayer &b_view_layer)
{
  view_layer.name = b_view_layer.name();

  /* Filter. */
  view_layer.use_background_shader = b_view_layer.use_sky();
  /* Always enable surfaces for baking, otherwise there is nothing to bake to. */
  view_layer.use_surfaces = b_view_layer.use_solid() || scene->bake_manager->get_baking();
  view_layer.use_hair = b_view_layer.use_strand();
  view_layer.use_volumes = b_view_layer.use_volumes();
  view_layer.use_motion_blur = b_view_layer.use_motion_blur() &&
                               b_scene.render().use_motion_blur();

  /* Material override. */
  view_layer.material_override = b_view_layer.material_override();

  /* Sample override. */
  PointerRNA cscene = RNA_pointer_get(&b_scene.ptr, "cycles");
  int use_layer_samples = get_enum(cscene, "use_layer_samples");

  view_layer.bound_samples = (use_layer_samples == 1);
  view_layer.samples = 0;

  if (use_layer_samples != 2) {
    int samples = b_view_layer.samples();
    view_layer.samples = samples;
  }
}

/* Images */
void BlenderSync::sync_images()
{
  /* Sync is a convention for this API, but currently it frees unused buffers. */

  const bool is_interface_locked = b_engine.render() && b_engine.render().use_lock_interface();
  if (is_interface_locked == false && BlenderSession::headless == false) {
    /* If interface is not locked, it's possible image is needed for
     * the display.
     */
    return;
  }
  /* Free buffers used by images which are not needed for render. */
  for (BL::Image &b_image : b_data.images) {
    /* TODO(sergey): Consider making it an utility function to check
     * whether image is considered builtin.
     */
    const bool is_builtin = b_image.packed_file() ||
                            b_image.source() == BL::Image::source_GENERATED ||
                            b_image.source() == BL::Image::source_MOVIE || b_engine.is_preview();
    if (is_builtin == false) {
      b_image.buffers_free();
    }
    /* TODO(sergey): Free builtin images not used by any shader. */
  }
}

/* Passes */

static PassType get_blender_pass_type(BL::RenderPass &b_pass)
{
  string name = b_pass.name();
#define MAP_PASS(passname, passtype) \
  if (name == passname) { \
    return passtype; \
  } \
  ((void)0)

  /* NOTE: Keep in sync with defined names from DNA_scene_types.h */

  MAP_PASS("Combined", PASS_COMBINED);
  MAP_PASS("Noisy Image", PASS_COMBINED);

  MAP_PASS("Depth", PASS_DEPTH);
  MAP_PASS("Mist", PASS_MIST);
  MAP_PASS("Position", PASS_POSITION);
  MAP_PASS("Normal", PASS_NORMAL);
  MAP_PASS("IndexOB", PASS_OBJECT_ID);
  MAP_PASS("UV", PASS_UV);
  MAP_PASS("Vector", PASS_MOTION);
  MAP_PASS("IndexMA", PASS_MATERIAL_ID);

  MAP_PASS("DiffDir", PASS_DIFFUSE_DIRECT);
  MAP_PASS("GlossDir", PASS_GLOSSY_DIRECT);
  MAP_PASS("TransDir", PASS_TRANSMISSION_DIRECT);
  MAP_PASS("VolumeDir", PASS_VOLUME_DIRECT);

  MAP_PASS("DiffInd", PASS_DIFFUSE_INDIRECT);
  MAP_PASS("GlossInd", PASS_GLOSSY_INDIRECT);
  MAP_PASS("TransInd", PASS_TRANSMISSION_INDIRECT);
  MAP_PASS("VolumeInd", PASS_VOLUME_INDIRECT);

  MAP_PASS("DiffCol", PASS_DIFFUSE_COLOR);
  MAP_PASS("GlossCol", PASS_GLOSSY_COLOR);
  MAP_PASS("TransCol", PASS_TRANSMISSION_COLOR);

  MAP_PASS("Emit", PASS_EMISSION);
  MAP_PASS("Env", PASS_BACKGROUND);
  MAP_PASS("AO", PASS_AO);
  MAP_PASS("Shadow", PASS_SHADOW);

  MAP_PASS("BakePrimitive", PASS_BAKE_PRIMITIVE);
  MAP_PASS("BakeDifferential", PASS_BAKE_DIFFERENTIAL);

  MAP_PASS("Denoising Normal", PASS_DENOISING_NORMAL);
  MAP_PASS("Denoising Albedo", PASS_DENOISING_ALBEDO);
  MAP_PASS("Denoising Depth", PASS_DENOISING_DEPTH);

  MAP_PASS("Shadow Catcher", PASS_SHADOW_CATCHER);
  MAP_PASS("Noisy Shadow Catcher", PASS_SHADOW_CATCHER);

  MAP_PASS("AdaptiveAuxBuffer", PASS_ADAPTIVE_AUX_BUFFER);
  MAP_PASS("Debug Sample Count", PASS_SAMPLE_COUNT);

  if (string_startswith(name, cryptomatte_prefix)) {
    return PASS_CRYPTOMATTE;
  }

#undef MAP_PASS

  return PASS_NONE;
}

static Pass *pass_add(Scene *scene,
                      PassType type,
                      const char *name,
                      PassMode mode = PassMode::DENOISED)
{
  Pass *pass = scene->create_node<Pass>();

  pass->set_type(type);
  pass->set_name(ustring(name));
  pass->set_mode(mode);

  return pass;
}

void BlenderSync::sync_render_passes(BL::RenderLayer &b_rlay, BL::ViewLayer &b_view_layer)
{
  PointerRNA cscene = RNA_pointer_get(&b_scene.ptr, "cycles");

  /* Delete all existing passes. */
  set<Pass *> clear_passes(scene->passes.begin(), scene->passes.end());
  scene->delete_nodes(clear_passes);

  /* Always add combined pass. */
  pass_add(scene, PASS_COMBINED, "Combined");

  /* Blender built-in data and light passes. */
  for (BL::RenderPass &b_pass : b_rlay.passes) {
    const PassType pass_type = get_blender_pass_type(b_pass);

    if (pass_type == PASS_NONE) {
      LOG(ERROR) << "Unknown pass " << b_pass.name();
      continue;
    }

    if (pass_type == PASS_MOTION &&
        (b_view_layer.use_motion_blur() && b_scene.render().use_motion_blur())) {
      continue;
    }

    pass_add(scene, pass_type, b_pass.name().c_str());
  }

  PointerRNA crl = RNA_pointer_get(&b_view_layer.ptr, "cycles");

  /* Debug passes. */
  if (get_boolean(crl, "pass_debug_sample_count")) {
    b_engine.add_pass("Debug Sample Count", 1, "X", b_view_layer.name().c_str());
    pass_add(scene, PASS_SAMPLE_COUNT, "Debug Sample Count");
  }

  /* Cycles specific passes. */
  if (get_boolean(crl, "use_pass_volume_direct")) {
    b_engine.add_pass("VolumeDir", 3, "RGB", b_view_layer.name().c_str());
    pass_add(scene, PASS_VOLUME_DIRECT, "VolumeDir");
  }
  if (get_boolean(crl, "use_pass_volume_indirect")) {
    b_engine.add_pass("VolumeInd", 3, "RGB", b_view_layer.name().c_str());
    pass_add(scene, PASS_VOLUME_INDIRECT, "VolumeInd");
  }
  if (get_boolean(crl, "use_pass_shadow_catcher")) {
    b_engine.add_pass("Shadow Catcher", 3, "RGB", b_view_layer.name().c_str());
    pass_add(scene, PASS_SHADOW_CATCHER, "Shadow Catcher");
  }

  /* Cryptomatte stores two ID/weight pairs per RGBA layer.
   * User facing parameter is the number of pairs. */
  int crypto_depth = divide_up(min(16, b_view_layer.pass_cryptomatte_depth()), 2);
  scene->film->set_cryptomatte_depth(crypto_depth);
  CryptomatteType cryptomatte_passes = CRYPT_NONE;
  if (b_view_layer.use_pass_cryptomatte_object()) {
    for (int i = 0; i < crypto_depth; i++) {
      string passname = cryptomatte_prefix + string_printf("Object%02d", i);
      b_engine.add_pass(passname.c_str(), 4, "RGBA", b_view_layer.name().c_str());
      pass_add(scene, PASS_CRYPTOMATTE, passname.c_str());
    }
    cryptomatte_passes = (CryptomatteType)(cryptomatte_passes | CRYPT_OBJECT);
  }
  if (b_view_layer.use_pass_cryptomatte_material()) {
    for (int i = 0; i < crypto_depth; i++) {
      string passname = cryptomatte_prefix + string_printf("Material%02d", i);
      b_engine.add_pass(passname.c_str(), 4, "RGBA", b_view_layer.name().c_str());
      pass_add(scene, PASS_CRYPTOMATTE, passname.c_str());
    }
    cryptomatte_passes = (CryptomatteType)(cryptomatte_passes | CRYPT_MATERIAL);
  }
  if (b_view_layer.use_pass_cryptomatte_asset()) {
    for (int i = 0; i < crypto_depth; i++) {
      string passname = cryptomatte_prefix + string_printf("Asset%02d", i);
      b_engine.add_pass(passname.c_str(), 4, "RGBA", b_view_layer.name().c_str());
      pass_add(scene, PASS_CRYPTOMATTE, passname.c_str());
    }
    cryptomatte_passes = (CryptomatteType)(cryptomatte_passes | CRYPT_ASSET);
  }
  scene->film->set_cryptomatte_passes(cryptomatte_passes);

  /* Denoising passes. */
  const bool use_denoising = get_boolean(cscene, "use_denoising") &&
                             get_boolean(crl, "use_denoising");
  const bool store_denoising_passes = get_boolean(crl, "denoising_store_passes");
  if (use_denoising) {
    b_engine.add_pass("Noisy Image", 4, "RGBA", b_view_layer.name().c_str());
    pass_add(scene, PASS_COMBINED, "Noisy Image", PassMode::NOISY);
    if (get_boolean(crl, "use_pass_shadow_catcher")) {
      b_engine.add_pass("Noisy Shadow Catcher", 3, "RGB", b_view_layer.name().c_str());
      pass_add(scene, PASS_SHADOW_CATCHER, "Noisy Shadow Catcher", PassMode::NOISY);
    }
  }
  if (store_denoising_passes) {
    b_engine.add_pass("Denoising Normal", 3, "XYZ", b_view_layer.name().c_str());
    pass_add(scene, PASS_DENOISING_NORMAL, "Denoising Normal", PassMode::NOISY);

    b_engine.add_pass("Denoising Albedo", 3, "RGB", b_view_layer.name().c_str());
    pass_add(scene, PASS_DENOISING_ALBEDO, "Denoising Albedo", PassMode::NOISY);

    b_engine.add_pass("Denoising Depth", 1, "Z", b_view_layer.name().c_str());
    pass_add(scene, PASS_DENOISING_DEPTH, "Denoising Depth", PassMode::NOISY);
  }

  /* Custom AOV passes. */
  BL::ViewLayer::aovs_iterator b_aov_iter;
  for (b_view_layer.aovs.begin(b_aov_iter); b_aov_iter != b_view_layer.aovs.end(); ++b_aov_iter) {
    BL::AOV b_aov(*b_aov_iter);
    if (!b_aov.is_valid()) {
      continue;
    }

    string name = b_aov.name();
    bool is_color = b_aov.type() == BL::AOV::type_COLOR;

    if (is_color) {
      b_engine.add_pass(name.c_str(), 4, "RGBA", b_view_layer.name().c_str());
      pass_add(scene, PASS_AOV_COLOR, name.c_str());
    }
    else {
      b_engine.add_pass(name.c_str(), 1, "X", b_view_layer.name().c_str());
      pass_add(scene, PASS_AOV_VALUE, name.c_str());
    }
  }

  scene->film->set_pass_alpha_threshold(b_view_layer.pass_alpha_threshold());
}

void BlenderSync::free_data_after_sync(BL::Depsgraph &b_depsgraph)
{
  /* When viewport display is not needed during render we can force some
   * caches to be releases from blender side in order to reduce peak memory
   * footprint during synchronization process.
   */

  const bool is_interface_locked = b_engine.render() && b_engine.render().use_lock_interface();
  const bool is_persistent_data = b_engine.render() && b_engine.render().use_persistent_data();
  const bool can_free_caches =
      (BlenderSession::headless || is_interface_locked) &&
      /* Baking re-uses the depsgraph multiple times, clearing crashes
       * reading un-evaluated mesh data which isn't aligned with the
       * geometry we're baking, see T71012. */
      !scene->bake_manager->get_baking() &&
      /* Persistent data must main caches for performance and correctness. */
      !is_persistent_data;

  if (!can_free_caches) {
    return;
  }
  /* TODO(sergey): We can actually remove the whole dependency graph,
   * but that will need some API support first.
   */
  for (BL::Object &b_ob : b_depsgraph.objects) {
    b_ob.cache_release();
  }
}

/* Scene Parameters */

SceneParams BlenderSync::get_scene_params(BL::Scene &b_scene, bool background)
{
  SceneParams params;
  PointerRNA cscene = RNA_pointer_get(&b_scene.ptr, "cycles");
  const bool shadingsystem = RNA_boolean_get(&cscene, "shading_system");

  if (shadingsystem == 0)
    params.shadingsystem = SHADINGSYSTEM_SVM;
  else if (shadingsystem == 1)
    params.shadingsystem = SHADINGSYSTEM_OSL;

  if (background || DebugFlags().viewport_static_bvh)
    params.bvh_type = BVH_TYPE_STATIC;
  else
    params.bvh_type = BVH_TYPE_DYNAMIC;

  params.use_bvh_spatial_split = RNA_boolean_get(&cscene, "debug_use_spatial_splits");
  params.use_bvh_unaligned_nodes = RNA_boolean_get(&cscene, "debug_use_hair_bvh");
  params.num_bvh_time_steps = RNA_int_get(&cscene, "debug_bvh_time_steps");

  PointerRNA csscene = RNA_pointer_get(&b_scene.ptr, "cycles_curves");
  params.hair_subdivisions = get_int(csscene, "subdivisions");
  params.hair_shape = (CurveShapeType)get_enum(
      csscene, "shape", CURVE_NUM_SHAPE_TYPES, CURVE_THICK);

  int texture_limit;
  if (background) {
    texture_limit = RNA_enum_get(&cscene, "texture_limit_render");
  }
  else {
    texture_limit = RNA_enum_get(&cscene, "texture_limit");
  }
  if (texture_limit > 0 && b_scene.render().use_simplify()) {
    params.texture_limit = 1 << (texture_limit + 6);
  }
  else {
    params.texture_limit = 0;
  }

  params.bvh_layout = DebugFlags().cpu.bvh_layout;

  params.background = background;

  return params;
}

/* Session Parameters */

bool BlenderSync::get_session_pause(BL::Scene &b_scene, bool background)
{
  PointerRNA cscene = RNA_pointer_get(&b_scene.ptr, "cycles");
  return (background) ? false : get_boolean(cscene, "preview_pause");
}

SessionParams BlenderSync::get_session_params(BL::RenderEngine &b_engine,
                                              BL::Preferences &b_preferences,
                                              BL::Scene &b_scene,
                                              bool background)
{
  SessionParams params;
  PointerRNA cscene = RNA_pointer_get(&b_scene.ptr, "cycles");

  /* feature set */
  params.experimental = (get_enum(cscene, "feature_set") != 0);

  /* Headless and background rendering. */
  params.headless = BlenderSession::headless;
  params.background = background;

  /* Device */
  params.threads = blender_device_threads(b_scene);
  params.device = blender_device_info(b_preferences, b_scene, params.background);

  /* samples */
  int samples = get_int(cscene, "samples");
  int preview_samples = get_int(cscene, "preview_samples");

  if (background) {
    params.samples = samples;
  }
  else {
    params.samples = preview_samples;
    if (params.samples == 0)
      params.samples = INT_MAX;
  }

  /* Clamp samples. */
  params.samples = min(params.samples, Integrator::MAX_SAMPLES);

  /* Viewport Performance */
  params.pixel_size = b_engine.get_preview_pixel_size(b_scene);

  if (background) {
    params.pixel_size = 1;
  }

  /* shading system - scene level needs full refresh */
  const bool shadingsystem = RNA_boolean_get(&cscene, "shading_system");

  if (shadingsystem == 0)
    params.shadingsystem = SHADINGSYSTEM_SVM;
  else if (shadingsystem == 1)
    params.shadingsystem = SHADINGSYSTEM_OSL;

  /* Time limit. */
  if (background) {
    params.time_limit = get_float(cscene, "time_limit");
  }
  else {
    /* For the viewport it kind of makes more sense to think in terms of the noise floor, which is
     * usually higher than acceptable level for the final frame. */
    /* TODO: It might be useful to support time limit in the viewport as well, but needs some
     * extra thoughts and input. */
    params.time_limit = 0.0;
  }

  /* Profiling. */
  params.use_profiling = params.device.has_profiling && !b_engine.is_preview() && background &&
                         BlenderSession::print_render_stats;

  if (background) {
    params.use_auto_tile = RNA_boolean_get(&cscene, "use_auto_tile");
    params.tile_size = max(get_int(cscene, "tile_size"), 8);
  }
  else {
    params.use_auto_tile = false;
  }

  return params;
}

DenoiseParams BlenderSync::get_denoise_params(BL::Scene &b_scene,
                                              BL::ViewLayer &b_view_layer,
                                              bool background)
{
  enum DenoiserInput {
    DENOISER_INPUT_RGB = 1,
    DENOISER_INPUT_RGB_ALBEDO = 2,
    DENOISER_INPUT_RGB_ALBEDO_NORMAL = 3,

    DENOISER_INPUT_NUM,
  };

  DenoiseParams denoising;
  PointerRNA cscene = RNA_pointer_get(&b_scene.ptr, "cycles");

  int input_passes = -1;

  if (background) {
    /* Final Render Denoising */
    denoising.use = get_boolean(cscene, "use_denoising");
    denoising.type = (DenoiserType)get_enum(cscene, "denoiser", DENOISER_NUM, DENOISER_NONE);
    denoising.prefilter = (DenoiserPrefilter)get_enum(
        cscene, "denoising_prefilter", DENOISER_PREFILTER_NUM, DENOISER_PREFILTER_NONE);

    input_passes = (DenoiserInput)get_enum(
        cscene, "denoising_input_passes", DENOISER_INPUT_NUM, DENOISER_INPUT_RGB_ALBEDO_NORMAL);

    if (b_view_layer) {
      PointerRNA clayer = RNA_pointer_get(&b_view_layer.ptr, "cycles");
      if (!get_boolean(clayer, "use_denoising")) {
        denoising.use = false;
      }
    }
  }
  else {
    /* Viewport Denoising */
    denoising.use = get_boolean(cscene, "use_preview_denoising");
    denoising.type = (DenoiserType)get_enum(
        cscene, "preview_denoiser", DENOISER_NUM, DENOISER_NONE);
    denoising.prefilter = (DenoiserPrefilter)get_enum(
        cscene, "preview_denoising_prefilter", DENOISER_PREFILTER_NUM, DENOISER_PREFILTER_FAST);
    denoising.start_sample = get_int(cscene, "preview_denoising_start_sample");

    input_passes = (DenoiserInput)get_enum(
        cscene, "preview_denoising_input_passes", DENOISER_INPUT_NUM, DENOISER_INPUT_RGB_ALBEDO);

    /* Auto select fastest denoiser. */
    if (denoising.type == DENOISER_NONE) {
      if (!Device::available_devices(DEVICE_MASK_OPTIX).empty()) {
        denoising.type = DENOISER_OPTIX;
      }
      else if (openimagedenoise_supported()) {
        denoising.type = DENOISER_OPENIMAGEDENOISE;
      }
      else {
        denoising.use = false;
      }
    }
  }

  switch (input_passes) {
    case DENOISER_INPUT_RGB:
      denoising.use_pass_albedo = false;
      denoising.use_pass_normal = false;
      break;

    case DENOISER_INPUT_RGB_ALBEDO:
      denoising.use_pass_albedo = true;
      denoising.use_pass_normal = false;
      break;

    case DENOISER_INPUT_RGB_ALBEDO_NORMAL:
      denoising.use_pass_albedo = true;
      denoising.use_pass_normal = true;
      break;

    default:
      LOG(ERROR) << "Unhandled input passes enum " << input_passes;
      break;
  }

  return denoising;
}

CCL_NAMESPACE_END
