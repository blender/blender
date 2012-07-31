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

#include "tile.h"

#include "util_algorithm.h"
#include "util_types.h"

CCL_NAMESPACE_BEGIN

TileManager::TileManager(bool progressive_, int num_samples_, int2 tile_size_, int min_size_, int num_devices_)
{
	progressive = progressive_;
	tile_size = tile_size_;
	min_size = min_size_;
	num_devices = num_devices_;

	BufferParams buffer_params;
	reset(buffer_params, 0);
}

TileManager::~TileManager()
{
}

void TileManager::reset(BufferParams& params_, int num_samples_)
{
	params = params_;

	start_resolution = 1;

	int w = params.width, h = params.height;

	if(min_size != INT_MAX) {
		while(w*h > min_size*min_size) {
			w = max(1, w/2); 
			h = max(1, h/2); 

			start_resolution *= 2;
		}
	}

	num_samples = num_samples_;

	state.buffer = BufferParams();
	state.sample = -1;
	state.num_tiles = 0;
	state.num_samples = 0;
	state.resolution = start_resolution;
	state.tiles.clear();
}

void TileManager::set_samples(int num_samples_)
{
	num_samples = num_samples_;
}

void TileManager::set_tiles()
{
	int resolution = state.resolution;
	int image_w = max(1, params.width/resolution);
	int image_h = max(1, params.height/resolution);

	state.tiles.clear();

	int num = min(image_h, num_devices);

	for(int device = 0; device < num; device++) {
		int device_y = (image_h/num)*device;
		int device_h = (device == num-1)? image_h - device*(image_h/num): image_h/num;

		int tile_w = (tile_size.x >= image_w)? 1: (image_w + tile_size.x - 1)/tile_size.x;
		int tile_h = (tile_size.y >= device_h)? 1: (device_h + tile_size.y - 1)/tile_size.y;
		int sub_w = (image_w + tile_w - 1)/tile_w;
		int sub_h = (device_h + tile_h - 1)/tile_h;

		for(int tile_y = 0; tile_y < tile_h; tile_y++) {
			for(int tile_x = 0; tile_x < tile_w; tile_x++) {
				int x = tile_x * sub_w;
				int y = tile_y * sub_h;
				int w = (tile_x == tile_w-1)? image_w - x: sub_w;
				int h = (tile_y == tile_h-1)? device_h - y: sub_h;

				state.tiles.push_back(Tile(x, y + device_y, w, h, device));
			}
		}
	}

	state.num_tiles = state.tiles.size();

	state.buffer.width = image_w;
	state.buffer.height = image_h;

	state.buffer.full_x = params.full_x/resolution;
	state.buffer.full_y = params.full_y/resolution;
	state.buffer.full_width = max(1, params.full_width/resolution);
	state.buffer.full_height = max(1, params.full_height/resolution);
}

bool TileManager::next_tile(Tile& tile, int device)
{
	list<Tile>::iterator iter, best = state.tiles.end();

	int resolution = state.resolution;
	int image_w = max(1, params.width/resolution);
	int image_h = max(1, params.height/resolution);

	int num = min(image_h, num_devices);

	int device_y = (image_h / num) * device;
	int device_h = (device == num - 1) ? image_h - device * (image_h / num) : image_h / num;

	int64_t centx = image_w / 2, centy = device_h / 2, tot = 1;
	int64_t mindist = (int64_t) image_w * (int64_t) device_h;

	/* find center of rendering tiles, image center counts for 1 too */
	for(iter = state.tiles.begin(); iter != state.tiles.end(); iter++) {
		if(iter->device == device && iter->rendering) {
			Tile &cur_tile = *iter;
			centx += cur_tile.x + cur_tile.w / 2;
			centy += cur_tile.y + cur_tile.h / 2;
			tot++;
		}
	}

	centx /= tot;
	centy /= tot;

	/* closest of the non-rendering tiles */
	for(iter = state.tiles.begin(); iter != state.tiles.end(); iter++) {
		if(iter->device == device && iter->rendering == false) {
			Tile &cur_tile = *iter;

			int64_t distx = centx - (cur_tile.x + cur_tile.w / 2);
			int64_t disty = centy - (cur_tile.y + cur_tile.h / 2);
			distx = (int64_t) sqrt((double)distx * distx + disty * disty);

			if (distx < mindist) {
				best = iter;
				mindist = distx;
			}
		}
	}

	if (best != state.tiles.end()) {
		best->rendering = true;
		tile = *best;

		return true;
	}

	return false;
}

bool TileManager::done()
{
	return (state.sample+state.num_samples >= num_samples && state.resolution == 1);
}

bool TileManager::next()
{
	if(done())
		return false;

	if(progressive && state.resolution > 1) {
		state.sample = 0;
		state.resolution /= 2;
		state.num_samples = 1;
		set_tiles();
	}
	else {
		state.sample++;

		if(progressive)
			state.num_samples = 1;
		else
			state.num_samples = num_samples;

		state.resolution = 1;
		set_tiles();
	}

	return true;
}

CCL_NAMESPACE_END

