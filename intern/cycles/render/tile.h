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

	TileManager(bool progressive, int num_samples, int2 tile_size, int start_resolution,
	            bool preserve_tile_device, bool background, int num_devices = 1);
	~TileManager();

	void reset(BufferParams& params, int num_samples);
	void set_samples(int num_samples);
	bool next();
	bool next_tile(Tile& tile, int device = 0);
	bool done();

protected:
	void set_tiles();

	bool progressive;
	int num_samples;
	int2 tile_size;
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

	list<Tile>::iterator next_center_tile(int device = 0);
};

CCL_NAMESPACE_END

#endif /* __TILE_H__ */

