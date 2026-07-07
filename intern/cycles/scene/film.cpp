/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "scene/film.h"
#include "device/device.h"
#include "scene/background.h"
#include "scene/bake.h"
#include "scene/camera.h"
#include "scene/integrator.h"
#include "scene/mesh.h"
#include "scene/object.h"
#include "scene/scene.h"
#include "scene/stats.h"
#include "scene/tables.h"

#include "util/log.h"
#include "util/math.h"
#include "util/math_cdf.h"
#include "util/time.h"

CCL_NAMESPACE_BEGIN

/* Pixel Filter */

static float filter_func_box(float /*v*/, float /*width*/)
{
  return 1.0f;
}

static float filter_func_gaussian(float v, const float width)
{
  v *= 6.0f / width;
  return expf(-2.0f * v * v);
}

static float filter_func_blackman_harris(float v, const float width)
{
  v = M_2PI_F * (v / width + 0.5f);
  return 0.35875f - 0.48829f * cosf(v) + 0.14128f * cosf(2.0f * v) - 0.01168f * cosf(3.0f * v);
}

static vector<float> filter_table(FilterType type, float width)
{
  vector<float> filter_table(FILTER_TABLE_SIZE);
  float (*filter_func)(float, float) = nullptr;

  switch (type) {
    case FILTER_BOX:
      filter_func = filter_func_box;
      break;
    case FILTER_GAUSSIAN:
      filter_func = filter_func_gaussian;
      width *= 3.0f;
      break;
    case FILTER_BLACKMAN_HARRIS:
      filter_func = filter_func_blackman_harris;
      width *= 2.0f;
      break;
    default:
      assert(0);
  }

  /* Create importance sampling table. */

  /* TODO(sergey): With the even filter table size resolution we can not
   * really make it nice symmetric importance map without sampling full range
   * (meaning, we would need to sample full filter range and not use the
   * make_symmetric argument).
   *
   * Current code matches exactly initial filter table code, but we should
   * consider either making FILTER_TABLE_SIZE odd value or sample full filter.
   */

  util_cdf_inverted(
      FILTER_TABLE_SIZE,
      0.0f,
      width * 0.5f,
      [filter_func, width](const float x) { return filter_func(x, width); },
      true,
      filter_table);

  return filter_table;
}

/* Film */

NODE_DEFINE(Film)
{
  NodeType *type = NodeType::add("film", create);

  SOCKET_FLOAT(exposure, "Exposure", 1.0f);
  SOCKET_FLOAT(pass_alpha_threshold, "Pass Alpha Threshold", 0.0f);

  static NodeEnum filter_enum;
  filter_enum.insert("box", FILTER_BOX);
  filter_enum.insert("gaussian", FILTER_GAUSSIAN);
  filter_enum.insert("blackman_harris", FILTER_BLACKMAN_HARRIS);

  SOCKET_ENUM(filter_type, "Filter Type", filter_enum, FILTER_BOX);
  SOCKET_FLOAT(filter_width, "Filter Width", 1.0f);

  SOCKET_FLOAT(mist_start, "Mist Start", 0.0f);
  SOCKET_FLOAT(mist_depth, "Mist Depth", 100.0f);
  SOCKET_FLOAT(mist_falloff, "Mist Falloff", 1.0f);

  const NodeEnum *pass_type_enum = Pass::get_type_enum();
  SOCKET_ENUM(display_pass, "Display Pass", *pass_type_enum, PASS_COMBINED);

  SOCKET_BOOLEAN(show_active_pixels, "Show Active Pixels", false);

  static NodeEnum cryptomatte_passes_enum;
  cryptomatte_passes_enum.insert("none", CRYPT_NONE);
  cryptomatte_passes_enum.insert("object", CRYPT_OBJECT);
  cryptomatte_passes_enum.insert("material", CRYPT_MATERIAL);
  cryptomatte_passes_enum.insert("asset", CRYPT_ASSET);
  cryptomatte_passes_enum.insert("accurate", CRYPT_ACCURATE);
  SOCKET_ENUM(cryptomatte_passes, "Cryptomatte Passes", cryptomatte_passes_enum, CRYPT_NONE);

  SOCKET_INT(cryptomatte_depth, "Cryptomatte Depth", 0);

  SOCKET_BOOLEAN(use_approximate_shadow_catcher, "Use Approximate Shadow Catcher", false);

  SOCKET_BOOLEAN(use_sample_count, "Use Sample Count Pass", false);

  return type;
}

Film::Film() : Node(get_node_type()), filter_table_offset_(TABLE_OFFSET_INVALID) {}

Film::~Film() = default;

void Film::add_default(Scene *scene)
{
  Pass *pass = scene->create_node<Pass>();
  pass->set_type(PASS_COMBINED);
}

void Film::device_update(Device *device, DeviceScene *dscene, Scene *scene)
{
  if (!is_modified()) {
    return;
  }

  const scoped_callback_timer timer([scene](double time) {
    if (scene->update_stats) {
      scene->update_stats->film.times.add_entry({"update", time});
    }
  });

  device_free(device, dscene, scene);

  KernelFilm *kfilm = &dscene->data.film;

  /* update data */
  kfilm->exposure = exposure;
  kfilm->pass_alpha_threshold = pass_alpha_threshold;
  kfilm->pass_flag = 0;

  kfilm->use_approximate_shadow_catcher = get_use_approximate_shadow_catcher();

  kfilm->light_pass_flag = 0;
  kfilm->pass_stride = 0;

  /* Mark with PASS_UNUSED to avoid mask test in the kernel. */
  kfilm->pass_combined = PASS_UNUSED;
  kfilm->pass_depth = PASS_UNUSED;
  kfilm->pass_position = PASS_UNUSED;
  kfilm->pass_normal = PASS_UNUSED;
  kfilm->pass_roughness = PASS_UNUSED;
  kfilm->pass_motion = PASS_UNUSED;
  kfilm->pass_motion_weight = PASS_UNUSED;
  kfilm->pass_uv = PASS_UNUSED;
  kfilm->pass_object_id = PASS_UNUSED;
  kfilm->pass_material_id = PASS_UNUSED;
  kfilm->pass_diffuse_color = PASS_UNUSED;
  kfilm->pass_glossy_color = PASS_UNUSED;
  kfilm->pass_transmission_color = PASS_UNUSED;
  kfilm->pass_background = PASS_UNUSED;
  kfilm->pass_emission = PASS_UNUSED;
  kfilm->pass_ao = PASS_UNUSED;
  kfilm->pass_diffuse_direct = PASS_UNUSED;
  kfilm->pass_diffuse_indirect = PASS_UNUSED;
  kfilm->pass_glossy_direct = PASS_UNUSED;
  kfilm->pass_glossy_indirect = PASS_UNUSED;
  kfilm->pass_transmission_direct = PASS_UNUSED;
  kfilm->pass_transmission_indirect = PASS_UNUSED;
  kfilm->pass_volume_direct = PASS_UNUSED;
  kfilm->pass_volume_indirect = PASS_UNUSED;
  kfilm->pass_volume_scatter = PASS_UNUSED;
  kfilm->pass_volume_transmit = PASS_UNUSED;
  kfilm->pass_volume_scatter_denoised = PASS_UNUSED;
  kfilm->pass_volume_transmit_denoised = PASS_UNUSED;
  kfilm->pass_volume_majorant = PASS_UNUSED;
  kfilm->pass_lightgroup = PASS_UNUSED;

  /* Mark passes as unused so that the kernel knows the pass is inaccessible. */
  kfilm->pass_denoising_normal = PASS_UNUSED;
  kfilm->pass_denoising_albedo = PASS_UNUSED;
  kfilm->pass_denoising_depth = PASS_UNUSED;
  kfilm->pass_sample_count = PASS_UNUSED;
  kfilm->pass_render_time = PASS_UNUSED;
  kfilm->pass_adaptive_aux_buffer = PASS_UNUSED;
  kfilm->pass_shadow_catcher = PASS_UNUSED;
  kfilm->pass_shadow_catcher_sample_count = PASS_UNUSED;
  kfilm->pass_shadow_catcher_matte = PASS_UNUSED;

  kfilm->pass_guiding_color = PASS_UNUSED;
  kfilm->pass_guiding_probability = PASS_UNUSED;
  kfilm->pass_guiding_avg_roughness = PASS_UNUSED;

  bool have_cryptomatte = false;
  bool have_aov_color = false;
  bool have_aov_value = false;
  bool have_lightgroup = false;

  for (size_t i = 0; i < scene->passes.size(); i++) {
    const Pass *pass = scene->passes[i];

    if (pass->get_type() == PASS_NONE || !pass->is_written()) {
      continue;
    }

    if (pass->get_mode() == PassMode::DENOISED) {
      /* Generally we only storing offsets of the noisy passes. The display pass is an exception
       * since it is a read operation and not a write. */
      if (pass->get_type() == PASS_VOLUME_TRANSMIT) {
        kfilm->pass_volume_transmit_denoised = kfilm->pass_stride;
      }
      else if (pass->get_type() == PASS_VOLUME_SCATTER) {
        kfilm->pass_volume_scatter_denoised = kfilm->pass_stride;
      }
      kfilm->pass_stride += pass->get_info().num_components;
      continue;
    }

    /* Can't do motion pass if no motion vectors are available. */
    if (pass->get_type() == PASS_MOTION || pass->get_type() == PASS_MOTION_WEIGHT) {
      if (scene->need_motion() != Scene::MOTION_PASS) {
        kfilm->pass_stride += pass->get_info().num_components;
        continue;
      }
    }

    const int pass_flag = (1 << (pass->get_type() % 32));
    if (pass->get_type() <= PASS_CATEGORY_LIGHT_END) {
      kfilm->light_pass_flag |= pass_flag;
    }
    else if (pass->get_type() <= PASS_CATEGORY_DATA_END) {
      kfilm->pass_flag |= pass_flag;
    }
    else {
      assert(pass->get_type() <= PASS_CATEGORY_BAKE_END);
    }

    if (!pass->get_lightgroup().empty()) {
      if (!have_lightgroup) {
        kfilm->pass_lightgroup = kfilm->pass_stride;
        have_lightgroup = true;
      }
      kfilm->pass_stride += pass->get_info().num_components;
      continue;
    }

    switch (pass->get_type()) {
      case PASS_COMBINED:
        kfilm->pass_combined = kfilm->pass_stride;
        break;
      case PASS_DEPTH:
        kfilm->pass_depth = kfilm->pass_stride;
        break;
      case PASS_NORMAL:
        kfilm->pass_normal = kfilm->pass_stride;
        break;
      case PASS_POSITION:
        kfilm->pass_position = kfilm->pass_stride;
        break;
      case PASS_ROUGHNESS:
        kfilm->pass_roughness = kfilm->pass_stride;
        break;
      case PASS_UV:
        kfilm->pass_uv = kfilm->pass_stride;
        break;
      case PASS_MOTION:
        kfilm->pass_motion = kfilm->pass_stride;
        break;
      case PASS_MOTION_WEIGHT:
        kfilm->pass_motion_weight = kfilm->pass_stride;
        break;
      case PASS_OBJECT_ID:
        kfilm->pass_object_id = kfilm->pass_stride;
        break;
      case PASS_MATERIAL_ID:
        kfilm->pass_material_id = kfilm->pass_stride;
        break;

      case PASS_MIST:
        kfilm->pass_mist = kfilm->pass_stride;
        break;
      case PASS_EMISSION:
        kfilm->pass_emission = kfilm->pass_stride;
        break;
      case PASS_BACKGROUND:
        kfilm->pass_background = kfilm->pass_stride;
        break;
      case PASS_AO:
        kfilm->pass_ao = kfilm->pass_stride;
        break;

      case PASS_DIFFUSE_COLOR:
        kfilm->pass_diffuse_color = kfilm->pass_stride;
        break;
      case PASS_GLOSSY_COLOR:
        kfilm->pass_glossy_color = kfilm->pass_stride;
        break;
      case PASS_TRANSMISSION_COLOR:
        kfilm->pass_transmission_color = kfilm->pass_stride;
        break;
      case PASS_DIFFUSE_INDIRECT:
        kfilm->pass_diffuse_indirect = kfilm->pass_stride;
        break;
      case PASS_GLOSSY_INDIRECT:
        kfilm->pass_glossy_indirect = kfilm->pass_stride;
        break;
      case PASS_TRANSMISSION_INDIRECT:
        kfilm->pass_transmission_indirect = kfilm->pass_stride;
        break;
      case PASS_VOLUME_INDIRECT:
        kfilm->pass_volume_indirect = kfilm->pass_stride;
        break;
      case PASS_DIFFUSE_DIRECT:
        kfilm->pass_diffuse_direct = kfilm->pass_stride;
        break;
      case PASS_GLOSSY_DIRECT:
        kfilm->pass_glossy_direct = kfilm->pass_stride;
        break;
      case PASS_TRANSMISSION_DIRECT:
        kfilm->pass_transmission_direct = kfilm->pass_stride;
        break;
      case PASS_VOLUME_DIRECT:
        kfilm->pass_volume_direct = kfilm->pass_stride;
        break;
      case PASS_VOLUME_SCATTER:
        kfilm->pass_volume_scatter = kfilm->pass_stride;
        break;
      case PASS_VOLUME_TRANSMIT:
        kfilm->pass_volume_transmit = kfilm->pass_stride;
        break;
      case PASS_VOLUME_MAJORANT:
        kfilm->pass_volume_majorant = kfilm->pass_stride;
        break;
      case PASS_VOLUME_MAJORANT_SAMPLE_COUNT:
        kfilm->pass_volume_majorant_sample_count = kfilm->pass_stride;
        break;

      case PASS_BAKE_PRIMITIVE:
        kfilm->pass_bake_primitive = kfilm->pass_stride;
        break;
      case PASS_BAKE_SEED:
        kfilm->pass_bake_seed = kfilm->pass_stride;
        break;
      case PASS_BAKE_DIFFERENTIAL:
        kfilm->pass_bake_differential = kfilm->pass_stride;
        break;

      case PASS_CRYPTOMATTE:
        kfilm->pass_cryptomatte = have_cryptomatte ?
                                      min(kfilm->pass_cryptomatte, kfilm->pass_stride) :
                                      kfilm->pass_stride;
        have_cryptomatte = true;
        break;

      case PASS_DENOISING_NORMAL:
        kfilm->pass_denoising_normal = kfilm->pass_stride;
        break;
      case PASS_DENOISING_ALBEDO:
        kfilm->pass_denoising_albedo = kfilm->pass_stride;
        break;
      case PASS_DENOISING_DEPTH:
        kfilm->pass_denoising_depth = kfilm->pass_stride;
        break;

      case PASS_SHADOW_CATCHER:
        kfilm->pass_shadow_catcher = kfilm->pass_stride;
        break;
      case PASS_SHADOW_CATCHER_SAMPLE_COUNT:
        kfilm->pass_shadow_catcher_sample_count = kfilm->pass_stride;
        break;
      case PASS_SHADOW_CATCHER_MATTE:
        kfilm->pass_shadow_catcher_matte = kfilm->pass_stride;
        break;

      case PASS_ADAPTIVE_AUX_BUFFER:
        kfilm->pass_adaptive_aux_buffer = kfilm->pass_stride;
        break;
      case PASS_SAMPLE_COUNT:
        kfilm->pass_sample_count = kfilm->pass_stride;
        break;
      case PASS_RENDER_TIME:
        kfilm->pass_render_time = kfilm->pass_stride;
        break;

      case PASS_AOV_COLOR:
        if (!have_aov_color) {
          kfilm->pass_aov_color = kfilm->pass_stride;
          have_aov_color = true;
        }
        break;
      case PASS_AOV_VALUE:
        if (!have_aov_value) {
          kfilm->pass_aov_value = kfilm->pass_stride;
          have_aov_value = true;
        }
        break;
      case PASS_GUIDING_COLOR:
        kfilm->pass_guiding_color = kfilm->pass_stride;
        break;
      case PASS_GUIDING_PROBABILITY:
        kfilm->pass_guiding_probability = kfilm->pass_stride;
        break;
      case PASS_GUIDING_AVG_ROUGHNESS:
        kfilm->pass_guiding_avg_roughness = kfilm->pass_stride;
        break;
      default:
        assert(false);
        break;
    }

    kfilm->pass_stride += pass->get_info().num_components;
  }

  /* update filter table */
  vector<float> table = filter_table(filter_type, filter_width);
  scene->lookup_tables->remove_table(&filter_table_offset_);
  filter_table_offset_ = scene->lookup_tables->add_table(dscene, table);
  dscene->data.tables.filter_table_offset = (int)filter_table_offset_;

  /* mist pass parameters */
  kfilm->mist_start = mist_start;
  kfilm->mist_inv_depth = (mist_depth > 0.0f) ? 1.0f / mist_depth : 0.0f;
  kfilm->mist_falloff = mist_falloff;

  kfilm->cryptomatte_passes = cryptomatte_passes;
  kfilm->cryptomatte_depth = cryptomatte_depth;

  clear_modified();
}

void Film::device_free(Device * /*device*/, DeviceScene * /*dscene*/, Scene *scene)
{
  scene->lookup_tables->remove_table(&filter_table_offset_);
}

int Film::get_aov_offset(Scene *scene, string name, bool &is_color)
{
  int offset_color = 0;
  int offset_value = 0;
  for (const Pass *pass : scene->passes) {
    if (pass->get_name() == name) {
      if (pass->get_type() == PASS_AOV_VALUE) {
        is_color = false;
        return offset_value;
      }
      if (pass->get_type() == PASS_AOV_COLOR) {
        is_color = true;
        return offset_color;
      }
    }

    if (pass->get_type() == PASS_AOV_VALUE) {
      offset_value += pass->get_info().num_components;
    }
    else if (pass->get_type() == PASS_AOV_COLOR) {
      offset_color += pass->get_info().num_components;
    }
  }

  return -1;
}

bool Film::update_lightgroups(Scene *scene)
{
  map<ustring, int> lightgroups;
  int i = 0;
  for (const Pass *pass : scene->passes) {
    const ustring lightgroup = pass->get_lightgroup();
    if (!lightgroup.empty()) {
      if (!lightgroups.count(lightgroup)) {
        lightgroups[lightgroup] = i++;
      }
    }
  }
  if (scene->lightgroups != lightgroups) {
    scene->lightgroups = lightgroups;
    return true;
  }

  return false;
}

void Film::update_passes(Scene *scene)
{
  const Background *background = scene->background;
  const BakeManager *bake_manager = scene->bake_manager.get();
  const ObjectManager *object_manager = scene->object_manager.get();
  Integrator *integrator = scene->integrator;

  if (!is_modified() && !object_manager->need_update() && !integrator->is_modified() &&
      !background->is_modified() && !scene->has_volume_modified())
  {
    return;
  }

  /* Remove auto generated passes and recreate them. */
  remove_auto_passes(scene);

  /* Display pass for viewport. */
  const PassType display_pass = get_display_pass();
  add_auto_pass(scene, display_pass);

  /* Assumption is that a combined pass always exists for now, for example
   * adaptive sampling is always based on a combined pass. But we should
   * try to lift this limitation in the future for faster rendering of
   * individual passes. */
  if (display_pass != PASS_COMBINED) {
    add_auto_pass(scene, PASS_COMBINED);
  }

  /* Create passes needed for adaptive sampling. */
  const AdaptiveSampling adaptive_sampling = integrator->get_adaptive_sampling();
  if (adaptive_sampling.use) {
    add_auto_pass(scene, PASS_SAMPLE_COUNT);
    add_auto_pass(scene, PASS_ADAPTIVE_AUX_BUFFER);
  }

  /* Create passes needed for denoising. */
  const bool use_denoise = integrator->get_use_denoise();
  if (use_denoise) {
    if (integrator->get_use_denoise_pass_normal()) {
      add_auto_pass(scene, PASS_DENOISING_NORMAL);
    }
    if (integrator->get_use_denoise_pass_albedo()) {
      add_auto_pass(scene, PASS_DENOISING_ALBEDO);
    }
  }

  /* Create passes for shadow catcher. */
  if (scene->has_shadow_catcher()) {
    const bool need_background = get_use_approximate_shadow_catcher() &&
                                 !background->get_transparent();

    add_auto_pass(scene, PASS_SHADOW_CATCHER);
    add_auto_pass(scene, PASS_SHADOW_CATCHER_SAMPLE_COUNT);
    add_auto_pass(scene, PASS_SHADOW_CATCHER_MATTE);

    if (need_background) {
      add_auto_pass(scene, PASS_BACKGROUND);
    }
  }
  else if (Pass::contains(scene->passes, PASS_SHADOW_CATCHER)) {
    add_auto_pass(scene, PASS_SHADOW_CATCHER);
    add_auto_pass(scene, PASS_SHADOW_CATCHER_SAMPLE_COUNT);
  }

  const vector<Pass *> passes_immutable = scene->passes;
  for (const Pass *pass : passes_immutable) {
    const PassInfo info = pass->get_info();
    /* Add utility passes needed to generate some light passes. */
    if (info.divide_type != PASS_NONE) {
      add_auto_pass(scene, info.divide_type);
    }
    if (info.direct_type != PASS_NONE) {
      add_auto_pass(scene, info.direct_type);
    }
    if (info.indirect_type != PASS_NONE) {
      add_auto_pass(scene, info.indirect_type);
    }

    /* NOTE: Enable all denoised passes when storage is requested.
     * This way it is possible to tweak denoiser parameters later on. */
    if (info.support_denoise && use_denoise) {
      add_auto_pass(scene, pass->get_type(), PassMode::DENOISED);
    }
  }

  if (bake_manager->get_baking()) {
    add_auto_pass(scene, PASS_BAKE_PRIMITIVE, "BakePrimitive");
    add_auto_pass(scene, PASS_BAKE_DIFFERENTIAL, "BakeDifferential");
    if (bake_manager->get_use_seed()) {
      add_auto_pass(scene, PASS_BAKE_SEED, "BakeSeed");
    }
  }

  /* Add sample count pass for tiled rendering. */
  if (use_sample_count) {
    if (!Pass::contains(scene->passes, PASS_SAMPLE_COUNT)) {
      add_auto_pass(scene, PASS_SAMPLE_COUNT);
    }
  }

  if (scene->has_volume()) {
    add_auto_pass(scene, PASS_VOLUME_SCATTER);
    add_auto_pass(scene, PASS_VOLUME_SCATTER, PassMode::DENOISED, "Volume Scatter");
    add_auto_pass(scene, PASS_VOLUME_TRANSMIT);
    add_auto_pass(scene, PASS_VOLUME_TRANSMIT, PassMode::DENOISED, "Volume Transmit");
    if (!Pass::contains(scene->passes, PASS_SAMPLE_COUNT)) {
      add_auto_pass(scene, PASS_SAMPLE_COUNT);
    }
    if (!Pass::contains(scene->passes, PASS_VOLUME_MAJORANT)) {
      add_auto_pass(scene, PASS_VOLUME_MAJORANT, "Volume Majorant");
    }
    add_auto_pass(scene, PASS_VOLUME_MAJORANT_SAMPLE_COUNT);
  }

  /* Remove duplicates and initialize internal pass info. */
  finalize_passes(scene, use_denoise);

  /* Flush scene updates. */
  const bool have_uv_pass = Pass::contains(scene->passes, PASS_UV);
  const bool have_motion_pass = Pass::contains(scene->passes, PASS_MOTION);
  const bool have_ao_pass = Pass::contains(scene->passes, PASS_AO);

  if (have_uv_pass != prev_have_uv_pass) {
    scene->geometry_manager->tag_update(scene, GeometryManager::UV_PASS_NEEDED);
    for (Shader *shader : scene->shaders) {
      shader->need_update_uvs = true;
    }
  }
  if (have_motion_pass != prev_have_motion_pass) {
    scene->geometry_manager->tag_update(scene, GeometryManager::MOTION_PASS_NEEDED);
  }
  if (have_ao_pass != prev_have_ao_pass) {
    scene->integrator->tag_update(scene, Integrator::AO_PASS_MODIFIED);
  }

  prev_have_uv_pass = have_uv_pass;
  prev_have_motion_pass = have_motion_pass;
  prev_have_ao_pass = have_ao_pass;

  tag_modified();

  /* Debug logging. */
  if (LOG_IS_ON(LOG_LEVEL_INFO)) {
    LOG_INFO << "Effective scene passes:";
    for (const Pass *pass : scene->passes) {
      LOG_INFO << "- " << *pass;
    }
  }
}

void Film::add_auto_pass(Scene *scene, PassType type, const char *name)
{
  add_auto_pass(scene, type, PassMode::NOISY, name);
}

void Film::add_auto_pass(Scene *scene, PassType type, PassMode mode, const char *name)
{
  unique_ptr<Pass> pass = make_unique<Pass>();
  pass->set_type(type);
  pass->set_mode(mode);
  pass->set_name(ustring((name) ? name : ""));
  pass->is_auto_ = true;

  pass->set_owner(scene);
  scene->passes.push_back(std::move(pass));
}

void Film::remove_auto_passes(Scene *scene)
{
  /* Remove all passes which were automatically created. */
  unique_ptr_vector<Pass> new_passes;

  for (size_t i = 0; i < scene->passes.size(); i++) {
    unique_ptr<Pass> pass = scene->passes.steal(i);
    if (!pass->is_auto_) {
      new_passes.push_back(std::move(pass));
    }
  }

  scene->passes = std::move(new_passes);
}

static bool compare_pass_order(const Pass *a, const Pass *b)
{
  /* On the highest level, sort by number of components.
   * Within passes of the same component count, sort so that all non-lightgroup passes come first.
   * Within that group, sort by type. */
  const int num_components_a = a->get_info().num_components;
  const int num_components_b = b->get_info().num_components;

  if (num_components_a == num_components_b) {
    const int is_lightgroup_a = !a->get_lightgroup().empty();
    const int is_lightgroup_b = !b->get_lightgroup().empty();
    if (is_lightgroup_a == is_lightgroup_b) {
      return (a->get_type() < b->get_type());
    }
    return is_lightgroup_b;
  }

  return num_components_a > num_components_b;
}

void Film::finalize_passes(Scene *scene, const bool use_denoise)
{
  /* Remove duplicate passes. */
  unique_ptr_vector<Pass> new_passes;

  for (size_t i = 0; i < scene->passes.size(); i++) {
    unique_ptr<Pass> pass = scene->passes.steal(i);

    /* Disable denoising on passes if denoising is disabled, or if the
     * pass does not support it. */
    const bool need_denoise = pass->get_info().support_denoise &&
                              (use_denoise || is_volume_guiding_pass(pass->get_type()));
    pass->set_mode(need_denoise ? pass->get_mode() : PassMode::NOISY);

    /* Merge duplicate passes. */
    bool duplicate_found = false;
    for (Pass *new_pass : new_passes) {
      /* If different type or denoising, don't merge. */
      if (new_pass->get_type() != pass->get_type() || new_pass->get_mode() != pass->get_mode()) {
        continue;
      }

      /* If both passes have a name and the names are different, don't merge.
       * If either pass has a name, we'll use that name. */
      if (!pass->get_name().empty() && !new_pass->get_name().empty() &&
          pass->get_name() != new_pass->get_name())
      {
        continue;
      }

      if (!pass->get_name().empty() && new_pass->get_name().empty()) {
        new_pass->set_name(pass->get_name());
      }

      new_pass->is_auto_ &= pass->is_auto_;
      duplicate_found = true;
      break;
    }

    if (!duplicate_found) {
      new_passes.push_back(std::move(pass));
    }
  }

  /* Order from by components and type, This is required for AOVs and cryptomatte passes,
   * which the kernel assumes to be in order. Note this must use stable sort so cryptomatte
   * passes remain in the right order. */
  new_passes.stable_sort(compare_pass_order);

  scene->passes = std::move(new_passes);
}

uint Film::get_kernel_features(const Scene *scene) const
{
  uint kernel_features = 0;

  for (const Pass *pass : scene->passes) {
    if (!pass->is_written()) {
      continue;
    }

    const PassType pass_type = pass->get_type();
    const PassMode pass_mode = pass->get_mode();

    const bool has_denoise_pass = (pass_mode == PassMode::DENOISED) &&
                                  !is_volume_guiding_pass(pass_type);

    if (has_denoise_pass || pass_type == PASS_DENOISING_NORMAL ||
        pass_type == PASS_DENOISING_ALBEDO || pass_type == PASS_DENOISING_DEPTH)
    {
      kernel_features |= KERNEL_FEATURE_DENOISING;
    }

    if (pass_type >= PASS_DIFFUSE && pass_type <= PASS_VOLUME_TRANSMIT) {
      kernel_features |= KERNEL_FEATURE_LIGHT_PASSES;
    }

    if (pass_type == PASS_AO) {
      kernel_features |= KERNEL_FEATURE_AO_PASS;
    }
  }

  return kernel_features;
}

CCL_NAMESPACE_END
