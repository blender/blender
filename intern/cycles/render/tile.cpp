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

CCL_NAMESPACE_BEGIN

TileManager::TileManager(bool progressive_, int samples_, int tile_size_, int min_size_)
{
	progressive = progressive_;
	tile_size = tile_size_;
	min_size = min_size_;

	BufferParams buffer_params;
	reset(buffer_params, 0);
}

TileManager::~TileManager()
{
}

void TileManager::reset(BufferParams& params_, int samples_)
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

	samples = samples_;

	state.buffer = BufferParams();
	state.sample = -1;
	state.resolution = start_resolution;
	state.tiles.clear();
}

void TileManager::set_samples(int samples_)
{
	samples = samples_;
}

void TileManager::set_tiles()
{
	int resolution = state.resolution;
	int image_w = max(1, params.width/resolution);
	int image_h = max(1, params.height/resolution);
	int tile_w = (tile_size >= image_w)? 1: (image_w + tile_size - 1)/tile_size;
	int tile_h = (tile_size >= image_h)? 1: (image_h + tile_size - 1)/tile_size;
	int sub_w = image_w/tile_w;
	int sub_h = image_h/tile_h;

	state.tiles.clear();

	for(int tile_y = 0; tile_y < tile_h; tile_y++) {
		for(int tile_x = 0; tile_x < tile_w; tile_x++) {
			int x = tile_x * sub_w;
			int y = tile_y * sub_h;
			int w = (tile_x == tile_w-1)? image_w - x: sub_w;
			int h = (tile_y == tile_h-1)? image_h - y: sub_h;

			state.tiles.push_back(Tile(x, y, w, h));
		}
	}

	state.buffer.width = image_w;
	state.buffer.height = image_h;

	state.buffer.full_x = params.full_x/resolution;
	state.buffer.full_y = params.full_y/resolution;
	state.buffer.full_width = max(1, params.full_width/resolution);
	state.buffer.full_height = max(1, params.full_height/resolution);
}

bool TileManager::done()
{
	return (state.sample+1 >= samples && state.resolution == 1);
}

bool TileManager::next()
{
	if(done())
		return false;

	if(progressive && state.resolution > 1) {
		state.sample = 0;
		state.resolution /= 2;
		set_tiles();
	}
	else {
		state.sample++;
		state.resolution = 1;
		set_tiles();
	}

	return true;
}

CCL_NAMESPACE_END

