/*
 * Copyright 2011, Blender Foundation.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef __BLENDER_SESSION_H__
#define __BLENDER_SESSION_H__

#include "device.h"
#include "scene.h"
#include "session.h"

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

	/* session */
	void create_session();
	void free_session();

	void reset_session(BL::BlendData b_data, BL::Scene b_scene);

	/* offline render */
	void render();

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
	void get_progress(float& progress, double& total_time);
	void test_cancel();
	void update_status_progress();

	bool background;
	Session *session;
	Scene *scene;
	BlenderSync *sync;
	double last_redraw_time;

	BL::RenderEngine b_engine;
	BL::UserPreferences b_userpref;
	BL::BlendData b_data;
	BL::Scene b_scene;
	BL::SpaceView3D b_v3d;
	BL::RegionView3D b_rv3d;
	string b_rlay_name;

	string last_status;
	float last_progress;

	int width, height;

protected:
	void do_write_update_render_result(BL::RenderResult b_rr, BL::RenderLayer b_rlay, RenderTile& rtile, bool do_update_only);
	void do_write_update_render_tile(RenderTile& rtile, bool do_update_only);
};

CCL_NAMESPACE_END

#endif /* __BLENDER_SESSION_H__ */
