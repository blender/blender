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

#ifndef __TABLES_H__
#define __TABLES_H__

#include <util_list.h>

CCL_NAMESPACE_BEGIN

class Device;
class DeviceScene;
class Scene;

enum { TABLE_CHUNK_SIZE = 256 };
enum { TABLE_OFFSET_INVALID = -1 };

class LookupTables {
public:
	struct Table {
		size_t offset;
		size_t size;
	};

	bool need_update;
	list<Table> lookup_tables;

	LookupTables();
	~LookupTables();

	void device_update(Device *device, DeviceScene *dscene);
	void device_free(Device *device, DeviceScene *dscene);

	size_t add_table(DeviceScene *dscene, vector<float>& data);
	void remove_table(size_t offset);
};

CCL_NAMESPACE_END

#endif /* __TABLES_H__ */

