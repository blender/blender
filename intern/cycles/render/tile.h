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

#ifndef __TILE_H__
#define __TILE_H__

#include <limits.h>

#include "render/buffers.h"
#include "util/util_list.h"

CCL_NAMESPACE_BEGIN

/* Tile */

class Tile {
public:
	int index;
	int x, y, w, h;
	int device;
	/* RENDER: The tile has to be rendered.
	 * RENDERED: The tile has been rendered, but can't be denoised yet (waiting for neighbors).
	 * DENOISE: The tile can be denoised now.
	 * DENOISED: The tile has been denoised, but can't be freed yet (waiting for neighbors).
	 * DONE: The tile is finished and has been freed. */
	typedef enum { RENDER = 0, RENDERED, DENOISE, DENOISED, DONE } State;
	State state;
	RenderBuffers *buffers;

	Tile()
	{}

	Tile(int index_, int x_, int y_, int w_, int h_, int device_, State state_ = RENDER)
	: index(index_), x(x_), y(y_), w(w_), h(h_), device(device_), state(state_), buffers(NULL) {}
};

/* Tile order */

/* Note: this should match enum_tile_order in properties.py */
enum TileOrder {
	TILE_CENTER = 0,
	TILE_RIGHT_TO_LEFT = 1,
	TILE_LEFT_TO_RIGHT = 2,
	TILE_TOP_TO_BOTTOM = 3,
	TILE_BOTTOM_TO_TOP = 4,
	TILE_HILBERT_SPIRAL = 5,
};

/* Tile Manager */

class TileManager {
public:
	BufferParams params;

	struct State {
		vector<Tile> tiles;
		int tile_stride;
		BufferParams buffer;
		int sample;
		int num_samples;
		int resolution_divider;
		int num_tiles;

		/* Total samples over all pixels: Generally num_samples*num_pixels,
		 * but can be higher due to the initial resolution division for previews. */
		uint64_t total_pixel_samples;

		/* These lists contain the indices of the tiles to be rendered/denoised and are used
		 * when acquiring a new tile for the device.
		 * Each list in each vector is for one logical device. */
		vector<list<int> > render_tiles;
		vector<list<int> > denoising_tiles;
	} state;

	int num_samples;

	TileManager(bool progressive, int num_samples, int2 tile_size, int start_resolution,
	            bool preserve_tile_device, bool background, TileOrder tile_order, int num_devices = 1, int pixel_size = 1);
	~TileManager();

	void device_free();
	void reset(BufferParams& params, int num_samples);
	void set_samples(int num_samples);
	bool next();
	bool next_tile(Tile* &tile, int device = 0);
	bool finish_tile(int index, bool& delete_tile);
	bool done();

	void set_tile_order(TileOrder tile_order_) { tile_order = tile_order_; }

	/* ** Sample range rendering. ** */

	/* Start sample in the range. */
	int range_start_sample;

	/* Number to samples in the rendering range. */
	int range_num_samples;

	/* Get number of actual samples to render. */
	int get_num_effective_samples();

	/* Schedule tiles for denoising after they've been rendered. */
	bool schedule_denoising;
protected:

	void set_tiles();

	bool progressive;
	int2 tile_size;
	TileOrder tile_order;
	int start_resolution;
	int pixel_size;
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

	/* Generate tile list, return number of tiles. */
	int gen_tiles(bool sliced);
	void gen_render_tiles();

	int get_neighbor_index(int index, int neighbor);
	bool check_neighbor_state(int index, Tile::State state);
};

CCL_NAMESPACE_END

#endif /* __TILE_H__ */
