/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

#include <stdlib.h>

#include "device/device.h"
#include "scene/background.h"
#include "scene/camera.h"
#include "scene/colorspace.h"
#include "scene/film.h"
#include "scene/integrator.h"
#include "scene/light.h"
#include "scene/mesh.h"
#include "scene/object.h"
#include "scene/scene.h"
#include "scene/shader.h"
#include "scene/stats.h"
#include "session/buffers.h"
#include "session/session.h"

#include "util/algorithm.h"
#include "util/color.h"
#include "util/foreach.h"
#include "util/function.h"
#include "util/hash.h"
#include "util/log.h"
#include "util/murmurhash.h"
#include "util/path.h"
#include "util/progress.h"
#include "util/time.h"

#include "blender/display_driver.h"
#include "blender/output_driver.h"
#include "blender/session.h"
#include "blender/sync.h"
#include "blender/util.h"

CCL_NAMESPACE_BEGIN

DeviceTypeMask BlenderSession::device_override = DEVICE_MASK_ALL;
bool BlenderSession::headless = false;
bool BlenderSession::print_render_stats = false;

BlenderSession::BlenderSession(BL::RenderEngine &b_engine,
                               BL::Preferences &b_userpref,
                               BL::BlendData &b_data,
                               bool preview_osl)
    : session(NULL),
      scene(NULL),
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
      python_thread_state(NULL),
      use_developer_ui(b_userpref.experimental().use_cycles_debug() &&
                       b_userpref.view().show_developer_ui())
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
      scene(NULL),
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
      python_thread_state(NULL),
      use_developer_ui(b_userpref.experimental().use_cycles_debug() &&
                       b_userpref.view().show_developer_ui())
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

void BlenderSession::create_session()
{
  const SessionParams session_params = BlenderSync::get_session_params(
      b_engine, b_userpref, b_scene, background);
  const SceneParams scene_params = BlenderSync::get_scene_params(
      b_scene, background, use_developer_ui);
  const bool session_pause = BlenderSync::get_session_pause(b_scene, background);

  /* reset status/progress */
  last_status = "";
  last_error = "";
  last_progress = -1.0;
  start_resize_time = 0.0;

  /* create session */
  session = new Session(session_params, scene_params);
  session->progress.set_update_callback(function_bind(&BlenderSession::tag_redraw, this));
  session->progress.set_cancel_callback(function_bind(&BlenderSession::test_cancel, this));
  session->set_pause(session_pause);

  /* create scene */
  scene = session->scene;
  scene->name = b_scene.name();

  /* create sync */
  sync = new BlenderSync(
      b_engine, b_data, b_scene, scene, !background, use_developer_ui, session->progress);
  BL::Object b_camera_override(b_engine.camera_override());
  if (b_v3d) {
    sync->sync_view(b_v3d, b_rv3d, width, height);
  }
  else {
    sync->sync_camera(b_render, b_camera_override, width, height, "");
  }

  /* set buffer parameters */
  const BufferParams buffer_params = BlenderSync::get_buffer_params(
      b_v3d, b_rv3d, scene->camera, width, height);
  session->reset(session_params, buffer_params);

  /* Viewport and preview (as in, material preview) does not do tiled rendering, so can inform
   * engine that no tracking of the tiles state is needed.
   * The offline rendering will make a decision when tile is being written. The penalty of asking
   * the engine to keep track of tiles state is minimal, so there is nothing to worry about here
   * about possible single-tiled final render. */
  if (!b_engine.is_preview() && !b_v3d) {
    b_engine.use_highlight_tiles(true);
  }
}

void BlenderSession::reset_session(BL::BlendData &b_data, BL::Depsgraph &b_depsgraph)
{
  /* Update data, scene and depsgraph pointers. These can change after undo. */
  this->b_data = b_data;
  this->b_depsgraph = b_depsgraph;
  this->b_scene = b_depsgraph.scene_eval();
  if (sync) {
    sync->reset(this->b_data, this->b_scene);
  }

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

  bool is_new_session = (session == NULL);
  if (is_new_session) {
    /* Initialize session and remember it was just created so not to
     * re-create it below.
     */
    create_session();
  }

  if (b_v3d) {
    /* NOTE: We need to create session, but all the code from below
     * will make viewport render to stuck on initialization.
     */
    return;
  }

  const SessionParams session_params = BlenderSync::get_session_params(
      b_engine, b_userpref, b_scene, background);
  const SceneParams scene_params = BlenderSync::get_scene_params(
      b_scene, background, use_developer_ui);

  if (scene->params.modified(scene_params) || session->params.modified(session_params) ||
      !this->b_render.use_persistent_data())
  {
    /* if scene or session parameters changed, it's easier to simply re-create
     * them rather than trying to distinguish which settings need to be updated
     */
    if (!is_new_session) {
      free_session();
      create_session();
    }
    return;
  }

  session->progress.reset();

  /* peak memory usage should show current render peak, not peak for all renders
   * made by this render session
   */
  session->stats.mem_peak = session->stats.mem_used;

  if (is_new_session) {
    /* Sync object should be re-created for new scene. */
    delete sync;
    sync = new BlenderSync(
        b_engine, b_data, b_scene, scene, !background, use_developer_ui, session->progress);
  }
  else {
    /* Sync recalculations to do just the required updates. */
    sync->sync_recalc(b_depsgraph, b_v3d);
  }

  BL::Object b_camera_override(b_engine.camera_override());
  sync->sync_camera(b_render, b_camera_override, width, height, "");

  BL::SpaceView3D b_null_space_view3d(PointerRNA_NULL);
  BL::RegionView3D b_null_region_view3d(PointerRNA_NULL);
  const BufferParams buffer_params = BlenderSync::get_buffer_params(
      b_null_space_view3d, b_null_region_view3d, scene->camera, width, height);
  session->reset(session_params, buffer_params);

  /* reset time */
  start_resize_time = 0.0;

  {
    thread_scoped_lock lock(draw_state_.mutex);
    draw_state_.last_pass_index = -1;
  }
}

void BlenderSession::free_session()
{
  if (session) {
    session->cancel(true);
  }

  delete sync;
  sync = nullptr;

  delete session;
  session = nullptr;

  display_driver_ = nullptr;
}

void BlenderSession::full_buffer_written(string_view filename)
{
  full_buffer_files_.emplace_back(filename);
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
  /* TODO(sergey): Need to bring this information back. */
#if 0
  if (session->tile_manager.range_num_samples != -1) {
    b_rr.stamp_data_add_field((prefix + "range_start_sample").c_str(),
                              to_string(session->tile_manager.range_start_sample).c_str());
    b_rr.stamp_data_add_field((prefix + "range_num_samples").c_str(),
                              to_string(session->tile_manager.range_num_samples).c_str());
  }
#endif

  /* Write cryptomatte metadata. */
  if (scene->film->get_cryptomatte_passes() & CRYPT_OBJECT) {
    add_cryptomatte_layer(b_rr,
                          view_layer_name + ".CryptoObject",
                          scene->object_manager->get_cryptomatte_objects(scene));
  }
  if (scene->film->get_cryptomatte_passes() & CRYPT_MATERIAL) {
    add_cryptomatte_layer(b_rr,
                          view_layer_name + ".CryptoMaterial",
                          scene->shader_manager->get_cryptomatte_materials(scene));
  }
  if (scene->film->get_cryptomatte_passes() & CRYPT_ASSET) {
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

  if (session->progress.get_cancel()) {
    update_status_progress();
    return;
  }

  /* Create driver to write out render results. */
  ensure_display_driver_if_needed();
  session->set_output_driver(make_unique<BlenderOutputDriver>(b_engine));

  session->full_buffer_written_cb = [&](string_view filename) { full_buffer_written(filename); };

  BL::ViewLayer b_view_layer = b_depsgraph.view_layer_eval();

  /* get buffer parameters */
  const SessionParams session_params = BlenderSync::get_session_params(
      b_engine, b_userpref, b_scene, background);
  BufferParams buffer_params = BlenderSync::get_buffer_params(
      b_v3d, b_rv3d, scene->camera, width, height);

  /* temporary render result to find needed passes and views */
  BL::RenderResult b_rr = b_engine.begin_result(0, 0, 1, 1, b_view_layer.name().c_str(), NULL);
  BL::RenderResult::layers_iterator b_single_rlay;
  b_rr.layers.begin(b_single_rlay);
  BL::RenderLayer b_rlay = *b_single_rlay;

  {
    thread_scoped_lock lock(draw_state_.mutex);
    b_rlay_name = b_view_layer.name();

    /* Signal that the display pass is to be updated. */
    draw_state_.last_pass_index = -1;
  }

  /* Compute render passes and film settings. */
  sync->sync_render_passes(b_rlay, b_view_layer);

  BL::RenderResult::views_iterator b_view_iter;

  int num_views = 0;
  for (b_rr.views.begin(b_view_iter); b_view_iter != b_rr.views.end(); ++b_view_iter) {
    num_views++;
  }

  int view_index = 0;
  for (b_rr.views.begin(b_view_iter); b_view_iter != b_rr.views.end(); ++b_view_iter, ++view_index)
  {
    b_rview_name = b_view_iter->name();

    buffer_params.layer = b_view_layer.name();
    buffer_params.view = b_rview_name;

    /* set the current view */
    b_engine.active_view_set(b_rview_name.c_str());

    /* Force update in this case, since the camera transform on each frame changes
     * in different views. This could be optimized by somehow storing the animated
     * camera transforms separate from the fixed stereo transform. */
    if ((scene->need_motion() != Scene::MOTION_NONE) && view_index > 0) {
      sync->tag_update();
    }

    /* update scene */
    BL::Object b_camera_override(b_engine.camera_override());
    sync->sync_camera(b_render, b_camera_override, width, height, b_rview_name.c_str());
    sync->sync_data(
        b_render, b_depsgraph, b_v3d, b_camera_override, width, height, &python_thread_state);
    builtin_images_load();

    /* Attempt to free all data which is held by Blender side, since at this
     * point we know that we've got everything to render current view layer.
     */
    /* At the moment we only free if we are not doing multi-view
     * (or if we are rendering the last view). See #58142/D4239 for discussion.
     */
    if (view_index == num_views - 1) {
      free_blender_memory_if_possible();
    }

    /* Make sure all views have different noise patterns. - hardcoded value just to make it random
     */
    if (view_index != 0) {
      int seed = scene->integrator->get_seed();
      seed += hash_uint2(seed, hash_uint2(view_index * 0xdeadbeef, 0));
      scene->integrator->set_seed(seed);
    }

    /* Update number of samples per layer. */
    const int samples = sync->get_layer_samples();
    const bool bound_samples = sync->get_layer_bound_samples();

    SessionParams effective_session_params = session_params;
    if (samples != 0 && (!bound_samples || (samples < session_params.samples))) {
      effective_session_params.samples = samples;
    }

    /* Update session itself. */
    session->reset(effective_session_params, buffer_params);

    /* render */
    if (!b_engine.is_preview() && background && print_render_stats) {
      scene->enable_update_stats();
    }

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
  b_engine.end_result(b_rr, true, false, false);

  /* When tiled rendering is used there will be no "write" done for the tile. Forcefully clear
   * highlighted tiles now, so that the highlight will be removed while processing full frame from
   * file. */
  b_engine.tile_highlight_clear_all();

  double total_time, render_time;
  session->progress.get_time(total_time, render_time);
  VLOG_INFO << "Total render time: " << total_time;
  VLOG_INFO << "Render time (without synchronization): " << render_time;
}

void BlenderSession::render_frame_finish()
{
  /* Processing of all layers and views is done. Clear the strings so that we can communicate
   * progress about reading files and denoising them. */
  b_rlay_name = "";
  b_rview_name = "";

  if (!b_render.use_persistent_data()) {
    /* Free the sync object so that it can properly dereference nodes from the scene graph before
     * the graph is freed. */
    delete sync;
    sync = nullptr;

    session->device_free();
  }

  for (string_view filename : full_buffer_files_) {
    session->process_full_buffer_from_disk(filename);
    if (check_and_report_session_error()) {
      break;
    }
  }

  for (string_view filename : full_buffer_files_) {
    path_remove(filename);
  }

  /* Clear output driver. */
  session->set_output_driver(nullptr);
  session->full_buffer_written_cb = function_null;

  /* The display driver is the source of drawing context for both drawing and possible graphics
   * interoperability objects in the path trace. Once the frame is finished the OpenGL context
   * might be freed form Blender side. Need to ensure that all GPU resources are freed prior to
   * that point.
   * Ideally would only do this when OpenGL context is actually destroyed, but there is no way to
   * know when this happens (at least in the code at the time when this comment was written).
   * The penalty of re-creating resources on every frame is unlikely to be noticed. */
  display_driver_ = nullptr;
  session->set_display_driver(nullptr);

  /* All the files are handled.
   * Clear the list so that this session can be re-used by Persistent Data. */
  full_buffer_files_.clear();
}

static bool bake_setup_pass(Scene *scene, const string &bake_type_str, const int bake_filter)
{
  Integrator *integrator = scene->integrator;
  Film *film = scene->film;
  const char *bake_type = bake_type_str.c_str();

  PassType type = PASS_NONE;
  bool use_direct_light = false;
  bool use_indirect_light = false;
  bool include_albedo = false;

  /* Data passes. */
  if (strcmp(bake_type, "POSITION") == 0) {
    type = PASS_POSITION;
  }
  else if (strcmp(bake_type, "NORMAL") == 0) {
    type = PASS_NORMAL;
  }
  else if (strcmp(bake_type, "UV") == 0) {
    type = PASS_UV;
  }
  else if (strcmp(bake_type, "ROUGHNESS") == 0) {
    type = PASS_ROUGHNESS;
  }
  else if (strcmp(bake_type, "EMIT") == 0) {
    type = PASS_EMISSION;
  }
  /* Environment pass. */
  else if (strcmp(bake_type, "ENVIRONMENT") == 0) {
    type = PASS_BACKGROUND;
  }
  /* AO pass. */
  else if (strcmp(bake_type, "AO") == 0) {
    type = PASS_AO;
  }
  /* Shadow pass. */
  else if (strcmp(bake_type, "SHADOW") == 0) {
    /* Bake as combined pass, together with marking the object as a shadow catcher. */
    type = PASS_SHADOW_CATCHER;
    film->set_use_approximate_shadow_catcher(true);

    use_direct_light = true;
    use_indirect_light = true;
    include_albedo = true;

    integrator->set_use_diffuse(true);
    integrator->set_use_glossy(true);
    integrator->set_use_transmission(true);
    integrator->set_use_emission(true);
  }
  /* Combined pass. */
  else if (strcmp(bake_type, "COMBINED") == 0) {
    type = PASS_COMBINED;
    film->set_use_approximate_shadow_catcher(true);

    use_direct_light = (bake_filter & BL::BakeSettings::pass_filter_DIRECT) != 0;
    use_indirect_light = (bake_filter & BL::BakeSettings::pass_filter_INDIRECT) != 0;
    include_albedo = (bake_filter & BL::BakeSettings::pass_filter_COLOR);

    integrator->set_use_diffuse((bake_filter & BL::BakeSettings::pass_filter_DIFFUSE) != 0);
    integrator->set_use_glossy((bake_filter & BL::BakeSettings::pass_filter_GLOSSY) != 0);
    integrator->set_use_transmission((bake_filter & BL::BakeSettings::pass_filter_TRANSMISSION) !=
                                     0);
    integrator->set_use_emission((bake_filter & BL::BakeSettings::pass_filter_EMIT) != 0);
  }
  /* Light component passes. */
  else if (strcmp(bake_type, "DIFFUSE") == 0) {
    if ((bake_filter & BL::BakeSettings::pass_filter_DIRECT) &&
        bake_filter & BL::BakeSettings::pass_filter_INDIRECT)
    {
      type = PASS_DIFFUSE;
      use_direct_light = true;
      use_indirect_light = true;
    }
    else if (bake_filter & BL::BakeSettings::pass_filter_DIRECT) {
      type = PASS_DIFFUSE_DIRECT;
      use_direct_light = true;
    }
    else if (bake_filter & BL::BakeSettings::pass_filter_INDIRECT) {
      type = PASS_DIFFUSE_INDIRECT;
      use_indirect_light = true;
    }
    else {
      type = PASS_DIFFUSE_COLOR;
    }

    include_albedo = (bake_filter & BL::BakeSettings::pass_filter_COLOR);
  }
  else if (strcmp(bake_type, "GLOSSY") == 0) {
    if ((bake_filter & BL::BakeSettings::pass_filter_DIRECT) &&
        bake_filter & BL::BakeSettings::pass_filter_INDIRECT)
    {
      type = PASS_GLOSSY;
      use_direct_light = true;
      use_indirect_light = true;
    }
    else if (bake_filter & BL::BakeSettings::pass_filter_DIRECT) {
      type = PASS_GLOSSY_DIRECT;
      use_direct_light = true;
    }
    else if (bake_filter & BL::BakeSettings::pass_filter_INDIRECT) {
      type = PASS_GLOSSY_INDIRECT;
      use_indirect_light = true;
    }
    else {
      type = PASS_GLOSSY_COLOR;
    }

    include_albedo = (bake_filter & BL::BakeSettings::pass_filter_COLOR);
  }
  else if (strcmp(bake_type, "TRANSMISSION") == 0) {
    if ((bake_filter & BL::BakeSettings::pass_filter_DIRECT) &&
        bake_filter & BL::BakeSettings::pass_filter_INDIRECT)
    {
      type = PASS_TRANSMISSION;
      use_direct_light = true;
      use_indirect_light = true;
    }
    else if (bake_filter & BL::BakeSettings::pass_filter_DIRECT) {
      type = PASS_TRANSMISSION_DIRECT;
      use_direct_light = true;
    }
    else if (bake_filter & BL::BakeSettings::pass_filter_INDIRECT) {
      type = PASS_TRANSMISSION_INDIRECT;
      use_indirect_light = true;
    }
    else {
      type = PASS_TRANSMISSION_COLOR;
    }

    include_albedo = (bake_filter & BL::BakeSettings::pass_filter_COLOR);
  }

  if (type == PASS_NONE) {
    return false;
  }

  /* Create pass. */
  Pass *pass = scene->create_node<Pass>();
  pass->set_name(ustring("Combined"));
  pass->set_type(type);
  pass->set_include_albedo(include_albedo);

  /* Disable direct indirect light for performance when not needed. */
  integrator->set_use_direct_light(use_direct_light);
  integrator->set_use_indirect_light(use_indirect_light);

  return true;
}

void BlenderSession::bake(BL::Depsgraph &b_depsgraph_,
                          BL::Object &b_object,
                          const string &bake_type,
                          const int bake_filter,
                          const int bake_width,
                          const int bake_height)
{
  b_depsgraph = b_depsgraph_;

  /* Initialize bake manager, before we load the baking kernels. */
  scene->bake_manager->set(scene, b_object.name());

  session->set_display_driver(nullptr);
  session->set_output_driver(make_unique<BlenderOutputDriver>(b_engine));
  session->full_buffer_written_cb = [&](string_view filename) { full_buffer_written(filename); };

  /* Sync scene. */
  BL::Object b_camera_override(b_engine.camera_override());
  sync->sync_camera(b_render, b_camera_override, width, height, "");
  sync->sync_data(
      b_render, b_depsgraph, b_v3d, b_camera_override, width, height, &python_thread_state);

  /* Add render pass that we want to bake, and name it Combined so that it is
   * used as that on the Blender side. */
  if (!bake_setup_pass(scene, bake_type, bake_filter)) {
    session->cancel(true);
  }

  /* Always use transparent background for baking. */
  scene->background->set_transparent(true);

  if (!session->progress.get_cancel()) {
    /* Load built-in images from Blender. */
    builtin_images_load();
  }

  /* Object might have been disabled for rendering or excluded in some
   * other way, in that case Blender will report a warning afterwards. */
  Object *bake_object = nullptr;
  if (!session->progress.get_cancel()) {
    foreach (Object *ob, scene->objects) {
      if (ob->name == b_object.name()) {
        bake_object = ob;
        break;
      }
    }
  }

  /* For the shadow pass, temporarily mark the object as a shadow catcher. */
  const bool was_shadow_catcher = (bake_object) ? bake_object->get_is_shadow_catcher() : false;
  if (bake_object && bake_type == "SHADOW") {
    bake_object->set_is_shadow_catcher(true);
  }

  if (bake_object && !session->progress.get_cancel()) {
    /* Get session and buffer parameters. */
    const SessionParams session_params = BlenderSync::get_session_params(
        b_engine, b_userpref, b_scene, background);

    BufferParams buffer_params;
    buffer_params.width = bake_width;
    buffer_params.height = bake_height;
    buffer_params.window_width = bake_width;
    buffer_params.window_height = bake_height;
    /* Unique layer name for multi-image baking. */
    buffer_params.layer = string_printf("bake_%d\n", bake_id++);

    /* Update session. */
    session->reset(session_params, buffer_params);

    session->progress.set_update_callback(
        function_bind(&BlenderSession::update_bake_progress, this));
  }

  /* Perform bake. Check cancel to avoid crash with incomplete scene data. */
  if (bake_object && !session->progress.get_cancel()) {
    session->start();
    session->wait();
  }

  /* Restore object state. */
  if (bake_object) {
    bake_object->set_is_shadow_catcher(was_shadow_catcher);
  }
}

void BlenderSession::synchronize(BL::Depsgraph &b_depsgraph_)
{
  /* only used for viewport render */
  if (!b_v3d)
    return;

  /* on session/scene parameter changes, we recreate session entirely */
  const SessionParams session_params = BlenderSync::get_session_params(
      b_engine, b_userpref, b_scene, background);
  const SceneParams scene_params = BlenderSync::get_scene_params(
      b_scene, background, use_developer_ui);
  const bool session_pause = BlenderSync::get_session_pause(b_scene, background);

  if (session->params.modified(session_params) || scene->params.modified(scene_params)) {
    free_session();
    create_session();
  }

  ensure_display_driver_if_needed();

  /* increase samples and render time, but never decrease */
  session->set_samples(session_params.samples);
  session->set_time_limit(session_params.time_limit);
  session->set_pause(session_pause);

  /* copy recalc flags, outside of mutex so we can decide to do the real
   * synchronization at a later time to not block on running updates */
  sync->sync_recalc(b_depsgraph_, b_v3d);

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

  /* get buffer parameters */
  const BufferParams buffer_params = BlenderSync::get_buffer_params(
      b_v3d, b_rv3d, scene->camera, width, height);

  /* reset if needed */
  if (scene->need_reset()) {
    session->reset(session_params, buffer_params);

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

void BlenderSession::draw(BL::SpaceImageEditor &space_image)
{
  if (!session || !session->scene) {
    /* Offline render drawing does not force the render engine update, which means it's possible
     * that the Session is not created yet. */
    return;
  }

  thread_scoped_lock lock(draw_state_.mutex);

  const int pass_index = space_image.image_user().multilayer_pass();
  if (pass_index != draw_state_.last_pass_index) {
    BL::RenderPass b_display_pass(b_engine.pass_by_index_get(b_rlay_name.c_str(), pass_index));
    if (!b_display_pass) {
      return;
    }

    Scene *scene = session->scene;

    thread_scoped_lock lock(scene->mutex);

    const Pass *pass = Pass::find(scene->passes, b_display_pass.name());
    if (!pass) {
      return;
    }

    scene->film->set_display_pass(pass->get_type());

    draw_state_.last_pass_index = pass_index;
  }

  if (display_driver_) {
    BL::Array<float, 2> zoom = space_image.zoom();
    display_driver_->set_zoom(zoom[0], zoom[1]);
  }

  session->draw();
}

void BlenderSession::view_draw(int w, int h)
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

      if (scene->camera->is_modified())
        reset = true;

      session->scene->mutex.unlock();
    }

    /* reset if requested */
    if (reset) {
      const SessionParams session_params = BlenderSync::get_session_params(
          b_engine, b_userpref, b_scene, background);
      const BufferParams buffer_params = BlenderSync::get_buffer_params(
          b_v3d, b_rv3d, scene->camera, width, height);
      const bool session_pause = BlenderSync::get_session_pause(b_scene, background);

      if (session_pause == false) {
        session->reset(session_params, buffer_params);
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
  session->draw();
}

void BlenderSession::get_status(string &status, string &substatus)
{
  session->progress.get_status(status, substatus);
}

void BlenderSession::get_progress(double &progress, double &total_time, double &render_time)
{
  session->progress.get_time(total_time, render_time);
  progress = session->progress.get_progress();
}

void BlenderSession::update_bake_progress()
{
  double progress = session->progress.get_progress();

  if (progress != last_progress) {
    b_engine.update_progress((float)progress);
    last_progress = progress;
  }
}

void BlenderSession::update_status_progress()
{
  string timestatus, status, substatus;
  string scene_status = "";
  double progress;
  double total_time, remaining_time = 0, render_time;
  float mem_used = (float)session->stats.mem_used / 1024.0f / 1024.0f;
  float mem_peak = (float)session->stats.mem_peak / 1024.0f / 1024.0f;

  get_status(status, substatus);
  get_progress(progress, total_time, render_time);

  if (progress > 0) {
    remaining_time = session->get_estimated_remaining_time();
  }

  if (background) {
    if (scene)
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
  }

  double current_time = time_dt();
  /* When rendering in a window, redraw the status at least once per second to keep the elapsed
   * and remaining time up-to-date. For headless rendering, only report when something
   * significant changes to keep the console output readable. */
  if (status != last_status || (!headless && (current_time - last_status_time) > 1.0)) {
    b_engine.update_stats("", (timestatus + scene_status + status).c_str());
    b_engine.update_memory_stats(mem_used, mem_peak);
    last_status = status;
    last_status_time = current_time;
  }
  if (progress != last_progress) {
    b_engine.update_progress((float)progress);
    last_progress = progress;
  }

  check_and_report_session_error();
}

bool BlenderSession::check_and_report_session_error()
{
  if (!session->progress.get_error()) {
    return false;
  }

  const string error = session->progress.get_error_message();
  if (error != last_error) {
    /* TODO(sergey): Currently C++ RNA API doesn't let us to use mnemonic name for the variable.
     * Would be nice to have this figured out.
     *
     * For until then, 1 << 5 means RPT_ERROR. */
    b_engine.report(1 << 5, error.c_str());
    b_engine.error_set(error.c_str());
    last_error = error;
  }

  return true;
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

void BlenderSession::ensure_display_driver_if_needed()
{
  if (display_driver_) {
    /* Driver is already created. */
    return;
  }

  if (headless) {
    /* No display needed for headless. */
    return;
  }

  if (b_engine.is_preview()) {
    /* TODO(sergey): Investigate whether DisplayDriver can be used for the preview as well. */
    return;
  }

  unique_ptr<BlenderDisplayDriver> display_driver = make_unique<BlenderDisplayDriver>(
      b_engine, b_scene, background);
  display_driver_ = display_driver.get();
  session->set_display_driver(std::move(display_driver));
}

CCL_NAMESPACE_END
