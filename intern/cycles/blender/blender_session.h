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

#ifndef __BLENDER_SESSION_H__
#define __BLENDER_SESSION_H__

#include "device/device.h"
#include "render/scene.h"
#include "render/session.h"
#include "render/bake.h"

#include "util/util_vector.h"

CCL_NAMESPACE_BEGIN

class ImageMetaData;
class Scene;
class Session;
class RenderBuffers;
class RenderTile;

class BlenderSession {
 public:
  BlenderSession(BL::RenderEngine &b_engine,
                 BL::Preferences &b_userpref,
                 BL::BlendData &b_data,
                 bool preview_osl);

  BlenderSession(BL::RenderEngine &b_engine,
                 BL::Preferences &b_userpref,
                 BL::BlendData &b_data,
                 BL::SpaceView3D &b_v3d,
                 BL::RegionView3D &b_rv3d,
                 int width,
                 int height);

  ~BlenderSession();

  void create();

  /* session */
  void create_session();
  void free_session();

  void reset_session(BL::BlendData &b_data, BL::Depsgraph &b_depsgraph);

  /* offline render */
  void render(BL::Depsgraph &b_depsgraph);

  void bake(BL::Depsgraph &b_depsgrah,
            BL::Object &b_object,
            const string &pass_type,
            const int custom_flag,
            const int object_id,
            BL::BakePixel &pixel_array,
            const size_t num_pixels,
            const int depth,
            float pixels[]);

  void write_render_result(BL::RenderResult &b_rr, BL::RenderLayer &b_rlay, RenderTile &rtile);
  void write_render_tile(RenderTile &rtile);

  /* update functions are used to update display buffer only after sample was rendered
   * only needed for better visual feedback */
  void update_render_result(BL::RenderResult &b_rr, BL::RenderLayer &b_rlay, RenderTile &rtile);
  void update_render_tile(RenderTile &rtile, bool highlight);

  /* interactive updates */
  void synchronize(BL::Depsgraph &b_depsgraph);

  /* drawing */
  bool draw(int w, int h);
  void tag_redraw();
  void tag_update();
  void get_status(string &status, string &substatus);
  void get_kernel_status(string &kernel_status);
  void get_progress(float &progress, double &total_time, double &render_time);
  void test_cancel();
  void update_status_progress();
  void update_bake_progress();

  bool background;
  Session *session;
  Scene *scene;
  BlenderSync *sync;
  double last_redraw_time;

  BL::RenderEngine b_engine;
  BL::Preferences b_userpref;
  BL::BlendData b_data;
  BL::RenderSettings b_render;
  BL::Depsgraph b_depsgraph;
  /* NOTE: Blender's scene might become invalid after call
   * free_blender_memory_if_possible().
   */
  BL::Scene b_scene;
  BL::SpaceView3D b_v3d;
  BL::RegionView3D b_rv3d;
  string b_rlay_name;
  string b_rview_name;

  string last_status;
  string last_error;
  float last_progress;
  double last_status_time;

  int width, height;
  bool preview_osl;
  double start_resize_time;

  void *python_thread_state;

  /* Global state which is common for all render sessions created from Blender.
   * Usually denotes command line arguments.
   */

  /* Blender is running from the command line, no windows are shown and some
   * extra render optimization is possible (possible to free draw-only data and
   * so on.
   */
  static bool headless;

  /* ** Resumable render ** */

  /* Overall number of chunks in which the sample range is to be devided. */
  static int num_resumable_chunks;

  /* Current resumable chunk index to render. */
  static int current_resumable_chunk;

  /* Alternative to single-chunk rendering to render a range of chunks. */
  static int start_resumable_chunk;
  static int end_resumable_chunk;

  static bool print_render_stats;

 protected:
  void stamp_view_layer_metadata(Scene *scene, const string &view_layer_name);

  void do_write_update_render_result(BL::RenderResult &b_rr,
                                     BL::RenderLayer &b_rlay,
                                     RenderTile &rtile,
                                     bool do_update_only);
  void do_write_update_render_tile(RenderTile &rtile, bool do_update_only, bool highlight);

  int builtin_image_frame(const string &builtin_name);
  void builtin_image_info(const string &builtin_name, void *builtin_data, ImageMetaData &metadata);
  bool builtin_image_pixels(const string &builtin_name,
                            void *builtin_data,
                            unsigned char *pixels,
                            const size_t pixels_size,
                            const bool associate_alpha,
                            const bool free_cache);
  bool builtin_image_float_pixels(const string &builtin_name,
                                  void *builtin_data,
                                  float *pixels,
                                  const size_t pixels_size,
                                  const bool associate_alpha,
                                  const bool free_cache);
  void builtin_images_load();

  /* Update tile manager to reflect resumable render settings. */
  void update_resumable_tile_manager(int num_samples);

  /* Is used after each render layer synchronization is done with the goal
   * of freeing render engine data which is held from Blender side (for
   * example, dependency graph).
   */
  void free_blender_memory_if_possible();
};

CCL_NAMESPACE_END

#endif /* __BLENDER_SESSION_H__ */
