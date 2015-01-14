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

#include "device.h"
#include "scene.h"
#include "session.h"
#include "bake.h"

#include "util_vector.h"

CCL_NAMESPACE_BEGIN

class Scene;
class Session;
class RenderBuffers;
class RenderTile;

class BlenderSession {
public:
	BlenderSession(BL::RenderEngine b_engine, BL::UserPreferences b_userpref,
		BL::BlendData b_data, BL::Scene b_scene);
	BlenderSession(BL::RenderEngine b_engine, BL::UserPreferences b_userpref,
		BL::BlendData b_data, BL::Scene b_scene,
		BL::SpaceView3D b_v3d, BL::RegionView3D b_rv3d, int width, int height);

	~BlenderSession();

	void create();

	/* session */
	void create_session();
	void free_session();

	void reset_session(BL::BlendData b_data, BL::Scene b_scene);

	/* offline render */
	void render();

	void bake(BL::Object b_object, const string& pass_type, BL::BakePixel pixel_array, const size_t num_pixels, const int depth, float pixels[]);

	void write_render_result(BL::RenderResult b_rr, BL::RenderLayer b_rlay, RenderTile& rtile);
	void write_render_tile(RenderTile& rtile);

	/* update functions are used to update display buffer only after sample was rendered
	 * only needed for better visual feedback */
	void update_render_result(BL::RenderResult b_rr, BL::RenderLayer b_rlay, RenderTile& rtile);
	void update_render_tile(RenderTile& rtile);

	/* interactive updates */
	void synchronize();

	/* drawing */
	bool draw(int w, int h);
	void tag_redraw();
	void tag_update();
	void get_status(string& status, string& substatus);
	void get_progress(float& progress, double& total_time, double& render_time);
	void test_cancel();
	void update_status_progress();
	void update_bake_progress();

	bool background;
	Session *session;
	Scene *scene;
	BlenderSync *sync;
	double last_redraw_time;

	BL::RenderEngine b_engine;
	BL::UserPreferences b_userpref;
	BL::BlendData b_data;
	BL::RenderSettings b_render;
	BL::Scene b_scene;
	BL::SpaceView3D b_v3d;
	BL::RegionView3D b_rv3d;
	string b_rlay_name;

	string last_status;
	string last_error;
	float last_progress;

	int width, height;
	double start_resize_time;

	void *python_thread_state;

protected:
	void do_write_update_render_result(BL::RenderResult b_rr, BL::RenderLayer b_rlay, RenderTile& rtile, bool do_update_only);
	void do_write_update_render_tile(RenderTile& rtile, bool do_update_only);

	int builtin_image_frame(const string &builtin_name);
	void builtin_image_info(const string &builtin_name, void *builtin_data, bool &is_float, int &width, int &height, int &depth, int &channels);
	bool builtin_image_pixels(const string &builtin_name, void *builtin_data, unsigned char *pixels);
	bool builtin_image_float_pixels(const string &builtin_name, void *builtin_data, float *pixels);
};

CCL_NAMESPACE_END

#endif /* __BLENDER_SESSION_H__ */
