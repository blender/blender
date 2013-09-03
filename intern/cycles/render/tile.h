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
 * limitations under the License
 */

#ifndef __TILE_H__
#define __TILE_H__

#include <limits.h>

#include "buffers.h"
#include "util_list.h"

CCL_NAMESPACE_BEGIN

/* Tile */

class Tile {
public:
	int index;
	int x, y, w, h;
	int device;
	bool rendering;

	Tile()
	{}

	Tile(int index_, int x_, int y_, int w_, int h_, int device_)
	: index(index_), x(x_), y(y_), w(w_), h(h_), device(device_), rendering(false) {}
};

/* Tile order */

/* Note: this should match enum_tile_order in properties.py */
enum TileOrder {
	TILE_CENTER = 0,
	TILE_RIGHT_TO_LEFT = 1,
	TILE_LEFT_TO_RIGHT = 2,
	TILE_TOP_TO_BOTTOM = 3,
	TILE_BOTTOM_TO_TOP = 4
};

/* Tile Manager */

class TileManager {
public:
	BufferParams params;

	struct State {
		BufferParams buffer;
		int sample;
		int num_samples;
		int resolution_divider;
		int num_tiles;
		int num_rendered_tiles;
		list<Tile> tiles;
	} state;

	int num_samples;

	TileManager(bool progressive, int num_samples, int2 tile_size, int start_resolution,
	            bool preserve_tile_device, bool background, TileOrder tile_order, int num_devices = 1);
	~TileManager();

	void reset(BufferParams& params, int num_samples);
	void set_samples(int num_samples);
	bool next();
	bool next_tile(Tile& tile, int device = 0);
	bool done();
	
	void set_tile_order(TileOrder tile_order_) { tile_order = tile_order_; }
protected:

	void set_tiles();

	bool progressive;
	int2 tile_size;
	TileOrder tile_order;
	int start_resolution;
	int num_devices;

	/* in some cases it is important that the same tile will be returned for the same
	 * device it was originally generated for (i.e. viewport rendering when buffer is
	 * allocating once for tile and then always used by it)
	 *
	 * in other cases any tile could be handled by any device (i.e. final rendering
	 * without progressive refine)
	 */
	bool preserve_tile_device;

	/* for background render tiles should exactly match render parts generated from
	 * blender side, which means image first gets split into tiles and then tiles are
	 * assigning to render devices
	 *
	 * however viewport rendering expects tiles to be allocated in a special way,
	 * meaning image is being sliced horizontally first and every device handles
	 * it's own slice
	 */
	bool background;

	/* splits image into tiles and assigns equal amount of tiles to every render device */
	void gen_tiles_global();

	/* slices image into as much pieces as how many devices are rendering this image */
	void gen_tiles_sliced();

	/* returns tiles for background render */
	list<Tile>::iterator next_background_tile(int device, TileOrder tile_order);

	/* returns first unhandled tile for viewport render */
	list<Tile>::iterator next_viewport_tile(int device);
};

CCL_NAMESPACE_END

#endif /* __TILE_H__ */

