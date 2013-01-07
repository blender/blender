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

TileManager::TileManager(bool progressive_, int num_samples_, int2 tile_size_, int start_resolution_,
                         bool preserve_tile_device_, bool background_, int tile_order_, int num_devices_)
{
	progressive = progressive_;
	tile_size = tile_size_;
	tile_order = tile_order_;
	start_resolution = start_resolution_;
	num_devices = num_devices_;
	preserve_tile_device = preserve_tile_device_;
	background = background_;

	BufferParams buffer_params;
	reset(buffer_params, 0);
}

TileManager::~TileManager()
{
}

void TileManager::reset(BufferParams& params_, int num_samples_)
{
	params = params_;

	int divider = 1;
	int w = params.width, h = params.height;

	if(start_resolution != INT_MAX) {
		while(w*h > start_resolution*start_resolution) {
			w = max(1, w/2); 
			h = max(1, h/2); 

			divider *= 2;
		}
	}

	num_samples = num_samples_;

	state.buffer = BufferParams();
	state.sample = -1;
	state.num_tiles = 0;
	state.num_rendered_tiles = 0;
	state.num_samples = 0;
	state.resolution_divider = divider;
	state.tiles.clear();
}

void TileManager::set_samples(int num_samples_)
{
	num_samples = num_samples_;
}

/* splits image into tiles and assigns equal amount of tiles to every render device */
void TileManager::gen_tiles_global()
{
	int resolution = state.resolution_divider;
	int image_w = max(1, params.width/resolution);
	int image_h = max(1, params.height/resolution);

	state.tiles.clear();

	int tile_w = (tile_size.x >= image_w)? 1: (image_w + tile_size.x - 1)/tile_size.x;
	int tile_h = (tile_size.y >= image_h)? 1: (image_h + tile_size.y - 1)/tile_size.y;

	int num_logical_devices = preserve_tile_device? num_devices: 1;
	int num = min(image_h, num_logical_devices);
	int tile_index = 0;

	int tiles_per_device = (tile_w * tile_h + num - 1) / num;
	int cur_device = 0, cur_tiles = 0;

	for(int tile_y = 0; tile_y < tile_h; tile_y++) {
		for(int tile_x = 0; tile_x < tile_w; tile_x++, tile_index++) {
			int x = tile_x * tile_size.x;
			int y = tile_y * tile_size.y;
			int w = (tile_x == tile_w-1)? image_w - x: tile_size.x;
			int h = (tile_y == tile_h-1)? image_h - y: tile_size.y;

			state.tiles.push_back(Tile(tile_index, x, y, w, h, cur_device));
			cur_tiles++;

			if(cur_tiles == tiles_per_device) {
				cur_tiles = 0;
				cur_device++;
			}
		}
	}
}

/* slices image into as much pieces as how many devices are rendering this image */
void TileManager::gen_tiles_sliced()
{
	int resolution = state.resolution_divider;
	int image_w = max(1, params.width/resolution);
	int image_h = max(1, params.height/resolution);

	state.tiles.clear();

	int num_logical_devices = preserve_tile_device? num_devices: 1;
	int num = min(image_h, num_logical_devices);
	int tile_index = 0;

	for(int device = 0; device < num; device++) {
		int device_y = (image_h/num)*device;
		int device_h = (device == num-1)? image_h - device*(image_h/num): image_h/num;

		int tile_w = (tile_size.x >= image_w)? 1: (image_w + tile_size.x - 1)/tile_size.x;
		int tile_h = (tile_size.y >= device_h)? 1: (device_h + tile_size.y - 1)/tile_size.y;

		for(int tile_y = 0; tile_y < tile_h; tile_y++) {
			for(int tile_x = 0; tile_x < tile_w; tile_x++, tile_index++) {
				int x = tile_x * tile_size.x;
				int y = tile_y * tile_size.y;
				int w = (tile_x == tile_w-1)? image_w - x: tile_size.x;
				int h = (tile_y == tile_h-1)? device_h - y: tile_size.y;

				state.tiles.push_back(Tile(tile_index, x, y + device_y, w, h, device));
			}
		}
	}
}

void TileManager::set_tiles()
{
	int resolution = state.resolution_divider;
	int image_w = max(1, params.width/resolution);
	int image_h = max(1, params.height/resolution);

	if(background)
		gen_tiles_global();
	else
		gen_tiles_sliced();

	state.num_tiles = state.tiles.size();

	state.buffer.width = image_w;
	state.buffer.height = image_h;

	state.buffer.full_x = params.full_x/resolution;
	state.buffer.full_y = params.full_y/resolution;
	state.buffer.full_width = max(1, params.full_width/resolution);
	state.buffer.full_height = max(1, params.full_height/resolution);
}

list<Tile>::iterator TileManager::next_viewport_tile(int device)
{
	list<Tile>::iterator iter;

	int logical_device = preserve_tile_device? device: 0;

	for(iter = state.tiles.begin(); iter != state.tiles.end(); iter++) {
		if(iter->device == logical_device && iter->rendering == false)
		return iter;
	}

	return state.tiles.end();
}

list<Tile>::iterator TileManager::next_center_tile(int device)
{
	list<Tile>::iterator iter, best = state.tiles.end();

	int resolution = state.resolution_divider;
	int image_w = max(1, params.width/resolution);
	int image_h = max(1, params.height/resolution);

	int logical_device = preserve_tile_device? device: 0;

	int64_t centx = image_w / 2, centy = image_h / 2, tot = 1;
	int64_t mindist = (int64_t) image_w * (int64_t) image_h;

	/* find center of rendering tiles, image center counts for 1 too */
	for(iter = state.tiles.begin(); iter != state.tiles.end(); iter++) {
		if(iter->rendering) {
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
		if(iter->device == logical_device && iter->rendering == false) {
			Tile &cur_tile = *iter;

			int64_t distx = centx - (cur_tile.x + cur_tile.w / 2);
			int64_t disty = centy - (cur_tile.y + cur_tile.h / 2);
			distx = (int64_t) sqrt((double)distx * distx + disty * disty);

			if(distx < mindist) {
				best = iter;
				mindist = distx;
			}
		}
	}

	return best;
}

list<Tile>::iterator TileManager::next_simple_tile(int device, int tile_order)
{
	list<Tile>::iterator iter, best = state.tiles.end();

	int resolution = state.resolution_divider;
	int logical_device = preserve_tile_device? device: 0;

	int64_t cordx = max(1, params.width/resolution);
	int64_t cordy = max(1, params.height/resolution);
	int64_t mindist = cordx * cordy;

	for(iter = state.tiles.begin(); iter != state.tiles.end(); iter++) {
		if(iter->device == logical_device && iter->rendering == false) {
			Tile &cur_tile = *iter;
			
			int64_t distx = cordx;
			
			if (tile_order == TileManager::RIGHT_TO_LEFT)
				distx = cordx - cur_tile.x;
			else if (tile_order == TileManager::LEFT_TO_RIGHT)
				distx = cordx + cur_tile.x;
			else if (tile_order == TileManager::TOP_TO_BOTTOM)
				distx = cordx - cur_tile.y;
			else /* TileManager::BOTTOM_TO_TOP */
				distx = cordx + cur_tile.y;

			if(distx < mindist) {
				best = iter;
				mindist = distx;
			}
		}
	}

	return best;
}

bool TileManager::next_tile(Tile& tile, int device)
{
	list<Tile>::iterator tile_it;
	if (background) {
		if(tile_order == TileManager::CENTER)
			tile_it = next_center_tile(device);
		else
			tile_it = next_simple_tile(device, tile_order);
	}
	else
		tile_it = next_viewport_tile(device);

	if(tile_it != state.tiles.end()) {
		tile_it->rendering = true;
		tile = *tile_it;
		state.num_rendered_tiles++;

		return true;
	}

	return false;
}

bool TileManager::done()
{
	return (state.sample+state.num_samples >= num_samples && state.resolution_divider == 1);
}

bool TileManager::next()
{
	if(done())
		return false;

	if(progressive && state.resolution_divider > 1) {
		state.sample = 0;
		state.resolution_divider /= 2;
		state.num_samples = 1;
		set_tiles();
	}
	else {
		state.sample++;

		if(progressive)
			state.num_samples = 1;
		else
			state.num_samples = num_samples;

		state.resolution_divider = 1;
		set_tiles();
	}

	return true;
}

CCL_NAMESPACE_END

