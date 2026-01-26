/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "BKE_appdir.hh"
#include "BKE_scene.hh"
#include "DEG_depsgraph_query.hh"
#include "DNA_world_types.h"
#include "RNA_prototypes.hh"
#include "RNA_types.hh"

#include "scene/background.h"
#include "scene/bake.h"
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

#include "util/hash.h"
#include "util/log.h"

CCL_NAMESPACE_BEGIN

static const char *cryptomatte_prefix = "Crypto";

/* Constructor */

BlenderSync::BlenderSync(blender::RenderEngine &b_engine,
                         blender::Main &b_data,
                         blender::Scene &b_scene,
                         Scene *scene,
                         bool preview,
                         bool use_developer_ui,
                         Progress &progress)
    : b_engine(&b_engine),
      b_data(&b_data),
      b_scene(&b_scene),
      b_bake_target(nullptr),
      shader_map(scene),
      object_map(scene),
      procedural_map(scene),
      geometry_map(scene),
      particle_system_map(scene),
      world_map(nullptr),
      world_recalc(false),
      scene(scene),
      preview(preview),
      use_developer_ui(use_developer_ui),
      dicing_rate(1.0f),
      max_subdivisions(12),
      progress(progress)

{
  blender::PointerRNA scene_rna_ptr = RNA_id_pointer_create(&b_scene.id);
  blender::PointerRNA cscene = RNA_pointer_get(&scene_rna_ptr, "cycles");
  dicing_rate = preview ? RNA_float_get(&cscene, "preview_dicing_rate") :
                          RNA_float_get(&cscene, "dicing_rate");
  max_subdivisions = RNA_int_get(&cscene, "max_subdivisions");
}

BlenderSync::~BlenderSync() = default;

void BlenderSync::reset(blender::Main &b_data, blender::Scene &b_scene)
{
  /* Update data and scene pointers in case they change in session reset,
   * for example after undo.
   * Note that we do not modify the `has_updates_` flag here because the sync
   * reset is also used during viewport navigation. */
  this->b_data = &b_data;
  this->b_scene = &b_scene;
}

void BlenderSync::tag_update()
{
  has_updates_ = true;
}

void BlenderSync::set_bake_target(blender::Object &b_object)
{
  b_bake_target = &b_object;
}

/* Sync */

void BlenderSync::sync_recalc(blender::Depsgraph &b_depsgraph,
                              blender::bScreen *b_screen,
                              blender::View3D *b_v3d,
                              blender::RegionView3D *b_rv3d)
{
  /* Sync recalc flags from blender to cycles. Actual update is done separate,
   * so we can do it later on if doing it immediate is not suitable. */
  blender::Object *b_dicing_camera_object = get_dicing_camera_object(b_v3d, b_rv3d);
  bool dicing_camera_updated = false;

  /* Iterate over all blender::IDs in this depsgraph. */
  blender::DEGIDIterData deg_iter_data{};
  deg_iter_data.graph = &b_depsgraph;
  deg_iter_data.only_updated = true;
  ITER_BEGIN (blender::DEG_iterator_ids_begin,
              blender::DEG_iterator_ids_next,
              blender::DEG_iterator_ids_end,
              &deg_iter_data,
              blender::ID *,
              b_id)
  {
    /* TODO(sergey): Can do more selective filter here. For example, ignore changes made to
     * screen data-block. Note that sync_data() needs to be called after object deletion, and
     * currently this is ensured by the scene blender::ID tagged for update, which sets the
     * `has_updates_` flag. */
    has_updates_ = true;

    const bool updated_shading = ((b_id->recalc & (blender::ID_RECALC_SHADING |
                                                   blender::ID_RECALC_ANIMATION)) != 0);

    /* Material */
    if (GS(b_id->name) == blender::ID_MA) {
      shader_map.set_recalc(b_id);
    }
    /* Light */
    else if (GS(b_id->name) == blender::ID_LA) {
      shader_map.set_recalc(b_id);
      geometry_map.set_recalc(b_id);
    }
    /* Object */
    else if (GS(b_id->name) == blender::ID_OB) {
      blender::Object *b_ob = blender::id_cast<blender::Object *>(b_id);
      const bool can_have_geometry = object_can_have_geometry(*b_ob);
      const bool is_light = !can_have_geometry && object_is_light(*b_ob);

      if (((b_ob->transflag & blender::OB_DUPLI) != 0) && updated_shading) {
        /* Needed for object color updates on instancer, among other things. */
        object_map.set_recalc(b_ob);
      }

      if (can_have_geometry || is_light) {
        const bool updated_geometry = (b_id->recalc & (blender::ID_RECALC_GEOMETRY |
                                                       blender::ID_RECALC_ALL)) != 0;
        const bool updated_transform = (b_id->recalc & blender::ID_RECALC_TRANSFORM) != 0;

        /* Geometry (mesh, hair, volume). */
        if (can_have_geometry) {
          if (updated_transform || updated_shading) {
            object_map.set_recalc(b_ob);
          }

          const bool use_adaptive_subdiv = object_subdivision_type(
                                               *b_ob, preview, use_adaptive_subdivision) !=
                                           Mesh::SUBDIVISION_NONE;

          /* Need to recompute geometry if the geometry changed, or the transform changed
           * and using adaptive subdivision. */
          if (updated_geometry || (updated_transform && use_adaptive_subdiv)) {
            blender::ID *key = BKE_object_is_modified(*b_ob) ?
                                   &b_ob->id :
                                   object_get_data(*b_ob, use_adaptive_subdiv);
            geometry_map.set_recalc(key);

            /* Sync all contained geometry instances as well when the object changed.. */
            const map<void *, set<blender::ID *>>::const_iterator instance_geometries =
                instance_geometries_by_object.find(b_ob);
            if (instance_geometries != instance_geometries_by_object.end()) {
              for (blender::ID *geometry : instance_geometries->second) {
                geometry_map.set_recalc(geometry);
              }
            }
          }

          if (updated_geometry) {
            if (!BLI_listbase_is_empty(&b_ob->particlesystem)) {
              particle_system_map.set_recalc(b_ob);
            }
          }
        }
        /* Light */
        else if (is_light) {
          if (updated_transform || updated_shading) {
            object_map.set_recalc(b_ob);
            geometry_map.set_recalc(b_ob);
          }

          if (updated_geometry) {
            geometry_map.set_recalc(b_ob);
          }
        }
      }
      else if (object_is_camera(*b_ob)) {
        shader_map.set_recalc(b_ob);
      }

      if (b_dicing_camera_object == b_ob) {
        dicing_camera_updated = true;
      }
    }
    /* Mesh */
    else if (GS(b_id->name) == blender::ID_ME) {
      geometry_map.set_recalc(b_id);
    }
    /* World */
    else if (GS(b_id->name) == blender::ID_WO) {
      const blender::World *b_world = blender::id_cast<blender::World *>(b_id);
      if (world_map == b_world) {
        world_recalc = true;
      }
      shader_map.set_recalc(b_id);
    }
    /* World */
    else if (GS(b_id->name) == blender::ID_SCE) {
      shader_map.set_recalc(b_id);
    }
    /* Volume */
    else if (GS(b_id->name) == blender::ID_VO) {
      geometry_map.set_recalc(b_id);
    }
    /* Camera */
    else if (GS(b_id->name) == blender::ID_CA) {
      if (b_dicing_camera_object && b_dicing_camera_object->data == b_id) {
        dicing_camera_updated = true;
      }
    }
  }

  if (use_adaptive_subdivision) {
    /* Mark all meshes as needing to be exported again if dicing changed. */
    blender::PointerRNA scene_rna_ptr = RNA_id_pointer_create(&b_scene->id);
    blender::PointerRNA cscene = RNA_pointer_get(&scene_rna_ptr, "cycles");
    bool dicing_prop_changed = false;

    const float updated_dicing_rate = preview ? RNA_float_get(&cscene, "preview_dicing_rate") :
                                                RNA_float_get(&cscene, "dicing_rate");

    if (dicing_rate != updated_dicing_rate) {
      dicing_rate = updated_dicing_rate;
      dicing_prop_changed = true;
    }

    const int updated_max_subdivisions = RNA_int_get(&cscene, "max_subdivisions");

    if (max_subdivisions != updated_max_subdivisions) {
      max_subdivisions = updated_max_subdivisions;
      dicing_prop_changed = true;
    }

    if ((dicing_camera_updated && !region_view3d_navigating_or_transforming(b_rv3d)) ||
        dicing_prop_changed)
    {
      has_updates_ = true;

      for (const pair<const GeometryKey, Geometry *> &iter : geometry_map.key_to_scene_data()) {
        Geometry *geom = iter.second;
        if (geom->is_mesh()) {
          Mesh *mesh = static_cast<Mesh *>(geom);
          if (mesh->get_subdivision_type() != Mesh::SUBDIVISION_NONE) {
            geometry_map.set_recalc(iter.first.id);
          }
        }
      }
    }
  }
  ITER_END;

  if (b_v3d) {
    const BlenderViewportParameters new_viewport_parameters(b_screen, b_v3d, use_developer_ui);

    if (viewport_parameters.shader_modified(new_viewport_parameters)) {
      world_recalc = true;
      has_updates_ = true;
    }

    has_updates_ |= viewport_parameters.modified(new_viewport_parameters);
  }
}

void BlenderSync::sync_data(blender::RenderData &b_render,
                            blender::Depsgraph &b_depsgraph,
                            blender::bScreen *b_screen,
                            blender::View3D *b_v3d,
                            blender::RegionView3D *b_rv3d,
                            const int width,
                            const int height,
                            void **python_thread_state,
                            const DeviceInfo &denoise_device_info)
{
  /* For auto refresh images. */
  ImageManager *image_manager = scene->image_manager.get();
  const int frame = b_scene->r.cfra;
  const bool auto_refresh_update = image_manager->set_animation_frame_update(frame);

  if (!has_updates_ && !auto_refresh_update) {
    return;
  }

  const scoped_timer timer;

  blender::ViewLayer &b_view_layer = *DEG_get_evaluated_view_layer(&b_depsgraph);

  /* TODO(sergey): This feels weak to pass view layer to the integrator, and even weaker to have an
   * implicit check on whether it is a background render or not. What is the nicer thing here? */
  const bool background = !b_v3d;

  sync_view_layer(b_view_layer);
  sync_integrator(b_view_layer, background, denoise_device_info);
  sync_film(b_view_layer, b_screen, b_v3d);
  sync_shaders(b_depsgraph, b_screen, b_v3d, auto_refresh_update);
  sync_images();

  geometry_synced.clear(); /* use for objects and motion sync */

  if (scene->need_motion() == Scene::MOTION_NONE || scene->need_motion() == Scene::MOTION_PASS ||
      scene->camera->get_motion_position() == MOTION_POSITION_CENTER)
  {
    sync_objects(b_depsgraph, b_screen, b_v3d);
  }
  sync_motion(b_render, b_depsgraph, b_screen, b_v3d, b_rv3d, width, height, python_thread_state);

  geometry_synced.clear();

  /* Shader sync done at the end, since object sync uses it.
   * false = don't delete unused shaders, not supported. */
  shader_map.post_sync(false);

  LOG_INFO << "Total time spent synchronizing data: " << timer.get_time();

  has_updates_ = false;
}

/* Integrator */

void BlenderSync::sync_integrator(blender::ViewLayer &b_view_layer,
                                  bool background,
                                  const DeviceInfo &denoise_device_info)
{
  blender::PointerRNA scene_rna_ptr = RNA_id_pointer_create(&b_scene->id);
  blender::PointerRNA cscene = RNA_pointer_get(&scene_rna_ptr, "cycles");

  /* No adaptive subdivision for baking, mesh needs to match Blender exactly. */
  use_adaptive_subdivision = !b_bake_target;

  Integrator *integrator = scene->integrator;

  integrator->set_min_bounce(get_int(cscene, "min_light_bounces"));
  integrator->set_max_bounce(get_int(cscene, "max_bounces"));

  integrator->set_max_diffuse_bounce(get_int(cscene, "diffuse_bounces"));
  integrator->set_max_glossy_bounce(get_int(cscene, "glossy_bounces"));
  integrator->set_max_transmission_bounce(get_int(cscene, "transmission_bounces"));
  integrator->set_max_volume_bounce(get_int(cscene, "volume_bounces"));
  integrator->set_transparent_min_bounce(get_int(cscene, "min_transparent_bounces"));
  integrator->set_transparent_max_bounce(get_int(cscene, "transparent_max_bounces"));

  integrator->set_volume_ray_marching(get_boolean(cscene, "volume_biased"));
  integrator->set_volume_max_steps(get_int(cscene, "volume_max_steps"));
  const float volume_step_rate = (preview) ? get_float(cscene, "volume_preview_step_rate") :
                                             get_float(cscene, "volume_step_rate");
  integrator->set_volume_step_rate(volume_step_rate);

  integrator->set_caustics_reflective(get_boolean(cscene, "caustics_reflective"));
  integrator->set_caustics_refractive(get_boolean(cscene, "caustics_refractive"));
  integrator->set_filter_glossy(get_float(cscene, "blur_glossy"));

  int seed = get_int(cscene, "seed");
  if (get_boolean(cscene, "use_animated_seed")) {
    seed = hash_uint2(b_scene->r.cfra, get_int(cscene, "seed"));
    if (b_scene->r.subframe != 0.0f) {
      /* TODO(sergey): Ideally should be some sort of hash_merge,
       * but this is good enough for now.
       */
      seed += hash_uint2((int)(b_scene->r.subframe * (float)INT_MAX), get_int(cscene, "seed"));
    }
  }

  integrator->set_seed(seed);

  integrator->set_sample_clamp_direct(get_float(cscene, "sample_clamp_direct"));
  integrator->set_sample_clamp_indirect(get_float(cscene, "sample_clamp_indirect"));
  if (!preview) {
    integrator->set_motion_blur(view_layer.use_motion_blur);
  }

  const bool use_light_tree = get_boolean(cscene, "use_light_tree");
  integrator->set_use_light_tree(use_light_tree);
  integrator->set_light_sampling_threshold(get_float(cscene, "light_sampling_threshold"));

  if (integrator->use_light_tree_is_modified()) {
    scene->light_manager->tag_update(scene, LightManager::UPDATE_ALL);
  }

  SamplingPattern sampling_pattern = (SamplingPattern)get_enum(
      cscene, "sampling_pattern", SAMPLING_NUM_PATTERNS, SAMPLING_PATTERN_TABULATED_SOBOL);

  switch (sampling_pattern) {
    case SAMPLING_PATTERN_AUTOMATIC:
      if (!background) {
        /* For interactive rendering, ensure that the first sample is in itself
         * blue-noise-distributed for smooth viewport navigation. */
        sampling_pattern = SAMPLING_PATTERN_BLUE_NOISE_FIRST;
      }
      else {
        /* For non-interactive rendering, default to a full blue-noise pattern. */
        sampling_pattern = SAMPLING_PATTERN_BLUE_NOISE_PURE;
      }
      break;
    case SAMPLING_PATTERN_TABULATED_SOBOL:
    case SAMPLING_PATTERN_BLUE_NOISE_PURE:
      /* Always allowed. */
      break;
    default:
      /* If not using developer UI, default to blue noise for "advanced" patterns. */
      if (!use_developer_ui) {
        sampling_pattern = SAMPLING_PATTERN_BLUE_NOISE_PURE;
      }
      break;
  }

  const bool is_vertex_baking = b_bake_target &&
                                b_scene->r.bake.target != blender::R_BAKE_TARGET_IMAGE_TEXTURES;
  scene->bake_manager->set_use_seed(is_vertex_baking);
  if (is_vertex_baking) {
    /* When baking vertex colors, the "pixels" in the output are unrelated to their neighbors,
     * so blue-noise sampling makes no sense. */
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
  const bool auto_scrambling_distance = get_boolean(cscene, "auto_scrambling_distance");
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
  const bool preview_scrambling_distance = get_boolean(cscene, "preview_scrambling_distance");
  if ((preview && !preview_scrambling_distance) ||
      sampling_pattern != SAMPLING_PATTERN_TABULATED_SOBOL)
  {
    scrambling_distance = 1.0f;
  }

  if (scrambling_distance != 1.0f) {
    LOG_INFO << "Using scrambling distance: " << scrambling_distance;
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
    const GuidingDistributionType guiding_distribution_type = (GuidingDistributionType)get_enum(
        cscene, "guiding_distribution_type", GUIDING_NUM_TYPES, GUIDING_TYPE_PARALLAX_AWARE_VMM);
    integrator->set_guiding_distribution_type(guiding_distribution_type);
    const GuidingDirectionalSamplingType guiding_directional_sampling_type =
        (GuidingDirectionalSamplingType)get_enum(cscene,
                                                 "guiding_directional_sampling_type",
                                                 GUIDING_DIRECTIONAL_SAMPLING_NUM_TYPES,
                                                 GUIDING_DIRECTIONAL_SAMPLING_TYPE_RIS);
    integrator->set_guiding_directional_sampling_type(guiding_directional_sampling_type);
    integrator->set_guiding_roughness_threshold(get_float(cscene, "guiding_roughness_threshold"));
  }

  DenoiseParams denoise_params = get_denoise_params(
      *b_scene, &b_view_layer, background, denoise_device_info);

  /* No denoising support for vertex color baking, vertices packed into image
   * buffer have no relation to neighbors. */
  if (is_vertex_baking) {
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

void BlenderSync::sync_film(blender::ViewLayer &b_view_layer,
                            blender::bScreen *b_screen,
                            blender::View3D *b_v3d)
{
  blender::PointerRNA scene_rna_ptr = RNA_id_pointer_create(&b_scene->id);
  blender::PointerRNA cscene = RNA_pointer_get(&scene_rna_ptr, "cycles");
  blender::PointerRNA view_layer_rna_ptr = RNA_pointer_create_id_subdata(
      b_scene->id, blender::RNA_ViewLayer, &b_view_layer);
  blender::PointerRNA crl = RNA_pointer_get(&view_layer_rna_ptr, "cycles");

  Film *film = scene->film;

  if (b_v3d) {
    const BlenderViewportParameters new_viewport_parameters(b_screen, b_v3d, use_developer_ui);
    film->set_display_pass(new_viewport_parameters.display_pass);
    film->set_show_active_pixels(new_viewport_parameters.show_active_pixels);
  }

  film->set_exposure(get_float(cscene, "film_exposure"));
  film->set_filter_type(
      (FilterType)get_enum(cscene, "pixel_filter_type", FILTER_NUM_TYPES, FILTER_BLACKMAN_HARRIS));
  const float filter_width = (film->get_filter_type() == FILTER_BOX) ?
                                 1.0f :
                                 get_float(cscene, "filter_width");
  film->set_filter_width(filter_width);

  if (b_scene->world) {
    film->set_mist_start(b_scene->world->miststa);
    film->set_mist_depth(b_scene->world->mistdist);

    switch (b_scene->world->mistype) {
      case blender::WO_MIST_QUADRATIC:
        film->set_mist_falloff(2.0f);
        break;
      case blender::WO_MIST_LINEAR:
        film->set_mist_falloff(1.0f);
        break;
      case blender::WO_MIST_INVERSE_QUADRATIC:
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

void BlenderSync::sync_view_layer(blender::ViewLayer &b_view_layer)
{
  view_layer.name = b_view_layer.name;

  /* Filter. */
  view_layer.use_background_shader = (b_view_layer.layflag & blender::SCE_LAY_SKY) != 0;
  /* Always enable surfaces for baking, otherwise there is nothing to bake to. */
  view_layer.use_surfaces = (b_view_layer.layflag & blender::SCE_LAY_SOLID) != 0 || b_bake_target;
  view_layer.use_hair = (b_view_layer.layflag & blender::SCE_LAY_STRAND) != 0;
  view_layer.use_volumes = (b_view_layer.layflag & blender::SCE_LAY_VOLUMES) != 0;
  view_layer.use_motion_blur = (b_view_layer.layflag & blender::SCE_LAY_MOTION_BLUR) != 0 &&
                               (b_scene->r.mode & blender::R_MBLUR) != 0;

  /* Material override. */
  view_layer.material_override = b_view_layer.mat_override;
  /* World override. */
  view_layer.world_override = b_view_layer.world_override;

  /* Sample override. */
  blender::PointerRNA scene_rna_ptr = RNA_id_pointer_create(&b_scene->id);
  blender::PointerRNA cscene = RNA_pointer_get(&scene_rna_ptr, "cycles");
  const int use_layer_samples = get_enum(cscene, "use_layer_samples");

  view_layer.bound_samples = (use_layer_samples == 1);
  view_layer.samples = 0;

  if (use_layer_samples != 2) {
    const int samples = b_view_layer.samples;
    view_layer.samples = samples;
  }
}

static blender::RenderData *engine_render_get(blender::RenderEngine *engine)
{
  if (engine->re) {
    return RE_engine_get_render_data(engine->re);
  }
  return nullptr;
}

/* Images */
void BlenderSync::sync_images()
{
  /* Sync is a convention for this API, but currently it frees unused buffers. */

  const bool is_interface_locked = engine_render_get(b_engine) &&
                                   engine_render_get(b_engine)->use_lock_interface;
  if (is_interface_locked == false && BlenderSession::headless == false) {
    /* If interface is not locked, it's possible image is needed for
     * the display.
     */
    return;
  }
  /* Free buffers used by images which are not needed for render. */
  for (blender::Image &b_image : b_data->images) {
    const bool is_builtin = image_is_builtin(b_image, *b_engine);
    if (is_builtin == false) {
      BKE_image_free_buffers_ex(&b_image, true);
    }
    /* TODO(sergey): Free builtin images not used by any shader. */
  }
}

/* Passes */

static bool get_known_pass_type(blender::RenderPass &b_pass, PassType &type, PassMode &mode)
{
  const string name = b_pass.name;
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
  MAP_PASS("Object Index", PASS_OBJECT_ID, false);
  MAP_PASS("UV", PASS_UV, false);
  MAP_PASS("Vector", PASS_MOTION, false);
  MAP_PASS("Material Index", PASS_MATERIAL_ID, false);

  MAP_PASS("Diffuse Direct", PASS_DIFFUSE_DIRECT, false);
  MAP_PASS("Glossy Direct", PASS_GLOSSY_DIRECT, false);
  MAP_PASS("Transmission Direct", PASS_TRANSMISSION_DIRECT, false);
  MAP_PASS("Volume Direct", PASS_VOLUME_DIRECT, false);

  MAP_PASS("Diffuse Indirect", PASS_DIFFUSE_INDIRECT, false);
  MAP_PASS("Glossy Indirect", PASS_GLOSSY_INDIRECT, false);
  MAP_PASS("Transmission Indirect", PASS_TRANSMISSION_INDIRECT, false);
  MAP_PASS("Volume Indirect", PASS_VOLUME_INDIRECT, false);
  MAP_PASS("Volume Scatter", PASS_VOLUME_SCATTER, false);
  MAP_PASS("Volume Transmit", PASS_VOLUME_TRANSMIT, false);
  MAP_PASS("Volume Majorant", PASS_VOLUME_MAJORANT, false);

  MAP_PASS("Diffuse Color", PASS_DIFFUSE_COLOR, false);
  MAP_PASS("Glossy Color", PASS_GLOSSY_COLOR, false);
  MAP_PASS("Transmission Color", PASS_TRANSMISSION_COLOR, false);

  MAP_PASS("Emission", PASS_EMISSION, false);
  MAP_PASS("Environment", PASS_BACKGROUND, false);
  MAP_PASS("Ambient Occlusion", PASS_AO, false);

  MAP_PASS("BakePrimitive", PASS_BAKE_PRIMITIVE, false);
  MAP_PASS("BakeSeed", PASS_BAKE_SEED, false);
  MAP_PASS("BakeDifferential", PASS_BAKE_DIFFERENTIAL, false);

  MAP_PASS("Denoising Albedo", PASS_DENOISING_ALBEDO, true);
  MAP_PASS("Denoising Normal", PASS_DENOISING_NORMAL, true);
  MAP_PASS("Denoising Depth", PASS_DENOISING_DEPTH, true);

  MAP_PASS("Shadow Catcher", PASS_SHADOW_CATCHER, false);
  MAP_PASS("Noisy Shadow Catcher", PASS_SHADOW_CATCHER, true);

  MAP_PASS("AdaptiveAuxBuffer", PASS_ADAPTIVE_AUX_BUFFER, false);
  MAP_PASS("Debug Sample Count", PASS_SAMPLE_COUNT, false);
  MAP_PASS("Render Time", PASS_RENDER_TIME, false);

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

void BlenderSync::sync_render_passes(blender::RenderLayer &b_rlay,
                                     blender::ViewLayer &b_view_layer)
{
  /* Delete all existing passes. */
  const vector<Pass *> &scene_passes = scene->passes;
  scene->delete_nodes(set<Pass *>(scene_passes.begin(), scene_passes.end()));

  /* Always add combined pass. */
  pass_add(scene, PASS_COMBINED, "Combined");

  /* Cryptomatte stores two blender::ID/weight pairs per RGBA layer.
   * User facing parameter is the number of pairs. */
  const int crypto_depth = divide_up(min(16, b_view_layer.cryptomatte_levels), 2);
  scene->film->set_cryptomatte_depth(crypto_depth);
  CryptomatteType cryptomatte_passes = CRYPT_NONE;
  if ((b_view_layer.cryptomatte_flag & blender::VIEW_LAYER_CRYPTOMATTE_OBJECT) != 0) {
    cryptomatte_passes = (CryptomatteType)(cryptomatte_passes | CRYPT_OBJECT);
  }
  if ((b_view_layer.cryptomatte_flag & blender::VIEW_LAYER_CRYPTOMATTE_MATERIAL) != 0) {
    cryptomatte_passes = (CryptomatteType)(cryptomatte_passes | CRYPT_MATERIAL);
  }
  if ((b_view_layer.cryptomatte_flag & blender::VIEW_LAYER_CRYPTOMATTE_ASSET) != 0) {
    cryptomatte_passes = (CryptomatteType)(cryptomatte_passes | CRYPT_ASSET);
  }
  scene->film->set_cryptomatte_passes(cryptomatte_passes);

  unordered_set<string> expected_passes;

  /* Custom AOV passes. */
  for (blender::ViewLayerAOV &b_aov : b_view_layer.aovs) {
    if ((b_aov.flag & blender::AOV_CONFLICT) != 0) {
      continue;
    }

    const string name = b_aov.name;
    const PassType type = (b_aov.type == blender::AOV_TYPE_COLOR) ? PASS_AOV_COLOR :
                                                                    PASS_AOV_VALUE;

    pass_add(scene, type, name.c_str());
    expected_passes.insert(name);
  }

  /* Light Group passes. */
  for (blender::ViewLayerLightgroup &b_lightgroup : b_view_layer.lightgroups) {
    const string name = string_printf("Combined_%s", b_lightgroup.name);

    Pass *pass = pass_add(scene, PASS_COMBINED, name.c_str(), PassMode::NOISY);
    pass->set_lightgroup(ustring(b_lightgroup.name));
    expected_passes.insert(name);
  }

  /* Sync the passes that were defined in engine.py. */
  for (blender::RenderPass &b_pass : b_rlay.passes) {
    PassType pass_type = PASS_NONE;
    PassMode pass_mode = PassMode::DENOISED;

    if (!get_known_pass_type(b_pass, pass_type, pass_mode)) {
      if (!expected_passes.count(b_pass.name)) {
        LOG_ERROR << "Unknown pass " << b_pass.name;
      }
      continue;
    }

    if (pass_type == PASS_MOTION && ((b_view_layer.layflag & blender::SCE_LAY_MOTION_BLUR) != 0 &&
                                     (b_scene->r.mode & blender::R_MBLUR) != 0))
    {
      continue;
    }

    pass_add(scene, pass_type, b_pass.name, pass_mode);
  }

  scene->film->set_pass_alpha_threshold(b_view_layer.pass_alpha_threshold);
}

void BlenderSync::free_data_after_sync(blender::Depsgraph &b_depsgraph)
{
  /* When viewport display is not needed during render we can force some
   * caches to be releases from blender side in order to reduce peak memory
   * footprint during synchronization process.
   */

  const bool is_interface_locked = engine_render_get(b_engine) &&
                                   engine_render_get(b_engine)->use_lock_interface;
  const bool is_persistent_data = engine_render_get(b_engine) &&
                                  engine_render_get(b_engine)->mode & blender::R_PERSISTENT_DATA;
  const bool can_free_caches =
      (BlenderSession::headless || is_interface_locked) &&
      /* Baking re-uses the depsgraph multiple times, clearing crashes
       * reading un-evaluated mesh data which isn't aligned with the
       * geometry we're baking, see #71012. */
      !b_bake_target &&
      /* Persistent data must main caches for performance and correctness. */
      !is_persistent_data;

  if (!can_free_caches) {
    return;
  }

  /* TODO(sergey): We can actually remove the whole dependency graph,
   * but that will need some API support first.
   */

  blender::DEGObjectIterSettings deg_iter_settings{};
  deg_iter_settings.depsgraph = &b_depsgraph;
  deg_iter_settings.flags = blender::DEG_ITER_OBJECT_FLAG_LINKED_DIRECTLY |
                            blender::DEG_ITER_OBJECT_FLAG_VISIBLE |
                            blender::DEG_ITER_OBJECT_FLAG_LINKED_VIA_SET;
  blender::DEGObjectIterData deg_iter_data{};
  deg_iter_data.settings = &deg_iter_settings;
  deg_iter_data.graph = deg_iter_settings.depsgraph;
  deg_iter_data.flag = deg_iter_settings.flags;

  ITER_BEGIN (blender::DEG_iterator_objects_begin,
              blender::DEG_iterator_objects_next,
              blender::DEG_iterator_objects_end,
              &deg_iter_data,
              blender::Object *,
              b_ob)
  {
    /* Grease pencil render requires all evaluated objects available as-is after Cycles is done
     * with its part. */
    if (b_ob->type == blender::OB_GREASE_PENCIL) {
      continue;
    }
    BKE_object_free_caches(b_ob);
  }
  ITER_END;
}

/* Scene Parameters */

SceneParams BlenderSync::get_scene_params(blender::Scene &b_scene,
                                          const bool background,
                                          const bool use_developer_ui)
{
  SceneParams params;
  blender::PointerRNA scene_rna_ptr = RNA_id_pointer_create(&b_scene.id);
  blender::PointerRNA cscene = RNA_pointer_get(&scene_rna_ptr, "cycles");
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

  blender::PointerRNA csscene = RNA_pointer_get(&scene_rna_ptr, "cycles_curves");
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
  if (texture_limit > 0 && (b_scene.r.mode & blender::R_SIMPLIFY) != 0) {
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

bool BlenderSync::get_session_pause(blender::Scene &b_scene, bool background)
{
  blender::PointerRNA scene_rna_ptr = RNA_id_pointer_create(&b_scene.id);
  blender::PointerRNA cscene = RNA_pointer_get(&scene_rna_ptr, "cycles");
  return (background) ? false : get_boolean(cscene, "preview_pause");
}

SessionParams BlenderSync::get_session_params(blender::RenderEngine &b_engine,
                                              blender::UserDef &b_preferences,
                                              blender::Scene &b_scene,
                                              bool background)
{
  SessionParams params;
  blender::PointerRNA scene_rna_ptr = RNA_id_pointer_create(&b_scene.id);
  blender::PointerRNA cscene = RNA_pointer_get(&scene_rna_ptr, "cycles");

  if (background && (b_engine.flag & blender::RE_ENGINE_PREVIEW) == 0) {
    /* Viewport and preview renders do not require temp directory and do request session
     * parameters more often than the background render.
     * Optimize RNA-C++ usage and memory allocation a bit by saving string access which we know
     * is not needed for viewport render. */
    params.temp_dir = blender::BKE_tempdir_session();
  }

  /* Headless and background rendering. */
  params.headless = BlenderSession::headless;
  params.background = background;

  /* Device */
  params.threads = blender_device_threads(b_scene);
  params.device = blender_device_info(b_preferences,
                                      b_scene,
                                      params.background,
                                      (b_engine.flag & blender::RE_ENGINE_PREVIEW) != 0,
                                      params.denoise_device);

  /* samples */
  const int samples = get_int(cscene, "samples");
  const int preview_samples = get_int(cscene, "preview_samples");
  const bool use_sample_subset = get_boolean(cscene, "use_sample_subset");
  const int sample_subset_offset = get_int(cscene, "sample_offset");
  const int sample_subset_length = get_int(cscene, "sample_subset_length");

  if (background) {
    params.samples = samples;

    params.use_sample_subset = use_sample_subset;
    params.sample_subset_offset = sample_subset_offset;
    params.sample_subset_length = sample_subset_length;
  }
  else {
    params.samples = preview_samples;
    if (params.samples == 0) {
      params.samples = INT_MAX;
    }
    params.use_sample_subset = false;
    params.sample_subset_offset = 0;
    params.sample_subset_length = 0;
  }

  /* Viewport Performance */
  params.pixel_size = BKE_render_preview_pixel_size(&b_scene.r);

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
    /* For the viewport it kind of makes more sense to think in terms of the noise floor, which
     * is usually higher than acceptable level for the final frame. */
    /* TODO: It might be useful to support time limit in the viewport as well, but needs some
     * extra thoughts and input. */
    params.time_limit = 0.0;
  }

  /* Profiling. */
  params.use_profiling = params.device.has_profiling &&
                         (b_engine.flag & blender::RE_ENGINE_PREVIEW) == 0 && background &&
                         BlenderSession::print_render_stats;

  if (background) {
    params.use_auto_tile = true;
    params.tile_size = max(get_int(cscene, "tile_size"), 8);
  }
  else {
    params.use_auto_tile = false;
  }

  return params;
}

DenoiseParams BlenderSync::get_denoise_params(blender::Scene &b_scene,
                                              blender::ViewLayer *b_view_layer,
                                              bool background,
                                              const DeviceInfo &denoise_device_info)
{
  enum DenoiserInput {
    DENOISER_INPUT_RGB = 1,
    DENOISER_INPUT_RGB_ALBEDO = 2,
    DENOISER_INPUT_RGB_ALBEDO_NORMAL = 3,

    DENOISER_INPUT_NUM,
  };

  DenoiseParams denoising;
  blender::PointerRNA scene_rna_ptr = RNA_id_pointer_create(&b_scene.id);
  blender::PointerRNA cscene = RNA_pointer_get(&scene_rna_ptr, "cycles");

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
      blender::PointerRNA view_layer_rna_ptr = RNA_pointer_create_id_subdata(
          b_scene.id, blender::RNA_ViewLayer, b_view_layer);
      blender::PointerRNA clayer = RNA_pointer_get(&view_layer_rna_ptr, "cycles");
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
      denoising.type = Denoiser::automatic_viewport_denoiser_type(denoise_device_info);
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
      LOG_ERROR << "Unhandled input passes enum " << input_passes;
      break;
  }

  return denoising;
}

CCL_NAMESPACE_END
