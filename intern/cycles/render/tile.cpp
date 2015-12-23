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

#include "tile.h"

#include "util_algorithm.h"
#include "util_types.h"

CCL_NAMESPACE_BEGIN

namespace {

class TileComparator {
public:
	TileComparator(TileOrder order, int2 center)
	 :  order_(order),
	    center_(center)
	{}

	bool operator()(Tile &a, Tile &b)
	{
		switch(order_) {
			case TILE_CENTER:
			{
				float2 dist_a = make_float2(center_.x - (a.x + a.w/2),
				                            center_.y - (a.y + a.h/2));
				float2 dist_b = make_float2(center_.x - (b.x + b.w/2),
				                            center_.y - (b.y + b.h/2));
				return dot(dist_a, dist_a) < dot(dist_b, dist_b);
			}
			case TILE_LEFT_TO_RIGHT:
				return (a.x == b.x)? (a.y < b.y): (a.x < b.x);
			case TILE_RIGHT_TO_LEFT:
				return (a.x == b.x)? (a.y < b.y): (a.x > b.x);
			case TILE_TOP_TO_BOTTOM:
				return (a.y == b.y)? (a.x < b.x): (a.y > b.y);
			case TILE_BOTTOM_TO_TOP:
			default:
				return (a.y == b.y)? (a.x < b.x): (a.y < b.y);
		}
	}

protected:
	TileOrder order_;
	int2 center_;
};

}  /* namespace */

TileManager::TileManager(bool progressive_, int num_samples_, int2 tile_size_, int start_resolution_,
                         bool preserve_tile_device_, bool background_, TileOrder tile_order_, int num_devices_)
{
	progressive = progressive_;
	tile_size = tile_size_;
	tile_order = tile_order_;
	start_resolution = start_resolution_;
	num_samples = num_samples_;
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

/* If sliced is false, splits image into tiles and assigns equal amount of tiles to every render device.
 * If sliced is true, slice image into as much pieces as how many devices are rendering this image. */
int TileManager::gen_tiles(bool sliced)
{
	int resolution = state.resolution_divider;
	int image_w = max(1, params.width/resolution);
	int image_h = max(1, params.height/resolution);
	int2 center = make_int2(image_w/2, image_h/2);

	state.tiles.clear();

	int num_logical_devices = preserve_tile_device? num_devices: 1;
	int num = min(image_h, num_logical_devices);
	int slice_num = sliced? num: 1;
	int tile_index = 0;

	state.tiles.clear();
	state.tiles.resize(num);
	vector<list<Tile> >::iterator tile_list = state.tiles.begin();

	for(int slice = 0; slice < slice_num; slice++) {
		int slice_y = (image_h/slice_num)*slice;
		int slice_h = (slice == slice_num-1)? image_h - slice*(image_h/slice_num): image_h/slice_num;

		int tile_w = (tile_size.x >= image_w)? 1: (image_w + tile_size.x - 1)/tile_size.x;
		int tile_h = (tile_size.y >= slice_h)? 1: (slice_h + tile_size.y - 1)/tile_size.y;

		int tiles_per_device = (tile_w * tile_h + num - 1) / num;
		int cur_device = 0, cur_tiles = 0;

		for(int tile_y = 0; tile_y < tile_h; tile_y++) {
			for(int tile_x = 0; tile_x < tile_w; tile_x++, tile_index++) {
				int x = tile_x * tile_size.x;
				int y = tile_y * tile_size.y;
				int w = (tile_x == tile_w-1)? image_w - x: tile_size.x;
				int h = (tile_y == tile_h-1)? slice_h - y: tile_size.y;

				tile_list->push_back(Tile(tile_index, x, y + slice_y, w, h, sliced? slice: cur_device));

				if(!sliced) {
					cur_tiles++;

					if(cur_tiles == tiles_per_device) {
						tile_list->sort(TileComparator(tile_order, center));
						tile_list++;
						cur_tiles = 0;
						cur_device++;
					}
				}
			}
		}
	}

	return tile_index;
}

void TileManager::set_tiles()
{
	int resolution = state.resolution_divider;
	int image_w = max(1, params.width/resolution);
	int image_h = max(1, params.height/resolution);

	state.num_tiles = gen_tiles(!background);

	state.buffer.width = image_w;
	state.buffer.height = image_h;

	state.buffer.full_x = params.full_x/resolution;
	state.buffer.full_y = params.full_y/resolution;
	state.buffer.full_width = max(1, params.full_width/resolution);
	state.buffer.full_height = max(1, params.full_height/resolution);
}

bool TileManager::next_tile(Tile& tile, int device)
{
	int logical_device = preserve_tile_device? device: 0;

	if((logical_device >= state.tiles.size()) || state.tiles[logical_device].empty())
		return false;

	tile = Tile(state.tiles[logical_device].front());
	state.tiles[logical_device].pop_front();
	state.num_rendered_tiles++;
	return true;
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

