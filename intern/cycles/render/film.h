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

#ifndef __FILM_H__
#define __FILM_H__

#include "util_string.h"
#include "util_vector.h"

#include "kernel_types.h"

CCL_NAMESPACE_BEGIN

class Device;
class DeviceScene;
class Scene;

class Pass {
public:
	PassType type;
	int components;
	bool filter;
	bool exposure;
	PassType divide_type;

	static void add(PassType type, vector<Pass>& passes);
	static bool equals(const vector<Pass>& A, const vector<Pass>& B);
	static bool contains(const vector<Pass>& passes, PassType);
};

class Film {
public:
	float exposure;
	vector<Pass> passes;
	bool need_update;

	Film();
	~Film();

	void device_update(Device *device, DeviceScene *dscene);
	void device_free(Device *device, DeviceScene *dscene);

	bool modified(const Film& film);
	void tag_passes_update(Scene *scene, const vector<Pass>& passes_);
	void tag_update(Scene *scene);
};

CCL_NAMESPACE_END

#endif /* __FILM_H__ */

