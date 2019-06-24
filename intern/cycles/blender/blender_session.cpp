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

#include <stdlib.h>

#include "device/device.h"
#include "render/background.h"
#include "render/buffers.h"
#include "render/camera.h"
#include "render/colorspace.h"
#include "render/film.h"
#include "render/integrator.h"
#include "render/light.h"
#include "render/mesh.h"
#include "render/object.h"
#include "render/scene.h"
#include "render/session.h"
#include "render/shader.h"
#include "render/stats.h"

#include "util/util_algorithm.h"
#include "util/util_color.h"
#include "util/util_foreach.h"
#include "util/util_function.h"
#include "util/util_hash.h"
#include "util/util_logging.h"
#include "util/util_murmurhash.h"
#include "util/util_progress.h"
#include "util/util_time.h"

#include "blender/blender_sync.h"
#include "blender/blender_session.h"
#include "blender/blender_util.h"

CCL_NAMESPACE_BEGIN

bool BlenderSession::headless = false;
int BlenderSession::num_resumable_chunks = 0;
int BlenderSession::current_resumable_chunk = 0;
int BlenderSession::start_resumable_chunk = 0;
int BlenderSession::end_resumable_chunk = 0;
bool BlenderSession::print_render_stats = false;

BlenderSession::BlenderSession(BL::RenderEngine &b_engine,
                               BL::Preferences &b_userpref,
                               BL::BlendData &b_data,
                               bool preview_osl)
    : session(NULL),
      sync(NULL),
      b_engine(b_engine),
      b_userpref(b_userpref),
      b_data(b_data),
      b_render(b_engine.render()),
      b_depsgraph(PointerRNA_NULL),
      b_scene(PointerRNA_NULL),
      b_v3d(PointerRNA_NULL),
      b_rv3d(PointerRNA_NULL),
      width(0),
      height(0),
      preview_osl(preview_osl),
      python_thread_state(NULL)
{
  /* offline render */
  background = true;
  last_redraw_time = 0.0;
  start_resize_time = 0.0;
  last_status_time = 0.0;
}

BlenderSession::BlenderSession(BL::RenderEngine &b_engine,
                               BL::Preferences &b_userpref,
                               BL::BlendData &b_data,
                               BL::SpaceView3D &b_v3d,
                               BL::RegionView3D &b_rv3d,
                               int width,
                               int height)
    : session(NULL),
      sync(NULL),
      b_engine(b_engine),
      b_userpref(b_userpref),
      b_data(b_data),
      b_render(b_engine.render()),
      b_depsgraph(PointerRNA_NULL),
      b_scene(PointerRNA_NULL),
      b_v3d(b_v3d),
      b_rv3d(b_rv3d),
      width(width),
      height(height),
      preview_osl(false),
      python_thread_state(NULL)
{
  /* 3d view render */
  background = false;
  last_redraw_time = 0.0;
  start_resize_time = 0.0;
  last_status_time = 0.0;
}

BlenderSession::~BlenderSession()
{
  free_session();
}

void BlenderSession::create()
{
  create_session();
}

void BlenderSession::create_session()
{
  SessionParams session_params = BlenderSync::get_session_params(
      b_engine, b_userpref, b_scene, background);
  SceneParams scene_params = BlenderSync::get_scene_params(b_scene, background);
  bool session_pause = BlenderSync::get_session_pause(b_scene, background);

  /* reset status/progress */
  last_status = "";
  last_error = "";
  last_progress = -1.0f;
  start_resize_time = 0.0;

  /* create session */
  session = new Session(session_params);
  session->scene = scene;
  session->progress.set_update_callback(function_bind(&BlenderSession::tag_redraw, this));
  session->progress.set_cancel_callback(function_bind(&BlenderSession::test_cancel, this));
  session->set_pause(session_pause);

  /* create scene */
  scene = new Scene(scene_params, session->device);
  scene->name = b_scene.name();

  /* setup callbacks for builtin image support */
  scene->image_manager->builtin_image_info_cb = function_bind(
      &BlenderSession::builtin_image_info, this, _1, _2, _3);
  scene->image_manager->builtin_image_pixels_cb = function_bind(
      &BlenderSession::builtin_image_pixels, this, _1, _2, _3, _4, _5, _6);
  scene->image_manager->builtin_image_float_pixels_cb = function_bind(
      &BlenderSession::builtin_image_float_pixels, this, _1, _2, _3, _4, _5, _6);

  session->scene = scene;

  /* There is no single depsgraph to use for the entire render.
   * So we need to handle this differently.
   *
   * We could loop over the final render result render layers in pipeline and keep Cycles unaware
   * of multiple layers, or perhaps move syncing further down in the pipeline.
   */
  /* create sync */
  sync = new BlenderSync(b_engine, b_data, b_scene, scene, !background, session->progress);
  BL::Object b_camera_override(b_engine.camera_override());
  if (b_v3d) {
    sync->sync_view(b_v3d, b_rv3d, width, height);
  }
  else {
    sync->sync_camera(b_render, b_camera_override, width, height, "");
  }

  /* set buffer parameters */
  BufferParams buffer_params = BlenderSync::get_buffer_params(
      b_render, b_v3d, b_rv3d, scene->camera, width, height);
  session->reset(buffer_params, session_params.samples);

  b_engine.use_highlight_tiles(session_params.progressive_refine == false);

  update_resumable_tile_manager(session_params.samples);
}

void BlenderSession::reset_session(BL::BlendData &b_data, BL::Depsgraph &b_depsgraph)
{
  this->b_data = b_data;
  this->b_depsgraph = b_depsgraph;
  this->b_scene = b_depsgraph.scene_eval();

  if (preview_osl) {
    PointerRNA cscene = RNA_pointer_get(&b_scene.ptr, "cycles");
    RNA_boolean_set(&cscene, "shading_system", preview_osl);
  }

  if (b_v3d) {
    this->b_render = b_scene.render();
  }
  else {
    this->b_render = b_engine.render();
    width = render_resolution_x(b_render);
    height = render_resolution_y(b_render);
  }

  if (session == NULL) {
    create();
  }

  if (b_v3d) {
    /* NOTE: We need to create session, but all the code from below
     * will make viewport render to stuck on initialization.
     */
    return;
  }

  SessionParams session_params = BlenderSync::get_session_params(
      b_engine, b_userpref, b_scene, background);
  SceneParams scene_params = BlenderSync::get_scene_params(b_scene, background);

  if (scene->params.modified(scene_params) || session->params.modified(session_params) ||
      !scene_params.persistent_data) {
    /* if scene or session parameters changed, it's easier to simply re-create
     * them rather than trying to distinguish which settings need to be updated
     */
    free_session();
    create_session();
    return;
  }

  session->progress.reset();
  scene->reset();

  session->tile_manager.set_tile_order(session_params.tile_order);

  /* peak memory usage should show current render peak, not peak for all renders
   * made by this render session
   */
  session->stats.mem_peak = session->stats.mem_used;

  /* There is no single depsgraph to use for the entire render.
   * See note on create_session().
   */
  /* sync object should be re-created */
  sync = new BlenderSync(b_engine, b_data, b_scene, scene, !background, session->progress);

  BL::SpaceView3D b_null_space_view3d(PointerRNA_NULL);
  BL::RegionView3D b_null_region_view3d(PointerRNA_NULL);
  BufferParams buffer_params = BlenderSync::get_buffer_params(
      b_render, b_null_space_view3d, b_null_region_view3d, scene->camera, width, height);
  session->reset(buffer_params, session_params.samples);

  b_engine.use_highlight_tiles(session_params.progressive_refine == false);

  /* reset time */
  start_resize_time = 0.0;
}

void BlenderSession::free_session()
{
  if (sync)
    delete sync;

  delete session;
}

static ShaderEvalType get_shader_type(const string &pass_type)
{
  const char *shader_type = pass_type.c_str();

  /* data passes */
  if (strcmp(shader_type, "NORMAL") == 0)
    return SHADER_EVAL_NORMAL;
  else if (strcmp(shader_type, "UV") == 0)
    return SHADER_EVAL_UV;
  else if (strcmp(shader_type, "ROUGHNESS") == 0)
    return SHADER_EVAL_ROUGHNESS;
  else if (strcmp(shader_type, "DIFFUSE_COLOR") == 0)
    return SHADER_EVAL_DIFFUSE_COLOR;
  else if (strcmp(shader_type, "GLOSSY_COLOR") == 0)
    return SHADER_EVAL_GLOSSY_COLOR;
  else if (strcmp(shader_type, "TRANSMISSION_COLOR") == 0)
    return SHADER_EVAL_TRANSMISSION_COLOR;
  else if (strcmp(shader_type, "SUBSURFACE_COLOR") == 0)
    return SHADER_EVAL_SUBSURFACE_COLOR;
  else if (strcmp(shader_type, "EMIT") == 0)
    return SHADER_EVAL_EMISSION;

  /* light passes */
  else if (strcmp(shader_type, "AO") == 0)
    return SHADER_EVAL_AO;
  else if (strcmp(shader_type, "COMBINED") == 0)
    return SHADER_EVAL_COMBINED;
  else if (strcmp(shader_type, "SHADOW") == 0)
    return SHADER_EVAL_SHADOW;
  else if (strcmp(shader_type, "DIFFUSE") == 0)
    return SHADER_EVAL_DIFFUSE;
  else if (strcmp(shader_type, "GLOSSY") == 0)
    return SHADER_EVAL_GLOSSY;
  else if (strcmp(shader_type, "TRANSMISSION") == 0)
    return SHADER_EVAL_TRANSMISSION;
  else if (strcmp(shader_type, "SUBSURFACE") == 0)
    return SHADER_EVAL_SUBSURFACE;

  /* extra */
  else if (strcmp(shader_type, "ENVIRONMENT") == 0)
    return SHADER_EVAL_ENVIRONMENT;

  else
    return SHADER_EVAL_BAKE;
}

static BL::RenderResult begin_render_result(BL::RenderEngine &b_engine,
                                            int x,
                                            int y,
                                            int w,
                                            int h,
                                            const char *layername,
                                            const char *viewname)
{
  return b_engine.begin_result(x, y, w, h, layername, viewname);
}

static void end_render_result(BL::RenderEngine &b_engine,
                              BL::RenderResult &b_rr,
                              bool cancel,
                              bool highlight,
                              bool do_merge_results)
{
  b_engine.end_result(b_rr, (int)cancel, (int)highlight, (int)do_merge_results);
}

void BlenderSession::do_write_update_render_tile(RenderTile &rtile,
                                                 bool do_update_only,
                                                 bool highlight)
{
  int x = rtile.x - session->tile_manager.params.full_x;
  int y = rtile.y - session->tile_manager.params.full_y;
  int w = rtile.w;
  int h = rtile.h;

  /* get render result */
  BL::RenderResult b_rr = begin_render_result(
      b_engine, x, y, w, h, b_rlay_name.c_str(), b_rview_name.c_str());

  /* can happen if the intersected rectangle gives 0 width or height */
  if (b_rr.ptr.data == NULL) {
    return;
  }

  BL::RenderResult::layers_iterator b_single_rlay;
  b_rr.layers.begin(b_single_rlay);

  /* layer will be missing if it was disabled in the UI */
  if (b_single_rlay == b_rr.layers.end())
    return;

  BL::RenderLayer b_rlay = *b_single_rlay;

  if (do_update_only) {
    /* Sample would be zero at initial tile update, which is only needed
     * to tag tile form blender side as IN PROGRESS for proper highlight
     * no buffers should be sent to blender yet. For denoise we also
     * keep showing the noisy buffers until denoise is done. */
    bool merge = (rtile.sample != 0) && (rtile.task != RenderTile::DENOISE);

    if (merge) {
      update_render_result(b_rlay, rtile);
    }

    end_render_result(b_engine, b_rr, true, highlight, merge);
  }
  else {
    /* Write final render result. */
    write_render_result(b_rlay, rtile);
    end_render_result(b_engine, b_rr, false, false, true);
  }
}

void BlenderSession::write_render_tile(RenderTile &rtile)
{
  do_write_update_render_tile(rtile, false, false);
}

void BlenderSession::update_render_tile(RenderTile &rtile, bool highlight)
{
  /* use final write for preview renders, otherwise render result wouldn't be
   * be updated in blender side
   * would need to be investigated a bit further, but for now shall be fine
   */
  if (!b_engine.is_preview())
    do_write_update_render_tile(rtile, true, highlight);
  else
    do_write_update_render_tile(rtile, false, false);
}

static void add_cryptomatte_layer(BL::RenderResult &b_rr, string name, string manifest)
{
  string identifier = string_printf("%08x", util_murmur_hash3(name.c_str(), name.length(), 0));
  string prefix = "cryptomatte/" + identifier.substr(0, 7) + "/";

  render_add_metadata(b_rr, prefix + "name", name);
  render_add_metadata(b_rr, prefix + "hash", "MurmurHash3_32");
  render_add_metadata(b_rr, prefix + "conversion", "uint32_to_float32");
  render_add_metadata(b_rr, prefix + "manifest", manifest);
}

void BlenderSession::stamp_view_layer_metadata(Scene *scene, const string &view_layer_name)
{
  BL::RenderResult b_rr = b_engine.get_result();
  string prefix = "cycles." + view_layer_name + ".";

  /* Configured number of samples for the view layer. */
  b_rr.stamp_data_add_field((prefix + "samples").c_str(),
                            to_string(session->params.samples).c_str());

  /* Store ranged samples information. */
  if (session->tile_manager.range_num_samples != -1) {
    b_rr.stamp_data_add_field((prefix + "range_start_sample").c_str(),
                              to_string(session->tile_manager.range_start_sample).c_str());
    b_rr.stamp_data_add_field((prefix + "range_num_samples").c_str(),
                              to_string(session->tile_manager.range_num_samples).c_str());
  }

  /* Write cryptomatte metadata. */
  if (scene->film->cryptomatte_passes & CRYPT_OBJECT) {
    add_cryptomatte_layer(b_rr,
                          view_layer_name + ".CryptoObject",
                          scene->object_manager->get_cryptomatte_objects(scene));
  }
  if (scene->film->cryptomatte_passes & CRYPT_MATERIAL) {
    add_cryptomatte_layer(b_rr,
                          view_layer_name + ".CryptoMaterial",
                          scene->shader_manager->get_cryptomatte_materials(scene));
  }
  if (scene->film->cryptomatte_passes & CRYPT_ASSET) {
    add_cryptomatte_layer(b_rr,
                          view_layer_name + ".CryptoAsset",
                          scene->object_manager->get_cryptomatte_assets(scene));
  }

  /* Store synchronization and bare-render times. */
  double total_time, render_time;
  session->progress.get_time(total_time, render_time);
  b_rr.stamp_data_add_field((prefix + "total_time").c_str(),
                            time_human_readable_from_seconds(total_time).c_str());
  b_rr.stamp_data_add_field((prefix + "render_time").c_str(),
                            time_human_readable_from_seconds(render_time).c_str());
  b_rr.stamp_data_add_field((prefix + "synchronization_time").c_str(),
                            time_human_readable_from_seconds(total_time - render_time).c_str());
}

void BlenderSession::render(BL::Depsgraph &b_depsgraph_)
{
  b_depsgraph = b_depsgraph_;

  /* set callback to write out render results */
  session->write_render_tile_cb = function_bind(&BlenderSession::write_render_tile, this, _1);
  session->update_render_tile_cb = function_bind(
      &BlenderSession::update_render_tile, this, _1, _2);

  /* get buffer parameters */
  SessionParams session_params = BlenderSync::get_session_params(
      b_engine, b_userpref, b_scene, background);
  BufferParams buffer_params = BlenderSync::get_buffer_params(
      b_render, b_v3d, b_rv3d, scene->camera, width, height);

  /* render each layer */
  BL::ViewLayer b_view_layer = b_depsgraph.view_layer_eval();

  /* temporary render result to find needed passes and views */
  BL::RenderResult b_rr = begin_render_result(
      b_engine, 0, 0, 1, 1, b_view_layer.name().c_str(), NULL);
  BL::RenderResult::layers_iterator b_single_rlay;
  b_rr.layers.begin(b_single_rlay);
  BL::RenderLayer b_rlay = *b_single_rlay;
  b_rlay_name = b_view_layer.name();

  /* add passes */
  vector<Pass> passes = sync->sync_render_passes(b_rlay, b_view_layer);
  buffer_params.passes = passes;

  PointerRNA crl = RNA_pointer_get(&b_view_layer.ptr, "cycles");
  bool full_denoising = get_boolean(crl, "use_denoising");
  bool write_denoising_passes = get_boolean(crl, "denoising_store_passes");

  bool run_denoising = full_denoising || write_denoising_passes;

  session->tile_manager.schedule_denoising = run_denoising;
  buffer_params.denoising_data_pass = run_denoising;
  buffer_params.denoising_clean_pass = (scene->film->denoising_flags & DENOISING_CLEAN_ALL_PASSES);
  buffer_params.denoising_prefiltered_pass = write_denoising_passes;

  session->params.run_denoising = run_denoising;
  session->params.full_denoising = full_denoising;
  session->params.write_denoising_passes = write_denoising_passes;
  session->params.denoising.radius = get_int(crl, "denoising_radius");
  session->params.denoising.strength = get_float(crl, "denoising_strength");
  session->params.denoising.feature_strength = get_float(crl, "denoising_feature_strength");
  session->params.denoising.relative_pca = get_boolean(crl, "denoising_relative_pca");

  scene->film->denoising_data_pass = buffer_params.denoising_data_pass;
  scene->film->denoising_clean_pass = buffer_params.denoising_clean_pass;
  scene->film->denoising_prefiltered_pass = buffer_params.denoising_prefiltered_pass;

  scene->film->pass_alpha_threshold = b_view_layer.pass_alpha_threshold();
  scene->film->tag_passes_update(scene, passes);
  scene->film->tag_update(scene);
  scene->integrator->tag_update(scene);

  BL::RenderResult::views_iterator b_view_iter;

  int num_views = 0;
  for (b_rr.views.begin(b_view_iter); b_view_iter != b_rr.views.end(); ++b_view_iter) {
    num_views++;
  }

  int view_index = 0;
  for (b_rr.views.begin(b_view_iter); b_view_iter != b_rr.views.end();
       ++b_view_iter, ++view_index) {
    b_rview_name = b_view_iter->name();

    /* set the current view */
    b_engine.active_view_set(b_rview_name.c_str());

    /* update scene */
    BL::Object b_camera_override(b_engine.camera_override());
    sync->sync_camera(b_render, b_camera_override, width, height, b_rview_name.c_str());
    sync->sync_data(
        b_render, b_depsgraph, b_v3d, b_camera_override, width, height, &python_thread_state);
    builtin_images_load();

    /* Attempt to free all data which is held by Blender side, since at this
     * point we knwo that we've got everything to render current view layer.
     */
    /* At the moment we only free if we are not doing multi-view
     * (or if we are rendering the last view). See T58142/D4239 for discussion.
     */
    if (view_index == num_views - 1) {
      free_blender_memory_if_possible();
    }

    /* Make sure all views have different noise patterns. - hardcoded value just to make it random
     */
    if (view_index != 0) {
      scene->integrator->seed += hash_int_2d(scene->integrator->seed,
                                             hash_int(view_index * 0xdeadbeef));
      scene->integrator->tag_update(scene);
    }

    /* Update number of samples per layer. */
    int samples = sync->get_layer_samples();
    bool bound_samples = sync->get_layer_bound_samples();
    int effective_layer_samples;

    if (samples != 0 && (!bound_samples || (samples < session_params.samples)))
      effective_layer_samples = samples;
    else
      effective_layer_samples = session_params.samples;

    /* Update tile manager if we're doing resumable render. */
    update_resumable_tile_manager(effective_layer_samples);

    /* Update session itself. */
    session->reset(buffer_params, effective_layer_samples);

    /* render */
    session->start();
    session->wait();

    if (!b_engine.is_preview() && background && print_render_stats) {
      RenderStats stats;
      session->collect_statistics(&stats);
      printf("Render statistics:\n%s\n", stats.full_report().c_str());
    }

    if (session->progress.get_cancel())
      break;
  }

  /* add metadata */
  stamp_view_layer_metadata(scene, b_rlay_name);

  /* free result without merging */
  end_render_result(b_engine, b_rr, true, true, false);

  double total_time, render_time;
  session->progress.get_time(total_time, render_time);
  VLOG(1) << "Total render time: " << total_time;
  VLOG(1) << "Render time (without synchronization): " << render_time;

  /* clear callback */
  session->write_render_tile_cb = function_null;
  session->update_render_tile_cb = function_null;

  /* TODO: find a way to clear this data for persistent data render */
#if 0
  /* free all memory used (host and device), so we wouldn't leave render
   * engine with extra memory allocated
   */

  session->device_free();

  delete sync;
  sync = NULL;
#endif
}

static void populate_bake_data(BakeData *data,
                               const int object_id,
                               BL::BakePixel &pixel_array,
                               const int num_pixels)
{
  BL::BakePixel bp = pixel_array;

  int i;
  for (i = 0; i < num_pixels; i++) {
    if (bp.object_id() == object_id) {
      data->set(i, bp.primitive_id(), bp.uv(), bp.du_dx(), bp.du_dy(), bp.dv_dx(), bp.dv_dy());
    }
    else {
      data->set_null(i);
    }
    bp = bp.next();
  }
}

static int bake_pass_filter_get(const int pass_filter)
{
  int flag = BAKE_FILTER_NONE;

  if ((pass_filter & BL::BakeSettings::pass_filter_DIRECT) != 0)
    flag |= BAKE_FILTER_DIRECT;
  if ((pass_filter & BL::BakeSettings::pass_filter_INDIRECT) != 0)
    flag |= BAKE_FILTER_INDIRECT;
  if ((pass_filter & BL::BakeSettings::pass_filter_COLOR) != 0)
    flag |= BAKE_FILTER_COLOR;

  if ((pass_filter & BL::BakeSettings::pass_filter_DIFFUSE) != 0)
    flag |= BAKE_FILTER_DIFFUSE;
  if ((pass_filter & BL::BakeSettings::pass_filter_GLOSSY) != 0)
    flag |= BAKE_FILTER_GLOSSY;
  if ((pass_filter & BL::BakeSettings::pass_filter_TRANSMISSION) != 0)
    flag |= BAKE_FILTER_TRANSMISSION;
  if ((pass_filter & BL::BakeSettings::pass_filter_SUBSURFACE) != 0)
    flag |= BAKE_FILTER_SUBSURFACE;

  if ((pass_filter & BL::BakeSettings::pass_filter_EMIT) != 0)
    flag |= BAKE_FILTER_EMISSION;
  if ((pass_filter & BL::BakeSettings::pass_filter_AO) != 0)
    flag |= BAKE_FILTER_AO;

  return flag;
}

void BlenderSession::bake(BL::Depsgraph &b_depsgraph_,
                          BL::Object &b_object,
                          const string &pass_type,
                          const int pass_filter,
                          const int object_id,
                          BL::BakePixel &pixel_array,
                          const size_t num_pixels,
                          const int /*depth*/,
                          float result[])
{
  b_depsgraph = b_depsgraph_;

  ShaderEvalType shader_type = get_shader_type(pass_type);

  /* Set baking flag in advance, so kernel loading can check if we need
   * any baking capabilities.
   */
  scene->bake_manager->set_baking(true);

  /* ensure kernels are loaded before we do any scene updates */
  session->load_kernels();

  if (shader_type == SHADER_EVAL_UV) {
    /* force UV to be available */
    Pass::add(PASS_UV, scene->film->passes);
  }

  int bake_pass_filter = bake_pass_filter_get(pass_filter);
  bake_pass_filter = BakeManager::shader_type_to_pass_filter(shader_type, bake_pass_filter);

  /* force use_light_pass to be true if we bake more than just colors */
  if (bake_pass_filter & ~BAKE_FILTER_COLOR) {
    Pass::add(PASS_LIGHT, scene->film->passes);
  }

  /* create device and update scene */
  scene->film->tag_update(scene);
  scene->integrator->tag_update(scene);

  if (!session->progress.get_cancel()) {
    /* update scene */
    BL::Object b_camera_override(b_engine.camera_override());
    sync->sync_camera(b_render, b_camera_override, width, height, "");
    sync->sync_data(
        b_render, b_depsgraph, b_v3d, b_camera_override, width, height, &python_thread_state);
    builtin_images_load();
  }

  BakeData *bake_data = NULL;

  if (!session->progress.get_cancel()) {
    /* get buffer parameters */
    SessionParams session_params = BlenderSync::get_session_params(
        b_engine, b_userpref, b_scene, background);
    BufferParams buffer_params = BlenderSync::get_buffer_params(
        b_render, b_v3d, b_rv3d, scene->camera, width, height);

    scene->bake_manager->set_shader_limit((size_t)b_engine.tile_x(), (size_t)b_engine.tile_y());

    /* set number of samples */
    session->tile_manager.set_samples(session_params.samples);
    session->reset(buffer_params, session_params.samples);
    session->update_scene();

    /* find object index. todo: is arbitrary - copied from mesh_displace.cpp */
    size_t object_index = OBJECT_NONE;
    int tri_offset = 0;

    for (size_t i = 0; i < scene->objects.size(); i++) {
      if (strcmp(scene->objects[i]->name.c_str(), b_object.name().c_str()) == 0) {
        object_index = i;
        tri_offset = scene->objects[i]->mesh->tri_offset;
        break;
      }
    }

    /* Object might have been disabled for rendering or excluded in some
     * other way, in that case Blender will report a warning afterwards. */
    if (object_index != OBJECT_NONE) {
      int object = object_index;

      bake_data = scene->bake_manager->init(object, tri_offset, num_pixels);
      populate_bake_data(bake_data, object_id, pixel_array, num_pixels);
    }

    /* set number of samples */
    session->tile_manager.set_samples(session_params.samples);
    session->reset(buffer_params, session_params.samples);
    session->update_scene();

    session->progress.set_update_callback(
        function_bind(&BlenderSession::update_bake_progress, this));
  }

  /* Perform bake. Check cancel to avoid crash with incomplete scene data. */
  if (!session->progress.get_cancel() && bake_data) {
    scene->bake_manager->bake(scene->device,
                              &scene->dscene,
                              scene,
                              session->progress,
                              shader_type,
                              bake_pass_filter,
                              bake_data,
                              result);
  }

  /* free all memory used (host and device), so we wouldn't leave render
   * engine with extra memory allocated
   */

  session->device_free();

  delete sync;
  sync = NULL;
}

void BlenderSession::do_write_update_render_result(BL::RenderLayer &b_rlay,
                                                   RenderTile &rtile,
                                                   bool do_update_only)
{
  RenderBuffers *buffers = rtile.buffers;

  /* copy data from device */
  if (!buffers->copy_from_device())
    return;

  float exposure = scene->film->exposure;

  vector<float> pixels(rtile.w * rtile.h * 4);

  /* Adjust absolute sample number to the range. */
  int sample = rtile.sample;
  const int range_start_sample = session->tile_manager.range_start_sample;
  if (range_start_sample != -1) {
    sample -= range_start_sample;
  }

  if (!do_update_only) {
    /* copy each pass */
    BL::RenderLayer::passes_iterator b_iter;

    for (b_rlay.passes.begin(b_iter); b_iter != b_rlay.passes.end(); ++b_iter) {
      BL::RenderPass b_pass(*b_iter);

      /* find matching pass type */
      PassType pass_type = BlenderSync::get_pass_type(b_pass);
      int components = b_pass.channels();

      bool read = false;
      if (pass_type != PASS_NONE) {
        /* copy pixels */
        read = buffers->get_pass_rect(
            pass_type, exposure, sample, components, &pixels[0], b_pass.name());
      }
      else {
        int denoising_offset = BlenderSync::get_denoising_pass(b_pass);
        if (denoising_offset >= 0) {
          read = buffers->get_denoising_pass_rect(
              denoising_offset, exposure, sample, components, &pixels[0]);
        }
      }

      if (!read) {
        memset(&pixels[0], 0, pixels.size() * sizeof(float));
      }

      b_pass.rect(&pixels[0]);
    }
  }
  else {
    /* copy combined pass */
    BL::RenderPass b_combined_pass(b_rlay.passes.find_by_name("Combined", b_rview_name.c_str()));
    if (buffers->get_pass_rect(PASS_COMBINED, exposure, sample, 4, &pixels[0], "Combined"))
      b_combined_pass.rect(&pixels[0]);
  }
}

void BlenderSession::write_render_result(BL::RenderLayer &b_rlay, RenderTile &rtile)
{
  do_write_update_render_result(b_rlay, rtile, false);
}

void BlenderSession::update_render_result(BL::RenderLayer &b_rlay, RenderTile &rtile)
{
  do_write_update_render_result(b_rlay, rtile, true);
}

void BlenderSession::synchronize(BL::Depsgraph &b_depsgraph_)
{
  /* only used for viewport render */
  if (!b_v3d)
    return;

  /* on session/scene parameter changes, we recreate session entirely */
  SessionParams session_params = BlenderSync::get_session_params(
      b_engine, b_userpref, b_scene, background);
  SceneParams scene_params = BlenderSync::get_scene_params(b_scene, background);
  bool session_pause = BlenderSync::get_session_pause(b_scene, background);

  if (session->params.modified(session_params) || scene->params.modified(scene_params)) {
    free_session();
    create_session();
    return;
  }

  /* increase samples, but never decrease */
  session->set_samples(session_params.samples);
  session->set_pause(session_pause);

  /* copy recalc flags, outside of mutex so we can decide to do the real
   * synchronization at a later time to not block on running updates */
  sync->sync_recalc(b_depsgraph_);

  /* don't do synchronization if on pause */
  if (session_pause) {
    tag_update();
    return;
  }

  /* try to acquire mutex. if we don't want to or can't, come back later */
  if (!session->ready_to_reset() || !session->scene->mutex.try_lock()) {
    tag_update();
    return;
  }

  /* data and camera synchronize */
  b_depsgraph = b_depsgraph_;

  BL::Object b_camera_override(b_engine.camera_override());
  sync->sync_data(
      b_render, b_depsgraph, b_v3d, b_camera_override, width, height, &python_thread_state);

  if (b_rv3d)
    sync->sync_view(b_v3d, b_rv3d, width, height);
  else
    sync->sync_camera(b_render, b_camera_override, width, height, "");

  /* reset if needed */
  if (scene->need_reset()) {
    BufferParams buffer_params = BlenderSync::get_buffer_params(
        b_render, b_v3d, b_rv3d, scene->camera, width, height);
    session->reset(buffer_params, session_params.samples);

    /* After session reset, so device is not accessing image data anymore. */
    builtin_images_load();

    /* reset time */
    start_resize_time = 0.0;
  }

  /* unlock */
  session->scene->mutex.unlock();

  /* Start rendering thread, if it's not running already. Do this
   * after all scene data has been synced at least once. */
  session->start();
}

bool BlenderSession::draw(int w, int h)
{
  /* pause in redraw in case update is not being called due to final render */
  session->set_pause(BlenderSync::get_session_pause(b_scene, background));

  /* before drawing, we verify camera and viewport size changes, because
   * we do not get update callbacks for those, we must detect them here */
  if (session->ready_to_reset()) {
    bool reset = false;

    /* if dimensions changed, reset */
    if (width != w || height != h) {
      if (start_resize_time == 0.0) {
        /* don't react immediately to resizes to avoid flickery resizing
         * of the viewport, and some window managers changing the window
         * size temporarily on unminimize */
        start_resize_time = time_dt();
        tag_redraw();
      }
      else if (time_dt() - start_resize_time < 0.2) {
        tag_redraw();
      }
      else {
        width = w;
        height = h;
        reset = true;
      }
    }

    /* try to acquire mutex. if we can't, come back later */
    if (!session->scene->mutex.try_lock()) {
      tag_update();
    }
    else {
      /* update camera from 3d view */

      sync->sync_view(b_v3d, b_rv3d, width, height);

      if (scene->camera->need_update)
        reset = true;

      session->scene->mutex.unlock();
    }

    /* reset if requested */
    if (reset) {
      SessionParams session_params = BlenderSync::get_session_params(
          b_engine, b_userpref, b_scene, background);
      BufferParams buffer_params = BlenderSync::get_buffer_params(
          b_render, b_v3d, b_rv3d, scene->camera, width, height);
      bool session_pause = BlenderSync::get_session_pause(b_scene, background);

      if (session_pause == false) {
        session->reset(buffer_params, session_params.samples);
        start_resize_time = 0.0;
      }
    }
  }
  else {
    tag_update();
  }

  /* update status and progress for 3d view draw */
  update_status_progress();

  /* draw */
  BufferParams buffer_params = BlenderSync::get_buffer_params(
      b_render, b_v3d, b_rv3d, scene->camera, width, height);
  DeviceDrawParams draw_params;

  if (session->params.display_buffer_linear) {
    draw_params.bind_display_space_shader_cb = function_bind(
        &BL::RenderEngine::bind_display_space_shader, &b_engine, b_scene);
    draw_params.unbind_display_space_shader_cb = function_bind(
        &BL::RenderEngine::unbind_display_space_shader, &b_engine);
  }

  return !session->draw(buffer_params, draw_params);
}

void BlenderSession::get_status(string &status, string &substatus)
{
  session->progress.get_status(status, substatus);
}

void BlenderSession::get_kernel_status(string &kernel_status)
{
  session->progress.get_kernel_status(kernel_status);
}

void BlenderSession::get_progress(float &progress, double &total_time, double &render_time)
{
  session->progress.get_time(total_time, render_time);
  progress = session->progress.get_progress();
}

void BlenderSession::update_bake_progress()
{
  float progress = session->progress.get_progress();

  if (progress != last_progress) {
    b_engine.update_progress(progress);
    last_progress = progress;
  }
}

void BlenderSession::update_status_progress()
{
  string timestatus, status, substatus, kernel_status;
  string scene_status = "";
  float progress;
  double total_time, remaining_time = 0, render_time;
  float mem_used = (float)session->stats.mem_used / 1024.0f / 1024.0f;
  float mem_peak = (float)session->stats.mem_peak / 1024.0f / 1024.0f;

  get_status(status, substatus);
  get_kernel_status(kernel_status);
  get_progress(progress, total_time, render_time);

  if (progress > 0)
    remaining_time = (1.0 - (double)progress) * (render_time / (double)progress);

  if (background) {
    scene_status += " | " + scene->name;
    if (b_rlay_name != "")
      scene_status += ", " + b_rlay_name;

    if (b_rview_name != "")
      scene_status += ", " + b_rview_name;

    if (remaining_time > 0) {
      timestatus += "Remaining:" + time_human_readable_from_seconds(remaining_time) + " | ";
    }

    timestatus += string_printf("Mem:%.2fM, Peak:%.2fM", (double)mem_used, (double)mem_peak);

    if (status.size() > 0)
      status = " | " + status;
    if (substatus.size() > 0)
      status += " | " + substatus;
    if (kernel_status.size() > 0)
      status += " | " + kernel_status;
  }

  double current_time = time_dt();
  /* When rendering in a window, redraw the status at least once per second to keep the elapsed and
   * remaining time up-to-date. For headless rendering, only report when something significant
   * changes to keep the console output readable. */
  if (status != last_status || (!headless && (current_time - last_status_time) > 1.0)) {
    b_engine.update_stats("", (timestatus + scene_status + status).c_str());
    b_engine.update_memory_stats(mem_used, mem_peak);
    last_status = status;
    last_status_time = current_time;
  }
  if (progress != last_progress) {
    b_engine.update_progress(progress);
    last_progress = progress;
  }

  if (session->progress.get_error()) {
    string error = session->progress.get_error_message();
    if (error != last_error) {
      /* TODO(sergey): Currently C++ RNA API doesn't let us to
       * use mnemonic name for the variable. Would be nice to
       * have this figured out.
       *
       * For until then, 1 << 5 means RPT_ERROR.
       */
      b_engine.report(1 << 5, error.c_str());
      b_engine.error_set(error.c_str());
      last_error = error;
    }
  }
}

void BlenderSession::tag_update()
{
  /* tell blender that we want to get another update callback */
  b_engine.tag_update();
}

void BlenderSession::tag_redraw()
{
  if (background) {
    /* update stats and progress, only for background here because
     * in 3d view we do it in draw for thread safety reasons */
    update_status_progress();

    /* offline render, redraw if timeout passed */
    if (time_dt() - last_redraw_time > 1.0) {
      b_engine.tag_redraw();
      last_redraw_time = time_dt();
    }
  }
  else {
    /* tell blender that we want to redraw */
    b_engine.tag_redraw();
  }
}

void BlenderSession::test_cancel()
{
  /* test if we need to cancel rendering */
  if (background)
    if (b_engine.test_break())
      session->progress.set_cancel("Cancelled");
}

/* builtin image file name is actually an image datablock name with
 * absolute sequence frame number concatenated via '@' character
 *
 * this function splits frame from builtin name
 */
int BlenderSession::builtin_image_frame(const string &builtin_name)
{
  int last = builtin_name.find_last_of('@');
  return atoi(builtin_name.substr(last + 1, builtin_name.size() - last - 1).c_str());
}

void BlenderSession::builtin_image_info(const string &builtin_name,
                                        void *builtin_data,
                                        ImageMetaData &metadata)
{
  /* empty image */
  metadata.width = 1;
  metadata.height = 1;

  if (!builtin_data)
    return;

  /* recover ID pointer */
  PointerRNA ptr;
  RNA_id_pointer_create((ID *)builtin_data, &ptr);
  BL::ID b_id(ptr);

  if (b_id.is_a(&RNA_Image)) {
    /* image data */
    BL::Image b_image(b_id);

    metadata.builtin_free_cache = !b_image.has_data();
    metadata.is_float = b_image.is_float();
    metadata.width = b_image.size()[0];
    metadata.height = b_image.size()[1];
    metadata.depth = 1;
    metadata.channels = b_image.channels();

    if (metadata.is_float) {
      /* Float images are already converted on the Blender side,
       * no need to do anything in Cycles. */
      metadata.colorspace = u_colorspace_raw;
    }
  }
  else if (b_id.is_a(&RNA_Object)) {
    /* smoke volume data */
    BL::Object b_ob(b_id);
    BL::SmokeDomainSettings b_domain = object_smoke_domain_find(b_ob);

    metadata.is_float = true;
    metadata.depth = 1;
    metadata.channels = 1;

    if (!b_domain)
      return;

    if (builtin_name == Attribute::standard_name(ATTR_STD_VOLUME_DENSITY) ||
        builtin_name == Attribute::standard_name(ATTR_STD_VOLUME_FLAME) ||
        builtin_name == Attribute::standard_name(ATTR_STD_VOLUME_HEAT) ||
        builtin_name == Attribute::standard_name(ATTR_STD_VOLUME_TEMPERATURE))
      metadata.channels = 1;
    else if (builtin_name == Attribute::standard_name(ATTR_STD_VOLUME_COLOR))
      metadata.channels = 4;
    else if (builtin_name == Attribute::standard_name(ATTR_STD_VOLUME_VELOCITY))
      metadata.channels = 3;
    else
      return;

    int3 resolution = get_int3(b_domain.domain_resolution());
    int amplify = (b_domain.use_high_resolution()) ? b_domain.amplify() + 1 : 1;

    /* Velocity and heat data is always low-resolution. */
    if (builtin_name == Attribute::standard_name(ATTR_STD_VOLUME_VELOCITY) ||
        builtin_name == Attribute::standard_name(ATTR_STD_VOLUME_HEAT)) {
      amplify = 1;
    }

    metadata.width = resolution.x * amplify;
    metadata.height = resolution.y * amplify;
    metadata.depth = resolution.z * amplify;
  }
  else {
    /* TODO(sergey): Check we're indeed in shader node tree. */
    PointerRNA ptr;
    RNA_pointer_create(NULL, &RNA_Node, builtin_data, &ptr);
    BL::Node b_node(ptr);
    if (b_node.is_a(&RNA_ShaderNodeTexPointDensity)) {
      BL::ShaderNodeTexPointDensity b_point_density_node(b_node);
      metadata.channels = 4;
      metadata.width = b_point_density_node.resolution();
      metadata.height = metadata.width;
      metadata.depth = metadata.width;
      metadata.is_float = true;
    }
  }
}

bool BlenderSession::builtin_image_pixels(const string &builtin_name,
                                          void *builtin_data,
                                          unsigned char *pixels,
                                          const size_t pixels_size,
                                          const bool associate_alpha,
                                          const bool free_cache)
{
  if (!builtin_data) {
    return false;
  }

  const int frame = builtin_image_frame(builtin_name);

  PointerRNA ptr;
  RNA_id_pointer_create((ID *)builtin_data, &ptr);
  BL::Image b_image(ptr);

  const int width = b_image.size()[0];
  const int height = b_image.size()[1];
  const int channels = b_image.channels();

  unsigned char *image_pixels = image_get_pixels_for_frame(b_image, frame);
  const size_t num_pixels = ((size_t)width) * height;

  if (image_pixels && num_pixels * channels == pixels_size) {
    memcpy(pixels, image_pixels, pixels_size * sizeof(unsigned char));
  }
  else {
    if (channels == 1) {
      memset(pixels, 0, pixels_size * sizeof(unsigned char));
    }
    else {
      const size_t num_pixels_safe = pixels_size / channels;
      unsigned char *cp = pixels;
      for (size_t i = 0; i < num_pixels_safe; i++, cp += channels) {
        cp[0] = 255;
        cp[1] = 0;
        cp[2] = 255;
        if (channels == 4) {
          cp[3] = 255;
        }
      }
    }
  }

  if (image_pixels) {
    MEM_freeN(image_pixels);
  }

  /* Free image buffers to save memory during render. */
  if (free_cache) {
    b_image.buffers_free();
  }

  if (associate_alpha) {
    /* Premultiply, byte images are always straight for Blender. */
    unsigned char *cp = pixels;
    for (size_t i = 0; i < num_pixels; i++, cp += channels) {
      cp[0] = (cp[0] * cp[3]) >> 8;
      cp[1] = (cp[1] * cp[3]) >> 8;
      cp[2] = (cp[2] * cp[3]) >> 8;
    }
  }
  return true;
}

bool BlenderSession::builtin_image_float_pixels(const string &builtin_name,
                                                void *builtin_data,
                                                float *pixels,
                                                const size_t pixels_size,
                                                const bool,
                                                const bool free_cache)
{
  if (!builtin_data) {
    return false;
  }

  PointerRNA ptr;
  RNA_id_pointer_create((ID *)builtin_data, &ptr);
  BL::ID b_id(ptr);

  if (b_id.is_a(&RNA_Image)) {
    /* image data */
    BL::Image b_image(b_id);
    int frame = builtin_image_frame(builtin_name);

    const int width = b_image.size()[0];
    const int height = b_image.size()[1];
    const int channels = b_image.channels();

    float *image_pixels;
    image_pixels = image_get_float_pixels_for_frame(b_image, frame);
    const size_t num_pixels = ((size_t)width) * height;

    if (image_pixels && num_pixels * channels == pixels_size) {
      memcpy(pixels, image_pixels, pixels_size * sizeof(float));
    }
    else {
      if (channels == 1) {
        memset(pixels, 0, num_pixels * sizeof(float));
      }
      else {
        const size_t num_pixels_safe = pixels_size / channels;
        float *fp = pixels;
        for (int i = 0; i < num_pixels_safe; i++, fp += channels) {
          fp[0] = 1.0f;
          fp[1] = 0.0f;
          fp[2] = 1.0f;
          if (channels == 4) {
            fp[3] = 1.0f;
          }
        }
      }
    }

    if (image_pixels) {
      MEM_freeN(image_pixels);
    }

    /* Free image buffers to save memory during render. */
    if (free_cache) {
      b_image.buffers_free();
    }

    return true;
  }
  else if (b_id.is_a(&RNA_Object)) {
    /* smoke volume data */
    BL::Object b_ob(b_id);
    BL::SmokeDomainSettings b_domain = object_smoke_domain_find(b_ob);

    if (!b_domain) {
      return false;
    }

    int3 resolution = get_int3(b_domain.domain_resolution());
    int length, amplify = (b_domain.use_high_resolution()) ? b_domain.amplify() + 1 : 1;

    /* Velocity and heat data is always low-resolution. */
    if (builtin_name == Attribute::standard_name(ATTR_STD_VOLUME_VELOCITY) ||
        builtin_name == Attribute::standard_name(ATTR_STD_VOLUME_HEAT)) {
      amplify = 1;
    }

    const int width = resolution.x * amplify;
    const int height = resolution.y * amplify;
    const int depth = resolution.z * amplify;
    const size_t num_pixels = ((size_t)width) * height * depth;

    if (builtin_name == Attribute::standard_name(ATTR_STD_VOLUME_DENSITY)) {
      SmokeDomainSettings_density_grid_get_length(&b_domain.ptr, &length);
      if (length == num_pixels) {
        SmokeDomainSettings_density_grid_get(&b_domain.ptr, pixels);
        return true;
      }
    }
    else if (builtin_name == Attribute::standard_name(ATTR_STD_VOLUME_FLAME)) {
      /* this is in range 0..1, and interpreted by the OpenGL smoke viewer
       * as 1500..3000 K with the first part faded to zero density */
      SmokeDomainSettings_flame_grid_get_length(&b_domain.ptr, &length);
      if (length == num_pixels) {
        SmokeDomainSettings_flame_grid_get(&b_domain.ptr, pixels);
        return true;
      }
    }
    else if (builtin_name == Attribute::standard_name(ATTR_STD_VOLUME_COLOR)) {
      /* the RGB is "premultiplied" by density for better interpolation results */
      SmokeDomainSettings_color_grid_get_length(&b_domain.ptr, &length);
      if (length == num_pixels * 4) {
        SmokeDomainSettings_color_grid_get(&b_domain.ptr, pixels);
        return true;
      }
    }
    else if (builtin_name == Attribute::standard_name(ATTR_STD_VOLUME_VELOCITY)) {
      SmokeDomainSettings_velocity_grid_get_length(&b_domain.ptr, &length);
      if (length == num_pixels * 3) {
        SmokeDomainSettings_velocity_grid_get(&b_domain.ptr, pixels);
        return true;
      }
    }
    else if (builtin_name == Attribute::standard_name(ATTR_STD_VOLUME_HEAT)) {
      SmokeDomainSettings_heat_grid_get_length(&b_domain.ptr, &length);
      if (length == num_pixels) {
        SmokeDomainSettings_heat_grid_get(&b_domain.ptr, pixels);
        return true;
      }
    }
    else if (builtin_name == Attribute::standard_name(ATTR_STD_VOLUME_TEMPERATURE)) {
      SmokeDomainSettings_temperature_grid_get_length(&b_domain.ptr, &length);
      if (length == num_pixels) {
        SmokeDomainSettings_temperature_grid_get(&b_domain.ptr, pixels);
        return true;
      }
    }
    else {
      fprintf(
          stderr, "Cycles error: unknown volume attribute %s, skipping\n", builtin_name.c_str());
      pixels[0] = 0.0f;
      return false;
    }

    fprintf(stderr, "Cycles error: unexpected smoke volume resolution, skipping\n");
  }
  else {
    /* We originally were passing view_layer here but in reality we need a
     * a depsgraph to pass to the RE_point_density_minmax() function.
     */
    /* TODO(sergey): Check we're indeed in shader node tree. */
    PointerRNA ptr;
    RNA_pointer_create(NULL, &RNA_Node, builtin_data, &ptr);
    BL::Node b_node(ptr);
    if (b_node.is_a(&RNA_ShaderNodeTexPointDensity)) {
      BL::ShaderNodeTexPointDensity b_point_density_node(b_node);
      int length;
      b_point_density_node.calc_point_density(b_depsgraph, &length, &pixels);
    }
  }

  return false;
}

void BlenderSession::builtin_images_load()
{
  /* Force builtin images to be loaded along with Blender data sync. This
   * is needed because we may be reading from depsgraph evaluated data which
   * can be freed by Blender before Cycles reads it.
   *
   * TODO: the assumption that no further access to builtin image data will
   * happen is really weak, and likely to break in the future. We should find
   * a better solution to hand over the data directly to the image manager
   * instead of through callbacks whose timing is difficult to control. */
  ImageManager *manager = session->scene->image_manager;
  Device *device = session->device;
  manager->device_load_builtin(device, session->scene, session->progress);
}

void BlenderSession::update_resumable_tile_manager(int num_samples)
{
  const int num_resumable_chunks = BlenderSession::num_resumable_chunks,
            current_resumable_chunk = BlenderSession::current_resumable_chunk;
  if (num_resumable_chunks == 0) {
    return;
  }

  if (num_resumable_chunks > num_samples) {
    fprintf(stderr,
            "Cycles warning: more sample chunks (%d) than samples (%d), "
            "this will cause some samples to be included in multiple chunks.\n",
            num_resumable_chunks,
            num_samples);
  }

  const float num_samples_per_chunk = (float)num_samples / num_resumable_chunks;

  float range_start_sample, range_num_samples;
  if (current_resumable_chunk != 0) {
    /* Single chunk rendering. */
    range_start_sample = num_samples_per_chunk * (current_resumable_chunk - 1);
    range_num_samples = num_samples_per_chunk;
  }
  else {
    /* Ranged-chunks. */
    const int num_chunks = end_resumable_chunk - start_resumable_chunk + 1;
    range_start_sample = num_samples_per_chunk * (start_resumable_chunk - 1);
    range_num_samples = num_chunks * num_samples_per_chunk;
  }

  /* Round after doing the multiplications with num_chunks and num_samples_per_chunk
   * to allow for many small chunks. */
  int rounded_range_start_sample = (int)floor(range_start_sample + 0.5f);
  int rounded_range_num_samples = max((int)floor(range_num_samples + 0.5f), 1);

  /* Make sure we don't overshoot. */
  if (rounded_range_start_sample + rounded_range_num_samples > num_samples) {
    rounded_range_num_samples = num_samples - rounded_range_num_samples;
  }

  VLOG(1) << "Samples range start is " << range_start_sample << ", "
          << "number of samples to render is " << range_num_samples;

  scene->integrator->start_sample = rounded_range_start_sample;
  scene->integrator->tag_update(scene);

  session->tile_manager.range_start_sample = rounded_range_start_sample;
  session->tile_manager.range_num_samples = rounded_range_num_samples;
}

void BlenderSession::free_blender_memory_if_possible()
{
  if (!background) {
    /* During interactive render we can not free anything: attempts to save
     * memory would cause things to be allocated and evaluated for every
     * updated sample.
     */
    return;
  }
  b_engine.free_blender_memory();
}

CCL_NAMESPACE_END
