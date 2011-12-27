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
	int x, y, w, h;

	Tile(int x_, int y_, int w_, int h_)
	: x(x_), y(y_), w(w_), h(h_) {}
};

/* Tile Manager */

class TileManager {
public:
	BufferParams params;

	struct State {
		BufferParams buffer;
		int sample;
		int resolution;
		list<Tile> tiles;
	} state;

	TileManager(bool progressive, int samples, int tile_size, int min_size);
	~TileManager();

	void reset(BufferParams& params, int samples);
	void set_samples(int samples);
	bool next();
	bool done();

protected:
	void set_tiles();

	bool progressive;
	int samples;
	int tile_size;
	int min_size;

	int start_resolution;
};

CCL_NAMESPACE_END

#endif /* __TILE_H__ */

