/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

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

#include "integrator/denoiser.h"

#include "util/debug.h"
#include "util/foreach.h"
#include "util/hash.h"
#include "util/log.h"
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

BlenderSync::~BlenderSync() {}

void BlenderSync::reset(BL::BlendData &b_data, BL::Scene &b_scene)
{
  /* Update data and scene pointers in case they change in session reset,
   * for example after undo.
   * Note that we do not modify the `has_updates_` flag here because the sync
   * reset is also used during viewport navigation. */
  this->b_data = b_data;
  this->b_scene = b_scene;
}

void BlenderSync::tag_update()
{
  has_updates_ = true;
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
            PointerRNA id_ptr = RNA_id_pointer_create((::ID *)iter.first.id);
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
      const bool can_have_geometry = object_can_have_geometry(b_ob);
      const bool is_light = !can_have_geometry && object_is_light(b_ob);

      if (b_ob.is_instancer() && b_update.is_updated_shading()) {
        /* Needed for e.g. object color updates on instancer. */
        object_map.set_recalc(b_ob);
      }

      if (can_have_geometry || is_light) {
        const bool updated_geometry = b_update.is_updated_geometry();

        /* Geometry (mesh, hair, volume). */
        if (can_have_geometry) {
          if (b_update.is_updated_transform() || b_update.is_updated_shading()) {
            object_map.set_recalc(b_ob);
          }

          if (updated_geometry ||
              (object_subdivision_type(b_ob, preview, experimental) != Mesh::SUBDIVISION_NONE))
          {
            BL::ID key = BKE_object_is_modified(b_ob) ? b_ob : b_ob.data();
            geometry_map.set_recalc(key);

            /* Sync all contained geometry instances as well when the object changed.. */
            map<void *, set<BL::ID>>::const_iterator instance_geometries =
                instance_geometries_by_object.find(b_ob.ptr.data);
            if (instance_geometries != instance_geometries_by_object.end()) {
              for (BL::ID geometry : instance_geometries->second) {
                geometry_map.set_recalc(geometry);
              }
            }
          }

          if (updated_geometry) {
            BL::Object::particle_systems_iterator b_psys;
            for (b_ob.particle_systems.begin(b_psys); b_psys != b_ob.particle_systems.end();
                 ++b_psys)
            {
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
      else if (object_is_camera(b_ob)) {
        shader_map.set_recalc(b_ob);
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
      shader_map.set_recalc(b_world);
    }
    /* World */
    else if (b_id.is_a(&RNA_Scene)) {
      shader_map.set_recalc(b_id);
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
                            void **python_thread_state,
                            const DeviceInfo &device_info)
{
  /* For auto refresh images. */
  ImageManager *image_manager = scene->image_manager;
  const int frame = b_scene.frame_current();
  const bool auto_refresh_update = image_manager->set_animation_frame_update(frame);

  if (!has_updates_ && !auto_refresh_update) {
    return;
  }

  scoped_timer timer;

  BL::ViewLayer b_view_layer = b_depsgraph.view_layer_eval();

  /* TODO(sergey): This feels weak to pass view layer to the integrator, and even weaker to have an
   * implicit check on whether it is a background render or not. What is the nicer thing here? */
  const bool background = !b_v3d;

  sync_view_layer(b_view_layer);
  sync_integrator(b_view_layer, background, device_info);
  sync_film(b_view_layer, b_v3d);
  sync_shaders(b_depsgraph, b_v3d, auto_refresh_update);
  sync_images();

  geometry_synced.clear(); /* use for objects and motion sync */

  if (scene->need_motion() == Scene::MOTION_PASS || scene->need_motion() == Scene::MOTION_NONE ||
      scene->camera->get_motion_position() == MOTION_POSITION_CENTER)
  {
    sync_objects(b_depsgraph, b_v3d);
  }
  sync_motion(b_render, b_depsgraph, b_v3d, b_override, width, height, python_thread_state);

  geometry_synced.clear();

  /* Shader sync done at the end, since object sync uses it.
   * false = don't delete unused shaders, not supported. */
  shader_map.post_sync(false);

  VLOG_INFO << "Total time spent synchronizing data: " << timer.get_time();

  has_updates_ = false;
}

/* Integrator */

void BlenderSync::sync_integrator(BL::ViewLayer &b_view_layer,
                                  bool background,
                                  const DeviceInfo &device_info)
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

  bool use_light_tree = get_boolean(cscene, "use_light_tree");
  integrator->set_use_light_tree(use_light_tree);
  integrator->set_light_sampling_threshold(get_float(cscene, "light_sampling_threshold"));

  if (integrator->use_light_tree_is_modified()) {
    scene->light_manager->tag_update(scene, LightManager::UPDATE_ALL);
  }

  SamplingPattern sampling_pattern;
  if (use_developer_ui) {
    sampling_pattern = (SamplingPattern)get_enum(
        cscene, "sampling_pattern", SAMPLING_NUM_PATTERNS, SAMPLING_PATTERN_TABULATED_SOBOL);
  }
  else {
    sampling_pattern = SAMPLING_PATTERN_TABULATED_SOBOL;
  }
  integrator->set_sampling_pattern(sampling_pattern);

  int samples = 1;
  bool use_adaptive_sampling = false;
  if (preview) {
    samples = get_int(cscene, "preview_samples");
    use_adaptive_sampling = RNA_boolean_get(&cscene, "use_preview_adaptive_sampling");
    integrator->set_use_adaptive_sampling(use_adaptive_sampling);
    integrator->set_adaptive_threshold(get_float(cscene, "preview_adaptive_threshold"));
    integrator->set_adaptive_min_samples(get_int(cscene, "preview_adaptive_min_samples"));
  }
  else {
    samples = get_int(cscene, "samples");
    use_adaptive_sampling = RNA_boolean_get(&cscene, "use_adaptive_sampling");
    integrator->set_use_adaptive_sampling(use_adaptive_sampling);
    integrator->set_adaptive_threshold(get_float(cscene, "adaptive_threshold"));
    integrator->set_adaptive_min_samples(get_int(cscene, "adaptive_min_samples"));
  }

  float scrambling_distance = get_float(cscene, "scrambling_distance");
  bool auto_scrambling_distance = get_boolean(cscene, "auto_scrambling_distance");
  if (auto_scrambling_distance) {
    if (samples == 0) {
      /* If samples is 0, then viewport rendering is set to render infinitely. In that case we
       * override the samples value with 4096 so the Automatic Scrambling Distance algorithm
       * picks a Scrambling Distance value with a good balance of performance and correlation
       * artifacts when rendering to high sample counts. */
      samples = 4096;
    }

    if (use_adaptive_sampling) {
      /* If Adaptive Sampling is enabled, use "min_samples" in the Automatic Scrambling Distance
       * algorithm to avoid artifacts common with Adaptive Sampling + Scrambling Distance. */
      const AdaptiveSampling adaptive_sampling = integrator->get_adaptive_sampling();
      samples = min(samples, adaptive_sampling.min_samples);
    }
    scrambling_distance *= 4.0f / sqrtf(samples);
  }

  /* Only use scrambling distance in the viewport if user wants to. */
  bool preview_scrambling_distance = get_boolean(cscene, "preview_scrambling_distance");
  if ((preview && !preview_scrambling_distance) ||
      sampling_pattern == SAMPLING_PATTERN_SOBOL_BURLEY)
  {
    scrambling_distance = 1.0f;
  }

  if (scrambling_distance != 1.0f) {
    VLOG_INFO << "Using scrambling distance: " << scrambling_distance;
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

#ifdef WITH_CYCLES_DEBUG
  DirectLightSamplingType direct_light_sampling_type = (DirectLightSamplingType)get_enum(
      cscene, "direct_light_sampling_type", DIRECT_LIGHT_SAMPLING_NUM, DIRECT_LIGHT_SAMPLING_MIS);
  integrator->set_direct_light_sampling_type(direct_light_sampling_type);
#endif

  integrator->set_use_guiding(get_boolean(cscene, "use_guiding"));
  integrator->set_use_surface_guiding(get_boolean(cscene, "use_surface_guiding"));
  integrator->set_use_volume_guiding(get_boolean(cscene, "use_volume_guiding"));
  integrator->set_guiding_training_samples(get_int(cscene, "guiding_training_samples"));

  if (use_developer_ui) {
    integrator->set_deterministic_guiding(get_boolean(cscene, "use_deterministic_guiding"));
    integrator->set_surface_guiding_probability(get_float(cscene, "surface_guiding_probability"));
    integrator->set_volume_guiding_probability(get_float(cscene, "volume_guiding_probability"));
    integrator->set_use_guiding_direct_light(get_boolean(cscene, "use_guiding_direct_light"));
    integrator->set_use_guiding_mis_weights(get_boolean(cscene, "use_guiding_mis_weights"));
    GuidingDistributionType guiding_distribution_type = (GuidingDistributionType)get_enum(
        cscene, "guiding_distribution_type", GUIDING_NUM_TYPES, GUIDING_TYPE_PARALLAX_AWARE_VMM);
    integrator->set_guiding_distribution_type(guiding_distribution_type);
    GuidingDirectionalSamplingType guiding_directional_sampling_type =
        (GuidingDirectionalSamplingType)get_enum(cscene,
                                                 "guiding_directional_sampling_type",
                                                 GUIDING_DIRECTIONAL_SAMPLING_NUM_TYPES,
                                                 GUIDING_DIRECTIONAL_SAMPLING_TYPE_RIS);
    integrator->set_guiding_directional_sampling_type(guiding_directional_sampling_type);
    integrator->set_guiding_roughness_threshold(get_float(cscene, "guiding_roughness_threshold"));
  }

  DenoiseParams denoise_params = get_denoise_params(
      b_scene, b_view_layer, background, device_info);

  /* No denoising support for vertex color baking, vertices packed into image
   * buffer have no relation to neighbors. */
  if (scene->bake_manager->get_baking() &&
      b_scene.render().bake().target() != BL::BakeSettings::target_IMAGE_TEXTURES)
  {
    denoise_params.use = false;
  }

  integrator->set_use_denoise(denoise_params.use);

  /* Only update denoiser parameters if the denoiser is actually used. This allows to tweak
   * denoiser parameters before enabling it without render resetting on every change. The downside
   * is that the interface and the integrator are technically out of sync. */
  if (denoise_params.use) {
    integrator->set_denoiser_type(denoise_params.type);
    integrator->set_denoise_use_gpu(denoise_params.use_gpu);
    integrator->set_denoise_start_sample(denoise_params.start_sample);
    integrator->set_use_denoise_pass_albedo(denoise_params.use_pass_albedo);
    integrator->set_use_denoise_pass_normal(denoise_params.use_pass_normal);
    integrator->set_denoiser_prefilter(denoise_params.prefilter);
    integrator->set_denoiser_quality(denoise_params.quality);
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
  /* World override. */
  view_layer.world_override = b_view_layer.world_override();

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

static bool get_known_pass_type(BL::RenderPass &b_pass, PassType &type, PassMode &mode)
{
  string name = b_pass.name();
#define MAP_PASS(passname, passtype, noisy) \
  if (name == passname) { \
    type = passtype; \
    mode = (noisy) ? PassMode::NOISY : PassMode::DENOISED; \
    return true; \
  } \
  ((void)0)

  /* NOTE: Keep in sync with defined names from engine.py */

  MAP_PASS("Combined", PASS_COMBINED, false);
  MAP_PASS("Noisy Image", PASS_COMBINED, true);

  MAP_PASS("Depth", PASS_DEPTH, false);
  MAP_PASS("Mist", PASS_MIST, false);
  MAP_PASS("Position", PASS_POSITION, false);
  MAP_PASS("Normal", PASS_NORMAL, false);
  MAP_PASS("IndexOB", PASS_OBJECT_ID, false);
  MAP_PASS("UV", PASS_UV, false);
  MAP_PASS("Vector", PASS_MOTION, false);
  MAP_PASS("IndexMA", PASS_MATERIAL_ID, false);

  MAP_PASS("DiffDir", PASS_DIFFUSE_DIRECT, false);
  MAP_PASS("GlossDir", PASS_GLOSSY_DIRECT, false);
  MAP_PASS("TransDir", PASS_TRANSMISSION_DIRECT, false);
  MAP_PASS("VolumeDir", PASS_VOLUME_DIRECT, false);

  MAP_PASS("DiffInd", PASS_DIFFUSE_INDIRECT, false);
  MAP_PASS("GlossInd", PASS_GLOSSY_INDIRECT, false);
  MAP_PASS("TransInd", PASS_TRANSMISSION_INDIRECT, false);
  MAP_PASS("VolumeInd", PASS_VOLUME_INDIRECT, false);

  MAP_PASS("DiffCol", PASS_DIFFUSE_COLOR, false);
  MAP_PASS("GlossCol", PASS_GLOSSY_COLOR, false);
  MAP_PASS("TransCol", PASS_TRANSMISSION_COLOR, false);

  MAP_PASS("Emit", PASS_EMISSION, false);
  MAP_PASS("Env", PASS_BACKGROUND, false);
  MAP_PASS("AO", PASS_AO, false);

  MAP_PASS("BakePrimitive", PASS_BAKE_PRIMITIVE, false);
  MAP_PASS("BakeDifferential", PASS_BAKE_DIFFERENTIAL, false);

  MAP_PASS("Denoising Normal", PASS_DENOISING_NORMAL, true);
  MAP_PASS("Denoising Albedo", PASS_DENOISING_ALBEDO, true);
  MAP_PASS("Denoising Depth", PASS_DENOISING_DEPTH, true);

  MAP_PASS("Shadow Catcher", PASS_SHADOW_CATCHER, false);
  MAP_PASS("Noisy Shadow Catcher", PASS_SHADOW_CATCHER, true);

  MAP_PASS("AdaptiveAuxBuffer", PASS_ADAPTIVE_AUX_BUFFER, false);
  MAP_PASS("Debug Sample Count", PASS_SAMPLE_COUNT, false);

  MAP_PASS("Guiding Color", PASS_GUIDING_COLOR, false);
  MAP_PASS("Guiding Probability", PASS_GUIDING_PROBABILITY, false);
  MAP_PASS("Guiding Average Roughness", PASS_GUIDING_AVG_ROUGHNESS, false);

  if (string_startswith(name, cryptomatte_prefix)) {
    type = PASS_CRYPTOMATTE;
    mode = PassMode::DENOISED;
    return true;
  }

#undef MAP_PASS

  return false;
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
  /* Delete all existing passes. */
  set<Pass *> clear_passes(scene->passes.begin(), scene->passes.end());
  scene->delete_nodes(clear_passes);

  /* Always add combined pass. */
  pass_add(scene, PASS_COMBINED, "Combined");

  /* Cryptomatte stores two ID/weight pairs per RGBA layer.
   * User facing parameter is the number of pairs. */
  int crypto_depth = divide_up(min(16, b_view_layer.pass_cryptomatte_depth()), 2);
  scene->film->set_cryptomatte_depth(crypto_depth);
  CryptomatteType cryptomatte_passes = CRYPT_NONE;
  if (b_view_layer.use_pass_cryptomatte_object()) {
    cryptomatte_passes = (CryptomatteType)(cryptomatte_passes | CRYPT_OBJECT);
  }
  if (b_view_layer.use_pass_cryptomatte_material()) {
    cryptomatte_passes = (CryptomatteType)(cryptomatte_passes | CRYPT_MATERIAL);
  }
  if (b_view_layer.use_pass_cryptomatte_asset()) {
    cryptomatte_passes = (CryptomatteType)(cryptomatte_passes | CRYPT_ASSET);
  }
  scene->film->set_cryptomatte_passes(cryptomatte_passes);

  unordered_set<string> expected_passes;

  /* Custom AOV passes. */
  BL::ViewLayer::aovs_iterator b_aov_iter;
  for (b_view_layer.aovs.begin(b_aov_iter); b_aov_iter != b_view_layer.aovs.end(); ++b_aov_iter) {
    BL::AOV b_aov(*b_aov_iter);
    if (!b_aov.is_valid()) {
      continue;
    }

    string name = b_aov.name();
    PassType type = (b_aov.type() == BL::AOV::type_COLOR) ? PASS_AOV_COLOR : PASS_AOV_VALUE;

    pass_add(scene, type, name.c_str());
    expected_passes.insert(name);
  }

  /* Light Group passes. */
  BL::ViewLayer::lightgroups_iterator b_lightgroup_iter;
  for (b_view_layer.lightgroups.begin(b_lightgroup_iter);
       b_lightgroup_iter != b_view_layer.lightgroups.end();
       ++b_lightgroup_iter)
  {
    BL::Lightgroup b_lightgroup(*b_lightgroup_iter);

    string name = string_printf("Combined_%s", b_lightgroup.name().c_str());

    Pass *pass = pass_add(scene, PASS_COMBINED, name.c_str(), PassMode::NOISY);
    pass->set_lightgroup(ustring(b_lightgroup.name()));
    expected_passes.insert(name);
  }

  /* Sync the passes that were defined in engine.py. */
  for (BL::RenderPass &b_pass : b_rlay.passes) {
    PassType pass_type = PASS_NONE;
    PassMode pass_mode = PassMode::DENOISED;

    if (!get_known_pass_type(b_pass, pass_type, pass_mode)) {
      if (!expected_passes.count(b_pass.name())) {
        LOG(ERROR) << "Unknown pass " << b_pass.name();
      }
      continue;
    }

    if (pass_type == PASS_MOTION &&
        (b_view_layer.use_motion_blur() && b_scene.render().use_motion_blur()))
    {
      continue;
    }

    pass_add(scene, pass_type, b_pass.name().c_str(), pass_mode);
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
       * geometry we're baking, see #71012. */
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
    /* Grease pencil render requires all evaluated objects available as-is after Cycles is done
     * with its part. */
    if (b_ob.type() == BL::Object::type_GREASEPENCIL || b_ob.type() == BL::Object::type_GPENCIL) {
      continue;
    }
    b_ob.cache_release();
  }
}

/* Scene Parameters */

SceneParams BlenderSync::get_scene_params(BL::Scene &b_scene,
                                          const bool background,
                                          const bool use_developer_ui)
{
  SceneParams params;
  PointerRNA cscene = RNA_pointer_get(&b_scene.ptr, "cycles");
  const bool shadingsystem = RNA_boolean_get(&cscene, "shading_system");

  if (shadingsystem == 0) {
    params.shadingsystem = SHADINGSYSTEM_SVM;
  }
  else if (shadingsystem == 1) {
    params.shadingsystem = SHADINGSYSTEM_OSL;
  }

  if (background || (use_developer_ui && get_enum(cscene, "debug_bvh_type"))) {
    params.bvh_type = BVH_TYPE_STATIC;
  }
  else {
    params.bvh_type = BVH_TYPE_DYNAMIC;
  }

  params.use_bvh_spatial_split = RNA_boolean_get(&cscene, "debug_use_spatial_splits");
  params.use_bvh_compact_structure = RNA_boolean_get(&cscene, "debug_use_compact_bvh");
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

  if (background && !b_engine.is_preview()) {
    /* Viewport and preview renders do not require temp directory and do request session
     * parameters more often than the background render.
     * Optimize RNA-C++ usage and memory allocation a bit by saving string access which we know is
     * not needed for viewport render. */
    params.temp_dir = b_engine.temporary_directory();
  }

  /* feature set */
  params.experimental = (get_enum(cscene, "feature_set") != 0);

  /* Headless and background rendering. */
  params.headless = BlenderSession::headless;
  params.background = background;

  /* Device */
  params.threads = blender_device_threads(b_scene);
  params.device = blender_device_info(
      b_preferences, b_scene, params.background, b_engine.is_preview(), params.denoise_device);

  /* samples */
  int samples = get_int(cscene, "samples");
  int preview_samples = get_int(cscene, "preview_samples");
  int sample_offset = get_int(cscene, "sample_offset");

  if (background) {
    params.samples = samples;
    params.sample_offset = sample_offset;
  }
  else {
    params.samples = preview_samples;
    if (params.samples == 0) {
      params.samples = INT_MAX;
    }
    params.sample_offset = 0;
  }

  /* Clamp sample offset. */
  params.sample_offset = clamp(params.sample_offset, 0, Integrator::MAX_SAMPLES);

  /* Clamp samples. */
  params.samples = clamp(params.samples, 0, Integrator::MAX_SAMPLES - params.sample_offset);

  /* Viewport Performance */
  params.pixel_size = b_engine.get_preview_pixel_size(b_scene);

  if (background) {
    params.pixel_size = 1;
  }

  /* shading system - scene level needs full refresh */
  const bool shadingsystem = RNA_boolean_get(&cscene, "shading_system");

  if (shadingsystem == 0) {
    params.shadingsystem = SHADINGSYSTEM_SVM;
  }
  else if (shadingsystem == 1) {
    params.shadingsystem = SHADINGSYSTEM_OSL;
  }

  /* Time limit. */
  if (background) {
    params.time_limit = (double)get_float(cscene, "time_limit");
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
                                              bool background,
                                              const DeviceInfo &device_info)
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
    denoising.use_gpu = get_boolean(cscene, "denoising_use_gpu");
    denoising.prefilter = (DenoiserPrefilter)get_enum(
        cscene, "denoising_prefilter", DENOISER_PREFILTER_NUM, DENOISER_PREFILTER_NONE);
    denoising.quality = (DenoiserQuality)get_enum(
        cscene, "denoising_quality", DENOISER_QUALITY_NUM, DENOISER_QUALITY_HIGH);

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
    denoising.use_gpu = get_boolean(cscene, "preview_denoising_use_gpu");
    denoising.prefilter = (DenoiserPrefilter)get_enum(
        cscene, "preview_denoising_prefilter", DENOISER_PREFILTER_NUM, DENOISER_PREFILTER_FAST);
    denoising.quality = (DenoiserQuality)get_enum(
        cscene, "preview_denoising_quality", DENOISER_QUALITY_NUM, DENOISER_QUALITY_BALANCED);
    denoising.start_sample = get_int(cscene, "preview_denoising_start_sample");

    input_passes = (DenoiserInput)get_enum(
        cscene, "preview_denoising_input_passes", DENOISER_INPUT_NUM, DENOISER_INPUT_RGB_ALBEDO);

    /* Auto select fastest denoiser. */
    if (denoising.type == DENOISER_NONE) {
      denoising.type = Denoiser::automatic_viewport_denoiser_type(device_info);
      if (denoising.type == DENOISER_NONE) {
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
